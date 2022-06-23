#include <mysql/mysql.h>
#include <iostream>
#include <string>
#include <bits/stdc++.h>
#include <unistd.h>

using namespace std;

int main(){
    MYSQL *sql = mysql_init(nullptr);
    sleep(1);
    sql = mysql_init(sql);
    if (!sql) {
        cout<<"MySql init error!"<<endl;
        assert(sql);
    }
    sql = mysql_real_connect(sql, "linux",
                                "linyueq", "123456",
                                "webServer", 3306, nullptr, 0);
    if (!sql) {
        cout<<"MySql Connect error!"<<endl;
    }
    assert(sql);
    
    bool flag = false;
    unsigned int j = 0;
    char order[256]={0};        //存放sql命令
    MYSQL_FIELD *fields = nullptr;  //存放结果集的列信息（行数列数啥的）
    MYSQL_RES *res = nullptr;       //存放结果集
    
    /* 查询用户及密码（正常来说不应该这样写死，容易被SQL注入攻击） */
    snprintf(order, 256, "SELECT username, password FROM user WHERE username='%s' LIMIT 1", "linyueq");

    if(mysql_query(sql, order)) {       //执行mysql命令
        mysql_free_result(res);
        return false; 
    }
    res = mysql_store_result(sql);      //将数据库中查询(mysql_query)得到的结果(集合)存放在MYSQL_RES结构中
    j = mysql_num_fields(res);          //返回结果集中的列的数目
    fields = mysql_fetch_fields(res);   //返回结果集中的列信息
    MYSQL_ROW row = mysql_fetch_row(res);
    cout<<res<<endl;
    printf("MYSQL ROW: %s %s\n", row[0], row[1]);
}