cmake_minimum_required(VERSION 3.30)
project(MiniShell C)

set(CMAKE_C_STANDARD 11)

add_executable(MiniShell main.c)


target_link_libraries(MiniShell PRIVATE ${CMAKE_SOURCE_DIR}/libparser.a)
set(CMAKE_EXE_LINKER_FLAGS "${CMAKE_EXE_LINKER_FLAGS} -no-pie")