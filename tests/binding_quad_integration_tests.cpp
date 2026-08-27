#include "graphics/quad_scene.hpp"
#include "layout/layout_engine.hpp"
#include "runtime/invalidation.hpp"

#include <ryn/reactive.hpp>

#include <algorithm>
#include <cmath>
#include <iostream>
#include <stdexcept>
#include <vector>

namespace {

void require(bool condition, const char* message) {
    if (!condition) {
        throw std::runtime_error(message);
    }
}

bool near(float left, float right) {
    return std::abs(left - right) < 0.00001F;
}

class RecordingUploadApi final : public ryn::graphics::QuadUploadApi {
public:
    ryn::graphics::QuadGpuBufferHandle create_vertex_buffer(std::size_t size) override {
        buffer.assign(size, std::byte{});
        return this;
    }

    void release_buffer(ryn::graphics::QuadGpuBufferHandle) noexcept override {}

    bool upload(
        ryn::graphics::QuadGpuBufferHandle handle,
        std::size_t offset,
        std::span<const std::byte> bytes) override {
        if (handle != this || offset + bytes.size() > buffer.size()) {
            error = "invalid fake upload";
            return false;
        }
        upload_offsets.push_back(offset);
        std::copy(bytes.begin(), bytes.end(), buffer.begin() + offset);
        return true;
    }

    const char* last_error() const noexcept override {
        return error.c_str();
    }

    std::vector<std::byte> buffer;
    std::vector<std::size_t> upload_offsets;
    std::string error;
};

void test_bindings_update_only_the_target_quad_range() {
    constexpr ryn::runtime::Size viewport{400.0F, 200.0F};
    ryn::runtime::NodeStore nodes;
    const auto root = nodes.create_root();
    const auto first = nodes.create_child(root);
    const auto second = nodes.create_child(root);
    ryn::layout::LayoutEngine layout(nodes);
    ryn::layout::FlexLayout flex;
    flex.main_gap = 10.0F;
    layout.set_layout(root, flex);
    layout.set_layout(first, ryn::layout::LeafLayout{{100.0F, 80.0F}});
    layout.set_layout(second, ryn::layout::LeafLayout{{100.0F, 80.0F}});
    static_cast<void>(layout.layout(root, ryn::layout::Constraints::fixed(
        viewport.width,
        viewport.height)));

    ryn::runtime::DirtyQueues dirty(nodes);
    ryn::runtime::NodePropertyWriter properties(nodes, dirty);
    static_cast<void>(properties.set_color(first, {0.1F, 0.3F, 0.9F, 1.0F}));
    static_cast<void>(properties.set_color(second, {0.9F, 0.2F, 0.1F, 1.0F}));
    dirty.clear();

    ryn::graphics::QuadScene scene(nodes);
    const auto first_quad = scene.add_quad(first, viewport, 8.0F);
    const auto second_quad = scene.add_quad(second, viewport, 12.0F);
    require(first_quad.instance_index == 0 && second_quad.instance_index == 1,
            "Quad instance ordering is incorrect");

    RecordingUploadApi upload_api;
    ryn::graphics::QuadGpuBuffer gpu_buffer(upload_api, scene.instances());
    const std::vector<std::byte> original_first(
        upload_api.buffer.begin(),
        upload_api.buffer.begin() + sizeof(ryn::graphics::QuadInstance));
    const auto initial_measure = nodes.require(second).measure_count;
    const auto initial_place = nodes.require(second).place_count;

    ryn::Signal<ryn::runtime::Color> color{{0.9F, 0.2F, 0.1F, 1.0F}};
    ryn::Signal<float> opacity{1.0F};
    ryn::Signal<ryn::runtime::Point> translation{{0.0F, 0.0F}};
    ryn::Scope scope;
    const auto color_connection = ryn::connect_binding(
        scope,
        ryn::bind([&] { return color.get(); }),
        [&](ryn::runtime::Color value) {
            static_cast<void>(properties.set_color(second, value));
        });
    const auto opacity_connection = ryn::connect_binding(
        scope,
        ryn::bind([&] { return opacity.get(); }),
        [&](float value) {
            static_cast<void>(properties.set_opacity(second, value));
        });
    const auto translation_connection = ryn::connect_binding(
        scope,
        ryn::bind([&] { return translation.get(); }),
        [&](ryn::runtime::Point value) {
            static_cast<void>(properties.set_translation(second, value));
        });
    require(color_connection.active()
                && opacity_connection.active()
                && translation_connection.active(),
            "Binding connection is inactive after mount");
    require(dirty.material_nodes().empty() && dirty.transform_nodes().empty(),
            "equal initial Binding values dirtied the Node");

    ryn::batch([&] {
        color.set({0.2F, 0.8F, 0.4F, 0.9F});
        opacity.set(0.6F);
        translation.set({10.0F, 5.0F});
    });

    require(dirty.material_nodes() == std::vector<ryn::runtime::NodeId>({second}),
            "Binding Material updates did not target only the second Node");
    require(dirty.transform_nodes() == std::vector<ryn::runtime::NodeId>({second}),
            "Binding Transform update did not target only the second Node");
    require(dirty.layout_roots().empty() && dirty.geometry_nodes().empty(),
            "Material/Transform Binding update expanded into Layout/Geometry");

    const auto updated = scene.sync_dirty(dirty, gpu_buffer, viewport);
    require(updated == 1 && scene.counters().instance_updates == 1,
            "dirty queues did not coalesce to one Quad instance update");
    require(gpu_buffer.counters().range_uploads == 1,
            "Quad update did not issue exactly one range upload");
    require(upload_api.upload_offsets == std::vector<std::size_t>({
                0,
                sizeof(ryn::graphics::QuadInstance),
            }),
            "Quad update uploaded the wrong instance byte range");
    require(std::equal(
                original_first.begin(),
                original_first.end(),
                upload_api.buffer.begin()),
            "unrelated first Quad bytes changed");

    const auto& instance = scene.instances().at(second_quad.instance_index);
    require(instance.color == std::array<float, 4>{0.2F, 0.8F, 0.4F, 0.9F}
                && instance.opacity == 0.6F,
            "Material Binding did not reach the target Quad instance");
    require(near(instance.translation[0], 0.05F)
                && near(instance.translation[1], -0.05F),
            "Transform Binding did not reach the target Quad instance");
    require(nodes.require(second).measure_count == initial_measure
                && nodes.require(second).place_count == initial_place,
            "targeted Quad update reran Measure or Place");
    require(scene.counters().primitive_rebuilds == 2,
            "targeted update rebuilt Quad topology");
}

} // namespace

int main() {
    try {
        test_bindings_update_only_the_target_quad_range();
    } catch (const std::exception& error) {
        std::cerr << error.what() << '\n';
        return 1;
    }
    return 0;
}
