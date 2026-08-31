#include "component/button_component.hpp"
#include "graphics/rounded_effect_gpu.hpp"

#include <ryn/rynui.hpp>

#include <array>
#include <cmath>
#include <exception>
#include <iostream>
#include <limits>
#include <map>
#include <memory>
#include <stdexcept>
#include <thread>

namespace {

void require(bool condition, const char* message) {
    if (!condition) {
        throw std::runtime_error(message);
    }
}

std::array<float, 4> channels(ryn::Color color) {
    return {color.red(), color.green(), color.blue(), color.alpha()};
}

bool near(float actual, float expected, float tolerance = 0.001F) {
    return std::fabs(actual - expected) <= tolerance;
}

ryn::runtime::Rect quad_bounds(
    const ryn::graphics::QuadInstance& instance,
    ryn::runtime::Size viewport = {640.0F, 360.0F}) {
    return {
        (instance.clip_rect[0] + 1.0F) * viewport.width * 0.5F,
        (1.0F - instance.clip_rect[1]) * viewport.height * 0.5F,
        instance.clip_rect[2] * viewport.width * 0.5F,
        -instance.clip_rect[3] * viewport.height * 0.5F,
    };
}

bool near_rect(ryn::runtime::Rect actual, ryn::runtime::Rect expected) {
    return near(actual.x, expected.x)
        && near(actual.y, expected.y)
        && near(actual.width, expected.width)
        && near(actual.height, expected.height);
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
        chains.emplace(14, chain);
        host = std::make_unique<ryn::detail::ButtonComponentHost>(
            nodes,
            layout,
            dirty,
            text_scene,
            [this](ryn::SystemFontFamily, std::uint32_t, std::uint32_t pixel_size) {
                return resolve_fonts(pixel_size);
            },
            frames);
    }

    static std::unique_ptr<ryn::font::FontRuntime> create_runtime() {
        auto created = ryn::font::FontRuntime::create();
        require(static_cast<bool>(created), "Font Runtime initialization failed");
        return std::move(created.runtime);
    }

    std::vector<ryn::font::FontIdentity> resolve_fonts(std::uint32_t pixel_size) {
        if (const auto found = chains.find(pixel_size); found != chains.end()) {
            return found->second;
        }
        const auto latin = fonts->load_font_file(
            RYNUI_VALIDATION_LATIN_FONT, 0, pixel_size);
        const auto cjk = fonts->load_font_file(
            RYNUI_VALIDATION_CJK_FONT, 0, pixel_size);
        if (!latin || !cjk) {
            return {};
        }
        auto resolved = std::vector<ryn::font::FontIdentity>{latin.font, cjk.font};
        chains.emplace(pixel_size, resolved);
        return resolved;
    }

    bool synchronize() {
        return host->layout_and_synchronize(
            {640.0F, 360.0F},
            {0.0F, 0.0F, 640.0F, 360.0F},
            {20.0F, 20.0F},
            10.0F);
    }

