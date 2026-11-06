/*
HTTP/1.1 200 OK\r\n                  <-- 状态行 (协议版本 状态码 状态描述)
Content-Type: text/plain\r\n         <-- 响应头 (告诉浏览器这是什么类型的数据)
Content-Length: 11\r\n               <-- 响应头 (告诉浏览器正文有多长)
\r\n                                 <-- 极其重要的空行！代表头部结束
Hello World                          <-- 响应体 (Body，真正要在浏览器页面上显示的字)
*/
#include <iostream>
#include <string>
#include <sys/socket.h>
#include <netinet/in.h>
#include <unistd.h>
#include <string.h>

int main()
{
    int server_fd=socket(AF_INET,SOCK_STREAM,0);
    int opt=1;
    setsockopt(server_fd,SOL_SOCKET,SO_REUSEADDR,&opt,sizeof(opt));

    struct sockaddr_in server_addr={};
    server_addr.sin_family=AF_INET;
    server_addr.sin_addr.s_addr=INADDR_ANY;
    server_addr.sin_port=htons(8080);

    bind(server_fd,(struct sockaddr*) &server_addr,sizeof(server_addr));
    listen(server_fd,128);

    std::cout << "=== HTTP 响应生成 Demo 启动 ===" << std::endl;
    std::cout << "请在浏览器访问: http://<你的虚拟机IP>:8080" << std::endl;

    //如果不要求提取客户端ip和端口可以将第二个第三个参数设为nullptr
    int client_fd=accept(server_fd,nullptr,nullptr);

    if(client_fd>0)
    {
        std::cout << "浏览器已连接，正在处理请求..." << std::endl;

        //recv接收浏览器客户端的请求
        char buffer[1024];
        memset(buffer,0,sizeof(buffer));
        //先recv浏览器客户端的请求报文然后服务器才send响应报文
        recv(client_fd,buffer,sizeof(buffer)-1,0);

        //手动组装http响应报文
        //浏览器里显示的页面
        std::string response_body="Hello World";

        std::string response = 
            "HTTP/1.1 200 OK\r\n"                                // 状态行：告诉浏览器一切正常
            "Content-Type: text/plain\r\n"                       // 响应头：告诉浏览器发的是纯文本，别当 html 乱解析
            "Content-Length: " + std::to_string(response_body.length()) + "\r\n" // 响应头：严格声明 Body 的字节数
            "Connection: close\r\n"                              // 响应头：告诉浏览器发完我就挂断了
            "\r\n"                                               // 核心：空行，代表 Header 彻底结束！
            + response_body;

        std::cout << "\n>>> 即将发送给浏览器的原始报文：\n" << response << std::endl;
        //send的调用接口是const char*，要将string转换为对应的类型传入
        send(client_fd, response.c_str(), response.length(), 0);
        std::cout << "\n>>> 响应发送完毕！请查看浏览器屏幕。" << std::endl;
        close(client_fd);
    }

    close(server_fd);

    return 0;
}