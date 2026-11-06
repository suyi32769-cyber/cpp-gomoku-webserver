#include <iostream>
#include <string>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <unistd.h>
#include <string.h>

int main()
{
    //创建监听套接字
    int server_fd=socket(AF_INET,SOCK_STREAM,0);
    if(server_fd==-1)
    {
        std::cerr<<"socket 创建失败 "<<std::endl;
        return 1;
    }

    //设置端口复用
    int opt=1;
    setsockopt(server_fd,SOL_SOCKET,SO_REUSEADDR,&opt,sizeof(opt));

    //服务器配置
    int port=8080;
    struct sockaddr_in server_addr={};
    server_addr.sin_addr.s_addr=INADDR_ANY;
    server_addr.sin_port=htons(port);
    server_addr.sin_family=AF_INET;

    if(bind(server_fd,(struct sockaddr*) &server_addr,sizeof(server_addr))==-1)
    {
        std::cerr<<"bind 失败"<<std::endl;
        return 1;
    }

    if(listen(server_fd,128)==-1)
    {
        std::cerr<<"listen 失败"<<std::endl;
        return 1;
    }

    std::cout << "=== HTTP Demo 启动 ===" << std::endl;
    std::cout << "请打开浏览器，在地址栏输入: http://<你的虚拟机IP>:" << port << std::endl;
    std::cout << "正在等待浏览器发起连接...\n" << std::endl;

    //等待浏览器连接
    struct sockaddr_in client_addr;
    socklen_t client_len=sizeof(client_addr);
    int client_fd=accept(server_fd,(struct sockaddr*) &client_addr,&client_len);

    if(client_fd==-1)
    {
        std::cerr<<"accept 失败"<<std::endl;
        return 1;
    }

    std::cout << ">>> 浏览器已连接！正在读取浏览器发来的原始 HTTP 数据... <<<\n" << std::endl;

    char buffer[4096];
    memset(buffer,0,sizeof(buffer));

    ssize_t bytes_read=recv(client_fd,buffer,sizeof(buffer)-1,0);
    if(bytes_read>0)
    {
        std::cout << "================= 浏览器发送的原始报文 =================\n";
        std::cout<<buffer;
        std::cout << "\n========================================================" << std::endl;
        std::cout<<"收到数据："<<bytes_read<<" 字节"<<std::endl;
    }
    else if(bytes_read==0)
    {
        std::cout<<"浏览器断开连接"<<std::endl;
    }
    else
    {
        std::cerr<<"recv 发生错误"<<std::endl;
    }
    std::string fake_response = "Hello Browser, I saw your raw request!";
    send(client_fd, fake_response.c_str(), fake_response.length(), 0);

    close(client_fd);
    close(server_fd);
    std::cout << "Demo 运行结束，已释放资源。" << std::endl;
    

    return 0;
}