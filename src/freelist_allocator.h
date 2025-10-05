#pragma once

#include <cstddef>
#include <cstdint>

namespace fast_alloc
{
    enum class FreeListStrategy
    {
        FirstFit, // Find first block that fits
        BestFit // Find the smallest block that fits
    };

    class FreeListAllocator
    {
    public:
        explicit FreeListAllocator(std::size_t size, FreeListStrategy strategy = FreeListStrategy::FirstFit);
        ~FreeListAllocator();

        // Disable copy
        FreeListAllocator(const FreeListAllocator&) = delete;
        FreeListAllocator& operator=(const FreeListAllocator&) = delete;

        // Enable move
        FreeListAllocator(FreeListAllocator&& other) noexcept;
        FreeListAllocator& operator=(FreeListAllocator&& other) noexcept;

        void* allocate(std::size_t size, std::size_t alignment = alignof(std::max_align_t));
        void deallocate(void* ptr);

        [[nodiscard]] std::size_t capacity() const noexcept { return size_; }
        [[nodiscard]] std::size_t used() const noexcept { return used_memory_; }
        [[nodiscard]] std::size_t available() const noexcept { return size_ - used_memory_; }
        [[nodiscard]] std::size_t num_allocations() const noexcept { return num_allocations_; }

    private:
        struct AllocationHeader
        {
            std::size_t size;
            std::size_t adjustment;
        };

        struct FreeBlock
        {
            std::size_t size;
            FreeBlock* next;
        };

        std::size_t size_;
        std::size_t used_memory_;
        std::size_t num_allocations_;
        FreeListStrategy strategy_;
        void* memory_;
        FreeBlock* free_blocks_;

        static void coalescence(FreeBlock* previous, FreeBlock* current);
        static std::size_t align_forward_with_header(
            std::size_t address,
            std::size_t alignment,
            std::size_t header_size,
            std::size_t& adjustment
        ) noexcept;
    };
} // namespace fast_alloc