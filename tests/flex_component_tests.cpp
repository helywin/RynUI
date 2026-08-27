#include "component/flex_component.hpp"
#include "runtime/frame_scheduler.hpp"

#include <ryn/rynui.hpp>

#include <exception>
#include <iostream>
#include <stdexcept>
#include <thread>
#include <vector>

namespace {

void require(bool condition, const char* message) {
    if (!condition) {
        throw std::runtime_error(message);
    }
}

struct FirstLeafState final {
    ryn::runtime::NodeId node;
};

struct SecondLeafState final {
    ryn::runtime::NodeId node;
};

template <typename State> ryn::runtime::ComponentId Leaf(ryn::runtime::Size size) {
    auto& services = ryn::detail::require_layout_component_services();
    auto& build = ryn::runtime::require_component_build_context();
    const auto component = build.mount_component<State>();
    const auto node = build.root(component);
    build.state<State>(component).node = node;
    services.layout.set_layout(node, ryn::layout::LeafLayout{size});
    build.on_resource_cleanup(component, [layout = &services.layout, node] {
        static_cast<void>(layout->remove_layout(node));
    });
    return component;
}

struct Fixture final {
    Fixture()
        : layout(nodes), dirty(nodes, &frames), components(nodes),
          services{nodes, layout, dirty} {}

    template <typename Function> void mount(Function&& function) {
        ryn::detail::ActiveLayoutComponentServices guard(services);
        components.mount(ryn::Content{std::forward<Function>(function)});
    }

    void clear() {
        dirty.clear();
        static_cast<void>(frames.consume_request());
    }

