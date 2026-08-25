#include <ryn/reactive.hpp>

#include <atomic>
#include <chrono>
#include <cstddef>
#include <cstdlib>
#include <iostream>
#include <new>
#include <stdexcept>

#if defined(_MSC_VER)
#include <malloc.h>
#endif

namespace allocation_probe {

std::atomic<bool> tracking{false};
std::atomic<std::size_t> count{0};

void record() noexcept {
    if (tracking.load(std::memory_order_relaxed)) {
        count.fetch_add(1, std::memory_order_relaxed);
    }
}

void* allocate(std::size_t size) {
    record();
    if (void* memory = std::malloc(size == 0 ? 1 : size)) {
        return memory;
    }
    throw std::bad_alloc();
}

void* allocate_aligned(std::size_t size, std::size_t alignment) {
    record();
#if defined(_MSC_VER)
    if (void* memory = _aligned_malloc(size == 0 ? 1 : size, alignment)) {
        return memory;
    }
#else
    void* memory = nullptr;
    if (posix_memalign(&memory, alignment, size == 0 ? 1 : size) == 0) {
        return memory;
    }
#endif
    throw std::bad_alloc();
}

void deallocate_aligned(void* memory) noexcept {
#if defined(_MSC_VER)
    _aligned_free(memory);
#else
    std::free(memory);
#endif
}

} // namespace allocation_probe

void* operator new(std::size_t size) {
    return allocation_probe::allocate(size);
}

void* operator new[](std::size_t size) {
    return allocation_probe::allocate(size);
}

void operator delete(void* memory) noexcept {
    std::free(memory);
}

void operator delete[](void* memory) noexcept {
    std::free(memory);
}

void operator delete(void* memory, std::size_t) noexcept {
    std::free(memory);
}

void operator delete[](void* memory, std::size_t) noexcept {
    std::free(memory);
}

void* operator new(std::size_t size, std::align_val_t alignment) {
    return allocation_probe::allocate_aligned(size, static_cast<std::size_t>(alignment));
}

void* operator new[](std::size_t size, std::align_val_t alignment) {
    return allocation_probe::allocate_aligned(size, static_cast<std::size_t>(alignment));
}

void operator delete(void* memory, std::align_val_t) noexcept {
    allocation_probe::deallocate_aligned(memory);
}

void operator delete[](void* memory, std::align_val_t) noexcept {
    allocation_probe::deallocate_aligned(memory);
}

void operator delete(void* memory, std::size_t, std::align_val_t) noexcept {
    allocation_probe::deallocate_aligned(memory);
}

void operator delete[](void* memory, std::size_t, std::align_val_t) noexcept {
    allocation_probe::deallocate_aligned(memory);
}

namespace {

void require(bool condition, const char* message) {
    if (!condition) {
        throw std::runtime_error(message);
    }
}

} // namespace

int main() {
    try {
        constexpr int warmup_writes = 256;
        constexpr int measured_writes = 10'000;

        ryn::Signal<int> input{0};
        ryn::Memo<int> doubled([&] { return input.get() * 2; });
        ryn::Scope scope;
        int observed_value = 0;
        int effect_runs = 0;
        ryn::effect(scope, [&] {
            observed_value = doubled.get();
            ++effect_runs;
        });

        for (int value = 1; value <= warmup_writes; ++value) {
            input.set(value);
        }

        allocation_probe::count.store(0, std::memory_order_relaxed);
        const auto started = std::chrono::steady_clock::now();
        allocation_probe::tracking.store(true, std::memory_order_relaxed);
        for (int offset = 1; offset <= measured_writes; ++offset) {
            input.set(warmup_writes + offset);
        }
        allocation_probe::tracking.store(false, std::memory_order_relaxed);
        const auto elapsed = std::chrono::steady_clock::now() - started;

        const auto allocations = allocation_probe::count.load(std::memory_order_relaxed);
        const auto elapsed_microseconds =
            std::chrono::duration_cast<std::chrono::microseconds>(elapsed).count();
        std::cout
            << "steady_state_writes=" << measured_writes
            << " allocations=" << allocations
            << " effect_runs=" << effect_runs
            << " elapsed_us=" << elapsed_microseconds
            << '\n';

        require(allocations == 0, "steady-state Signal write/flush allocated heap memory");
        require(observed_value == (warmup_writes + measured_writes) * 2,
                "reactive graph did not reach the final value");
        require(effect_runs == 1 + warmup_writes + measured_writes,
                "reactive graph did not flush exactly once per write");
    } catch (const std::exception& error) {
        allocation_probe::tracking.store(false, std::memory_order_relaxed);
        std::cerr << error.what() << '\n';
        return 1;
    }
    return 0;
}
