#include "component/button_component.hpp"
#include "component/space_component.hpp"
#include "runtime/frame_scheduler.hpp"
#include "runtime/layout_style_adapter.hpp"

#include <ryn/rynui.hpp>

#include <cmath>
#include <exception>
#include <iostream>
#include <memory>
#include <stdexcept>
#include <thread>
#include <vector>

namespace {

void require(bool condition, const char* message) {
    if (!condition) {
        throw std::runtime_error(message);
    }
}

bool near(float left, float right) {
    return std::abs(left - right) < 0.0001F;
}

struct FirstLeafState final {
    ryn::runtime::NodeId node;
};

struct SecondLeafState final {
    ryn::runtime::NodeId node;
};

struct ThirdLeafState final {
    ryn::runtime::NodeId node;
};

template <typename State>
ryn::runtime::ComponentId Leaf(
    ryn::runtime::Size size,
    ryn::LayoutStyle style = {}) {
    auto& services = ryn::detail::require_layout_component_services();
    auto& build = ryn::runtime::require_component_build_context();
    const auto component = build.mount_component<State>();
    const auto node = build.root(component);
    build.state<State>(component).node = node;
    services.layout.set_layout(node, ryn::layout::LeafLayout{size});
    build.on_resource_cleanup(component, [layout = &services.layout, node] {
        static_cast<void>(layout->remove_layout(node));
    });
    ryn::runtime::connect_layout_style(
        build.scope(component),
        style,
        node,
        services.nodes,
        services.dirty);
    return component;
}

struct LayoutFixture final {
    LayoutFixture()
        : layout(nodes),
          dirty(nodes, &frames),
          components(nodes),
          services{nodes, layout, dirty} {}

