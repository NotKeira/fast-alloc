#include "pool_allocator.h"

#include <cassert>
#include <cstring>

#ifdef _WIN32
#include <malloc.h>
#else
#include <cstdlib>
#endif

namespace fast_alloc
{
    PoolAllocator::PoolAllocator(std::size_t block_size, std::size_t block_count)
        : block_size_(block_size)
          , block_count_(block_count)
          , allocated_count_(0)
          , memory_(nullptr)
          , free_list_(nullptr)
    {
        assert(block_size >= sizeof(void*) && "Block size must be at least pointer size");
        assert(block_count > 0 && "Block count must be greater than zero");

        // Allocate the memory pool
#ifdef _WIN32
        memory_ = _aligned_malloc(block_size_ * block_count_, alignof(std::max_align_t));
#else
        memory_ = std::aligned_alloc(alignof(std::max_align_t), block_size_ * block_count_);
#endif
        assert(memory_ && "Failed to allocate memory pool");

        // Initialise free list - each block points to the next
        auto* block = static_cast<std::byte*>(memory_);
        free_list_ = block;

        for (std::size_t i = 0; i < block_count_ - 1; ++i)
        {
            const auto current = reinterpret_cast<void**>(block);
            block += block_size_;
            *current = block;
        }

        // Last block points to nullptr
        void** last = reinterpret_cast<void**>(block);
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

        // Push back to free list
        void** block = static_cast<void**>(ptr);
        *block = free_list_;
        free_list_ = ptr;
        --allocated_count_;
    }
} // namespace fast_alloc