    ryn::runtime::NodeStore nodes;
    ryn::layout::LayoutEngine layout;
    ryn::runtime::FrameRequestState frames;
    ryn::runtime::DirtyQueues dirty;
    ryn::runtime::ComponentHost components;
    ryn::detail::LayoutComponentServices services;
};

void test_mount_topology_and_lifecycle() {
    bool missing_services = false;
    try {
        ryn::Flex(ryn::FlexProps{}, [] {});
    } catch (const std::logic_error&) {
        missing_services = true;
    }
    require(missing_services, "Flex outside layout component services was accepted");

    Fixture no_build;
    bool missing_build = false;
    try {
        ryn::detail::ActiveLayoutComponentServices guard(no_build.services);
        ryn::Flex(ryn::FlexProps{}, [] {});
    } catch (const std::logic_error&) {
        missing_build = true;
    }
    require(missing_build, "Flex outside ComponentHost build context was accepted");

    Fixture fixture;
    int content_runs = 0;
    ryn::runtime::ComponentId first_leaf;
    ryn::runtime::ComponentId nested_flex;
    ryn::runtime::ComponentId second_leaf;
    fixture.mount([&] {
        ryn::Flex(ryn::FlexProps{}.gap(ryn::SpaceSize::Small), [&] {
            ++content_runs;
            first_leaf = Leaf<FirstLeafState>({20.0F, 10.0F});
            ryn::Flex(ryn::FlexProps{}.vertical(true).gap(ryn::dp(3.0F)), [&] {
                ++content_runs;
                second_leaf = Leaf<SecondLeafState>({30.0F, 12.0F});
            });
            nested_flex =
                fixture.components.children(fixture.components.root_components().front())[1];
        });
        ryn::Flex(ryn::FlexProps{}.layout(ryn::LayoutStyle{}.width(ryn::dp(42.0F))),
                  [&] { ++content_runs; });
    });

    const auto roots = fixture.components.root_components();
    require(content_runs == 3 && fixture.components.mount_runs() == 1 &&
                fixture.components.component_count() == 5 && roots.size() == 2 &&
                fixture.components.children(roots[0]).size() == 2 &&
                fixture.components.parent(first_leaf) == roots[0] &&
                fixture.components.parent(nested_flex) == roots[0] &&
                fixture.components.parent(second_leaf) == nested_flex &&
                fixture.nodes.require(fixture.components.root(roots[0])).children.size() == 2 &&
                fixture.nodes.require(fixture.components.root(roots[1])).external_layout.width ==
                    42.0F &&
                fixture.components.paint_traversal().empty(),
            "Flex did not preserve heterogeneous, nested, empty, or invisible topology");
    require(fixture.components.state<FirstLeafState>(first_leaf) != nullptr &&
                fixture.components.state<SecondLeafState>(second_leaf) != nullptr &&
                fixture.components.state<ryn::detail::FlexComponentState>(roots[0]) != nullptr,
            "Flex erased typed child component state");

    const auto root_node = fixture.components.root(roots[0]);
    const auto measured = fixture.layout.layout(root_node, {0.0F, 200.0F, 0.0F, 100.0F});
    require(measured.width == 58.0F && measured.height == 12.0F,
            "Small Theme gap did not resolve to 8 logical pixels");

    Fixture throwing;
    bool slot_exception = false;
    try {
        throwing.mount([] {
            ryn::Flex(ryn::FlexProps{}, [] {
                static_cast<void>(Leaf<FirstLeafState>({10.0F, 10.0F}));
                throw std::runtime_error("Flex slot failure");
            });
        });
    } catch (const std::runtime_error&) {
        slot_exception = true;
    }
    require(slot_exception && throwing.nodes.size() == 0 &&
                throwing.components.component_count() == 0,
            "throwing Flex content leaked retained resources");

    Fixture wrong_thread;
    std::exception_ptr thread_error;
    std::thread worker([&] {
        try {
            wrong_thread.mount([] { ryn::Flex(ryn::FlexProps{}, [] {}); });
        } catch (...) {
            thread_error = std::current_exception();
        }
    });
    worker.join();
    require(thread_error != nullptr && wrong_thread.nodes.size() == 0 &&
                wrong_thread.components.component_count() == 0,
            "wrong-thread Flex mount changed retained state");
}

void test_reactive_phases_identity_and_cleanup() {
    Fixture fixture;
    ryn::Signal<bool> vertical{false};
    ryn::Signal<bool> wrap{false};
    ryn::Signal<ryn::FlexJustify> justify{ryn::FlexJustify::Start};
    ryn::Signal<ryn::FlexAlign> align{ryn::FlexAlign::Start};
    ryn::Signal<ryn::LayoutGap> gap{ryn::LayoutGap{ryn::dp(0.0F)}};
    int content_runs = 0;
    ryn::runtime::ComponentId first;
    ryn::runtime::ComponentId second;
    fixture.mount([&] {
        ryn::Flex(
            ryn::FlexProps{}.vertical(vertical).wrap(wrap).justify(justify).align(align).gap(gap),
            [&] {
                ++content_runs;
                first = Leaf<FirstLeafState>({20.0F, 10.0F});
                second = Leaf<SecondLeafState>({30.0F, 20.0F});
            });
    });
    const auto flex = fixture.components.root_components().front();
    const auto root = fixture.components.root(flex);
    const auto first_node = fixture.components.root(first);
    const auto second_node = fixture.components.root(second);
    static_cast<void>(fixture.layout.layout(root, ryn::layout::Constraints::fixed(100.0F, 40.0F)));
    const auto initial_diagnostics = fixture.layout.flex_layout_diagnostics(root);
    const auto first_measure_count = fixture.nodes.require(first_node).measure_count;
    const auto second_measure_count = fixture.nodes.require(second_node).measure_count;
    const auto component_count = fixture.components.component_count();
    fixture.clear();

    justify.set(ryn::FlexJustify::Center);
    require(fixture.dirty.layout_roots().empty() &&
                fixture.dirty.placement_roots() == std::vector{root} &&
                fixture.dirty.geometry_nodes() == std::vector{root} &&
                fixture.dirty.hit_test_nodes() == std::vector{root},
            "Flex justify escaped placement-only invalidation");
    fixture.layout.place(root);
    require(fixture.nodes.require(first_node).bounds.x == 25.0F &&
                fixture.nodes.require(first_node).measure_count == first_measure_count &&
                fixture.nodes.require(second_node).measure_count == second_measure_count &&
                fixture.layout.flex_layout_diagnostics(root).item_capacity ==
                    initial_diagnostics.item_capacity,
            "Flex justify discarded measurement or reusable scratch");
    fixture.clear();

    align.set(ryn::FlexAlign::End);
    require(fixture.dirty.layout_roots().empty() &&
                fixture.dirty.placement_roots() == std::vector{root},
            "Flex align escaped placement-only invalidation");
    fixture.layout.place(root);
    require(fixture.nodes.require(first_node).bounds.y == 30.0F,
            "Flex align did not reuse measurement for cross placement");
    fixture.clear();

    gap.set(ryn::LayoutGap{ryn::dp(4.0F), ryn::dp(6.0F)});
    require(fixture.dirty.layout_roots() == std::vector{root} &&
                fixture.dirty.placement_roots().empty() &&
                fixture.dirty.geometry_nodes() == std::vector{root} &&
                fixture.dirty.hit_test_nodes() == std::vector{root},
            "Flex gap did not invalidate only its target measure subtree");
    static_cast<void>(fixture.layout.layout(root, ryn::layout::Constraints::fixed(100.0F, 40.0F)));
    require(fixture.components.state<ryn::detail::FlexComponentState>(flex)->model.main_gap ==
                    4.0F &&
                fixture.components.state<ryn::detail::FlexComponentState>(flex)->model.cross_gap ==
                    6.0F,
            "custom dual-axis Flex gap was not retained");
    fixture.clear();
    gap.set(ryn::LayoutGap{ryn::SpaceSize::Middle});
    require(fixture.components.state<ryn::detail::FlexComponentState>(flex)->model.main_gap ==
                    16.0F &&
                fixture.components.state<ryn::detail::FlexComponentState>(flex)->model.cross_gap ==
                    16.0F,
            "Middle Flex gap did not resolve from the 16-pixel Theme token");
    static_cast<void>(fixture.layout.layout(root, ryn::layout::Constraints::fixed(100.0F, 40.0F)));
    fixture.clear();

    vertical.set(true);
    require(fixture.dirty.layout_roots() == std::vector{root},
            "Flex direction did not request target subtree measurement");
    static_cast<void>(fixture.layout.layout(root, ryn::layout::Constraints::fixed(100.0F, 80.0F)));
    fixture.clear();
    wrap.set(true);
    require(fixture.dirty.layout_roots() == std::vector{root},
            "Flex wrap did not request target subtree measurement");

    require(content_runs == 1 && fixture.components.component_count() == component_count &&
                fixture.components.children(flex) == std::vector({first, second}) &&
                fixture.components.paint_traversal().empty(),
            "reactive Flex Props reran content or changed scene/component identity");
    fixture.clear();
    wrap.set(true);
    require(!fixture.frames.pending() && fixture.dirty.layout_roots().empty() &&
                fixture.dirty.placement_roots().empty(),
            "equal Flex update requested an idle frame");

    const auto model_before_invalid =
        fixture.components.state<ryn::detail::FlexComponentState>(flex)->model;
    bool invalid_justify = false;
    try {
        justify.set(static_cast<ryn::FlexJustify>(255));
    } catch (const std::invalid_argument&) {
        invalid_justify = true;
    }
    require(invalid_justify &&
                fixture.components.state<ryn::detail::FlexComponentState>(flex)->model ==
                    model_before_invalid &&
                !fixture.frames.pending() && fixture.dirty.layout_roots().empty() &&
                fixture.dirty.placement_roots().empty(),
            "invalid reactive Flex value changed model or dirty state");

    const auto stale_node = root;
    require(fixture.components.destroy(flex), "Flex destroy failed");
    fixture.clear();
    vertical.set(false);
    gap.set(ryn::LayoutGap{ryn::SpaceSize::Large});
    require(!fixture.frames.pending(), "destroyed Flex Prop subscription remained active");
    const auto replacement = fixture.nodes.create_root();
    require(replacement.index == stale_node.index &&
                replacement.generation != stale_node.generation,
            "Flex destroy/reuse fixture did not reuse the node slot");
    bool stale_layout_removed = false;
    try {
        static_cast<void>(
            fixture.layout.layout(replacement, ryn::layout::Constraints::fixed(10.0F, 10.0F)));
    } catch (const std::logic_error&) {
        stale_layout_removed = true;
    }
    require(stale_layout_removed, "destroyed Flex layout leaked into a reused NodeId");
}

void test_theme_preset_gap_updates_only_subscribed_flex() {
    Fixture fixture;
    ryn::Signal<ryn::ThemeConfig> config{ryn::ThemeConfig{}};
    int content_runs = 0;
    fixture.mount([&] {
        ryn::Theme(
            ryn::ThemeProps{}.config(config),
            ryn::ThemeContent{[&] {
                ryn::Flex(ryn::FlexProps{}.gap(ryn::SpaceSize::Small), [&] {
                    ++content_runs;
                    static_cast<void>(Leaf<FirstLeafState>({10.0F, 10.0F}));
                });
                ryn::Flex(ryn::FlexProps{}.gap(ryn::dp(7.0F)), [&] {
                    ++content_runs;
                    static_cast<void>(Leaf<SecondLeafState>({10.0F, 10.0F}));
                });
            }});
    });
    const auto roots = fixture.components.root_components();
    require(roots.size() == 2, "themed Flex fixture lost transparent Theme roots");
    const auto preset = roots[0];
    const auto custom = roots[1];
    const auto preset_node = fixture.components.root(preset);
    const auto custom_node = fixture.components.root(custom);
    fixture.clear();

    auto resized = ryn::ThemeConfig{};
    resized.seed.size_unit = ryn::dp(5.0F);
    require(config.set(resized)
                && fixture.dirty.layout_roots()
                    == std::vector<ryn::runtime::NodeId>{preset_node}
                && fixture.components.state<ryn::detail::FlexComponentState>(preset)
                    ->model.main_gap == 10.0F
                && fixture.components.state<ryn::detail::FlexComponentState>(custom)
                    ->model.main_gap == 7.0F
                && fixture.nodes.require(custom_node).measure_count == 0,
            "Flex preset Theme gap changed custom gap or invalidated the wrong subtree");
    require(content_runs == 2 && fixture.components.component_count() == 4,
            "Flex Theme gap update reran content or rebuilt component identity");

    fixture.clear();
    resized.seed.color_primary = ryn::Color::rgba8(114, 46, 209);
    require(config.set(resized)
                && fixture.dirty.layout_roots().empty()
                && !fixture.frames.pending(),
            "unrelated Theme color notified Flex gap subscribers");
}

} // namespace

int main() {
    try {
        test_mount_topology_and_lifecycle();
        test_reactive_phases_identity_and_cleanup();
        test_theme_preset_gap_updates_only_subscribed_flex();
    } catch (const std::exception& error) {
        std::cerr << error.what() << '\n';
        return 1;
    }
    return 0;
}
