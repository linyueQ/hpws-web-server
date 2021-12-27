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
    if(connQue_.empty()){
        LOG_WARN("SqlConnPool busy!");
        return nullptr;
    }
    //有连接可用的时候才继续执行（这里处理线程可能会阻塞）
    //  应该设置一个定时器专门处理超时操作
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
