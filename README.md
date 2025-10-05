# fast-alloc

High-performance custom memory allocators with comprehensive benchmarks for game development and real-time applications.

## Allocators

- **Pool Allocator**: Fixed-size block allocation for homogeneous objects (particles, game entities)
- **Stack Allocator**: Linear allocator with frame-based reset for temporary allocations
- **Free List Allocator**: General-purpose allocator with first-fit and best-fit strategies

## Performance

Benchmarks demonstrate significant performance improvements over standard allocation in common game development
patterns:

- Pool allocation: 10-50x faster than `new` for same-sized objects
- Stack allocation: Near-zero cost for temporary per-frame data
- Free list: Reduced fragmentation and overhead vs `malloc`

## Building

Requires C++20 compiler and CMake 3.20+.

```bash
cmake -B build
cmake --build build
cd build/benchmarks
./alloc_benchmarks
```

## Project Structure

```
fast-alloc/
├── src/
│   ├── pool_allocator.hpp/cpp
│   ├── stack_allocator.hpp/cpp
│   └── freelist_allocator.hpp/cpp
├── benchmarks/
│   └── benchmark suite comparing against malloc/new
├── tests/
│   └── unit tests for correctness
└── docs/
    └── performance analysis and graphs
```

## Licence

MIT Licence - see [LICENSE](LICENSE) for details.