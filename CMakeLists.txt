cmake_minimum_required(VERSION 3.8.1)
project(tcpclientserver)

find_package (Threads)

set(CMAKE_CXX_STANDARD 11)
set(CMAKE_CXX_FLAGS ${CMAKE_CXX_FLAGS} "-std=c++14")

add_library(${PROJECT_NAME} SHARED
        src/TcpClient.cpp
        src/TcpServer.cpp
        src/Client.cpp
        src/PipeRet.cpp
        src/common.cpp)
