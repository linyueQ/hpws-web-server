#ifndef SQLCONNRAII_H
#define SQLCONNRAII_H
#include "sqlconnpool.h"

/* 资源在对象构造初始化 资源在对象析构时释放*/
class SqlConnRAII {
public:
    //这里要传二级指针，因为我们希望改变的是外面的指针的指向，
    //  如果传的是一级指针，那即便改变了函数里面指针的指向，
    //  对外面的指针是不会产生影响的
    SqlConnRAII(MYSQL** sql, SqlConnPool *connpool) {
        assert(connpool);
        //这里没有做判断处理（GetConn可能返回nullptr）
        *sql = connpool->GetConn();
        sql_ = *sql;
        connpool_ = connpool;
    }
    
    ~SqlConnRAII() {
        if(sql_) { connpool_->FreeConn(sql_); }
    }
    
private:
    MYSQL *sql_;
    SqlConnPool* connpool_;
};

#endif //SQLCONNRAII_H