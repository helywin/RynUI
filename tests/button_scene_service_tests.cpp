#include "component/button_scene_service.hpp"

#include <ryn/component.hpp>

#include <algorithm>
#include <array>
#include <atomic>
#include <cstddef>
#include <iostream>
#include <span>
#include <stdexcept>
#include <string>
#include <thread>
#include <vector>

namespace {

struct TestState final {};

void require(bool condition, const char* message) {
    if (!condition) {
        throw std::runtime_error(message);
    }
}

ryn::component::ButtonVisualData visuals(float offset) {
    ryn::component::ButtonVisualData result;
    for (std::size_t index = 0; index < result.size(); ++index) {
        result[index] = {
            {offset, 0.8F, 0.4F, -0.2F},
            {0.1F * static_cast<float>(index + 1), 0.3F, 0.7F, 1.0F},
            index >= static_cast<std::size_t>(
                         ryn::component::ButtonVisualLayer::loading_indicator)
                ? 0.0F
                : 1.0F,
            0.1F,
            {0.0F, 0.0F},
        };
    }
    return result;
}

class RecordingUploadApi final : public ryn::graphics::QuadUploadApi {
public:
    struct Buffer final {
        std::vector<std::byte> bytes;
    };

    ~RecordingUploadApi() override {
        for (auto* buffer : live) {
            delete buffer;
        }
    }

    ryn::graphics::QuadGpuBufferHandle create_vertex_buffer(
        std::size_t size) override {
        auto* buffer = new Buffer;
        buffer->bytes.resize(size);
        live.push_back(buffer);
        ++creates;
        return buffer;
    }

    void release_buffer(ryn::graphics::QuadGpuBufferHandle handle) noexcept override {
        auto* buffer = static_cast<Buffer*>(handle);
        const auto found = std::find(live.begin(), live.end(), buffer);
        if (found != live.end()) {
            delete *found;
            live.erase(found);
            ++releases;
        }
    }

    bool upload(
        ryn::graphics::QuadGpuBufferHandle handle,
        std::size_t offset,
        std::span<const std::byte> bytes) override {
        if (fail_next) {
            fail_next = false;
            error = "injected Quad upload failure";
            return false;
        }
        auto* buffer = static_cast<Buffer*>(handle);
        if (buffer == nullptr || offset + bytes.size() > buffer->bytes.size()) {
            error = "invalid Quad upload";
            return false;
        }
        std::copy(bytes.begin(), bytes.end(), buffer->bytes.begin() + offset);
        upload_offsets.push_back(offset);
        upload_sizes.push_back(bytes.size());
        return true;
    }

    const char* last_error() const noexcept override { return error.c_str(); }

    std::vector<Buffer*> live;
    std::vector<std::size_t> upload_offsets;
    std::vector<std::size_t> upload_sizes;
    std::string error;
    int creates{0};
    int releases{0};
    bool fail_next{false};
};

struct Fixture final {
    ryn::runtime::NodeStore nodes;
    ryn::runtime::ComponentHost components{nodes};
    std::array<ryn::runtime::ComponentId, 3> component_ids;
    std::array<ryn::runtime::SceneFragmentId, 3> fragments;
    ryn::input::InteractionRegistry interactions{components, nodes};
    ryn::input::HitTestSnapshot hit_test{interactions, nodes};
    ryn::component::ComponentSceneComposer composer{
        components, interactions, hit_test};
    ryn::component::ButtonSceneService buttons{components, nodes, composer};
    std::array<ryn::input::InteractionId, 3> interaction_ids;

