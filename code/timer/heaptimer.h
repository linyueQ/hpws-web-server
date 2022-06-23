#ifndef HEAP_TIMER_H
#define HEAP_TIMER_H

/**********************************************************************
 * -------------------------------Timer--------------------------------
 * 
 * 这里的定时器有点特殊，它是通过chrono类进行实现的，并没有用setitimer，也
 * 没有利用信号机制；值得注意的是，整个定时器其实只有在epoll_wait的循环中
 * 用到，并没有用在其他位置（事实上定时器还可以用于线程池缩容检查）
 * 
***********************************************************************/

#include <queue>
#include <unordered_map>
#include <time.h>
#include <algorithm>
#include <arpa/inet.h> 
#include <functional> 
#include <assert.h> 
#include <chrono>
#include "../log/log.h"

typedef std::function<void()> TimeoutCallBack;      //异步回调函数
typedef std::chrono::high_resolution_clock Clock;   //时钟类
typedef std::chrono::milliseconds MS;               //毫秒
typedef Clock::time_point TimeStamp;                //时间戳

struct TimerNode {
    int id;                 //定时器id
    TimeStamp expires;      //要过期的时间
    TimeoutCallBack cb;     //回调函数
    bool operator<(const TimerNode& t) {
        return expires < t.expires;
    }
};
class HeapTimer {
public:
    HeapTimer() { heap_.reserve(64); }

    ~HeapTimer() { clear(); }
    
    void adjust(int id, int newExpires);    //连接有操作时，更新事件的过期时间

    void add(int id, int timeOut, const TimeoutCallBack& cb);

    void doWork(int id);

    void clear();

    void tick();

    void pop();

    int getNextTick();      //处理堆中所有的超时事务，并返回距离下一个要过期的事件还要过多久

private:
    void del_(size_t i);
    
    void siftup_(size_t i);

    bool siftdown_(size_t index, size_t n);

    void SwapNode_(size_t i, size_t j);

    std::vector<TimerNode> heap_;

    std::unordered_map<int, size_t> ref_;   //<id,在数组中的idx>
};

#endif //HEAP_TIMER_H