#include "component/button_component.hpp"

#include <ryn/rynui.hpp>

#include <array>
#include <exception>
#include <iostream>
#include <limits>
#include <memory>
#include <stdexcept>
#include <thread>

namespace {

void require(bool condition, const char* message) {
    if (!condition) {
        throw std::runtime_error(message);
    }
}

struct Fixture final {
    Fixture()
        : layout(nodes),
          dirty(nodes, &frames),
          fonts(create_runtime()),
          engine(*fonts),
          text_scene(*fonts, engine, frames) {
        const auto latin = fonts->load_font_file(
            RYNUI_VALIDATION_LATIN_FONT, 0, 14);
        const auto cjk = fonts->load_font_file(
            RYNUI_VALIDATION_CJK_FONT, 0, 14);
        require(latin && cjk, "Button component fonts failed to load");
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

    bool synchronize() {
        return host->layout_and_synchronize(
            {640.0F, 360.0F},
            {0.0F, 0.0F, 640.0F, 360.0F},
            {20.0F, 20.0F},
            10.0F);
    }

    ryn::runtime::Rect bounds(std::size_t index) const {
        return nodes.require(host->mounted_buttons()[index].node).bounds;
    }

    ryn::runtime::Point center(std::size_t index) const {
        const auto value = bounds(index);
        return {
            value.x + value.width * 0.5F,
            value.y + value.height * 0.5F,
        };
    }

    const ryn::graphics::QuadInstance& layer(
        std::size_t button,
        ryn::component::ButtonVisualLayer visual) const {
        const auto range = host->button_scene().visual_range(
            host->mounted_buttons()[button].scene);
        return host->button_scene().instances().at(
            range.first + static_cast<std::uint32_t>(visual));
    }

    std::array<float, 4> text_color(std::size_t index) const {
        const auto scene = host->text().mounted_texts()[index].scene;
        const auto range = text_scene.primitive(scene).instances;
        require(range.count != 0, "Button Text produced no glyph instances");
        return text_scene.glyph_scene().instances().at(range.first).color;
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

ryn::input::PointerInputEvent pointer_event(
    ryn::input::PointerAction action,
    ryn::runtime::Point point,
    ryn::input::PointerButton button = ryn::input::PointerButton::none) {
    return {
        ryn::input::PointerIdentity::mouse(),
        action,
        button,
        point.x,
        point.y,
    };
}

ryn::input::KeyboardInputEvent key(
    ryn::input::Key value,
    ryn::input::KeyAction action,
    bool repeat = false) {
    return {value, action, ryn::input::KeyModifier::none, repeat};
}

void clear_observation_state(Fixture& fixture) {
    fixture.dirty.clear();
    fixture.host->button_scene().instances().clear_dirty_ranges();
    fixture.text_scene.glyph_scene().instances().clear_dirty_ranges();
    static_cast<void>(fixture.frames.consume_request());
}

void test_mount_scene_composition_and_lifecycle() {
    bool outside_host_diagnosed = false;
    try {
        ryn::Button(ryn::ButtonProps{}, [] {});
    } catch (const std::logic_error&) {
        outside_host_diagnosed = true;
    }
    require(outside_host_diagnosed,
            "ryn::Button outside ButtonComponentHost was accepted");

    Fixture fixture;
    ryn::Signal<ryn::ButtonType> first_type{ryn::ButtonType::Default};
    ryn::Signal<ryn::ControlSize> first_size{ryn::ControlSize::Middle};
    ryn::Signal<bool> first_disabled{false};
    ryn::Signal<bool> first_loading{false};
    int content_runs = 0;
    fixture.host->mount(ryn::Content{[&] {
        ryn::Button(
            ryn::ButtonProps{}
                .type(first_type)
                .size(first_size)
                .disabled(first_disabled)
                .loading(first_loading),
            [&] {
                ++content_runs;
                ryn::Text(u8"默认按钮");
            });
        ryn::Button(
            ryn::ButtonProps{}.type(ryn::ButtonType::Primary),
            [&] {
                ++content_runs;
                ryn::Text(u8"Primary");
            });
    }});
    require(fixture.synchronize(), "mounted Buttons did not synchronize");
    require(content_runs == 2
                && fixture.host->components().mount_runs() == 1
                && fixture.host->components().component_count() == 4
                && fixture.host->mounted_buttons().size() == 2
                && fixture.host->text().mounted_texts().size() == 2
                && fixture.host->interactions().size() == 2
                && fixture.host->button_scene().size() == 2
                && fixture.nodes.size() == 4,
            "Button mount did not create stable component resources");
    const auto first = fixture.host->mounted_buttons()[0];
    const auto second = fixture.host->mounted_buttons()[1];
    const auto first_text = fixture.host->text().mounted_texts()[0];
    require(fixture.host->components().parent(first_text.component) == first.component
                && fixture.nodes.require(first_text.scene.valid()
                        ? fixture.text_scene.node(first_text.scene)
                        : ryn::runtime::NodeId{})
                    .parent == first.node,
            "Button content did not mount as a persistent child subtree");
    require(fixture.bounds(0).height == 32.0F
                && fixture.bounds(1).height == 32.0F
                && fixture.text_color(0)
                    == fixture.host->text().theme().button
                        .default_variant.normal.foreground
                && fixture.text_color(1)
                    == fixture.host->text().theme().button
                        .primary_variant.normal.foreground,
            "Button layout or inherited foreground did not use Theme tokens");
    const auto commands = fixture.host->scene_composer().ordered_scene().commands();
    require(commands.size() == 4
                && commands[0].kind == ryn::graphics::SceneDrawKind::quad
                && commands[1].kind == ryn::graphics::SceneDrawKind::glyph
                && commands[2].kind == ryn::graphics::SceneDrawKind::quad
                && commands[3].kind == ryn::graphics::SceneDrawKind::glyph,
            "Button/Text paint traversal did not preserve sibling composition");
    require(fixture.host->hit_test().hit_test(fixture.center(0)) == first.interaction
                && fixture.host->hit_test().hit_test(fixture.center(1))
                    == second.interaction,
            "Button visual order and HitTest target diverged");

    require(fixture.host->destroy(first.component)
                && !fixture.host->components().contains(first.component)
                && !fixture.host->interactions().contains(first.interaction)
                && fixture.host->button_scene().size() == 1
                && fixture.host->text().mounted_texts().size() == 1,
            "Button destroy leaked its content, interaction, or scene range");
    static_cast<void>(fixture.frames.consume_request());
    first_type.set(ryn::ButtonType::Primary);
    first_size.set(ryn::ControlSize::Large);
    first_disabled.set(true);
    first_loading.set(true);
    require(!fixture.frames.pending()
                && fixture.host->mounted_buttons().size() == 1
                && fixture.host->mounted_buttons()[0].component == second.component,
            "destroyed Button Prop subscriptions changed live state");

    Fixture throwing;
    bool slot_exception = false;
    try {
        throwing.host->mount(ryn::Content{[] {
            ryn::Button(ryn::ButtonProps{}, [] {
                ryn::Text(u8"partial");
                throw std::runtime_error("Button slot failure");
            });
        }});
    } catch (const std::runtime_error&) {
        slot_exception = true;
    }
    require(slot_exception
                && throwing.nodes.size() == 0
                && throwing.host->components().component_count() == 0
                && throwing.host->interactions().size() == 0
                && throwing.host->button_scene().size() == 0
                && throwing.text_scene.size() == 0
                && throwing.host->mounted_buttons().empty(),
            "throwing Button content leaked partial resources");

    Fixture wrong_thread;
    std::exception_ptr thread_error;
    std::thread worker([&] {
        try {
            wrong_thread.host->mount(ryn::Content{[] {
                ryn::Button(ryn::ButtonProps{}, [] {});
            }});
        } catch (...) {
            thread_error = std::current_exception();
        }
    });
    worker.join();
    require(thread_error != nullptr
                && wrong_thread.nodes.size() == 0
                && wrong_thread.host->mounted_buttons().empty(),
            "wrong-thread Button mount changed retained state");
}

void test_reactive_state_matrix_and_minimal_dirty_ranges() {
    Fixture fixture;
    ryn::Signal<ryn::ButtonType> type{ryn::ButtonType::Default};
    ryn::Signal<ryn::ControlSize> size{ryn::ControlSize::Middle};
    ryn::Signal<bool> disabled{false};
    ryn::Signal<bool> loading{false};
    fixture.host->mount(ryn::Content{[&] {
        ryn::Button(
            ryn::ButtonProps{}
                .type(type)
                .size(size)
                .disabled(disabled)
                .loading(loading),
            [] { ryn::Text(u8"确定 CJK"); });
        ryn::Button(
            ryn::ButtonProps{},
            [] { ryn::Text(u8"Stable sibling"); });
    }});
    require(fixture.synchronize(), "reactive Button fixture did not synchronize");
    const auto target = fixture.host->mounted_buttons()[0];
    const auto sibling = fixture.host->mounted_buttons()[1];
    const auto target_text = fixture.host->text().mounted_texts()[0].scene;
    const auto sibling_text = fixture.host->text().mounted_texts()[1].scene;
    const auto target_text_counters = fixture.text_scene.text_state(target_text).counters();
    const auto sibling_text_counters = fixture.text_scene.text_state(sibling_text).counters();
    const auto target_measure_count = fixture.nodes.require(target.node).measure_count;
    const auto scene_rebuilds = fixture.host->scene_composer().diagnostics().rebuilds;
    clear_observation_state(fixture);

    const auto point = fixture.center(0);
    fixture.host->pointer().dispatch(pointer_event(
        ryn::input::PointerAction::move, point));
    require(fixture.host->snapshot(target.component).hovered
                && fixture.dirty.layout_roots().empty()
                && fixture.dirty.material_nodes().size() == 2
                && !fixture.host->button_scene().instances()
                    .material_dirty_ranges().empty(),
            "Button hover escaped target Button/Text Material invalidation");
    require(fixture.synchronize(), "Button hover did not synchronize");
    require(fixture.layer(0, ryn::component::ButtonVisualLayer::border).color
                    == fixture.host->text().theme().button
                        .default_variant.hover.border
                && fixture.text_color(0)
                    == fixture.host->text().theme().button
                        .default_variant.hover.foreground
                && fixture.nodes.require(target.node).measure_count
                    == target_measure_count
                && fixture.host->scene_composer().diagnostics().rebuilds
                    == scene_rebuilds
                && fixture.text_scene.text_state(target_text).counters().shape_count
                    == target_text_counters.shape_count
                && fixture.text_scene.text_state(target_text).counters().measure_count
                    == target_text_counters.measure_count
                && fixture.text_scene.text_state(sibling_text).counters().shape_count
                    == sibling_text_counters.shape_count,
            "Button hover relaid, reshaped, or rebuilt scene structure");

    fixture.host->pointer().dispatch(pointer_event(
        ryn::input::PointerAction::down,
        point,
        ryn::input::PointerButton::primary));
    require(fixture.host->snapshot(target.component).pointer_pressed
                && fixture.layer(0, ryn::component::ButtonVisualLayer::border).color
                    == fixture.host->text().theme().button
                        .default_variant.active.border,
            "Button pointer press did not resolve active visual state");
    disabled.set(true);
    const auto pointer = fixture.host->pointer().state(
        ryn::input::PointerIdentity::mouse());
    require(fixture.host->snapshot(target.component).disabled
                && !fixture.host->snapshot(target.component).hovered
                && !fixture.host->snapshot(target.component).pointer_pressed
                && pointer.has_value()
                && !pointer->capture.has_value()
                && !fixture.host->interactions().require(target.interaction).eligible
                && fixture.layer(0, ryn::component::ButtonVisualLayer::background).color
                    == fixture.host->text().theme().button.disabled.background,
            "disabled state did not win atomically over hover/pressed");
    type.set(ryn::ButtonType::Primary);
    require(fixture.layer(0, ryn::component::ButtonVisualLayer::background).color
                    == fixture.host->text().theme().button.disabled.background,
            "type update escaped disabled visual priority");

    disabled.set(false);
    require(fixture.layer(0, ryn::component::ButtonVisualLayer::background).color
                    == fixture.host->text().theme().button
                        .primary_variant.normal.background,
            "Primary Button normal state did not use the locked token");
    size.set(ryn::ControlSize::Large);
    const auto sibling_identity = sibling.component;
    require(fixture.synchronize(), "Large Button update did not synchronize");
    require(fixture.bounds(0).height == 40.0F
                && fixture.host->mounted_buttons()[1].component == sibling_identity
                && fixture.text_scene.text_state(target_text).counters().shape_count
                    == target_text_counters.shape_count,
            "Button size update changed sibling identity or reshaped Text");
    size.set(ryn::ControlSize::Small);
    require(fixture.synchronize(), "Small Button update did not synchronize");
    require(fixture.bounds(0).height == 24.0F,
            "Small Button did not use the locked control height");
    size.set(ryn::ControlSize::Middle);
    require(fixture.synchronize(), "Middle Button update did not synchronize");
    require(fixture.bounds(0).height == 32.0F,
            "Middle Button did not use the locked control height");
    size.set(ryn::ControlSize::Large);
    require(fixture.synchronize(), "Large Button restore did not synchronize");

    fixture.host->focus().dispatch(key(
        ryn::input::Key::tab,
        ryn::input::KeyAction::down));
    require(fixture.host->focus().state().focused == target.interaction
                && fixture.host->snapshot(target.component).focus.focus_visible
                && fixture.layer(0, ryn::component::ButtonVisualLayer::focus_ring).opacity
                    == 1.0F,
            "keyboard focus did not resolve focus-visible layer");
    loading.set(true);
    require(fixture.host->focus().state().focused == target.interaction,
            "loading transition incorrectly cleared Button focus");
    require(fixture.synchronize(), "loading Button did not synchronize");
    const auto loading_layer = fixture.layer(
        0, ryn::component::ButtonVisualLayer::loading_indicator);
    require(fixture.host->snapshot(target.component).loading
                && loading_layer.opacity
                    == fixture.host->text().theme().button.loading_opacity
                && loading_layer.clip_rect[2] > 0.0F
                && loading_layer.clip_rect[3] < 0.0F
                && fixture.text_color(0)[3]
                    == fixture.host->text().theme().button.loading_opacity,
            "loading state missed indicator geometry, opacity, or Text context");
    loading.set(false);
    fixture.host->pointer().dispatch(pointer_event(
        ryn::input::PointerAction::move,
        {500.0F, 300.0F}));
    fixture.host->pointer().dispatch(pointer_event(
        ryn::input::PointerAction::move,
        fixture.center(0)));
    require(fixture.layer(0, ryn::component::ButtonVisualLayer::background).color
                    == fixture.host->text().theme().button
                        .primary_variant.hover.background,
            "Primary Button hover did not use the locked state token");
    fixture.host->pointer().dispatch(pointer_event(
        ryn::input::PointerAction::down,
        fixture.center(0),
        ryn::input::PointerButton::primary));
    require(fixture.layer(0, ryn::component::ButtonVisualLayer::background).color
                    == fixture.host->text().theme().button
                        .primary_variant.active.background,
            "Primary Button pressed did not use the locked state token");
    fixture.host->pointer().cancel_all();
}

void test_pointer_keyboard_click_path_and_callback_mutation() {
    Fixture fixture;
    ryn::Signal<bool> loading{false};
    int clicks = 0;
    bool callback_observed_settled_state = false;
    bool arm_loading = false;
    ryn::runtime::ComponentId component;
    fixture.host->mount(ryn::Content{[&] {
        ryn::Button(
            ryn::ButtonProps{}
                .loading(loading)
                .onClick([&] {
                    ++clicks;
                    const auto state = fixture.host->snapshot(component);
                    const auto pointer = fixture.host->pointer().state(
                        ryn::input::PointerIdentity::mouse());
                    callback_observed_settled_state = !state.pointer_pressed
                        && pointer.has_value()
                        && !pointer->capture.has_value();
                    if (arm_loading) {
                        loading.set(true);
                    }
                }),
            [] { ryn::Text(u8"Submit"); });
    }});
    component = fixture.host->mounted_buttons()[0].component;
    require(fixture.synchronize(), "click path Button did not synchronize");
    const auto inside = fixture.center(0);
    const ryn::runtime::Point outside{500.0F, 300.0F};

    fixture.host->pointer().dispatch(pointer_event(
        ryn::input::PointerAction::down,
        inside,
        ryn::input::PointerButton::primary));
    fixture.host->pointer().dispatch(pointer_event(
        ryn::input::PointerAction::up,
        inside,
        ryn::input::PointerButton::primary));
    require(clicks == 1 && callback_observed_settled_state,
            "pointer click did not settle press/capture before callback");

    fixture.host->pointer().dispatch(pointer_event(
        ryn::input::PointerAction::down,
        inside,
        ryn::input::PointerButton::primary));
    fixture.host->pointer().dispatch(pointer_event(
        ryn::input::PointerAction::move, outside));
    fixture.host->pointer().dispatch(pointer_event(
        ryn::input::PointerAction::up,
        outside,
        ryn::input::PointerButton::primary));
    fixture.host->pointer().dispatch(pointer_event(
        ryn::input::PointerAction::down,
        inside,
        ryn::input::PointerButton::secondary));
    fixture.host->pointer().dispatch(pointer_event(
        ryn::input::PointerAction::up,
        inside,
        ryn::input::PointerButton::secondary));
    require(clicks == 1, "drag-out or secondary pointer produced a click");

    fixture.host->pointer().dispatch(pointer_event(
        ryn::input::PointerAction::down,
        inside,
        ryn::input::PointerButton::primary));
    fixture.host->pointer().cancel_all();
    fixture.host->pointer().dispatch(pointer_event(
        ryn::input::PointerAction::up,
        inside,
        ryn::input::PointerButton::primary));
    fixture.host->pointer().dispatch(pointer_event(
        ryn::input::PointerAction::down,
        inside,
        ryn::input::PointerButton::primary));
    fixture.host->set_window_active(false);
    fixture.host->set_window_active(true);
    fixture.host->pointer().dispatch(pointer_event(
        ryn::input::PointerAction::up,
        inside,
        ryn::input::PointerButton::primary));
    require(clicks == 1,
            "pointer cancel or window focus loss produced a click");

    fixture.host->focus().dispatch(key(
        ryn::input::Key::tab,
        ryn::input::KeyAction::down));
    fixture.host->focus().dispatch(key(
        ryn::input::Key::enter,
        ryn::input::KeyAction::down));
    fixture.host->focus().dispatch(key(
        ryn::input::Key::enter,
        ryn::input::KeyAction::down,
        true));
    fixture.host->focus().dispatch(key(
        ryn::input::Key::enter,
        ryn::input::KeyAction::up));
    fixture.host->focus().dispatch(key(
        ryn::input::Key::space,
        ryn::input::KeyAction::down));
    require(fixture.host->snapshot(component).focus.keyboard_pressed,
            "Space key down did not share Button pressed visual state");
    fixture.host->focus().dispatch(key(
        ryn::input::Key::space,
        ryn::input::KeyAction::up));
    require(clicks == 3
                && !fixture.host->snapshot(component).focus.keyboard_pressed,
            "Enter/Space did not share the one-click callback path");

    arm_loading = true;
    fixture.host->focus().dispatch(key(
        ryn::input::Key::enter,
        ryn::input::KeyAction::down));
    require(clicks == 4 && loading.get(),
            "first submit click did not enter reactive loading");
    fixture.host->focus().dispatch(key(
        ryn::input::Key::enter,
        ryn::input::KeyAction::up));
    fixture.host->focus().dispatch(key(
        ryn::input::Key::enter,
        ryn::input::KeyAction::down));
    fixture.host->pointer().dispatch(pointer_event(
        ryn::input::PointerAction::down,
        inside,
        ryn::input::PointerButton::primary));
    fixture.host->pointer().dispatch(pointer_event(
        ryn::input::PointerAction::up,
        inside,
        ryn::input::PointerButton::primary));
    require(clicks == 4
                && fixture.host->focus().state().focused
                    == fixture.host->mounted_buttons()[0].interaction,
            "loading Button accepted a duplicate activation or lost focus");

    Fixture self_destroy;
    int destroy_clicks = 0;
    ryn::runtime::ComponentId doomed;
    self_destroy.host->mount(ryn::Content{[&] {
        ryn::Button(
            ryn::ButtonProps{}.onClick([&] {
                ++destroy_clicks;
                require(self_destroy.host->destroy(doomed),
                        "self-destroy callback could not destroy Button");
            }),
            [] { ryn::Text(u8"Destroy"); });
    }});
    doomed = self_destroy.host->mounted_buttons()[0].component;
    require(self_destroy.synchronize(), "self-destroy Button did not synchronize");
    const auto destroy_point = self_destroy.center(0);
    self_destroy.host->pointer().dispatch(pointer_event(
        ryn::input::PointerAction::down,
        destroy_point,
        ryn::input::PointerButton::primary));
    self_destroy.host->pointer().dispatch(pointer_event(
        ryn::input::PointerAction::up,
        destroy_point,
        ryn::input::PointerButton::primary));
    require(destroy_clicks == 1
                && self_destroy.host->mounted_buttons().empty()
                && self_destroy.host->interactions().size() == 0
                && self_destroy.host->button_scene().size() == 0,
            "self-destroy click accessed or retained stale Button resources");

    Fixture throwing;
    int throw_clicks = 0;
    throwing.host->mount(ryn::Content{[&] {
        ryn::Button(
            ryn::ButtonProps{}.onClick([&] {
                ++throw_clicks;
                throw std::runtime_error("click failure");
            }),
            [] { ryn::Text(u8"Throw"); });
    }});
    require(throwing.synchronize(), "throwing callback Button did not synchronize");
    const auto throw_point = throwing.center(0);
    bool callback_exception = false;
    try {
        throwing.host->pointer().dispatch(pointer_event(
            ryn::input::PointerAction::down,
            throw_point,
            ryn::input::PointerButton::primary));
        throwing.host->pointer().dispatch(pointer_event(
            ryn::input::PointerAction::up,
            throw_point,
            ryn::input::PointerButton::primary));
    } catch (const std::runtime_error&) {
        callback_exception = true;
    }
    const auto pointer = throwing.host->pointer().state(
        ryn::input::PointerIdentity::mouse());
    require(callback_exception
                && throw_clicks == 1
                && !throwing.host->snapshot(
                    throwing.host->mounted_buttons()[0].component)
                    .pointer_pressed
                && pointer.has_value()
                && !pointer->capture.has_value(),
            "throwing click callback leaked pressed/capture state");
}

struct ParentState final {};
struct ParentContentSlot final {};
using ParentContent = ryn::SlotContent<ParentContentSlot>;

ryn::runtime::ComponentId mount_parent(
    ryn::layout::LayoutEngine& layout,
    const ParentContent& content) {
    auto& build = ryn::runtime::require_component_build_context();
    const auto parent = build.mount_component<ParentState>();
    layout.set_layout(build.root(parent), ryn::layout::BoxLayout{});
    build.mount_slot(parent, content);
    return parent;
}

void test_click_callback_can_destroy_parent_scope() {
    Fixture fixture;
    ryn::runtime::ComponentId parent;
    int clicks = 0;
    fixture.host->mount(ryn::Content{[&] {
        parent = mount_parent(fixture.layout, ParentContent{[&] {
            ryn::Button(
                ryn::ButtonProps{}.onClick([&] {
                    ++clicks;
                    require(fixture.host->destroy(parent),
                            "click callback could not destroy parent Scope");
                }),
                [] { ryn::Text(u8"Destroy parent"); });
        }});
    }});
    require(fixture.synchronize(), "parent destroy Button did not synchronize");
    const auto point = fixture.center(0);
    fixture.host->pointer().dispatch(pointer_event(
        ryn::input::PointerAction::down,
        point,
        ryn::input::PointerButton::primary));
    fixture.host->pointer().dispatch(pointer_event(
        ryn::input::PointerAction::up,
        point,
        ryn::input::PointerButton::primary));
    require(clicks == 1
                && fixture.nodes.size() == 0
                && fixture.host->mounted_buttons().empty()
                && fixture.host->interactions().size() == 0
                && fixture.host->button_scene().size() == 0
                && fixture.text_scene.size() == 0,
            "parent Scope destroy retained Button subtree resources");
}

void test_flex_composes_text_button_and_nested_flex() {
    Fixture fixture;
    ryn::Signal<ryn::FlexJustify> justify{ryn::FlexJustify::Start};
    int content_runs = 0;
    fixture.host->mount(ryn::Content{[&] {
        ryn::Flex(
            ryn::FlexProps{}
                .justify(justify)
                .align(ryn::FlexAlign::Center)
                .gap(ryn::SpaceSize::Small),
            [&] {
                ++content_runs;
                ryn::Text(u8"Label");
                ryn::Button(
                    ryn::ButtonProps{}.type(ryn::ButtonType::Primary),
                    [] { ryn::Text(u8"Action"); });
                ryn::Flex(
                    ryn::FlexProps{}.vertical(true).gap(ryn::dp(2.0F)),
                    [] { ryn::Text(u8"Nested"); });
            });
    }});
    require(fixture.synchronize(), "Flex Text/Button composition did not synchronize");

    const auto flex = fixture.host->components().root_components().front();
    const auto flex_node = fixture.host->components().root(flex);
    const auto children = fixture.host->components().children(flex);
    const auto paint = fixture.host->components().paint_traversal();
    const auto scene_rebuilds = fixture.host->scene_composer().diagnostics().rebuilds;
    require(content_runs == 1
                && fixture.host->components().component_count() == 6
                && children.size() == 3
                && fixture.nodes.require(flex_node).children.size() == 3
                && fixture.host->mounted_buttons().size() == 1
                && fixture.host->text().mounted_texts().size() == 3
                && paint.size() == 4
                && paint[0].placement
                    == ryn::runtime::SceneFragmentPlacement::before_children,
            "Flex did not compose Text, Button, and nested Flex as direct retained children");
    require(fixture.host->hit_test().hit_test(fixture.center(0))
                    == fixture.host->mounted_buttons()[0].interaction,
            "Button inside Flex lost its interaction geometry");

    clear_observation_state(fixture);
    justify.set(ryn::FlexJustify::End);
    require(fixture.dirty.layout_roots().empty()
                && fixture.dirty.placement_roots() == std::vector{flex_node},
            "Flex composition justify update did not stay placement-only");
    require(fixture.synchronize(), "reactive Flex composition did not synchronize");
    require(content_runs == 1
                && fixture.host->components().children(flex) == children
                && fixture.host->scene_composer().diagnostics().rebuilds == scene_rebuilds,
            "Flex update reran content or rebuilt Text/Button scene topology");
}

} // namespace

int main() {
    try {
        test_mount_scene_composition_and_lifecycle();
        test_reactive_state_matrix_and_minimal_dirty_ranges();
        test_pointer_keyboard_click_path_and_callback_mutation();
        test_click_callback_can_destroy_parent_scope();
        test_flex_composes_text_button_and_nested_flex();
    } catch (const std::exception& error) {
        std::cerr << error.what() << '\n';
        return 1;
    }
    return 0;
}