    Fixture() {
        components.mount(ryn::Content{[&] {
            auto& build = ryn::runtime::require_component_build_context();
            for (std::size_t index = 0; index < component_ids.size(); ++index) {
                component_ids[index] = build.mount_component<TestState>();
                fragments[index] = build.register_scene_fragment(
                    component_ids[index],
                    ryn::runtime::SceneFragmentPlacement::before_children);
            }
        }});
        for (std::size_t index = 0; index < component_ids.size(); ++index) {
            auto& node = nodes.require(components.root(component_ids[index]));
            node.bounds = {10.0F, 10.0F, 80.0F, 40.0F};
            node.place_generation = 1;
            interaction_ids[index] = interactions.create({
                component_ids[index],
                components.root(component_ids[index]),
                std::nullopt,
                true,
                true,
                {},
            });
        }
        composer.reserve(3, 6, 3);
        buttons.reserve(3);
    }
};

void test_fixed_ranges_compaction_and_shared_hit_order() {
    Fixture fixture;
    require(fixture.buttons.instances().capacity()
                == 3 * ryn::component::button_visual_layer_count,
            "Button scene reserve did not preallocate fixed spinner topology");
    const auto first = fixture.buttons.create(
        fixture.component_ids[0],
        fixture.components.root(fixture.component_ids[0]),
        fixture.fragments[0],
        fixture.interaction_ids[0],
        visuals(0.0F));
    const auto second = fixture.buttons.create(
        fixture.component_ids[1],
        fixture.components.root(fixture.component_ids[1]),
        fixture.fragments[1],
        fixture.interaction_ids[1],
        visuals(0.1F));
    require(fixture.buttons.visual_range(first)
                    == ryn::graphics::QuadInstanceRange{
                        0,
                        ryn::component::button_visual_layer_count}
                && fixture.buttons.visual_range(second)
                    == ryn::graphics::QuadInstanceRange{
                        ryn::component::button_visual_layer_count,
                        ryn::component::button_visual_layer_count}
                && fixture.buttons.instances().at(2).opacity == 0.0F
                && fixture.buttons.instances().at(
                    ryn::component::button_visual_layer_count + 2).opacity == 0.0F,
            "Button fixed visual ranges or hidden layers differ");

    std::atomic<bool> wrong_thread_rejected{false};
    std::thread worker([&] {
        try {
            static_cast<void>(fixture.buttons.update(first, visuals(0.5F)));
        } catch (const std::logic_error&) {
            wrong_thread_rejected.store(true, std::memory_order_relaxed);
        }
    });
    worker.join();
    require(wrong_thread_rejected.load(std::memory_order_relaxed),
            "wrong-thread Button scene update was not rejected");

    fixture.composer.rebuild({0.0F, 0.0F, 100.0F, 100.0F});
    require(fixture.composer.ordered_scene().commands().size() == 1
                && fixture.composer.ordered_scene().commands().front()
                    == ryn::graphics::SceneDrawCommand{
                        ryn::graphics::SceneDrawKind::quad,
                        0,
                        2 * ryn::component::button_visual_layer_count,
                        ryn::graphics::invalid_glyph_atlas_page,
                    }
                && fixture.hit_test.hit_test({20.0F, 20.0F})
                    == fixture.interaction_ids[1],
            "Button draw order and HitTest order diverged");

    auto changed = visuals(0.1F);
    fixture.buttons.instances().clear_dirty_ranges();
    const auto changed_segment = ryn::component::button_loading_segment_index(3);
    changed[changed_segment].color = {0.9F, 0.2F, 0.1F, 1.0F};
    require(fixture.buttons.update(second, changed) == 1
                && fixture.buttons.instances().material_dirty_ranges().size() == 1
                && fixture.buttons.instances().material_dirty_ranges().front()
                    == ryn::graphics::QuadInstanceRange{
                        static_cast<std::uint32_t>(
                            ryn::component::button_visual_layer_count
                            + changed_segment),
                        1}
                && fixture.buttons.instances().geometry_dirty_ranges().empty(),
            "Button Material update expanded beyond one visual layer");

    const auto third = fixture.buttons.create(
        fixture.component_ids[2],
        fixture.components.root(fixture.component_ids[2]),
        fixture.fragments[2],
        fixture.interaction_ids[2],
        visuals(0.2F));
    require(fixture.buttons.destroy(second)
                && fixture.buttons.visual_range(third)
                    == ryn::graphics::QuadInstanceRange{
                        ryn::component::button_visual_layer_count,
                        ryn::component::button_visual_layer_count}
                && fixture.buttons.instances().size()
                    == 2 * ryn::component::button_visual_layer_count,
            "middle Button destroy did not compact/remap the surviving range");
    fixture.composer.rebuild({0.0F, 0.0F, 100.0F, 100.0F});
    require(fixture.hit_test.hit_test({20.0F, 20.0F})
                    == fixture.interaction_ids[2]
                && fixture.buttons.diagnostics().fragment_remaps == 1,
            "compacted Button fragment lost visual/HitTest order");

    require(fixture.buttons.destroy(first), "first Button scene destroy failed");
    const auto replacement = fixture.buttons.create(
        fixture.component_ids[0],
        fixture.components.root(fixture.component_ids[0]),
        fixture.fragments[0],
        fixture.interaction_ids[0],
        visuals(0.3F));
    require(replacement.index == first.index
                && replacement.generation != first.generation,
            "Button scene slot reuse did not advance generation");
    bool stale_rejected = false;
    try {
        static_cast<void>(fixture.buttons.update(first, visuals(0.4F)));
    } catch (const std::out_of_range&) {
        stale_rejected = true;
    }
    require(stale_rejected, "stale ButtonSceneId updated a replacement slot");
}

void test_gpu_capacity_sparse_upload_and_failure_retention() {
    Fixture fixture;
    const auto first = fixture.buttons.create(
        fixture.component_ids[0],
        fixture.components.root(fixture.component_ids[0]),
        fixture.fragments[0],
        fixture.interaction_ids[0],
        visuals(0.0F));
    const auto second = fixture.buttons.create(
        fixture.component_ids[1],
        fixture.components.root(fixture.component_ids[1]),
        fixture.fragments[1],
        fixture.interaction_ids[1],
        visuals(0.1F));
    RecordingUploadApi api;
    {
        ryn::graphics::QuadGpuBuffer gpu(api, fixture.buttons.instances());
        static_cast<void>(fixture.buttons.create(
            fixture.component_ids[2],
            fixture.components.root(fixture.component_ids[2]),
            fixture.fragments[2],
            fixture.interaction_ids[2],
            visuals(0.2F)));
        fixture.buttons.synchronize_gpu(gpu);
        require(gpu.capacity() == 4 * ryn::component::button_visual_layer_count
                    && api.creates == 2,
                "Button Quad GPU buffer did not retain growth capacity");

        auto changed = visuals(0.1F);
        const auto changed_segment =
            ryn::component::button_loading_segment_index(0);
        changed[changed_segment].color = {0.8F, 0.1F, 0.2F, 1.0F};
        static_cast<void>(fixture.buttons.update(second, changed));
        fixture.buttons.synchronize_gpu(gpu);
        require(api.upload_offsets.back()
                    == (ryn::component::button_visual_layer_count + changed_segment)
                        * sizeof(ryn::graphics::QuadInstance)
                && api.upload_sizes.back()
                    == sizeof(ryn::graphics::QuadInstance),
            "Button Material change did not use a one-instance GPU upload");

        changed[1].opacity = 0.5F;
        static_cast<void>(fixture.buttons.update(second, changed));
        api.fail_next = true;
        bool failed = false;
        try {
            fixture.buttons.synchronize_gpu(gpu);
        } catch (const std::runtime_error&) {
            failed = true;
        }
        require(failed
                    && !fixture.buttons.instances().material_dirty_ranges().empty(),
                "failed Button GPU upload discarded dirty state");
        fixture.buttons.synchronize_gpu(gpu);
        require(fixture.buttons.instances().material_dirty_ranges().empty(),
                "successful Button GPU retry retained dirty state");
    }
    require(api.live.empty() && api.releases == api.creates,
            "Button Quad GPU buffers leaked across growth/teardown");
    static_cast<void>(first);
}

} // namespace

int main() {
    try {
        test_fixed_ranges_compaction_and_shared_hit_order();
        test_gpu_capacity_sparse_upload_and_failure_retention();
    } catch (const std::exception& error) {
        std::cerr << error.what() << '\n';
        return 1;
    }
    return 0;
}
