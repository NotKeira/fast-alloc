#include "threadsafe_pool_allocator.h"

#include <cassert>
#include <cstring>
#include <cstdlib>


namespace fast_alloc
{
    ThreadSafePoolAllocator::ThreadSafePoolAllocator(const std::size_t block_size, const std::size_t block_count)
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

        for (std::size_t i = 0; i < block_count_ - 1; ++i)
        {
            const auto current = reinterpret_cast<void**>(block);
            block += block_size_;
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

        *static_cast<void**>(ptr) = free_list_.load(std::memory_order_relaxed);
        free_list_.store(ptr, std::memory_order_relaxed);
        allocated_count_.fetch_sub(1, std::memory_order_relaxed);
    }
} // namespace fast_alloc
