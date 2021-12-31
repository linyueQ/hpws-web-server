#ifndef LOG_H
#define LOG_H

#include <mutex>
#include <string>
#include <thread>
#include <sys/time.h>
#include <string.h>
#include <stdarg.h>           // vastart va_end
#include <assert.h>
#include <sys/stat.h>         // mkdir
#include "blockqueue.h"
#include "../buffer/buffer.h"

class Log {
public:
    void init(int level, const char* path = "./log", 
                const char* suffix =".log",
                int maxQueueCapacity = 1024);

    static Log* Instance();         //获取单例
    static void FlushLogThread();   //异步写日志到文件中的线程函数

    void write(int level, const char *format,...);  //将日志内容写入文件中
    void flush();                                   //用于通知FlushLogThread继续写日志，清空Buffer

    int GetLevel();
    void SetLevel(int level);
    bool IsOpen() { return isOpen_; }
    
private:
    Log();
    void AppendLogLevelTitle_(int level);
    virtual ~Log();
    void AsyncWrite_();

private:
    static const int LOG_PATH_LEN = 256;    //最大路径长度
    static const int LOG_NAME_LEN = 256;    //最大日志文件名长度
    static const int MAX_LINES = 50000;     //每个文件最大几行（超过50000行另开一个文件）

    const char* path_;      //路径
    const char* suffix_;    //.log后缀
    int lineCount_;         //单个文件已经写了多少行
    int toDay_;             //当前日期

    bool isOpen_;           //日志功能是否打开
 
    Buffer buff_;           //Buffer存放待写入文件的日志数据
    int level_;             //日志级别（只记录level_级别以下的日志）
    bool isAsync_;          //是否异步

    FILE* fp_;                                          //日志文件的文件描述符
    std::unique_ptr<BlockDeque<std::string>> deque_;    //消息存放
    std::unique_ptr<std::thread> writeThread_;          //将日志信息写入文件的线程
    std::mutex mtx_;                                    //互斥量，锁日志资源，避免写线程修改Buffer、fp_、deque_等数据
};

//宏定义进行日志书写，##__VA_ARGS__表示多个参数
//  需要日志功能处于开启状态，并且等级大于等于预设的level才打印该条日志
#define LOG_BASE(level, format, ...) \
    do {\
        Log* log = Log::Instance();\
        if (log->IsOpen() && log->GetLevel() >= level) {\
            log->write(level, format, ##__VA_ARGS__); \
            log->flush();\
        }\
    } while(0);

#define LOG_DEBUG(format, ...) do {LOG_BASE(0, format, ##__VA_ARGS__)} while(0);
#define LOG_INFO(format, ...) do {LOG_BASE(1, format, ##__VA_ARGS__)} while(0);
#define LOG_WARN(format, ...) do {LOG_BASE(2, format, ##__VA_ARGS__)} while(0);
#define LOG_ERROR(format, ...) do {LOG_BASE(3, format, ##__VA_ARGS__)} while(0);

#endif //LOG_H