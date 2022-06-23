#ifndef HEAP_TIMER_H
#define HEAP_TIMER_H

/**********************************************************************
 * -------------------------------Timer--------------------------------
 * 
 * 这里的定时器有点特殊，它是通过chrono类进行实现的，并没有用setitimer，也
 * 没有利用信号机制；值得注意的是，整个定时器其实只有在epoll_wait的循环中
 * 用到，并没有用在其他位置（事实上定时器还可以用于线程池缩容检查）
 * 
 * 基于红黑树实现的timer
 * 
***********************************************************************/

#include <unordered_map>
#include <time.h>
#include <algorithm>
#include <arpa/inet.h> 
#include <functional> 
#include <assert.h> 
#include <chrono>
#include <memory>
#include <utility>

#include "rbtree.h"
#include "../log/log.h"

typedef std::function<void()> TimeoutCallBack;      // 异步回调函数
typedef std::chrono::high_resolution_clock Clock;   // 时钟类
typedef std::chrono::milliseconds MS;               // 毫秒
typedef Clock::time_point TimeStamp;                // 时间戳

// 封装成rbKey，避免红黑树认为同一个时间戳不同id的任务是冲突的
struct rbKey{
    int id;
    TimeStamp expire;
	rbKey(){
        id=-1;
    }
	rbKey(const rbKey& a){
		id=a.id;
		expire=a.expire;
	}
	rbKey(rbKey&& a){
		id=a.id;
		expire=a.expire;
	}
	rbKey& operator=(const rbKey& a){
		id=a.id;
		expire=a.expire;
		return *this;
	}
	bool operator==(const rbKey& a) const{
		return id==a.id && expire==a.expire;
	}
    bool operator<(const rbKey& a) const{
        if(expire < a.expire){
            return true;
        }else if(expire > a.expire){
            return false;
        }else{
			return id < a.id;
		}
    }
	bool operator>(const rbKey& a) const{
        if(expire > a.expire){
            return true;
        }else if(expire < a.expire){
            return false;
        }else{
            return id > a.id;
        }
    }
};

class RBTimer {
public:
    RBTimer() {}

    ~RBTimer() { clear(); }

    void add(int id, int timeOut, const TimeoutCallBack& cb);   // 添加定时事件

    void del(int id);                       // 根据id删除节点

    MS getExpire(int id);                   // 通过id获取某个事件还有多久过期

    void adjust(int id, int timeout);       // 延迟事件的响应，更新事件的过期时间

    void clear();                           // 清空红黑树

    void tick();                            // 处理堆中所有的超时事务

    void pop();                             // 弹出最早超时的节点    

    int getNextTick();                      // 调用tick()，并返回距离下一个要过期的事件还要过多久

private:
    rbtree<rbKey> rbtree_;                             // 一棵key类型为TimeStamp的红黑树
    std::unordered_map<int,TreeNode<rbKey>*> ref_;     // <id,TreeNode*> 用于去重，保证id相异，并获取超时时间在树中的位置
};

#endif //HEAP_TIMER_H