    std::size_t tick(std::int64_t microseconds) {
        return host->tick_animations(
            ryn::animation::AnimationTime::microseconds(microseconds));
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

    const ryn::graphics::QuadInstance& loading_segment(
        std::size_t button,
        std::size_t segment) const {
        const auto range = host->button_scene().visual_range(
            host->mounted_buttons()[button].scene);
        return host->button_scene().instances().at(
            range.first + static_cast<std::uint32_t>(
                ryn::component::button_loading_segment_index(segment)));
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
    std::map<std::uint32_t, std::vector<ryn::font::FontIdentity>> chains;
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
                && fixture.host->animations().diagnostics().scopes == 2
                && fixture.host->animations().diagnostics().targets == 10
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
                    == channels(ryn::resolve_theme().button().default_color)
                && fixture.text_color(1)
                    == channels(ryn::resolve_theme().button().primary_color),
            "Button layout or inherited foreground did not use Theme tokens");
    const auto first_shadows = fixture.host->button_scene().shadow_effects(first.scene);
    const auto second_shadows = fixture.host->button_scene().shadow_effects(second.scene);
    require(first_shadows.size() == 1 && second_shadows.size() == 1
                && fixture.host->rounded_effects().at(first_shadows.front()).material.color
                    == ryn::resolve_theme().button().default_shadow[0].color
                && fixture.host->rounded_effects().at(second_shadows.front()).material.color
                    == ryn::resolve_theme().button().primary_shadow[0].color,
            "Default and Primary Button shadows did not resolve component tokens");
    const auto commands = fixture.host->scene_composer().ordered_scene().commands();
    require(commands.size() == 6
                && commands[0].kind == ryn::graphics::SceneDrawKind::rounded_effect
                && commands[1].kind == ryn::graphics::SceneDrawKind::quad
                && commands[2].kind == ryn::graphics::SceneDrawKind::glyph
                && commands[3].kind == ryn::graphics::SceneDrawKind::rounded_effect
                && commands[4].kind == ryn::graphics::SceneDrawKind::quad
                && commands[5].kind == ryn::graphics::SceneDrawKind::glyph,
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
    require(fixture.host->animations().diagnostics().scopes == 1
                && fixture.host->animations().diagnostics().targets == 5
                && fixture.host->animations().size() == 0,
            "Button destroy leaked animation scope or target resources");
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
                && throwing.host->animations().diagnostics().scopes == 0
                && throwing.host->animations().diagnostics().targets == 0
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
    fixture.host->set_motion_preference(
        ryn::animation::MotionPreference::reduced);
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
    const auto target_shadow =
        fixture.host->button_scene().shadow_effects(target.scene).front();
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
                    == channels(ryn::resolve_theme().button().default_hover_color)
                && fixture.text_color(0)
                    == channels(ryn::resolve_theme().button().default_hover_color)
                && fixture.nodes.require(target.node).measure_count
                    == target_measure_count
                && fixture.host->scene_composer().diagnostics().rebuilds
                    == scene_rebuilds
                && fixture.text_scene.text_state(target_text).counters().shape_count
                    == target_text_counters.shape_count
                && fixture.text_scene.text_state(target_text).counters().measure_count
                    == target_text_counters.measure_count
                && fixture.text_scene.text_state(sibling_text).counters().shape_count
                    == sibling_text_counters.shape_count
                && fixture.host->button_scene().shadow_effects(target.scene).front()
                    == target_shadow,
            "Button hover relaid, reshaped, or rebuilt scene structure");

    fixture.host->pointer().dispatch(pointer_event(
        ryn::input::PointerAction::down,
        point,
        ryn::input::PointerButton::primary));
    require(fixture.host->snapshot(target.component).pointer_pressed
                && fixture.layer(0, ryn::component::ButtonVisualLayer::border).color
                    == channels(ryn::resolve_theme().button().default_active_color),
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
                    == channels(ryn::resolve_theme().button().disabled_background),
            "disabled state did not win atomically over hover/pressed");
    type.set(ryn::ButtonType::Primary);
    require(fixture.layer(0, ryn::component::ButtonVisualLayer::background).color
                    == channels(ryn::resolve_theme().button().disabled_background),
            "type update escaped disabled visual priority");

    disabled.set(false);
    require(fixture.layer(0, ryn::component::ButtonVisualLayer::background).color
                    == channels(ryn::resolve_theme().button().primary_background)
                && fixture.host->rounded_effects().at(target_shadow).material.color
                    == ryn::resolve_theme().button().primary_shadow[0].color,
            "Primary Button normal state did not use the locked token");
    size.set(ryn::ControlSize::Large);
    const auto sibling_identity = sibling.component;
    require(fixture.synchronize(), "Large Button update did not synchronize");
    require(fixture.bounds(0).height == 40.0F
                && fixture.host->mounted_buttons()[1].component == sibling_identity
                && fixture.text_scene.text_state(target_text).counters().shape_count
                    == target_text_counters.shape_count + 1,
            "Button size update missed typography reshaping or changed sibling identity");
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
                && fixture.host->button_scene().focus_effect(target.scene).material.opacity
                    == 1.0F,
            "keyboard focus did not resolve focus-visible layer");
    const auto& focus_effect = fixture.host->button_scene().focus_effect(target.scene);
    const auto& focus_shape = focus_effect.geometry.shape.rect;
    const auto focus_mid_y = focus_shape.y + focus_shape.height * 0.5F;
    require(focus_effect.geometry.outline_offset == 1.0F
                && focus_effect.geometry.outline_width == 3.0F
                && ryn::graphics::rounded_effect_coverage(
                    {focus_shape.x + focus_shape.width + 0.25F, focus_mid_y},
                    focus_effect) == 0.0F
                && ryn::graphics::rounded_effect_coverage(
                    {focus_shape.x + focus_shape.width + 2.5F, focus_mid_y},
                    focus_effect) > 0.99F,
            "Button focus did not preserve a transparent gap and hollow 3px ring");
    const auto focus_150 = ryn::graphics::pack_rounded_effect_instance(
        focus_effect,
        {960, 540, 1.5F});
    require(focus_150.effect_params[1] == 4.5F
                && focus_150.effect_params[2] == 1.5F
                && ryn::graphics::rounded_effect_gpu_coverage_reference(
                    {
                        (focus_shape.x + focus_shape.width + 0.25F) * 1.5F,
                        focus_mid_y * 1.5F,
                    },
                    focus_150) == 0.0F
                && ryn::graphics::rounded_effect_gpu_coverage_reference(
                    {
                        (focus_shape.x + focus_shape.width + 2.5F) * 1.5F,
                        focus_mid_y * 1.5F,
                    },
                    focus_150) > 0.99F,
            "Button focus logical geometry drifted at 150 percent display scale");
    loading.set(true);
    require(fixture.host->focus().state().focused == target.interaction,
            "loading transition incorrectly cleared Button focus");
    require(fixture.synchronize(), "loading Button did not synchronize");
    const auto loading_layer = fixture.layer(
        0, ryn::component::ButtonVisualLayer::loading_indicator);
    require(fixture.host->snapshot(target.component).loading
                && loading_layer.opacity
                    == ryn::resolve_theme().button().loading_opacity
                && loading_layer.clip_rect[2] > 0.0F
                && loading_layer.clip_rect[3] < 0.0F
                && fixture.text_color(0)[3]
                    == ryn::resolve_theme().button().loading_opacity,
            "loading state missed indicator geometry, opacity, or Text context");
    loading.set(false);
    fixture.host->pointer().dispatch(pointer_event(
        ryn::input::PointerAction::move,
        {500.0F, 300.0F}));
    fixture.host->pointer().dispatch(pointer_event(
        ryn::input::PointerAction::move,
        fixture.center(0)));
    require(fixture.layer(0, ryn::component::ButtonVisualLayer::background).color
                    == channels(ryn::resolve_theme().button().primary_hover_background),
            "Primary Button hover did not use the locked state token");
    fixture.host->pointer().dispatch(pointer_event(
        ryn::input::PointerAction::down,
        fixture.center(0),
        ryn::input::PointerButton::primary));
    require(fixture.layer(0, ryn::component::ButtonVisualLayer::background).color
                    == channels(ryn::resolve_theme().button().primary_active_background),
            "Primary Button pressed did not use the locked state token");
    fixture.host->pointer().cancel_all();

