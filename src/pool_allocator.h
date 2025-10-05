#pragma once

#include <cstddef>
#include <cstdint>

namespace fast_alloc
{
    class PoolAllocator
    {
    public:
        PoolAllocator(std::size_t block_size, std::size_t block_count);
        ~PoolAllocator();

        // Disable copy
        PoolAllocator(const PoolAllocator&) = delete;
        PoolAllocator& operator=(const PoolAllocator&) = delete;

        // Enable move
        PoolAllocator(PoolAllocator&& other) noexcept;
        PoolAllocator& operator=(PoolAllocator&& other) noexcept;

        void* allocate();
        void deallocate(void* ptr);

        [[nodiscard]] std::size_t block_size() const noexcept { return block_size_; }
        [[nodiscard]] std::size_t capacity() const noexcept { return block_count_; }
        [[nodiscard]] std::size_t allocated() const noexcept { return allocated_count_; }
        [[nodiscard]] bool is_full() const noexcept { return allocated_count_ >= block_count_; }

    private:
        std::size_t block_size_;
        std::size_t block_count_;
        std::size_t allocated_count_;
        void* memory_;
        void* free_list_;
    };
} // namespace fast_alloc
