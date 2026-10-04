#include <catch2/catch_test_macros.hpp>
#include <catch2/generators/catch_generators.hpp>
#include "freelist_allocator.h"
#include <algorithm>
#include <array>
#include <limits>
#include <new>
#include <random>
#include <stdexcept>
#include <utility>
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

TEST_CASE("FreeListAllocator rejects undersized capacity", "[freelist][validation]")
{
    constexpr std::size_t metadata_size = sizeof(std::size_t) + sizeof(void*);
    const auto size = GENERATE_COPY(std::size_t{0}, std::size_t{1}, metadata_size);
    const auto strategy = GENERATE(FreeListStrategy::FirstFit, FreeListStrategy::BestFit);
    REQUIRE_THROWS_AS(FreeListAllocator(size, strategy), std::invalid_argument);

    FreeListAllocator allocator(metadata_size + 1, strategy);
    void* ptr = allocator.allocate(1, 1);
    REQUIRE(ptr != nullptr);
    REQUIRE(allocator.available() == 0);
    allocator.deallocate(ptr);
    REQUIRE(allocator.available() == allocator.capacity());
}

TEST_CASE("FreeListAllocator rejects unknown strategy", "[freelist][validation]")
{
    const auto strategy = GENERATE(static_cast<FreeListStrategy>(-1), static_cast<FreeListStrategy>(2));
    REQUIRE_THROWS_AS(FreeListAllocator(1024, strategy), std::invalid_argument);
}

TEST_CASE("FreeListAllocator rejects zero-sized requests", "[freelist][validation]")
{
    const auto strategy = GENERATE(FreeListStrategy::FirstFit, FreeListStrategy::BestFit);
    FreeListAllocator allocator(1024, strategy);
    auto* first = static_cast<std::byte*>(allocator.allocate(16));
    REQUIRE(first != nullptr);
    std::fill_n(first, 16, std::byte{0x5a});
    const std::size_t used = allocator.used();
    const std::size_t available = allocator.available();
    const std::size_t allocations = allocator.num_allocations();

    REQUIRE_THROWS_AS(allocator.allocate(0), std::invalid_argument);
    REQUIRE(allocator.used() == used);
    REQUIRE(allocator.available() == available);
    REQUIRE(allocator.num_allocations() == allocations);
    REQUIRE(std::all_of(first, first + 16, [](const std::byte byte)
    {
        return byte == std::byte{0x5a};
    }));
    allocator.deallocate(first);
    REQUIRE(allocator.allocate(16) == first);
}

