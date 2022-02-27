# WebServer
用C++14实现了一个高性能WEB服务器，并使用了webbench对index.html进行了并发访问测试

## 功能
* 利用IO多路复用技术epoll和线程池实现了Reactor高并发模型；
* 基于C++11新特性实现了一个支持异步返回结果的线程池；
* 使用C++11的有限状态机和正则表达式逐行解析HTTP请求报文，实现了静态资源请求的处理；
* 使用STL封装char模拟队列结构，实现了具备扩容能力的RingBuffer用户级缓冲区；
* 基于小根堆实现了的连接定时器，用于关闭超时的非活跃连接；
* 利用单例模式（懒汉式）和阻塞队列（deque+mutex）实现异步的日志系统，在多线程下记录服务器的运行状态；
* 利用RAII机制实现了数据库连接池，减少数据库连接反复建立与关闭的开销，同时实现了用户注册登录功能;

## 环境
* Linux-Ubuntu 18.04
* C++11/14
* MySql

## 目录树
```
.
├── code           源代码
│   ├── buffer
│   ├── config
│   ├── http
│   ├── log
│   ├── timer
│   ├── pool
│   ├── server
│   └── main.cpp
├── test           单元测试
│   ├── Makefile
│   └── test.cpp
├── resources      静态资源
│   ├── index.html
│   ├── image
│   ├── video
│   ├── js
│   └── css
├── bin            可执行文件
│   └── server
├── log            日志文件
├── webbench-1.5   压力测试
├── build          
│   └── Makefile
├── Makefile
├── LICENSE
└── readme.md
```


## 项目启动
需要先配置好对应的数据库
```bash
// 建立yourdb库
create database yourdb;

// 创建user表
USE yourdb;
CREATE TABLE user(
    username char(50) NULL,
    password char(50) NULL
)ENGINE=InnoDB;

// 添加数据
INSERT INTO user(username, password) VALUES('name', 'password');
```

```bash
//根目录下执行编译
make
//执行服务器
./bin/server
```

## 压力测试
```bash
linyueq@ubuntu:~/WebServer-master$ ./webbench-1.5/webbench -c 8000 -t 10 http://192.168.77.129:1316/index.html
Webbench - Simple Web Benchmark 1.5
Copyright (c) Radim Kolar 1997-2004, GPL Open Source Software.

Benchmarking: GET http://192.168.77.129:1316/index.html
8000 clients, running 10 sec.

Speed=160818 pages/min, 8663507 bytes/sec.
Requests: 26803 susceed, 0 failed.

8000 clients, running 10 sec.

Speed=160818 pages/min, 8663507 bytes/sec.
Requests: 26803 susceed, 0 failed.
```

* 测试环境: VMware Ubuntu:18.04 cpu:i7-8550U 内存:4G 
* 结果：Client 8000 | QPS 2.6K+
