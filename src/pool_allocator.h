#pragma once

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
     * @note Memory overhead: 0 bytes per allocation (uses free space for intrusive list).
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
         * @throws assert if block_size < sizeof(void*) or block_count == 0
         */
        PoolAllocator(std::size_t block_size, std::size_t block_count);
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

        /** @brief Get the size of each block in bytes. */
        [[nodiscard]] std::size_t block_size() const noexcept { return block_size_; }

        /** @brief Get the total capacity (number of blocks). */
        [[nodiscard]] std::size_t capacity() const noexcept { return block_count_; }

        /** @brief Get the number of currently allocated blocks. */
        [[nodiscard]] std::size_t allocated() const noexcept { return allocated_count_; }

        /** @brief Check if the pool is full (no blocks available). */
        [[nodiscard]] bool is_full() const noexcept { return allocated_count_ >= block_count_; }

    private:
        std::size_t block_size_;
        std::size_t block_count_;
        std::size_t allocated_count_;
        void* memory_;
        void* free_list_;  // Intrusive linked list of free blocks
    };
} // namespace fast_alloc
