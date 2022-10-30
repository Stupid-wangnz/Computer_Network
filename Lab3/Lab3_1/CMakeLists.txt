cmake_minimum_required(VERSION 3.22)
project(Lab3_1)
set(CMAKE_EXE_LINKER_FLAGS "-static")
set(CMAKE_CXX_STANDARD 14)
link_libraries(ws2_32 wsock32)

add_executable(server server.cpp)
add_executable(client client.cpp)
