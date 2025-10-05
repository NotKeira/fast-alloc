#include <benchmark/benchmark.h>
#include "stack_allocator.h"

using namespace fast_alloc;

// Benchmark stack allocator vs malloc
static void BM_StackAllocator_Allocate(benchmark::State& state) {
    const std::size_t stack_size = 1024 * 1024; // 1MB
    StackAllocator stack(stack_size);
    
    for (auto _ : state) {
        void* ptr = stack.allocate(64);
        benchmark::DoNotOptimize(ptr);
        stack.reset();
    }
    
    state.SetItemsProcessed(state.iterations());
}
BENCHMARK(BM_StackAllocator_Allocate);

static void BM_Malloc_Allocate(benchmark::State& state) {
    for (auto _ : state) {
        void* ptr = malloc(64);
        benchmark::DoNotOptimize(ptr);
        free(ptr);
    }
    
    state.SetItemsProcessed(state.iterations());
}
BENCHMARK(BM_Malloc_Allocate);

// Benchmark frame-based allocation pattern
static void BM_StackAllocator_FramePattern(benchmark::State& state) {
    const std::size_t stack_size = 1024 * 1024; // 1MB
    const std::size_t allocs_per_frame = state.range(0);
    StackAllocator stack(stack_size);
    
    for (auto _ : state) {
        // Simulate frame allocations
        for (std::size_t i = 0; i < allocs_per_frame; ++i) {
            void* ptr = stack.allocate(64);
            benchmark::DoNotOptimize(ptr);
        }
        
        // Reset at end of frame
        stack.reset();
    }
    
    state.SetItemsProcessed(state.iterations() * allocs_per_frame);
}
BENCHMARK(BM_StackAllocator_FramePattern)->Arg(10)->Arg(100)->Arg(1000);

static void BM_Malloc_FramePattern(benchmark::State& state) {
    const std::size_t allocs_per_frame = state.range(0);
    
    for (auto _ : state) {
        void** ptrs = new void*[allocs_per_frame];
        
        for (std::size_t i = 0; i < allocs_per_frame; ++i) {
            ptrs[i] = malloc(64);
        }
        
        benchmark::DoNotOptimize(ptrs);
        
        for (std::size_t i = 0; i < allocs_per_frame; ++i) {
            free(ptrs[i]);
        }
        
        delete[] ptrs;
    }
    
    state.SetItemsProcessed(state.iterations() * allocs_per_frame);
}
BENCHMARK(BM_Malloc_FramePattern)->Arg(10)->Arg(100)->Arg(1000);

// Benchmark aligned allocations
static void BM_StackAllocator_AlignedAllocate(benchmark::State& state) {
    const std::size_t stack_size = 1024 * 1024;
    const std::size_t alignment = state.range(0);
    StackAllocator stack(stack_size);
    
    for (auto _ : state) {
        void* ptr = stack.allocate(64, alignment);
        benchmark::DoNotOptimize(ptr);
        stack.reset();
    }
    
    state.SetItemsProcessed(state.iterations());
}
BENCHMARK(BM_StackAllocator_AlignedAllocate)->Arg(16)->Arg(32)->Arg(64);