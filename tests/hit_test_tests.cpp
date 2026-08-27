#include "input/interaction_registry.hpp"
#include "layout/layout_engine.hpp"

#include <ryn/component.hpp>

#include <array>
#include <iostream>
#include <stdexcept>

namespace {

struct TestState final {};
struct ChildrenSlot final {};
using Children = ryn::SlotContent<ChildrenSlot>;

ryn::runtime::ComponentId mount_leaf() {
    return ryn::runtime::require_component_build_context()
        .mount_component<TestState>();
}

ryn::runtime::ComponentId mount_parent(const Children& children) {
    auto& context = ryn::runtime::require_component_build_context();
    const auto parent = context.mount_component<TestState>();
    context.mount_slot(parent, children);
    return parent;
}

void require(bool condition, const char* message) {
    if (!condition) {
        throw std::runtime_error(message);
    }
}

void commit(
    ryn::runtime::NodeStore& nodes,
    ryn::runtime::NodeId node,
    ryn::runtime::Rect bounds,
    ryn::runtime::Point translation = {}) {
    auto& retained = nodes.require(node);
    retained.bounds = bounds;
    retained.translation = translation;
    retained.place_generation = 1;
}

ryn::input::InteractionId register_interaction(
    ryn::input::InteractionRegistry& registry,
    ryn::runtime::ComponentHost& components,
    ryn::runtime::ComponentId component,
    std::optional<ryn::input::InteractionId> parent = std::nullopt,
    bool eligible = true) {
    return registry.create({
        component,
        components.root(component),
        parent,
        eligible,
        false,
        {},
    });
}

void test_nested_translation_clip_and_visual_child_delegation() {
    ryn::runtime::NodeStore nodes;
    ryn::runtime::ComponentHost components(nodes);
    ryn::runtime::ComponentId parent_component;
    ryn::runtime::ComponentId child_component;
    components.mount(ryn::Content{[&] {
        parent_component = mount_parent(Children{[&] {
            child_component = mount_leaf();
        }});
    }});
    const auto visual_child = nodes.create_child(components.root(child_component));

    ryn::input::InteractionRegistry registry(components, nodes);
    const auto parent = register_interaction(
        registry, components, parent_component);
    const auto child = register_interaction(
        registry, components, child_component, parent);
    commit(nodes, components.root(parent_component), {0.0F, 0.0F, 100.0F, 100.0F});
    commit(
        nodes,
        components.root(child_component),
        {10.0F, 10.0F, 50.0F, 50.0F},
        {5.0F, 7.0F});
    commit(nodes, visual_child, {15.0F, 17.0F, 20.0F, 20.0F});

    ryn::input::HitTestSnapshot snapshot(registry, nodes);
    const std::array entries{
        ryn::input::HitTestPaintEntry{parent, ryn::runtime::Rect{0.0F, 0.0F, 40.0F, 40.0F}},
        ryn::input::HitTestPaintEntry{child, std::nullopt},
    };
    snapshot.rebuild(entries, {0.0F, 0.0F, 80.0F, 80.0F});

    require(snapshot.records().size() == 2
                && snapshot.records()[1].depth == 1,
            "nested HitTest depth differs");
    require(snapshot.records()[1].translated_bounds
                == ryn::runtime::Rect{15.0F, 17.0F, 50.0F, 50.0F},
            "committed translation was not applied to hit bounds");
    require(snapshot.records()[1].effective_clip
                == ryn::runtime::Rect{0.0F, 0.0F, 40.0F, 40.0F},
            "ancestor/window effective clip differs");
    require(snapshot.hit_test({20.0F, 20.0F}) == child,
            "deepest interactive target was not selected");
    require(snapshot.hit_test({35.0F, 35.0F}) == child,
            "pure visual child stole hit ownership from its interaction ancestor");
    require(!snapshot.hit_test({45.0F, 25.0F}).has_value(),
            "point outside effective clip was accepted");
}

void test_paint_order_eligibility_and_half_open_boundaries() {
    ryn::runtime::NodeStore nodes;
    ryn::runtime::ComponentHost components(nodes);
    ryn::runtime::ComponentId first_component;
    ryn::runtime::ComponentId second_component;
    components.mount(ryn::Content{[&] {
        first_component = mount_leaf();
        second_component = mount_leaf();
    }});
    commit(nodes, components.root(first_component), {10.0F, 10.0F, 30.0F, 30.0F});
    commit(nodes, components.root(second_component), {10.0F, 10.0F, 30.0F, 30.0F});
    nodes.require(components.root(first_component)).external_layout.order = 10;
    nodes.require(components.root(second_component)).external_layout.order = -10;

    ryn::input::InteractionRegistry registry(components, nodes);
    const auto first = register_interaction(
        registry, components, first_component);
    const auto second = register_interaction(
        registry, components, second_component);
    ryn::input::HitTestSnapshot snapshot(registry, nodes);
    const std::array entries{
        ryn::input::HitTestPaintEntry{first, std::nullopt},
        ryn::input::HitTestPaintEntry{second, std::nullopt},
    };
    snapshot.rebuild(entries, {0.0F, 0.0F, 100.0F, 100.0F});

    require(snapshot.hit_test({20.0F, 20.0F}) == second,
            "last-painted overlapping sibling did not win");
    require(registry.set_eligible(second, false),
            "eligibility update was suppressed");
    require(snapshot.refresh_interaction(second) == 1,
            "eligibility refresh touched the wrong range");
    require(snapshot.hit_test({20.0F, 20.0F}) == first,
            "disabled top sibling blocked the eligible sibling");
    require(!snapshot.hit_test({40.0F, 20.0F}).has_value(),
            "right edge was not treated as half-open");
    require(!snapshot.hit_test({20.0F, 40.0F}).has_value(),
            "bottom edge was not treated as half-open");
}

void test_margin_committed_bounds_and_missing_layout() {
    ryn::runtime::NodeStore nodes;
    ryn::runtime::ComponentHost components(nodes);
    ryn::runtime::ComponentId parent_component;
    ryn::runtime::ComponentId child_component;
    ryn::runtime::ComponentId unplaced_component;
    components.mount(ryn::Content{[&] {
        parent_component = mount_parent(Children{[&] {
            child_component = mount_leaf();
        }});
        unplaced_component = mount_leaf();
    }});

    ryn::layout::LayoutEngine layout(nodes);
    const auto parent_node = components.root(parent_component);
    const auto child_node = components.root(child_component);
    layout.set_layout(parent_node, ryn::layout::BoxLayout{});
    layout.set_layout(child_node, ryn::layout::LeafLayout{{80.0F, 80.0F}});
    nodes.require(child_node).external_layout.margin = {10.0F, 10.0F, 10.0F, 10.0F};
    static_cast<void>(layout.layout(
        parent_node,
        ryn::layout::Constraints::fixed(100.0F, 100.0F)));

    ryn::input::InteractionRegistry registry(components, nodes);
    const auto child = register_interaction(
        registry, components, child_component);
    const auto unplaced = register_interaction(
        registry, components, unplaced_component);
    ryn::input::HitTestSnapshot snapshot(registry, nodes);
    const std::array entries{
        ryn::input::HitTestPaintEntry{child, std::nullopt},
        ryn::input::HitTestPaintEntry{unplaced, std::nullopt},
    };
    snapshot.rebuild(entries, {0.0F, 0.0F, 100.0F, 100.0F});

    require(nodes.require(child_node).bounds
                == ryn::runtime::Rect{10.0F, 10.0F, 80.0F, 80.0F},
            "layout setup did not commit margin-adjusted bounds");
    require(!snapshot.hit_test({5.0F, 5.0F}).has_value(),
            "margin area was included in component hit bounds");
    require(snapshot.hit_test({15.0F, 15.0F}) == child,
            "margin-adjusted committed bounds were not hittable");
    require(!snapshot.records()[1].has_committed_layout,
            "unplaced Node was marked as committed");
}

void test_wrapped_flex_commits_hit_test_bounds() {
    ryn::runtime::NodeStore nodes;
    ryn::runtime::ComponentHost components(nodes);
    ryn::runtime::ComponentId parent_component;
    std::array<ryn::runtime::ComponentId, 3> child_components;
    components.mount(ryn::Content{[&] {
        parent_component = mount_parent(Children{[&] {
            for (auto& child : child_components) {
                child = mount_leaf();
            }
        }});
    }});

    const auto parent_node = components.root(parent_component);
    ryn::layout::LayoutEngine layout(nodes);
    ryn::layout::FlexLayout flex;
    flex.main_gap = 5.0F;
    flex.cross_gap = 7.0F;
    flex.wrap = ryn::layout::FlexWrap::wrap;
    layout.set_layout(parent_node, flex);
    for (const auto component : child_components) {
        layout.set_layout(
            components.root(component),
            ryn::layout::LeafLayout{{20.0F, 10.0F}});
    }
    nodes.require(components.root(child_components[2])).external_layout.order = -1;
    static_cast<void>(layout.layout(parent_node, ryn::layout::Constraints::fixed(50.0F, 40.0F)));

    ryn::input::InteractionRegistry registry(components, nodes);
    std::array<ryn::input::HitTestPaintEntry, 3> entries;
    std::array<ryn::input::InteractionId, 3> interactions;
    for (std::size_t index = 0; index < child_components.size(); ++index) {
        interactions[index] = register_interaction(
            registry,
            components,
            child_components[index]);
        entries[index] = {interactions[index], std::nullopt};
    }
    ryn::input::HitTestSnapshot snapshot(registry, nodes);
    snapshot.rebuild(entries, {0.0F, 0.0F, 50.0F, 40.0F});

    require(nodes.require(components.root(child_components[2])).bounds ==
                    ryn::runtime::Rect{0.0F, 0.0F, 20.0F, 10.0F} &&
                nodes.require(components.root(child_components[1])).bounds ==
                    ryn::runtime::Rect{0.0F, 17.0F, 20.0F, 10.0F} &&
                snapshot.hit_test({5.0F, 1.0F}) == interactions[2] &&
                snapshot.hit_test({30.0F, 1.0F}) == interactions[0] &&
                snapshot.hit_test({5.0F, 18.0F}) == interactions[1] &&
                !snapshot.hit_test({25.0F, 18.0F}).has_value(),
            "Flex layout order changed paint identity or stale HitTest bounds");
}

void test_stale_snapshot_entries_and_invalid_traversals_are_safe() {
    ryn::runtime::NodeStore nodes;
    ryn::runtime::ComponentHost components(nodes);
    ryn::runtime::ComponentId parent_component;
    ryn::runtime::ComponentId child_component;
    components.mount(ryn::Content{[&] {
        parent_component = mount_parent(Children{[&] {
            child_component = mount_leaf();
        }});
    }});
    commit(nodes, components.root(parent_component), {0.0F, 0.0F, 50.0F, 50.0F});
    commit(nodes, components.root(child_component), {0.0F, 0.0F, 50.0F, 50.0F});

    ryn::input::InteractionRegistry registry(components, nodes);
    const auto parent = register_interaction(
        registry, components, parent_component);
    const auto child = register_interaction(
        registry, components, child_component, parent);
    ryn::input::HitTestSnapshot snapshot(registry, nodes);

    bool child_first_rejected = false;
    try {
        const std::array child_first{
            ryn::input::HitTestPaintEntry{child, std::nullopt},
            ryn::input::HitTestPaintEntry{parent, std::nullopt},
        };
        snapshot.rebuild(child_first, {0.0F, 0.0F, 100.0F, 100.0F});
    } catch (const std::invalid_argument&) {
        child_first_rejected = true;
    }
    require(child_first_rejected,
            "child-before-parent paint traversal was accepted");

    bool duplicate_rejected = false;
    try {
        const std::array duplicates{
            ryn::input::HitTestPaintEntry{parent, std::nullopt},
            ryn::input::HitTestPaintEntry{parent, std::nullopt},
        };
        snapshot.rebuild(duplicates, {0.0F, 0.0F, 100.0F, 100.0F});
    } catch (const std::invalid_argument&) {
        duplicate_rejected = true;
    }
    require(duplicate_rejected, "duplicate paint traversal entry was accepted");

    const std::array valid_entries{
        ryn::input::HitTestPaintEntry{parent, std::nullopt},
        ryn::input::HitTestPaintEntry{child, std::nullopt},
    };
    snapshot.rebuild(valid_entries, {0.0F, 0.0F, 100.0F, 100.0F});
    require(registry.remove(child), "stale snapshot setup remove failed");
    const auto replacement = register_interaction(
        registry, components, child_component, parent);
    require(replacement.index == child.index
                && replacement.generation != child.generation,
            "stale snapshot setup did not reuse the interaction slot");
    require(snapshot.hit_test({10.0F, 10.0F}) == parent,
            "reused interaction slot aliased the stale child snapshot");
    require(snapshot.diagnostics().stale_skips == 1,
            "stale snapshot skip was not diagnosed");

    const std::array replacement_entries{
        ryn::input::HitTestPaintEntry{parent, std::nullopt},
        ryn::input::HitTestPaintEntry{replacement, std::nullopt},
    };
    snapshot.rebuild(replacement_entries, {0.0F, 0.0F, 100.0F, 100.0F});
    require(snapshot.hit_test({10.0F, 10.0F}) == replacement,
            "replacement interaction was not hittable after snapshot rebuild");
}

} // namespace

int main() {
    try {
        test_nested_translation_clip_and_visual_child_delegation();
        test_paint_order_eligibility_and_half_open_boundaries();
        test_margin_committed_bounds_and_missing_layout();
        test_wrapped_flex_commits_hit_test_bounds();
        test_stale_snapshot_entries_and_invalid_traversals_are_safe();
    } catch (const std::exception& error) {
        std::cerr << error.what() << '\n';
        return 1;
    }
    return 0;
}
