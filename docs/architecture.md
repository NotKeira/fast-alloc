# Architecture and Implementation

This document explains the design decisions, implementation details, and trade-offs of each allocator in fast-alloc.

## Table of Contents

- [Design Philosophy](#design-philosophy)
- [Pool Allocator](#pool-allocator)
- [Stack Allocator](#stack-allocator)
- [Free List Allocator](#free-list-allocator)
- [Performance Analysis](#performance-analysis)
- [Trade-offs](#trade-offs)

## Design Philosophy

Standard allocation (`malloc`, `new`) is designed for general-purpose use, which introduces overhead that's unnecessary
for specific patterns common in game development:

1. **Thread synchronisation** - Global heap requires locks
2. **Metadata overhead** - Each allocation stores size and bookkeeping data
3. **Fragmentation** - Variable-size allocations create memory gaps
4. **Cache misses** - Allocations scattered across memory
5. **Non-deterministic timing** - Allocation time varies unpredictably

Custom allocators exploit domain-specific knowledge to eliminate these costs.

## Pool Allocator

### Use Case

Allocating many objects of the **same size** that are allocated and deallocated frequently:

- Game entities (enemies, bullets, pickups)
- Particle systems
- Audio voices
- Network packets

### Memory Layout

```
Memory Pool (contiguous allocation):
┌─────────────────────────────────────────────────────┐
│ Block 0 │ Block 1 │ Block 2 │ ... │ Block N-1      │
└─────────────────────────────────────────────────────┘
  64 bytes  64 bytes  64 bytes        64 bytes

Free List (intrusive linked list):
Block 0 → Block 1 → Block 2 → ... → nullptr
```

### Implementation Details

**Initialisation:**

1. Allocate one large contiguous block: `block_size * block_count`
2. Treat each block as a node in a linked list
3. Store "next" pointer in the first bytes of each free block
4. No separate metadata needed - uses the free space itself

**Allocation (O(1)):**

```cpp
void* allocate() {
    void* block = free_list_;           // Get head of free list
    free_list_ = *(void**)free_list_;   // Move to next
    return block;
}
```

**Deallocation (O(1)):**

```cpp
void deallocate(void* ptr) {
    *(void**)ptr = free_list_;  // Point block to current head
    free_list_ = ptr;            // Make block new head
}
```

### Performance Characteristics

- **Allocation**: O(1) - single pointer dereference
- **Deallocation**: O(1) - two pointer assignments
- **Memory overhead**: 0 bytes per allocation (uses free space for list)
- **Fragmentation**: None (all blocks same size)
- **Cache performance**: Excellent (contiguous memory)

**Benchmark Results:**

- 4.5ns per allocation vs 33ns for `new` (7.3x faster)
- 220M allocations/sec throughput

### Limitations

- Fixed block size only
- Must know maximum capacity upfront
- Cannot grow dynamically
- Allocations fail when pool exhausted

## Stack Allocator

### Use Case

**Temporary allocations** with last-in-first-out (LIFO) lifetime:

- Per-frame scratch memory
- Function-local temporary buffers
- Rendering command lists
- String formatting

### Memory Layout

```
Stack Memory (grows upward):
┌─────────────────────────────────────────────────┐
│ Alloc 1 │ Alloc 2 │ Alloc 3 │      Free        │
└─────────────────────────────────────────────────┘
          ↑
       current

After reset():
┌─────────────────────────────────────────────────┐
│                    Free                         │
└─────────────────────────────────────────────────┘
↑
current (back to start)
```

### Implementation Details

**Allocation with Alignment:**

```cpp
void* allocate(size_t size, size_t alignment) {
    uintptr_t current_addr = (uintptr_t)current_;
    
    // Align forward to meet alignment requirement
    uintptr_t aligned_addr = (current_addr + alignment - 1) & ~(alignment - 1);
    
    // Move current pointer forward
    current_ = (void*)(aligned_addr + size);
    
    return (void*)aligned_addr;
}
```

**Reset (O(1)):**

```cpp
void reset() {
    current_ = memory_;  // Just reset pointer to start
}
```

**Marker-Based Reset:**

```cpp
void* marker = stack.get_marker();  // Save position
// ... allocations ...
stack.reset(marker);                // Rewind to marker
```

### Performance Characteristics

- **Allocation**: O(1) - pointer arithmetic only
- **Reset**: O(1) - single pointer assignment
- **Memory overhead**: 0 bytes per allocation
- **Fragmentation**: None
- **Cache performance**: Excellent (linear access pattern)

**Benchmark Results:**

- 6.7ns per allocation vs 31ns for `malloc` (4.6x faster)
- Frame pattern (1000 allocs): 5.8μs vs 38μs (6.5x faster)

### Limitations

- Must free in reverse order (or reset all)
- No individual deallocation
- Must estimate maximum size
- Memory wasted if overallocated

### Alignment Handling

Alignment is critical for SIMD operations and hardware requirements:

```
Unaligned (wastes space):
┌────────────────────────────────┐
│ X │ Padding │    Data (16B)    │
└────────────────────────────────┘

Aligned to 16 bytes:
┌────────────────────────────────┐
│    Data (16B)    │ Next alloc  │
└────────────────────────────────┘
```

## Free List Allocator

### Use Case

**Variable-size allocations** that need individual deallocation:

- Game assets (textures, meshes, audio)
- Dynamic strings
- Script objects
- UI elements

### Memory Layout

```
Initial state (one large free block):
┌──────────────────────────────────────┐
│         Free Block (4096 bytes)      │
│  size=4096, next=nullptr             │
└──────────────────────────────────────┘

After allocations:
┌─────────┬──────────┬─────────┬───────────┐
│ Alloc 1 │ Free(50) │ Alloc 2 │ Free(980) │
│  100B   │  50B     │  150B   │   980B    │
└─────────┴──────────┴─────────┴───────────┘
            ↓                      ↓
         Free list: Block1 → Block2 → nullptr
```

### Implementation Details

**Allocation Header:**

```cpp
struct AllocationHeader {
    size_t size;        // Total allocation size (including header)
    size_t adjustment;  // Bytes added for alignment
};

Memory layout:
[Header][Padding for alignment][User Data]
         ↑
         Returned pointer (aligned)
```

**First-Fit Strategy:**

- Traverse free list from start
- Use first block large enough
- Fast but can cause fragmentation

**Best-Fit Strategy:**

- Traverse entire free list
- Use smallest block that fits
- Slower but reduces fragmentation

**Block Splitting:**

```cpp
if (block->size - required_size > sizeof(FreeBlock)) {
    // Split: create new free block from remainder
    FreeBlock* new_block = (FreeBlock*)(block_addr + required_size);
    new_block->size = block->size - required_size;
    new_block->next = block->next;
    // Insert new block into free list
}
```

**Coalescence (Defragmentation):**
When deallocating, merge adjacent free blocks:

```
Before deallocation:
[Used A][Free 1][Used B][Free 2]

After deallocating B:
[Used A][Free 1][Free (B)][Free 2]

After coalescence:
[Used A][      Free (merged)     ]
```

Implementation:

```cpp
void coalescence(FreeBlock* prev, FreeBlock* current) {
    // Merge with next if adjacent
    if (current + current->size == current->next) {
        current->size += current->next->size;
        current->next = current->next->next;
    }
    
    // Merge with previous if adjacent
    if (prev + prev->size == current) {
        prev->size += current->size;
        prev->next = current->next;
    }
}
```

### Performance Characteristics

- **Allocation**: O(n) worst case (traverse free list)
    - First-fit: O(n) average, stops at first match
    - Best-fit: O(n) always, must check all blocks
- **Deallocation**: O(n) worst case (find insertion point + coalescence)
- **Memory overhead**: 16 bytes per allocation (header)
- **Fragmentation**: Mitigated by coalescence
- **Cache performance**: Poor (scattered allocations)

**Benchmark Results:**

- First-fit: 15ns vs 31ns for `malloc` (2.1x faster)
- Best-fit: 16ns vs 31ns for `malloc` (1.9x faster)

### Limitations

- Slower than Pool/Stack (must search free list)
- External fragmentation possible
- More memory overhead (header per allocation)
- Not real-time safe (non-deterministic timing)

## Performance Analysis

### Why Are Custom Allocators Faster?

**1. Reduced Indirection**

- `malloc`: Global heap → lock → search → allocate
- Custom: Direct memory pool access

**2. No Thread Synchronisation**

- `malloc`: Must lock for thread safety
- Custom: Single-threaded or lock-free design

**3. Better Cache Locality**

- `malloc`: Allocations scattered across heap
- Pool/Stack: Contiguous memory, sequential access

**4. Zero Metadata (Pool/Stack)**

- `malloc`: 16-32 bytes overhead per allocation
- Pool: 0 bytes (uses free space)
- Stack: 0 bytes (just bump pointer)

**5. Predictable Performance**

- `malloc`: O(n) worst case, varies by heap state
- Pool: O(1) always
- Stack: O(1) always

### Memory Access Patterns

```
malloc pattern (poor cache locality):
Heap: [X] ... [Y] ... [Z] ... [W]
      ↑ miss  ↑ miss  ↑ miss  ↑ miss

Pool pattern (excellent cache locality):
Pool: [A][B][C][D][E][F][G][H]
      ↑ miss ↑ hit ↑ hit ↑ hit
```

### Benchmark Methodology

All benchmarks run on:

- Windows 10/11
- MSVC 2019/2022
- Release build with optimisations (`/O2`)
- Google Benchmark framework
- Multiple iterations to reduce variance

## Trade-offs

### When to Use Each Allocator

| Allocator     | Best For                                | Avoid If                         |
|---------------|-----------------------------------------|----------------------------------|
| **Pool**      | Same-size objects, frequent alloc/free  | Variable sizes, unbounded growth |
| **Stack**     | Temporary per-frame data, LIFO lifetime | Need individual deallocation     |
| **Free List** | Variable sizes, individual deallocation | Need deterministic timing        |
| **malloc**    | Prototyping, unknown patterns           | Performance critical             |

### Memory vs Speed Trade-offs

**Pool Allocator:**

- ✅ Fastest allocation/deallocation
- ✅ Zero fragmentation
- ❌ Must reserve maximum capacity upfront
- ❌ Wasted memory if underutilised

**Stack Allocator:**

- ✅ Near-zero cost allocation
- ✅ Perfect for frame-based patterns
- ❌ Limited deallocation flexibility
- ❌ Must estimate maximum size

**Free List Allocator:**

- ✅ Flexible for variable sizes
- ✅ Individual deallocation
- ❌ Slower than Pool/Stack
- ❌ External fragmentation possible

### Production Considerations

**Thread Safety:**
Current implementations are single-threaded. For multi-threaded use:

- Add mutex locks (simplest, adds overhead)
- Use lock-free atomics (complex, high performance)
- Per-thread allocators (best performance, more memory)

**Debug Features:**
Production allocators should add:

- Bounds checking (guard bytes)
- Leak detection
- Use-after-free detection
- Memory fill patterns (0xCD, 0xDD)

**Monitoring:**
Track metrics for tuning:

- Peak memory usage
- Allocation failures
- Fragmentation percentage
- Average allocation time

## Further Optimisations

### SIMD Optimisation

Use SSE/AVX for bulk operations:

```cpp
// Zero memory with SIMD
void zero_memory_simd(void* ptr, size_t size) {
    __m256i zero = _mm256_setzero_si256();
    for (size_t i = 0; i < size; i += 32) {
        _mm256_store_si256((__m256i*)(ptr + i), zero);
    }
}
```

### Hardware Prefetching

Hint CPU to prefetch next allocation:

```cpp
void* allocate() {
    void* block = free_list_;
    _mm_prefetch(*(char**)free_list_, _MM_HINT_T0);
    free_list_ = *(void**)free_list_;
    return block;
}
```

### Virtual Memory

Use OS virtual memory for large pools:

```cpp
// Reserve address space, commit pages as needed
void* reserve = VirtualAlloc(nullptr, size, MEM_RESERVE, PAGE_READWRITE);
void* commit = VirtualAlloc(reserve, commit_size, MEM_COMMIT, PAGE_READWRITE);
```

## References

- Game Engine Architecture (Jason Gregory)
- Memory Management in C++ (Niklaus Wirth)
- Effective Modern C++ (Scott Meyers)
- [CppCon: Allocator Design](https://www.youtube.com/watch?v=LIb3L4vKZ7U)