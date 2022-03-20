#ifndef WEBSERVER_H
#define WEBSERVER_H

#include <unordered_map>
#include <fcntl.h>       // fcntl()
#include <unistd.h>      // close()
#include <assert.h>
#include <errno.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <signal.h>

#include "epoller.h"
#include "../log/log.h"
#include "../timer/heaptimer.h"
#include "../pool/sqlconnpool.h"
#include "../pool/mythreadpool.h"
#include "../pool/sqlconnRAII.h"
#include "../http/httpconn.h"

class WebServer {
public:
    WebServer(
        int port, int trigMode, int timeoutMS, bool OptLinger, 
        int sqlPort, const char* sqlUser, const  char* sqlPwd, const char* dbName,
        int connPoolNum, int threadNum, bool openLog, int logLevel, int logQueSize);

    ~WebServer();
    void Start();

private:
    bool InitSocket_();                         //初始化socket
    void InitEventMode_(int trigMode);          //初始化事件
    void AddClient_(int fd, sockaddr_in addr);  //新增用户连接
  
    void DealListen_();                         //封装listen需要处理的事务
    void DealWrite_(HttpConn* client);          //封装写事务
    void DealRead_(HttpConn* client);           //封装读事务

    void SendError_(int fd, const char*info);   //向客户端发送错误信息
    void ExtentTime_(HttpConn* client);         //延长超时时间
    void CloseConn_(HttpConn* client);          //关闭连接

    void OnRead_(HttpConn* client);             //服务器处于Read状态时调用
    void OnWrite_(HttpConn* client);            //服务器处于Write状态时调用
    void OnProcess(HttpConn* client);           //服务器处于Process状态时调用

    static const int MAX_FD = 65536;    // 最大的文件描述符的个数

    static int SetFdNonblock(int fd);   // 设置文件描述符非阻塞

    int port_;                          // 服务器接收的端口
    bool openLinger_;                   // 是否打开优雅关闭
    int timeoutMS_;                     // 连接的超时时间，单位ms 
    bool isClose_;                      // 是否关闭
    int listenFd_;                      // 监听的文件描述符
    char* srcDir_;                      // 资源的目录
    
    uint32_t listenEvent_;              // 监听的文件描述符的事件
    uint32_t connEvent_;                // 连接的文件描述符的事件
   
    std::unique_ptr<HeapTimer> timer_;          // 定时器
    std::unique_ptr<MyThreadPool> threadpool_;  // 线程池
    std::unique_ptr<Epoller> epoller_;          // epoll对象
    std::unordered_map<int, HttpConn> users_;   // 客户端连接的信息【文件描述符，HttpConn】
};


#endif //WEBSERVER_H