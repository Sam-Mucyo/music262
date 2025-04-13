cmake_minimum_required(VERSION 3.15)
project(music262)

# Core Audio requires macOS
if(NOT APPLE)
    message(FATAL_ERROR "This project requires macOS for Core Audio")
endif()

add_subdirectory(client)
add_subdirectory(server)