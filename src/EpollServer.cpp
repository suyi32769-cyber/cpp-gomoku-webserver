#include <iostream>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <unistd.h>
#include <string.h>
#include <fcntl.h>

#include "EpollServer.h"
#include "RuntimeMetrics.h"

EpollServer::EpollServer(int port):port(port),server_fd(-1),epoll_fd(-1),thread_pool(8),logger("logs/epoll_server.log")
{
    (void)runtimeMetrics();
    //初始化事件接收数组
    memset(events,0,sizeof(events));
    logger.info("EpollServer 实例创建，组件初始化完成。");
}

EpollServer::~EpollServer()
{
    if(server_fd!=-1)
    {
        close(server_fd);
    }
    if(epoll_fd!=-1)
    {
        close(epoll_fd);
    }
    logger.info("EpollServer 资源已安全释放，服务器关闭。");
}

//设置socket为非阻塞
void EpollServer::set_nonblocking(int fd)
{
    int flags=fcntl(fd,F_GETFL,0);
    if(flags!=-1)
    {
        fcntl(fd,F_SETFL,flags|O_NONBLOCK);
    }
}

//初始化Server
void EpollServer::init_server()
{
    server_fd=socket(AF_INET,SOCK_STREAM,0);
    if(server_fd==-1)
    {
        logger.error("socket 创建失败: "+std::string(strerror(errno)));
        //相较于return需要把错误一路传回main函数较为冗长，exit直接终止进程
        exit(EXIT_FAILURE);
    }
    //设置端口复用，防止重启服务器的时候提示端口被占用，否则会发生之前出现过的TIME_WAIT 状态下无法连接端口。
    //setsockopt:修改套接字属性
    //int setsockopt(int sockfd, int level, int optname, const void *optval, socklen_t optlen);
    //sockfd：目标套接字  level：协议层级（IP层、TCP层、Socket应用层），SOL_SOCKET代表套接字级别
    //optname：选项名称，SO_REUSEADDR (Socket Option REUSE ADDRess 的缩写)，代表端口复用
    //*optval：1表示打开，0表示关闭，必须传入指针。通常先定义一个整型变量 int opt = 1;，然后把它的地址传进去 &opt
    //optlen：指针指向的内存块的大小
    //返回值：成功为0，失败为-1，设置errno
    //注：setsockopt 必须在 socket() 创建之后，且在 bind() 绑定之前调用！！
    int opt = 1;
    setsockopt(server_fd, SOL_SOCKET, SO_REUSEADDR, &opt, sizeof(opt));

    struct sockaddr_in server_addr={};
    // Local development deployment: do not expose this demo to the LAN.
    server_addr.sin_addr.s_addr=htonl(INADDR_LOOPBACK);
    server_addr.sin_family=AF_INET;
    server_addr.sin_port=htons(port);

    if(bind(server_fd,(struct sockaddr*) &server_addr,sizeof(server_addr))==-1)
    {
        //左边底层是字符数组指针（const char*），右边是字符指针（char*），两个指针相加是非法的
        //调用了 C++ 标准库字符串类 std::string 的构造函数。它把右边的 char* 包装成了一个真正的 C++ 字符串对象。
        //只要加号 + 的两边至少有一个是 std::string 对象，就会触发运算符重载
        logger.error("bind 失败: " + std::string(strerror(errno)));
        exit(EXIT_FAILURE);
    }

    if(listen(server_fd,SOMAXCONN)==-1)
    {
        logger.error("listen 失败: " + std::string(strerror(errno)));
        exit(EXIT_FAILURE);
    }

    set_nonblocking(server_fd);
    //std::to_string:把数字（int, float, double 等）安全地转换为 std::string 字符串对象
    logger.info("Server 初始化成功，监听端口: "+std::to_string(port));
}

//初始化epoll
void EpollServer::init_epoll()
{
    epoll_fd=epoll_create1(EPOLL_CLOEXEC);
    if(epoll_fd==-1)
    {  
        logger.error("epoll 创建失败: " + std::string(strerror(errno)));
        exit(EXIT_FAILURE);
    }

    //将server_fd注册到epoll
    struct epoll_event ev={};
    //不加修饰：LT(水平触发)：只要信号处于高电平（就绪状态），就持续不断地发送触发信号；
    //ET(边缘触发)：对应电路里的边缘跳变，必须搭配非阻塞IO且必须循环读写否则读写会丢失内容
    ev.events=EPOLLIN;
    ev.data.fd=server_fd;
    if(epoll_ctl(epoll_fd,EPOLL_CTL_ADD,server_fd,&ev)==-1)
    {
        logger.error("epoll_ctl 添加 server_fd 失败: " + std::string(strerror(errno)));
        exit(EXIT_FAILURE);
    }
    logger.info("epoll 初始化成功，已接管 server_fd。");
}

