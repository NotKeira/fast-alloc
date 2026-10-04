#include <benchmark/benchmark.h>
#include "pool_allocator.h"
#include <vector>

using namespace fast_alloc;

static void BM_PoolAllocator_Allocate(benchmark::State& state)
{
    constexpr std::size_t block_size = 64;
    constexpr std::size_t block_count = 10000;
    PoolAllocator pool(block_size, block_count);

    for (auto _ : state)
    {
        void* ptr = pool.allocate();
        benchmark::DoNotOptimize(ptr);
        pool.deallocate(ptr);
    }

    state.SetItemsProcessed(state.iterations());
}

BENCHMARK(BM_PoolAllocator_Allocate);

static void BM_NewDelete_Allocate(benchmark::State& state)
{
    for (auto _ : state)
    {
        constexpr std::size_t block_size = 64;
        void* ptr = operator new(block_size);
        benchmark::DoNotOptimize(ptr);
        operator delete(ptr);
    }

    state.SetItemsProcessed(state.iterations());
}

BENCHMARK(BM_NewDelete_Allocate);

static void BM_PoolAllocator_BulkAllocate(benchmark::State& state)
{
    const std::size_t num_allocs = state.range(0);
    constexpr std::size_t block_count = 10000;
    constexpr std::size_t block_size = 64;
    PoolAllocator pool(block_size, block_count);
    std::vector<void*> ptrs(num_allocs);
    int64_t completed = 0;

    for (auto _ : state)
    {
        for (auto& ptr : ptrs)
        {
            ptr = pool.allocate();
        }

        benchmark::DoNotOptimize(ptrs.data());
        benchmark::ClobberMemory();

        for (void* ptr : ptrs)
        {
            if (ptr)
            {
                pool.deallocate(ptr);
                ++completed;
            }
        }
    }

    state.SetItemsProcessed(completed);
    if (completed != state.iterations() * state.range(0))
    {
        state.SkipWithError("Pool exhausted during benchmark");
    }
    if (pool.allocated() != 0)
    {
        state.SkipWithError("Pool still has outstanding allocations");
    }
}

BENCHMARK(BM_PoolAllocator_BulkAllocate)->Arg(100)->Arg(1000)->Arg(5000);

static void BM_NewDelete_BulkAllocate(benchmark::State& state)
{
    const std::size_t num_allocs = state.range(0);
    std::vector<void*> ptrs(num_allocs);

    for (auto _ : state)
    {
        for (auto& ptr : ptrs)
        {
            constexpr std::size_t block_size = 64;
            ptr = operator new(block_size);
        }

        benchmark::DoNotOptimize(ptrs.data());
        benchmark::ClobberMemory();

        for (void* ptr : ptrs)
        {
            operator delete(ptr);
        }
    }

    state.SetItemsProcessed(state.iterations() * num_allocs);
}

BENCHMARK(BM_NewDelete_BulkAllocate)->Arg(100)->Arg(1000)->Arg(5000);
