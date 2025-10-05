#include "freelist_allocator.h"
#include <malloc.h>
#include <cassert>
// #include <algorithm>

namespace fast_alloc
{
    FreeListAllocator::FreeListAllocator(std::size_t size, FreeListStrategy strategy)
        : size_(size)
          , used_memory_(0)
          , num_allocations_(0)
          , strategy_(strategy)
          , memory_(nullptr)
          , free_blocks_(nullptr)
    {
        assert(size > sizeof(FreeBlock) && "Size must be larger than FreeBlock");

        memory_ = _aligned_malloc(size_, alignof(std::max_align_t));
        assert(memory_ && "Failed to allocate memory");

        // Initialise with one large free block
        free_blocks_ = static_cast<FreeBlock*>(memory_);
        free_blocks_->size = size_;
        free_blocks_->next = nullptr;
    }

    FreeListAllocator::~FreeListAllocator()
    {
        if (memory_)
        {
            _aligned_free(memory_);
        }
    }

    FreeListAllocator::FreeListAllocator(FreeListAllocator&& other) noexcept
        : size_(other.size_)
          , used_memory_(other.used_memory_)
          , num_allocations_(other.num_allocations_)
          , strategy_(other.strategy_)
          , memory_(other.memory_)
          , free_blocks_(other.free_blocks_)
    {
        other.memory_ = nullptr;
        other.free_blocks_ = nullptr;
        other.size_ = 0;
        other.used_memory_ = 0;
        other.num_allocations_ = 0;
    }

    FreeListAllocator& FreeListAllocator::operator=(FreeListAllocator&& other) noexcept
    {
        if (this != &other)
        {
            if (memory_)
            {
                _aligned_free(memory_);
            }

            size_ = other.size_;
            used_memory_ = other.used_memory_;
            num_allocations_ = other.num_allocations_;
            strategy_ = other.strategy_;
            memory_ = other.memory_;
            free_blocks_ = other.free_blocks_;

            other.memory_ = nullptr;
            other.free_blocks_ = nullptr;
            other.size_ = 0;
            other.used_memory_ = 0;
            other.num_allocations_ = 0;
        }
        return *this;
    }

    void* FreeListAllocator::allocate(std::size_t size, std::size_t alignment)
    {
        assert(size > 0 && "Allocation size must be greater than zero");
        assert(memory_ && "Allocator not initialised");

        FreeBlock* prev_block = nullptr;
        FreeBlock* current_block = free_blocks_;
        FreeBlock* best_block = nullptr;
        FreeBlock* best_prev = nullptr;

        std::size_t best_size = SIZE_MAX;

        // Search for suitable block
        while (current_block)
        {
            std::size_t adjustment = 0;
            // std::size_t aligned_address = align_forward_with_header(
            //     reinterpret_cast<std::size_t>(current_block),
            //     alignment,
            //     sizeof(AllocationHeader),
            //     adjustment
            // );

            if (const std::size_t total_size = size + adjustment; current_block->size >= total_size)
            {
                if (strategy_ == FreeListStrategy::FirstFit)
                {
                    // Use first fit - take this block immediately
                    best_block = current_block;
                    best_prev = prev_block;
                    break;
                }
                else if (strategy_ == FreeListStrategy::BestFit)
                {
                    // Use best fit - find smallest suitable block
                    if (current_block->size < best_size)
                    {
                        best_size = current_block->size;
                        best_block = current_block;
                        best_prev = prev_block;
                    }
                }
            }

            prev_block = current_block;
            current_block = current_block->next;
        }

        if (!best_block)
        {
            return nullptr; // No suitable block found
        }

        // Calculate adjustment again for the selected block
        std::size_t adjustment = 0;
        std::size_t aligned_address = align_forward_with_header(
            reinterpret_cast<std::size_t>(best_block),
            alignment,
            sizeof(AllocationHeader),
            adjustment
        );

        std::size_t total_size = size + adjustment;

        // If remaining space is large enough, split the block
        if (best_block->size - total_size > sizeof(FreeBlock))
        {
            FreeBlock* new_block = reinterpret_cast<FreeBlock*>(
                reinterpret_cast<std::size_t>(best_block) + total_size
            );
            new_block->size = best_block->size - total_size;
            new_block->next = best_block->next;

            if (best_prev)
            {
                best_prev->next = new_block;
            }
            else
            {
                free_blocks_ = new_block;
            }
        }
        else
        {
            // Use entire block
            if (best_prev)
            {
                best_prev->next = best_block->next;
            }
            else
            {
                free_blocks_ = best_block->next;
            }
        }

        // Write allocation header
        AllocationHeader* header = reinterpret_cast<AllocationHeader*>(
            aligned_address - sizeof(AllocationHeader)
        );
        header->size = total_size;
        header->adjustment = adjustment;

        used_memory_ += total_size;
        ++num_allocations_;

        return reinterpret_cast<void*>(aligned_address);
    }

    void FreeListAllocator::deallocate(void* ptr)
    {
        if (!ptr)
        {
            return;
        }

        assert(memory_ && "Allocator not initialised");
        assert(num_allocations_ > 0 && "Deallocating from empty allocator");

        // Get allocation header
        std::size_t block_address = reinterpret_cast<std::size_t>(ptr);
        AllocationHeader* header = reinterpret_cast<AllocationHeader*>(
            block_address - sizeof(AllocationHeader)
        );

        std::size_t block_start = block_address - header->adjustment;
        std::size_t block_size = header->size;

        // Create new free block
        FreeBlock* new_block = reinterpret_cast<FreeBlock*>(block_start);
        new_block->size = block_size;
        new_block->next = nullptr;

        // Insert into free list (sorted by address for coalescence)
        FreeBlock* prev_block = nullptr;
        FreeBlock* current_block = free_blocks_;

        while (current_block)
        {
            if (reinterpret_cast<std::size_t>(current_block) > block_start)
            {
                break;
            }
            prev_block = current_block;
            current_block = current_block->next;
        }

        // Insert new block
        if (prev_block)
        {
            prev_block->next = new_block;
        }
        else
        {
            free_blocks_ = new_block;
        }
        new_block->next = current_block;

        // Try to merge with adjacent blocks
        coalescence(prev_block, new_block);

        used_memory_ -= block_size;
        --num_allocations_;
    }

    void FreeListAllocator::coalescence(FreeBlock* previous, FreeBlock* current)
    {
        // Merge with next block if adjacent
        if (current->next)
        {
            std::size_t current_end = reinterpret_cast<std::size_t>(current) + current->size;
            std::size_t next_start = reinterpret_cast<std::size_t>(current->next);

            if (current_end == next_start)
            {
                current->size += current->next->size;
                current->next = current->next->next;
            }
        }

        // Merge with previous block if adjacent
        if (previous)
        {
            std::size_t prev_end = reinterpret_cast<std::size_t>(previous) + previous->size;
            std::size_t current_start = reinterpret_cast<std::size_t>(current);

            if (prev_end == current_start)
            {
                previous->size += current->size;
                previous->next = current->next;
            }
        }
    }

    std::size_t FreeListAllocator::align_forward_with_header(
        std::size_t address,
        std::size_t alignment,
        std::size_t header_size,
        std::size_t& adjustment
    ) const noexcept
    {
        assert((alignment & (alignment - 1)) == 0 && "Alignment must be power of 2");

        std::size_t aligned_address = address + header_size;

        std::size_t modulo = aligned_address & (alignment - 1);
        if (modulo != 0)
        {
            aligned_address += alignment - modulo;
        }

        adjustment = aligned_address - address;

        return aligned_address;
    }
} // namespace fast_alloc
