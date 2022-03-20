#include "sqlconnpool.h"
using namespace std;

SqlConnPool::SqlConnPool() {
    useCount_ = 0;
    freeCount_ = 0;
}

SqlConnPool* SqlConnPool::Instance() {
    static SqlConnPool connPool;
    return &connPool;
}

void SqlConnPool::Init(const char* host, int port,
            const char* user,const char* pwd, const char* dbName,
            int connSize = 10) {
    assert(connSize > 0);
    //数据库连接池实例化后就创建10个连接，使用的都是同一个用户
    //  （面向复杂的业务逻辑的话可能不是这么做的）
    for (int i = 0; i < connSize; i++) {
        MYSQL *sql = nullptr;
        sql = mysql_init(sql);
        if (!sql) {
            LOG_ERROR("MySql init error!");
            assert(sql);
        }
        sql = mysql_real_connect(sql, host,
                                 user, pwd,
                                 dbName, port, nullptr, 0);
        printf("SqlConnPool::GetConn()->connQue.front()--%s",connQue_.front()->user);
        if (!sql) {
            LOG_ERROR("MySql Connect error!");
        }
        //将建立好的连接放入连接队列中
        connQue_.push(sql);
    }
    MAX_CONN_ = connSize;
    sem_init(&semId_, 0, MAX_CONN_);//初始化信号量
}

MYSQL* SqlConnPool::GetConn() {
    MYSQL *sql = nullptr;
    //Step1：先用一个判断，将connQue为空时继续有其他线程进入的话，可以
    //  避免他们阻塞在sem_wait上
    if(connQue_.empty()){
        LOG_WARN("SqlConnPool busy!");
        return nullptr;
    }
    //Step2：有连接可用的时候才继续执行（这里处理线程可能会阻塞）
    //  应该设置一个定时器专门处理超时操作
    //为什么要有信号量？如果没有sem_wait，然后在lock_guard以后有检查queue.size()==0
    //  然后再决定是否释放锁，那么当连接池长时间为空，然后有其他线程想FreeConn时，
    //  很有可能因为获取不到锁而一直无法放回连接，另一边获取连接的线程却不断获取到
    //  锁，然后发现没有连接，又释放锁（多次无效地获取锁），极大地浪费了资源。所以，
    //  我们需要先加个信号量。当有信号量时，最多只有pool.size()个线程能够进入sem_wait
    //  后面的临界区，其他线程要么就在connQue.empty()处返回，要么就阻塞在sem_wait中
    //  没有其他线程会和FreeConn的线程争用mutex，大大减少了无效的上下文切换
    sem_wait(&semId_);
    {
        lock_guard<mutex> locker(mtx_);
        sql = connQue_.front();
        connQue_.pop();
    }
    return sql;
}

void SqlConnPool::FreeConn(MYSQL* sql) {
    assert(sql);
    lock_guard<mutex> locker(mtx_);
    connQue_.push(sql);
    sem_post(&semId_);
}

void SqlConnPool::ClosePool() {
    lock_guard<mutex> locker(mtx_);
    while(!connQue_.empty()) {
        auto item = connQue_.front();
        connQue_.pop();
        mysql_close(item);
    }
    mysql_library_end();
}

// int SqlConnPool::GetFreeConnCount() {
//     lock_guard<mutex> locker(mtx_);
//     return connQue_.size();
// }

SqlConnPool::~SqlConnPool() {
    ClosePool();
}
