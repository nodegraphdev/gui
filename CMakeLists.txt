cmake_minimum_required(VERSION 3.13)

project(nodegraph-ui CXX)

add_subdirectory(vendor/pugixml)

set(CMAKE_CXX_STANDARD 17)
set(COMPILE_OPTIONS COMPILE_OPTIONS -g)

include_directories(include)

add_library(ngui include/ngui.h ui/ngui.cpp ui/image.cpp)
target_link_libraries(ngui SDL2 SDL2_image pugixml)

add_executable(test-ngui test/main.cpp)
target_link_libraries(test-ngui ngui)
