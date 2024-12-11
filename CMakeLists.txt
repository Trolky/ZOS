cmake_minimum_required(VERSION 3.29)
project(ZOS_sem)

set(CMAKE_CXX_STANDARD 20)

add_executable(ZOS_sem main.cpp
        filesystem.cpp
        filesystem.h
        structures.h
        path_utils.cpp
        path_utils.h
)
