#include "rbtimer.h"

void RBTimer::add(int id, int timeout, const TimeoutCallBack& cb) {
    assert(id >= 0);
    
    /* add的结点本来就存在：则通过ref_获取该节点的rbKey，然后删除该节点，更新时间并重新插入到树中 */
    if(ref_.count(id) > 0) {
        auto nodePtr = rbtree_.erase(ref_[id]->key);
    }
    /* add的结点为新节点，维护ref_*/
    TimeoutCallBack* cb_ptr=new TimeoutCallBack(cb);
    rbKey rbk;
    rbk.id = id;
    rbk.expire = Clock::now() + (MS)timeout;

    TreeNode<rbKey> *node_ptr;
    rbtree_.insert(rbk, cb_ptr, node_ptr);
    ref_[id] = node_ptr; //深拷贝
}

void RBTimer::del(int id) {
    /* 删除指定id的结点，并维护ref_ */
    assert(rbtree_.size()>0 && ref_.count(id)>0);
    
    rbtree_.erase(ref_[id]->key);
    ref_.erase(id);
}

MS RBTimer::getExpire(int id){
    /* 获取剩余的过期时间 */
    assert(ref_.count(id)>0);

    auto expire = ref_[id]->key.expire;
    return std::chrono::duration_cast<MS>(expire - Clock::now());
}

void RBTimer::adjust(int id, int timeout) {
    /* 找到该节点，删除并重新加入该节点 */
    assert(rbtree_.size()>0 && ref_.count(id)>0);

    TimeoutCallBack *cb_ptr = static_cast<TimeoutCallBack *>(ref_[id]->value);
    rbKey rbk;
    rbk.id = id;
    rbk.expire = Clock::now() + (MS)timeout;

    del(id);

    TreeNode<rbKey> *node_ptr=nullptr;
    rbtree_.insert(rbk, cb_ptr, node_ptr);
    ref_[id] = node_ptr; //深拷贝
}

void RBTimer::clear() {
    ref_.clear();
    rbtree_.clear();
}

void RBTimer::tick() {
    /* 清除超时结点 */
    if(rbtree_.size()==0) {
        return;
    }
    while(rbtree_.size()>0) {
        auto node = rbtree_.getMin();
        if(std::chrono::duration_cast<MS>(node->key.expire - Clock::now()).count() > 0) { 
            //定时器还没到时间
            break;
        }
        //回调函数（其实应该交给线程池去做）
        (*static_cast<TimeoutCallBack*>(node->value))();
        //删除节点
        del(node->key.id);
    }
}

void RBTimer::pop() {
    assert(rbtree_.size()>0);
    rbtree_.popMin();
}


int RBTimer::getNextTick() {
    tick();
    size_t res = -1;
    //返回距离下一个事件过期还要过多久（指导epoll_wait的等待时间）
    if(rbtree_.size()>0) {
        auto node = rbtree_.getMin();
        res = std::chrono::duration_cast<MS>(node->key.expire - Clock::now()).count();
        if(res < 0) { res = 0; }
    }
    return res;
}