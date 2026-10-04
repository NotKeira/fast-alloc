cmake_minimum_required(VERSION 3.20)
project(fast_alloc VERSION 1.2.0 LANGUAGES CXX)

if (CMAKE_SOURCE_DIR STREQUAL CMAKE_CURRENT_SOURCE_DIR)
    set(CMAKE_EXPORT_COMPILE_COMMANDS ON)
endif ()

function(fast_alloc_configure_target target)
    set_target_properties(${target} PROPERTIES CXX_EXTENSIONS OFF)

    if (MSVC)
        target_compile_options(${target} PRIVATE
                /W4 /WX
                "$<$<CONFIG:Release>:/O2>"
                "$<$<CONFIG:Release>:/GL>"
        )
        target_link_options(${target} PRIVATE "$<$<CONFIG:Release>:/LTCG>")
    else ()
        target_compile_options(${target} PRIVATE
                -Wall -Wextra -Wpedantic -Werror
                "$<$<CONFIG:Release>:-O3>"
                "$<$<CONFIG:Release>:-march=native>"
        )
    endif ()
endfunction()

option(BUILD_TESTS "Build unit tests" ON)
option(BUILD_BENCHMARKS "Build benchmarks" ON)

find_package(Threads REQUIRED)

# Main library
add_library(fast_alloc STATIC
        src/detail/aligned_memory.cpp
        src/pool_allocator.cpp
        src/stack_allocator.cpp
        src/freelist_allocator.cpp
        src/threadsafe_pool_allocator.cpp
)

target_include_directories(fast_alloc PUBLIC
        ${CMAKE_CURRENT_SOURCE_DIR}/src
)
target_compile_features(fast_alloc PUBLIC cxx_std_20)
target_link_libraries(fast_alloc PUBLIC Threads::Threads)
fast_alloc_configure_target(fast_alloc)

if (MSVC)
    target_link_options(fast_alloc INTERFACE "$<$<CONFIG:Release>:/LTCG>")
endif ()

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

    # Match the tests' language standard so Catch2's optional formatters agree.
    # Dependency providers can supply an already-built imported package instead.
    if (TARGET Catch2)
        target_compile_features(Catch2 PUBLIC cxx_std_20)
    endif ()

    add_executable(alloc_tests
            tests/test_main.cpp
            tests/test_pool.cpp
            tests/test_stack.cpp
            tests/test_freelist.cpp
            tests/test_threadsafe_pool.cpp

    )

    target_link_libraries(alloc_tests PRIVATE
            fast_alloc
            Catch2::Catch2WithMain
    )
    fast_alloc_configure_target(alloc_tests)

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
            benchmarks/bench_threadsafe_pool.cpp
    )

    target_link_libraries(alloc_benchmarks PRIVATE
            fast_alloc
            benchmark::benchmark
    )
    fast_alloc_configure_target(alloc_benchmarks)
endif ()
