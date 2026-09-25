#include "input/interaction_registry.hpp"
#include "runtime/invalidation.hpp"
#include "support/allocation_probe.hpp"

#include <ryn/component.hpp>

#include <array>
#include <iostream>
#include <stdexcept>
#include <vector>

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

void commit(
    ryn::runtime::NodeStore& nodes,
    ryn::runtime::NodeId id,
    ryn::runtime::Rect bounds) {
    auto& node = nodes.require(id);
    node.bounds = bounds;
    node.place_generation = 1;
}

void test_dirty_refresh_is_minimal_and_observable() {
    ryn::runtime::NodeStore nodes;
    ryn::runtime::ComponentHost components(nodes);
    ryn::runtime::ComponentId first_component;
    ryn::runtime::ComponentId second_component;
    components.mount(ryn::Content{[&] {
        first_component = mount_leaf();
        second_component = mount_leaf();
    }});
    const auto first_node = components.root(first_component);
    const auto second_node = components.root(second_component);
    commit(nodes, first_node, {0.0F, 0.0F, 20.0F, 20.0F});
    commit(nodes, second_node, {100.0F, 0.0F, 20.0F, 20.0F});

    ryn::input::InteractionRegistry registry(components, nodes);
    const auto first = registry.create({
        first_component, first_node, std::nullopt, true, true, {}});
    const auto second = registry.create({
        second_component, second_node, std::nullopt, true, true, {}});
    ryn::input::HitTestSnapshot snapshot(registry, nodes);
    const std::array paint_entries{
        ryn::input::HitTestPaintEntry{first, std::nullopt},
        ryn::input::HitTestPaintEntry{second, std::nullopt},
    };
    snapshot.rebuild(paint_entries, {0.0F, 0.0F, 200.0F, 100.0F});
    require(snapshot.diagnostics().snapshot_rebuilds == 1
                && snapshot.diagnostics().records_refreshed == 2,
            "initial HitTest snapshot diagnostics differ");

    ryn::runtime::DirtyQueues dirty(nodes);
    ryn::runtime::NodePropertyWriter properties(nodes, dirty);
    require(properties.set_color(first_node, {0.2F, 0.3F, 0.4F, 1.0F}),
            "material setup update was suppressed");
    require(dirty.hit_test_nodes().empty(),
            "Material-only update dirtied HitTest");
    require(snapshot.refresh(dirty.hit_test_nodes()) == 0,
            "Material-only update refreshed HitTest records");
    require(snapshot.diagnostics().records_refreshed == 2,
            "Material-only update changed refresh diagnostics");

    dirty.clear();
    dirty.invalidate(first_node, ryn::runtime::DirtyFlags::Geometry);
    require(dirty.hit_test_nodes().empty(),
            "Geometry-only update dirtied HitTest without bounds changes");
    dirty.clear();

    require(properties.set_translation(first_node, {30.0F, 0.0F}),
            "translation setup update was suppressed");
    require(dirty.hit_test_nodes()
                == std::vector<ryn::runtime::NodeId>({first_node}),
            "translation queued the wrong HitTest range");
    require(snapshot.refresh(dirty.hit_test_nodes()) == 1,
            "translation refreshed more than its target interaction");
    require(!snapshot.hit_test({5.0F, 5.0F}).has_value()
                && snapshot.hit_test({35.0F, 5.0F}) == first,
            "translation refresh did not update hit geometry");

    dirty.clear();
    require(properties.set_size(first_node, {50.0F, 30.0F}),
            "size setup update was suppressed");
    require(dirty.hit_test_nodes()
                == std::vector<ryn::runtime::NodeId>({first_node}),
            "size update did not queue its layout subtree root");
    commit(nodes, first_node, {0.0F, 0.0F, 50.0F, 30.0F});
    require(snapshot.refresh(dirty.hit_test_nodes()) == 1,
            "committed size refreshed more than its subtree");

    require(registry.set_eligible(first, false),
            "eligibility setup update was suppressed");
    require(snapshot.refresh_interaction(first) == 1,
            "eligibility refreshed more than its interaction subtree");
    require(!snapshot.hit_test({10.0F, 10.0F}).has_value(),
            "ineligible interaction remained hittable");

    dirty.clear();
    dirty.invalidate(first_node, ryn::runtime::DirtyFlags::Structure);
    require(dirty.hit_test_nodes()
                == std::vector<ryn::runtime::NodeId>({first_node}),
            "Structure update did not queue a HitTest subtree refresh");

    const auto diagnostics = snapshot.diagnostics();
    require(diagnostics.records_refreshed == 5,
            "minimal HitTest refresh count differs");
    require(diagnostics.queries == 3
                && diagnostics.hits == 1,
            "HitTest query/hit diagnostics differ");
}

void test_stale_component_is_counted_and_skipped() {
    ryn::runtime::NodeStore nodes;
    ryn::runtime::ComponentHost components(nodes);
    ryn::runtime::ComponentId component;
    components.mount(ryn::Content{[&] { component = mount_leaf(); }});
    const auto node = components.root(component);
    commit(nodes, node, {0.0F, 0.0F, 20.0F, 20.0F});

    ryn::input::InteractionRegistry registry(components, nodes);
    const auto interaction = registry.create({
        component, node, std::nullopt, true, false, {}});
    ryn::input::HitTestSnapshot snapshot(registry, nodes);
    const std::array paint_entries{
        ryn::input::HitTestPaintEntry{interaction, std::nullopt},
    };
    snapshot.rebuild(paint_entries, {0.0F, 0.0F, 100.0F, 100.0F});
    require(components.destroy(component), "stale component setup destroy failed");
    require(!snapshot.hit_test({5.0F, 5.0F}).has_value(),
            "stale component remained hittable");
    require(snapshot.diagnostics().stale_skips == 1,
            "stale HitTest record was not diagnosed");
}