    fixture.host->pointer().dispatch(pointer_event(
        ryn::input::PointerAction::move, {500.0F, 300.0F}));
    type.set(ryn::ButtonType::Danger);
    require(fixture.layer(0, ryn::component::ButtonVisualLayer::background).color
                    == channels(ryn::resolve_theme().button().danger_background),
            "Danger Button normal state did not use its component token");
    fixture.host->pointer().dispatch(pointer_event(
        ryn::input::PointerAction::move, fixture.center(0)));
    require(fixture.synchronize(), "Default hover did not synchronize");
    require(fixture.layer(0, ryn::component::ButtonVisualLayer::background).color
                    == channels(ryn::resolve_theme().button().danger_hover_background),
            "Danger Button hover state did not use its component token");
    fixture.host->pointer().dispatch(pointer_event(
        ryn::input::PointerAction::down,
        fixture.center(0),
        ryn::input::PointerButton::primary));
    require(fixture.layer(0, ryn::component::ButtonVisualLayer::background).color
                    == channels(ryn::resolve_theme().button().danger_active_background),
            "Danger Button active state did not use its component token");
    loading.set(true);
    const auto danger_shadow = fixture.host->button_scene().shadow_effects(target.scene);
    require(fixture.layer(0, ryn::component::ButtonVisualLayer::background).color
                    == channels(ryn::resolve_theme().button().danger_background)
                && danger_shadow.size() == 1
                && fixture.host->rounded_effects().at(danger_shadow.front()).material.opacity
                    == ryn::resolve_theme().button().loading_opacity,
            "Danger loading precedence lost its normal color or faded shadow");
    loading.set(false);
    disabled.set(true);
    require(fixture.layer(0, ryn::component::ButtonVisualLayer::background).color
                    == channels(ryn::resolve_theme().button().disabled_background)
                && fixture.host->rounded_effects().at(danger_shadow.front()).material.opacity
                    == 0.0F,
            "disabled state did not win over Danger interaction and shadow");
}

