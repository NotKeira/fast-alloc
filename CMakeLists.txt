cmake_minimum_required(VERSION 4.0)
project(fast_alloc VERSION 1.0.0 LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

set(CMAKE_EXPORT_COMPILE_COMMANDS ON)

if (MSVC)
    add_compile_options(/W4 /WX)
else ()
    add_compile_options(-Wall -Wextra -Wpedantic -Werror)
endif ()

if (CMAKE_BUILD_TYPE STREQUAL "Release")
    if (MSVC)
        add_compile_options(/O2 /GL)
        add_link_options(/LTCG)
    else ()
        add_compile_options(-O3 -march=native)
    endif ()
endif ()


option(BUILD_TESTS "Build unit tests" ON)
option(BUILD_BENCHMARKS "Build benchmarks" ON)

add_library(fast_alloc STATIC
        src/pool_allocator.cpp
        src/stack_allocator.cpp
        src/freelist_allocator.cpp
)

target_include_directories(fast_alloc PUBLIC
        ${CMAKE_SOURCE_DIR}/src
)

if (BUILD_TESTS)
    enable_testing()
    add_subdirectory(tests)
endif ()

# Benchmarks
if (BUILD_BENCHMARKS)
    add_subdirectory(benchmarks)
endif ()