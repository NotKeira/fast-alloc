#include <benchmark/benchmark.h>
#include "threadsafe_pool_allocator.h"
#include <vector>
#include <memory>

using namespace fast_alloc;

static void BM_ThreadSafePoolAllocator_SingleThread(benchmark::State& state)
{
    constexpr std::size_t block_size = 64;
    constexpr std::size_t block_count = 10000;
    ThreadSafePoolAllocator pool(block_size, block_count);

    for (auto _ : state)
    {
        void* ptr = pool.allocate();
        benchmark::DoNotOptimize(ptr);
        pool.deallocate(ptr);
    }

    state.SetItemsProcessed(static_cast<int64_t>(state.iterations()));
}

BENCHMARK(BM_ThreadSafePoolAllocator_SingleThread);

namespace
{
    // Setup and teardown run once per measurement, outside the worker threads.
    std::unique_ptr<ThreadSafePoolAllocator> shared_pool;

    void setup_pool(const benchmark::State&)
    {
        shared_pool = std::make_unique<ThreadSafePoolAllocator>(64, 10000);
    }

    void setup_contention_pool(const benchmark::State&)
    {
        shared_pool = std::make_unique<ThreadSafePoolAllocator>(64, 1000);
    }

    void teardown_pool(const benchmark::State&)
    {
        shared_pool.reset();
    }

    void record_pool_operations(benchmark::State& state, const int64_t completed,
                                const int64_t expected)
    {
        state.SetItemsProcessed(completed);
        if (completed != expected)
        {
            state.SkipWithError("Pool exhausted during benchmark");
        }
        // Google Benchmark synchronises workers at the end of the timed loop.
        if (state.thread_index() == 0 && shared_pool->allocated() != 0)
        {
            state.SkipWithError("Pool still has outstanding allocations");
        }
    }
}

static void BM_ThreadSafePoolAllocator_MultiThread(benchmark::State& state)
{
    auto& pool = *shared_pool;
    int64_t completed = 0;

    for (auto _ : state)
    {
        void* ptr = pool.allocate();
        benchmark::DoNotOptimize(ptr);
        if (ptr)
        {
            pool.deallocate(ptr);
            ++completed;
        }
    }

    record_pool_operations(state, completed, state.iterations());
}

BENCHMARK(BM_ThreadSafePoolAllocator_MultiThread)
    ->Threads(2)
    ->Threads(4)
    ->Threads(8)
    ->Setup(setup_pool)
    ->Teardown(teardown_pool)
    ->UseRealTime();

static void BM_ThreadSafePoolAllocator_Contention(benchmark::State& state)
{
    auto& pool = *shared_pool;
    constexpr int operations = 100;
    int64_t completed = 0;

    for (auto _ : state)
    {
        for (int j = 0; j < operations; ++j)
        {
            void* ptr = pool.allocate();
            benchmark::DoNotOptimize(ptr);
            if (ptr)
            {
                pool.deallocate(ptr);
                ++completed;
            }
        }
    }

    record_pool_operations(state, completed, state.iterations() * operations);
}

BENCHMARK(BM_ThreadSafePoolAllocator_Contention)
    ->Threads(2)
    ->Threads(4)
    ->Threads(8)
    ->Setup(setup_contention_pool)
    ->Teardown(teardown_pool)
    ->UseRealTime();

static void BM_NewDelete_MultiThread(benchmark::State& state)
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

BENCHMARK(BM_NewDelete_MultiThread)
    ->Threads(2)
    ->Threads(4)
    ->Threads(8)
    ->UseRealTime();

static void BM_ThreadSafePoolAllocator_BulkOperations(benchmark::State& state)
{
    auto& pool = *shared_pool;
    const auto operations_per_thread = static_cast<std::size_t>(state.range(0));
    std::vector<void*> ptrs(operations_per_thread);
    int64_t completed = 0;

    for (auto _ : state)
    {
        for (auto& ptr : ptrs)
        {
            ptr = pool.allocate();
            benchmark::DoNotOptimize(ptr);
        }

        for (void* ptr : ptrs)
        {
            if (ptr)
            {
                pool.deallocate(ptr);
                ++completed;
            }
        }
    }

    record_pool_operations(state, completed, state.iterations() * state.range(0));
}

BENCHMARK(BM_ThreadSafePoolAllocator_BulkOperations)
    ->Arg(100)
    ->Arg(500)
    ->Arg(1000)
    ->Threads(4)
    ->Setup(setup_pool)
    ->Teardown(teardown_pool)
    ->UseRealTime();
