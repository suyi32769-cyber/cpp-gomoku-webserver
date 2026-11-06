#pragma once
#include <string>
#include <map>
class HttpRequest
{
private:
    //请求行
    std::string method;
    std::string path;
    std::string version;
    std::string body;
    //请求头
    std::map<std::string,std::string> headers;

    void parse(const std::string &raw_request);
public:
    //构造函数和析构函数
    HttpRequest(const std::string &raw_request);
    //防御性声明:这个类里面没有需要手动 delete 的动态内存
    ~HttpRequest()=default;
    //对外暴露只读接口
    //const在前面是修饰返回值：表示这个函数的返回值类型是const无法被修改，在后面是修饰函数：表示这个函数无法修改任何变量。
    //如果一个函数只有 1~3 行，且仅仅是为了返回一个变量（Getter），或者赋一个简单的值（Setter），直接在头文件里实现：编译器编译时视为内联函数，避免调用cpu压栈出栈的开销
    std::string get_method() const {return method;}
    std::string get_path() const {return path;}
    std::string get_version() const {return version;}
    std::string get_body() const {return body;}
    //根据key获取对应的value
    std::string get_header(const std::string &key) const;
};
