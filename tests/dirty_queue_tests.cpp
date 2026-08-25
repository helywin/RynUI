#include "layout/layout_engine.hpp"
#include "runtime/invalidation.hpp"

#include <iostream>
#include <stdexcept>

namespace {

void require(bool condition, const char* message) {
    if (!condition) {
        throw std::runtime_error(message);
    }
}

void test_material_and_transform_updates_skip_layout() {
    ryn::runtime::NodeStore nodes;
    const auto root = nodes.create_root();
    const auto child = nodes.create_child(root);
    ryn::layout::LayoutEngine layout(nodes);
    layout.set_layout(root, ryn::layout::BoxLayout{});
    layout.set_layout(child, ryn::layout::LeafLayout{{20.0F, 10.0F}});
    static_cast<void>(layout.layout(root, {0.0F, 100.0F, 0.0F, 100.0F}));

    const auto root_measure = nodes.require(root).measure_count;
    const auto root_place = nodes.require(root).place_count;
    const auto child_measure = nodes.require(child).measure_count;
    const auto child_place = nodes.require(child).place_count;
    ryn::runtime::DirtyQueues dirty(nodes);
    ryn::runtime::NodePropertyWriter properties(nodes, dirty);

    require(properties.set_color(child, {0.2F, 0.4F, 0.8F, 1.0F}),
            "color update was suppressed");
    require(properties.set_opacity(child, 0.5F), "opacity update was suppressed");
    require(dirty.material_nodes() == std::vector<ryn::runtime::NodeId>({child}),
            "Material queue did not deduplicate the target Node");
    require(dirty.layout_roots().empty(), "Material update queued Layout");
    require(dirty.geometry_nodes().empty(), "Material update queued Geometry");
    require(dirty.transform_nodes().empty(), "Material update queued Transform");

    dirty.clear();
    require(properties.set_translation(child, {5.0F, 7.0F}),
            "translation update was suppressed");
    require(dirty.transform_nodes() == std::vector<ryn::runtime::NodeId>({child}),
            "translation did not queue the target Transform");
    require(dirty.layout_roots().empty(), "Transform update queued Layout");
    require(ryn::runtime::has_any(
                ryn::runtime::dirty_flags_for(ryn::runtime::NodeProperty::translation),
                ryn::runtime::DirtyFlags::HitTest),
            "translation mapping omitted HitTest");

    require(nodes.require(root).measure_count == root_measure
                && nodes.require(root).place_count == root_place
                && nodes.require(child).measure_count == child_measure
                && nodes.require(child).place_count == child_place,
            "Material or Transform update changed Measure/Layout counters");
}

void test_size_update_queues_layout_root_and_geometry() {
    ryn::runtime::NodeStore nodes;
    const auto root = nodes.create_root();
    const auto child = nodes.create_child(root);
    ryn::layout::LayoutEngine layout(nodes);
    layout.set_layout(root, ryn::layout::BoxLayout{});
    layout.set_layout(child, ryn::layout::LeafLayout{{20.0F, 10.0F}});
    static_cast<void>(layout.layout(root, {0.0F, 100.0F, 0.0F, 100.0F}));
    const auto previous_measure = nodes.require(child).measure_count;
    const auto previous_place = nodes.require(child).place_count;

    ryn::runtime::DirtyQueues dirty(nodes);
    ryn::runtime::NodePropertyWriter properties(nodes, dirty);
    require(properties.set_size(child, {50.0F, 30.0F}), "size update was suppressed");
    require(!properties.set_size(child, {50.0F, 30.0F}),
            "equal size update was not suppressed");
    require(dirty.layout_roots() == std::vector<ryn::runtime::NodeId>({root}),
            "size update did not queue the affected layout root exactly once");
    require(dirty.geometry_nodes() == std::vector<ryn::runtime::NodeId>({child}),
            "size update did not queue target Geometry exactly once");
    require(dirty.material_nodes().empty(), "size update queued Material");
    require(dirty.transform_nodes().empty(), "size update queued Transform");

    for (const auto layout_root : dirty.layout_roots()) {
        static_cast<void>(layout.layout(layout_root, {0.0F, 100.0F, 0.0F, 100.0F}));
    }
    require(nodes.require(child).measured_size == ryn::runtime::Size{50.0F, 30.0F},
            "size update was not consumed by the next layout pass");
    require(nodes.require(child).measure_count == previous_measure + 1
                && nodes.require(child).place_count == previous_place + 1,
            "size update did not rerun Measure and Place");
}

} // namespace

int main() {
    try {
        test_material_and_transform_updates_skip_layout();
        test_size_update_queues_layout_root_and_geometry();
    } catch (const std::exception& error) {
        std::cerr << error.what() << '\n';
        return 1;
    }
    return 0;
}