void test_solid_border_box_and_focus_modalities_at_simulated_dpi() {
    Fixture fixture;
    fixture.host->set_motion_preference(
        ryn::animation::MotionPreference::reduced);
    fixture.host->mount(ryn::Content{[] {
        ryn::Button(
            ryn::ButtonProps{}.type(ryn::ButtonType::Default),
            [] { ryn::Text(u8"Default"); });
        ryn::Button(
            ryn::ButtonProps{}.type(ryn::ButtonType::Primary),
            [] { ryn::Text(u8"Primary"); });
        ryn::Button(
            ryn::ButtonProps{}.type(ryn::ButtonType::Danger),
            [] { ryn::Text(u8"Danger"); });
    }});
    require(fixture.synchronize(), "Button visual matrix did not synchronize");

    const auto default_button = fixture.host->mounted_buttons()[0];
    const auto primary_button = fixture.host->mounted_buttons()[1];
    const auto danger_button = fixture.host->mounted_buttons()[2];
    const auto default_root = fixture.bounds(0);
    const auto primary_root = fixture.bounds(1);
    const auto danger_root = fixture.bounds(2);
    const auto default_background = quad_bounds(fixture.layer(
        0, ryn::component::ButtonVisualLayer::background));
    const auto primary_background = quad_bounds(fixture.layer(
        1, ryn::component::ButtonVisualLayer::background));
    const auto danger_background = quad_bounds(fixture.layer(
        2, ryn::component::ButtonVisualLayer::background));

    require(near(default_background.x, default_root.x + 1.0F)
                && near(default_background.y, default_root.y + 1.0F)
                && near(default_background.width, default_root.width - 2.0F)
                && near(default_background.height, default_root.height - 2.0F)
                && near_rect(primary_background, primary_root)
                && near_rect(danger_background, danger_root)
                && fixture.layer(1, ryn::component::ButtonVisualLayer::border)
                    .color[3] == 0.0F
                && fixture.layer(2, ryn::component::ButtonVisualLayer::border)
                    .color[3] == 0.0F,
            "solid Button fill did not cover the transparent border box");
    require(fixture.host->hit_test().hit_test(fixture.center(0))
                    == default_button.interaction
                && fixture.host->hit_test().hit_test(fixture.center(1))
                    == primary_button.interaction
                && fixture.host->hit_test().hit_test(fixture.center(2))
                    == danger_button.interaction,
            "solid border-box painting changed Button HitTest geometry");

    for (const float scale : std::array{1.0F, 1.25F, 1.5F, 2.0F}) {
        const auto& primary_fill = fixture.layer(
            1, ryn::component::ButtonVisualLayer::background);
        require(near(primary_background.x * scale, primary_root.x * scale)
                    && near(primary_background.y * scale, primary_root.y * scale)
                    && near(primary_background.width * scale, primary_root.width * scale)
                    && near(primary_background.height * scale, primary_root.height * scale)
                    && near(
                        primary_fill.corner_radius
                            * std::min(primary_root.width, primary_root.height)
                            * scale,
                        6.0F * scale),
                "solid Button border-box geometry drifted at simulated DPI");
    }

    fixture.host->pointer().dispatch(pointer_event(
        ryn::input::PointerAction::move, fixture.center(0)));
    require(fixture.layer(0, ryn::component::ButtonVisualLayer::border).color
                    == channels(ryn::resolve_theme().button().default_hover_color),
            "Default hover missed its existing 1px border");
    require(fixture.host->button_scene().focus_effect(
                default_button.scene).material.opacity == 0.0F,
            "Default hover incorrectly created a focus ring");

    fixture.host->pointer().dispatch(pointer_event(
        ryn::input::PointerAction::move, fixture.center(1)));
    require(fixture.synchronize(), "Primary hover did not synchronize");
    require(fixture.layer(1, ryn::component::ButtonVisualLayer::background).color
                    == channels(
                        ryn::resolve_theme().button().primary_hover_background)
                && fixture.layer(1, ryn::component::ButtonVisualLayer::border)
                    .color[3] == 0.0F
                && fixture.host->button_scene().focus_effect(
                    primary_button.scene).material.opacity == 0.0F
                && near_rect(
                    quad_bounds(fixture.layer(
                        1, ryn::component::ButtonVisualLayer::background)),
                    primary_root),
            "Primary hover introduced a border, focus ring, or transparent edge");

    fixture.host->pointer().dispatch(pointer_event(
        ryn::input::PointerAction::move, fixture.center(2)));
    require(fixture.synchronize(), "Danger hover did not synchronize");
    require(fixture.layer(2, ryn::component::ButtonVisualLayer::background).color
                    == channels(
                        ryn::resolve_theme().button().danger_hover_background)
                && fixture.layer(2, ryn::component::ButtonVisualLayer::border)
                    .color[3] == 0.0F
                && fixture.host->button_scene().focus_effect(
                    danger_button.scene).material.opacity == 0.0F
                && near_rect(
                    quad_bounds(fixture.layer(
                        2, ryn::component::ButtonVisualLayer::background)),
                    danger_root),
            "Danger hover introduced a blue border, focus ring, or transparent edge");

    fixture.host->pointer().dispatch(pointer_event(
        ryn::input::PointerAction::move, fixture.center(1)));
    fixture.host->pointer().dispatch(pointer_event(
        ryn::input::PointerAction::down,
        fixture.center(1),
        ryn::input::PointerButton::primary));
    fixture.host->pointer().dispatch(pointer_event(
        ryn::input::PointerAction::up,
        fixture.center(1),
        ryn::input::PointerButton::primary));
    require(fixture.host->focus().state().focused == primary_button.interaction
                && !fixture.host->snapshot(primary_button.component)
                    .focus.focus_visible
                && fixture.host->button_scene().focus_effect(
                    primary_button.scene).material.opacity == 0.0F,
            "pointer focus incorrectly enabled the keyboard focus ring");

    fixture.host->focus().dispatch(key(
        ryn::input::Key::tab,
        ryn::input::KeyAction::down));
    const auto& focus = fixture.host->button_scene().focus_effect(
        danger_button.scene);
    const auto& shape = focus.geometry.shape.rect;
    const float mid_y = shape.y + shape.height * 0.5F;
    require(fixture.host->focus().state().focused == danger_button.interaction
                && fixture.host->snapshot(danger_button.component)
                    .focus.focus_visible
                && focus.material.opacity == 1.0F
                && focus.material.color
                    == ryn::resolve_theme().map().color_primary_border
                && focus.geometry.outline_width == 3.0F
                && focus.geometry.outline_offset == 1.0F,
            "keyboard focus did not use the locked hollow focus outline");

    for (const float scale : std::array{1.0F, 1.25F, 1.5F, 2.0F}) {
        const ryn::graphics::RoundedEffectDeviceMetrics metrics{
            static_cast<std::uint32_t>(std::lround(640.0F * scale)),
            static_cast<std::uint32_t>(std::lround(360.0F * scale)),
            scale,
        };
        const auto packed = ryn::graphics::pack_rounded_effect_instance(
            focus, metrics);
        require(near(packed.effect_params[1], 3.0F * scale)
                    && near(packed.effect_params[2], 1.0F * scale)
                    && ryn::graphics::rounded_effect_coverage(
                        {shape.x + shape.width + 0.25F, mid_y}, focus) == 0.0F
                    && ryn::graphics::rounded_effect_coverage(
                        {shape.x + shape.width + 2.5F, mid_y}, focus) > 0.99F
                    && ryn::graphics::rounded_effect_gpu_coverage_reference(
                        {
                            (shape.x + shape.width + 0.25F) * scale,
                            mid_y * scale,
                        },
                        packed) == 0.0F
                    && ryn::graphics::rounded_effect_gpu_coverage_reference(
                        {
                            (shape.x + shape.width + 2.5F) * scale,
                            mid_y * scale,
                        },
                        packed) > 0.99F,
                "focus gap/ring CPU and GPU references diverged at simulated DPI");
    }

    fixture.host->pointer().dispatch(pointer_event(
        ryn::input::PointerAction::move, fixture.center(0)));
    require(fixture.host->button_scene().focus_effect(
                default_button.scene).material.opacity == 0.0F
                && fixture.host->button_scene().focus_effect(
                    danger_button.scene).material.opacity == 1.0F,
            "pointer hover copied keyboard focus-visible state to another Button");
}