void test_large_batch_preserves_ancestor_and_unrelated_records() {
    ryn::runtime::NodeStore nodes;
    ryn::runtime::ComponentHost components(nodes);
    ryn::runtime::ComponentId component;
    components.mount(ryn::Content{[&] { component = mount_leaf(); }});
    const auto root = components.root(component);
    const auto ancestor = nodes.create_child(root);
    const auto child = nodes.create_child(ancestor);
    const auto outside = nodes.create_child(root);
    commit(nodes, ancestor, {0.0F, 0.0F, 40.0F, 40.0F});
    commit(nodes, child, {5.0F, 5.0F, 10.0F, 10.0F});
    commit(nodes, outside, {80.0F, 0.0F, 10.0F, 10.0F});

    ryn::input::InteractionRegistry registry(components, nodes);
    const auto parent_interaction = registry.create({
        component, ancestor, std::nullopt, true, false, {}});
    const auto child_interaction = registry.create({
        component, child, parent_interaction, true, false, {}});
    const auto outside_interaction = registry.create({
        component, outside, std::nullopt, true, false, {}});
    ryn::input::HitTestSnapshot snapshot(registry, nodes);
    const std::array entries{
        ryn::input::HitTestPaintEntry{parent_interaction, std::nullopt},
        ryn::input::HitTestPaintEntry{child_interaction, std::nullopt},
        ryn::input::HitTestPaintEntry{outside_interaction, std::nullopt},
    };
    snapshot.rebuild(entries, {0.0F, 0.0F, 100.0F, 100.0F});

    std::vector<ryn::runtime::NodeId> unrelated;
    for (int index = 0; index < 9; ++index) {
        unrelated.push_back(nodes.create_child(root));
    }
    require(snapshot.refresh(unrelated) == 0,
            "large unrelated batch refreshed HitTest records");

    auto dirty = unrelated;
    dirty.push_back(child);
    dirty.push_back(ancestor);
    dirty.push_back(ancestor);
    require(registry.set_eligible(parent_interaction, false),
            "parent eligibility setup failed");
    require(snapshot.refresh(dirty) == 2,
            "large ancestor batch did not refresh each affected record once");
    require(!snapshot.hit_test({6.0F, 6.0F}).has_value()
                && snapshot.hit_test({85.0F, 5.0F}) == outside_interaction,
            "large batch lost ancestor eligibility or unrelated hit");

    require(registry.set_eligible(parent_interaction, true),
            "parent eligibility restore failed");
    require(snapshot.refresh(dirty) == 2
                && snapshot.hit_test({6.0F, 6.0F}) == child_interaction,
            "next batch retained stale epoch or lost child hit");
}

void test_large_batch_distinguishes_reused_slot_and_small_batch_allocations() {
    ryn::runtime::NodeStore nodes;
    ryn::runtime::ComponentHost components(nodes);
    ryn::runtime::ComponentId component;
    components.mount(ryn::Content{[&] { component = mount_leaf(); }});
    const auto root = components.root(component);
    const auto stale = nodes.create_child(root);
    std::vector<ryn::runtime::NodeId> unrelated;
    for (int index = 0; index < 9; ++index) {
        unrelated.push_back(nodes.create_child(root));
    }
    require(nodes.destroy(stale), "stale Node setup destroy failed");
    const auto replacement = nodes.create_child(root);
    require(replacement.index == stale.index
                && replacement.generation != stale.generation,
            "Node slot was not reused with a new generation");
    commit(nodes, replacement, {0.0F, 0.0F, 10.0F, 10.0F});

    ryn::input::InteractionRegistry registry(components, nodes);
    const auto interaction = registry.create({
        component, replacement, std::nullopt, true, false, {}});
    ryn::input::HitTestSnapshot snapshot(registry, nodes);
    const std::array entries{
        ryn::input::HitTestPaintEntry{interaction, std::nullopt},
    };
    snapshot.rebuild(entries, {0.0F, 0.0F, 100.0F, 100.0F});

    auto dirty = unrelated;
    dirty.push_back(stale);
    require(snapshot.refresh(dirty) == 0,
            "stale generation dirtied reused Node record");
    dirty.push_back(replacement);
    require(snapshot.refresh(dirty) == 1,
            "mixed old and new generations lost live Node refresh");

    const std::array single{replacement};
    ryn_test::allocation::begin();
    const auto refreshed = snapshot.refresh(single);
    const auto allocations = ryn_test::allocation::end();
    require(refreshed == 1 && allocations == 0,
            "single-node HitTest refresh allocated or lost the record");
}

} // namespace

int main() {
    try {
        test_dirty_refresh_is_minimal_and_observable();
        test_stale_component_is_counted_and_skipped();
        test_large_batch_preserves_ancestor_and_unrelated_records();
        test_large_batch_distinguishes_reused_slot_and_small_batch_allocations();
    } catch (const std::exception& error) {
        std::cerr << error.what() << '\n';
        return 1;
    }
    return 0;
}
