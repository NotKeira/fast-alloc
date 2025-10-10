#pragma once

#include <cstddef>
#include <cstdint>
#include <atomic>
#include <mutex>

namespace fast_alloc
{
    /**
     * @brief Thread-safe fixed-size block memory pool allocator.
     * 
     * Thread-safe variant of PoolAllocator using mutex protection.
     * Provides safe concurrent access at the cost of additional synchronization overhead.
     * 
     * Ideal for: multithreaded particle systems, concurrent audio processing,
     * network packet pools accessed by multiple threads.
     * 
     * @note Thread-safety: Fully thread-safe using std::mutex.
     * @note Memory overhead: 0 bytes per allocation (uses free space for intrusive list).
     * @note Fragmentation: None (all blocks same size).
     * @note Performance: Slightly slower than PoolAllocator due to mutex overhead.
     * 
     * @warning Move operations are disabled to prevent unsafe concurrent access.
     * @warning Block size must be at least sizeof(void*) to store free list pointers.
     */
    class ThreadSafePoolAllocator
    {
    public:
        /**
         * @brief Construct a thread-safe pool allocator.
         * 
         * @param block_size Size in bytes of each block (must be >= sizeof(void*))
         * @param block_count Number of blocks to allocate
         * @throws assert if block_size < sizeof(void*) or block_count == 0
         */
        ThreadSafePoolAllocator(std::size_t block_size, std::size_t block_count);
        ~ThreadSafePoolAllocator();

        // Disable copy
        ThreadSafePoolAllocator(const ThreadSafePoolAllocator&) = delete;
        ThreadSafePoolAllocator& operator=(const ThreadSafePoolAllocator&) = delete;

        // Disable move (unsafe with mutex)
        ThreadSafePoolAllocator(ThreadSafePoolAllocator&&) = delete;
        ThreadSafePoolAllocator& operator=(ThreadSafePoolAllocator&&) = delete;

        /**
         * @brief Allocate a single block from the pool (thread-safe).
         * 
         * @return Pointer to allocated block, or nullptr if pool is exhausted.
         * @note Complexity: O(1) + mutex lock overhead
         * @note Thread-safe: Yes
         */
        void* allocate();

        /**
         * @brief Return a block to the pool (thread-safe).
         * 
         * @param ptr Pointer to block (must be from this allocator). nullptr is safely ignored.
         * @note Complexity: O(1) + mutex lock overhead
         * @note Thread-safe: Yes
         * @warning Passing invalid pointers will trigger assertions in debug builds.
         */
        void deallocate(void* ptr);

        /** @brief Get the size of each block in bytes (thread-safe). */
        [[nodiscard]] std::size_t block_size() const noexcept { return block_size_; }

        /** @brief Get the total capacity (number of blocks) (thread-safe). */
        [[nodiscard]] std::size_t capacity() const noexcept { return block_count_; }

        /**
         * @brief Get the number of currently allocated blocks (thread-safe).
         * @note Uses relaxed memory ordering for performance.
         */
        [[nodiscard]] std::size_t allocated() const noexcept
        {
            return allocated_count_.load(std::memory_order_relaxed);
        }

        /** @brief Check if the pool is full (thread-safe). */
        [[nodiscard]] bool is_full() const noexcept
        {
            return allocated() >= block_count_;
        }

    private:
        mutable std::mutex mutex_;               ///< Mutex protecting allocate/deallocate operations
        std::size_t block_size_;                 ///< Size of each block
        std::size_t block_count_;                ///< Total number of blocks
        std::atomic<std::size_t> allocated_count_; ///< Current allocation count
        void* memory_;                           ///< Base memory pointer
        std::atomic<void*> free_list_;          ///< Head of intrusive free list
    };
} // namespace fast_alloc
