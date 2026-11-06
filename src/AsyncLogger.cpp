#include <iostream>
#include <chrono>
#include <ctime>
#include <sstream>

#include "AsyncLogger.h"
//私有函数实现
std::string AsyncLogger::get_current_time()
{
    //1.获取当前时间点，C++内部形式，人看不懂
    auto now=std::chrono::system_clock::now();
    //2.将时间点转换为1970-01-01 00:00:00 到当前时间经过了多少秒
    std::time_t in_time_t=std::chrono::system_clock::to_time_t(now);
    //3.将总秒数转换为所在时区的时间(使用localtime_r更安全)
    std::tm local_tm;
    localtime_r(&in_time_t,&local_tm);

    char buffer[128];
    std::strftime(buffer,sizeof(buffer),"%Y-%m-%d %H:%M:%S",&local_tm);

    return std::string(buffer);
}

std::string AsyncLogger::level_to_string(LogLevel level)
{
    if(level==LogLevel::INFO)
    {
        return "INFO";
    }
    if(level==LogLevel::ERROR)
    {
        return "ERROR";
    }
    return "UNKNOWN";
}

void AsyncLogger::append_log(LogLevel level,const std::string &message)
{
    //字符串拼接,将所有数据类型拼接成长字符串
    std::stringstream ss;
    //日志输出格式：[时间] [级别] [线程ID] 消息
    ss<<"["<<get_current_time()<<"] "<<"["<<level_to_string(level)<<"] "<<"[thread "<<std::this_thread::get_id()<<"] "<<message;
    if(message.compare(0,7,"[HTTP] ")==0 || message=="[监控] 请求读取失败、超时或连接已关闭" || message=="[监控] 请求处理异常" || message=="[监控] 响应发送失败") {
        std::lock_guard<std::mutex> guard(recentMutex);
        recentHttp.push_back(ss.str());
        if(recentHttp.size()>30) recentHttp.pop_front();
    }
    //将日志放入队列
    logQueue.push(ss.str());
}

std::vector<std::string> AsyncLogger::recentRequests() {
    std::lock_guard<std::mutex> guard(recentMutex);
    return std::vector<std::string>(recentHttp.rbegin(),recentHttp.rend());
}

//公共接口实现
AsyncLogger::AsyncLogger(const std::string &file_name)
{
    //打开日志并以追加的方式写入
    outputFile.open(file_name,std::ios::app);

    if(!outputFile.is_open())
    {
        //正常输出：cout，错误输出：cerr  符合c++语义
        std::cerr<<"错误：无法打开日志文件"<<file_name<<std::endl;
        std::cerr<<"请检查路径是否存在"<<std::endl;
    }

    //this表示捕获当前对象的成员变量和成员函数
    //移动赋值
    logThread=std::thread([this]()
{
    std::string log_msg;
    //只要没调用stop就一直从日志队列取任务
    while(logQueue.pop(log_msg))
    {
        //判断文件是否打开成功，只有打开成功才能写日志
        if(outputFile.is_open())
        {
            //写日志文件
            //std::endl 每次都会 flush，频繁 flush 会影响性能。（每秒几千条）
            outputFile<<log_msg<<"\n";
            //内容不会直接写到磁盘，而是先写到缓冲区
            //让文件流把缓冲区里的内容尽快写出去
            //outputFile.flush();
            //即保证低并发情况下不会发生缓冲区不写入硬盘中的日志文件，又保证了高并发情况下不会频繁写入硬盘拖累QPS：
            if(logQueue.empty())
            {
                outputFile.flush();
            }
        }
    }
});
}

void AsyncLogger::info(const std::string &message)
{
    append_log(LogLevel::INFO,message);
}
void AsyncLogger::error(const std::string &message)
{
    append_log(LogLevel::ERROR,message);
}

AsyncLogger::~AsyncLogger()
{
    logQueue.stop();
    if(logThread.joinable())
    {
        logThread.join();
    }
    if(outputFile.is_open())
    {
        outputFile.close();
    }
}
