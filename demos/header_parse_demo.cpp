/*
\r（回车）:回到最左侧，但不换行，如果继续输入则会覆盖
\n（换行）：从当前位置往下一行
Linux 系统： 认为没必要搞两个动作，只用一个 \n 就同时代表了“机头回起点”+“纸张滚一行”。
Windows 系统： 比较传统，坚持认为换行必须是两个动作的组合，即 先回车、再换行，所以写作 \r\n。
HTTP 协议： 作为全球网络标准，它统一规定：所有报文的行尾，必须使用 Windows 风格的 \r\n
*/
#include <iostream>
#include <string>
#include <sstream>
#include <map>

int main()
{
    //模拟从recv拿到的HTTP请求头
    //末尾的\r\n代表请求头结束
    std::string raw_headers = 
        "Host: localhost\r\n"
        "Connection: keep-alive\r\n"
        "User-Agent: Chrome\r\n"
        "\r\n";
    std::cout << "=== 待解析的原始 Headers 数据 ===" << std::endl;
    std::cout << raw_headers;
    std::cout << "=================================\n" << std::endl;
    
    //由于请求头是多个键值对，所以刚好使用map来存储
    std::map<std::string,std::string> headers;

    //将字符串装载进字符串流，并用getline扫描
    //ss是一个数据管道，维护了一个读取光标
    std::stringstream ss(raw_headers);
    //line是用来临时装每一行的数据，并在后续操作中保存到map
    std::string line;

    //std::istream& getline(std::istream& is, std::string& str, char delim = '\n');
    //is：输入流对象;str：用来接收读到内容的 std::string 变量;delim：截断字符。默认值是 '\n'
    //请求头的每一行末尾是\r\n
    //返回值： 返回输入流自身的引用。只要流里还有数据没读完，它在循环条件中转换成 bool 就是 true；读空了或出错就是 false
    //getline：扫描字符遇到\n停止，并存入line，所以line中保存的是"信息"+\r
    while(std::getline(ss,line))
    {

        if(line=="\r"||line.empty())
        {
            //line里面不可能出现\n因为我已经用getline截断了，所以要判断\r作为是否为空的依据而不是\r\n
            std::cout << "[提示] 扫描到 HTTP 头部结束标志(空行)，停止解析。" << std::endl;
            break;
        }

        //手动精细寻找键值对
        //寻找冒号
        size_t colon_pos=line.find(':');
        if(colon_pos!=std::string::npos)
        {
            //请求头是每个键值对单独一行然后每一行末尾是空格，所以提取value的逻辑可以是直接到最后，因为前面使用了getline将这多行请求头分别每一行截取到了line里面
            //提取key
            std::string key=line.substr(0,colon_pos);
            //提取value
            std::string value=line.substr(colon_pos+1);

            //修复value末尾的'\r':getline遇到\n进行截断，但是末尾是\r\n所以会保留\r，不删除的话后续做字符串对比会因为多了一个不可见的 \r 而全部匹配失败
            //.back()返回最后一个字符的引用，.end()是迭代器
            if(!value.empty()&&value.back()=='\r')
            {
                value.pop_back();
            }

            //修复冒号后面的空格：HTTP协议的习惯
            //"Host: localhost" 切完后 value 是 " localhost"
            if(!value.empty()&&value.front()==' ')
            {
                //erase(pos)	移除迭代器 pos 指向的元素
                //erase(first, last)	移除 [first, last) 区间内的元素
                value.erase(0,1);
            }
            //详情见map的插入方式
            headers[key]=value;
        }
    }
    std::cout << "\n>>> 解析完成！开始进行 map 检索测试 <<<\n" << std::endl;
    std::cout << "headers[\"Host\"]       = " << headers["Host"] << std::endl;
    std::cout << "headers[\"Connection\"] = " << headers["Connection"] << std::endl;
    std::cout << "headers[\"User-Agent\"] = " << headers["User-Agent"] << std::endl;

    //用于测试，读取一个不存在的键
    std::cout << "headers[\"Accept\"]     = " << headers["Accept"] << " (未传即为空)" << std::endl;
    return 0;
}