//处理新连接接口
void EpollServer::accept_connection()
{
    struct sockaddr_in client_addr;
    socklen_t client_len=sizeof(client_addr);
    int client_fd=accept(server_fd,(struct sockaddr*) &client_addr,&client_len);
    if(client_fd==-1)
    {
        if(errno!=EAGAIN&&errno!=EWOULDBLOCK)
        {
            logger.error("accept 错误: " + std::string(strerror(errno)));
        }
        return;
    }

    //新客户端设置为阻塞  
    set_nonblocking(client_fd);

    //客户端加入epoll监控
    struct epoll_event ev={};
    //EPOLLONESHOT:单次触发
    //针对 Linux 操作系统的 epoll 实例生效,只能叫醒一次 epoll_wait(主线程)
    ev.events=EPOLLIN|EPOLLONESHOT;
    ev.data.fd=client_fd;
    if(epoll_ctl(epoll_fd,EPOLL_CTL_ADD,client_fd,&ev)==-1) { close(client_fd); return; }
    ++runtimeMetrics().accepted;
    ++runtimeMetrics().active;

    //使用异步日志来记录新连接
    std::string client_ip=inet_ntoa(client_addr.sin_addr);
    int client_port=ntohs(client_addr.sin_port);
    logger.info("[新连接] 客户端 " + client_ip + ":" + std::to_string(client_port) + " 已连接, fd: " + std::to_string(client_fd));
}

//处理客户端数据收发接口
void EpollServer::handle_client_data(int client_fd)
{
    char buffer[1024];
    memset(buffer,0,sizeof(buffer));

    ssize_t bytes_read=recv(client_fd,buffer,sizeof(buffer)-1,0);
    if(bytes_read>0)
    {
        //调用string的构造函数实例化
        //主线程将buffer（栈上）的内存空间写入数据并给任务队列，随后瞬间主线程结束，栈空间被销毁，而buffer里面的内容也被销毁
        //所以必须使用深拷贝，否则工作线程无法读出正确内容，std::string msg(buffer)：堆内存
        std::string msg(buffer);
        logger.info("[主线程] 从 fd " + std::to_string(client_fd) + " 接收到数据，即将投递给线程池...");
        
        //主线程只负责将接收到的任务封装成闭包，不负责参与发送或其他耗时计算，所以不能直接在主线程调用send()
        /*ssize_t bytes_sent=send(client_fd,buffer,bytes_read,0);
        if(bytes_sent==0)
        {
            std::cerr << "send 失败: " << strerror(errno) << std::endl;
        }
    }*/
        thread_pool.submit([this,client_fd,msg]()
        {
            //以下代码在后台工作线程中执行,而不由主线程执行，主线程只负责闭包封装任务
            this->logger.info("[工作线程] 正在处理 fd " + std::to_string(client_fd) + " 的业务，数据: " + msg);
            //工作线程发送给客户端，实现echo
            ssize_t bytes_sent=send(client_fd,msg.c_str(),msg.length(),0);
            if(bytes_sent==-1)
            {
                //this指向当前对象的成员变量logger
                this->logger.error("[工作线程] send 失败: " + std::string(strerror(errno)));
            }
        });
    }
    else if(bytes_read==0)
    {
        logger.info("[断开] 客户端 fd: " + std::to_string(client_fd) + " 已主动下线。");
        close(client_fd);
    }
    else
    {
        if(errno==EAGAIN||errno==EWOULDBLOCK)
        {
            return;
        }
        else
        {
            logger.error("[异常] 客户端 fd: " + std::to_string(client_fd) + " 发生网络异常断开。");
            close(client_fd);
        }
    }
}

void EpollServer::start()
{
    init_server();
    init_epoll();
    logger.info(">>> Reactor EventLoop 正式启动，等待高并发事件触发 <<<");

    while(true)
    {
        int num_events=epoll_wait(epoll_fd,events,MAX_EVENTS,-1);
        if(num_events==-1)
        {
            if(errno==EINTR)
            {
                continue;
            }
            logger.error("epoll_wait 错误: " + std::string(strerror(errno)));
            break;
        }
        runtimeMetrics().epollEvents.fetch_add(static_cast<uint64_t>(num_events));
        for(int i=0;i<num_events;i++)
        {
            int active_fd=events[i].data.fd;
            //事件A:新客户端申请连接服务器
            if(active_fd==server_fd)
            {
                accept_connection();
            }
            //事件B：收发信息
            else
            {
                handle_client_data(active_fd);
            }
        }
    }
}

//重置单次触发
void EpollServer::reset_oneshot(int fd)
{
    struct epoll_event ev={};
    /*
    || 用来做条件判断（if (a == 1 || b == 2)）。
    | 用来给函数传多个状态组合（ev.events = EPOLLIN | EPOLLONESHOT）
    */
    ev.events=EPOLLIN|EPOLLONESHOT;
    ev.data.fd=fd;
    //使用的是 EPOLL_CTL_MOD (Modify 修改)，而不是 ADD
    //因为这个 fd 原本就在 epoll 的红黑树里，只是修改它的状态
    if(epoll_ctl(epoll_fd,EPOLL_CTL_MOD,fd,&ev)==-1)
    {
        logger.error("重置 EPOLLONESHOT 失败: " + std::string(strerror(errno)));
    }
}




