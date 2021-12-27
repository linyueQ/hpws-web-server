/* 线程池整体思路
 * 
 */
#include <functional>
#include <thread>
#include <mutex>
#include <condition_variable>
#include <vector>
#include <queue>
#include <future>
#include <atomic>
#include <stdexcept>

class MyThreadPool{
private:
    using Task=std::function<void()>;
    struct Pool{
        std::vector<std::thread> threads;  // 存放线程，可扩容
        std::queue<Task> tasks;            // 存放待处理任务
        std::mutex mtx;                    // 锁tasks
        std::atomic<int> idleThreadNum;    // 有多少个空闲的线程
        std::condition_variable cond;      // 避免线程反复无效地获取mtx
        std::atomic<bool> isClosed;        // 标识线程池是否关闭
    };
    std::shared_ptr<Pool> _pool;
    const int maxThreadNum;                  // 线程池扩容后最多多少个线程

public:
    MyThreadPool()=delete;
    explicit MyThreadPool(size_t _initThreadNum=8, size_t _maxThreadNum=16):
        maxThreadNum(_maxThreadNum), _pool(std::make_shared<Pool>()){
        std::shared_ptr<Pool>& pool=_pool;
        AddThreads(_initThreadNum);
        pool->idleThreadNum = _initThreadNum;
        pool->isClosed = false;
    }
    MyThreadPool(const MyThreadPool&)=delete;
    MyThreadPool(MyThreadPool&&)=delete;
    ~MyThreadPool(){
        std::shared_ptr<Pool>& pool=_pool;
        {
            std::lock_guard<std::mutex> locker(pool->mtx);
            pool->isClosed=true;
        }
        //把资源释放以后必须唤醒线程，避免之前由于cond.wait导致有等待线程的存在
        pool->cond.notify_all();
        //等待所有thread退出才回收线程池相关资源
        for(auto& th:pool->threads){
            if(th.joinable()){
                th.join();
            }
        }
    }
    
    bool AddThreads(int threadCount){
        //检查线程数量是否安全
        std::shared_ptr<Pool>& pool=_pool;
        if (threadCount < 0 || pool->threads.size() + threadCount > maxThreadNum) {
            printf("[ThreadCount invalid] or [too many threads to hold], stop adding threads!\n");
            return false;
        }
        //创建threadCount个线程，并存起来
        for (int i = 0; i < threadCount; i++) {
            pool->threads.emplace_back(std::thread(&MyThreadPool::ThreadCreate,this));
            pool->idleThreadNum++;
        }
        return true;
    }

    void ThreadCreate(){
        std::shared_ptr<Pool>& pool=_pool;
        Task task=nullptr;
        while(true){
            //这里这么写的最大缺点是它每次都要创建一个unique_lock
            std::unique_lock<std::mutex> locker(pool->mtx);
            if(!pool->tasks.empty()){
                task=move(pool->tasks.front());
                pool->tasks.pop();
                locker.unlock();
                pool->idleThreadNum--;
                task();
                pool->idleThreadNum++;
            }else if(pool->isClosed){
                break;
            }else{//条件变量等待通知
                pool->cond.wait(locker);
            }
        }
    }

	//用于添加任务函数，但任务函数的参数值不能为右值引用，否则编译会出错
	// 另外，需要调用者保证异常的处理，异常也会存放在返回值中
    //PS：返回类型那里编译器会误报
	template<class F, class... ARGS>
	auto AddTask(F&& f, ARGS&&... args)->std::future<decltype(f(args...))> {
		std::shared_ptr<Pool>& pool = _pool;
		//Step1：检查线程池是否仍然可用
		if (pool->isClosed) throw std::runtime_error("Thread pool has closed!\n");
		//Step2：创建任务实例
		using retType = decltype(f(args...));
		auto task = std::make_shared<std::packaged_task<retType()>>(
			std::bind(std::forward<F>(f), std::forward<ARGS>(args)...)
		);
		//Step3：创建future获取异步返回值
		std::future<retType> fu = task->get_future();
		//Step4：将任务加入队列
		{
			std::lock_guard<std::mutex> locker(pool->mtx);
			//这里相当于又包装了一层匿名函数再放到里面
			pool->tasks.emplace([task]() { 
				(*task)();
			});
		}
		//Step5：检查线程池是否需要创建更多线程来处理任务
		// printf("idleThreadNum = %d\n", pool->idleThreadNum.load());
		if (pool->idleThreadNum.load() < 4 && pool->threads.size() + 4 <= maxThreadNum)
			AddThreads(4);
		//Step6：唤醒一个线程进行处理，同时避免虚假唤醒
		pool->cond.notify_one();
		return fu;
	}
};