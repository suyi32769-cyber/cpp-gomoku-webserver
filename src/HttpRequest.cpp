#include "HttpRequest.h"
#include <sstream>
#include <iostream>
#include <algorithm>
#include <cctype>

//构造函数：收到请求报文后交给parse函数处理
HttpRequest::HttpRequest(const std::string &raw_request)
{
    parse(raw_request);
}

void HttpRequest::parse(const std::string &raw_request)
{
    size_t split = raw_request.find("\r\n\r\n");
    if (split != std::string::npos) body = raw_request.substr(split + 4);
    if(raw_request.empty())
    {
        return;
    }
    std::stringstream ss(raw_request); //为了利用ss内部维护的读取指针
    std::string line;
    //解析请求行
    //提取第一行
    //光标扫到\n停止并将值赋给line
    if(std::getline(ss,line))
    {
        if(!line.empty()&&line.back()=='\r')
        {
            line.pop_back();
        }
        std::stringstream line_ss(line);
        //>> 自动跳过空格提取变量，只有stringstream具有
        line_ss>>method>>path>>version;
    }

    //解析请求头
    while(std::getline(ss,line))
    {
        //请求头最后一行\r\n的判断
        if(line=="\r"||line.empty())
        {
            break;  
        }
        //定位冒号，用于提取key和value
        size_t colon_pos=line.find(':');
        if(colon_pos!=std::string::npos)
        {
            //substr(起始下标, 截取长度)，第二个参数不是结束位置
            std::string key=line.substr(0,colon_pos);
            std::transform(key.begin(),key.end(),key.begin(),[](unsigned char c){return std::tolower(c);});
            std::string value=line.substr(colon_pos+1);
            //清理value前面的空格（由":"导致的）
            //erase左闭右开
            if(!value.empty()&&value.front()==' ')
            {
                value.erase(0,1);
            }
            //清理value尾部换行符\r
            if(!value.empty()&&value.back()=='\r')
            {
                value.pop_back();
            }
            //存入map
            headers[key]=value;
        }
    }
}

std::string HttpRequest::get_header(const std::string &key) const
{
    std::string normalized = key;
    std::transform(normalized.begin(),normalized.end(),normalized.begin(),[](unsigned char c){return std::tolower(c);});
    // std::map 的 count() 方法用于检查 key 是否存在，存在返回 1，不存在返回 0。注：不能使用operator
    //headers["Host"] (operator[])：如果存在，返回 value。如果不存在，在字典里新建一个名叫 "Host" 的空键值对；但是get_header 函数结尾加了 const表示不能修改
    //headers.at("Host") (at)：如果存在，返回 value。如果不存在，不会修改字典，而是直接抛出一个异常
    if(headers.count(normalized)>0)
    {
        return headers.at(normalized);
    }
    return "";
}
