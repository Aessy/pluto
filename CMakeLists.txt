project(serial_server)

cmake_minimum_required(VERSION 3.11)

SET(SRC main.cpp)

add_executable(serial_server ${SRC})
target_compile_options(serial_server PRIVATE -std=c++17 -g)
target_link_libraries(serial_server X11 Xft nana fontconfig json11 boost_system pthread)
