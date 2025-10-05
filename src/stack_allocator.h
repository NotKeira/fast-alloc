#pragma once

#include <cstddef>
#include <cstdint>

namespace fast_alloc
{
    class StackAllocator
    {
    public:
        explicit StackAllocator(std::size_t size);
        ~StackAllocator();

        // Disable copy
        StackAllocator(const StackAllocator&) = delete;
        StackAllocator& operator=(const StackAllocator&) = delete;

        // Enable move
        StackAllocator(StackAllocator&& other) noexcept;
        StackAllocator& operator=(StackAllocator&& other) noexcept;

        void* allocate(std::size_t size, std::size_t alignment = alignof(std::max_align_t));

        // Reset to a previous marker or to the beginning
        void reset(void* marker = nullptr);

        // Get current position for later reset
        [[nodiscard]] void* get_marker() const noexcept { return current_; }

        [[nodiscard]] std::size_t capacity() const noexcept { return size_; }
        [[nodiscard]] std::size_t used() const noexcept;
        [[nodiscard]] std::size_t available() const noexcept;

    private:
        std::size_t size_;
        void* memory_;
        void* current_;

        [[nodiscard]] static std::size_t align_forward(std::size_t address, std::size_t alignment) noexcept;
    };
} // namespace fast_alloc