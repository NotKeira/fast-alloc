# fast-alloc

![CI](https://github.com/NotKeira/fast-alloc/workflows/CI/badge.svg)

Custom C++20 memory allocators for game development, with unit tests and
benchmarks against `malloc/free` and `new/delete`.

## Allocators

- **Pool Allocator**: Fixed-size block allocation for homogeneous objects (particles, game entities)
- **Thread-Safe Pool Allocator**: Mutex-protected pool allocator for concurrent access
- **Stack Allocator**: Linear allocator with frame-based reset for temporary allocations
- **Free List Allocator**: General-purpose allocator with first-fit and best-fit strategies

Stack and free-list capacities do not need to be multiples of alignment. Stack
capacity must be positive; free-list capacity must exceed the size of its internal
free-block metadata. Backing memory is rounded internally for alignment, while
the reported capacity stays as requested. Stack usage includes alignment padding;
free-list usage also includes allocation headers and unsplit remainders.

All allocators own their backing memory through RAII. Constructors throw
`std::bad_alloc` if the backing size calculation overflows or allocation fails.
Allocation requests return `nullptr` when no suitable space remains; failed stack
and free-list requests leave the allocator unchanged.

Only `ThreadSafePoolAllocator` synchronises allocation and deallocation. The other
allocators require external synchronisation when shared between threads.
`PoolAllocator`, `StackAllocator` and `FreeListAllocator` support moves; none of the
allocators support copying, and the thread-safe pool cannot be moved.

## Performance

Results measured on 3 October 2026 at
[3164ed7](https://github.com/NotKeira/fast-alloc/commit/3164ed7a1f545c4b64d949e1d84e539bb10f2943),
using Release builds and Google Benchmark 1.9.1. Times are CPU time per benchmark
iteration; allocation workloads use 64-byte blocks. Speedup is the standard
allocator's time divided by the custom allocator's time, so values below 1 mean slower.
Results depend on the machine and workload.

Allocate/free rows include both operations; stack rows include allocation and
reset. Frame timings cover 1,000 allocations followed by one stack reset or 1,000
calls to `free`. The `malloc` frame benchmark also allocates and releases a pointer
array. Custom allocators reserve their backing storage before the timed loops.
Thread-safe pool results use one thread and include mutex locking.

### Linux (GCC Release)

Debian 13, AMD Ryzen 7 PRO 8700GE, GCC 14.2.0. Median of seven repetitions with a
minimum run time of 0.5 seconds and a 0.1-second warm-up, pinned to CPU 2 with
frequency scaling enabled. Benchmark order was randomly interleaved.

| Allocator                 | Operation                  | Time     | Standard                   | Speedup |
|---------------------------|----------------------------|----------|----------------------------|---------|
| **Pool**                  | Allocate/free              | 2.00 ns  | 6.03 ns (`new/delete`)      | 3.0x    |
| **Thread-Safe Pool**       | Allocate/free (one thread)  | 9.98 ns  | 6.03 ns (`new/delete`)      | 0.60x   |
| **Stack**                 | Allocate/reset             | 1.99 ns  | 4.77 ns (`malloc/free`)     | 2.4x    |
| **Stack**                 | Frame (1,000 allocations)  | 1205 ns  | 14385 ns (`malloc/free`)    | 11.9x   |
| **Free List (First-Fit)**  | Allocate/free              | 4.66 ns  | 4.67 ns (`malloc/free`)     | 1.0x    |
| **Free List (Best-Fit)**   | Allocate/free              | 6.18 ns  | 4.67 ns (`malloc/free`)     | 0.76x   |

### macOS (Apple Clang Release)

GitHub-hosted `macos-26-arm64` runner, Apple Clang 21.0.0.21000101. One CI
repetition with a minimum run time of 0.1 seconds;
[benchmark log](https://github.com/NotKeira/fast-alloc/actions/runs/37083097328/job/111087671247).

| Allocator                 | Operation                  | Time     | Standard                   | Speedup |
|---------------------------|----------------------------|----------|----------------------------|---------|
| **Pool**                  | Allocate/free              | 4.60 ns  | 18.5 ns (`new/delete`)      | 4.0x    |
| **Thread-Safe Pool**       | Allocate/free (one thread)  | 21.2 ns  | 18.5 ns (`new/delete`)      | 0.87x   |
| **Stack**                 | Allocate/reset             | 3.37 ns  | 16.0 ns (`malloc/free`)     | 4.7x    |
| **Stack**                 | Frame (1,000 allocations)  | 2395 ns  | 16863 ns (`malloc/free`)    | 7.0x    |
| **Free List (First-Fit)**  | Allocate/free              | 7.22 ns  | 15.9 ns (`malloc/free`)     | 2.2x    |
| **Free List (Best-Fit)**   | Allocate/free              | 9.51 ns  | 15.9 ns (`malloc/free`)     | 1.7x    |

### Windows (MSVC Release)

GitHub-hosted Windows Server 2025 runner, MSVC 19.51.36260.0. One CI repetition
with a minimum run time of 0.1 seconds;
[benchmark log](https://github.com/NotKeira/fast-alloc/actions/runs/37083097328/job/111087671528).

| Allocator                 | Operation                  | Time     | Standard                   | Speedup |
|---------------------------|----------------------------|----------|----------------------------|---------|
| **Pool**                  | Allocate/free              | 2.34 ns  | 34.9 ns (`new/delete`)      | 14.9x   |
| **Thread-Safe Pool**       | Allocate/free (one thread)  | 21.0 ns  | 34.9 ns (`new/delete`)      | 1.7x    |
| **Stack**                 | Allocate/reset             | 1.74 ns  | 34.9 ns (`malloc/free`)     | 20.1x   |
| **Stack**                 | Frame (1,000 allocations)  | 1946 ns  | 46712 ns (`malloc/free`)    | 24.0x   |
| **Free List (First-Fit)**  | Allocate/free              | 6.23 ns  | 41.9 ns (`malloc/free`)     | 6.7x    |
| **Free List (Best-Fit)**   | Allocate/free              | 6.28 ns  | 41.9 ns (`malloc/free`)     | 6.7x    |

### Key Results

- **Pool**: 3.0-14.9x faster than `new/delete` for allocation/free pairs in these runs.
- **Stack**: 2.4-20.1x faster for allocation/reset, and 7.0-24.0x faster for the
  1,000-allocation frame workload.
- **Free List**: First-fit matches the Linux `malloc/free` baseline; best-fit is
  slower there. Both strategies are faster than their macOS and Windows baselines.
- **Thread-Safe Pool**: Single-thread timings include the cost of synchronisation;
  these measurements do not establish performance under contention.

To reproduce the Linux measurements after building in Release mode:

```bash
taskset -c 2 ./build/alloc_benchmarks \
  --benchmark_filter='^(BM_.*_Allocate|BM_StackAllocator_FramePattern/1000|BM_Malloc_FramePattern/1000|BM_FreeListAllocator_(FirstFit|BestFit)|BM_Malloc_Compare|BM_ThreadSafePoolAllocator_SingleThread)$' \
  --benchmark_min_time=0.5s --benchmark_min_warmup_time=0.1 \
  --benchmark_repetitions=7 --benchmark_enable_random_interleaving=true \
  --benchmark_report_aggregates_only=true
```

## Building

Requires a C++20 compiler, CMake 3.20 or newer, Git and a build tool such as Make,
Ninja or MSBuild.

Tests and benchmarks are enabled by default. CMake fetches Catch2 3.5.0 for tests
and Google Benchmark 1.9.1 for benchmarks, so the initial configuration needs
access to GitHub. The library target is `fast_alloc`; the executable targets are
`alloc_tests` and `alloc_benchmarks`.

Non-MSVC Release builds use `-O3 -march=native`, so their generated code depends
on the build machine's CPU.

### Windows (MSVC, Visual Studio)

```powershell
cmake -S . -B build -DCMAKE_BUILD_TYPE=Release
cmake --build build --config Release
.\build\Release\alloc_benchmarks.exe
ctest --test-dir build -C Release --output-on-failure
```

### Linux/macOS

```bash
cmake -S . -B build -DCMAKE_BUILD_TYPE=Release
cmake --build build
./build/alloc_benchmarks
ctest --test-dir build --output-on-failure
```

### Library Only

Disable both optional targets to build the static library without downloading
test or benchmark dependencies:

```bash
cmake -S . -B build/library -DCMAKE_BUILD_TYPE=Release \
  -DBUILD_TESTS=OFF -DBUILD_BENCHMARKS=OFF
cmake --build build/library --config Release
```

### With Sanitisers (Linux, Clang)

CI runs AddressSanitizer, ThreadSanitizer and UndefinedBehaviorSanitizer separately
with Clang on Linux. Use a separate build directory for each configuration:

```bash
# AddressSanitizer, including leak detection
CC=clang CXX=clang++ cmake -S . -B build/asan -DCMAKE_BUILD_TYPE=Debug \
  -DBUILD_BENCHMARKS=OFF \
  -DCMAKE_CXX_FLAGS="-fsanitize=address -fno-omit-frame-pointer -g" \
  -DCMAKE_EXE_LINKER_FLAGS="-fsanitize=address"
cmake --build build/asan
ASAN_OPTIONS=detect_leaks=1 ctest --test-dir build/asan --output-on-failure

# ThreadSanitizer
CC=clang CXX=clang++ cmake -S . -B build/tsan -DCMAKE_BUILD_TYPE=Debug \
  -DBUILD_BENCHMARKS=OFF \
  -DCMAKE_CXX_FLAGS="-fsanitize=thread -fno-omit-frame-pointer -g" \
  -DCMAKE_EXE_LINKER_FLAGS="-fsanitize=thread"
cmake --build build/tsan
TSAN_OPTIONS="second_deadlock_stack=1 suppressions=$PWD/tsan.supp" \
  ctest --test-dir build/tsan --output-on-failure

# UndefinedBehaviorSanitizer
CC=clang CXX=clang++ cmake -S . -B build/ubsan -DCMAKE_BUILD_TYPE=Debug \
  -DBUILD_BENCHMARKS=OFF \
  -DCMAKE_CXX_FLAGS="-fsanitize=undefined -fno-omit-frame-pointer -g" \
  -DCMAKE_EXE_LINKER_FLAGS="-fsanitize=undefined"
cmake --build build/ubsan
ctest --test-dir build/ubsan --output-on-failure
```

## Project Structure

```
fast-alloc/
├── .github/
│   └── workflows/
│       └── ci.yaml                  # Builds, tests, benchmarks and sanitisers
├── .idea/                          # JetBrains project settings
├── benchmarks/                     # Google Benchmark comparisons
│   ├── benchmark_main.cpp
│   ├── bench_freelist.cpp
│   ├── bench_pool.cpp
│   ├── bench_stack.cpp
│   └── bench_threadsafe_pool.cpp
├── docs/
│   ├── architecture.md
│   └── USAGE.md
├── src/
│   ├── detail/                     # Shared aligned-memory ownership
│   │   ├── aligned_memory.cpp
│   │   └── aligned_memory.h
│   ├── freelist_allocator.cpp
│   ├── freelist_allocator.h
│   ├── pool_allocator.cpp
│   ├── pool_allocator.h
│   ├── stack_allocator.cpp
│   ├── stack_allocator.h
│   ├── threadsafe_pool_allocator.cpp
│   └── threadsafe_pool_allocator.h
├── tests/                          # Catch2 unit and concurrency tests
│   ├── test_freelist.cpp
│   ├── test_main.cpp
│   ├── test_pool.cpp
│   ├── test_stack.cpp
│   └── test_threadsafe_pool.cpp
├── .editorconfig
├── .gitignore
├── CMakeLists.txt                  # Library, test and benchmark targets
├── CONTRIBUTING.md
├── LICENSE
├── README.md
└── tsan.supp                       # ThreadSanitizer suppressions for Catch2
```

## Usage Examples

For comprehensive usage examples and patterns, see [docs/USAGE.md](docs/USAGE.md).

These allocators return raw storage. Construct and destroy C++ objects separately;
returning a block or resetting the stack does not run object destructors. Return
each live block to its owning allocator at most once. Pool and free-list
`deallocate(nullptr)` calls are safely ignored.

Invalid constructor sizes, unknown free-list strategies, zero-byte free-list
requests and alignments that are zero or not powers of two throw
`std::invalid_argument` in both Debug and Release builds. Stack zero-byte requests
remain supported. Pointer ownership and stack marker validity remain caller
responsibilities, with the existing Debug assertions retained.

### Quick Start

#### Pool Allocator

```cpp
#include "pool_allocator.h"

int main()
{
    fast_alloc::PoolAllocator pool(64, 1000);
    if (void* obj = pool.allocate())
    {
        // Use the 64-byte block...
        pool.deallocate(obj);
    }
}
```

Both pool allocators require `block_size >= sizeof(void*)` and a positive block
count. They align every block to `alignof(std::max_align_t)` by default,
adding trailing padding where needed. An optional third constructor argument
sets the alignment for types with stricter requirements, such as
`PoolAllocator(sizeof(T), 1000, alignof(T))`. `block_size()` reports the requested
size; `block_stride()` includes padding, and `alignment()` reports the guaranteed
block alignment.

The requested pool alignment must be a non-zero power of two; invalid values
throw `std::invalid_argument`. Values below `alignof(void*)` are raised to that
minimum.

#### Thread-Safe Pool Allocator

```cpp
#include "threadsafe_pool_allocator.h"
#include <thread>

int main()
{
    fast_alloc::ThreadSafePoolAllocator pool(64, 1000);
    auto worker = [&pool]
    {
        if (void* obj = pool.allocate())
        {
            // Use the block within this thread...
            pool.deallocate(obj);
        }
    };

    std::thread t1(worker);
    std::thread t2(worker);
    t1.join();
    t2.join();
}
```

#### Stack Allocator

```cpp
#include "stack_allocator.h"
#include <cstring>

int main()
{
    fast_alloc::StackAllocator stack(1024 * 1024); // 1 MiB
    if (void* temp = stack.allocate(256))
    {
        std::memset(temp, 0, 256);
        // Use temporary data for this frame...
    }

    stack.reset(); // Discard all allocations at frame end
}
```

`reset()` discards all allocations; `reset(marker)` rewinds to a marker returned
by `get_marker()`. Discarded allocations must no longer be accessed. Individual
blocks cannot be freed. `allocate(size, alignment)` defaults to
`alignof(std::max_align_t)`.

#### Free List Allocator

```cpp
#include "freelist_allocator.h"

int main()
{
    fast_alloc::FreeListAllocator allocator(
        1024 * 1024,
        fast_alloc::FreeListStrategy::FirstFit
    );

    if (void* data = allocator.allocate(128))
    {
        // Use the block...
        allocator.deallocate(data);
    }
}
```

`FirstFit` is the default strategy; `BestFit` searches for the smallest suitable
free block. `allocate(size, alignment)` defaults to `alignof(std::max_align_t)`.
Deallocation coalesces adjacent free blocks. `available()` reports total unused
bytes, so it does not guarantee that a request of that size will fit: headers,
alignment and fragmentation affect the space needed.

## Why Custom Allocators?

These allocators reserve backing storage at construction and reuse it for later
requests. Choose one that matches the allocation lifetime and size requirements:

- **Pool**: Reuse fixed-size blocks with O(1) allocation and deallocation.
- **Stack**: Allocate temporary data with pointer bumping and discard it in bulk
  or rewind to a marker.
- **Free List**: Manage variable-size blocks with individual frees and automatic
  coalescence; allocation and deallocation search the free list in O(n) time.
- **Thread-Safe Pool**: Share fixed-size block allocation between threads through
  a mutex. Lock waiting can affect latency.

Capacities are fixed after construction. Pool blocks can contain alignment
padding; stack allocations can require padding; free-list blocks also need
metadata and can fragment. Benchmark representative application workloads when
choosing an allocator.

## Testing

The Catch2 test suite covers:

- Unit tests for all allocators
- Move construction and assignment for movable allocators
- Exhaustion, nullptr handling and arithmetic overflow
- Invalid constructor parameters and allocation requests in Debug and Release
- Default and explicit alignment, including odd-sized pool blocks
- Arbitrary byte capacities and free-list split/coalescence accounting
- Concurrent stress tests for thread-safe variants

CI builds and tests Debug and Release configurations on Windows (MSVC), Linux
(GCC and Clang) and macOS (Apple Clang). Separate Linux jobs run ASan with leak
detection, TSan with the Catch2 suppressions in `tsan.supp`, and UBSan.

## Contributing

Contributions are welcome! Please see [CONTRIBUTING.md](CONTRIBUTING.md) for guidelines.

## Licence

MIT Licence - see [LICENSE](LICENSE) for details.

## Author

Keira Hopkins

GitHub Copilot is credited as a contributor for four commits, from
[308f853](https://github.com/NotKeira/fast-alloc/commit/308f853e58021d14ac4c278d94fa5ba06abda953)
to [a48a4f1](https://github.com/NotKeira/fast-alloc/commit/a48a4f11be95e778951122c7a2cd1333fd9d6ef0),
inclusive. Copilot is no longer used in this project.
