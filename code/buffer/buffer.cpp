 #include "buffer.h"

Buffer::Buffer(int initBuffSize) : buffer_(initBuffSize), readPos_(0), writePos_(0) {}

// 可以读的数据的大小  写位置 - 读位置，中间的数据就是可以读的大小
size_t Buffer::ReadableBytes() const {  
    return writePos_ - readPos_;
}

// 可以写的数据大小，缓冲区的总大小 - 写位置
size_t Buffer::WritableBytes() const {
    return buffer_.size() - writePos_;
}

// 前面可回收的空间，即当前读取到哪个位置，表示前面的数据已经被读走了，可以回收
size_t Buffer::PrependableBytes() const {
    return readPos_;
}

// 当前未读字节的位置
const char* Buffer::Peek() const {
    return BeginPtr_() + readPos_;
}

// 从读指针处回收定量的空间
void Buffer::Retrieve(size_t len) {
    assert(len <= ReadableBytes());
    readPos_ += len;
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

std::string Buffer::RetrieveAllToStr() {
    std::string str(Peek(), ReadableBytes());
    RetrieveAll();
    return str;
}

const char* Buffer::BeginWriteConst() const {
    return BeginPtr_() + writePos_;
}

char* Buffer::BeginWrite() {
    return BeginPtr_() + writePos_;
}

void Buffer::HasWritten(size_t len) {
    writePos_ += len;
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
    EnsureWriteable(len);//确保还剩len的空间，不够空间就会扩容
    std::copy(str, str + len, BeginWrite());//将char数组中的数据拷贝到扩展后的位置
    HasWritten(len);
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

//从文件描述符的TCP缓冲区中读取数据
ssize_t Buffer::ReadFd(int fd, int* saveErrno) {
    
    // 临时的数组，保证能够把所有的数据都读出来
    char buff[65535];
    
    //因为是从文件描述符中读取数据所以说应该把数据写到用户级缓冲区（readBuff中），
    //  因此，获取的应该是写指针
    struct iovec iov[2];
    const size_t writable = WritableBytes();
    
    /* 分散读fd， 保证数据全部读完 */
    //  第一块映射到readBuff的可写空间的其实位置（获取readBuff的写指针位置，映射到第一块的iov_base中）；
    //  第二块则是为了防止readBuff当前空间不够，所以临时申请的buff空间，映射到这一块来；
    iov[0].iov_base = BeginPtr_() + writePos_;
    iov[0].iov_len = writable;
    iov[1].iov_base = buff;
    iov[1].iov_len = sizeof(buff);

    const ssize_t len = readv(fd, iov, 2);
    if(len < 0) {
        *saveErrno = errno;
    }
    // 读出的长度小于readBuff的剩余空间
    else if(static_cast<size_t>(len) <= writable) {
        writePos_ += len;
    }
    // 读出的长度大于readBuff的剩余空间，扩容之后将buff数据复制到里面去
    else {
        writePos_ = buffer_.size();
        Append(buff, len - writable);
    }
    return len;
}

ssize_t Buffer::WriteFd(int fd, int* saveErrno) {
    size_t readSize = ReadableBytes();
    ssize_t len = write(fd, Peek(), readSize);
    if(len < 0) {
        *saveErrno = errno;
        return len;
    } 
    readPos_ += len;
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

//用于申请更多的空间
void Buffer::MakeSpace_(size_t len) {
    //不够空间
    if(WritableBytes() + PrependableBytes() < len) {
        //vector重新申请一块能够满足大小的空间（原来的数据也会被一并复制过去）
        buffer_.resize(writePos_ + len + 1);
    } 
    //空间足够但是不连续，则需要把readPos_到writePos_之间的数据移到开头，并重置读写指针
    else {
        size_t readable = ReadableBytes();
        std::copy(BeginPtr_() + readPos_, BeginPtr_() + writePos_, BeginPtr_());
        readPos_ = 0;
        writePos_ = readPos_ + readable;
        assert(readable == ReadableBytes());
    }
}