    template <typename Function>
    void mount(Function&& function) {
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

struct VisualFixture final {
    VisualFixture()
        : layout(nodes),
          dirty(nodes, &frames),
          fonts(create_runtime()),
          engine(*fonts),
          text_scene(*fonts, engine, frames) {
        const auto latin = fonts->load_font_file(RYNUI_VALIDATION_LATIN_FONT, 0, 14);
        const auto cjk = fonts->load_font_file(RYNUI_VALIDATION_CJK_FONT, 0, 14);
        require(latin && cjk, "Space composition fonts failed to load");
        chain = {latin.font, cjk.font};
        host = std::make_unique<ryn::detail::ButtonComponentHost>(
            nodes,
            layout,
            dirty,
            text_scene,
            chain,
            frames);
    }

    static std::unique_ptr<ryn::font::FontRuntime> create_runtime() {
        auto created = ryn::font::FontRuntime::create();
        require(static_cast<bool>(created), "Font Runtime initialization failed");
        return std::move(created.runtime);
    }

    bool synchronize(float width, float height = 240.0F) {
        return host->layout_and_synchronize(
            {width, height},
            {0.0F, 0.0F, width, height});
    }

    ryn::runtime::NodeStore nodes;
    ryn::layout::LayoutEngine layout;
    ryn::runtime::FrameRequestState frames;
    ryn::runtime::DirtyQueues dirty;
    std::unique_ptr<ryn::font::FontRuntime> fonts;
    ryn::text::TextEngine engine;
    ryn::detail::TextSceneService text_scene;
    std::vector<ryn::font::FontIdentity> chain;
    std::unique_ptr<ryn::detail::ButtonComponentHost> host;
};

ryn::input::KeyboardInputEvent tab_key() {
    return {
        ryn::input::Key::tab,
        ryn::input::KeyAction::down,
        ryn::input::KeyModifier::none,
        false,
    };
}

void test_mount_defaults_policy_and_lifecycle() {
    bool missing_services = false;
    try {
        ryn::Space(ryn::SpaceProps{}, [] {});
    } catch (const std::logic_error&) {
        missing_services = true;
    }
    require(missing_services, "Space outside layout component services was accepted");

    LayoutFixture no_build;
    bool missing_build = false;
    try {
        ryn::detail::ActiveLayoutComponentServices guard(no_build.services);
        ryn::Space(ryn::SpaceProps{}, [] {});
    } catch (const std::logic_error&) {
        missing_build = true;
    }
    require(missing_build, "Space outside ComponentHost build context was accepted");

    LayoutFixture fixture;
    int content_runs = 0;
    ryn::runtime::ComponentId first;
    ryn::runtime::ComponentId second;
    ryn::runtime::ComponentId third;
    ryn::runtime::ComponentId nested;
    fixture.mount([&] {
        ryn::Space(ryn::SpaceProps{}, [&] {
            ++content_runs;
            first = Leaf<FirstLeafState>(
                {10.0F, 10.0F},
                ryn::LayoutStyle{}
                    .flex_grow(4.0F)
                    .flex_shrink(8.0F)
                    .flex_basis(ryn::dp(50.0F))
                    .align_self(ryn::FlexAlignSelf::end)
                    .order(10));
            second = Leaf<SecondLeafState>(
                {20.0F, 20.0F},
                ryn::LayoutStyle{}.order(-10));
            ryn::Space(ryn::SpaceProps{}.vertical(true).size(ryn::SpaceSize::Middle), [&] {
                ++content_runs;
                third = Leaf<ThirdLeafState>({12.0F, 12.0F});
            });
            nested = fixture.components.children(
                fixture.components.root_components().front())[2];
        });
        ryn::Space(ryn::SpaceProps{}.size(ryn::SpaceSize::Large), [&] {
            ++content_runs;
        });
    });

    const auto roots = fixture.components.root_components();
    require(content_runs == 3 && fixture.components.mount_runs() == 1
                && fixture.components.component_count() == 6
                && roots.size() == 2
                && fixture.components.children(roots[0]) == std::vector({first, second, nested})
                && fixture.components.parent(third) == nested
                && fixture.nodes.require(fixture.components.root(roots[0])).children.size() == 3
                && fixture.components.paint_traversal().empty(),
            "Space added wrappers or changed heterogeneous/nested/empty topology");

    const auto* root_state =
        fixture.components.state<ryn::detail::SpaceComponentState>(roots[0]);
    const auto* nested_state =
        fixture.components.state<ryn::detail::SpaceComponentState>(nested);
    const auto* empty_state =
        fixture.components.state<ryn::detail::SpaceComponentState>(roots[1]);
    require(root_state != nullptr && nested_state != nullptr && empty_state != nullptr
                && root_state->model.direction == ryn::layout::FlexDirection::horizontal
                && root_state->model.wrap == ryn::layout::FlexWrap::no_wrap
                && root_state->model.align == ryn::layout::FlexAlign::start
                && root_state->model.main_gap == 8.0F
                && root_state->model.cross_gap == 8.0F
                && root_state->model.item_policy == ryn::layout::FlexItemPolicy::sequential
                && nested_state->model.main_gap == 16.0F
                && empty_state->model.main_gap == 24.0F,
            "Space defaults or shared Theme size presets were not resolved");

    const auto root_node = fixture.components.root(roots[0]);
    const auto first_node = fixture.components.root(first);
    const auto second_node = fixture.components.root(second);
    static_cast<void>(fixture.layout.layout(
        root_node,
        ryn::layout::Constraints::fixed(100.0F, 40.0F)));
    require(fixture.nodes.require(first_node).bounds.x == 0.0F
                && fixture.nodes.require(first_node).bounds.y == 0.0F
                && fixture.nodes.require(first_node).bounds.width == 10.0F
                && fixture.nodes.require(second_node).bounds.x == 18.0F
                && fixture.nodes.require(second_node).bounds.width == 20.0F,
            "Space allowed grow/shrink/basis/align-self/order to affect sequential items");

    LayoutFixture custom;
    custom.mount([] {
        ryn::Space(
            ryn::SpaceProps{}.vertical(true).wrap(true).size(ryn::dp(3.0F), ryn::dp(5.0F)),
            [] {});
    });
    const auto* custom_state = custom.components.state<ryn::detail::SpaceComponentState>(
        custom.components.root_components().front());
    require(custom_state != nullptr
                && custom_state->model.direction == ryn::layout::FlexDirection::vertical
                && custom_state->model.wrap == ryn::layout::FlexWrap::wrap
                && custom_state->model.main_gap == 3.0F
                && custom_state->model.cross_gap == 5.0F,
            "Space custom dual-axis size or orientation did not reach its model");

    LayoutFixture throwing;
    bool slot_exception = false;
    try {
        throwing.mount([] {
            ryn::Space(ryn::SpaceProps{}, [] {
                static_cast<void>(Leaf<FirstLeafState>({10.0F, 10.0F}));
                throw std::runtime_error("Space slot failure");
            });
        });
    } catch (const std::runtime_error&) {
        slot_exception = true;
    }
    require(slot_exception && throwing.nodes.size() == 0
                && throwing.components.component_count() == 0,
            "throwing Space content leaked retained resources");

    LayoutFixture wrong_thread;
    std::exception_ptr thread_error;
    std::thread worker([&] {
        try {
            wrong_thread.mount([] { ryn::Space(ryn::SpaceProps{}, [] {}); });
        } catch (...) {
            thread_error = std::current_exception();
        }
    });
    worker.join();
    require(thread_error != nullptr && wrong_thread.nodes.size() == 0
                && wrong_thread.components.component_count() == 0,
            "wrong-thread Space mount changed retained state");
}

void test_wrap_alignment_margin_and_identity() {
    LayoutFixture fixture;
    ryn::runtime::ComponentId first;
    ryn::runtime::ComponentId second;
    fixture.mount([&] {
        ryn::Space(
            ryn::SpaceProps{}
                .wrap(true)
                .align(ryn::SpaceAlign::Center)
                .size(ryn::dp(8.0F), ryn::dp(6.0F)),
            [&] {
                first = Leaf<FirstLeafState>(
                    {20.0F, 10.0F},
                    ryn::LayoutStyle{}.margin_right(ryn::dp(2.0F)));
                second = Leaf<SecondLeafState>(
                    {30.0F, 20.0F},
                    ryn::LayoutStyle{}.margin_left(ryn::dp(3.0F)));
            });
    });
    const auto space = fixture.components.root_components().front();
    const auto root = fixture.components.root(space);
    const auto first_node = fixture.components.root(first);
    const auto second_node = fixture.components.root(second);
    const auto children = fixture.components.children(space);

    static_cast<void>(fixture.layout.layout(
        root,
        ryn::layout::Constraints::fixed(80.0F, 40.0F)));
    require(fixture.layout.flex_layout_diagnostics(root).line_count == 1
                && near(fixture.nodes.require(first_node).bounds.y, 15.0F)
                && near(fixture.nodes.require(second_node).bounds.y, 10.0F)
                && near(
                    fixture.nodes.require(second_node).bounds.x
                        - (fixture.nodes.require(first_node).bounds.x
                           + fixture.nodes.require(first_node).bounds.width),
                    13.0F),
            "wide Space alignment, gap, or margins were not placed deterministically");

    static_cast<void>(fixture.layout.layout(
        root,
        ryn::layout::Constraints::fixed(40.0F, 50.0F)));
    require(fixture.layout.flex_layout_diagnostics(root).line_count == 2
                && near(fixture.nodes.require(first_node).bounds.y, 0.0F)
                && near(fixture.nodes.require(second_node).bounds.x, 3.0F)
                && near(fixture.nodes.require(second_node).bounds.y, 16.0F),
            "narrow Space did not wrap complete items with the cross gap");

    static_cast<void>(fixture.layout.layout(
        root,
        ryn::layout::Constraints::fixed(80.0F, 40.0F)));
    require(fixture.layout.flex_layout_diagnostics(root).line_count == 1
                && fixture.components.children(space) == children
                && fixture.components.root(first) == first_node
                && fixture.components.root(second) == second_node,
            "wide-narrow-wide Space transition remounted direct children");
}

void test_reactive_phases_cleanup_and_steady_state() {
    LayoutFixture fixture;
    ryn::Signal<bool> vertical{false};
    ryn::Signal<bool> wrap{false};
    ryn::Signal<ryn::SpaceAlign> align{ryn::SpaceAlign::Start};
    ryn::Signal<ryn::LayoutGap> size{ryn::LayoutGap{ryn::SpaceSize::Small}};
    int content_runs = 0;
    ryn::runtime::ComponentId first;
    ryn::runtime::ComponentId second;
    fixture.mount([&] {
        ryn::Space(
            ryn::SpaceProps{}.vertical(vertical).wrap(wrap).align(align).size(size),
            [&] {
                ++content_runs;
                first = Leaf<FirstLeafState>({20.0F, 10.0F});
                second = Leaf<SecondLeafState>({30.0F, 20.0F});
            });
    });
    const auto space = fixture.components.root_components().front();
    const auto root = fixture.components.root(space);
    const auto first_node = fixture.components.root(first);
    const auto second_node = fixture.components.root(second);
    static_cast<void>(fixture.layout.layout(
        root,
        ryn::layout::Constraints::fixed(100.0F, 40.0F)));
    const auto initial_diagnostics = fixture.layout.flex_layout_diagnostics(root);
    const auto first_measure_count = fixture.nodes.require(first_node).measure_count;
    const auto second_measure_count = fixture.nodes.require(second_node).measure_count;
    const auto component_count = fixture.components.component_count();
    fixture.clear();

    align.set(ryn::SpaceAlign::Center);
    require(fixture.dirty.layout_roots().empty()
                && fixture.dirty.placement_roots() == std::vector{root}
                && fixture.dirty.geometry_nodes() == std::vector{root}
                && fixture.dirty.hit_test_nodes() == std::vector{root},
            "Space align escaped placement-only invalidation");
    fixture.layout.place(root);
    require(fixture.nodes.require(first_node).measure_count == first_measure_count
                && fixture.nodes.require(second_node).measure_count == second_measure_count
                && near(fixture.nodes.require(first_node).bounds.y, 15.0F),
            "Space align discarded measurement or placed the child incorrectly");
    fixture.clear();

    size.set(ryn::LayoutGap{ryn::SpaceSize::Middle});
    require(fixture.dirty.layout_roots() == std::vector{root},
            "Space size did not invalidate its measure subtree");
    static_cast<void>(fixture.layout.layout(
        root,
        ryn::layout::Constraints::fixed(100.0F, 40.0F)));
    require(fixture.components.state<ryn::detail::SpaceComponentState>(space)->model.main_gap
                    == 16.0F,
            "Middle Space size did not resolve from Theme");
    fixture.clear();
    size.set(ryn::LayoutGap{ryn::SpaceSize::Large});
    require(fixture.components.state<ryn::detail::SpaceComponentState>(space)->model.main_gap
                    == 24.0F,
            "Large Space size did not resolve from Theme");
    static_cast<void>(fixture.layout.layout(
        root,
        ryn::layout::Constraints::fixed(100.0F, 40.0F)));
    fixture.clear();
    size.set(ryn::LayoutGap{ryn::dp(4.0F), ryn::dp(6.0F)});
    require(fixture.components.state<ryn::detail::SpaceComponentState>(space)->model.main_gap
                    == 4.0F
                && fixture.components.state<ryn::detail::SpaceComponentState>(space)
                       ->model.cross_gap == 6.0F,
            "custom Space size did not retain both axes");
    static_cast<void>(fixture.layout.layout(
        root,
        ryn::layout::Constraints::fixed(100.0F, 40.0F)));
    fixture.clear();

    vertical.set(true);
    require(fixture.dirty.layout_roots() == std::vector{root},
            "Space orientation did not request target subtree measurement");
    static_cast<void>(fixture.layout.layout(
        root,
        ryn::layout::Constraints::fixed(100.0F, 80.0F)));
    fixture.clear();
    wrap.set(true);
    require(fixture.dirty.layout_roots() == std::vector{root},
            "Space wrap did not request target subtree measurement");
    static_cast<void>(fixture.layout.layout(
        root,
        ryn::layout::Constraints::fixed(100.0F, 80.0F)));
    const auto stable_capacity = fixture.layout.flex_layout_diagnostics(root);
    for (int iteration = 0; iteration < 8; ++iteration) {
        static_cast<void>(fixture.layout.layout(
            root,
            ryn::layout::Constraints::fixed(100.0F, 80.0F)));
    }
    require(fixture.layout.flex_layout_diagnostics(root).item_capacity
                    == stable_capacity.item_capacity
                && fixture.layout.flex_layout_diagnostics(root).line_capacity
                    == stable_capacity.line_capacity
                && stable_capacity.item_capacity == initial_diagnostics.item_capacity,
            "steady-state Space layout discarded reusable item/line scratch");

    require(content_runs == 1
                && fixture.components.component_count() == component_count
                && fixture.components.children(space) == std::vector({first, second}),
            "reactive Space Props reran content or changed component identity");
    fixture.clear();
    wrap.set(true);
    require(!fixture.frames.pending()
                && fixture.dirty.layout_roots().empty()
                && fixture.dirty.placement_roots().empty(),
            "equal Space update requested an idle frame");

    const auto model_before_invalid =
        fixture.components.state<ryn::detail::SpaceComponentState>(space)->model;
    bool invalid_align = false;
    try {
        align.set(static_cast<ryn::SpaceAlign>(255));
    } catch (const std::invalid_argument&) {
        invalid_align = true;
    }
    require(invalid_align
                && fixture.components.state<ryn::detail::SpaceComponentState>(space)->model
                    == model_before_invalid
                && !fixture.frames.pending()
                && fixture.dirty.layout_roots().empty()
                && fixture.dirty.placement_roots().empty(),
            "invalid reactive Space value changed committed model or dirty state");

    const auto stale_node = root;
    require(fixture.components.destroy(space), "Space destroy failed");
    fixture.clear();
    vertical.set(false);
    wrap.set(false);
    align.set(ryn::SpaceAlign::End);
    size.set(ryn::LayoutGap{ryn::SpaceSize::Small});
    require(!fixture.frames.pending(), "destroyed Space Prop subscription remained active");
    const auto replacement = fixture.nodes.create_root();
    require(replacement.index == stale_node.index
                && replacement.generation != stale_node.generation,
            "Space destroy/reuse fixture did not reuse the node slot");
    bool stale_layout_removed = false;
    try {
        static_cast<void>(fixture.layout.layout(
            replacement,
            ryn::layout::Constraints::fixed(10.0F, 10.0F)));
    } catch (const std::logic_error&) {
        stale_layout_removed = true;
    }
    require(stale_layout_removed, "destroyed Space layout leaked into a reused NodeId");
}

void test_text_button_hit_test_and_focus_composition() {
    VisualFixture fixture;
    int content_runs = 0;
    fixture.host->mount(ryn::Content{[&] {
        ryn::Space(
            ryn::SpaceProps{}
                .wrap(true)
                .align(ryn::SpaceAlign::Center)
                .size(ryn::SpaceSize::Middle),
            [&] {
                ++content_runs;
                ryn::Text(
                    ryn::TextProps{}
                        .content(u8"中文 Latin")
                        .layout(ryn::LayoutStyle{}.margin_right(ryn::dp(2.0F))));
                ryn::Button(
                    ryn::ButtonProps{}.size(ryn::ControlSize::Small),
                    [&] {
                        ++content_runs;
                        ryn::Text(u8"Small");
                    });
                ryn::Button(
                    ryn::ButtonProps{}.size(ryn::ControlSize::Large),
                    [&] {
                        ++content_runs;
                        ryn::Text(u8"大按钮");
                    });
            });
    }});
    require(fixture.synchronize(640.0F),
            "Space Text/Button composition did not synchronize");

    const auto space = fixture.host->components().root_components().front();
    const auto root = fixture.host->components().root(space);
    const auto children = fixture.host->components().children(space);
    require(content_runs == 3
                && children.size() == 3
                && fixture.nodes.require(root).children.size() == 3
                && fixture.host->mounted_buttons().size() == 2
                && fixture.host->text().mounted_texts().size() == 3
                && fixture.layout.flex_layout_diagnostics(root).line_count == 1,
            "Space did not retain direct CJK/Latin Text and differently sized Button children");

    const auto text_bounds =
        fixture.nodes.require(fixture.host->components().root(children[0])).bounds;
    const auto first_button = fixture.host->mounted_buttons()[0];
    const auto second_button = fixture.host->mounted_buttons()[1];
    const auto first_bounds = fixture.nodes.require(first_button.node).bounds;
    const auto second_bounds = fixture.nodes.require(second_button.node).bounds;
    require(near(first_bounds.x - (text_bounds.x + text_bounds.width), 18.0F)
                && near(second_bounds.x - (first_bounds.x + first_bounds.width), 16.0F)
                && near(first_bounds.height, 24.0F)
                && near(second_bounds.height, 40.0F),
            "Space Theme gap, margin, or Button size composition is incorrect");

    const auto first_center = ryn::runtime::Point{
        first_bounds.x + first_bounds.width * 0.5F,
        first_bounds.y + first_bounds.height * 0.5F,
    };
    const auto second_center = ryn::runtime::Point{
        second_bounds.x + second_bounds.width * 0.5F,
        second_bounds.y + second_bounds.height * 0.5F,
    };
    require(fixture.host->hit_test().hit_test(first_center) == first_button.interaction
                && fixture.host->hit_test().hit_test(second_center) == second_button.interaction,
            "Space placement diverged from Button HitTest bounds");
    fixture.host->focus().dispatch(tab_key());
    require(fixture.host->focus().state().focused == first_button.interaction,
            "Space changed first Button keyboard focus order");
    fixture.host->focus().dispatch(tab_key());
    require(fixture.host->focus().state().focused == second_button.interaction,
            "Space changed second Button keyboard focus order");

    require(fixture.synchronize(120.0F), "narrow Space composition did not synchronize");
    require(fixture.layout.flex_layout_diagnostics(root).line_count > 1
                && fixture.host->components().children(space) == children,
            "narrow Space did not wrap without remounting Text/Button children");
    require(fixture.synchronize(640.0F), "restored Space composition did not synchronize");
    require(fixture.layout.flex_layout_diagnostics(root).line_count == 1
                && fixture.host->components().children(space) == children
                && fixture.host->mounted_buttons()[0].component == first_button.component
                && fixture.host->mounted_buttons()[1].component == second_button.component,
            "wide-narrow-wide Space composition changed retained identities");
}

void test_theme_preset_size_updates_only_subscribed_space() {
    LayoutFixture fixture;
    ryn::Signal<ryn::ThemeConfig> config{ryn::ThemeConfig{}};
    int content_runs = 0;
    fixture.mount([&] {
        ryn::Theme(
            ryn::ThemeProps{}.config(config),
            ryn::ThemeContent{[&] {
                ryn::Space(ryn::SpaceProps{}.size(ryn::SpaceSize::Small), [&] {
                    ++content_runs;
                    static_cast<void>(Leaf<FirstLeafState>({10.0F, 10.0F}));
                });
                ryn::Space(ryn::SpaceProps{}.size(ryn::dp(7.0F)), [&] {
                    ++content_runs;
                    static_cast<void>(Leaf<SecondLeafState>({10.0F, 10.0F}));
                });
            }});
    });
    const auto roots = fixture.components.root_components();
    require(roots.size() == 2, "themed Space fixture lost transparent Theme roots");
    const auto preset = roots[0];
    const auto custom = roots[1];
    const auto preset_node = fixture.components.root(preset);
    fixture.clear();

    auto resized = ryn::ThemeConfig{};
    resized.seed.size_unit = ryn::dp(5.0F);
    require(config.set(resized)
                && fixture.dirty.layout_roots()
                    == std::vector<ryn::runtime::NodeId>{preset_node}
                && fixture.components.state<ryn::detail::SpaceComponentState>(preset)
                    ->model.main_gap == 10.0F
                && fixture.components.state<ryn::detail::SpaceComponentState>(custom)
                    ->model.main_gap == 7.0F
                && content_runs == 2
                && fixture.components.component_count() == 4,
            "Space Theme preset update changed custom size, content, or identity");

    fixture.clear();
    resized.seed.color_primary = ryn::Color::rgba8(114, 46, 209);
    require(config.set(resized)
                && fixture.dirty.layout_roots().empty()
                && !fixture.frames.pending(),
            "unrelated Theme color notified Space size subscribers");
}

} // namespace

int main() {
    try {
        test_mount_defaults_policy_and_lifecycle();
        test_wrap_alignment_margin_and_identity();
        test_reactive_phases_cleanup_and_steady_state();
        test_text_button_hit_test_and_focus_composition();
        test_theme_preset_size_updates_only_subscribed_space();
    } catch (const std::exception& error) {
        std::cerr << error.what() << '\n';
        return 1;
    }
    return 0;
}
