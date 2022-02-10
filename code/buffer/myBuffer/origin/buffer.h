#ifndef BUFFER_H
#define BUFFER_H
#include <cstring>   //perror
#include <iostream>
#include <unistd.h>  // write
#include <sys/uio.h> //readv
#include <vector> //readv
#include <atomic>
#include <assert.h>

//用户级缓冲区，每个socket连接都创建了用户级的ReadBuff和WriteBuff
class Buffer {
public:
    Buffer(int initBuffSize = 1024);
    ~Buffer() = default;

    size_t WritableBytes() const;       
    size_t ReadableBytes() const ;
    size_t PrependableBytes() const;

    const char* Peek() const;
    void EnsureWriteable(size_t len);
    void HasWritten(size_t len);

    //回收资源
    void Retrieve(size_t len);
    void RetrieveUntil(const char* end);

    void RetrieveAll() ;
    std::string RetrieveAllToStr();

    const char* BeginWriteConst() const;
    char* BeginWrite();

    //将字符串数据写入缓冲区
    void Append(const std::string& str);
    void Append(const char* str, size_t len);
    void Append(const void* data, size_t len);
    void Append(const Buffer& buff);

    //将fd的数据
    ssize_t ReadFd(int fd, int* Errno); 
    ssize_t WriteFd(int fd, int* Errno);

private:
    char* BeginPtr_();              // 获取内存起始位置
    const char* BeginPtr_() const;  // 获取内存起始位置
    void MakeSpace_(size_t len);    // 创建空间

    std::vector<char> buffer_;  // 具体装数据的vector（换成deque感觉会好些），此外可以做成ringBuffer（避免总要往前挪数据）
    std::atomic<std::size_t> readPos_;  // 读的位置（读指针）
    std::atomic<std::size_t> writePos_; // 写的位置（写指针）
};

#endif //BUFFER_H