# CppWebServer: 高性能 C++11 异步并发 Web 服务器

![C++](https://img.shields.io/badge/C++-11-blue.svg) ![Build](https://img.shields.io/badge/build-CMake-brightgreen.svg) ![Platform](https://img.shields.io/badge/platform-Linux-lightgrey.svg) ![Protocol](https://img.shields.io/badge/protocol-HTTP%2F1.1-yellow.svg)

## 1. 项目简介

本项目是一个基于现代 C++ (C++11) 和 Linux epoll I/O 多路复用技术开发的轻量级、高并发 TCP 服务端程序。项目从基础的阻塞网络模型深度演进，自主实现了单 Reactor 事件驱动模型、并发工作线程池 (ThreadPool) 与无阻塞异步日志系统 (AsyncLogger)。

通过引入经典的生产者-消费者模型与事件循环，本项目成功实现了网络 I/O 接入、业务逻辑处理与磁盘 I/O 的彻底物理级解耦。有效解决了极端并发场景下的阻塞假死、线程频繁创建开销以及磁盘 I/O 拖慢主干网络的问题，具备工业级高性能 WebServer 的底层核心骨架。在此基础上，本项目进一步完成了完整的 HTTP/1.1 应用层协议栈支持，并配备了具有高级拟态质感的前端展示主页（Engineered by XueRui Chan）。

---

## 2. 核心特性与技术亮点

* **事件驱动的 Reactor 核心 (Event Loop)**：基于 Linux `epoll` 机制与非阻塞 Socket 构建，单线程即可轻松管理成千上万的并发连接，彻底消除 `accept` 与 `recv` 在无数据时的阻塞假死问题。
* **高性能并发调度 (Thread Pool)**：预先分配并托管固定数量的工作线程，彻底消除高并发下的线程频繁创建/销毁开销。基于 `std::mutex` 和 `std::condition_variable` 实现线程安全的任务获取与条件阻塞，榨干 CPU 算力。
* **极低延迟的异步日志 (Async Logging)**：业务线程仅负责日志对象的内存拼接并推入内存阻塞队列，由独立的后台专职线程负责缓慢的磁盘落盘 (flush) 操作，保障核心网络收发主干道畅通无阻，实现极低延迟。
* **优雅的异常与生命周期管理**：采用面向对象 (OOP) 思想深度重构，采用 RAII 机制管理锁资源 (`std::unique_lock`) 与文件流，确保安全析构防内存/句柄泄漏。
* **深拷贝防数据污染**：利用 C++ `std::string` 深拷贝机制与 Lambda 表达式闭包，精准斩断底层网络缓存区与业务逻辑生命周期的耦合，确保工作线程跨线程安全获取纯净报文。
* **动态路由与代理机制 (Dynamic Router)**：引入面向对象的代理模式，支持基于 Lambda 闭包的 RESTful API 动态注册（如 `/api/ping`），实现后端业务逻辑的极简挂载。
* **静态资源分发 (StaticFileServer)**：支持本地 `www/` 目录的静态 HTML/CSS/图片资源回源，并自动进行 MIME 类型推断，耗时的磁盘读取被完美下放至后台工作线程。

---

## 3. 目录结构

```text
http_web_server/
├── CMakeLists.txt         # CMake 自动化构建配置文件
├── bin/                   # 可执行文件统一输出目录（由 CMake 自动路由）
├── build/                 # 外部构建临时工作目录
├── demos/                 # 前期 API 学习与底层机制测试 Demo
├── include/               # 头文件目录（对外暴露的接口声明）
│   ├── AsyncLogger.h      # 异步日志模块声明
│   ├── BlockingQueue.h    # 线程安全阻塞队列模板实现
│   ├── EpollServer.h      # Reactor 服务器核心逻辑与架构声明
│   ├── ThreadPool.h       # 线程池模块声明
│   ├── HttpRequest.h      # HTTP 报文反序列化/解析模块声明
│   ├── HttpResponse.h     # HTTP 报文序列化/打包模块声明
│   ├── Router.h           # 动态路由控制模块声明
│   └── StaticFileServer.h # 静态磁盘文件读取模块声明
├── logs/                  # 日志文件输出目录
│   └── epoll_server.log   # 服务端异步运行日志（自动生成）
├── src/                   # 源代码目录（核心业务模块实现）
│   ├── AsyncLogger.cpp    # 异步日志后台刷盘逻辑实现
│   ├── EpollServer.cpp    # epoll 事件分发、非阻塞 I/O 与任务传递实现
│   ├── ThreadPool.cpp     # 线程池生命周期管理与消费逻辑实现
│   ├── HttpRequest.cpp    # 报文切割与请求字段提取实现
│   ├── HttpResponse.cpp   # 状态码维护与响应报文组装实现
│   ├── Router.cpp         # 动态 API 路由与静态资源调度分发实现
│   ├── StaticFileServer.cpp # 本地静态文件流式读取与 MIME 映射实现
│   └── HttpServer.cpp     # 请求深拷贝与多态业务接管实现
└── main.cpp               # 服务端主程序入口（位于根目录）
```
## 4. 编译与运行指南

本项目采用 CMake 进行自动化构建，专为 Linux 环境设计。请确保您的系统已安装 `g++` 编译器与 `cmake` 构建工具。

### 4.1 编译项目

本项目强烈建议使用外部构建 (Out-of-source build) 以保持源码目录的绝对整洁。请在项目根目录下依次执行以下命令：

```bash
# 1. 创建并进入独立的构建目录
mkdir build && cd build

# 2. 读取 CMakeLists.txt，生成底层 Makefile
cmake ..

# 3. 执行编译（支持多核并发编译，例如使用 make -j4）
make
```
### 4.2 运行服务与测试

编译顺利完成后，可执行文件 `webserver` 将被 CMake 自动投递至项目根目录的 `bin` 文件夹中。

```bash
# 1. 退回项目根目录并启动服务端
cd ..
./bin/webserver
```
输入你主机的 IP 地址（假设是 192.168.xxx.xxx）

测试静态网页： 输入 http://192.168.xxx.xxx:8080/index.html

测试动态 API： 输入 http://192.168.xxx.xxx:8080/api/ping

运行结束后，系统将在项目根目录下自动生成 `logs/epoll_server.log` 日志文件。您可以查看该文件，以验证主线程网络分发、多线程并发处理以及资源安全释放的精准时序。