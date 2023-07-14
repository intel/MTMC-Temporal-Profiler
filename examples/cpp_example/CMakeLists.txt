cmake_minimum_required(VERSION 3.12)
project(cpp_example)

add_executable(cpp_example cpp_example.cpp)
target_include_directories(cpp_example PUBLIC /usr/local/include)
target_link_directories(cpp_example PUBLIC /usr/local/lib64)
target_link_libraries(cpp_example PUBLIC pfc -lpthread)