void test_animated_presentation_retarget_and_motion_policy() {
    Fixture fixture;
    ryn::Signal<bool> loading{false};
    ryn::ThemeConfig theme;
    ryn::Signal<ryn::ThemeConfig> theme_config{theme};
    fixture.host->mount(ryn::Content{[&] {
        ryn::Theme(
            ryn::ThemeProps{}.config(theme_config),
            ryn::ThemeContent{[&] {
                ryn::Button(
                    ryn::ButtonProps{}.loading(loading),
                    [] { ryn::Text(u8"Animated Button"); });
            }});
    }});
    require(fixture.synchronize(), "animated Button fixture did not synchronize");
    const auto mounted = fixture.host->mounted_buttons().front();
    const auto initial = fixture.host->snapshot(mounted.component);
    const auto measure_count = fixture.nodes.require(mounted.node).measure_count;
    const auto scene = mounted.scene;
    clear_observation_state(fixture);

    fixture.host->set_animation_time(
        ryn::animation::AnimationTime::microseconds(0));
    fixture.host->pointer().dispatch(pointer_event(
        ryn::input::PointerAction::move, fixture.center(0)));
    const auto hover_started = fixture.host->snapshot(mounted.component);
    require(hover_started.hovered
                && hover_started.presentation_border == initial.presentation_border
                && fixture.host->animations().size() == 2
                && fixture.dirty.layout_roots().empty(),
            "hover did not separate synchronous state from animated presentation");

    static_cast<void>(fixture.tick(100'000));
    const auto hover_mid = fixture.host->snapshot(mounted.component);
    const auto hover_target = ryn::resolve_theme().button().default_hover_color;
    require(hover_mid.presentation_border != initial.presentation_border
                && hover_mid.presentation_border != hover_target
                && fixture.dirty.material_nodes().size() == 2
                && fixture.dirty.animation_nodes().size() == 1
                && fixture.dirty.layout_roots().empty()
                && fixture.dirty.hit_test_nodes().empty(),
            "hover midpoint did not produce local Material/Animation invalidation");
    const auto before_retarget = hover_mid.presentation_border;
    const auto retargeted_before = fixture.host->animations().diagnostics().retargeted;

    fixture.host->set_animation_time(
        ryn::animation::AnimationTime::microseconds(100'000));
    fixture.host->pointer().dispatch(pointer_event(
        ryn::input::PointerAction::down,
        fixture.center(0),
        ryn::input::PointerButton::primary));
    const auto active_started = fixture.host->snapshot(mounted.component);
    require(active_started.pointer_pressed
                && active_started.presentation_border == before_retarget
                && fixture.host->animations().diagnostics().retargeted
                    > retargeted_before,
            "active retarget jumped instead of sampling the current presentation");
    static_cast<void>(fixture.tick(200'000));
    const auto active_mid = fixture.host->snapshot(mounted.component);
    require(active_mid.presentation_border != before_retarget
                && active_mid.presentation_border
                    != ryn::resolve_theme().button().default_active_color,
            "active retarget did not interpolate from the hover midpoint");

    auto dark = theme;
    dark.algorithms = {ryn::ThemeAlgorithm::Dark};
    fixture.host->set_animation_time(
        ryn::animation::AnimationTime::microseconds(200'000));
    require(theme_config.set(dark), "animated Theme switch was ignored");
    const auto theme_started = fixture.host->snapshot(mounted.component);
    require(theme_started.presentation_border == active_mid.presentation_border,
            "Theme switch jumped instead of retargeting current presentation");
    static_cast<void>(fixture.tick(300'000));
    require(fixture.host->snapshot(mounted.component).presentation_border
                != theme_started.presentation_border,
            "Theme switch did not animate the retained presentation");
    fixture.host->set_animation_time(
        ryn::animation::AnimationTime::microseconds(300'000));
    require(theme_config.set(theme), "default Theme restoration was ignored");

    fixture.host->focus().dispatch(key(
        ryn::input::Key::tab,
        ryn::input::KeyAction::down));
    require(fixture.host->button_scene().focus_effect(scene).material.opacity == 1.0F,
            "focus-visible outline was incorrectly animated");

    loading.set(true);
    const auto loading_started = fixture.host->snapshot(mounted.component);
    require(loading_started.loading
                && !fixture.host->snapshot(mounted.component).pointer_pressed,
            "loading state did not synchronously settle interaction eligibility");
    fixture.host->set_motion_preference(
        ryn::animation::MotionPreference::reduced);
    const auto reduced = fixture.host->snapshot(mounted.component);
    require(fixture.host->animations().size() == 0
                && !fixture.host->animations().next_deadline().has_value()
                && reduced.presentation_loading_mix == 1.0F
                && reduced.presentation_background
                    == ryn::resolve_theme().button().default_background
                && fixture.nodes.require(mounted.node).measure_count == measure_count
                && fixture.host->mounted_buttons().front().scene == scene,
            "reduced motion did not snap to final retained Button presentation");
}

void test_retained_loading_spinner_phase_and_policy_lifecycle() {
    Fixture fixture;
    ryn::ThemeConfig theme;
    ryn::Signal<ryn::ThemeConfig> theme_config{theme};
    ryn::Signal<bool> loading{true};
    fixture.host->mount(ryn::Content{[&] {
        ryn::Theme(
            ryn::ThemeProps{}.config(theme_config),
            ryn::ThemeContent{[&] {
                ryn::Button(
                    ryn::ButtonProps{}.loading(loading),
                    [] { ryn::Text(u8"First spinner"); });
                ryn::Button(
                    ryn::ButtonProps{}.loading(loading),
                    [] { ryn::Text(u8"Second spinner"); });
            }});
    }});
    require(fixture.synchronize(), "loading spinner fixture did not synchronize");
    const auto first = fixture.host->mounted_buttons()[0];
    const auto second = fixture.host->mounted_buttons()[1];
    const auto first_range = fixture.host->button_scene().visual_range(first.scene);
    const auto second_range = fixture.host->button_scene().visual_range(second.scene);
    const auto instance_count = fixture.host->button_scene().instances().size();
    const auto scene_rebuilds = fixture.host->scene_composer().diagnostics().rebuilds;
    require(first_range.count == ryn::component::button_visual_layer_count
                && second_range.count == ryn::component::button_visual_layer_count
                && instance_count == 2 * ryn::component::button_visual_layer_count
                && fixture.host->animations().size() == 2
                && fixture.host->snapshot(first.component).spinner_running
                && fixture.host->snapshot(second.component).spinner_running,
            "loading Buttons did not retain eight segments and one phase each");
    for (std::size_t segment = 0;
         segment < ryn::component::button_loading_segment_count;
         ++segment) {
        const auto& visual = fixture.loading_segment(0, segment);
        require(visual.clip_rect[2] > 0.0F
                    && visual.clip_rect[3] < 0.0F
                    && visual.corner_radius == 0.5F
                    && visual.opacity > 0.0F,
                "loading spinner segment geometry or visibility is invalid");
    }
    const auto& scale_reference = fixture.loading_segment(0, 0);
    require(near(
                std::fabs(scale_reference.clip_rect[2]) * 0.5F * 960.0F,
                14.0F * 0.22F * 1.5F,
                0.01F),
            "loading spinner segment did not scale from logical to 150 percent pixels");

    clear_observation_state(fixture);
    static_cast<void>(fixture.tick(50'000));
    const auto first_tick = fixture.host->snapshot(first.component);
    const auto second_tick = fixture.host->snapshot(second.component);
    const auto& material_ranges =
        fixture.host->button_scene().instances().material_dirty_ranges();
    require(near(first_tick.spinner_phase, 0.0625F)
                && near(second_tick.spinner_phase, 0.0625F)
                && material_ranges.size() == 2
                && material_ranges[0] == ryn::graphics::QuadInstanceRange{
                    first_range.first + 2,
                    ryn::component::button_loading_segment_count}
                && material_ranges[1] == ryn::graphics::QuadInstanceRange{
                    second_range.first + 2,
                    ryn::component::button_loading_segment_count}
                && fixture.dirty.material_nodes().size() == 2
                && fixture.dirty.animation_nodes().size() == 2
                && fixture.dirty.layout_roots().empty()
                && fixture.dirty.hit_test_nodes().empty(),
            "spinner phase escaped its exact Material/Animation ranges");

    clear_observation_state(fixture);
    static_cast<void>(fixture.tick(800'000));
    require(near(fixture.host->snapshot(first.component).spinner_phase, 0.0F)
                && fixture.host->snapshot(first.component).spinner_running
                && fixture.host->animations().size() == 2,
            "spinner phase did not wrap and continue independent of skipped cadence");

    require(fixture.host->destroy(first.component)
                && fixture.host->animations().size() == 1
                && fixture.host->animations().diagnostics().scopes == 1
                && fixture.host->animations().diagnostics().targets == 5
                && fixture.host->mounted_buttons().front().component == second.component
                && fixture.host->mounted_buttons().front().scene == second.scene,
            "spinner owner destroy leaked its phase or changed sibling identity");

    auto motion_disabled = theme;
    motion_disabled.seed.motion = false;
    require(theme_config.set(motion_disabled)
                && fixture.host->animations().size() == 0
                && !fixture.host->animations().next_deadline().has_value(),
            "Theme motion=false retained a spinner deadline");
    const auto static_spinner = fixture.host->snapshot(second.component);
    require(!static_spinner.spinner_running
                && near(static_spinner.spinner_phase, 0.0F)
                && fixture.loading_segment(0, 0).opacity
                    > fixture.loading_segment(0, 4).opacity,
            "Theme motion=false did not retain a recognizable static indicator");

    require(theme_config.set(theme)
                && fixture.host->snapshot(second.component).spinner_running
                && fixture.host->animations().size() == 1,
            "Theme motion restoration did not restart the retained spinner");
    fixture.host->set_motion_preference(
        ryn::animation::MotionPreference::reduced);
    require(!fixture.host->snapshot(second.component).spinner_running
                && fixture.host->animations().size() == 0
                && !fixture.host->animations().next_deadline().has_value(),
            "reduced motion retained a spinner deadline");
    fixture.host->set_motion_preference(
        ryn::animation::MotionPreference::normal);
    require(fixture.host->snapshot(second.component).spinner_running,
            "normal motion did not restart the spinner");

    loading.set(false);
    require(!fixture.host->snapshot(second.component).spinner_running,
            "loading=false retained continuous spinner phase");
    static_cast<void>(fixture.tick(1'000'000));
    require(fixture.host->animations().size() == 0
                && !fixture.host->animations().next_deadline().has_value()
                && near(
                    fixture.host->snapshot(second.component)
                        .presentation_loading_mix,
                    0.0F)
                && fixture.host->mounted_buttons().front().scene == second.scene
                && fixture.host->scene_composer().diagnostics().rebuilds
                    == scene_rebuilds
                && fixture.host->button_scene().instances().size()
                    == instance_count - ryn::component::button_visual_layer_count,
            "loading completion did not recover idle with retained sibling topology");
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
    require(clicks == 1 && callback_observed_settled_state
                && fixture.host->focus().state().focused
                    == fixture.host->mounted_buttons()[0].interaction
                && !fixture.host->snapshot(component).focus.focus_visible
                && fixture.host->button_scene().focus_effect(
                    fixture.host->mounted_buttons()[0].scene).material.opacity == 0.0F,
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

void test_nested_theme_override_updates_button_without_remounting() {
    Fixture fixture;
    fixture.host->set_motion_preference(
        ryn::animation::MotionPreference::reduced);
    ryn::ThemeConfig initial;
    const auto initial_color = ryn::Color::rgba8(114, 46, 209);
    initial.button.tokens.danger_background = initial_color;
    initial.button.tokens.border_radius = ryn::dp(9.0F);
    initial.button.tokens.danger_shadow = ryn::ShadowList{
        {ryn::ShadowKind::outer, {0.0F, 2.0F}, 4.0F, 0.0F,
         ryn::Color::rgba8(80, 20, 120, 80)},
    };
    ryn::Signal<ryn::ThemeConfig> config{initial};
    int content_runs = 0;
    fixture.host->mount(ryn::Content{[&] {
        ryn::Theme(
            ryn::ThemeProps{}.config(config),
            ryn::ThemeContent{[&] {
                ryn::Button(
                    ryn::ButtonProps{}.type(ryn::ButtonType::Danger),
                    [&] {
                        ++content_runs;
                        ryn::Text(u8"Nested danger");
                    });
            }});
    }});
    require(fixture.synchronize(), "nested Theme Button did not synchronize");
    const auto target = fixture.host->mounted_buttons().front();
    const auto text = fixture.host->text().mounted_texts().front().scene;
    const auto text_counters = fixture.text_scene.text_state(text).counters();
    const auto component_count = fixture.host->components().component_count();
    const auto scene_rebuilds = fixture.host->scene_composer().diagnostics().rebuilds;
    require(fixture.layer(0, ryn::component::ButtonVisualLayer::background).color
                    == channels(initial_color)
                && fixture.host->button_scene().shadow_effects(target.scene).size() == 1,
            "nested Theme Button did not apply initial color and shadow overrides");
    clear_observation_state(fixture);

    auto recolored = initial;
    const auto next_color = ryn::Color::rgba8(250, 140, 22);
    recolored.button.tokens.danger_background = next_color;
    require(config.set(recolored)
                && fixture.layer(0, ryn::component::ButtonVisualLayer::background).color
                    == channels(next_color)
                && fixture.dirty.layout_roots().empty(),
            "nested Button color token escaped Material invalidation");
    require(fixture.synchronize(), "nested Button color update did not synchronize");
    require(content_runs == 1
                && fixture.host->components().component_count() == component_count
                && fixture.host->mounted_buttons().front().component == target.component
                && fixture.host->mounted_buttons().front().scene == target.scene
                && fixture.host->scene_composer().diagnostics().rebuilds == scene_rebuilds
                && fixture.text_scene.text_state(text).counters().shape_count
                    == text_counters.shape_count,
            "nested Button color update remounted content, scene, or Text");

    clear_observation_state(fixture);
    auto effects = recolored;
    effects.button.tokens.border_radius = ryn::dp(12.0F);
    effects.button.tokens.danger_shadow = ryn::ShadowList{
        {ryn::ShadowKind::outer, {0.0F, 2.0F}, 4.0F, 0.0F,
         ryn::Color::rgba8(80, 20, 120, 70)},
        {ryn::ShadowKind::outer, {0.0F, 6.0F}, 12.0F, 1.0F,
         ryn::Color::rgba8(80, 20, 120, 40)},
    };
    require(config.set(effects), "nested Button effect Theme update was ignored");
    require(fixture.synchronize(), "nested Button effect update did not synchronize");
    require(content_runs == 1
                && fixture.host->mounted_buttons().front().component == target.component
                && fixture.host->mounted_buttons().front().scene == target.scene
                && fixture.host->button_scene().shadow_effects(target.scene).size() == 2
                && fixture.host->button_scene().focus_effect(target.scene)
                    .geometry.shape.radius == 12.0F
                && fixture.text_scene.text_state(text).counters().shape_count
                    == text_counters.shape_count,
            "nested Button effect update rebuilt identity or missed radius/shadow tokens");
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
        test_solid_border_box_and_focus_modalities_at_simulated_dpi();
        test_animated_presentation_retarget_and_motion_policy();
        test_retained_loading_spinner_phase_and_policy_lifecycle();
        test_pointer_keyboard_click_path_and_callback_mutation();
        test_nested_theme_override_updates_button_without_remounting();
        test_click_callback_can_destroy_parent_scope();
        test_flex_composes_text_button_and_nested_flex();
    } catch (const std::exception& error) {
        std::cerr << error.what() << '\n';
        return 1;
    }
    return 0;
}
