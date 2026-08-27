#include "input/interaction_registry.hpp"
#include "runtime/invalidation.hpp"

#include <ryn/component.hpp>

#include <array>
#include <iostream>
#include <stdexcept>

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

} // namespace

int main() {
    try {
        test_dirty_refresh_is_minimal_and_observable();
        test_stale_component_is_counted_and_skipped();
    } catch (const std::exception& error) {
        std::cerr << error.what() << '\n';
        return 1;
    }
    return 0;
}
