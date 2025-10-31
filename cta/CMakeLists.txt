cmake_minimum_required(VERSION 3.15)
project(cta)

include_directories(include)

add_executable(cta src/main.cpp)

if(WIN32)
    target_sources(cta PRIVATE src/ProcessMonitor.Linux.cpp)
else()
    target_sources(cta PRIVATE src/ProcessMonitor.Linux.cpp)
endif()
