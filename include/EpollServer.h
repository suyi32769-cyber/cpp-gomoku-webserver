#pragma once
#include <sys/epoll.h>

#include "ThreadPool.h"
#include "AsyncLogger.h"

class EpollServer
{
private:
    int port;   //服务器端口
    int server_fd;  //监听套接字
    int epoll_fd;   //epoll实例句柄
    //static (静态)：意味着这个变量属于这个类本身，而不属于某个具体的对象实例。
    //节省内存空间
    static const int MAX_EVENTS=1024;  //epoll_wait 一次最多取回的事件数
    struct epoll_event events[MAX_EVENTS];   //事件接收数组

    //定义初始化接口
    void init_server();
    void init_epoll();

    //定义设置非阻塞接口
    void set_nonblocking(int fd);

    //定义业务处理接口
    void accept_connection();
    
    
protected:
    //virtual:虚函数，主要是为了晚绑定
    /*
    在子类中重写了handle_client_data这个函数，所以我要使用虚函数来修饰，虚函数具有晚绑定的特性，如果不晚绑定，子类重写是无效的,
    调用的时候还是会跳转到父类的没有重新的handle_client_data，而晚绑定就是专门针对子类重写父类的成员函数，简而言之就是具体情况具体分析
    protected：public与private的中和，外部无法访问但是子类可以继承
    */
    virtual void handle_client_data(int client_fd);
    ThreadPool thread_pool;
    AsyncLogger logger;
    //新增
    void reset_oneshot(int fd);
public:
    //构造与析构
    EpollServer(int port);
    ~EpollServer();

    //拷贝禁用
    EpollServer(const EpollServer&)=delete;
    EpollServer& operator=(const EpollServer&)=delete;

    //对外暴露启动接口
    void start();

};