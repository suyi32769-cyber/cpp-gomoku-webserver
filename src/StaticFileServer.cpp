//读取静态资源
//Router 的核心职责仅仅是分发： 它的任务是拿着 path 去找对应的处理逻辑
#include "StaticFileServer.h"
#include <fstream>
#include <iostream>
//新增：
#include <fcntl.h>      // 用于 open() 和 O_RDONLY
#include <sys/stat.h>   // 用于 stat() 获取文件状态
#include <unistd.h>     // 用于 close()

StaticFileServer::StaticFileServer(const std::string &directory):base_dir(directory){}

bool StaticFileServer::serve_file(const HttpRequest &req,HttpResponse &res) const
{
    std::string path=req.get_path();
    if (path.find("..") != std::string::npos || path.find('%') != std::string::npos || path.find('\\') != std::string::npos) return false;
    //默认主页
    if(path=="/")
    {
        path="/index.html";
    }
    //拼接真实物理地址
    std::string filepath=base_dir+path;

    //零拷贝修改：
    //1.操作系统在内核里建立与文件的连接，返回一个编号 src_fd
    int src_fd=open(filepath.c_str(),O_RDONLY);
    if(src_fd==-1)
    {
        return false;
    }

    //2.获取文件大小。不需要把文件读进内存去量长度，硬盘的文件系统里存了大小
    //stat() 直接去文件系统元数据里抄回这个数字，填入 stat_buf，速度极快
    struct stat stat_buf;
    if(stat(filepath.c_str(),&stat_buf)==-1)
    {
        close(src_fd);
        return false;
    }

    //3.把通道编号 (src_fd) 和 大小 交给 res 保存,整个过程 0 内存拷贝
    res.set_file(src_fd,stat_buf.st_size);

    /*
    //二进制打开文件
    std::ifstream file(filepath,std::ios::binary);
    if(!file.is_open())
    {
        return false;
    }
    //二进制模式打开文件
    //file与content的关系：
    //file：
    //文件输入流:不是文件数据本身，它是一个系统接口
    //C++ 只是向操作系统申请打开了一条连接物理硬盘中该文件的“通道”。此时，文件的字节内容依然安静地躺在磁盘（ROM）上，并没有进入程序的运行内存（RAM）
    //content：在RAM中开辟空间，通过 file 这个系统通道，向硬盘发起 I/O 读取指令，塞进 content
    //将硬盘里的文件，塞进名叫 content 的用户态字符串中
    //发生2次上下文切换和2次深拷贝（拖垮性能）
    //第一次切换上下文：用户态-内核态  第一次深拷贝：硬盘-内核缓冲区（Read Buffer）
    //第二次深拷贝：内核缓冲区-用户缓冲区（content）  结束后第二次上下文切换：内核态-用户态
    std::string content((std::istreambuf_iterator<char>(file)),std::istreambuf_iterator<char>());
    file.close();
    */

    res.set_status_code(200);
    res.set_header("Content-Type",get_mime_type(filepath));
    res.set_header("Connection",req.get_header("Connection"));
    //res.set_body(content);
    return true;
}
std::string StaticFileServer::get_mime_type(const std::string& filepath) const 
{
    size_t dot_pos = filepath.find_last_of('.');
    if (dot_pos == std::string::npos) return "text/plain";

    std::string ext = filepath.substr(dot_pos);
    if (ext == ".html" || ext == ".htm") return "text/html";
    if (ext == ".css") return "text/css";
    if (ext == ".js") return "application/javascript";
    if (ext == ".png") return "image/png";
    if (ext == ".jpg" || ext == ".jpeg") return "image/jpeg";
    if (ext == ".json") return "application/json";
    return "text/plain";
}
