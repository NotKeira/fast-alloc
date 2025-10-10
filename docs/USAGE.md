# Usage Guide

This guide provides comprehensive examples and best practices for using fast-alloc allocators.

## Table of Contents

- [Pool Allocator](#pool-allocator)
- [Thread-Safe Pool Allocator](#thread-safe-pool-allocator)
- [Stack Allocator](#stack-allocator)
- [Free List Allocator](#free-list-allocator)
- [Best Practices](#best-practices)
- [Common Patterns](#common-patterns)

## Pool Allocator

### Basic Usage

```cpp
#include "pool_allocator.h"

// Create pool for 64-byte blocks, 1000 capacity
fast_alloc::PoolAllocator pool(64, 1000);

// Allocate a block
void* block = pool.allocate();
if (block) {
    // Use block...
    pool.deallocate(block);
}
```

### Particle System Example

```cpp
struct Particle {
    float x, y, z;
    float vx, vy, vz;
    float lifetime;
    // ... other properties
};

class ParticleSystem {
    fast_alloc::PoolAllocator pool_;
    
public:
    ParticleSystem() : pool_(sizeof(Particle), 10000) {}
    
    Particle* spawn(float x, float y, float z) {
        void* mem = pool_.allocate();
        if (!mem) return nullptr; // Pool exhausted
        
        return new (mem) Particle{x, y, z, 0, 0, 0, 1.0f};
    }
    
    void destroy(Particle* p) {
        p->~Particle();
        pool_.deallocate(p);
    }
};
```

### Checking Pool Status

```cpp
fast_alloc::PoolAllocator pool(64, 100);

std::cout << "Block size: " << pool.block_size() << " bytes\n";
std::cout << "Capacity: " << pool.capacity() << " blocks\n";
std::cout << "Allocated: " << pool.allocated() << " blocks\n";
std::cout << "Full: " << (pool.is_full() ? "yes" : "no") << "\n";
```

## Thread-Safe Pool Allocator

### Multi-threaded Audio System

```cpp
#include "threadsafe_pool_allocator.h"
#include <thread>
#include <vector>

struct AudioVoice {
    float frequency;
    float amplitude;
    // ... other properties
};

class AudioEngine {
    fast_alloc::ThreadSafePoolAllocator pool_;
    
public:
    AudioEngine() : pool_(sizeof(AudioVoice), 256) {}
    
    // Called from multiple threads
    AudioVoice* allocate_voice() {
        void* mem = pool_.allocate();
        if (!mem) return nullptr;
        return new (mem) AudioVoice{};
    }
    
    void free_voice(AudioVoice* voice) {
        voice->~AudioVoice();
        pool_.deallocate(voice);
    }
};

void audio_thread(AudioEngine& engine) {
    for (int i = 0; i < 100; ++i) {
        AudioVoice* voice = engine.allocate_voice();
        if (voice) {
            // Process audio...
            engine.free_voice(voice);
        }
    }
}

int main() {
    AudioEngine engine;
    
    std::vector<std::thread> threads;
    for (int i = 0; i < 4; ++i) {
        threads.emplace_back(audio_thread, std::ref(engine));
    }
    
    for (auto& t : threads) {
        t.join();
    }
}
```

## Stack Allocator

### Frame-Based Allocation

```cpp
#include "stack_allocator.h"

class Game {
    fast_alloc::StackAllocator frame_allocator_;
    
public:
    Game() : frame_allocator_(1024 * 1024) {} // 1MB per frame
    
    void update() {
        // Allocate temporary data for this frame
        void* temp_buffer = frame_allocator_.allocate(4096);
        void* particle_buffer = frame_allocator_.allocate(8192);
        
        // Use buffers...
        
        // Reset at end of frame - O(1) operation!
        frame_allocator_.reset();
    }
};
```

### Scoped Allocations with Markers

```cpp
fast_alloc::StackAllocator stack(1024 * 1024);

void process_level() {
    void* marker = stack.get_marker();
    
    // Allocate level data
    void* level_data = stack.allocate(100000);
    void* entity_data = stack.allocate(50000);
    
    // Process level...
    
    // Rewind to marker when done
    stack.reset(marker);
}

void main_loop() {
    for (int frame = 0; frame < 60; ++frame) {
        process_level();
        // Each frame starts with clean slate
    }
}
```

### Alignment Example

```cpp
fast_alloc::StackAllocator stack(1024);

// Allocate with specific alignment (e.g., for SIMD)
void* simd_buffer = stack.allocate(256, 32); // 32-byte aligned
void* cache_line = stack.allocate(64, 64);   // 64-byte aligned

// Verify alignment
assert(reinterpret_cast<std::uintptr_t>(simd_buffer) % 32 == 0);
```

### Checking Stack Status

```cpp
fast_alloc::StackAllocator stack(1024);

std::cout << "Capacity: " << stack.capacity() << " bytes\n";
std::cout << "Used: " << stack.used() << " bytes\n";
std::cout << "Available: " << stack.available() << " bytes\n";

void* data = stack.allocate(500);
std::cout << "After allocation: " << stack.used() << " bytes used\n";
```

## Free List Allocator

### Basic Variable-Size Allocation

```cpp
#include "freelist_allocator.h"

fast_alloc::FreeListAllocator allocator(
    1024 * 1024,  // 1MB total
    fast_alloc::FreeListStrategy::FirstFit
);

// Allocate different sizes
void* small = allocator.allocate(100);
void* medium = allocator.allocate(1000);
void* large = allocator.allocate(10000);

// Deallocate in any order
allocator.deallocate(medium);
allocator.deallocate(small);
allocator.deallocate(large);
```

### Asset Manager Example

```cpp
class AssetManager {
    fast_alloc::FreeListAllocator allocator_;
    
public:
    AssetManager() 
        : allocator_(100 * 1024 * 1024, // 100MB
                     fast_alloc::FreeListStrategy::BestFit) {}
    
    void* load_texture(size_t size) {
        return allocator_.allocate(size, 16); // 16-byte aligned
    }
    
    void* load_mesh(size_t size) {
        return allocator_.allocate(size, 32); // 32-byte aligned
    }
    
    void unload_asset(void* asset) {
        allocator_.deallocate(asset);
    }
    
    void print_stats() {
        std::cout << "Total: " << allocator_.capacity() << " bytes\n";
        std::cout << "Used: " << allocator_.used() << " bytes\n";
        std::cout << "Free: " << allocator_.available() << " bytes\n";
        std::cout << "Active allocations: " << allocator_.num_allocations() << "\n";
    }
};
```

### First-Fit vs Best-Fit

```cpp
// First-Fit: Faster allocation, may fragment more
fast_alloc::FreeListAllocator first_fit(
    1024 * 1024,
    fast_alloc::FreeListStrategy::FirstFit
);

// Best-Fit: Slower allocation, reduces fragmentation
fast_alloc::FreeListAllocator best_fit(
    1024 * 1024,
    fast_alloc::FreeListStrategy::BestFit
);

// Use First-Fit for:
// - Frequent allocations/deallocations
// - When speed is critical
// - Short-lived allocations

// Use Best-Fit for:
// - Long-lived allocations
// - When fragmentation is a concern
// - Variable-size allocations with high reuse
```

## Best Practices

### Choosing the Right Allocator

```cpp
// Pool: Same-size objects, frequent alloc/dealloc
fast_alloc::PoolAllocator entity_pool(sizeof(Entity), 10000);

// Stack: Temporary per-frame data, LIFO lifetime
fast_alloc::StackAllocator frame_stack(1024 * 1024);

// Free List: Variable sizes, individual deallocation
fast_alloc::FreeListAllocator asset_allocator(100 * 1024 * 1024);
```

### Error Handling

```cpp
fast_alloc::PoolAllocator pool(64, 100);

void* ptr = pool.allocate();
if (!ptr) {
    // Pool exhausted - handle gracefully
    std::cerr << "Pool allocation failed!\n";
    return;
}

// Always check before use
use_memory(ptr);
pool.deallocate(ptr);
```

### Placement New Pattern

```cpp
template<typename T>
class ObjectPool {
    fast_alloc::PoolAllocator pool_;
    
public:
    ObjectPool(size_t capacity) 
        : pool_(sizeof(T), capacity) {}
    
    template<typename... Args>
    T* create(Args&&... args) {
        void* mem = pool_.allocate();
        if (!mem) return nullptr;
        
        // Construct object in-place
        return new (mem) T(std::forward<Args>(args)...);
    }
    
    void destroy(T* obj) {
        // Explicit destructor call
        obj->~T();
        pool_.deallocate(obj);
    }
};

// Usage
ObjectPool<Entity> entities(1000);
Entity* e = entities.create(x, y, z);
entities.destroy(e);
```

### RAII Wrapper

```cpp
template<typename Allocator>
class ScopedAllocation {
    Allocator& allocator_;
    void* ptr_;
    
public:
    ScopedAllocation(Allocator& alloc, size_t size)
        : allocator_(alloc), ptr_(allocator_.allocate(size)) {}
    
    ~ScopedAllocation() {
        if (ptr_) {
            allocator_.deallocate(ptr_);
        }
    }
    
    void* get() const { return ptr_; }
    
    // Prevent copy
    ScopedAllocation(const ScopedAllocation&) = delete;
    ScopedAllocation& operator=(const ScopedAllocation&) = delete;
};

// Usage
fast_alloc::PoolAllocator pool(64, 100);
{
    ScopedAllocation alloc(pool, 64);
    // Use alloc.get()...
} // Automatically deallocated
```

## Common Patterns

### Hybrid Approach

```cpp
class GameEngine {
    // Pool for entities (same size, frequent alloc/dealloc)
    fast_alloc::PoolAllocator entity_pool_;
    
    // Stack for per-frame temporary data
    fast_alloc::StackAllocator frame_stack_;
    
    // Free list for variable-size assets
    fast_alloc::FreeListAllocator asset_allocator_;
    
public:
    GameEngine()
        : entity_pool_(sizeof(Entity), 10000)
        , frame_stack_(2 * 1024 * 1024)  // 2MB
        , asset_allocator_(100 * 1024 * 1024,  // 100MB
                           fast_alloc::FreeListStrategy::BestFit) {}
    
    void update() {
        // Use appropriate allocator for each task
        Entity* e = create_entity();         // Pool
        void* temp = frame_stack_.allocate(1024);  // Stack
        void* asset = load_asset(size);      // Free list
        
        // ... game logic ...
        
        frame_stack_.reset(); // Clear frame data
    }
};
```

### Memory Budget Management

```cpp
class MemoryManager {
    fast_alloc::PoolAllocator small_pool_;
    fast_alloc::PoolAllocator medium_pool_;
    fast_alloc::FreeListAllocator large_allocator_;
    
public:
    void* allocate(size_t size) {
        if (size <= 64) {
            return small_pool_.allocate();
        } else if (size <= 256) {
            return medium_pool_.allocate();
        } else {
            return large_allocator_.allocate(size);
        }
    }
    
    void print_budget() {
        size_t small_used = small_pool_.allocated() * small_pool_.block_size();
        size_t medium_used = medium_pool_.allocated() * medium_pool_.block_size();
        size_t large_used = large_allocator_.used();
        
        std::cout << "Memory Budget:\n"
                  << "  Small (<64B): " << small_used << "\n"
                  << "  Medium (<256B): " << medium_used << "\n"
                  << "  Large: " << large_used << "\n"
                  << "  Total: " << (small_used + medium_used + large_used) << "\n";
    }
};
```

## Performance Tips

1. **Pre-allocate**: Size pools and stacks appropriately to avoid failures
2. **Batch operations**: Allocate/deallocate in batches when possible
3. **Reuse**: Prefer pool allocators for objects with high turnover
4. **Profile**: Measure actual performance impact in your use case
5. **Alignment**: Use appropriate alignment for SIMD and cache optimization

## Debugging

Enable assertions in debug builds to catch common errors:

```cpp
// These will assert in debug builds:
pool.deallocate(invalid_ptr);  // Pointer not from this pool
stack.reset(invalid_marker);   // Invalid marker
allocator.allocate(0);         // Zero-size allocation
```

Build with sanitizers to catch memory issues:

```bash
cmake -B build -DCMAKE_BUILD_TYPE=Debug \
  -DCMAKE_CXX_FLAGS="-fsanitize=address -g"
```
