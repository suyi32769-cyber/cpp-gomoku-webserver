#pragma once
#include <string>
#include "HttpRequest.h"
#include "HttpResponse.h"

class StaticFileServer
{
private:
    //静态资源的根目录，默认为www
    std::string base_dir;
    //辅助函数：根据文件后缀推断MIME Type
    std::string get_mime_type(const std::string &filepath) const;
public:
    //const std::string &directory="www"：这句代码是设置默认参数
    //不传参数： 直接写 StaticFileServer server ----把默认的 "www" 塞进 directory 里
    //传自定义参数：覆盖原来默认的"www"
    //等价于：
    /*
    // 1. 无参构造函数（自己硬编码兜底）
StaticFileServer::StaticFileServer() {
    base_dir = "www";
}
// 2. 有参构造函数（接收用户传参）
StaticFileServer::StaticFileServer(const std::string& directory) {
    base_dir = directory;
}
    */
    StaticFileServer(const std::string &directory="www");
    ~StaticFileServer()=default;
    //文件存在且读取成功返回true；反之为false
    bool serve_file(const HttpRequest &req,HttpResponse &res) const;
};