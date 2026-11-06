#pragma once
#include "EpollServer.h"
#include "Router.h"
#include "HttpRequest.h"
#include "HttpResponse.h"
#include <nlohmann/json.hpp>
//公有继承EpollServer
class HttpServer:public EpollServer
{
private:
    Router router;
protected:
    //override:防止拼写错误让编译器误以为重新写了一个函数从而不报错，该修饰符就是向编译器申明我正在重写父类函数
    void handle_client_data(int client_fd) override;
public:
    HttpServer(int port);
    ~HttpServer()=default;
    //设计模式中的代理（proxy）模式：
    //两个 add_route 接口不是同一个函数，HttpServer::add_route：这是一个公有包装器 (Wrapper)。它暴露给外面的用户
    //因为router不能暴露，里面存放路由表所以必须是private，但是用户又要使用router类的成员函数，可以这样设计
    //Router::add_route是public，HttpServer 内部的router对象是private
    void add_route(const std::string &path,HandleFunc handler);
    nlohmann::json metricsSnapshot();
};
