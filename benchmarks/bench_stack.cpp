#include <benchmark/benchmark.h>
#include "stack_allocator.h"
#include <vector>

using namespace fast_alloc;

static void BM_StackAllocator_Allocate(benchmark::State& state)
{
    constexpr std::size_t stack_size = 1024 * 1024;
    StackAllocator stack(stack_size);

    for (auto _ : state)
    {
        void* ptr = stack.allocate(64);
        benchmark::DoNotOptimize(ptr);
        stack.reset();
    }

    state.SetItemsProcessed(state.iterations());
}

BENCHMARK(BM_StackAllocator_Allocate);

static void BM_Malloc_Allocate(benchmark::State& state)
{
    for (auto _ : state)
    {
        void* ptr = malloc(64);
        benchmark::DoNotOptimize(ptr);
        free(ptr);
    }

    state.SetItemsProcessed(state.iterations());
}

BENCHMARK(BM_Malloc_Allocate);

static void BM_StackAllocator_FramePattern(benchmark::State& state)
{
    constexpr std::size_t stack_size = 1024 * 1024;
    const std::size_t allocs_per_frame = state.range(0);
    StackAllocator stack(stack_size);

    for (auto _ : state)
    {
        for (std::size_t i = 0; i < allocs_per_frame; ++i)
        {
            void* ptr = stack.allocate(64);
            benchmark::DoNotOptimize(ptr);
        }

        stack.reset();
    }

    state.SetItemsProcessed(state.iterations() * allocs_per_frame);
}

BENCHMARK(BM_StackAllocator_FramePattern)->Arg(10)->Arg(100)->Arg(1000);

static void BM_Malloc_FramePattern(benchmark::State& state)
{
    const std::size_t allocs_per_frame = state.range(0);
    std::vector<void*> ptrs(allocs_per_frame);
    int64_t completed = 0;

    for (auto _ : state)
    {
        for (auto& ptr : ptrs)
        {
            ptr = malloc(64);
        }

        benchmark::DoNotOptimize(ptrs.data());
        benchmark::ClobberMemory();

        for (void* ptr : ptrs)
        {
            if (ptr)
            {
                free(ptr);
                ++completed;
            }
        }
    }

    state.SetItemsProcessed(completed);
    if (completed != state.iterations() * state.range(0))
    {
        state.SkipWithError("malloc failed during benchmark");
    }
}

BENCHMARK(BM_Malloc_FramePattern)->Arg(10)->Arg(100)->Arg(1000);

static void BM_StackAllocator_AlignedAllocate(benchmark::State& state)
{
    constexpr std::size_t stack_size = 1024 * 1024;
    const std::size_t alignment = state.range(0);
    StackAllocator stack(stack_size);

    for (auto _ : state)
    {
        void* ptr = stack.allocate(64, alignment);
        benchmark::DoNotOptimize(ptr);
        stack.reset();
    }

    state.SetItemsProcessed(state.iterations());
}

BENCHMARK(BM_StackAllocator_AlignedAllocate)->Arg(16)->Arg(32)->Arg(64);
