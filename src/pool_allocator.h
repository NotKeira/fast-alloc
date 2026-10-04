#pragma once

#include "detail/aligned_memory.h"

#include <cstddef>
#include <cstdint>

namespace fast_alloc
{
    /**
     * @brief Fixed-size block memory pool allocator.
     * 
     * Extremely fast O(1) allocation/deallocation for objects of uniform size.
     * Ideal for particle systems, game entities, audio voices, and network packets.
     * 
     * @note Thread-safety: Not thread-safe. Use ThreadSafePoolAllocator for concurrent access.
     * @note No separate per-allocation metadata (uses free space for the intrusive list).
     * @note Blocks may include trailing padding to meet the requested alignment.
     * @note Fragmentation: None (all blocks same size).
     * 
     * @warning Block size must be at least sizeof(void*) to store free list pointers.
     */
    class PoolAllocator
    {
    public:
        /**
         * @brief Construct a pool allocator.
         * 
         * @param block_size Size in bytes of each block (must be >= sizeof(void*))
         * @param block_count Number of blocks to allocate
         * @param alignment Required block alignment (non-zero power of two).
         *        Defaults to alignof(std::max_align_t); raised to alignof(void*) if smaller.
         * @throws std::invalid_argument if block_size < sizeof(void*), block_count == 0,
         *         or alignment is zero or not a power of two
         * @throws std::bad_alloc if the padded stride or total pool size overflows, or backing allocation fails
         */
        PoolAllocator(std::size_t block_size, std::size_t block_count,
                      std::size_t alignment = alignof(std::max_align_t));
        ~PoolAllocator();

        // Disable copy
        PoolAllocator(const PoolAllocator&) = delete;
        PoolAllocator& operator=(const PoolAllocator&) = delete;

        // Enable move
        PoolAllocator(PoolAllocator&& other) noexcept;
        PoolAllocator& operator=(PoolAllocator&& other) noexcept;

        /**
         * @brief Allocate a single block from the pool.
         * 
         * @return Pointer to allocated block, or nullptr if pool is exhausted.
         * @note Complexity: O(1) - single pointer dereference
         */
        void* allocate();

        /**
         * @brief Return a block to the pool.
         * 
         * @param ptr Pointer to block (must be from this allocator). nullptr is safely ignored.
         * @note Complexity: O(1) - two pointer assignments
         * @warning Passing invalid pointers will trigger assertions in debug builds.
         */
        void deallocate(void* ptr);

        /** @brief Get the requested size of each block in bytes, excluding padding. */
        [[nodiscard]] std::size_t block_size() const noexcept { return block_size_; }

        /** @brief Get the distance between consecutive blocks in bytes, including padding. */
        [[nodiscard]] std::size_t block_stride() const noexcept { return block_stride_; }

        /** @brief Get the guaranteed alignment of every block in bytes. */
        [[nodiscard]] std::size_t alignment() const noexcept { return alignment_; }

        /** @brief Get the total capacity (number of blocks). */
        [[nodiscard]] std::size_t capacity() const noexcept { return block_count_; }

        /** @brief Get the number of currently allocated blocks. */
        [[nodiscard]] std::size_t allocated() const noexcept { return allocated_count_; }

        /** @brief Check if the pool is full (no blocks available). */
        [[nodiscard]] bool is_full() const noexcept { return allocated_count_ >= block_count_; }

    private:
        std::size_t block_size_;
        std::size_t block_stride_;
        std::size_t alignment_;
        std::size_t block_count_;
        std::size_t allocated_count_;
        detail::AlignedMemory memory_;
        void* free_list_;  // Intrusive linked list of free blocks
    };
} // namespace fast_alloc
