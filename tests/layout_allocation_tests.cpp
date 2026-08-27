#include "layout/layout_engine.hpp"

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
    return allocation_probe::allocate_aligned(
        size,
        static_cast<std::size_t>(alignment));
}

void* operator new[](std::size_t size, std::align_val_t alignment) {
    return allocation_probe::allocate_aligned(
        size,
        static_cast<std::size_t>(alignment));
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
        constexpr std::size_t child_count = 24;
        constexpr std::size_t measured_layouts = 1'000;
        ryn::runtime::NodeStore nodes;
        const auto root = nodes.create_root();
        ryn::layout::LayoutEngine layout(nodes);
        ryn::layout::FlexLayout flex;
        flex.main_gap = 8.0F;
        flex.cross_gap = 12.0F;
        flex.wrap = ryn::layout::FlexWrap::wrap;
        flex.justify = ryn::layout::FlexJustify::space_between;
        flex.align = ryn::layout::FlexAlign::center;
        layout.set_layout(root, flex);
        for (std::size_t index = 0; index < child_count; ++index) {
            const auto child = nodes.create_child(root);
            layout.set_layout(child, ryn::layout::LeafLayout{{
                18.0F + static_cast<float>(index % 4),
                10.0F + static_cast<float>(index % 3),
            }});
            nodes.require(child).external_layout.flex_grow = 1.0F + static_cast<float>(index % 3);
            nodes.require(child).external_layout.order = static_cast<int>(index % 5) - 2;
        }

        const auto constraints = ryn::layout::Constraints::fixed(180.0F, 120.0F);
        static_cast<void>(layout.layout(root, constraints));
        static_cast<void>(layout.layout(root, constraints));
        const auto warm = layout.flex_layout_diagnostics(root);
        require(warm.item_count == child_count && warm.line_count > 1,
                "allocation fixture did not form wrapped Flex lines");

        allocation_probe::count.store(0, std::memory_order_relaxed);
        allocation_probe::tracking.store(true, std::memory_order_relaxed);
        for (std::size_t index = 0; index < measured_layouts; ++index) {
            static_cast<void>(layout.layout(root, constraints));
        }
        allocation_probe::tracking.store(false, std::memory_order_relaxed);

        const auto allocations = allocation_probe::count.load(
            std::memory_order_relaxed);
        const auto stable = layout.flex_layout_diagnostics(root);
        require(allocations == 0,
                "stable wrapped Flex layout allocated");
        require(stable.item_capacity == warm.item_capacity
                    && stable.line_capacity == warm.line_capacity,
                "stable wrapped Flex layout changed scratch capacity");

        std::cout
            << "flex_children=" << child_count
            << " layouts=" << measured_layouts
            << " lines=" << stable.line_count
            << " allocations=" << allocations
            << '\n';
    } catch (const std::exception& error) {
        allocation_probe::tracking.store(false, std::memory_order_relaxed);
        std::cerr << error.what() << '\n';
        return 1;
    }
    return 0;
}
