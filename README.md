# fast-alloc

![CI](https://github.com/NotKeira/fast-alloc/workflows/CI/badge.svg)

High-performance custom memory allocators with comprehensive benchmarks for game development and real-time applications.

## Allocators

- **Pool Allocator**: Fixed-size block allocation for homogeneous objects (particles, game entities)
- **Thread-Safe Pool Allocator**: Mutex-protected pool allocator for concurrent access
- **Stack Allocator**: Linear allocator with frame-based reset for temporary allocations
- **Free List Allocator**: General-purpose allocator with first-fit and best-fit strategies

## Performance

Benchmarks across multiple platforms demonstrate significant performance improvements over standard allocation:

### Linux (GCC Release)

| Allocator                 | Operation                   | Time   | vs Standard         | Speedup          |
|---------------------------|-----------------------------|--------|---------------------|------------------|
| **Pool**                  | Single allocation           | 1.97ns | 7.07ns (new/delete) | **3.6x faster**  |
| **Stack**                 | Single allocation           | 2.20ns | 5.16ns (malloc)     | **2.3x faster**  |
| **Stack**                 | Frame pattern (1000 allocs) | 1203ns | 12715ns (malloc)    | **10.6x faster** |
| **Free List (First-Fit)** | Single allocation           | 4.99ns | 4.85ns (malloc)     | **Similar**      |
| **Free List (Best-Fit)**  | Single allocation           | 5.80ns | 4.85ns (malloc)     | **Similar**      |

### macOS (Apple Clang Release)

| Allocator                 | Operation                   | Time   | vs Standard         | Speedup         |
|---------------------------|-----------------------------|--------|---------------------|-----------------|
| **Pool**                  | Single allocation           | 4.38ns | 19.4ns (new/delete) | **4.4x faster** |
| **Stack**                 | Single allocation           | 2.37ns | 16.9ns (malloc)     | **7.1x faster** |
| **Stack**                 | Frame pattern (1000 allocs) | 3736ns | 20393ns (malloc)    | **5.5x faster** |
| **Free List (First-Fit)** | Single allocation           | 7.48ns | 16.5ns (malloc)     | **2.2x faster** |

### Windows (MSVC Release)

| Allocator                 | Operation                   | Time   | vs Standard         | Speedup          |
|---------------------------|-----------------------------|--------|---------------------|------------------|
| **Pool**                  | Single allocation           | 2.22ns | 36.9ns (new/delete) | **16.6x faster** |
| **Stack**                 | Single allocation           | 1.26ns | 36.2ns (malloc)     | **28.7x faster** |
| **Stack**                 | Frame pattern (1000 allocs) | 1269ns | 44627ns (malloc)    | **35.2x faster** |
| **Free List (First-Fit)** | Single allocation           | 5.25ns | 36.2ns (malloc)     | **6.9x faster**  |

### Key Results

- **Pool Allocator**: Up to 500M+ allocations/sec vs 50-150M with `new/delete`
- **Stack Allocator**: Up to 800M+ allocations/sec vs 25-200M with `malloc`
- **Windows shows most dramatic improvements** due to Windows heap overhead
- **Zero fragmentation** with Pool and Stack allocators
- **Automatic coalescence** reduces fragmentation in Free List allocator
- **Thread-safe variant** available for concurrent workloads

## Building

Requires C++20 compiler and CMake 3.14+.

### Windows (MSVC)

```bash
cmake -B build
cmake --build build --config Release
.\build\Release\alloc_benchmarks.exe
ctest --test-dir build -C Release
```

### Linux/macOS

```bash
cmake -B build -DCMAKE_BUILD_TYPE=Release
cmake --build build
./build/alloc_benchmarks
ctest --test-dir build --output-on-failure
```

### With Sanitizers (Linux/macOS)

```bash
# Address Sanitizer
cmake -B build -DCMAKE_BUILD_TYPE=Debug \
  -DCMAKE_CXX_FLAGS="-fsanitize=address -fno-omit-frame-pointer -g"
cmake --build build
ctest --test-dir build --output-on-failure

# Thread Sanitizer
cmake -B build -DCMAKE_BUILD_TYPE=Debug \
  -DCMAKE_CXX_FLAGS="-fsanitize=thread -g"
cmake --build build
TSAN_OPTIONS="second_deadlock_stack=1 suppressions=tsan.supp" \
  ctest --test-dir build --output-on-failure
```

## Project Structure

```
fast-alloc/
├── src/
│   ├── pool_allocator.h/cpp              - Fixed-size block allocator
│   ├── threadsafe_pool_allocator.h/cpp   - Thread-safe pool allocator
│   ├── stack_allocator.h/cpp             - Linear allocator with reset
│   └── freelist_allocator.h/cpp          - General-purpose with coalescence
├── benchmarks/
│   └── Comprehensive performance benchmarks vs malloc/new
├── tests/
│   ├── Unit tests with Catch2
│   └── Concurrent stress tests for thread-safe allocators
├── .github/workflows/
│   └── CI with sanitizers (ASan, TSan, UBSan)
└── tsan.supp
    └── ThreadSanitizer suppressions for Catch2
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

### Thread-Safe Pool Allocator

```cpp
#include "threadsafe_pool_allocator.h"
#include <thread>

fast_alloc::ThreadSafePoolAllocator pool(64, 1000);

void worker_thread() {
    void* obj = pool.allocate();
    // Thread-safe allocation
    pool.deallocate(obj);
}

std::thread t1(worker_thread);
std::thread t2(worker_thread);
t1.join();
t2.join();
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

## Testing

Comprehensive test suite with Catch2:

- Unit tests for all allocators
- Move semantics validation
- Edge case handling (nullptr, overflow, alignment)
- Concurrent stress tests for thread-safe variants
- CI with multiple sanitizers (ASan, TSan, UBSan)

## Licence

MIT Licence - see [LICENSE](LICENSE) for details.

## Author

Keira Hopkins