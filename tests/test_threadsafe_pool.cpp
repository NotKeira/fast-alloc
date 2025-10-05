#include <catch2/catch_test_macros.hpp>
#include "threadsafe_pool_allocator.h"
#include <thread>
#include <vector>

using namespace fast_alloc;

TEST_CASE("ThreadSafePoolAllocator basic allocation", "[threadsafe_pool]")
{
    ThreadSafePoolAllocator pool(64, 10);

    SECTION("Single allocation")
    {
        void* ptr = pool.allocate();
        REQUIRE(ptr != nullptr);
        REQUIRE(pool.allocated() == 1);

        pool.deallocate(ptr);
        REQUIRE(pool.allocated() == 0);
    }

    SECTION("Multiple allocations")
    {
        void* ptr1 = pool.allocate();
        void* ptr2 = pool.allocate();
        void* ptr3 = pool.allocate();

        REQUIRE(ptr1 != nullptr);
        REQUIRE(ptr2 != nullptr);
        REQUIRE(ptr3 != nullptr);
        REQUIRE(ptr1 != ptr2);
        REQUIRE(ptr2 != ptr3);
        REQUIRE(pool.allocated() == 3);

        pool.deallocate(ptr1);
        pool.deallocate(ptr2);
        pool.deallocate(ptr3);
        REQUIRE(pool.allocated() == 0);
    }
}

TEST_CASE("ThreadSafePoolAllocator capacity", "[threadsafe_pool]")
{
    ThreadSafePoolAllocator pool(64, 5);

    void* ptrs[5];
    for (int i = 0; i < 5; ++i)
    {
        ptrs[i] = pool.allocate();
        REQUIRE(ptrs[i] != nullptr);
    }

    REQUIRE(pool.is_full());
    REQUIRE(pool.allocated() == 5);

    // Next allocation should fail
    void* ptr = pool.allocate();
    REQUIRE(ptr == nullptr);

    // Clean up
    for (int i = 0; i < 5; ++i)
    {
        pool.deallocate(ptrs[i]);
    }
}

TEST_CASE("ThreadSafePoolAllocator concurrent allocations", "[threadsafe_pool]")
{
    constexpr std::size_t num_threads = 4;
    constexpr std::size_t allocs_per_thread = 100;
    constexpr std::size_t total_blocks = num_threads * allocs_per_thread;

    ThreadSafePoolAllocator pool(64, total_blocks);

    std::vector<std::thread> threads;
    std::vector<std::vector<void*>> thread_ptrs(num_threads);

    // Allocate from multiple threads
    for (std::size_t i = 0; i < num_threads; ++i)
    {
        threads.emplace_back([&pool, &thread_ptrs, i]()
        {
            for (std::size_t j = 0; j < allocs_per_thread; ++j)
            {
                void* ptr = pool.allocate();
                REQUIRE(ptr != nullptr);
                thread_ptrs[i].push_back(ptr);
            }
        });
    }

    for (auto& thread : threads)
    {
        thread.join();
    }

    REQUIRE(pool.allocated() == total_blocks);
    REQUIRE(pool.is_full());

    // Deallocate from multiple threads
    threads.clear();
    for (std::size_t i = 0; i < num_threads; ++i)
    {
        threads.emplace_back([&pool, &thread_ptrs, i]()
        {
            for (void* ptr : thread_ptrs[i])
            {
                pool.deallocate(ptr);
            }
        });
    }

    for (auto& thread : threads)
    {
        thread.join();
    }

    REQUIRE(pool.allocated() == 0);
}

TEST_CASE("ThreadSafePoolAllocator concurrent alloc/dealloc", "[threadsafe_pool]")
{
    constexpr std::size_t num_threads = 4;
    constexpr std::size_t operations = 1000;

    ThreadSafePoolAllocator pool(64, 100);

    std::vector<std::thread> threads;

    for (std::size_t i = 0; i < num_threads; ++i)
    {
        threads.emplace_back([&pool]()
        {
            for (std::size_t j = 0; j < operations; ++j)
            {
                if (void* ptr = pool.allocate())
                {
                    // Do some work with the pointer
                    *static_cast<int*>(ptr) = 42;
                    pool.deallocate(ptr);
                }
            }
        });
    }

    for (auto& thread : threads)
    {
        thread.join();
    }

    // All allocations should be freed
    REQUIRE(pool.allocated() == 0);
}

TEST_CASE("ThreadSafePoolAllocator stress test", "[threadsafe_pool]")
{
    constexpr std::size_t num_threads = 8;
    constexpr std::size_t operations = 10000;

    ThreadSafePoolAllocator pool(128, 1000);

    std::vector<std::thread> threads;
    std::atomic<std::size_t> total_allocations{0};
    std::atomic<std::size_t> failed_allocations{0};

    for (std::size_t i = 0; i < num_threads; ++i)
    {
        threads.emplace_back([&pool, &total_allocations, &failed_allocations]()
        {
            std::vector<void*> local_ptrs;
            local_ptrs.reserve(operations / 10);

            for (std::size_t j = 0; j < operations; ++j)
            {
                if (j % 3 == 0 && !local_ptrs.empty())
                {
                    // Deallocate
                    pool.deallocate(local_ptrs.back());
                    local_ptrs.pop_back();
                }
                else
                {
                    // Allocate
                    if (void* ptr = pool.allocate())
                    {
                        local_ptrs.push_back(ptr);
                        total_allocations.fetch_add(1, std::memory_order_relaxed);
                    }
                    else
                    {
                        failed_allocations.fetch_add(1, std::memory_order_relaxed);
                    }
                }
            }

            // Clean up remaining allocations
            for (void* ptr : local_ptrs)
            {
                pool.deallocate(ptr);
            }
        });
    }

    for (auto& thread : threads)
    {
        thread.join();
    }

    REQUIRE(pool.allocated() == 0);
    REQUIRE(total_allocations > 0);
}
