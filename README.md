# fast-alloc

![CI](https://github.com/NotKeira/fast-alloc/workflows/CI/badge.svg)

High-performance custom memory allocators with comprehensive benchmarks for game development and real-time applications.
## Allocators

- **Pool Allocator**: Fixed-size block allocation for homogeneous objects (particles, game entities)
- **Stack Allocator**: Linear allocator with frame-based reset for temporary allocations
- **Free List Allocator**: General-purpose allocator with first-fit and best-fit strategies

## Performance

Benchmarks on Windows (MSVC, Release build) demonstrate significant performance improvements over standard allocation:

| Allocator                 | Operation                   | Time  | vs Standard           | Speedup         |
|---------------------------|-----------------------------|-------|-----------------------|-----------------|
| **Pool**                  | Single allocation           | 4.5ns | 33ns (new/delete)     | **7.3x faster** |
| **Stack**                 | Single allocation           | 6.7ns | 31ns (malloc/free)    | **4.6x faster** |
| **Stack**                 | Frame pattern (1000 allocs) | 5.8μs | 38μs (malloc pattern) | **6.5x faster** |
| **Free List (First-Fit)** | Single allocation           | 15ns  | 31ns (malloc)         | **2.1x faster** |
| **Free List (Best-Fit)**  | Single allocation           | 16ns  | 31ns (malloc)         | **1.9x faster** |

### Key Results

- **Pool Allocator**: 220M allocations/sec vs 30M with `new/delete`
- **Stack Allocator**: 152M allocations/sec vs 33M with `malloc`
- **Zero fragmentation** with Pool and Stack allocators
- **Automatic coalescence** reduces fragmentation in Free List allocator

## Building

Requires C++20 compiler and CMake 4.0+.

### Windows (MSVC)

```bash
cmake -B build
cmake --build build --config Release
.\build\Release\alloc_benchmarks.exe
.\build\Debug\alloc_tests.exe
```

### Linux/macOS

```bash
cmake -B build -DCMAKE_BUILD_TYPE=Release
cmake --build build
./build/alloc_benchmarks
./build/alloc_tests
```

## Project Structure

```
fast-alloc/
├── src/
│   ├── pool_allocator.h/cpp      - Fixed-size block allocator
│   ├── stack_allocator.h/cpp     - Linear allocator with reset
│   └── freelist_allocator.h/cpp  - General-purpose with coalescence
├── benchmarks/
│   └── Comprehensive performance benchmarks vs malloc/new
├── tests/
│   └── Unit tests with Catch2
└── docs/
    └── Performance analysis and implementation details
```

## Usage Examples

### Pool Allocator

```cpp
#include "pool_allocator.h"

// Create pool for 64-byte objects
fast_alloc::PoolAllocator pool(64, 1000);

void* obj = pool.allocate();
// Use object...
pool.deallocate(obj);
```

### Stack Allocator

```cpp
#include "stack_allocator.h"

fast_alloc::StackAllocator stack(1024 * 1024); // 1MB

void frame_update() {
    void* temp1 = stack.allocate(256);
    void* temp2 = stack.allocate(512);
    // Use temporary data...
    
    stack.reset(); // Free everything at frame end
}
```

### Free List Allocator

```cpp
#include "freelist_allocator.h"

fast_alloc::FreeListAllocator allocator(
    1024 * 1024, 
    fast_alloc::FreeListStrategy::FirstFit
);

void* data = allocator.allocate(128);
// Use data...
allocator.deallocate(data);
```

## Why Custom Allocators?

Standard `malloc`/`new` are general-purpose but slow for game development patterns:

- Thread synchronisation overhead
- Poor cache locality
- Unpredictable timing
- Memory fragmentation

Custom allocators exploit specific allocation patterns for dramatic performance improvements.

## Licence

MIT Licence - see [LICENSE](LICENSE) for details.

## Author

Keira Hopkins