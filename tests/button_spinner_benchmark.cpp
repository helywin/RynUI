#include "component/button_scene_service.hpp"

#include <ryn/component.hpp>

#include <algorithm>
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
    if (void* memory = std::malloc(size == 0 ? 1 : size)) return memory;
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

void* operator new(std::size_t size) { return allocation_probe::allocate(size); }
void* operator new[](std::size_t size) { return allocation_probe::allocate(size); }
void operator delete(void* memory) noexcept { std::free(memory); }
void operator delete[](void* memory) noexcept { std::free(memory); }
void operator delete(void* memory, std::size_t) noexcept { std::free(memory); }
void operator delete[](void* memory, std::size_t) noexcept { std::free(memory); }
void* operator new(std::size_t size, std::align_val_t alignment) {
    return allocation_probe::allocate_aligned(
        size, static_cast<std::size_t>(alignment));
}
void* operator new[](std::size_t size, std::align_val_t alignment) {
    return allocation_probe::allocate_aligned(
        size, static_cast<std::size_t>(alignment));
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

struct BenchmarkState final {};

void require(bool condition, const char* message) {
    if (!condition) {
        throw std::runtime_error(message);
    }
}

ryn::component::ButtonVisualData spinner_visuals() {
    ryn::component::ButtonVisualData visuals;
    for (std::size_t index = 0; index < visuals.size(); ++index) {
        visuals[index] = {
            {-0.8F, 0.8F, 0.02F, -0.02F},
            {0.1F, 0.45F, 0.9F, 1.0F},
            index < 2 ? 1.0F : 0.2F,
            0.5F,
            {0.0F, 0.0F},
        };
    }
    return visuals;
}

void set_phase(
    ryn::component::ButtonVisualData& visuals,
    std::size_t phase) {
    for (std::size_t segment = 0;
         segment < ryn::component::button_loading_segment_count;
         ++segment) {
        const auto distance = (segment + ryn::component::button_loading_segment_count
            - phase) % ryn::component::button_loading_segment_count;
        visuals[ryn::component::button_loading_segment_index(segment)].opacity =
            0.2F + 0.8F
                * static_cast<float>(
                    ryn::component::button_loading_segment_count - distance)
                / static_cast<float>(ryn::component::button_loading_segment_count);
    }
}

} // namespace

int main() {
    try {
        constexpr std::size_t button_count = 256;
        constexpr std::size_t iterations = 20'000;
        ryn::runtime::NodeStore nodes;
        ryn::runtime::ComponentHost components(nodes);
        std::vector<ryn::runtime::ComponentId> component_ids;
        std::vector<ryn::runtime::SceneFragmentId> fragments;
        component_ids.reserve(button_count);
        fragments.reserve(button_count);
        components.mount(ryn::Content{[&] {
            auto& build = ryn::runtime::require_component_build_context();
            for (std::size_t index = 0; index < button_count; ++index) {
                const auto component = build.mount_component<BenchmarkState>();
                component_ids.push_back(component);
                fragments.push_back(build.register_scene_fragment(
                    component,
                    ryn::runtime::SceneFragmentPlacement::before_children));
            }
        }});
        ryn::input::InteractionRegistry interactions(components, nodes);
        ryn::input::HitTestSnapshot hit_test(interactions, nodes);
        ryn::component::ComponentSceneComposer composer(
            components, interactions, hit_test);
        composer.reserve(button_count, button_count, 0);
        ryn::component::ButtonSceneService buttons(components, nodes, composer);
        buttons.reserve(button_count);

        std::vector<ryn::component::ButtonSceneId> scenes;
        std::vector<ryn::component::ButtonVisualData> visuals;
        scenes.reserve(button_count);
        visuals.reserve(button_count);
        for (std::size_t index = 0; index < button_count; ++index) {
            visuals.push_back(spinner_visuals());
            scenes.push_back(buttons.create(
                component_ids[index],
                components.root(component_ids[index]),
                fragments[index],
                std::nullopt,
                visuals.back()));
        }
        buttons.instances().clear_dirty_ranges();
        const auto capacity = buttons.instances().capacity();
        require(capacity == button_count * ryn::component::button_visual_layer_count,
                "spinner benchmark did not retain its reserved Quad capacity");

        std::size_t material_updates = 0;
        const auto started = std::chrono::steady_clock::now();
        allocation_probe::count.store(0, std::memory_order_relaxed);
        allocation_probe::tracking.store(true, std::memory_order_relaxed);
        for (std::size_t iteration = 0; iteration < iterations; ++iteration) {
            const auto button = iteration % button_count;
            set_phase(
                visuals[button],
                (iteration / button_count + 1)
                    % ryn::component::button_loading_segment_count);
            material_updates += buttons.update(scenes[button], visuals[button]);
            buttons.instances().clear_dirty_ranges();
        }
        allocation_probe::tracking.store(false, std::memory_order_relaxed);
        const auto elapsed = std::chrono::duration_cast<std::chrono::microseconds>(
            std::chrono::steady_clock::now() - started);
        const auto allocations = allocation_probe::count.load(
            std::memory_order_relaxed);

        require(allocations == 0,
                "reserved spinner Material updates allocated heap memory");
        require(buttons.instances().capacity() == capacity
                    && buttons.instances().size() == capacity
                    && buttons.size() == button_count,
                "spinner steady-state update changed retained capacity or topology");
        require(buttons.diagnostics().geometry_updates == 0
                    && buttons.diagnostics().range_compactions == 0
                    && buttons.diagnostics().fragment_remaps == 0
                    && material_updates > iterations,
                "spinner steady-state update escaped Material-only ranges");
        require(elapsed < std::chrono::seconds(30),
                "spinner capacity benchmark exceeded the hang guard");
        std::cout << "buttons=" << button_count
                  << " segments=" << ryn::component::button_loading_segment_count
                  << " iterations=" << iterations
                  << " allocations=" << allocations
                  << " material_updates=" << material_updates
                  << " capacity=" << capacity
                  << " elapsed_us=" << elapsed.count() << '\n';
    } catch (const std::exception& error) {
        allocation_probe::tracking.store(false, std::memory_order_relaxed);
        std::cerr << error.what() << '\n';
        return 1;
    }
    return 0;
}
