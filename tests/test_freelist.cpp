#include <catch2/catch_test_macros.hpp>
#include "freelist_allocator.h"
#include <vector>

using namespace fast_alloc;

TEST_CASE("FreeListAllocator basic allocation", "[freelist]")
{
    FreeListAllocator allocator(4096, FreeListStrategy::FirstFit);

    SECTION("Single allocation")
    {
        void* ptr = allocator.allocate(64);
        REQUIRE(ptr != nullptr);
        REQUIRE(allocator.used() > 0);
        REQUIRE(allocator.num_allocations() == 1);

        allocator.deallocate(ptr);
        REQUIRE(allocator.num_allocations() == 0);
    }

    SECTION("Multiple allocations")
    {
        void* ptr1 = allocator.allocate(64);
        void* ptr2 = allocator.allocate(128);
        void* ptr3 = allocator.allocate(256);

        REQUIRE(ptr1 != nullptr);
        REQUIRE(ptr2 != nullptr);
        REQUIRE(ptr3 != nullptr);
        REQUIRE(allocator.num_allocations() == 3);

        allocator.deallocate(ptr1);
        allocator.deallocate(ptr2);
        allocator.deallocate(ptr3);
        REQUIRE(allocator.num_allocations() == 0);
    }
}

TEST_CASE("FreeListAllocator variable sizes", "[freelist]")
{
    FreeListAllocator allocator(8192, FreeListStrategy::FirstFit);

    std::vector<void*> ptrs;
    const std::vector<std::size_t> sizes = {16, 32, 64, 128, 256, 512, 1024};

    for (const std::size_t size : sizes)
    {
        void* ptr = allocator.allocate(size);
        REQUIRE(ptr != nullptr);
        ptrs.push_back(ptr);
    }

    REQUIRE(allocator.num_allocations() == sizes.size());

    for (void* ptr : ptrs)
    {
        allocator.deallocate(ptr);
    }

    REQUIRE(allocator.num_allocations() == 0);
}

TEST_CASE("FreeListAllocator strategies", "[freelist]")
{
    SECTION("First fit")
    {
        FreeListAllocator allocator(4096, FreeListStrategy::FirstFit);

        void* ptr1 = allocator.allocate(100);
        void* ptr2 = allocator.allocate(200);
        void* ptr3 = allocator.allocate(150);

        REQUIRE(ptr1 != nullptr);
        REQUIRE(ptr2 != nullptr);
        REQUIRE(ptr3 != nullptr);

        allocator.deallocate(ptr1);
        allocator.deallocate(ptr2);
        allocator.deallocate(ptr3);
    }

    SECTION("Best fit")
    {
        FreeListAllocator allocator(4096, FreeListStrategy::BestFit);

        void* ptr1 = allocator.allocate(100);
        void* ptr2 = allocator.allocate(200);
        void* ptr3 = allocator.allocate(150);

        REQUIRE(ptr1 != nullptr);
        REQUIRE(ptr2 != nullptr);
        REQUIRE(ptr3 != nullptr);

        allocator.deallocate(ptr1);
        allocator.deallocate(ptr2);
        allocator.deallocate(ptr3);
    }
}

TEST_CASE("FreeListAllocator coalescence", "[freelist]")
{
    FreeListAllocator allocator(4096, FreeListStrategy::FirstFit);

    void* ptr1 = allocator.allocate(100);
    void* ptr2 = allocator.allocate(100);
    void* ptr3 = allocator.allocate(100);

    REQUIRE(allocator.num_allocations() == 3);

    allocator.deallocate(ptr2);
    REQUIRE(allocator.num_allocations() == 2);

    allocator.deallocate(ptr1);
    REQUIRE(allocator.num_allocations() == 1);

    allocator.deallocate(ptr3);
    REQUIRE(allocator.num_allocations() == 0);
}

TEST_CASE("FreeListAllocator alignment", "[freelist]")
{
    FreeListAllocator allocator(4096, FreeListStrategy::FirstFit);

    SECTION("16-byte alignment")
    {
        void* ptr = allocator.allocate(64, 16);
        REQUIRE(ptr != nullptr);
        REQUIRE(reinterpret_cast<std::uintptr_t>(ptr) % 16 == 0);
        allocator.deallocate(ptr);
    }

    SECTION("32-byte alignment")
    {
        void* ptr = allocator.allocate(128, 32);
        REQUIRE(ptr != nullptr);
        REQUIRE(reinterpret_cast<std::uintptr_t>(ptr) % 32 == 0);
        allocator.deallocate(ptr);
    }

    SECTION("64-byte alignment")
    {
        void* ptr = allocator.allocate(256, 64);
        REQUIRE(ptr != nullptr);
        REQUIRE(reinterpret_cast<std::uintptr_t>(ptr) % 64 == 0);
        allocator.deallocate(ptr);
    }
}

TEST_CASE("FreeListAllocator exhaustion", "[freelist]")
{
    FreeListAllocator allocator(512, FreeListStrategy::FirstFit);

    void* ptr1 = allocator.allocate(200);
    void* ptr2 = allocator.allocate(200);

    REQUIRE(ptr1 != nullptr);
    REQUIRE(ptr2 != nullptr);

    void* ptr3 = allocator.allocate(200);
    REQUIRE(ptr3 == nullptr);

    allocator.deallocate(ptr1);
    allocator.deallocate(ptr2);
}

TEST_CASE("FreeListAllocator move semantics", "[freelist]")
{
    FreeListAllocator allocator1(4096, FreeListStrategy::FirstFit);
    void* ptr = allocator1.allocate(100);
    REQUIRE(ptr != nullptr);
    REQUIRE(allocator1.num_allocations() == 1);

    FreeListAllocator allocator2(std::move(allocator1));
    REQUIRE(allocator2.num_allocations() == 1);
    REQUIRE(allocator2.capacity() == 4096);

    allocator2.deallocate(ptr);
    REQUIRE(allocator2.num_allocations() == 0);
}

TEST_CASE("FreeListAllocator fragmentation handling", "[freelist]")
{
    FreeListAllocator allocator(4096, FreeListStrategy::FirstFit);

    std::vector<void*> ptrs;

    for (int i = 0; i < 20; ++i)
    {
        void* ptr = allocator.allocate(100);
        REQUIRE(ptr != nullptr);
        ptrs.push_back(ptr);
    }

    for (std::size_t i = 1; i < ptrs.size(); i += 2)
    {
        allocator.deallocate(ptrs[i]);
        ptrs[i] = nullptr;
    }

    void* ptr = allocator.allocate(50);
    REQUIRE(ptr != nullptr);
    allocator.deallocate(ptr);

    for (void* p : ptrs)
    {
        if (p) allocator.deallocate(p);
    }
}

TEST_CASE("FreeListAllocator properties", "[freelist]")
{
    constexpr std::size_t capacity = 8192;
    const FreeListAllocator allocator(capacity, FreeListStrategy::BestFit);

    REQUIRE(allocator.capacity() == capacity);
    REQUIRE(allocator.used() == 0);
    REQUIRE(allocator.available() == capacity);
    REQUIRE(allocator.num_allocations() == 0);
}

TEST_CASE("FreeListAllocator nullptr handling", "[freelist]")
{
    FreeListAllocator allocator(4096, FreeListStrategy::FirstFit);

    allocator.deallocate(nullptr);
    REQUIRE(allocator.num_allocations() == 0);
}
