#include "runtime/frame_scheduler.hpp"
#include "runtime/invalidation.hpp"
#include "runtime/node_store.hpp"
#include "theme/theme_runtime.hpp"

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

int main() {
    try {
        const auto scope = ryn::theme_runtime::ThemeScope::create_default();
        ryn::runtime::NodeStore nodes;
        const auto node = nodes.create_root();
        ryn::runtime::FrameRequestState frames;
        ryn::runtime::DirtyQueues dirty(nodes, &frames);
        int invalidations = 0;
        auto subscription = scope->capture(
            [&](ryn::theme_runtime::DirtyPhase phase) {
                ++invalidations;
                dirty.invalidate(node, ryn::runtime::dirty_flags_for_theme(phase));
            },
            [&] { static_cast<void>(scope->text_color()); });
        const auto subscription_allocations =
            scope->diagnostics().subscription_allocations;
        constexpr int equal_updates = 10'000;
        const ryn::ThemeConfig equal_config;
        allocation_probe::count.store(0, std::memory_order_relaxed);
        allocation_probe::tracking.store(true, std::memory_order_relaxed);
        for (int index = 0; index < equal_updates; ++index) {
            static_cast<void>(scope->update(equal_config));
        }
        allocation_probe::tracking.store(false, std::memory_order_relaxed);
        const auto equal_update_allocations =
            allocation_probe::count.load(std::memory_order_relaxed);
        if (equal_update_allocations != 0) {
            std::cerr << "equal update allocations: "
                      << equal_update_allocations << '\n';
            throw std::runtime_error("equal steady-state Theme updates allocated");
        }
        if (invalidations != 0
                || frames.pending()
                || scope->diagnostics().subscription_allocations
                    != subscription_allocations
                || scope->diagnostics().snapshot_reuses
                    < static_cast<std::uint64_t>(equal_updates)) {
            throw std::runtime_error(
                "equal Theme updates requested work or changed subscriptions");
        }

        ryn::ThemeConfig color;
        color.text.tokens.color = ryn::Color::rgba8(20, 80, 180);
        static_cast<void>(scope->update(color));
        if (invalidations != 1
                || !frames.pending()
                || dirty.material_nodes().size() != 1
                || !dirty.layout_roots().empty()
                || !dirty.text_nodes().empty()
                || scope->diagnostics().dirty_phase
                    != ryn::theme_runtime::DirtyPhase::paint_material) {
            throw std::runtime_error(
                "local Theme color update requested measurement work");
        }
        static_cast<void>(subscription);
    } catch (const std::exception& error) {
        std::cerr << error.what() << '\n';
        return 1;
    }
    return 0;
}
