#include <catch2/catch_test_macros.hpp>
#include "threadsafe_pool_allocator.h"
#include <thread>
#include <vector>
#include <atomic>

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

    SECTION("Fill pool completely")
    {
        void* ptrs[5];
        for (auto& ptr : ptrs)
        {
            ptr = pool.allocate();
            REQUIRE(ptr != nullptr);
        }

        REQUIRE(pool.is_full());
        REQUIRE(pool.allocated() == 5);

        void* overflow_ptr = pool.allocate();
        REQUIRE(overflow_ptr == nullptr);

        for (auto& ptr : ptrs)
        {
            pool.deallocate(ptr);
        }
    }
}

TEST_CASE("ThreadSafePoolAllocator nullptr handling", "[threadsafe_pool]")
{
    ThreadSafePoolAllocator pool(64, 5);

    pool.deallocate(nullptr);
    REQUIRE(pool.allocated() == 0);
}

TEST_CASE("ThreadSafePoolAllocator alignment", "[threadsafe_pool]")
{
    ThreadSafePoolAllocator pool(64, 5);

    void* ptr = pool.allocate();
    REQUIRE(ptr != nullptr);

    const auto address = reinterpret_cast<std::uintptr_t>(ptr);
    REQUIRE(address % alignof(std::max_align_t) == 0);

    pool.deallocate(ptr);
}

TEST_CASE("ThreadSafePoolAllocator properties", "[threadsafe_pool]")
{
    constexpr std::size_t block_size = 128;
    constexpr std::size_t block_count = 20;

    const ThreadSafePoolAllocator pool(block_size, block_count);

    REQUIRE(pool.block_size() == block_size);
    REQUIRE(pool.capacity() == block_count);
    REQUIRE(pool.allocated() == 0);
    REQUIRE_FALSE(pool.is_full());
}

TEST_CASE("ThreadSafePoolAllocator concurrent allocations", "[threadsafe_pool]")
{
    constexpr std::size_t num_threads = 4;
    constexpr std::size_t allocs_per_thread = 100;
    constexpr std::size_t total_blocks = num_threads * allocs_per_thread;

    ThreadSafePoolAllocator pool(64, total_blocks);

    std::vector<std::thread> threads;
    std::vector<std::vector<void*>> thread_ptrs(num_threads);

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
    constexpr std::size_t pool_size = 100;

    ThreadSafePoolAllocator pool(64, pool_size);

    std::vector<std::thread> threads;
    std::atomic<std::size_t> successful_ops{0};

    for (std::size_t i = 0; i < num_threads; ++i)
    {
        threads.emplace_back([&pool, &successful_ops]()
        {
            for (std::size_t j = 0; j < operations; ++j)
            {
                if (void* ptr = pool.allocate())
                {
                    successful_ops.fetch_add(1, std::memory_order_relaxed);
                    pool.deallocate(ptr);
                }
            }
        });
    }

    for (auto& thread : threads)
    {
        thread.join();
    }

    REQUIRE(pool.allocated() == 0);
    REQUIRE(successful_ops > 0);
}

TEST_CASE("ThreadSafePoolAllocator stress test", "[threadsafe_pool]")
{
    constexpr std::size_t num_threads = 8;
    constexpr std::size_t operations = 10000;
    constexpr std::size_t pool_capacity = 1000;

    ThreadSafePoolAllocator pool(128, pool_capacity);

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
                    pool.deallocate(local_ptrs.back());
                    local_ptrs.pop_back();
                }
                else
                {
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

TEST_CASE("ThreadSafePoolAllocator interleaved operations", "[threadsafe_pool]")
{
    constexpr std::size_t num_threads = 4;
    constexpr std::size_t iterations = 500;

    ThreadSafePoolAllocator pool(64, 200);

    std::vector<std::thread> threads;

    for (std::size_t i = 0; i < num_threads; ++i)
    {
        threads.emplace_back([&pool]()
        {
            for (std::size_t j = 0; j < iterations; ++j)
            {
                void* p1 = pool.allocate();
                void* p2 = pool.allocate();

                if (p1) pool.deallocate(p1);

                void* p3 = pool.allocate();

                if (p2) pool.deallocate(p2);
                if (p3) pool.deallocate(p3);
            }
        });
    }

    for (auto& thread : threads)
    {
        thread.join();
    }

    REQUIRE(pool.allocated() == 0);
}
