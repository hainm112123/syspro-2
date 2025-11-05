cmake_minimum_required(VERSION 3.15)
project(cta)

include_directories(include /usr/src/linux-headers-${KERNEL_RELEASE}/include)

find_program(CLANG clang)
if (NOT CLANG)
    message(FATAL_ERROR "clang is required to compile BPF object. Install clang.")
endif()
set(BPF_SRC ${CMAKE_SOURCE_DIR}/src/monitor.bpf.c)
set(BPF_OBJ ${CMAKE_BINARY_DIR}/monitor.bpf.o)
add_custom_command(
    OUTPUT ${BPF_OBJ}
    COMMAND ${CLANG} -O2 -g -target bpf -D __TARGET_ARCH_x86 -I ${CMAKE_SOURCE_DIR}/include -c ${BPF_SRC} -o ${BPF_OBJ}
    DEPENDS ${BPF_SRC}
    COMMENT "Compiling BPF object ${BPF_OBJ}"
)
add_custom_target(bpf_obj ALL DEPENDS ${BPF_OBJ})

find_library(LIBBPF_LIB bpf)
if (NOT LIBBPF_LIB)
    message(FATAL_ERROR "libbpf not found. Install libbpf-dev or libbpf.")
endif()

execute_process(
  COMMAND uname -r
  OUTPUT_VARIABLE KERNEL_RELEASE
  OUTPUT_STRIP_TRAILING_WHITESPACE
)

add_executable(cta src/main.cpp)

if(WIN32)
    target_sources(cta PRIVATE src/ProcessMonitor.Linux.cpp)
else()
    target_sources(cta PRIVATE src/ProcessMonitor.Linux.cpp)
endif()

target_link_libraries(cta PRIVATE bpf elf z)
add_dependencies(cta bpf_obj)
target_include_directories(cta PRIVATE /usr/include)
