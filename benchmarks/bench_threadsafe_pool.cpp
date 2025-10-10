#include <benchmark/benchmark.h>
#include "threadsafe_pool_allocator.h"
#include <vector>
#include <thread>

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

static void BM_ThreadSafePoolAllocator_MultiThread(benchmark::State& state)
{
    constexpr std::size_t block_size = 64;
    constexpr std::size_t block_count = 10000;
    ThreadSafePoolAllocator pool(block_size, block_count);

    const auto num_threads = static_cast<std::size_t>(state.range(0));

    for (auto _ : state)
    {
        std::vector<std::thread> threads;
        threads.reserve(num_threads);

        for (std::size_t i = 0; i < num_threads; ++i)
        {
            threads.emplace_back([&pool]()
            {
                void* ptr = pool.allocate();
                benchmark::DoNotOptimize(ptr);
                if (ptr) pool.deallocate(ptr);
            });
        }

        for (auto& thread : threads)
        {
            thread.join();
        }
    }

    state.SetItemsProcessed(static_cast<int64_t>(state.iterations() * num_threads));
}

BENCHMARK(BM_ThreadSafePoolAllocator_MultiThread)
    ->Arg(2)
    ->Arg(4)
    ->Arg(8)
    ->UseRealTime();

static void BM_ThreadSafePoolAllocator_Contention(benchmark::State& state)
{
    constexpr std::size_t block_size = 64;
    constexpr std::size_t block_count = 1000;
    ThreadSafePoolAllocator pool(block_size, block_count);

    const auto num_threads = static_cast<std::size_t>(state.range(0));

    for (auto _ : state)
    {
        std::vector<std::thread> threads;
        threads.reserve(num_threads);

        for (std::size_t i = 0; i < num_threads; ++i)
        {
            threads.emplace_back([&pool]()
            {
                constexpr int operations = 100;
                for (int j = 0; j < operations; ++j)
                {
                    void* ptr = pool.allocate();
                    if (ptr) pool.deallocate(ptr);
                }
            });
        }

        for (auto& thread : threads)
        {
            thread.join();
        }
    }

    state.SetItemsProcessed(static_cast<int64_t>(state.iterations() * num_threads * 100));
}

BENCHMARK(BM_ThreadSafePoolAllocator_Contention)
    ->Arg(2)
    ->Arg(4)
    ->Arg(8)
    ->UseRealTime();

static void BM_NewDelete_MultiThread(benchmark::State& state)
{
    const auto num_threads = static_cast<std::size_t>(state.range(0));

    for (auto _ : state)
    {
        std::vector<std::thread> threads;
        threads.reserve(num_threads);

        for (std::size_t i = 0; i < num_threads; ++i)
        {
            threads.emplace_back([]()
            {
                constexpr std::size_t block_size = 64;
                void* ptr = operator new(block_size);
                benchmark::DoNotOptimize(ptr);
                operator delete(ptr);
            });
        }

        for (auto& thread : threads)
        {
            thread.join();
        }
    }

    state.SetItemsProcessed(static_cast<int64_t>(state.iterations() * num_threads));
}

BENCHMARK(BM_NewDelete_MultiThread)
    ->Arg(2)
    ->Arg(4)
    ->Arg(8)
    ->UseRealTime();

static void BM_ThreadSafePoolAllocator_BulkOperations(benchmark::State& state)
{
    constexpr std::size_t block_size = 64;
    constexpr std::size_t block_count = 10000;
    ThreadSafePoolAllocator pool(block_size, block_count);

    const auto operations_per_thread = static_cast<std::size_t>(state.range(0));
    constexpr std::size_t num_threads = 4;

    for (auto _ : state)
    {
        std::vector<std::thread> threads;
        threads.reserve(num_threads);

        for (std::size_t i = 0; i < num_threads; ++i)
        {
            threads.emplace_back([&pool, operations_per_thread]()
            {
                std::vector<void*> ptrs;
                ptrs.reserve(operations_per_thread);

                for (std::size_t j = 0; j < operations_per_thread; ++j)
                {
                    void* ptr = pool.allocate();
                    if (ptr) ptrs.push_back(ptr);
                }

                for (void* ptr : ptrs)
                {
                    pool.deallocate(ptr);
                }
            });
        }

        for (auto& thread : threads)
        {
            thread.join();
        }
    }

    state.SetItemsProcessed(static_cast<int64_t>(state.iterations() * num_threads * operations_per_thread));
}

BENCHMARK(BM_ThreadSafePoolAllocator_BulkOperations)
    ->Arg(100)
    ->Arg(500)
    ->Arg(1000)
    ->UseRealTime();
