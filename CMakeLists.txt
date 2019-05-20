cmake_minimum_required (VERSION 2.6)
project (Median)

set (Median_VERSION_MAJOR 1)
set (Median_VERSION_MINOR 0)

SET(CMAKE_CXX_COMPILER /usr/bin/c++)  
find_package (Threads)
# add the executable
add_executable(tcprelay main.cpp equeue.cpp maputils.cpp threadpool.cpp cfg.cpp)
add_executable(tcprelayctl cli/tcprelayctl.cpp)
target_link_libraries (tcprelay ${CMAKE_THREAD_LIBS_INIT})

