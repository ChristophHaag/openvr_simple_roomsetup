cmake_minimum_required(VERSION 2.6)
project(openv_simple_roomsetup)

add_executable(openv_simple_roomsetup main.cpp)

install(TARGETS openv_simple_roomsetup RUNTIME DESTINATION bin)
