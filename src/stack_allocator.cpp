#include "stack_allocator.h"
#include <malloc.h>
#include <cassert>

namespace fast_alloc
{
    StackAllocator::StackAllocator(std::size_t size)
        : size_(size)
          , memory_(nullptr)
          , current_(nullptr)
    {
        assert(size > 0 && "Stack size must be greater than zero");

        memory_ = _aligned_malloc(size_, alignof(std::max_align_t));
        assert(memory_ && "Failed to allocate stack memory");

        current_ = memory_;
    }

    StackAllocator::~StackAllocator()
    {
        if (memory_)
        {
            _aligned_free(memory_);
        }
    }

    StackAllocator::StackAllocator(StackAllocator&& other) noexcept
        : size_(other.size_)
          , memory_(other.memory_)
          , current_(other.current_)
    {
        other.memory_ = nullptr;
        other.current_ = nullptr;
        other.size_ = 0;
    }

    StackAllocator& StackAllocator::operator=(StackAllocator&& other) noexcept
    {
        if (this != &other)
        {
            if (memory_)
            {
                _aligned_free(memory_);
            }

            size_ = other.size_;
            memory_ = other.memory_;
            current_ = other.current_;

            other.memory_ = nullptr;
            other.current_ = nullptr;
            other.size_ = 0;
        }
        return *this;
    }

    void* StackAllocator::allocate(std::size_t size, std::size_t alignment)
    {
        assert(memory_ && "Allocator not initialised");

        // Calculate aligned address
        std::size_t current_address = reinterpret_cast<std::size_t>(current_);
        std::size_t aligned_address = align_forward(current_address, alignment);
        std::size_t adjustment = aligned_address - current_address;

        // Check if we have enough space
        std::size_t total_size = size + adjustment;
        if (used() + total_size > size_)
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
            std::size_t marker_address = reinterpret_cast<std::size_t>(marker);
            std::size_t start_address = reinterpret_cast<std::size_t>(memory_);
            std::size_t end_address = start_address + size_;

            assert(marker_address >= start_address && marker_address <= end_address
                && "Invalid marker");

            current_ = marker;
        }
        else
        {
            // Reset to beginning
            current_ = memory_;
        }
    }

    std::size_t StackAllocator::used() const noexcept
    {
        if (!memory_ || !current_)
        {
            return 0;
        }

        std::size_t start = reinterpret_cast<std::size_t>(memory_);
        std::size_t current = reinterpret_cast<std::size_t>(current_);

        return current - start;
    }

    std::size_t StackAllocator::available() const noexcept
    {
        return size_ - used();
    }

    std::size_t StackAllocator::align_forward(std::size_t address, std::size_t alignment) const noexcept
    {
        assert((alignment & (alignment - 1)) == 0 && "Alignment must be power of 2");

        std::size_t modulo = address & (alignment - 1);
        if (modulo != 0)
        {
            address += alignment - modulo;
        }

        return address;
    }
} // namespace fast_alloc
