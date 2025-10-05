#include <catch2/catch_test_macros.hpp>
#include "pool_allocator.h"

using namespace fast_alloc;

TEST_CASE("PoolAllocator basic allocation", "[pool]")
{
    PoolAllocator pool(64, 10);

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

TEST_CASE("PoolAllocator capacity", "[pool]")
{
    PoolAllocator pool(64, 5);

    SECTION("Fill pool completely")
    {
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
}

TEST_CASE("PoolAllocator reuse", "[pool]")
{
    PoolAllocator pool(64, 3);

    void* ptr1 = pool.allocate();
    void* ptr2 = pool.allocate();

    pool.deallocate(ptr1);
    REQUIRE(pool.allocated() == 1);

    // Should reuse freed block
    void* ptr3 = pool.allocate();
    REQUIRE(ptr3 != nullptr);
    REQUIRE(pool.allocated() == 2);

    pool.deallocate(ptr2);
    pool.deallocate(ptr3);
}

TEST_CASE("PoolAllocator move semantics", "[pool]")
{
    PoolAllocator pool1(64, 10);
    void* ptr = pool1.allocate();
    REQUIRE(ptr != nullptr);
    REQUIRE(pool1.allocated() == 1);

    // Move constructor
    PoolAllocator pool2(std::move(pool1));
    REQUIRE(pool2.allocated() == 1);
    REQUIRE(pool2.capacity() == 10);

    pool2.deallocate(ptr);
    REQUIRE(pool2.allocated() == 0);
}

TEST_CASE("PoolAllocator properties", "[pool]")
{
    constexpr std::size_t block_size = 128;
    constexpr std::size_t block_count = 20;

    const PoolAllocator pool(block_size, block_count);

    REQUIRE(pool.block_size() == block_size);
    REQUIRE(pool.capacity() == block_count);
    REQUIRE(pool.allocated() == 0);
    REQUIRE_FALSE(pool.is_full());
}
