//日志系统的作用就是：把程序运行过程中的关键信息保存下来，方便调试、排错、复盘
//异步日志的思路:业务线程不直接写文件，只负责把日志消息丢进队列。专门的日志线程负责写文件。
//ThreadPool 是用来执行“普通任务”的,AsyncLogger 是用来记录“程序状态”的
//程序->文件 o（以程序为主体） 写；文件->程序 i 读   
#pragma once
#include <iostream>
#include <fstream>
#include <string>
#include <thread>
#include <chrono>
#include <ctime>
#include <sstream>
#include <deque>
#include <mutex>
#include <vector>

#include "BlockingQueue.h"
//定义一个只能取固定几个值(INFO,ERROR)的类型(LogLevel)
//enum:枚举类型，当一个变量只能从几个固定选项里选一个时，就可以用 enum
enum class LogLevel
{
    INFO,    //正常信息
    ERROR    //错误信息
};

class AsyncLogger
{
private:
    BlockingQueue<std::string>logQueue;  //日志队列
    std::thread logThread;   //日志线程
    //定义输出流对象，用于向文件写内容
    std::ofstream outputFile;
    std::mutex recentMutex;
    std::deque<std::string> recentHttp;
    std::string get_current_time();
    std::string level_to_string(LogLevel level);
    void append_log(LogLevel level,const std::string &message);

public:
    // 禁止克隆
    AsyncLogger(const AsyncLogger&)=delete;
    AsyncLogger& operator=(const AsyncLogger&)=delete;

    AsyncLogger(const std::string &file_name);
    void info(const std::string &message);
    void error(const std::string &message);
    size_t pendingLogs() { return logQueue.size(); }
    std::vector<std::string> recentRequests();
    ~AsyncLogger();
};
