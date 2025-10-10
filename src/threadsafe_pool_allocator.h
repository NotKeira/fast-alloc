#pragma once

#include <cstddef>
#include <cstdint>
#include <atomic>
#include <mutex>

namespace fast_alloc
{
    class ThreadSafePoolAllocator
    {
    public:
        ThreadSafePoolAllocator(std::size_t block_size, std::size_t block_count);
        ~ThreadSafePoolAllocator();

        // Disable copy
        ThreadSafePoolAllocator(const ThreadSafePoolAllocator&) = delete;
        ThreadSafePoolAllocator& operator=(const ThreadSafePoolAllocator&) = delete;

        // Disable move
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
        mutable std::mutex mutex_;
        std::size_t block_size_;
        std::size_t block_count_;
        std::atomic<std::size_t> allocated_count_;
        void* memory_;
        std::atomic<void*> free_list_;
    };
} // namespace fast_alloc
