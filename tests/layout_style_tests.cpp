#include "layout/layout_engine.hpp"
#include "runtime/component_host.hpp"
#include "runtime/frame_scheduler.hpp"
#include "runtime/layout_style_adapter.hpp"

#include <ryn/layout_style.hpp>

#include <iostream>
#include <limits>
#include <stdexcept>
#include <thread>
#include <utility>
#include <vector>

namespace {

void require(bool condition, const char* message) {
    if (!condition) {
        throw std::runtime_error(message);
    }
}

struct LayoutComponentState final {};
struct LayoutChildrenSlot final {};
using LayoutChildren = ryn::SlotContent<LayoutChildrenSlot>;

ryn::runtime::ComponentId mount_layout_component(const LayoutChildren* children = nullptr) {
    auto& context = ryn::runtime::require_component_build_context();
    const auto component = context.mount_component<LayoutComponentState>();
    if (children != nullptr) {
        context.mount_slot(component, *children);
    }
    return component;
}

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

    const auto expect_invalid_flex = [&](ryn::LayoutStyle style) {
        bool diagnosed = false;
        try {
            ryn::runtime::connect_layout_style(scope, style, node, nodes, dirty);
        } catch (const std::invalid_argument&) {
            diagnosed = true;
        }
        require(diagnosed, "invalid Flex child LayoutStyle was accepted");
        require(nodes.require(node).external_layout == ryn::runtime::ExternalLayoutStyle{},
                "invalid Flex child LayoutStyle partially changed metadata");
    };
    ryn::LayoutStyle invalid_grow;
    invalid_grow.flex_grow(-1.0F);
    expect_invalid_flex(std::move(invalid_grow));
    ryn::LayoutStyle nan_grow;
    nan_grow.flex_grow(std::numeric_limits<float>::quiet_NaN());
    expect_invalid_flex(std::move(nan_grow));
    ryn::LayoutStyle infinite_grow;
    infinite_grow.flex_grow(std::numeric_limits<float>::infinity());
    expect_invalid_flex(std::move(infinite_grow));
    ryn::LayoutStyle negative_shrink;
    negative_shrink.flex_shrink(-1.0F);
    expect_invalid_flex(std::move(negative_shrink));
    ryn::LayoutStyle nan_shrink;
    nan_shrink.flex_shrink(std::numeric_limits<float>::quiet_NaN());
    expect_invalid_flex(std::move(nan_shrink));
    ryn::LayoutStyle invalid_shrink;
    invalid_shrink.flex_shrink(std::numeric_limits<float>::infinity());
    expect_invalid_flex(std::move(invalid_shrink));
    ryn::LayoutStyle negative_basis;
    negative_basis.flex_basis(ryn::dp(-1.0F));
    expect_invalid_flex(std::move(negative_basis));
    ryn::LayoutStyle invalid_basis;
    invalid_basis.flex_basis(ryn::dp(std::numeric_limits<float>::quiet_NaN()));
    expect_invalid_flex(std::move(invalid_basis));
    ryn::LayoutStyle infinite_basis;
    infinite_basis.flex_basis(ryn::dp(std::numeric_limits<float>::infinity()));
    expect_invalid_flex(std::move(infinite_basis));
    ryn::LayoutStyle invalid_align;
    invalid_align.align_self(static_cast<ryn::FlexAlignSelf>(99));
    expect_invalid_flex(std::move(invalid_align));
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

void test_flex_item_props_target_only_the_direct_parent_subtree() {
    ryn::runtime::NodeStore nodes;
    const auto root = nodes.create_root();
    const auto flex = nodes.create_child(root);
    const auto first = nodes.create_child(flex);
    const auto second = nodes.create_child(flex);
    const auto unrelated = nodes.create_child(root);
    const auto declaration_order = nodes.require(flex).children;
    ryn::runtime::FrameRequestState frames;
    ryn::runtime::DirtyQueues dirty(nodes, &frames);
    ryn::Scope scope;
    ryn::Signal<float> grow{1.0F};
    ryn::Signal<float> shrink{1.0F};
    ryn::Signal<ryn::LogicalLength> basis{ryn::dp(20.0F)};
    ryn::Signal<ryn::FlexAlignSelf> align{ryn::FlexAlignSelf::automatic};
    ryn::Signal<int> order{0};
    ryn::LayoutStyle style;
    style.flex_grow(grow).flex_shrink(shrink).flex_basis(basis).align_self(align).order(order);
    ryn::runtime::connect_layout_style(scope, style, first, nodes, dirty);
    require(nodes.require(first).external_layout.flex_grow == 1.0F &&
                nodes.require(first).external_layout.flex_shrink == 1.0F &&
                nodes.require(first).external_layout.flex_basis == 20.0F,
            "initial Flex child props did not reach retained metadata");
    require(dirty.layout_roots() == std::vector<ryn::runtime::NodeId>({flex}) &&
                dirty.geometry_nodes() == std::vector<ryn::runtime::NodeId>({flex}) &&
                dirty.hit_test_nodes() == std::vector<ryn::runtime::NodeId>({flex}),
            "initial Flex child props escaped their direct parent subtree");
    dirty.clear();
    static_cast<void>(frames.consume_request());

    bool invalid_grow_diagnosed = false;
    try {
        grow.set(-1.0F);
    } catch (const std::invalid_argument&) {
        invalid_grow_diagnosed = true;
    }
    require(invalid_grow_diagnosed && nodes.require(first).external_layout.flex_grow == 1.0F &&
                dirty.layout_roots().empty() && !frames.pending(),
            "invalid reactive Flex grow changed retained metadata or Dirty state");

    grow.set(2.0F);
    shrink.set(0.5F);
    basis.set(ryn::auto_length);
    require(nodes.require(first).external_layout.flex_grow == 2.0F &&
                nodes.require(first).external_layout.flex_shrink == 0.5F &&
                !nodes.require(first).external_layout.flex_basis.has_value() &&
                dirty.layout_roots() == std::vector<ryn::runtime::NodeId>({flex}) &&
                dirty.geometry_nodes() == std::vector<ryn::runtime::NodeId>({flex}),
            "reactive grow, shrink, or auto basis missed local Flex invalidation");
    dirty.clear();
    static_cast<void>(frames.consume_request());
    basis.set(ryn::dp(30.0F));
    require(nodes.require(first).external_layout.flex_basis == 30.0F &&
                dirty.layout_roots() == std::vector<ryn::runtime::NodeId>({flex}),
            "reactive fixed Flex basis did not restore retained metadata");
    dirty.clear();
    static_cast<void>(frames.consume_request());

    order.set(-2);
    require(nodes.require(first).external_layout.order == -2 &&
                dirty.layout_roots() == std::vector<ryn::runtime::NodeId>({flex}) &&
                dirty.geometry_nodes() == std::vector<ryn::runtime::NodeId>({flex}) &&
                dirty.hit_test_nodes() == std::vector<ryn::runtime::NodeId>({flex}) &&
                nodes.require(flex).children == declaration_order &&
                nodes.require(root).children ==
                    std::vector<ryn::runtime::NodeId>({flex, unrelated}) &&
                nodes.find(second) != nullptr,
            "reactive order changed structure or dirtied a sibling subtree");
    dirty.clear();
    static_cast<void>(frames.consume_request());

    align.set(ryn::FlexAlignSelf::end);
    require(dirty.layout_roots().empty() &&
                dirty.placement_roots() == std::vector<ryn::runtime::NodeId>({flex}) &&
                dirty.geometry_nodes() == std::vector<ryn::runtime::NodeId>({flex}) &&
                dirty.hit_test_nodes() == std::vector<ryn::runtime::NodeId>({flex}),
            "align-self update repeated measurement or escaped its subtree");
}

void test_flex_item_reactivity_respects_owner_and_component_lifecycle() {
    ryn::runtime::NodeStore nodes;
    ryn::runtime::ComponentHost host(nodes);
    ryn::runtime::FrameRequestState frames;
    ryn::runtime::DirtyQueues dirty(nodes, &frames);
    ryn::Signal<int> order{0};
    ryn::runtime::ComponentId parent;
    ryn::runtime::ComponentId child;
    ryn::runtime::SceneFragmentId fragment;

    host.mount(ryn::Content{[&] {
        const LayoutChildren children{[&] {
            child = mount_layout_component();
            auto& context = ryn::runtime::require_component_build_context();
            fragment = context.register_scene_fragment(
                child, ryn::runtime::SceneFragmentPlacement::before_children);
            ryn::LayoutStyle style;
            style.order(order);
            ryn::runtime::connect_layout_style(context.scope(child), style, context.root(child),
                                               nodes, dirty);
        }};
        parent = mount_layout_component(&children);
    }});
    const auto parent_node = host.root(parent);
    const auto child_node = host.root(child);
    const auto component_children = host.children(parent);
    const auto node_children = nodes.require(parent_node).children;
    const auto initial_paint_span = host.paint_traversal();
    const std::vector<ryn::runtime::SceneFragmentPaintEntry> initial_paint(
        initial_paint_span.begin(), initial_paint_span.end());
    dirty.clear();
    static_cast<void>(frames.consume_request());

    order.set(3);
    const auto updated_paint_span = host.paint_traversal();
    const std::vector<ryn::runtime::SceneFragmentPaintEntry> updated_paint(
        updated_paint_span.begin(), updated_paint_span.end());
    require(host.mount_runs() == 1 && host.component_count() == 2 &&
                host.children(parent) == component_children &&
                nodes.require(parent_node).children == node_children &&
                host.root(child) == child_node && host.contains(fragment) &&
                updated_paint == initial_paint,
            "Flex child update remounted content or rebuilt topology");
    dirty.clear();
    static_cast<void>(frames.consume_request());

    bool wrong_thread_rejected = false;
    std::thread worker([&] {
        try {
            order.set(4);
        } catch (const std::logic_error&) {
            wrong_thread_rejected = true;
        }
    });
    worker.join();
    require(wrong_thread_rejected && order.get() == 3 &&
                nodes.require(child_node).external_layout.order == 3 &&
                dirty.layout_roots().empty() && dirty.geometry_nodes().empty() && !frames.pending(),
            "wrong-thread Flex child update mutated retained state");

    require(host.destroy(parent), "Flex lifecycle parent destroy failed");
    order.set(5);
    require(nodes.size() == 0 && host.component_count() == 0 && dirty.layout_roots().empty() &&
                dirty.geometry_nodes().empty() && !frames.pending(),
            "destroyed Flex child still observed Signal writes");
}

} // namespace

int main() {
    try {
        test_initial_style_is_atomic_and_creates_no_wrapper();
        test_reactive_style_uses_minimal_invalidation();
        test_invalid_reactive_update_keeps_previous_metadata();
        test_flex_item_props_target_only_the_direct_parent_subtree();
        test_flex_item_reactivity_respects_owner_and_component_lifecycle();
    } catch (const std::exception& error) {
        std::cerr << error.what() << '\n';
        return 1;
    }
    return 0;
}
