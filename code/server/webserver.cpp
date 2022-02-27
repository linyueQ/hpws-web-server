#include "webserver.h"

using namespace std;

WebServer::WebServer(
            int port, int trigMode, int timeoutMS, bool OptLinger,
            int sqlPort, const char* sqlUser, const  char* sqlPwd,
            const char* dbName, int connPoolNum, int threadNum,
            bool openLog, int logLevel, int logQueSize):
            port_(port), openLinger_(OptLinger), timeoutMS_(timeoutMS), isClose_(false),
            timer_(new HeapTimer()), threadpool_(new MyThreadPool(threadNum)), epoller_(new Epoller())
    {
    // Step1：获取HTTP服务器的资源目录（装了各种各样的html文件）
    // /home/linyueq/WebServer-master/
    srcDir_ = getcwd(nullptr, 256); // 获取当前的工作路径
    assert(srcDir_);
    // /home/linyueq/WebServer-master/resources/
    strncat(srcDir_, "/resources/", 16);    // 拼接出资源目录
    
    // 初始化各种资源
    // Step2：初始化HttpConn的静态成员，客户端连接进来后会封装成HttpConn
    HttpConn::userCount = 0;        //当前所有连接数
    HttpConn::srcDir = srcDir_;     //设置资源目录

    // 初始化数据库连接池
    SqlConnPool::Instance()->Init("localhost", sqlPort, sqlUser, sqlPwd, dbName, connPoolNum);

    // 初始化epoll事件的模式（指EPOLL的触发模式、以及最开始要监听什么类型的事件）
    InitEventMode_(trigMode);
    
    // 初始化网络通信相关的一些内容
    if(!InitSocket_()) { isClose_ = true;}
    // printf("Init before Log init\n");
    if(openLog) {
        // 初始化日志信息
        Log::Instance()->init(logLevel, "./log", ".log", logQueSize);
        if(isClose_) { LOG_ERROR("========== Server init error!=========="); }
        else {
            LOG_INFO("========== Server init ==========");
            LOG_INFO("Port:%d, OpenLinger: %s", port_, OptLinger? "true":"false");
            LOG_INFO("Listen Mode: %s, OpenConn Mode: %s",
                            (listenEvent_ & EPOLLET ? "ET": "LT"),
                            (connEvent_ & EPOLLET ? "ET": "LT"));
            LOG_INFO("LogSys level: %d", logLevel);
            LOG_INFO("srcDir: %s", HttpConn::srcDir);
            LOG_INFO("SqlConnPool num: %d, ThreadPool num: %d", connPoolNum, threadNum);
        }
    }
    // printf("Init after Log init\n");
}

WebServer::~WebServer() {
    close(listenFd_);
    isClose_ = true;
    free(srcDir_);
    SqlConnPool::Instance()->ClosePool();
}

// 设置 监听的文件描述符 和 通信的文件描述符 的模式（默认是水平触发）
void WebServer::InitEventMode_(int trigMode) {
    //之前epoll使用的时候我们都是根据read(curfd, buf, sizeof(buf))返回是否
    //  为0来判断连接是否断开，但实际上Linux在2.6.17后提供了一种名为
    //  EPOLLRDHUP的事件，用于监听对端是否关闭连接(对端调用close 或者 shutdown(SHUT_WR))
    //  PS：EPOLLHUP则表示读写都关闭
    listenEvent_ = EPOLLRDHUP;
    //EPOLLONESHOT用于避免同一个Socket由于先后发生不同事件而分配给不同进程
    //  每次监听完成后都会清空epoll_event里面的监听事件（包括EPOLLONESHOT），
    //  需要等待上一个事件完成后，调用epoll_ctl重新设置
    connEvent_ = EPOLLONESHOT | EPOLLRDHUP;

    switch (trigMode)
    {
    case 0:
        break;
    case 1:
        connEvent_ |= EPOLLET;
        break;
    case 2:
        listenEvent_ |= EPOLLET;
        break;
    case 3: //listen和conn都是通过边沿触发
        listenEvent_ |= EPOLLET;
        connEvent_ |= EPOLLET;
        break;
    default:
        listenEvent_ |= EPOLLET;
        connEvent_ |= EPOLLET;
        break;
    }
    HttpConn::isET = (connEvent_ & EPOLLET);
}

