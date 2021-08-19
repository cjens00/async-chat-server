cmake_minimum_required(VERSION 3.20)
project(game_server)

set(CMAKE_CXX_STANDARD 20)

find_package(asio CONFIG REQUIRED)
add_executable(game_server main.cpp net_core_from_tutorial.cpp net_core.cpp net_core.hpp)

target_link_libraries(game_server PRIVATE asio asio::asio)
