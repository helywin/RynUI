#include "graphics/quad_primitive.hpp"
#include "graphics/rounded_effect.hpp"

#include <atomic>
#include <cstddef>
#include <cstdlib>
#include <iostream>
#include <new>
#include <stdexcept>
#include <vector>

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

int main() {
    try {
        constexpr std::size_t effect_count = 1'024;
        static_assert(sizeof(ryn::graphics::QuadInstance) == 48);
        ryn::graphics::RoundedEffectStore store;
        store.reserve(effect_count);
        std::vector<ryn::graphics::RoundedEffectInstance> effects;
        effects.reserve(effect_count);
        for (std::size_t index = 0; index < effect_count; ++index) {
            effects.push_back(ryn::graphics::make_shadow_effect(
                {{static_cast<float>(index % 32) * 12.0F,
                  static_cast<float>(index / 32) * 12.0F,
                  10.0F,
                  10.0F}, 2.0F},
                {ryn::ShadowKind::outer, {}, 2.0F, 0.0F,
                 ryn::Color::rgba8(0, 0, 0, 64)}));
        }
        const auto ids = store.add_batch(effects);
        const ryn::runtime::Rect clip{0.0F, 0.0F, 500.0F, 500.0F};
        if (!store.compact(clip) || store.packed_instances().size() != effect_count) {
            throw std::runtime_error("1k rounded-effect setup did not remain fully packed");
        }
        store.clear_dirty_ranges();
        const auto capacity = store.slot_capacity();
        const auto equal_material = store.at(ids.front()).material;

        allocation_probe::count.store(0, std::memory_order_relaxed);
        allocation_probe::tracking.store(true, std::memory_order_relaxed);
        for (int iteration = 0; iteration < 10'000; ++iteration) {
            static_cast<void>(store.compact(clip));
            static_cast<void>(store.update_material(ids.front(), equal_material));
        }
        allocation_probe::tracking.store(false, std::memory_order_relaxed);
        const auto allocations = allocation_probe::count.load(std::memory_order_relaxed);
        if (allocations != 0 || store.slot_capacity() != capacity
                || store.diagnostics().idle_compactions != 10'000
                || !store.material_dirty_ranges().empty()
                || !store.geometry_dirty_ranges().empty()) {
            std::cerr << "steady-state allocations: " << allocations << '\n';
            throw std::runtime_error(
                "idle/equal rounded-effect updates allocated or requested uploads");
        }
    } catch (const std::exception& error) {
        std::cerr << error.what() << '\n';
        return 1;
    }
    return 0;
}
