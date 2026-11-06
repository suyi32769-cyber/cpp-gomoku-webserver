#include <iostream>
#include <string>
#include <fstream>
#include <sstream>
//该demo仅仅是为了测试所以在终端显示
//根据文件名后缀推断MIME Type
std::string get_mime_type(const std::string &filepath)
{
    //查找最后一个点的位置，用于提取后缀名
    size_t dot_pos=filepath.find_last_of('.');
    //没有后缀名当作纯文本
    if(dot_pos==std::string::npos)
    {
        return "text/plaint";
    }
    //提取后缀
    std::string ext=filepath.substr(dot_pos);
    //"text/html"等---固定的Content-Type类型
    if(ext==".html"||ext==".htm") return "text/html";
    if(ext==".css") return "text/css";
    if(ext==".js") return "application/javascript";
    if(ext==".png") return "image/png";
    if (ext == ".jpg" || ext == ".jpeg") return "image/jpeg";
    if (ext == ".json") return "application/json";
    //未知类型为默认纯文本
    return "text/plaint";
}

int main()
{
    //如果用户只输入了/（未指明具体文件），我们要自动帮他映射到 www/index.html；如果指明则直接跳转到具体的
    //服务器不能直接拿 /index.html 去读，因为那是 Linux 系统的根目录,要自动在前面加上www
    //正式项目里，req_path这个变量的值是动态从浏览器请求报文里切出来的
    std::string req_path="/";       //假设用户只输入了域名，没有加具体文件名
    std::string target_file="www";  //静态资源文件夹
    if(req_path=="/")
    {
        target_file+="/index.html";
    }
    else
    {
        target_file+=req_path;
    }
    std::cout << ">>> 浏览器请求路径: " << req_path << std::endl;
    std::cout << ">>> 映射本地物理路径: " << target_file << "\n" << std::endl;

    //打开文件
    //创建了file对象且自动调用open，第一个参数是文件路径，第二个参数是打开模式（std::ios::binary 表示按纯二进制读取）
    std::ifstream file(target_file,std::ios::binary);
    std::string response;
    if(!file.is_open())
    {
        //文件不存在，404
        std::cout << "[ERROR] 文件未找到！准备生成 404 报文..." << std::endl;
        std::string body="<h1>404 Not Found</h1><p>The requested file does not exist.</p>";
        //方便阅读代码
        response="HTTP/1.1 404 Not Found\r\n"
                 "Content-Type: text/html\r\n"
                 //to_string:把数字变成文本并拼接
                 "Content-Length: " + std::to_string(body.length()) + "\r\n"
                 "Connection: close\r\n"
                 "\r\n" + body;
    }
    else
    {
        std::cout << "[SUCCESS] 文件打开成功！正在读取内容..." << std::endl;
        //如果没有这句：
        /*
        一行一行地读
        while (std::getline(file, temp_line))
        {
            file_content += temp_line + "\n"; // 读完一行拼上去，并手动补上换行符
        }
        1.内存频繁地重新分配和拷贝
        2.image.png没有换行符，getline 会强行去里面找 \n
        
        std::string的其中一个构造函数：std::string(起始位置的指针/迭代器, 结束位置的指针/迭代器);
        std::istreambuf_iterator<char>(file)：文件在硬盘上的物理起点
        std::istreambuf_iterator<char>()：文件在硬盘上的物理终点
        建个字符串。内容从文件底层缓冲区的起点开始，一直到文件的尽头。不要管里面是文字、空格还是换行符，原封不动地按字节全拷进去
        */
        
        std::string file_content((std::istreambuf_iterator<char>(file)),std::istreambuf_iterator<char>());
        file.close();

        // 获取对应的 MIME Type
        std::string mime_type=get_mime_type(target_file);
        response = "HTTP/1.1 200 OK\r\n"
                   "Content-Type: " + mime_type + "\r\n"
                   "Content-Length: " + std::to_string(file_content.length()) + "\r\n"
                   "Connection: close\r\n"
                   "\r\n" + file_content;
    }
    std::cout << "\n================= 最终生成的 HTTP 响应报文 =================\n";
    std::cout << response << std::endl;
    std::cout << "==========================================================\n";

    return 0;
}