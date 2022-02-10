#ifndef THREADPOOL_H
#define THREADPOOL_H

#include <mutex>
#include <condition_variable>
#include <queue>
#include <thread>
#include <functional>
#include <assert.h>

//线程池是基于“生产者-消费者”模型来写的
class MyThreadPool {
public:
    //直接把线程池逻辑写在构造函数里面
    //使用shared_ptr所创建的实例能够避免忘记释放相应对象（当引用数为0时会自动析构对象）
    explicit MyThreadPool(size_t threadCount = 8): pool_(std::make_shared<Pool>()) {
            assert(threadCount > 0);

            // 创建threadCount个子线程
            for(size_t i = 0; i < threadCount; i++) {
                //调用thread函数创建并执行线程，使用lambda来写函数逻辑
                std::thread([pool = pool_] {
                    //unique_lock默认是在构造函数中获取mtx，并执行lock操作
                    std::unique_lock<std::mutex> locker(pool->mtx);
                    while(true) {
                        if(!pool->tasks.empty()) {
                            // 从任务队列中取第一个任务
                            // move接受一个参数，然后返回一个该参数对应的右值引用（无论这个参数是左值引用还是右值引用）
                            // 这样做能够减少拷贝开销（相当于将pool->tasks.front()的地址交给了task，然后清空了task.front()
                            // 切换指向后的空间，所以释放也很简单）
                            // PS：move本质上是一个强制类型转换
                            auto task = std::move(pool->tasks.front());
                            // 移除掉队列中第一个元素
                            pool->tasks.pop();
                            // 重复unlock没有问题，但重复lock就会发生死锁
                            // 这里先unlock再lock其实才是正确的，因为unique_lock的构造函数会默认自动进行lock操作
                            locker.unlock();
                            // 执行task的时候不应该一直占用锁，所以释放掉
                            task();
                            locker.lock();
                        } 
                        // 如果线程池已经关闭，那么直接跳出循环，线程结束
                        else if(pool->isClosed) break;
                        // 如果队列为空，则等待（之所以要放在最后是因为cond可能虚假唤醒）
                        else pool->cond.wait(locker);   
                    }
                    //由于使用了unique_lock，所以无需手动unlock，unique_lock会在作用域结束的时候自动解锁
                }).detach();// 线程分离（无需在主线程使用wait来回收资源）
            }
    }

    //=default表示使用默认定义的无参构造函数，将该类保持为POD数据类型
    MyThreadPool() = default;

    //Type&& 是右值引用
    MyThreadPool(MyThreadPool&&) = default;
    
    ~MyThreadPool() {
        if(static_cast<bool>(pool_)) {
            {
                //lock_guard是一个互斥量包装程序，它提供了一种方便的RAII（Resource acquisition is initialization）
                //风格的机制来在作用域块的持续时间内拥有一个互斥量
                std::lock_guard<std::mutex> locker(pool_->mtx);
                pool_->isClosed = true;
            }
            //让其他线程不要等了（不会有新任务进来）
            //  PS：这样做其实不安全，因为mutex可能被释放掉了，然后那边仍有线程在运行，
		    //	    mutex在类析构以后没掉了，但线程池由于是detach()的，仍然有可能
            //      会访问mutex，这时就会出问题
            pool_->cond.notify_all();
        }
    }

    template<class F>
    void AddTask(F&& task) {
        {
            std::lock_guard<std::mutex> locker(pool_->mtx);
            //forward接受一个参数，然后返回该参数本来所对应的类型的引用
            pool_->tasks.emplace(std::forward<F>(task));
        }
        // 唤醒一个等待的线程（避免cond的虚假唤醒问题）
        pool_->cond.notify_one();
    }

private:
    // 结构体
    struct Pool {
        std::mutex mtx;                             // 互斥锁
        std::condition_variable cond;               // 条件变量
        bool isClosed;                              // 是否关闭
        std::queue<std::function<void()>> tasks;    // 队列（保存的是任务对应的函数）
    };
    std::shared_ptr<Pool> pool_;  //  池子
};


#endif //THREADPOOL_H