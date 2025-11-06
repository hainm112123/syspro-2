cmake_minimum_required(VERSION 3.15)
project(ctb)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

add_executable(ctb src/main.cpp)
if (WIN32) 
	include_directories(include)
	target_sources(ctb PRIVATE src/ProcessMonitor.Win.cpp)
else()
	include_directories(include)
	target_sources(ctb PRIVATE src/ProcessMonitor.Win.cpp)
endif()