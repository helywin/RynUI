#include "layout/layout_engine.hpp"
#include "runtime/component_host.hpp"
#include "runtime/frame_scheduler.hpp"
#include "runtime/layout_style_adapter.hpp"

#include <ryn/layout_style.hpp>

#include <iostream>
#include <limits>
#include <stdexcept>

namespace {

void require(bool condition, const char* message) {
    if (!condition) {
        throw std::runtime_error(message);
    }
}

struct LayoutComponentState final {};

void test_initial_style_is_atomic_and_creates_no_wrapper() {
    ryn::runtime::NodeStore nodes;
    const auto node = nodes.create_root();
    ryn::runtime::FrameRequestState frames;
    ryn::runtime::DirtyQueues dirty(nodes, &frames);
    ryn::Scope scope;

    ryn::LayoutStyle invalid;
    invalid.min_width(ryn::dp(100.0F)).max_width(ryn::dp(50.0F));
    bool min_max_diagnosed = false;
    try {
        ryn::runtime::connect_layout_style(
            scope,
            invalid,
            node,
            nodes,
            dirty);
    } catch (const std::invalid_argument&) {
        min_max_diagnosed = true;
    }
    require(min_max_diagnosed, "inverted LayoutStyle min/max was accepted");
    require(nodes.require(node).external_layout == ryn::runtime::ExternalLayoutStyle{},
            "invalid initial LayoutStyle partially changed Node metadata");
    require(nodes.size() == 1,
            "LayoutStyle created a wrapper Node");
    require(dirty.layout_roots().empty()
                && dirty.placement_roots().empty()
                && dirty.geometry_nodes().empty()
                && !frames.pending(),
            "invalid initial LayoutStyle marked Dirty or requested a frame");

    ryn::LayoutStyle auto_margin;
    auto_margin.margin_left(ryn::auto_length);
    bool auto_margin_diagnosed = false;
    try {
        ryn::runtime::connect_layout_style(
            scope,
            auto_margin,
            node,
            nodes,
            dirty);
    } catch (const std::invalid_argument&) {
        auto_margin_diagnosed = true;
    }
    require(auto_margin_diagnosed, "auto margin entered Node metadata");
    require(nodes.require(node).external_layout == ryn::runtime::ExternalLayoutStyle{},
            "invalid auto margin partially changed Node metadata");

    ryn::LayoutStyle negative;
    negative.height(ryn::dp(-1.0F));
    bool negative_diagnosed = false;
    try {
        ryn::runtime::connect_layout_style(
            scope,
            negative,
            node,
            nodes,
            dirty);
    } catch (const std::invalid_argument&) {
        negative_diagnosed = true;
    }
    require(negative_diagnosed, "negative LayoutStyle height was accepted");
    require(nodes.require(node).external_layout == ryn::runtime::ExternalLayoutStyle{},
            "negative initial LayoutStyle partially changed Node metadata");
}

void test_reactive_style_uses_minimal_invalidation() {
    ryn::runtime::NodeStore nodes;
    ryn::runtime::ComponentHost host(nodes);
    ryn::runtime::FrameRequestState frames;
    ryn::runtime::DirtyQueues dirty(nodes, &frames);
    ryn::layout::LayoutEngine layout(nodes);
    ryn::Signal<ryn::LogicalLength> width{ryn::dp(80.0F)};
    ryn::Signal<ryn::LogicalLength> margin{ryn::dp(4.0F)};
    ryn::runtime::ComponentId component;

    host.mount(ryn::Content{[&] {
        auto& context = ryn::runtime::require_component_build_context();
        component = context.mount_component<LayoutComponentState>();
        layout.set_layout(component.valid() ? context.root(component) : ryn::runtime::NodeId{},
                          ryn::layout::LeafLayout{{30.0F, 20.0F}});
        ryn::LayoutStyle style;
        style.width(width).margin_left(margin);
        ryn::runtime::connect_layout_style(
            context.scope(component),
            style,
            context.root(component),
            nodes,
            dirty);
    }});

    const auto node = host.root(component);
    require(nodes.size() == 1 && host.component_count() == 1,
            "LayoutStyle added a wrapper component or Node");
    require(dirty.layout_roots() == std::vector<ryn::runtime::NodeId>({node})
                && dirty.placement_roots().empty(),
            "initial width did not request Measure/Layout exactly once");
    dirty.clear();
    require(frames.consume_request(), "initial LayoutStyle frame was not requested");
    static_cast<void>(layout.layout(
        node,
        {0.0F, 200.0F, 0.0F, 100.0F}));
    const auto initial_measure_count = nodes.require(node).measure_count;
    const auto initial_place_count = nodes.require(node).place_count;

    require(!width.set(ryn::dp(80.0F)),
            "equal LayoutStyle width reported a change");
    require(dirty.layout_roots().empty()
                && dirty.placement_roots().empty()
                && !frames.pending(),
            "equal LayoutStyle width expanded invalidation");

    margin.set(ryn::dp(10.0F));
    require(dirty.layout_roots().empty()
                && dirty.placement_roots()
                    == std::vector<ryn::runtime::NodeId>({node})
                && dirty.geometry_nodes()
                    == std::vector<ryn::runtime::NodeId>({node}),
            "pure margin update did not stay in Placement/Geometry");
    require(dirty.material_nodes().empty(),
            "margin update changed Material state");
    require(frames.consume_request(), "margin update did not request a frame");
    dirty.clear();
    layout.place(node);
    require(nodes.require(node).measure_count == initial_measure_count
                && nodes.require(node).place_count == initial_place_count + 1,
            "pure margin update repeated measurement");
    require(nodes.require(node).bounds
                == ryn::runtime::Rect{10.0F, 0.0F, 80.0F, 20.0F},
            "margin update did not change content placement");

    width.set(ryn::dp(60.0F));
    require(dirty.layout_roots() == std::vector<ryn::runtime::NodeId>({node})
                && dirty.placement_roots().empty(),
            "width update did not request Measure/Layout");
    require(host.mount_runs() == 1,
            "reactive LayoutStyle update reran Host content");
    static_cast<void>(layout.layout(
        node,
        {0.0F, 200.0F, 0.0F, 100.0F}));
    require(nodes.require(node).measured_size.width == 60.0F,
            "reactive width did not constrain measurement");
}

void test_invalid_reactive_update_keeps_previous_metadata() {
    ryn::runtime::NodeStore nodes;
    const auto node = nodes.create_root();
    ryn::runtime::FrameRequestState frames;
    ryn::runtime::DirtyQueues dirty(nodes, &frames);
    ryn::Scope scope;
    ryn::Signal<ryn::LogicalLength> maximum{ryn::dp(80.0F)};
    ryn::LayoutStyle style;
    style.min_width(ryn::dp(20.0F)).max_width(maximum);
    ryn::runtime::connect_layout_style(scope, style, node, nodes, dirty);
    dirty.clear();
    static_cast<void>(frames.consume_request());

    bool inverted_diagnosed = false;
    try {
        maximum.set(ryn::dp(10.0F));
    } catch (const std::invalid_argument&) {
        inverted_diagnosed = true;
    }
    require(inverted_diagnosed, "invalid reactive min/max update was accepted");
    require(nodes.require(node).external_layout.max_width == 80.0F,
            "invalid reactive update changed existing Node metadata");
    require(dirty.layout_roots().empty() && !frames.pending(),
            "invalid reactive update marked Dirty or requested a frame");

    bool nan_diagnosed = false;
    try {
        maximum.set(ryn::dp(std::numeric_limits<float>::quiet_NaN()));
    } catch (const std::invalid_argument&) {
        nan_diagnosed = true;
    }
    require(nan_diagnosed, "NaN reactive LayoutStyle update was accepted");
    require(nodes.require(node).external_layout.max_width == 80.0F,
            "NaN reactive update changed existing Node metadata");

    maximum.set(ryn::dp(90.0F));
    require(nodes.require(node).external_layout.max_width == 90.0F
                && dirty.layout_roots()
                    == std::vector<ryn::runtime::NodeId>({node}),
            "valid update did not recover after an invalid value");
}

} // namespace

int main() {
    try {
        test_initial_style_is_atomic_and_creates_no_wrapper();
        test_reactive_style_uses_minimal_invalidation();
        test_invalid_reactive_update_keeps_previous_metadata();
    } catch (const std::exception& error) {
        std::cerr << error.what() << '\n';
        return 1;
    }
    return 0;
}
