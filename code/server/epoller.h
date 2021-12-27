#ifndef EPOLLER_H
#define EPOLLER_H

#include <sys/epoll.h> //epoll_ctl()
#include <fcntl.h>  // fcntl()
#include <unistd.h> // close()
#include <assert.h> // close()
#include <vector>
#include <errno.h>

//将epoll的操作全部封装了一波，包括
//构造函数:epoll_create
//epoll_ctl(epollFd_, EPOLL_CTL_ADD, listenFd,&epev)
//epoll_ctl(epollFd_, EPOLL_CTL_MOD, fd, &epev);
//epoll_ctl(epollFd_, EPOLL_CTL_DEL, fd, &epev);
//epoll_wait(epollFd_,&events_,events.size(),timeOutMs);
class Epoller {
public:
    explicit Epoller(int maxEvent = 1024);

    ~Epoller();

    bool AddFd(int fd, uint32_t events);

    bool ModFd(int fd, uint32_t events);

    bool DelFd(int fd);

    int Wait(int timeoutMs = -1);

    int GetEventFd(size_t i) const;

    uint32_t GetEvents(size_t i) const;
        
private:
    int epollFd_;   // epoll_create()创建一个epoll对象，返回值就是epollFd
    std::vector<struct epoll_event> events_;    // 检测到的事件的集合（epoll_event数组）
};

#endif //EPOLLER_H