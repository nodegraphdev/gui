cmake_minimum_required(VERSION 3.13)

project(nodegraph-ui CXX)

set (CMAKE_CXX_STANDARD 17)

include_directories(include)

add_library(ngui include/ngui.h ui/ngui.cpp)