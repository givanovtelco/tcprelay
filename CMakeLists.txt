cmake_minimum_required (VERSION 2.6)
project (Median)

set (Median_VERSION_MAJOR 1)
set (Median_VERSION_MINOR 0)

SET(CMAKE_CXX_COMPILER /usr/bin/c++)  
find_package (Threads)

add_executable(tcprelay main.cpp equeue.cpp maputils.cpp threadpool.cpp cfg.cpp)
target_link_libraries (tcprelay ${CMAKE_THREAD_LIBS_INIT})
add_library(srv STATIC srvlib/connpool.cpp)
add_executable(tcprelayctl cli/tcprelayctl.cpp)
add_executable(echoserver server/server.cpp)
target_include_directories(
    echoserver PUBLIC
    srvlib
    )
target_link_libraries(echoserver ${PROJECT_BINARY_DIR}/libsrv.a)
add_dependencies(echoserver srv)

