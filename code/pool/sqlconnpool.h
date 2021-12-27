#ifndef SQLCONNPOOL_H
#define SQLCONNPOOL_H

#include <mysql/mysql.h>
#include <string>
#include <queue>
#include <mutex>
#include <semaphore.h>
#include <thread>
#include "../log/log.h"

//SqlConnPool的实现方式和ThreadPool略有不同：
// 1、ThreadPool是创建了任务队列，然后thread去取用队列中的task，
//      而ConnPool是任务取用可用连接并进行相应的业务处理；
// 2、ThreadPool会使用mutex+q.empty()实现互斥和任务取用，
//      而ConnPool是通过mutex+sem实现互斥和连接取用；
// 3、ConnPool使用了懒汉式单例模式（C++ 11 局部静态Instance直接获取实例）
class SqlConnPool {
public:
    //静态成员函数（用于获取实例）
    static SqlConnPool *Instance();

    MYSQL *GetConn();               //获取连接
    void FreeConn(MYSQL * conn);    //归还连接
    // int GetFreeConnCount();         //查看空闲连接数（没啥用）

    //Init进行数据库连接，事实上所有数据库的连接都是用我们自己创建的
    //  用户来登录的，dbName也只有一个
    void Init(const char* host, int port,
              const char* user,const char* pwd, 
              const char* dbName, int connSize);
    void ClosePool();

private:
    SqlConnPool();
    ~SqlConnPool();

    int MAX_CONN_;  // 最大的连接数
    int useCount_;  // 当前的用户数
    int freeCount_; // 空闲的用户数

    std::queue<MYSQL *> connQue_;   // 可用连接队列（MYSQL *）
    std::mutex mtx_;                // 互斥锁
    sem_t semId_;                   // 信号量
};


#endif // SQLCONNPOOL_H