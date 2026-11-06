#include "HttpServer.h"
#include "RuntimeMetrics.h"
#include <sys/socket.h>
#include <sys/sendfile.h>
#include <poll.h>
#include <unistd.h>
#include <cerrno>
#include <cstdlib>
#include <algorithm>
#include <cctype>

namespace {
struct ClientGuard { int fd; ~ClientGuard() { close(fd); --runtimeMetrics().active; } };
std::string safePath(const std::string &path) {
    static const char *allowed[]={"/", "/index.html", "/home.css", "/gomoku.html", "/gomoku.css", "/gomoku.js", "/history.css", "/history.js", "/monitor.html", "/monitor.css", "/monitor.js", "/api/monitor", "/api/ping", "/api/game/state", "/api/game/new", "/api/game/move", "/api/game/ai", "/api/game/history", "/api/game/replay"};
    for(const char *entry:allowed) if(path==entry) return path;
    return "[其他路径]";
}
bool waitFd(int fd, short events) { pollfd p{fd, events, 0}; return poll(&p, 1, 5000) > 0 && (p.revents & events); }
bool sendAll(int fd, const std::string &data) {
    size_t pos=0;
    while (pos<data.size()) { ssize_t n=send(fd,data.data()+pos,data.size()-pos,MSG_NOSIGNAL); if(n>0) pos+=size_t(n); else if(n<0 && (errno==EAGAIN || errno==EWOULDBLOCK)) { if(!waitFd(fd,POLLOUT)) return false; } else if(n<0 && errno==EINTR) continue; else return false; }
    return true;
}
bool readRequest(int fd, std::string &request) {
    size_t target=std::string::npos; char buf[4096];
    while (request.size() <= 16384) {
        if (target != std::string::npos && request.size() >= target) { request.resize(target); return true; }
        if (!waitFd(fd,POLLIN)) return false;
        ssize_t n=recv(fd,buf,sizeof(buf),0); if(n<=0) { if(n<0 && (errno==EAGAIN || errno==EINTR)) continue; return false; }
        request.append(buf,size_t(n)); size_t sep=request.find("\r\n\r\n");
        if (sep!=std::string::npos && target==std::string::npos) {
            std::string headers=request.substr(0,sep); std::transform(headers.begin(),headers.end(),headers.begin(),[](unsigned char c){return std::tolower(c);});
            if(headers.find("\r\ntransfer-encoding:")!=std::string::npos) return false;
            size_t pos=headers.find("\r\ncontent-length:"); size_t length=0;
            if(pos!=std::string::npos) { pos+=17; size_t end=headers.find("\r\n",pos); std::string value=headers.substr(pos,end-pos); size_t start=value.find_first_not_of(" \t"), last=value.find_last_not_of(" \t"); if(start==std::string::npos) return false; value=value.substr(start,last-start+1); if(value.empty() || value.size()>5 || value.find_first_not_of("0123456789")!=std::string::npos) return false; length=std::strtoul(value.c_str(),nullptr,10); }
            target=sep+4+length; if(target>16384) return false;
        }
    } return false;
}
}
HttpServer::HttpServer(int port):EpollServer(port) {}
void HttpServer::add_route(const std::string &path,HandleFunc handler) { router.add_route(path,handler); }
nlohmann::json HttpServer::metricsSnapshot() {
    auto result=runtimeMetrics().snapshot();
    auto workers=thread_pool.workerStates(); size_t active=0; for(bool b:workers) if(b) ++active;
    result["workers"]=workers; result["worker_total"]=workers.size(); result["worker_active"]=active;
    result["task_queue"]=thread_pool.pendingTasks(); result["log_queue"]=logger.pendingLogs();
    result["recent_requests"]=logger.recentRequests();
    return result;
}
void HttpServer::handle_client_data(int fd) {
    bool queued=thread_pool.trySubmit([this,fd] {
        ClientGuard client{fd};
        try {
            std::string raw; if (!readRequest(fd,raw)) { ++runtimeMetrics().failed; logger.error("[监控] 请求读取失败、超时或连接已关闭"); return; }
            HttpRequest req(raw); HttpResponse res;
            bool publicMode=std::getenv("GOMOKU_PUBLIC_ORIGIN") && *std::getenv("GOMOKU_PUBLIC_ORIGIN");
            if(publicMode && (req.get_path()=="/api/monitor" || req.get_path()=="/monitor.html" || req.get_path()=="/monitor.js" || req.get_path()=="/monitor.css" || req.get_path()=="/playful-monitor.css")) { res.set_status_code(404); res.set_body("Not Found"); }
            else router.handle_request(req,res);
            res.set_header("Referrer-Policy","no-referrer"); res.set_header("X-Frame-Options","DENY"); res.set_header("Connection","close"); res.set_header("X-Content-Type-Options","nosniff");
            if(router.hasRoute(req.get_path())) { res.set_header("X-Route-Type","dynamic"); res.set_header("X-Route-Pattern",safePath(req.get_path())); }
            bool ok=sendAll(fd,res.to_string());
            if(res.get_file_fd()!=-1) {
                off_t offset=0; size_t remaining=res.get_file_size();
                while(ok && remaining>0) { ssize_t n=sendfile(fd,res.get_file_fd(),&offset,remaining); if(n>0) remaining-=size_t(n); else if(n<0 && (errno==EAGAIN || errno==EWOULDBLOCK)) ok=waitFd(fd,POLLOUT); else if(n<0 && errno==EINTR) continue; else { ok=false; break; } }
                close(res.get_file_fd());
            }
            if(req.get_path()!="/api/monitor") {
                ++runtimeMetrics().completed;
                if(res.get_status_code()>=400) { ++runtimeMetrics().failed; if(res.get_status_code()==404) ++runtimeMetrics().notFound; }
                else if(router.hasRoute(req.get_path())) ++runtimeMetrics().dynamicRoutes;
                else if(res.get_file_fd()!=-1) ++runtimeMetrics().staticFiles;
                if(!ok) { ++runtimeMetrics().failed; logger.error("[监控] 响应发送失败"); }
                std::string method=(req.get_method()=="GET" || req.get_method()=="POST")?req.get_method():"OTHER";
                std::string msg="[HTTP] " + method+" "+safePath(req.get_path())+" "+std::to_string(res.get_status_code());
                if(res.get_status_code()>=400) logger.error(msg); else logger.info(msg);
            }
        } catch(...) { ++runtimeMetrics().failed; logger.error("[监控] 请求处理异常"); }
    }, 128);
    if(!queued) { const char message[]="HTTP/1.1 503 Service Unavailable\r\nConnection: close\r\nContent-Length: 0\r\n\r\n"; send(fd,message,sizeof(message)-1,MSG_NOSIGNAL); close(fd); --runtimeMetrics().active; ++runtimeMetrics().failed; }
}
