#include "Router.h"
#include <fstream>
#include <iostream>
//注册路由函数：绑定路径与具体执行的函数--各个不同模块的业务接口（如：支付，请求，登录等等）
void Router::add_route(const std::string &path,HandleFunc handler)
{
    routes[path]=handler;
}
void Router::handle_request(const HttpRequest &req,HttpResponse &res) const
{
    std::string path=req.get_path();
    //1.查找动态路由 (API):写的代码，并以键值对存储在map中的.router.add_route
    //count() 用于检查字典中是否存在该键
    //动态api：通过 add_route 注册的那个Lambda 表达式
    if(routes.count(path)>0)
    {
        //std::cout << "[Router] 命中动态路由: " << path << std::endl;
        //存在键值对则返回value,并调用执行value(一段函数)
        //等价于：HandlerFunc my_function = routes.at(path);
        //my_function(req, res);
        //routes.at(path)：取出key为path对应的value（即lambda表达式的首地址）
        //(req,res)：执行这个lambda函数
        routes.at(path)(req,res);
        return;   
    }
    //2.如果不是动态 API，则尝试读取静态文件：物理文件
    //std::cout << "[Router] 尝试读取静态文件: " << path << std::endl;
    // 阶段 2：如果不是动态 API，交给专业的 StaticFileServer 去读取静态文件
    bool success = static_server.serve_file(req, res);

    // 阶段 3：如果静态服务器也找不到文件，返回 404
    if (!success) 
    {
        res.set_status_code(404);
        res.set_header("Content-Type", "text/html");
        res.set_body("<html><body><h1>404 Not Found</h1></body></html>");
    }
    
}