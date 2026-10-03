#include <catch2/catch_test_macros.hpp>
#include <catch2/generators/catch_generators.hpp>
#include "pool_allocator.h"
#include <cstring>
#include <limits>
#include <new>
#include <stdexcept>
#include <utility>
#include <vector>

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

TEST_CASE("PoolAllocator backing size overflow", "[pool][overflow]")
{
    constexpr std::size_t maximum = std::numeric_limits<std::size_t>::max();

    SECTION("Block count overflow")
    {
        REQUIRE_THROWS_AS(PoolAllocator(64, maximum / 64 + 1), std::bad_alloc);
    }

    SECTION("Block size overflow")
    {
        REQUIRE_THROWS_AS(PoolAllocator(maximum / 2 + 1, 2), std::bad_alloc);
    }

    SECTION("Stride rounding overflow")
    {
        REQUIRE_THROWS_AS(PoolAllocator(maximum, 1, 64), std::bad_alloc);
    }

    SECTION("Padded size overflow when the requested size fits")
    {
        constexpr std::size_t block_size = 65;
        constexpr std::size_t block_count = maximum / block_size;
        REQUIRE_THROWS_AS(PoolAllocator(block_size, block_count, 64), std::bad_alloc);
    }
}

TEST_CASE("PoolAllocator reuse", "[pool]")
{
    PoolAllocator pool(64, 3);

    void* ptr1 = pool.allocate();
    void* ptr2 = pool.allocate();

    pool.deallocate(ptr1);
    REQUIRE(pool.allocated() == 1);

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

TEST_CASE("PoolAllocator nullptr handling", "[pool]")
{
    PoolAllocator pool(64, 5);

    pool.deallocate(nullptr);
    REQUIRE(pool.allocated() == 0);
}

TEST_CASE("PoolAllocator alignment", "[pool]")
{
    PoolAllocator pool(64, 5);

    void* ptr = pool.allocate();
    REQUIRE(ptr != nullptr);

    const auto address = reinterpret_cast<std::uintptr_t>(ptr);
    REQUIRE(address % alignof(std::max_align_t) == 0);

    pool.deallocate(ptr);
}

TEST_CASE("PoolAllocator aligns every odd-sized block", "[pool][alignment]")
{
    constexpr std::size_t block_size = sizeof(void*) + 1;
    constexpr std::size_t block_count = alignof(std::max_align_t);
    PoolAllocator pool(block_size, block_count);
    std::vector<void*> blocks;

    for (std::size_t i = 0; i < block_count; ++i)
    {
        void* ptr = pool.allocate();
        REQUIRE(ptr != nullptr);
        REQUIRE(reinterpret_cast<std::uintptr_t>(ptr) % alignof(std::max_align_t) == 0);
        std::memset(ptr, static_cast<int>(i + 1), block_size);
        blocks.push_back(ptr);
    }
    REQUIRE(pool.allocate() == nullptr);
    REQUIRE(pool.block_size() == block_size);

    for (std::size_t i = 0; i < blocks.size(); ++i)
    {
        const auto* bytes = static_cast<const unsigned char*>(blocks[i]);
        for (std::size_t j = 0; j < block_size; ++j)
        {
            REQUIRE(bytes[j] == i + 1);
        }
    }

    for (void* ptr : blocks)
    {
        pool.deallocate(ptr);
    }
    REQUIRE(pool.allocated() == 0);

    for (std::size_t i = 0; i < block_count; ++i)
    {
        void* ptr = pool.allocate();
        REQUIRE(ptr == blocks[block_count - i - 1]);
        REQUIRE(reinterpret_cast<std::uintptr_t>(ptr) % alignof(std::max_align_t) == 0);
    }
}

TEST_CASE("PoolAllocator configurable alignment", "[pool][alignment]")
{
    const auto block_size = GENERATE(sizeof(void*), sizeof(void*) + 1, 3 * alignof(std::max_align_t));
    const auto alignment = GENERATE(std::size_t{1}, 2, 4, 8, 16, 32, 64, 128);
    PoolAllocator pool(block_size, 3, alignment);

    REQUIRE(pool.block_size() == block_size);
    REQUIRE(pool.alignment() >= alignment);
    REQUIRE(pool.alignment() >= alignof(void*));
    REQUIRE(pool.block_stride() >= block_size);
    REQUIRE(pool.block_stride() % pool.alignment() == 0);
    REQUIRE(pool.block_stride() - block_size < pool.alignment());

    void* blocks[3];
    for (std::size_t i = 0; i < 3; ++i)
    {
        blocks[i] = pool.allocate();
        REQUIRE(blocks[i] != nullptr);
        REQUIRE(reinterpret_cast<std::uintptr_t>(blocks[i]) % alignment == 0);
        REQUIRE(reinterpret_cast<std::uintptr_t>(blocks[i]) % alignof(void*) == 0);
        if (i > 0)
        {
            const auto distance = static_cast<std::byte*>(blocks[i]) - static_cast<std::byte*>(blocks[i - 1]);
            REQUIRE(static_cast<std::size_t>(distance) == pool.block_stride());
        }
    }
    REQUIRE(pool.allocate() == nullptr);

    for (void* ptr : blocks)
    {
        pool.deallocate(ptr);
    }
    REQUIRE(pool.allocated() == 0);
}

TEST_CASE("PoolAllocator supports objects with extended alignment", "[pool][alignment]")
{
    struct alignas(64) Object
    {
        std::size_t value;
    };

    PoolAllocator pool(sizeof(Object), 3, alignof(Object));
    Object* objects[3];
    for (std::size_t i = 0; i < 3; ++i)
    {
        void* ptr = pool.allocate();
        REQUIRE(ptr != nullptr);
        REQUIRE(reinterpret_cast<std::uintptr_t>(ptr) % alignof(Object) == 0);
        objects[i] = new (ptr) Object{i};
    }

    for (std::size_t i = 0; i < 3; ++i)
    {
        REQUIRE(objects[i]->value == i);
        objects[i]->~Object();
        pool.deallocate(objects[i]);
    }
}

TEST_CASE("PoolAllocator rejects invalid alignment", "[pool][alignment]")
{
    const auto alignment = GENERATE(std::size_t{0}, 3, 24);
    REQUIRE_THROWS_AS(PoolAllocator(64, 3, alignment), std::invalid_argument);
}

TEST_CASE("PoolAllocator moves preserve padded block layout", "[pool][alignment]")
{
    PoolAllocator source(sizeof(void*) + 1, 3, 64);
    void* first = source.allocate();
    void* second = source.allocate();
    REQUIRE(first != nullptr);
    REQUIRE(second != nullptr);

    SECTION("Move construction")
    {
        PoolAllocator destination(std::move(source));
        REQUIRE(destination.block_size() == sizeof(void*) + 1);
        REQUIRE(destination.block_stride() == 64);
        REQUIRE(destination.alignment() == 64);
        REQUIRE(destination.allocated() == 2);
        destination.deallocate(second);
        REQUIRE(destination.allocate() == second);
        destination.deallocate(first);
        destination.deallocate(second);
        REQUIRE(destination.allocated() == 0);
    }

    SECTION("Move assignment")
    {
        PoolAllocator destination(128, 2);
        destination = std::move(source);
        REQUIRE(destination.block_size() == sizeof(void*) + 1);
        REQUIRE(destination.block_stride() == 64);
        REQUIRE(destination.alignment() == 64);
        REQUIRE(destination.capacity() == 3);
        REQUIRE(destination.allocated() == 2);
        destination.deallocate(second);
        REQUIRE(destination.allocate() == second);
        destination.deallocate(first);
        destination.deallocate(second);
        REQUIRE(destination.allocated() == 0);
    }
}

TEST_CASE("PoolAllocator interleaved operations", "[pool]")
{
    PoolAllocator pool(64, 5);

    void* p1 = pool.allocate();
    void* p2 = pool.allocate();
    pool.deallocate(p1);
    void* p3 = pool.allocate();
    pool.deallocate(p2);
    void* p4 = pool.allocate();

    REQUIRE(pool.allocated() == 2);

    pool.deallocate(p3);
    pool.deallocate(p4);
}
