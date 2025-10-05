#include <benchmark/benchmark.h>
#include "freelist_allocator.h"
#include <vector>
#include <random>

using namespace fast_alloc;

// Benchmark free list allocator vs malloc
static void BM_FreeListAllocator_FirstFit(benchmark::State& state)
{
    const std::size_t allocator_size = 1024 * 1024; // 1MB
    FreeListAllocator allocator(allocator_size, FreeListStrategy::FirstFit);

    for (auto _ : state)
    {
        void* ptr = allocator.allocate(64);
        benchmark::DoNotOptimize(ptr);
        allocator.deallocate(ptr);
    }

    state.SetItemsProcessed(state.iterations());
}

BENCHMARK (BM_FreeListAllocator_FirstFit);

static void BM_FreeListAllocator_BestFit(benchmark::State& state)
{
    const std::size_t allocator_size = 1024 * 1024;
    FreeListAllocator allocator(allocator_size, FreeListStrategy::BestFit);

    for (auto _ : state)
    {
        void* ptr = allocator.allocate(64);
        benchmark::DoNotOptimize(ptr);
        allocator.deallocate(ptr);
    }

    state.SetItemsProcessed(state.iterations());
}

BENCHMARK (BM_FreeListAllocator_BestFit);

static void BM_Malloc_Compare(benchmark::State& state)
{
    for (auto _ : state)
    {
        void* ptr = malloc(64);
        benchmark::DoNotOptimize(ptr);
        free(ptr);
    }

    state.SetItemsProcessed(state.iterations());
}

BENCHMARK (BM_Malloc_Compare);

// Benchmark variable-sized allocations
static void BM_FreeListAllocator_VariableSizes(benchmark::State& state)
{
    const std::size_t allocator_size = 1024 * 1024;
    FreeListAllocator allocator(allocator_size, FreeListStrategy::FirstFit);

    std::vector<std::size_t> sizes = {16, 32, 64, 128, 256, 512};
    std::size_t size_idx = 0;

    for (auto _ : state)
    {
        void* ptr = allocator.allocate(sizes[size_idx]);
        benchmark::DoNotOptimize(ptr);
        allocator.deallocate(ptr);

        size_idx = (size_idx + 1) % sizes.size();
    }

    state.SetItemsProcessed(state.iterations());
}

BENCHMARK (BM_FreeListAllocator_VariableSizes);

// Benchmark fragmentation handling
static void BM_FreeListAllocator_Fragmentation(benchmark::State& state)
{
    const std::size_t allocator_size = 1024 * 1024;
    const std::size_t num_allocs = 100;

    for (auto _ : state)
    {
        state.PauseTiming();
        FreeListAllocator allocator(allocator_size, FreeListStrategy::FirstFit);
        std::vector<void*> ptrs;
        ptrs.reserve(num_allocs);

        // Allocate many blocks
        for (std::size_t i = 0; i < num_allocs; ++i)
        {
            ptrs.push_back(allocator.allocate(1024));
        }

        // Free every other block to create fragmentation
        for (std::size_t i = 1; i < num_allocs; i += 2)
        {
            allocator.deallocate(ptrs[i]);
            ptrs[i] = nullptr;
        }
        state.ResumeTiming();

        // Benchmark allocation in fragmented state
        void* ptr = allocator.allocate(512);
        benchmark::DoNotOptimize(ptr);
        allocator.deallocate(ptr);

        state.PauseTiming();
        for (void* p : ptrs)
        {
            if (p) allocator.deallocate(p);
        }
        state.ResumeTiming();
    }
}

BENCHMARK (BM_FreeListAllocator_Fragmentation);
