#pragma once

#include <cstddef>
#include <cstdint>

namespace fast_alloc
{
    /**
     * @brief Allocation strategy for free list allocator.
     */
    enum class FreeListStrategy
    {
        FirstFit, ///< Find first block that fits (faster, may fragment more)
        BestFit   ///< Find smallest block that fits (slower, reduces fragmentation)
    };

    /**
     * @brief General-purpose allocator supporting variable-sized allocations.
     * 
     * Maintains a linked list of free memory blocks and supports individual
     * deallocation. Automatically coalesces adjacent free blocks to reduce
     * fragmentation.
     * 
     * Ideal for: game assets, dynamic strings, script objects, UI elements,
     * any scenario requiring variable-sized allocations with individual frees.
     * 
     * @note Thread-safety: Not thread-safe.
     * @note Memory overhead: 16 bytes per allocation (AllocationHeader).
     * @note Fragmentation: Mitigated by automatic coalescence.
     * @note Performance: O(n) allocation/deallocation (searches free list).
     * 
     * @warning Not suitable for real-time systems requiring deterministic timing.
     */
    class FreeListAllocator
    {
    public:
        /**
         * @brief Construct a free list allocator.
         * 
         * @param size Total size in bytes of memory to manage
         * @param strategy Allocation strategy (FirstFit or BestFit)
         * @throws assert if size <= sizeof(FreeBlock)
         */
        explicit FreeListAllocator(std::size_t size, FreeListStrategy strategy = FreeListStrategy::FirstFit);
        ~FreeListAllocator();

        // Disable copy
        FreeListAllocator(const FreeListAllocator&) = delete;
        FreeListAllocator& operator=(const FreeListAllocator&) = delete;

        // Enable move
        FreeListAllocator(FreeListAllocator&& other) noexcept;
        FreeListAllocator& operator=(FreeListAllocator&& other) noexcept;

        /**
         * @brief Allocate memory block.
         * 
         * @param size Number of bytes to allocate (must be > 0)
         * @param alignment Memory alignment requirement (default: alignof(std::max_align_t))
         * @return Pointer to allocated memory, or nullptr if no suitable block found
         * @note Complexity: O(n) where n is number of free blocks
         * 
         * The allocator will search the free list using the configured strategy:
         * - FirstFit: Returns first block large enough (faster)
         * - BestFit: Returns smallest block large enough (less fragmentation)
         */
        void* allocate(std::size_t size, std::size_t alignment = alignof(std::max_align_t));

        /**
         * @brief Deallocate memory block.
         * 
         * Automatically coalesces with adjacent free blocks to reduce fragmentation.
         * 
         * @param ptr Pointer to memory (must be from this allocator). nullptr is safely ignored.
         * @note Complexity: O(n) where n is number of free blocks
         */
        void deallocate(void* ptr);

        /** @brief Get total capacity in bytes. */
        [[nodiscard]] std::size_t capacity() const noexcept { return size_; }

        /** @brief Get currently used bytes (including headers). */
        [[nodiscard]] std::size_t used() const noexcept { return used_memory_; }

        /** @brief Get available bytes remaining. */
        [[nodiscard]] std::size_t available() const noexcept { return size_ - used_memory_; }

        /** @brief Get number of active allocations. */
        [[nodiscard]] std::size_t num_allocations() const noexcept { return num_allocations_; }

    private:
        /**
         * @brief Header stored before each allocation.
         * Contains size and alignment adjustment information.
         */
        struct AllocationHeader
        {
            std::size_t size;       ///< Total size including header and adjustment
            std::size_t adjustment; ///< Bytes added for alignment
        };

        /**
         * @brief Free block node in the intrusive free list.
         * Stored in the free memory itself.
         */
        struct FreeBlock
        {
            std::size_t size; ///< Size of this free block
            FreeBlock* next;  ///< Next free block (sorted by address)
        };

        std::size_t size_;
        std::size_t used_memory_;
        std::size_t num_allocations_;
        FreeListStrategy strategy_;
        void* memory_;
        FreeBlock* free_blocks_; ///< Head of free list (sorted by address for coalescence)

        /**
         * @brief Merge adjacent free blocks.
         * 
         * @param previous Previous free block in list
         * @param current Current free block to potentially merge
         */
        static void coalescence(FreeBlock* previous, FreeBlock* current);

        /**
         * @brief Calculate aligned address accounting for header.
         * 
         * @param address Starting address
         * @param alignment Desired alignment
         * @param header_size Size of allocation header
         * @param[out] adjustment Total adjustment needed (header + padding)
         * @return Aligned address after header
         */
        static std::size_t align_forward_with_header(
            std::size_t address,
            std::size_t alignment,
            std::size_t header_size,
            std::size_t& adjustment
        ) noexcept;
    };
} // namespace fast_alloc
