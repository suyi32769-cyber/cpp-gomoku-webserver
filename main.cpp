#include "HttpServer.h"
#include "Gomoku.h"
#include <curl/curl.h>
#include <csignal>
#include <iostream>
#include <cstdlib>

int main() {
    std::signal(SIGPIPE, SIG_IGN);
    curl_global_init(CURL_GLOBAL_DEFAULT);
    // 1. 实例化 HttpServer，监听 8080 端口
    int port=8080;
    if(const char *testPort=std::getenv("GOMOKU_TEST_PORT")) { int parsed=std::atoi(testPort); if(parsed>0 && parsed<=65535) port=parsed; }
    HttpServer server(port);
    registerGomokuRoutes(server);
    server.add_route("/api/monitor", [&server](const HttpRequest& req, HttpResponse& res) {
        if(req.get_method()!="GET") { res.set_status_code(405); res.set_body("Method Not Allowed"); return; }
        res.set_header("Content-Type","application/json; charset=utf-8");
        res.set_header("Cache-Control","no-store"); res.set_body(server.metricsSnapshot().dump());
    });

    // 2. 注册动态 API (使用 Lambda 表达式)
    server.add_route("/api/ping", [](const HttpRequest& req, HttpResponse& res) {
        res.set_status_code(200);
        res.set_header("Content-Type", "application/json");
        res.set_body("{\"status\": \"success\", \"message\": \"pong!\"}");
    });

    // 3. 启动服务器 (调用的是父类 EpollServer 的 start 函数)
    // 此时主线程会死循环在 epoll_wait 中，等待连接和数据
    server.start();

    return 0;
}
