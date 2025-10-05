add_library(fast_alloc STATIC
        pool_allocator.cpp
)

target_include_directories(fast_alloc PUBLIC
        ${CMAKE_CURRENT_SOURCE_DIR}
)