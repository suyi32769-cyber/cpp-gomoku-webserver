# 1. 指定 CMake 最低版本要求
cmake_minimum_required(VERSION 3.10)

# 2. 定义项目名称和使用的语言
project(CppWebServer CXX)

# 3. 设置 C++ 标准为 C++11，并强制开启
set(CMAKE_CXX_STANDARD 11)
set(CMAKE_CXX_STANDARD_REQUIRED True)

# 4. 开启所有常见警告，培养写健壮代码的习惯
set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -Wall -O2 -pthread")

# 5. 指定生成的二进制文件输出目录为 bin/
set(CMAKE_RUNTIME_OUTPUT_DIRECTORY ${CMAKE_SOURCE_DIR}/bin)

# 6. 告诉编译器去哪里找头文件
include_directories(${CMAKE_SOURCE_DIR}/include)

# 7. 收集 src 目录下所有的核心业务源文件
file(GLOB SOURCES "src/*.cpp")

# ==========================================
# 8. 【核心修改点】：把根目录下的 main.cpp 加进来！
# 将 main.cpp 和 src 目录下的 ${SOURCES} 一起编译组装
# ==========================================
set(FIVECHESS_DIR ${CMAKE_SOURCE_DIR}/third_party/FiveChess/FiveChess)
include_directories(${FIVECHESS_DIR})
add_executable(webserver main.cpp ${SOURCES}
    ${FIVECHESS_DIR}/ChessEngine.cpp
    ${FIVECHESS_DIR}/ACSearcher.cpp
    ${FIVECHESS_DIR}/PossiblePositionManager.cpp)

# 9. 链接必要的系统级线程库
find_package(CURL REQUIRED)
find_package(OpenSSL REQUIRED)
find_package(nlohmann_json REQUIRED)
find_package(SQLiteCpp REQUIRED)
target_link_libraries(webserver pthread CURL::libcurl OpenSSL::Crypto nlohmann_json::nlohmann_json SQLiteCpp sqlite3)
