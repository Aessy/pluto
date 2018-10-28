project(serial_server)

cmake_minimum_required(VERSION 3.11)

SET(SRC super_metroid.cpp
        main.cpp
        service.cpp
        HttpServer.cpp
        json11.cpp)

add_executable(serial_server ${SRC})
target_compile_options(serial_server PRIVATE -std=c++17 -g)
target_link_libraries(serial_server serialport boost_system pthread)
