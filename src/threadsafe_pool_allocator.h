#pragma once

#include "detail/aligned_memory.h"

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
     * @note No separate per-allocation metadata (uses free space for the intrusive list).
     * @note Blocks may include trailing padding to meet the requested alignment.
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
         * @param alignment Required block alignment (non-zero power of two).
         *        Defaults to alignof(std::max_align_t); raised to alignof(void*) if smaller.
         * @throws assert if block_size < sizeof(void*) or block_count == 0
         * @throws std::invalid_argument if alignment is zero or not a power of two
         * @throws std::bad_alloc if the padded stride or total pool size overflows, or backing allocation fails
         */
        ThreadSafePoolAllocator(std::size_t block_size, std::size_t block_count,
                               std::size_t alignment = alignof(std::max_align_t));
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

        /** @brief Get the requested block size in bytes, excluding padding (thread-safe). */
        [[nodiscard]] std::size_t block_size() const noexcept { return block_size_; }

        /** @brief Get the distance between consecutive blocks in bytes, including padding (thread-safe). */
        [[nodiscard]] std::size_t block_stride() const noexcept { return block_stride_; }

        /** @brief Get the guaranteed alignment of every block in bytes (thread-safe). */
        [[nodiscard]] std::size_t alignment() const noexcept { return alignment_; }

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
        std::size_t block_size_;                 ///< Requested size of each block
        std::size_t block_stride_;               ///< Padded distance between blocks
        std::size_t alignment_;                  ///< Guaranteed block alignment
        std::size_t block_count_;                ///< Total number of blocks
        std::atomic<std::size_t> allocated_count_; ///< Current allocation count
        detail::AlignedMemory memory_;           ///< Owned backing memory
        std::atomic<void*> free_list_;          ///< Head of intrusive free list
    };
} // namespace fast_alloc