// 启动服务器（服务器运行期间都会在while循环中进行）
void WebServer::Start() {
    // printf("WebServer start\n");
    int timeMS = -1;  /* epoll wait timeout == -1 无事件将阻塞 */
    if(!isClose_) { LOG_INFO("========== Server start =========="); }
    //下面这里就是服务器的监听逻辑了，其实就是epoll里面的epoll_wait主线程
    while(!isClose_) {

        // 如果设置了超时时间，例如60s,则只要一个连接60秒没有读写操作，则关闭
        if(timeoutMS_ > 0) {
            // 通过定时器GetNextTick(),清除超时的节点，然后获取最先要超时的连接的超时时间
            timeMS = timer_->GetNextTick();
        }

        // timeMS是最先要超时的连接的超时的时间，传递到epoll_wait()函数中
        // 当timeMS时间内有事件发生，epoll_wait()返回，否则等到了timeMS时间后才返回
        // 这样做的目的是为了让epoll_wait()调用次数变少，提高效率
        int eventCnt = epoller_->Wait(timeMS);

        // printf("\n==============epoller_wait start==============\n");
        // 循环处理每一个事件
        for(int i = 0; i < eventCnt; i++) {
            /* 处理事件 */
            int fd = epoller_->GetEventFd(i);   // 获取事件对应的fd
            uint32_t events = epoller_->GetEvents(i);   // 获取事件的类型
            
            // 监听的文件描述符有事件，说明有新的连接进来
            if(fd == listenFd_) {
                DealListen_();  // 处理监听的操作，接受客户端连接
            }
            
            // 需要终止HTTP连接的一些情况
            else if(events & (EPOLLRDHUP | EPOLLHUP | EPOLLERR)) {
                // printf("events error or EPOLLRDHUP|EPOLLHUP!\n");
                assert(users_.count(fd) > 0);
                CloseConn_(&users_[fd]);    // 关闭连接
            }

            // 有数据到达
            else if(events & EPOLLIN) {
                // printf("start to read data\n");
                assert(users_.count(fd) > 0);
                DealRead_(&users_[fd]); // 处理读操作
            }
            
            // 可以发送数据
            else if(events & EPOLLOUT) {
                // printf("start to write data\n");
                assert(users_.count(fd) > 0);
                DealWrite_(&users_[fd]);    // 处理写操作
            } else {
                LOG_ERROR("Unexpected event");
            }
        }
    }
}

// 发送错误提示信息
void WebServer::SendError_(int fd, const char*info) {
    assert(fd > 0);
    int ret = send(fd, info, strlen(info), 0);
    if(ret < 0) {
        LOG_WARN("send error to client[%d] error!", fd);
    }
    close(fd);
}

// 关闭连接（从epoll中删除，解除响应对象中的内存映射，用户数递减，关闭文件描述符）
void WebServer::CloseConn_(HttpConn* client) {
    assert(client);
    LOG_INFO("Client[%d] quit!", client->GetFd());
    epoller_->DelFd(client->GetFd());
    client->Close();
}

// 添加客户端
void WebServer::AddClient_(int fd, sockaddr_in addr) {
    assert(fd > 0);
    //初始化客户端连接（直接存到unordered_map里面了）
    users_[fd].init(fd, addr);
    if(timeoutMS_ > 0) {
        // 添加到定时器对象中，当检测到超时时执行CloseConn_函数进行关闭连接
        timer_->add(fd, timeoutMS_, std::bind(&WebServer::CloseConn_, this, &users_[fd]));
    }
    // 添加到epoll中进行管理
    epoller_->AddFd(fd, EPOLLIN | connEvent_);
    // 设置文件描述符非阻塞
    SetFdNonblock(fd);
    LOG_INFO("Client[%d] in!", users_[fd].GetFd());
}

void WebServer::DealListen_() {
    struct sockaddr_in addr; // 保存连接的客户端的信息
    socklen_t len = sizeof(addr);
    // 如果监听文件描述符设置的是 ET模式，则需要循环把所有连接处理了，否则只需处理一次
    do {
        int fd = accept(listenFd_, (struct sockaddr *)&addr, &len);
        if(fd <= 0) return;//非阻塞，accept没有客户端连接请求会直接返回-1
        else if(HttpConn::userCount >= MAX_FD) {
            SendError_(fd, "Server busy!"); //send和write用起来一样
            LOG_WARN("Clients is full!");
            return;
        }
        AddClient_(fd, addr);   // 添加客户端
    } while(listenEvent_ & EPOLLET);
}

// 处理读
void WebServer::DealRead_(HttpConn* client) {
    assert(client);
    ExtentTime_(client);   // 延长这个客户端的超时时间
    // 加入到队列中等待线程池中的线程处理（读取数据）——这里绑定的是成员函数，有this指针
    threadpool_->AddTask(std::bind(&WebServer::OnRead_, this, client));
}

// 处理写
void WebServer::DealWrite_(HttpConn* client) {
    assert(client);
    ExtentTime_(client);// 延长这个客户端的超时时间
    // 加入到队列中等待线程池中的线程处理（写数据）——这里绑定的是成员函数，有this指针
    threadpool_->AddTask(std::bind(&WebServer::OnWrite_, this, client));
}

// 延长客户端的超时时间
void WebServer::ExtentTime_(HttpConn* client) {
    assert(client);
    if(timeoutMS_ > 0) { timer_->adjust(client->GetFd(), timeoutMS_); }
}

// 这个方法是在子线程中执行的（读取数据），这是一个状态（先读取数据）
void WebServer::OnRead_(HttpConn* client) {
    // printf("Start OnRead\n");
    assert(client);
    int ret = -1;
    int readErrno = 0;
    // 读数据-->用户缓冲区（对应HttpConn对象的readBuffer里面）
    ret = client->read(&readErrno); 
    // printf("%d\n",ret);
    if(ret <= 0 && readErrno != EAGAIN) {
        CloseConn_(client);
        return;
    }
    // 业务逻辑的处理
    OnProcess(client);
}

