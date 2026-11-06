#include <iostream>
#include <string>
#include <sstream>   //引入字符串流
// 方法一：利用 stringstream 自动解析
void parse_with_stream(const std::string &raw_line)
{
    std::cout << "--- 方法一：使用 std::stringstream 解析 ---" << std::endl;
    //初始化流对象
    //原始请求行字符串塞进了这个管道里。现在，ss 内部就存着这串文本，并且有一个隐形的“读取光标”指向最开头
    std::stringstream ss(raw_line);

    std::string method;
    std::string path;
    std::string version;
    //自动跳过前方的空格、制表符（\t）或换行符（\r, \n），并以遇到下一个空格或换行作为一次读取的终点,并赋值
    ss>>method>>path>>version;
    std::cout << "method  = " << method << std::endl;
    std::cout << "path    = " << path << std::endl;
    std::cout << "version = " << version << std::endl;
    std::cout << std::endl;
}

// 方法二：利用 find 和 substr 手动解析
//HTTP 协议官方硬性规定了请求行的格式必须用空格切分
void parse_with_find_substr(const std::string &raw_line)
{
    std::cout << "--- 方法二：使用 find 和 substr 手动解析 ---" << std::endl;
    size_t first_space=raw_line.find(' ');
    if(first_space==std::string::npos)
    {
        std::cerr << "错误：找不到第一个空格" << std::endl;
        return;
    }
    //std::string substr(起始位置, 截取长度),返回一个全新的 std::string 对象，包含截取出来的核心文本
    std::string method=raw_line.substr(0,first_space);

    //std::string find(要查找的字符，从哪个索引开始往后查找),返回目标字符在字符串中的下标索引
    size_t second_space=raw_line.find(' ',first_space+1);
    if(second_space==std::string::npos)
    {
        std::cerr << "错误：找不到第二个空格" << std::endl;
        return;
    }
    size_t path_len=second_space-first_space-1;
    std::string path=raw_line.substr(first_space+1,path_len);

    //HTTP 请求行里不存在第三个空格
    //标准的请求行一共就三块内容：[方法] [路径] [版本],\r\n代表结束符
    size_t end_pos=raw_line.find("\r\n",second_space+1);
    std::string version;
    // 如果找到了 \r\n，就只截取到 \r 之前。万一输入的测试数据没带 \r\n，就直接截取到字符串末尾
    if(end_pos!=std::string::npos)
    {
        version=raw_line.substr(second_space+1,end_pos-second_space-1);
    }
    else
    {
        version=raw_line.substr(second_space+1);
    }
    std::cout << "method  = " << method << std::endl;
    std::cout << "path    = " << path << std::endl;
    std::cout << "version = " << version << std::endl;
    std::cout << std::endl;
}

int main()
{
    std::string raw_request_line = "GET /index.html HTTP/1.1\r\n";
    std::cout << "原始待解析请求行: " << "GET /index.html HTTP/1.1\\r\\n" << "\n\n";
    parse_with_stream(raw_request_line);
    parse_with_find_substr(raw_request_line);
    return 0;
}