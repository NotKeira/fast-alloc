#include "stack_allocator.h"

#include <cassert>
#include <limits>
#include <utility>

namespace fast_alloc
{
    StackAllocator::StackAllocator(const std::size_t size)
        : size_(size)
          , memory_(nullptr)
          , current_(nullptr)
    {
        assert(size > 0 && "Stack size must be greater than zero");

        memory_ = detail::allocate_aligned_memory(size_, alignof(std::max_align_t));
        current_ = memory_.get();
    }

    StackAllocator::~StackAllocator() = default;

    StackAllocator::StackAllocator(StackAllocator&& other) noexcept
        : size_(other.size_)
          , memory_(std::move(other.memory_))
          , current_(other.current_)
    {
        other.current_ = nullptr;
        other.size_ = 0;
    }

    StackAllocator& StackAllocator::operator=(StackAllocator&& other) noexcept
    {
        if (this != &other)
        {
            size_ = other.size_;
            memory_ = std::move(other.memory_);
            current_ = other.current_;

            other.current_ = nullptr;
            other.size_ = 0;
        }
        return *this;
    }

    void* StackAllocator::allocate(const std::size_t size, const std::size_t alignment)
    {
        assert(memory_ && "Allocator not initialised");

        const std::size_t remaining = available();
        if (size > remaining)
        {
            return nullptr;
        }

        // Calculate aligned address
        const auto current_address = reinterpret_cast<std::size_t>(current_);
        const std::size_t aligned_address = align_forward(current_address, alignment);
        if (aligned_address == 0)
        {
            return nullptr;
        }
        const std::size_t adjustment = aligned_address - current_address;

        // Subtract the payload before checking padding to avoid size overflow.
        if (adjustment > remaining - size)
        {
            return nullptr; // Out of memory
        }

        // Update current pointer
        current_ = reinterpret_cast<void*>(aligned_address + size);

        return reinterpret_cast<void*>(aligned_address);
    }

    void StackAllocator::reset(void* marker)
    {
        assert(memory_ && "Allocator not initialised");

        if (marker)
        {
            // Validate marker is within our memory range
            const auto start_address = reinterpret_cast<std::size_t>(memory_.get());
            const std::size_t end_address = start_address + size_;
            const auto marker_address = reinterpret_cast<std::size_t>(marker);

            assert(marker_address >= start_address && marker_address <= end_address
                && "Invalid marker");

            // Suppress unused variable warnings
            (void)marker_address;
            (void)end_address;

            current_ = marker;
        }
        else
        {
            // Reset to beginning
            current_ = memory_.get();
        }
    }

    std::size_t StackAllocator::used() const noexcept
    {
        if (!memory_ || !current_)
        {
            return 0;
        }

        const auto start = reinterpret_cast<std::size_t>(memory_.get());
        const auto current = reinterpret_cast<std::size_t>(current_);

        return current - start;
    }

    std::size_t StackAllocator::available() const noexcept
    {
        return size_ - used();
    }

    std::size_t StackAllocator::align_forward(std::size_t address, const std::size_t alignment) noexcept
    {
        assert((alignment & (alignment - 1)) == 0 && "Alignment must be power of 2");

        if (const std::size_t modulo = address & (alignment - 1); modulo != 0)
        {
            const std::size_t padding = alignment - modulo;
            if (padding > std::numeric_limits<std::size_t>::max() - address)
            {
                return 0;
            }
            address += padding;
        }

        return address;
    }
} // namespace fast_alloc
