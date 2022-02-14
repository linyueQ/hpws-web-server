#include "buffer.h"
#include <iostream>
#include <memory>

/********************************************************************************
 * RingBuffer实现用户缓冲区，分三种情况
 * 
 * 1、writePos>readPos，缓冲区没用完，也还没有进入轮回；
 * 2、writePos<readPos-1，缓冲区没用完，但是进入轮回了；
 * 3、writePos==readPos-1，缓冲区用完了，需要扩充缓冲区；
 * 
 * 为了区分Buffer是否填满，比起vector大小设置为size+1；
 * 
 * 大坑：readPos_为0时会引发各种各样的错误
 * 
 ********************************************************************************/

Buffer::Buffer(int initBuffSize) : buffer_(initBuffSize), readPos_(0), writePos_(0) {}

// 可读的数据的大小
size_t Buffer::ReadableBytes() const {  
    //对应上述三种情况，只需要分为比它小的和比它大的就可以
    if(writePos_>=readPos_) {
        return writePos_ - readPos_;
    } else {
        return ReadableTailBytes()+writePos_;
    }
}

// 可以写的数据大小
size_t Buffer::WritableBytes() const {
    //对应上述三种情况，只需要分为比它小的和比它大的就可以
    if(writePos_>=readPos_) {
        //readPos_为0时是特殊的，WritableTailBytes()无法用到最后一个格子的数据
        if(readPos_==0) return WritableTailBytes();
        else return WritableTailBytes()+readPos_-1;
    } else {
        return readPos_-writePos_-1;
    }
}

size_t Buffer::capacity() const {
    return buffer_.size();
}

size_t Buffer::WritableTailBytes() const {
    int tailBytes=buffer_.size()-writePos_;
    //这是个坑，因为writePos_<readPos_时至少要留下一个空位
    if(readPos_==0) tailBytes-=1; 
    return tailBytes;
}

size_t Buffer::ReadableTailBytes() const {
    return buffer_.size()-readPos_;
}

// 当前未读字节的位置
const char* Buffer::Peek() const {
    return BeginPtr_() + readPos_;
}

// 从读指针处回收定量的空间
void Buffer::Retrieve(size_t len) {
    assert(len <= ReadableBytes());
    readPos_ = (readPos_ + len) % buffer_.size();
}

//buff.RetrieveUntil(lineEnd + 2);
void Buffer::RetrieveUntil(const char* end) {
    assert(Peek() <= end );
    Retrieve(end - Peek());
}

//初始化缓冲区（回收所有资源）
void Buffer::RetrieveAll() {
    bzero(&buffer_[0], buffer_.size());
    readPos_ = 0;
    writePos_ = 0;
}

void Buffer::ReadToDst(char* dst, size_t len) {
    //这里需要分成两段
    if(len>ReadableBytes()) len=ReadableBytes();
    if(writePos_>=readPos_) {
        memcpy(dst,Peek(),len);
    } else {
        int RdTailBytes=ReadableTailBytes();
        memcpy(dst,Peek(),RdTailBytes);
        memcpy(dst+RdTailBytes,BeginPtr_(),len-RdTailBytes);
    }
    Retrieve(len);
}

std::string Buffer::RetrieveAllToStr() {
    std::unique_ptr<char[]> tmp(new char[ReadableBytes()]);
    ReadToDst(tmp.get(),ReadableBytes());
    std::string &&tmpStr=tmp.get();
    RetrieveAll();
    return std::forward<std::string>(tmpStr);
}

const char* Buffer::BeginWriteConst() const {
    return BeginPtr_() + writePos_;
}

char* Buffer::BeginWrite() {
    return BeginPtr_() + writePos_;
}

void Buffer::HasWritten(size_t len) {
    writePos_ = (writePos_ + len) % buffer_.size();
} 

void Buffer::Append(const std::string& str) {
    Append(str.data(), str.length());
}

void Buffer::Append(const void* data, size_t len) {
    assert(data);
    Append(static_cast<const char*>(data), len);
    
}

//  Append(buff, len - writable);   buff临时数组，len-writable是临时数组中的数据个数
void Buffer::Append(const char* str, size_t len) {
    assert(str);
    //确保还剩len的空间，不够空间就会扩容，扩容以后也有两种情况：
    //  writePos_>=readPos_和writePos_<readPos_，这样编码太复杂
    //  了，所以我们每次申请到新空间就重整空间，使writePos_>=readPos_
    EnsureWriteable(len);
    if(len<=WritableTailBytes()){
        //够位置就直接copy(writePos_<readPos_也可以归类到该情况)
        std::copy(str, str + len, BeginWrite());//将char数组中的数据拷贝到扩展后的位置
        HasWritten(len); 
    }else{
        //分两段copy
        int WtTailBytes=WritableTailBytes();
        std::copy(str,str+WtTailBytes,BeginWrite());
        HasWritten(WtTailBytes);
        std::copy(str+WtTailBytes,str+len,BeginWrite());
        HasWritten(len-WtTailBytes);
    }
}

