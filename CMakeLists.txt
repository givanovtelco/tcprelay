cmake_minimum_required (VERSION 2.6)
project (Median)

set (Median_VERSION_MAJOR 1)
set (Median_VERSION_MINOR 0)

SET(CMAKE_CXX_COMPILER /usr/bin/c++)  

# add the executable
add_executable(relay main.cpp equeue.cpp maputils.cpp)
