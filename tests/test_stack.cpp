#include <catch2/catch_test_macros.hpp>
#include <catch2/generators/catch_generators.hpp>
#include "stack_allocator.h"
#include <algorithm>
#include <limits>
#include <new>
#include <utility>

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
        stack.allocate(100);
        stack.allocate(200);

        REQUIRE(stack.used() > 0);

        stack.reset();

        REQUIRE(stack.used() == 0);
        REQUIRE(stack.available() == 1024);
    }

    SECTION("Marker-based reset")
    {
        stack.allocate(100);
        void* marker = stack.get_marker();

        stack.allocate(200);
        stack.allocate(150);

        const std::size_t used_before = stack.used();
        REQUIRE(used_before >= 450);

        stack.reset(marker);

        const std::size_t used_after = stack.used();
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

    void* ptr3 = stack.allocate(100);
    REQUIRE(ptr3 == nullptr);
}

TEST_CASE("StackAllocator preserves arbitrary byte capacities", "[stack][backing_memory]")
{
    const auto capacity = GENERATE(std::size_t{1}, alignof(std::max_align_t) + 1, 257);
    StackAllocator stack(capacity);
    REQUIRE(stack.capacity() == capacity);
    REQUIRE(stack.available() == capacity);

    auto* ptr = static_cast<std::byte*>(stack.allocate(capacity, 1));
    REQUIRE(ptr != nullptr);
    REQUIRE(reinterpret_cast<std::uintptr_t>(ptr) % alignof(std::max_align_t) == 0);
    std::fill_n(ptr, capacity, std::byte{0x5a});
    REQUIRE(std::all_of(ptr, ptr + capacity, [](const std::byte byte)
    {
        return byte == std::byte{0x5a};
    }));
    REQUIRE(stack.used() == capacity);
    REQUIRE(stack.available() == 0);
    REQUIRE(stack.allocate(1, 1) == nullptr);

    stack.reset();
    REQUIRE(stack.available() == capacity);
    REQUIRE(stack.allocate(capacity, 1) == ptr);
    REQUIRE(stack.allocate(1, 1) == nullptr);
}

TEST_CASE("StackAllocator backing size rounding overflow", "[stack][overflow][backing_memory]")
{
    REQUIRE_THROWS_AS(StackAllocator(std::numeric_limits<std::size_t>::max()), std::bad_alloc);
}

TEST_CASE("StackAllocator move assignment transfers backing memory", "[stack][backing_memory]")
{
    StackAllocator source(257);
    auto* ptr = static_cast<std::byte*>(source.allocate(17, 1));
    REQUIRE(ptr != nullptr);
    std::fill_n(ptr, 17, std::byte{0x5a});

    StackAllocator destination(129);
    REQUIRE(destination.allocate(65, 1) != nullptr);
    destination = std::move(source);

    REQUIRE(destination.capacity() == 257);
    REQUIRE(destination.used() == 17);
    REQUIRE(source.capacity() == 0);
    REQUIRE(source.used() == 0);
    REQUIRE(std::all_of(ptr, ptr + 17, [](const std::byte byte)
    {
        return byte == std::byte{0x5a};
    }));
    REQUIRE(destination.allocate(destination.available(), 1) == ptr + 17);
    REQUIRE(destination.used() == destination.capacity());
    destination.reset();
    REQUIRE(destination.allocate(destination.capacity(), 1) == ptr);
}

TEST_CASE("StackAllocator allocation size overflow", "[stack][overflow]")
{
    constexpr std::size_t maximum = std::numeric_limits<std::size_t>::max();
    const auto size = GENERATE_COPY(maximum, maximum - 1, maximum - 7);
    const auto alignment = GENERATE(1u, 16u, 64u);
    StackAllocator stack(1024);

    auto* first = static_cast<std::byte*>(stack.allocate(1, 1));
    REQUIRE(first != nullptr);
    *first = std::byte{0x5a};
    void* marker = stack.get_marker();
    const std::size_t used = stack.used();
    const std::size_t available = stack.available();

    REQUIRE(stack.allocate(size, alignment) == nullptr);
    REQUIRE(stack.get_marker() == marker);
    REQUIRE(stack.used() == used);
    REQUIRE(stack.available() == available);
    REQUIRE(*first == std::byte{0x5a});

    REQUIRE(stack.allocate(available, 1) == marker);
    REQUIRE(stack.used() == stack.capacity());
}

TEST_CASE("StackAllocator excessive alignment", "[stack][overflow]")
{
    constexpr std::size_t alignment = std::size_t{1} << (std::numeric_limits<std::size_t>::digits - 1);
    StackAllocator stack(1024);
    REQUIRE(stack.allocate(1, 1) != nullptr);
    void* marker = stack.get_marker();
    const std::size_t used = stack.used();
    const std::size_t available = stack.available();

    REQUIRE(stack.allocate(1, alignment) == nullptr);
    REQUIRE(stack.get_marker() == marker);
    REQUIRE(stack.used() == used);
    REQUIRE(stack.available() == available);
    REQUIRE(stack.allocate(available, 1) == marker);
}

TEST_CASE("StackAllocator move semantics", "[stack]")
{
    StackAllocator stack1(1024);
    void* ptr = stack1.allocate(100);
    REQUIRE(ptr != nullptr);

    const std::size_t used = stack1.used();

    const StackAllocator stack2(std::move(stack1));
    REQUIRE(stack2.capacity() == 1024);
    REQUIRE(stack2.used() == used);
}

TEST_CASE("StackAllocator frame pattern", "[stack]")
{
    StackAllocator stack(4096);

    for (int frame = 0; frame < 10; ++frame)
    {
        void* ptr1 = stack.allocate(64);
        void* ptr2 = stack.allocate(128);
        void* ptr3 = stack.allocate(256);

        REQUIRE(ptr1 != nullptr);
        REQUIRE(ptr2 != nullptr);
        REQUIRE(ptr3 != nullptr);

        stack.reset();
        REQUIRE(stack.used() == 0);
    }
}

TEST_CASE("StackAllocator properties", "[stack]")
{
    constexpr std::size_t capacity = 2048;
    const StackAllocator stack(capacity);

    REQUIRE(stack.capacity() == capacity);
    REQUIRE(stack.used() == 0);
    REQUIRE(stack.available() == capacity);
}

TEST_CASE("StackAllocator marker validation", "[stack]")
{
    StackAllocator stack(1024);

    void* ptr1 = stack.allocate(100);
    void* marker = stack.get_marker();
    void* ptr2 = stack.allocate(200);

    REQUIRE(ptr1 != nullptr);
    REQUIRE(marker != nullptr);
    REQUIRE(ptr2 != nullptr);

    REQUIRE(marker != stack.get_marker());
}

TEST_CASE("StackAllocator zero-size allocation", "[stack]")
{
    StackAllocator stack(1024);

    const std::size_t used_before = stack.used();
    void* ptr = stack.allocate(0);

    REQUIRE(ptr != nullptr);
    REQUIRE(stack.used() == used_before);
}
