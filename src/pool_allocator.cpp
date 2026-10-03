#include "pool_allocator.h"

#include <algorithm>
#include <cassert>
#include <cstring>
#include <limits>
#include <new>
#include <stdexcept>

#ifdef _WIN32
#include <malloc.h>
#else
#include <cstdlib>
#endif

namespace fast_alloc
{
    PoolAllocator::PoolAllocator(const std::size_t block_size, const std::size_t block_count,
                                 const std::size_t alignment)
        : block_size_(block_size)
          , block_stride_(block_size)
          , alignment_(alignment)
          , block_count_(block_count)
          , allocated_count_(0)
          , memory_(nullptr)
          , free_list_(nullptr)
    {
        assert(block_size >= sizeof(void*) && "Block size must be at least pointer size");
        assert(block_count > 0 && "Block count must be greater than zero");

        if (alignment_ == 0 || (alignment_ & (alignment_ - 1)) != 0)
        {
            throw std::invalid_argument("Alignment must be a non-zero power of two");
        }
        alignment_ = std::max(alignment_, alignof(void*));

        // Pad the stride so every block can hold an aligned free list pointer.
        const std::size_t padding = (alignment_ - block_size_ % alignment_) % alignment_;
        if (padding > std::numeric_limits<std::size_t>::max() - block_size_)
        {
            throw std::bad_alloc();
        }
        block_stride_ = block_size_ + padding;

        if (block_count_ != 0 && block_stride_ > std::numeric_limits<std::size_t>::max() / block_count_)
        {
            throw std::bad_alloc();
        }
        const std::size_t pool_size = block_stride_ * block_count_;

#ifdef _WIN32
        memory_ = _aligned_malloc(pool_size, alignment_);
#else
        memory_ = std::aligned_alloc(alignment_, pool_size);
#endif
        assert(memory_ && "Failed to allocate memory pool");

        // Initialise free list - each block points to the next
        auto* block = static_cast<std::byte*>(memory_);
        free_list_ = block;

        for (std::size_t i = 0; i < block_count_ - 1; ++i)
        {
            const auto current = reinterpret_cast<void**>(block);
            block += block_stride_;
            *current = block;
        }

        // Last block points to nullptr
        const auto last = reinterpret_cast<void**>(block);
        *last = nullptr;
    }

    PoolAllocator::~PoolAllocator()
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

    PoolAllocator::PoolAllocator(PoolAllocator&& other) noexcept
        : block_size_(other.block_size_)
          , block_stride_(other.block_stride_)
          , alignment_(other.alignment_)
          , block_count_(other.block_count_)
          , allocated_count_(other.allocated_count_)
          , memory_(other.memory_)
          , free_list_(other.free_list_)
    {
        other.memory_ = nullptr;
        other.free_list_ = nullptr;
        other.allocated_count_ = 0;
    }

    PoolAllocator& PoolAllocator::operator=(PoolAllocator&& other) noexcept
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

            block_size_ = other.block_size_;
            block_stride_ = other.block_stride_;
            alignment_ = other.alignment_;
            block_count_ = other.block_count_;
            allocated_count_ = other.allocated_count_;
            memory_ = other.memory_;
            free_list_ = other.free_list_;

            other.memory_ = nullptr;
            other.free_list_ = nullptr;
            other.allocated_count_ = 0;
        }
        return *this;
    }

    void* PoolAllocator::allocate()
    {
        if (!free_list_)
        {
            return nullptr; // Pool exhausted
        }

        // Pop from free list
        void* block = free_list_;
        free_list_ = *static_cast<void**>(free_list_);
        ++allocated_count_;

        return block;
    }

    void PoolAllocator::deallocate(void* ptr)
    {
        if (!ptr)
        {
            return;
        }

        assert(allocated_count_ > 0 && "Deallocating from empty pool");

        // Validate pointer is within our memory range
        const auto ptr_address = reinterpret_cast<std::size_t>(ptr);
        const auto memory_start = reinterpret_cast<std::size_t>(memory_);
        const auto memory_end = memory_start + (block_stride_ * block_count_);

        assert(ptr_address >= memory_start && ptr_address < memory_end
            && "Pointer outside pool memory range");

        // Validate pointer is properly aligned to a block boundary
        assert((ptr_address - memory_start) % block_stride_ == 0
            && "Pointer not aligned to block boundary");

        // Suppress unused variable warnings in release builds
        (void)ptr_address;
        (void)memory_end;

        // Push back to free list
        const auto block = static_cast<void**>(ptr);
        *block = free_list_;
        free_list_ = ptr;
        --allocated_count_;
    }
} // namespace fast_alloc
