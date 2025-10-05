cmake_minimum_required(VERSION 3.20)
project(fast_alloc VERSION 1.1.0 LANGUAGES CXX)

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

# Main library
add_library(fast_alloc STATIC
        src/pool_allocator.cpp
        src/stack_allocator.cpp
        src/freelist_allocator.cpp
)

target_include_directories(fast_alloc PUBLIC
        ${CMAKE_SOURCE_DIR}/src
)

# Tests
if (BUILD_TESTS)
    enable_testing()

    # Fetch Catch2
    include(FetchContent)
    FetchContent_Declare(
            Catch2
            GIT_REPOSITORY https://github.com/catchorg/Catch2.git
            GIT_TAG v3.5.0
    )
    FetchContent_MakeAvailable(Catch2)

    add_executable(alloc_tests
            tests/test_main.cpp
            tests/test_pool.cpp
            tests/test_stack.cpp
            tests/test_freelist.cpp

    )

    target_link_libraries(alloc_tests PRIVATE
            fast_alloc
            Catch2::Catch2WithMain
    )

    include(CTest)
    include(Catch)
    catch_discover_tests(alloc_tests)
endif ()

# Benchmarks
if (BUILD_BENCHMARKS)
    # Fetch Google Benchmark
    include(FetchContent)
    FetchContent_Declare(
            benchmark
            GIT_REPOSITORY https://github.com/google/benchmark.git
            GIT_TAG v1.9.1
    )
    set(BENCHMARK_ENABLE_TESTING OFF CACHE BOOL "" FORCE)
    set(BENCHMARK_ENABLE_INSTALL OFF CACHE BOOL "" FORCE)
    FetchContent_MakeAvailable(benchmark)

    add_executable(alloc_benchmarks
            benchmarks/benchmark_main.cpp
            benchmarks/bench_pool.cpp
            benchmarks/bench_stack.cpp
            benchmarks/bench_freelist.cpp
    )

    target_link_libraries(alloc_benchmarks PRIVATE
            fast_alloc
            benchmark::benchmark
    )
endif ()