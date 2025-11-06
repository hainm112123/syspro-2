cmake_minimum_required(VERSION 3.15)
project(ctb)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

add_executable(ctb src/main.cpp)
if (WIN32) 
	include_directories(include)
	target_sources(ctb PRIVATE src/EventLogger.Win.cpp src/utils.cpp)
else()
	include_directories(include)
	target_sources(ctb PRIVATE src/EventLogger.Linux.cpp src/utils.cpp)
endif()

add_custom_command(TARGET ctb POST_BUILD
	COMMAND ${CMAKE_COMMAND} -E copy_if_different
	"${CMAKE_SOURCE_DIR}/utils/config.json"
	"$<TARGET_FILE_DIR:ctb>/config.json"
)
