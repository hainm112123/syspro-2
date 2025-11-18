cmake_minimum_required(VERSION 3.15)
project(cta)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

add_executable(cta src/main.cpp)

target_sources(cta PRIVATE src/utils.cpp src/ConfigManager.cpp)

if(WIN32)
  find_library(ADVAPI32_LIBRARY NAMES advapi32)
  find_library(KERNEL32_LIBRARY NAMES kernel32)
  include_directories(include)
  target_link_libraries(cta PRIVATE advapi32.lib ${KERNEL32_LIBRARY} Ws2_32 tdh.lib)
  target_sources(cta PRIVATE src/ProcessMonitor.Win.cpp)
else()
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
  target_sources(cta PRIVATE src/ProcessMonitor.Linux.cpp)
  target_link_libraries(cta PRIVATE bpf elf z pthread rt)
  add_dependencies(cta bpf_obj)
  target_include_directories(cta PRIVATE /usr/include)
endif()
