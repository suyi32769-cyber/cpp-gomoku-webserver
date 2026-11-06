#pragma once
#include <string>
#include <map>
class HttpResponse
{
private:
    //存储状态码，响应头，响应体
    int status_code;
    std::map<std::string,std::string> headers;
    std::string body;
    //新增：
    //用于零拷贝的文件信息：
    //初始化为 -1，表示默认没有关联物理文件
    int file_fd=-1;
    //记录文件的大小，用于生成 Content-Length 响应头
    size_t file_size=0;

    //根据状态码获取对应的状态信息描述（属于状态行）
    std::string get_status_message(int code) const;
public:
    HttpResponse();
    ~HttpResponse()=default;
    //写入接口：供外部填充数据
    void set_status_code(int code);
    void set_header(const std::string &key,const std::string &value);
    void set_body(const std::string &content);
    int get_status_code() const {return status_code;}

    //将对象里的数据封装成响应报文的格式
    std::string to_string() const;

    //新增：
    //此接口把“文件句柄”和“大小”存入对象，而不存入 body 字符串
    void set_file(int fd,size_t size);
    //获取文件句柄和大小的接口
    int get_file_fd() const {return file_fd;}
    size_t get_file_size() const {return file_size;}
};