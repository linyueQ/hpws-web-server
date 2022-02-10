#include "httpconn.h"
using namespace std;

const char* HttpConn::srcDir;
std::atomic<int> HttpConn::userCount;

bool HttpConn::isET;

HttpConn::HttpConn() { 
    fd_ = -1;
    addr_ = { 0 };
    isClose_ = true;
};

HttpConn::~HttpConn() { 
    Close(); 
};

void HttpConn::init(int fd, const sockaddr_in& addr) {
    assert(fd > 0);
    userCount++;
    addr_ = addr;
    fd_ = fd;
    // 每一个Http连接都有自己的用户态读写缓冲区
    writeBuff_.RetrieveAll();
    readBuff_.RetrieveAll();
    isClose_ = false;
    LOG_INFO("Client[%d](%s:%d) in, userCount:%d", fd_, GetIP(), GetPort(), (int)userCount);
}

void HttpConn::Close() {
    response_.UnmapFile();  // 接触内存映射
    if(isClose_ == false){
        isClose_ = true; 
        userCount--;
        close(fd_);
        LOG_INFO("Client[%d](%s:%d) quit, UserCount:%d", fd_, GetIP(), GetPort(), (int)userCount);
    }
}

int HttpConn::GetFd() const {
    return fd_;
};

struct sockaddr_in HttpConn::GetAddr() const {
    return addr_;
}

const char* HttpConn::GetIP() const {
    return inet_ntoa(addr_.sin_addr);
}

int HttpConn::GetPort() const {
    return addr_.sin_port;
}

ssize_t HttpConn::read(int* saveErrno) {
    // 一次性读出所有数据
    printf("start HttpConn::read\n");
    ssize_t len = -1;
    do {
        len = readBuff_.ReadFd(fd_, saveErrno);
        if (len <= 0) {
            printf("HttpConn::read: len<=0, break\n");
            break;
        }
    } while (isET);
    return len;
}

ssize_t HttpConn::write(int* saveErrno) {
    ssize_t len = -1;
    //write的每个循环都有3种情况：
    //  （1）第一块每写完；（2）第一块写完，第二块每写完；（3）两块都写完；
    do {
        // uio.h提供writev来分散写iov数组里面的数据（这里是将数据写到socket文件描述符中）
        //  写完以后有几种情况：
        //  （1）如果全部数据写入，则直接进入分支2；
        //  （2）如果只写了第一块的一部分，则len<iov[0]_.len，进入分支3
        //  （3）如果写完第一块，但第二块每写完，则先进入分支2，然后两块的len都转换为0;
        len = writev(fd_, iov_, iovCnt_);//非阻塞
        if(len <= 0) {//有可能数据没写完，但是socket的写缓冲区不够位置，所以break
            *saveErrno = errno;
            break;
        }
        // 这种情况是所有数据都传输结束了
        if(iov_[0].iov_len + iov_[1].iov_len  == 0) { break; } /* 传输结束 */
        // 第一块的数据已经全部写入到socket中，接着写第二块内存，做相应的处理
        else if(static_cast<size_t>(len) > iov_[0].iov_len) {
            iov_[1].iov_base = (uint8_t*) iov_[1].iov_base + (len - iov_[0].iov_len);
            iov_[1].iov_len -= (len - iov_[0].iov_len);
            if(iov_[0].iov_len) {
                writeBuff_.RetrieveAll();
                iov_[0].iov_len = 0;
            }
        }
        // 第一块内存还没写完，移动下一次写的指针（还没有写到第二块内存的数据）
        else {
            iov_[0].iov_base = (uint8_t*)iov_[0].iov_base + len; 
            iov_[0].iov_len -= len; 
            writeBuff_.Retrieve(len);
        }
    } while(isET || ToWriteBytes() > 10240);//10KB
    return len;
}

// 处理用户发送过来的请求（数据已经读到readBuffer中）
//  业务逻辑处理（这里只提供了一个资源访问功能）
bool HttpConn::process() {
    // 初始化请求对象
    printf("start HttpConn::process()\n");
    request_.Init();
    
    // 尝试读取数据并进行相应处理
    if(readBuff_.ReadableBytes() <= 0) {    // 判断是否有请求数据
        printf("No available data, return false\n");
        return false;
    }
    else if(request_.parse(readBuff_)) {    // 有数据就解析HTTP请求
        LOG_DEBUG("%s", request_.path().c_str());
        // 初始化响应对象（返回HTTP状态码：200-OK）
        response_.Init(srcDir, request_.path(), request_.IsKeepAlive(), 200);
    } else {                                // 解析失败
        // 初始化响应对象（返回HTTP状态码：400-客户端请求错误）
        response_.Init(srcDir, request_.path(), false, 400);
    }

    // 生成响应信息（往writeBuff_中写入响应信息）
    response_.MakeResponse(writeBuff_);

    // 响应头
    iov_[0].iov_base = const_cast<char*>(writeBuff_.Peek());
    iov_[0].iov_len = writeBuff_.ReadableBytes();
    iovCnt_ = 1;

    // 要传输的资源文件（返回mmFile_，而事实上mmFile里面已经在process()过程中完成了mmap）
    if(response_.FileLen() > 0  && response_.File()) {
        iov_[1].iov_base = response_.File();
        iov_[1].iov_len = response_.FileLen();
        iovCnt_ = 2;
    }
    
    LOG_DEBUG("filesize:%d, %d  to %d", response_.FileLen() , iovCnt_, ToWriteBytes());
    return true;
}
