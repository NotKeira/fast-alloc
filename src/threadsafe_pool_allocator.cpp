#include "threadsafe_pool_allocator.h"

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
    ThreadSafePoolAllocator::ThreadSafePoolAllocator(const std::size_t block_size, const std::size_t block_count,
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

        for (std::size_t i = 0; i < block_count_ - 1; ++i)
        {
            const auto current = reinterpret_cast<void**>(block);
            block += block_stride_;
            *current = block;
        }

        // Last block points to nullptr
        const auto last = reinterpret_cast<void**>(block);
        *last = nullptr;

        // Set initial free list head
        free_list_.store(memory_, std::memory_order_release);
    }

    ThreadSafePoolAllocator::~ThreadSafePoolAllocator()
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

    void* ThreadSafePoolAllocator::allocate()
    {
        std::lock_guard<std::mutex> lock(mutex_);

        void* ptr = free_list_.load(std::memory_order_relaxed);
        if (!ptr) return nullptr;

        void* next = *static_cast<void**>(ptr);
        free_list_.store(next, std::memory_order_relaxed);
        allocated_count_.fetch_add(1, std::memory_order_relaxed);

        return ptr;
    }

    void ThreadSafePoolAllocator::deallocate(void* ptr)
    {
        if (!ptr) return;

        std::lock_guard<std::mutex> lock(mutex_);

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

        *static_cast<void**>(ptr) = free_list_.load(std::memory_order_relaxed);
        free_list_.store(ptr, std::memory_order_relaxed);
        allocated_count_.fetch_sub(1, std::memory_order_relaxed);
    }
} // namespace fast_alloc
