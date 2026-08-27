#include "input/interaction_registry.hpp"

#include <ryn/component.hpp>

#include <atomic>
#include <chrono>
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

struct TestState final {};

ryn::runtime::ComponentId mount_leaf() {
    return ryn::runtime::require_component_build_context()
        .mount_component<TestState>();
}

void require(bool condition, const char* message) {
    if (!condition) {
        throw std::runtime_error(message);
    }
}

} // namespace

int main() {
    try {
        constexpr std::size_t record_count = 1'000;
        constexpr std::size_t query_count = 10'000;

        ryn::runtime::NodeStore nodes;
        ryn::runtime::ComponentHost components(nodes);
        std::vector<ryn::runtime::ComponentId> component_ids;
        component_ids.reserve(record_count);
        components.mount(ryn::Content{[&] {
            for (std::size_t index = 0; index < record_count; ++index) {
                component_ids.push_back(mount_leaf());
            }
        }});

        ryn::input::InteractionRegistry registry(components, nodes);
        registry.reserve(record_count);
        std::vector<ryn::input::HitTestPaintEntry> paint_entries;
        paint_entries.reserve(record_count);
        for (std::size_t index = 0; index < record_count; ++index) {
            const auto node = components.root(component_ids[index]);
            auto& retained = nodes.require(node);
            retained.bounds = {
                100.0F + static_cast<float>(index % 10),
                100.0F + static_cast<float>(index / 10),
                1.0F,
                1.0F,
            };
            retained.place_generation = 1;
            const auto interaction = registry.create({
                component_ids[index],
                node,
                std::nullopt,
                true,
                false,
                {},
            });
            paint_entries.push_back({interaction, std::nullopt});
        }

        ryn::input::HitTestSnapshot snapshot(registry, nodes);
        snapshot.reserve(record_count);
        snapshot.rebuild(
            paint_entries,
            {0.0F, 0.0F, 2'000.0F, 2'000.0F});
        const auto stable_capacity = snapshot.capacity();

        allocation_probe::count.store(0, std::memory_order_relaxed);
        const auto started = std::chrono::steady_clock::now();
        allocation_probe::tracking.store(true, std::memory_order_relaxed);
        for (std::size_t index = 0; index < query_count; ++index) {
            require(!snapshot.hit_test({1.0F, 1.0F}).has_value(),
                    "benchmark query unexpectedly hit a record");
        }
        allocation_probe::tracking.store(false, std::memory_order_relaxed);
        const auto elapsed = std::chrono::steady_clock::now() - started;

        const auto allocations = allocation_probe::count.load(std::memory_order_relaxed);
        const auto elapsed_microseconds =
            std::chrono::duration_cast<std::chrono::microseconds>(elapsed).count();
        require(allocations == 0, "steady-state HitTest query allocated");
        require(snapshot.capacity() == stable_capacity,
                "steady-state HitTest query changed snapshot capacity");
        require(snapshot.diagnostics().records_examined == record_count * query_count,
                "dense HitTest benchmark did not scan the expected records");

        std::cout
            << "hit_test_records=" << record_count
            << " queries=" << query_count
            << " records_examined=" << snapshot.diagnostics().records_examined
            << " allocations=" << allocations
            << " elapsed_us=" << elapsed_microseconds
            << '\n';
    } catch (const std::exception& error) {
        allocation_probe::tracking.store(false, std::memory_order_relaxed);
        std::cerr << error.what() << '\n';
        return 1;
    }
    return 0;
}
