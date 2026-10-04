#pragma once

#include "detail/aligned_memory.h"

#include <cstddef>
#include <cstdint>

namespace fast_alloc
{
    /**
     * @brief Linear (stack-based) allocator with frame reset capability.
     * 
     * Extremely fast O(1) allocation using pointer bumping. Perfect for temporary
     * per-frame allocations with LIFO (last-in-first-out) lifetime.
     * 
     * Ideal for: rendering command lists, string formatting, scratch buffers,
     * temporary calculations within a frame or function scope.
     * 
     * @note Thread-safety: Not thread-safe.
     * @note Memory overhead: 0 bytes per allocation.
     * @note Fragmentation: None.
     * 
     * @warning Cannot deallocate individual allocations - only reset to marker or beginning.
     */
    class StackAllocator
    {
    public:
        /**
         * @brief Construct a stack allocator.
         * 
         * @param size Total size in bytes of the stack memory
         * @throws std::invalid_argument if size == 0
         * @throws std::bad_alloc if backing size rounding overflows or allocation fails
         * @note Backing memory padding does not increase the requested capacity.
         */
        explicit StackAllocator(std::size_t size);
        ~StackAllocator();

        // Disable copy
        StackAllocator(const StackAllocator&) = delete;
        StackAllocator& operator=(const StackAllocator&) = delete;

        // Enable move
        StackAllocator(StackAllocator&& other) noexcept;
        StackAllocator& operator=(StackAllocator&& other) noexcept;

        /**
         * @brief Allocate memory from the stack.
         * 
         * @param size Number of bytes to allocate (zero is permitted)
         * @param alignment Non-zero power-of-two alignment (default: alignof(std::max_align_t))
         * @return Pointer to allocated memory, or nullptr if insufficient space
         * @throws std::invalid_argument if alignment is zero or not a power of two
         * @note Complexity: O(1) - pointer arithmetic only
         * @note Failed allocations leave the allocator unchanged.
         * 
         * Example:
         * @code
         * void* ptr = stack.allocate(256, 16);  // 256 bytes, 16-byte aligned
         * @endcode
         */
        void* allocate(std::size_t size, std::size_t alignment = alignof(std::max_align_t));

        /**
         * @brief Reset allocator to a previous state or to the beginning.
         * 
         * @param marker Position to reset to (from get_marker()), or nullptr to reset to start
         * @note Complexity: O(1) - single pointer assignment
         * 
         * Example:
         * @code
         * void* marker = stack.get_marker();
         * // ... allocations ...
         * stack.reset(marker);  // Rewind to marker
         * stack.reset();        // Reset to beginning
         * @endcode
         */
        void reset(void* marker = nullptr);

        /**
         * @brief Get current position marker for later reset.
         * 
         * @return Opaque marker representing current stack position
         * @note Use with reset() to implement scoped memory allocation
         */
        [[nodiscard]] void* get_marker() const noexcept { return current_; }

        /** @brief Get total capacity in bytes. */
        [[nodiscard]] std::size_t capacity() const noexcept { return size_; }

        /** @brief Get currently used bytes. */
        [[nodiscard]] std::size_t used() const noexcept;

        /** @brief Get available bytes remaining. */
        [[nodiscard]] std::size_t available() const noexcept;

    private:
        std::size_t size_;
        detail::AlignedMemory memory_;
        void* current_;  // Current top of stack

        /**
         * @brief Align address forward to meet alignment requirement.
         * @param address Address to align
         * @param alignment Alignment requirement (must be power of 2)
         * @return Aligned address, or zero if the address calculation overflows
         */
        [[nodiscard]] static std::size_t align_forward(std::size_t address, std::size_t alignment) noexcept;
    };
} // namespace fast_alloc
