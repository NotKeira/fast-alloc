#include <benchmark/benchmark.h>
#include "pool_allocator.h"
#include <vector>

using namespace fast_alloc;

// Benchmark pool allocator vs new/delete
static void BM_PoolAllocator_Allocate(benchmark::State& state)
{
    const std::size_t block_size = 64;
    const std::size_t block_count = 10000;
    PoolAllocator pool(block_size, block_count);

    for (auto _ : state)
    {
        void* ptr = pool.allocate();
        benchmark::DoNotOptimize(ptr);
        pool.deallocate(ptr);
    }

    state.SetItemsProcessed(state.iterations());
}

BENCHMARK (BM_PoolAllocator_Allocate);

static void BM_NewDelete_Allocate(benchmark::State& state)
{
    const std::size_t block_size = 64;

    for (auto _ : state)
    {
        void* ptr = operator new(block_size);
        benchmark::DoNotOptimize(ptr);
        operator delete(ptr);
    }

    state.SetItemsProcessed(state.iterations());
}

BENCHMARK (BM_NewDelete_Allocate);

// Benchmark bulk allocations
static void BM_PoolAllocator_BulkAllocate(benchmark::State& state)
{
    const std::size_t block_size = 64;
    const std::size_t block_count = 10000;
    const std::size_t num_allocs = state.range(0);

    for (auto _ : state)
    {
        PoolAllocator pool(block_size, block_count);
        std::vector<void*> ptrs;
        ptrs.reserve(num_allocs);

        for (std::size_t i = 0; i < num_allocs; ++i)
        {
            ptrs.push_back(pool.allocate());
        }

        benchmark::DoNotOptimize(ptrs.data());

        for (void* ptr : ptrs)
        {
            pool.deallocate(ptr);
        }
    }

    state.SetItemsProcessed(state.iterations() * num_allocs);
}

BENCHMARK(BM_PoolAllocator_BulkAllocate)->Arg(100)->Arg(1000)->Arg(5000);

static void BM_NewDelete_BulkAllocate(benchmark::State& state)
{
    const std::size_t block_size = 64;
    const std::size_t num_allocs = state.range(0);

    for (auto _ : state)
    {
        std::vector<void*> ptrs;
        ptrs.reserve(num_allocs);

        for (std::size_t i = 0; i < num_allocs; ++i)
        {
            ptrs.push_back(operator new(block_size));
        }

        benchmark::DoNotOptimize(ptrs.data());

        for (void* ptr : ptrs)
        {
            operator delete(ptr);
        }
    }

    state.SetItemsProcessed(state.iterations() * num_allocs);
}

BENCHMARK(BM_NewDelete_BulkAllocate)->Arg(100)->Arg(1000)->Arg(5000);
