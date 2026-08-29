#include "animation/runtime.hpp"

#include <atomic>
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
    if (void* memory = std::malloc(size == 0 ? 1 : size)) return memory;
    throw std::bad_alloc();
}

void* allocate_aligned(std::size_t size, std::size_t alignment) {
    record();
#if defined(_MSC_VER)
    if (void* memory = _aligned_malloc(size == 0 ? 1 : size, alignment)) return memory;
#else
    void* memory = nullptr;
    if (posix_memalign(&memory, alignment, size == 0 ? 1 : size) == 0) return memory;
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

void* operator new(std::size_t size) { return allocation_probe::allocate(size); }
void* operator new[](std::size_t size) { return allocation_probe::allocate(size); }
void operator delete(void* memory) noexcept { std::free(memory); }
void operator delete[](void* memory) noexcept { std::free(memory); }
void operator delete(void* memory, std::size_t) noexcept { std::free(memory); }
void operator delete[](void* memory, std::size_t) noexcept { std::free(memory); }
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

struct ScalarSink final : ryn::animation::AnimationTargetSink {
    void apply(
        ryn::animation::AnimationId,
        ryn::animation::AnimationTargetId,
        const ryn::animation::AnimationValue& candidate,
        ryn::animation::AnimationDirtyDomain) override {
        value = candidate;
        ++applies;
    }

    ryn::animation::AnimationValue value{0.0F};
    std::size_t applies{0};
};

} // namespace

int main() {
    try {
        using namespace ryn::animation;
        constexpr int iterations = 10'000;
        AnimationRuntime runtime;
        runtime.reserve(16, 2, 4);
        ScalarSink sink;
        const auto scope = runtime.create_scope();
        const auto target = runtime.register_target(
            scope, sink, AnimationValueKind::scalar,
            AnimationDirtyDomain::material | AnimationDirtyDomain::animation);
        const AnimationSpec spec{
            {}, AnimationDuration::microseconds(100), Easing::linear()};
        const auto capacity_growths = runtime.diagnostics().capacity_growths;

        allocation_probe::count.store(0, std::memory_order_relaxed);
        allocation_probe::tracking.store(true, std::memory_order_relaxed);
        for (int iteration = 0; iteration < iterations; ++iteration) {
            const auto base = AnimationTime::microseconds(
                static_cast<std::int64_t>(iteration) * 1000);
            const auto id = runtime.play(target, 0.0F, 1.0F, spec, base);
            static_cast<void>(runtime.tick(
                base + AnimationDuration::microseconds(25)));
            static_cast<void>(runtime.retarget(
                id, 2.0F, spec,
                base + AnimationDuration::microseconds(25)));
            static_cast<void>(runtime.tick(
                base + AnimationDuration::microseconds(50)));
            if ((iteration & 1) == 0) {
                static_cast<void>(runtime.cancel(
                    id, base + AnimationDuration::microseconds(75)));
            } else {
                static_cast<void>(runtime.finish(id));
            }
        }
        allocation_probe::tracking.store(false, std::memory_order_relaxed);

        const auto allocations = allocation_probe::count.load(
            std::memory_order_relaxed);
        const auto& diagnostics = runtime.diagnostics();
        std::cout << "iterations=" << iterations
                  << " allocations=" << allocations
                  << " created=" << diagnostics.created
                  << " completed=" << diagnostics.completed
                  << " canceled=" << diagnostics.canceled
                  << " retargeted=" << diagnostics.retargeted
                  << " applies=" << sink.applies << '\n';
        if (allocations != 0) {
            throw std::runtime_error(
                "reserved AnimationRuntime steady-state paths allocated heap memory");
        }
        if (diagnostics.capacity_growths != capacity_growths) {
            throw std::runtime_error(
                "AnimationRuntime capacity grew after reserve");
        }
        if (diagnostics.active != 0
                || diagnostics.created != static_cast<std::uint64_t>(iterations)
                || diagnostics.retargeted != static_cast<std::uint64_t>(iterations)
                || diagnostics.completed + diagnostics.canceled
                    != diagnostics.created) {
            throw std::runtime_error(
                "AnimationRuntime lifecycle diagnostics are not conserved");
        }
    } catch (const std::exception& error) {
        allocation_probe::tracking.store(false, std::memory_order_relaxed);
        std::cerr << error.what() << '\n';
        return 1;
    }
    return 0;
}
