cmake_minimum_required(VERSION 3.5)
find_package(rapidfuzz REQUIRED)
add_executable(kfinder main.cpp)
target_link_libraries(kfinder rapidfuzz::rapidfuzz)