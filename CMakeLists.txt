cmake_minimum_required(VERSION 3.20)
project(Async-Chat-Server)

set(CMAKE_CXX_STANDARD 20)
find_package(Boost REQUIRED json)
find_package(asio CONFIG REQUIRED)
find_package(fmt CONFIG REQUIRED)
add_executable(Async-Chat-Server main.cpp net_core.cpp net_core.hpp serial_types.hpp)

target_link_libraries(Async-Chat-Server PRIVATE asio asio::asio)
target_link_libraries(Async-Chat-Server PRIVATE fmt::fmt)
target_link_libraries(Async-Chat-Server PRIVATE Boost::boost Boost::json)
