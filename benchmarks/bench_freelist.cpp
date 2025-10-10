#include <benchmark/benchmark.h>
#include "freelist_allocator.h"
#include <vector>

using namespace fast_alloc;

static void BM_FreeListAllocator_FirstFit(benchmark::State& state)
{
    constexpr std::size_t allocator_size = 1024 * 1024;
    FreeListAllocator allocator(allocator_size, FreeListStrategy::FirstFit);

    for (auto _ : state)
    {
        void* ptr = allocator.allocate(64);
        benchmark::DoNotOptimize(ptr);
        allocator.deallocate(ptr);
    }

    state.SetItemsProcessed(state.iterations());
}

BENCHMARK(BM_FreeListAllocator_FirstFit);

static void BM_FreeListAllocator_BestFit(benchmark::State& state)
{
    constexpr std::size_t allocator_size = 1024 * 1024;
    FreeListAllocator allocator(allocator_size, FreeListStrategy::BestFit);

    for (auto _ : state)
    {
        void* ptr = allocator.allocate(64);
        benchmark::DoNotOptimize(ptr);
        allocator.deallocate(ptr);
    }

    state.SetItemsProcessed(state.iterations());
}

BENCHMARK(BM_FreeListAllocator_BestFit);

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

BENCHMARK(BM_Malloc_Compare);

static void BM_FreeListAllocator_VariableSizes(benchmark::State& state)
{
    constexpr std::size_t allocator_size = 1024 * 1024;
    FreeListAllocator allocator(allocator_size, FreeListStrategy::FirstFit);

    const std::vector<std::size_t> sizes = {16, 32, 64, 128, 256, 512};
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

BENCHMARK(BM_FreeListAllocator_VariableSizes);

static void BM_FreeListAllocator_Fragmentation(benchmark::State& state)
{
    constexpr std::size_t allocator_size = 1024 * 1024;

    for (auto _ : state)
    {
        constexpr std::size_t num_allocs = 100;
        state.PauseTiming();
        FreeListAllocator allocator(allocator_size, FreeListStrategy::FirstFit);
        std::vector<void*> ptrs;
        ptrs.reserve(num_allocs);

        for (std::size_t i = 0; i < num_allocs; ++i)
        {
            ptrs.push_back(allocator.allocate(1024));
        }

        for (std::size_t i = 1; i < num_allocs; i += 2)
        {
            allocator.deallocate(ptrs[i]);
            ptrs[i] = nullptr;
        }
        state.ResumeTiming();

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

BENCHMARK(BM_FreeListAllocator_Fragmentation);
