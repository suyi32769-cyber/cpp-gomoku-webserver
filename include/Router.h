/*
HttpRequest 负责解码，HttpResponse 负责编码；Router：按图索骥：决定到底用什么内容去填满那个空的res对象
Router：1.排查内存字典 2.排查硬盘 3.404
HttpRequest 产出数据： 产出一个装满客户需求的、只读的 req 对象。
Router 加工数据： 接收只读的 req 和一个空的 res。它不生产数据，它只负责寻找正确的代码或文件，把空 res 填满。
HttpResponse 序列化数据： 接收被填满的 res，将其打包成可以发送给底层的纯文本字符串。
*/
#pragma once
#include <string>
#include <map>
#include <functional>
#include "HttpResponse.h"
#include "HttpRequest.h"
#include "StaticFileServer.h"
//using:取名，类似于typdef
//std::function< 返回值类型 (参数1类型, 参数2类型...) >:装代码
using HandleFunc=std::function<void(const HttpRequest &req,HttpResponse &res)>;
class Router
{
private:
    //核心路由表：存储 URL 到 函数 的映射
    std::map<std::string,HandleFunc> routes;
    StaticFileServer static_server;
public:
    Router()=default;
    ~Router()=default;
    //让使用者可以往字典里添加自定义的 URL 和对应的处理函数,避免所有业务逻辑都直接写在服务器的 handle_request 核心代码里
    /*
    // 张三只需要在外面调用 API 把自己的逻辑“挂载”上去就行了
        router.add_route("/login", [](req, res){
        res.set_body("登录成功！");
        });
    */
    void add_route(const std::string &path,HandleFunc handler);
    bool hasRoute(const std::string &path) const { return routes.count(path)>0; }
    //接收请求并产生回应:接收请求req，修改响应res
    void handle_request(const HttpRequest &req,HttpResponse &res) const;
};
