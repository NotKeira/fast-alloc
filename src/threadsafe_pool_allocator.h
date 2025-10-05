#pragma once

#include <cstddef>
#include <cstdint>
#include <atomic>

namespace fast_alloc
{
    // Thread-safe pool allocator using lock-free stack
    class ThreadSafePoolAllocator
    {
    public:
        ThreadSafePoolAllocator(std::size_t block_size, std::size_t block_count);
        ~ThreadSafePoolAllocator();

        // Disable copy
        ThreadSafePoolAllocator(const ThreadSafePoolAllocator&) = delete;
        ThreadSafePoolAllocator& operator=(const ThreadSafePoolAllocator&) = delete;

        // Disable move (atomics can't be moved)
        ThreadSafePoolAllocator(ThreadSafePoolAllocator&&) = delete;
        ThreadSafePoolAllocator& operator=(ThreadSafePoolAllocator&&) = delete;

        void* allocate();
        void deallocate(void* ptr);

        [[nodiscard]] std::size_t block_size() const noexcept { return block_size_; }
        [[nodiscard]] std::size_t capacity() const noexcept { return block_count_; }

        [[nodiscard]] std::size_t allocated() const noexcept
        {
            return allocated_count_.load(std::memory_order_relaxed);
        }

        [[nodiscard]] bool is_full() const noexcept
        {
            return allocated() >= block_count_;
        }

    private:
        std::size_t block_size_;
        std::size_t block_count_;
        std::atomic<std::size_t> allocated_count_;
        void* memory_;
        std::atomic<void*> free_list_;
    };
} // namespace fast_alloc
