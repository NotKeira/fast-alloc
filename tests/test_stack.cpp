#include <catch2/catch_test_macros.hpp>
#include "stack_allocator.h"

using namespace fast_alloc;

TEST_CASE("StackAllocator basic allocation", "[stack]")
{
    StackAllocator stack(1024);

    SECTION("Single allocation")
    {
        void* ptr = stack.allocate(64);
        REQUIRE(ptr != nullptr);
        REQUIRE(stack.used() >= 64);
        REQUIRE(stack.available() <= 1024 - 64);
    }

    SECTION("Multiple allocations")
    {
        void* ptr1 = stack.allocate(64);
        void* ptr2 = stack.allocate(128);
        void* ptr3 = stack.allocate(32);

        REQUIRE(ptr1 != nullptr);
        REQUIRE(ptr2 != nullptr);
        REQUIRE(ptr3 != nullptr);
        REQUIRE(stack.used() >= 64 + 128 + 32);
    }
}

TEST_CASE("StackAllocator reset", "[stack]")
{
    StackAllocator stack(1024);

    SECTION("Full reset")
    {
        void* ptr1 = stack.allocate(100);
        void* ptr2 = stack.allocate(200);

        REQUIRE(stack.used() > 0);

        stack.reset();

        REQUIRE(stack.used() == 0);
        REQUIRE(stack.available() == 1024);
    }

    SECTION("Marker-based reset")
    {
        void* ptr1 = stack.allocate(100);
        void* marker = stack.get_marker();

        void* ptr2 = stack.allocate(200);
        void* ptr3 = stack.allocate(150);

        std::size_t used_before = stack.used();
        REQUIRE(used_before >= 450);

        // Reset to marker
        stack.reset(marker);

        std::size_t used_after = stack.used();
        REQUIRE(used_after < used_before);
        REQUIRE(used_after >= 100);
    }
}

TEST_CASE("StackAllocator alignment", "[stack]")
{
    StackAllocator stack(1024);

    SECTION("16-byte alignment")
    {
        void* ptr = stack.allocate(64, 16);
        REQUIRE(ptr != nullptr);
        REQUIRE(reinterpret_cast<std::uintptr_t>(ptr) % 16 == 0);
    }

    SECTION("32-byte alignment")
    {
        void* ptr = stack.allocate(64, 32);
        REQUIRE(ptr != nullptr);
        REQUIRE(reinterpret_cast<std::uintptr_t>(ptr) % 32 == 0);
    }

    SECTION("64-byte alignment")
    {
        void* ptr = stack.allocate(128, 64);
        REQUIRE(ptr != nullptr);
        REQUIRE(reinterpret_cast<std::uintptr_t>(ptr) % 64 == 0);
    }
}

TEST_CASE("StackAllocator exhaustion", "[stack]")
{
    StackAllocator stack(256);

    void* ptr1 = stack.allocate(100);
    void* ptr2 = stack.allocate(100);

    REQUIRE(ptr1 != nullptr);
    REQUIRE(ptr2 != nullptr);

    // Should fail - not enough space
    void* ptr3 = stack.allocate(100);
    REQUIRE(ptr3 == nullptr);
}

TEST_CASE("StackAllocator move semantics", "[stack]")
{
    StackAllocator stack1(1024);
    void* ptr = stack1.allocate(100);
    REQUIRE(ptr != nullptr);

    const std::size_t used = stack1.used();

    // Move constructor
    const StackAllocator stack2(std::move(stack1));
    REQUIRE(stack2.capacity() == 1024);
    REQUIRE(stack2.used() == used);
}

TEST_CASE("StackAllocator frame pattern", "[stack]")
{
    StackAllocator stack(4096);

    // Simulate multiple frames
    for (int frame = 0; frame < 10; ++frame)
    {
        void* ptr1 = stack.allocate(64);
        void* ptr2 = stack.allocate(128);
        void* ptr3 = stack.allocate(256);

        REQUIRE(ptr1 != nullptr);
        REQUIRE(ptr2 != nullptr);
        REQUIRE(ptr3 != nullptr);

        // End of frame - reset
        stack.reset();
        REQUIRE(stack.used() == 0);
    }
}
