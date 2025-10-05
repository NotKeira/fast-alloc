#include "threadsafe_pool_allocator.h"
#include <cassert>
#include <cstring>

#ifdef _WIN32
#include <malloc.h>
#else
#include <cstdlib>
#endif

namespace fast_alloc
{
    ThreadSafePoolAllocator::ThreadSafePoolAllocator(const std::size_t block_size, const std::size_t block_count)
        : block_size_(block_size)
          , block_count_(block_count)
          , allocated_count_(0)
          , memory_(nullptr)
          , free_list_({nullptr, 0})
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
        free_list_.store({memory_, 0}, std::memory_order_relaxed);
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
        TaggedPointer old_head = free_list_.load(std::memory_order_acquire);

        while (old_head.ptr != nullptr)
        {
            // Read next pointer from the block
            void* next = *static_cast<void**>(old_head.ptr);

            // Try to update head to next with incremented tag

            // CAS: if free_list_ still equals old_head, update to new_head
            if (const TaggedPointer new_head = {next, old_head.tag + 1}; free_list_.compare_exchange_weak(
                old_head,
                new_head,
                std::memory_order_release,
                std::memory_order_acquire))
            {
                // Successfully allocated
                allocated_count_.fetch_add(1, std::memory_order_relaxed);
                return old_head.ptr;
            }

            // CAS failed, old_head now contains the new value, retry
        }

        // Pool exhausted
        return nullptr;
    }

    void ThreadSafePoolAllocator::deallocate(void* ptr)
    {
        if (!ptr)
        {
            return;
        }

        TaggedPointer old_head = free_list_.load(std::memory_order_acquire);
        TaggedPointer new_head{};

        do
        {
            // Make this block point to current head
            *static_cast<void**>(ptr) = old_head.ptr;

            // New head points to this block with incremented tag
            new_head = {ptr, old_head.tag + 1};

            // CAS: if free_list_ still equals old_head, update to new_head
        }
        while (!free_list_.compare_exchange_weak(
            old_head,
            new_head,
            std::memory_order_release,
            std::memory_order_acquire));

        // Successfully deallocated
        allocated_count_.fetch_sub(1, std::memory_order_relaxed);
    }
} // namespace fast_alloc
