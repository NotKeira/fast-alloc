#pragma once

#include <cstddef>
#include <memory>

namespace fast_alloc::detail
{
    struct AlignedMemoryDeleter
    {
        void operator()(void* memory) const noexcept;
    };

    using AlignedMemory = std::unique_ptr<void, AlignedMemoryDeleter>;

    // Alignment must be a non-zero power of two. Throws std::bad_alloc on overflow.
    [[nodiscard]] std::size_t round_up_to_alignment(std::size_t size, std::size_t alignment);

    // Returns owned backing memory, or throws std::bad_alloc on overflow or allocation failure.
    [[nodiscard]] AlignedMemory allocate_aligned_memory(std::size_t size, std::size_t alignment);
} // namespace fast_alloc::detail
