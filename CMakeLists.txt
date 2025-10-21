cmake_minimum_required(VERSION 3.15)
project(MemoryPoolTest VERSION 1.0.0 LANGUAGES CXX)

# 设置 C++ 标准
set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

# 编译选项 - 添加调试信息和优化
set(CMAKE_BUILD_TYPE Release)
set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -O2 -g -Wall -Wextra -pedantic -pthread")

# 主测试程序
add_executable(MemoryPoolTest
    tests/UnitTest.cpp
    src/MemoryPool.cpp
)

# 设置头文件目录
target_include_directories(MemoryPoolTest PRIVATE include)

# 链接线程库
find_package(Threads REQUIRED)
target_link_libraries(MemoryPoolTest Threads::Threads)