TEST_CASE("FreeListAllocator rejects invalid request alignment", "[freelist][validation]")
{
    const auto alignment = GENERATE(std::size_t{0}, std::size_t{3}, std::size_t{6},
        std::numeric_limits<std::size_t>::max());
    const auto size = GENERATE(std::size_t{1}, std::size_t{1025});
    const auto strategy = GENERATE(FreeListStrategy::FirstFit, FreeListStrategy::BestFit);
    const bool exhausted = GENERATE(false, true);
    FreeListAllocator allocator(1024, strategy);
    auto* first = static_cast<std::byte*>(allocator.allocate(16));
    REQUIRE(first != nullptr);
    std::fill_n(first, 16, std::byte{0x5a});
    void* remainder = nullptr;
    if (exhausted)
    {
        const std::size_t overhead = allocator.used() - 16;
        remainder = allocator.allocate(allocator.available() - overhead);
        REQUIRE(remainder != nullptr);
        REQUIRE(allocator.available() == 0);
    }
    const std::size_t used = allocator.used();
    const std::size_t available = allocator.available();
    const std::size_t allocations = allocator.num_allocations();

    REQUIRE_THROWS_AS(allocator.allocate(size, alignment), std::invalid_argument);
    REQUIRE(allocator.used() == used);
    REQUIRE(allocator.available() == available);
    REQUIRE(allocator.num_allocations() == allocations);
    REQUIRE(std::all_of(first, first + 16, [](const std::byte byte)
    {
        return byte == std::byte{0x5a};
    }));
    void* second = allocator.allocate(16);
    REQUIRE((second == nullptr) == exhausted);
    allocator.deallocate(second);
    allocator.deallocate(remainder);
    allocator.deallocate(first);
    REQUIRE(allocator.available() == allocator.capacity());
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

TEST_CASE("FreeListAllocator preserves arbitrary byte capacities", "[freelist][backing_memory]")
{
    const auto capacity = GENERATE(std::size_t{65}, 257, 1025);
    const auto strategy = GENERATE(FreeListStrategy::FirstFit, FreeListStrategy::BestFit);
    FreeListAllocator allocator(capacity, strategy);
    REQUIRE(allocator.capacity() == capacity);
    REQUIRE(allocator.available() == capacity);

    void* probe = allocator.allocate(16);
    REQUIRE(probe != nullptr);
    REQUIRE(reinterpret_cast<std::uintptr_t>(probe) % alignof(std::max_align_t) == 0);
    const std::size_t overhead = allocator.used() - 16;
    allocator.deallocate(probe);
    const std::size_t payload = capacity - overhead;

    for (int i = 0; i < 3; ++i)
    {
        auto* ptr = static_cast<std::byte*>(allocator.allocate(payload));
        REQUIRE(ptr != nullptr);
        std::fill_n(ptr, payload, std::byte{0x5a});
        REQUIRE(std::all_of(ptr, ptr + payload, [](const std::byte byte)
        {
            return byte == std::byte{0x5a};
        }));
        REQUIRE(allocator.used() == capacity);
        REQUIRE(allocator.available() == 0);
        REQUIRE(allocator.allocate(1) == nullptr);
        allocator.deallocate(ptr);
        REQUIRE(allocator.used() == 0);
        REQUIRE(allocator.available() == capacity);
    }
}

TEST_CASE("FreeListAllocator backing size rounding overflow", "[freelist][overflow][backing_memory]")
{
    const auto strategy = GENERATE(FreeListStrategy::FirstFit, FreeListStrategy::BestFit);
    REQUIRE_THROWS_AS(FreeListAllocator(std::numeric_limits<std::size_t>::max(), strategy), std::bad_alloc);
}

TEST_CASE("FreeListAllocator move assignment transfers backing memory", "[freelist][backing_memory]")
{
    const auto strategy = GENERATE(FreeListStrategy::FirstFit, FreeListStrategy::BestFit);
    FreeListAllocator source(257, strategy);
    auto* ptr = static_cast<std::byte*>(source.allocate(16));
    REQUIRE(ptr != nullptr);
    std::fill_n(ptr, 16, std::byte{0x5a});
    const std::size_t overhead = source.used() - 16;

    FreeListAllocator destination(129);
    REQUIRE(destination.allocate(32) != nullptr);
    destination = std::move(source);

    REQUIRE(destination.capacity() == 257);
    REQUIRE(destination.num_allocations() == 1);
    REQUIRE(source.capacity() == 0);
    REQUIRE(source.used() == 0);
    REQUIRE(source.num_allocations() == 0);
    REQUIRE(std::all_of(ptr, ptr + 16, [](const std::byte byte)
    {
        return byte == std::byte{0x5a};
    }));
    destination.deallocate(ptr);
    REQUIRE(destination.used() == 0);

    void* replacement = destination.allocate(destination.capacity() - overhead);
    REQUIRE(replacement != nullptr);
    REQUIRE(destination.used() == destination.capacity());
    destination.deallocate(replacement);
    REQUIRE(destination.available() == destination.capacity());
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

TEST_CASE("FreeListAllocator alignment after odd-sized allocations", "[freelist][alignment]")
{
    const auto strategy = GENERATE(FreeListStrategy::FirstFit, FreeListStrategy::BestFit);
    const auto alignment = GENERATE(1u, 2u, 4u, 8u, 16u, 32u, 64u);
    FreeListAllocator allocator(4096, strategy);

    constexpr std::array<std::size_t, 11> sizes = {1, 3, 7, 9, 15, 17, 31, 33, 63, 65, 100};
    std::array<std::byte*, sizes.size()> ptrs{};

    for (std::size_t i = 0; i < sizes.size(); ++i)
    {
        ptrs[i] = static_cast<std::byte*>(allocator.allocate(sizes[i], alignment));
        REQUIRE(ptrs[i] != nullptr);
        REQUIRE(reinterpret_cast<std::uintptr_t>(ptrs[i]) % alignment == 0);
        std::fill_n(ptrs[i], sizes[i], static_cast<std::byte>(i + 1));
    }

    for (std::size_t i = 0; i < sizes.size(); i += 2)
    {
        allocator.deallocate(ptrs[i]);
    }

    for (std::size_t i = 1; i < sizes.size(); i += 2)
    {
        const auto value = static_cast<std::byte>(i + 1);
        REQUIRE(std::all_of(ptrs[i], ptrs[i] + sizes[i], [value](const std::byte byte)
        {
            return byte == value;
        }));
        allocator.deallocate(ptrs[i]);
    }

    REQUIRE(allocator.used() == 0);
    REQUIRE(allocator.available() == allocator.capacity());
    REQUIRE(allocator.num_allocations() == 0);

    void* ptr = allocator.allocate(4000);
    REQUIRE(ptr != nullptr);
    allocator.deallocate(ptr);
}

TEST_CASE("FreeListAllocator alignment with small remainders", "[freelist][alignment]")
{
    const auto strategy = GENERATE(FreeListStrategy::FirstFit, FreeListStrategy::BestFit);
    const auto size = GENERATE(81u, 82u, 83u, 84u, 85u, 86u, 87u, 88u,
                              89u, 90u, 91u, 92u, 93u, 94u, 95u, 96u, 97u);
    FreeListAllocator allocator(128, strategy);

    auto* first = static_cast<std::byte*>(allocator.allocate(size, 1));
    REQUIRE(first != nullptr);
    std::fill_n(first, size, std::byte{0x5a});

    auto* second = static_cast<std::byte*>(allocator.allocate(1, 1));
    if (second)
    {
        *second = std::byte{0xa5};
        allocator.deallocate(second);
    }

    REQUIRE(std::all_of(first, first + size, [](const std::byte byte)
    {
        return byte == std::byte{0x5a};
    }));

    allocator.deallocate(first);
    REQUIRE(allocator.used() == 0);
    REQUIRE(allocator.num_allocations() == 0);
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

TEST_CASE("FreeListAllocator whole-block allocation accounting", "[freelist][capacity]")
{
    const auto strategy = GENERATE(FreeListStrategy::FirstFit, FreeListStrategy::BestFit);
    const auto remainder = GENERATE(std::size_t{0}, std::size_t{1},
                                    sizeof(std::size_t), 2 * sizeof(std::size_t));
    FreeListAllocator allocator(128, strategy);

    void* probe = allocator.allocate(16, 1);
    REQUIRE(probe != nullptr);
    const std::size_t overhead = allocator.used() - 16;
    allocator.deallocate(probe);

    void* ptr = allocator.allocate(allocator.capacity() - overhead - remainder, 1);
    REQUIRE(ptr != nullptr);
    REQUIRE(allocator.used() == allocator.capacity());
    REQUIRE(allocator.available() == 0);
    REQUIRE(allocator.num_allocations() == 1);
    REQUIRE(allocator.allocate(1, 1) == nullptr);

    allocator.deallocate(ptr);
    REQUIRE(allocator.used() == 0);
    REQUIRE(allocator.available() == allocator.capacity());
    REQUIRE(allocator.num_allocations() == 0);

    for (int i = 0; i < 3; ++i)
    {
        ptr = allocator.allocate(allocator.capacity() - overhead, 1);
        REQUIRE(ptr != nullptr);
        REQUIRE(allocator.used() == allocator.capacity());
        allocator.deallocate(ptr);
        REQUIRE(allocator.used() == 0);
    }
}

TEST_CASE("FreeListAllocator coalescence after whole-block allocation", "[freelist][capacity]")
{
    const auto strategy = GENERATE(FreeListStrategy::FirstFit, FreeListStrategy::BestFit);
    FreeListAllocator allocator(512, strategy);

    void* left = allocator.allocate(32, 1);
    REQUIRE(left != nullptr);
    const std::size_t overhead = allocator.used() - 32;

    void* separator = allocator.allocate(32, 1);
    void* middle = allocator.allocate(96, 1);
    void* right = allocator.allocate(32, 1);
    REQUIRE(separator != nullptr);
    REQUIRE(middle != nullptr);
    REQUIRE(right != nullptr);

    allocator.deallocate(left);
    allocator.deallocate(middle);

    void* replacement = allocator.allocate(95, 1);
    REQUIRE(replacement == middle);

    allocator.deallocate(replacement);
    allocator.deallocate(separator);
    allocator.deallocate(right);
    REQUIRE(allocator.used() == 0);
    REQUIRE(allocator.available() == allocator.capacity());
    REQUIRE(allocator.num_allocations() == 0);

    void* ptr = allocator.allocate(allocator.capacity() - overhead, 1);
    REQUIRE(ptr != nullptr);
    allocator.deallocate(ptr);
}

TEST_CASE("FreeListAllocator allocation size overflow", "[freelist][overflow]")
{
    constexpr std::size_t maximum = std::numeric_limits<std::size_t>::max();
    const auto size = GENERATE_COPY(maximum, maximum - 1, maximum - 15);
    const auto alignment = GENERATE(1u, 8u, 16u, 64u);
    const auto strategy = GENERATE(FreeListStrategy::FirstFit, FreeListStrategy::BestFit);
    FreeListAllocator allocator(1024, strategy);

    auto* first = static_cast<std::byte*>(allocator.allocate(16));
    REQUIRE(first != nullptr);
    std::fill_n(first, 16, std::byte{0x5a});
    const std::size_t used = allocator.used();
    const std::size_t available = allocator.available();
    const std::size_t allocations = allocator.num_allocations();

    REQUIRE(allocator.allocate(size, alignment) == nullptr);
    REQUIRE(allocator.used() == used);
    REQUIRE(allocator.available() == available);
    REQUIRE(allocator.num_allocations() == allocations);
    REQUIRE(std::all_of(first, first + 16, [](const std::byte byte)
    {
        return byte == std::byte{0x5a};
    }));

    void* second = allocator.allocate(64);
    REQUIRE(second != nullptr);
    allocator.deallocate(second);
    allocator.deallocate(first);
    REQUIRE(allocator.used() == 0);

    void* ptr = allocator.allocate(allocator.capacity() - used);
    REQUIRE(ptr != nullptr);
    allocator.deallocate(ptr);
}

TEST_CASE("FreeListAllocator excessive alignment", "[freelist][overflow]")
{
    constexpr std::size_t alignment = std::size_t{1} << (std::numeric_limits<std::size_t>::digits - 1);
    const auto strategy = GENERATE(FreeListStrategy::FirstFit, FreeListStrategy::BestFit);
    FreeListAllocator allocator(1024, strategy);
    void* first = allocator.allocate(16);
    REQUIRE(first != nullptr);
    const std::size_t used = allocator.used();
    const std::size_t available = allocator.available();
    const std::size_t allocations = allocator.num_allocations();

    REQUIRE(allocator.allocate(1, alignment) == nullptr);
    REQUIRE(allocator.used() == used);
    REQUIRE(allocator.available() == available);
    REQUIRE(allocator.num_allocations() == allocations);
    void* second = allocator.allocate(64);
    REQUIRE(second != nullptr);
    allocator.deallocate(second);
    allocator.deallocate(first);
    REQUIRE(allocator.used() == 0);
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

TEST_CASE("FreeListAllocator mixed allocation and free sequences", "[freelist][regression]")
{
    const auto strategy = GENERATE(FreeListStrategy::FirstFit, FreeListStrategy::BestFit);
    const auto capacity = GENERATE(std::size_t{4096}, std::size_t{4099});
    const auto seed = GENERATE(std::uint32_t{1}, std::uint32_t{42}, std::uint32_t{2026});
    CAPTURE(strategy, capacity, seed);

    FreeListAllocator allocator(capacity, strategy);
    std::mt19937 random(seed);
    constexpr std::array<std::size_t, 12> sizes = {1, 3, 7, 15, 17, 31, 63, 65, 127, 257, 513, 1023};
    constexpr std::array<std::size_t, 8> alignments = {1, 2, 4, 8, 16, 32, 64, 256};

    struct Allocation
    {
        std::byte* ptr;
        std::size_t size;
        std::size_t reserved;
        std::byte value;
    };
    std::vector<Allocation> live;
    std::size_t reserved = 0;
    std::size_t successes = 0;
    std::size_t failures = 0;

    const auto verify_live_allocations = [&]()
    {
        REQUIRE(allocator.num_allocations() == live.size());
        REQUIRE(allocator.used() == reserved);
        REQUIRE(allocator.used() <= capacity);
        REQUIRE(allocator.available() == capacity - reserved);
        for (const auto& allocation : live)
        {
            REQUIRE(std::all_of(allocation.ptr, allocation.ptr + allocation.size,
                [&](const std::byte byte) { return byte == allocation.value; }));
        }
    };

    const auto free_one = [&]()
    {
        const std::size_t index = random() % live.size();
        allocator.deallocate(live[index].ptr);
        reserved -= live[index].reserved;
        live[index] = live.back();
        live.pop_back();
    };

    for (std::size_t step = 0; step < 1200; ++step)
    {
        CAPTURE(step);
        if (!live.empty() && random() % 4 == 0)
        {
            free_one();
        }
        else
        {
            // Select requests directly from the engine for reproducible random values.
            const std::size_t size = sizes[random() % sizes.size()];
            const std::size_t alignment = alignments[random() % alignments.size()];
            CAPTURE(size, alignment);
            auto* ptr = static_cast<std::byte*>(allocator.allocate(size, alignment));
            if (ptr)
            {
                REQUIRE(reinterpret_cast<std::uintptr_t>(ptr) % alignment == 0);
                const auto start = reinterpret_cast<std::uintptr_t>(ptr);
                for (const auto& allocation : live)
                {
                    const auto other = reinterpret_cast<std::uintptr_t>(allocation.ptr);
                    REQUIRE((start + size <= other || other + allocation.size <= start));
                }

                REQUIRE(allocator.used() >= reserved + size);
                const std::size_t block_size = allocator.used() - reserved;
                const auto value = static_cast<std::byte>(1 + step % 255);
                std::fill_n(ptr, size, value);
                live.push_back({ptr, size, block_size, value});
                reserved += block_size;
                ++successes;
            }
            else
            {
                ++failures;
            }
        }
        verify_live_allocations();
    }

    REQUIRE(successes > 0);
    REQUIRE(failures > 0);
    while (!live.empty())
    {
        free_one();
        verify_live_allocations();
    }

    // Full accounting recovery must also leave one contiguous, reusable free block.
    void* probe = allocator.allocate(1, 1);
    REQUIRE(probe != nullptr);
    const std::size_t overhead = allocator.used() - 1;
    allocator.deallocate(probe);

    auto* whole = static_cast<std::byte*>(allocator.allocate(capacity - overhead, 1));
    REQUIRE(whole != nullptr);
    REQUIRE(allocator.used() == capacity);
    REQUIRE(allocator.available() == 0);
    REQUIRE(allocator.num_allocations() == 1);
    REQUIRE(allocator.allocate(1, 1) == nullptr);
    std::fill_n(whole, capacity - overhead, std::byte{0xa5});
    REQUIRE(std::all_of(whole, whole + capacity - overhead,
        [](const std::byte byte) { return byte == std::byte{0xa5}; }));
    allocator.deallocate(whole);
    verify_live_allocations();
}
