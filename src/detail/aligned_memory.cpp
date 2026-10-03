#include "aligned_memory.h"

#include <cassert>
#include <limits>
#include <new>

#ifdef _WIN32
#include <malloc.h>
#else
#include <cstdlib>
#endif

namespace fast_alloc::detail
{
    void AlignedMemoryDeleter::operator()(void* memory) const noexcept
    {
#ifdef _WIN32
        _aligned_free(memory);
#else
        std::free(memory);
#endif
    }

    std::size_t round_up_to_alignment(const std::size_t size, const std::size_t alignment)
    {
        assert(alignment != 0 && (alignment & (alignment - 1)) == 0
            && "Alignment must be a non-zero power of two");

        const std::size_t padding = (alignment - size % alignment) % alignment;
        if (padding > std::numeric_limits<std::size_t>::max() - size)
        {
            throw std::bad_alloc();
        }
        return size + padding;
    }

    AlignedMemory allocate_aligned_memory(const std::size_t size, const std::size_t alignment)
    {
        assert(size > 0 && "Backing memory size must be greater than zero");
        const std::size_t backing_size = round_up_to_alignment(size, alignment);

#ifdef _WIN32
        AlignedMemory memory(_aligned_malloc(backing_size, alignment));
#else
        AlignedMemory memory(std::aligned_alloc(alignment, backing_size));
#endif
        if (!memory)
        {
            throw std::bad_alloc();
        }
        return memory;
    }
} // namespace fast_alloc::detail
