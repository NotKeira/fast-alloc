#include "stack_allocator.h"

#include <cassert>

#ifdef _WIN32
#include <malloc.h>
#else
#include <cstdlib>
#endif

namespace fast_alloc
{
    StackAllocator::StackAllocator(const std::size_t size)
        : size_(size)
          , memory_(nullptr)
          , current_(nullptr)
    {
        assert(size > 0 && "Stack size must be greater than zero");

#ifdef _WIN32
        memory_ = _aligned_malloc(size_, alignof(std::max_align_t));
#else
        memory_ = std::aligned_alloc(alignof(std::max_align_t), size_);
#endif
        assert(memory_ && "Failed to allocate stack memory");

        current_ = memory_;
    }

    StackAllocator::~StackAllocator()
    {
        if (memory_)
        {
#ifdef _WIN32
            _aligned_free(memory_);
#else
            std::free(memory_);
#endif
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
#ifdef _WIN32
                _aligned_free(memory_);
#else
                std::free(memory_);
#endif
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

    void* StackAllocator::allocate(const std::size_t size, const std::size_t alignment)
    {
        assert(memory_ && "Allocator not initialised");

        // Calculate aligned address
        const auto current_address = reinterpret_cast<std::size_t>(current_);
        const std::size_t aligned_address = align_forward(current_address, alignment);
        const std::size_t adjustment = aligned_address - current_address;

        // Check if we have enough space
        if (const std::size_t total_size = size + adjustment; used() + total_size > size_)
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
            const auto start_address = reinterpret_cast<std::size_t>(memory_);
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
            current_ = memory_;
        }
    }

    std::size_t StackAllocator::used() const noexcept
    {
        if (!memory_ || !current_)
        {
            return 0;
        }

        const auto start = reinterpret_cast<std::size_t>(memory_);
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
            address += alignment - modulo;
        }

        return address;
    }
} // namespace fast_alloc