void Buffer::Append(const Buffer& buff) {
    Append(buff.Peek(), buff.ReadableBytes());
}

void Buffer::EnsureWriteable(size_t len) {
    if(WritableBytes() < len) {
        MakeSpace_(len);
    }
    assert(WritableBytes() >= len);
}

//用于申请更多的空间（扩容核心）
void Buffer::MakeSpace_(size_t len) {
    int tail=buffer_.size();
    while(WritableBytes()<len){
        buffer_.resize(buffer_.size()*2);
    }
    //我们可以在扩容后，执行一次空间重整，把writePos_那段数据挪到后面去，让writePos_>=readPos_
    if(writePos_<readPos_){
        std::copy(BeginPtr_(), BeginPtr_() + writePos_, BeginPtr_() + tail);
        writePos_=tail+writePos_;
    }
}

//*******************************************************************************************

//从文件描述符的TCP缓冲区中读取数据
ssize_t Buffer::ReadFd(int fd, int* saveErrno) {
    
    // printf("start Buffer::ReadFd\n");
    // 临时的数组，保证能够把所有的数据都读出来
    char buff[65535];
    
    //因为是从文件描述符中读取数据所以说应该把数据写到用户级缓冲区（readBuff中），
    //  因此，获取的应该是写指针
    struct iovec iov[3];
    
    /* 分散读fd， 保证数据全部读完 */
    //  第一块映射到Buffer的可写空间的其实位置（获取readBuff的写指针位置，映射到第一块的iov_base中）；
    //  第二块是填补0~readPos_-1的空间
    //  第三块则是为了防止Buffer当前空间不够，所以临时申请的buff空间，映射到这一块来；
    if(writePos_>=readPos_){
        //readPos_是size_t，size_t是unsigned int啊！！！
        // printf("Buffer::ReadFd: writePos_>=readPos_\n");
        iov[0].iov_base = BeginPtr_() + writePos_;
        iov[0].iov_len = WritableTailBytes();
        iov[1].iov_base = BeginPtr_();
        iov[1].iov_len = (readPos_>=1 ? readPos_-1:0);
        iov[2].iov_base = buff;
        iov[2].iov_len = sizeof(buff);
    }else{
        // printf("Buffer::ReadFd: writePos_<readPos_\n");
        iov[0].iov_base = BeginPtr_() + writePos_;
        iov[0].iov_len = WritableBytes();
        iov[1].iov_base = BeginPtr_()+ (readPos_>=1 ? readPos_-1:0);
        iov[1].iov_len = 0;
        iov[2].iov_base = buff;
        iov[2].iov_len = sizeof(buff);
    }
    const ssize_t len = readv(fd, iov, 3);
    if(len < 0) {
        *saveErrno = errno;
        // perror("ReadFd Fail");
    }
    // 读出的长度小于Buffer的剩余空间
    else if(static_cast<size_t>(len) <= WritableBytes()) {
        HasWritten(len);
    }
    // 读出的长度大于Buffer的剩余空间，扩容之后将buff数据复制到里面去
    else {
        //已经写满了数据，所以writePos_应该更新到
        Append(buff, len - WritableBytes());
    }
    // printf("finish Buffer::ReadFd\n");
    return len;
}

ssize_t Buffer::WriteFd(int fd, int* saveErrno) {
    //这里分两种情况，writePos_>=readPos_和writePos_<readPos
    struct iovec iov[2];
    if(writePos_>=readPos_){
        iov[0].iov_base=BeginPtr_()+readPos_;
        iov[0].iov_len=ReadableBytes();
        iov[1].iov_base=BeginPtr_()+writePos_;
        iov[1].iov_len=0;
    }else{
        iov[0].iov_base=BeginPtr_()+readPos_;
        iov[0].iov_len=ReadableTailBytes();
        iov[1].iov_base=BeginPtr_();
        iov[1].iov_len=writePos_;
    }
    ssize_t len = writev(fd, iov, 2);
    if(len < 0) {
        *saveErrno = errno;
        return len;
    } 
    Retrieve(len);
    return len;
}

char* Buffer::BeginPtr_() {
    return &*buffer_.begin();
}

const char* Buffer::BeginPtr_() const {
    //这里为了获取vector存储空间的首地址，它先获取起始迭代器
    // 所指向的位置(*)，然后再取址（很巧妙）
    return &*buffer_.begin();
}