// 业务逻辑的处理，处理结束后更新事件
void WebServer::OnProcess(HttpConn* client) {
    bool status = client->process();
    if(status) {//处理成功，刷新epev事件，监听业务数据什么时候准备好可以进行发送
        epoller_->ModFd(client->GetFd(), connEvent_ | EPOLLOUT);
    } else { //无处理数据，刷新epev事件，然后继续监听EPOLL_IN信息
        epoller_->ModFd(client->GetFd(), connEvent_ | EPOLLIN);
    }
}

// 写数据
void WebServer::OnWrite_(HttpConn* client) {
    assert(client);
    int ret = -1;
    int writeErrno = 0;
    // 写数据
    ret = client->write(&writeErrno);   

    // 如果将要写的字节等于0，说明写完了，判断是否要保持连接，保持连接继续去处理
    if(client->ToWriteBytes() == 0) {
        /* 传输完成 */
        if(client->IsKeepAlive()) {
            //连接如果是keep-alive的话，我们处理完当前数据可能还会有其他数据达到
            //  因此，我们应该再次检查读缓冲区里面有没有数据，调用client->process()，
            //  如果读缓冲区里面真的没有数据要处理了，我们才在epoll中重新注册EPOLL_IN事件，
            //  继续进行监听
            OnProcess(client);
            return;
        }
    }
    else if(ret < 0) {//考虑到有可能会因为写缓冲区满了，导致用户缓冲区有数据没传完的情况，所以要判断EAGAIN
        if(writeErrno == EAGAIN) {
            /* 继续传输 */
            epoller_->ModFd(client->GetFd(), connEvent_ | EPOLLOUT);
            return;
        }
    }
    CloseConn_(client);
}

/* Create listenFd */
bool WebServer::InitSocket_() {
    //Step1：设置listenFd
    int ret;
    struct sockaddr_in addr;
    if(port_ > 65535 || port_ < 1024) {
        LOG_ERROR("Port:%d error!",  port_);
        return false;
    }
    addr.sin_family = AF_INET;
    addr.sin_addr.s_addr = htonl(INADDR_ANY);
    addr.sin_port = htons(port_);

    //Step2：获取监听socket的文件描述符
    listenFd_ = socket(AF_INET, SOCK_STREAM, 0);
    if(listenFd_ < 0) {
        LOG_ERROR("Create socket error!", port_);
        return false;
    }

    //Step3：设置优雅关闭
    struct linger optLinger = { 0 };
    
    if(openLinger_) {
        /* 优雅关闭: 直到所剩数据发送完毕或超时 */
        optLinger.l_onoff = 1;  //是否等待缓冲区中所剩数据全部发送，并由对方应用接收后才（不仅仅是收到ACK）
        optLinger.l_linger = 1; //优雅关闭最长延迟多少时间，单位为s
    }
    ret = setsockopt(listenFd_, SOL_SOCKET, SO_LINGER, &optLinger, sizeof(optLinger));
    if(ret < 0) {
        close(listenFd_);
        LOG_ERROR("Init linger error!", port_);
        return false;
    }

    //Step4：设置端口复用
    // PS:只有最后一个绑定在该端口上的套接字会正常接收数据
    int optval = 1;
    ret = setsockopt(listenFd_, SOL_SOCKET, SO_REUSEADDR, (const void*)&optval, sizeof(int));
    if(ret == -1) {
        LOG_ERROR("set socket setsockopt error !");
        close(listenFd_);
        return false;
    }

    //Step5：bind+listen
    ret = bind(listenFd_, (struct sockaddr *)&addr, sizeof(addr));
    if(ret < 0) {
        LOG_ERROR("Bind Port:%d error!", port_);
        close(listenFd_);
        return false;
    }

    //第二个参数为backlog，min(backlog, somaxconn)共同影响半连接队列的代销
    //  backlog表示accept队列的大小
    ret = listen(listenFd_, 6);
    if(ret < 0) {
        LOG_ERROR("Listen port:%d error!", port_);
        close(listenFd_);
        return false;
    }

    //Step6：添加listen文件描述符到epollfd里面
    ret = epoller_->AddFd(listenFd_,  listenEvent_ | EPOLLIN);
    if(ret == 0) {
        LOG_ERROR("Add listen error!");
        close(listenFd_);
        return false;
    }
    //Step7：设置文件描述符非阻塞
    SetFdNonblock(listenFd_);
    LOG_INFO("Server port:%d", port_);
    return true;
}

// 设置文件描述符非阻塞
int WebServer::SetFdNonblock(int fd) {
    assert(fd > 0);
    // int flag = fcntl(fd, F_GETFD, 0);
    // flag = flag  | O_NONBLOCK;
    // // flag  |= O_NONBLOCK;
    // fcntl(fd, F_SETFL, flag);
    return fcntl(fd, F_SETFL, fcntl(fd, F_GETFD, 0) | O_NONBLOCK);
}


