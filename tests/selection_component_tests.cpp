#include "component/selection_component.hpp"
#include "support/input_fixture.hpp"

#include <ryn/rynui.hpp>

#include <array>
#include <cmath>
#include <iostream>
#include <stdexcept>

namespace {
using namespace ryn;
using namespace ryn::input;
using Fixture = ryn_test::input_component::Fixture;

void require(bool value, const char* message) {
    if (!value) throw std::runtime_error(message);
}

PointerInputEvent pointer(PointerAction action, runtime::Point point,
    PointerButton button = PointerButton::none) {
    return {PointerIdentity::mouse(), action, button, point.x, point.y};
}

runtime::Point center(const Fixture& fixture, runtime::NodeId node) {
    const auto bounds = fixture.nodes.require(node).bounds;
    return {bounds.x + bounds.width / 2.0F, bounds.y + bounds.height / 2.0F};
}

runtime::Rect quad_bounds(const graphics::QuadInstance& quad,
    runtime::Size viewport = {320.0F, 240.0F}) {
    return {(quad.clip_rect[0] + 1.0F) * viewport.width / 2.0F,
        (1.0F - quad.clip_rect[1]) * viewport.height / 2.0F,
        quad.clip_rect[2] * viewport.width / 2.0F,
        -quad.clip_rect[3] * viewport.height / 2.0F};
}

bool near(float left, float right) { return std::fabs(left - right) < 0.02F; }

KeyboardInputEvent key(Key value, KeyAction action, bool repeat = false) {
    return {value, action, KeyModifier::none, repeat};
}

void keyboard_and_modes() {
    Fixture f;
    detail::SelectionComponentHost selection{f.services};
    Signal<bool> controlled{false}, loading{false}, disabled{false};
    int switch_changes{}, checkbox_changes{}, label_runs{};
    f.buttons.mount(Content{[&] {
        Switch(SwitchProps{}.checked(controlled).loading(loading)
            .onChange([&](bool value) { ++switch_changes; controlled.set(value); }));
        Checkbox(CheckboxProps{}.defaultChecked(false).disabled(disabled)
            .indeterminate(true).onChange([&](bool) { ++checkbox_changes; }),
            CheckboxLabel{[&] { ++label_runs; Text(u8"选择 Choice"); }});
    }});
    require(selection.mounted().size() == 2, "selection controls did not mount");
    f.synchronize();
    const auto sw = selection.mounted()[0];
    const auto cb = selection.mounted()[1];
    require(!selection.snapshot(sw.component).checked && !selection.snapshot(cb.component).checked,
        "selection initial state mismatch");
    require(f.services.focus().request_focus(sw.interaction, FocusModality::keyboard),
        "Switch focus failed");
    f.services.focus().dispatch(key(Key::enter, KeyAction::down));
    require(switch_changes == 0, "Enter toggled Switch");
    f.services.focus().dispatch(key(Key::space, KeyAction::down));
    f.services.focus().dispatch(key(Key::space, KeyAction::down, true));
    require(selection.snapshot(sw.component).focus.keyboard_pressed,
        "Switch did not show keyboard pressed state");
    f.services.focus().dispatch(key(Key::space, KeyAction::up));
    require(switch_changes == 1 && controlled.get() && selection.snapshot(sw.component).checked,
        "controlled Switch did not echo one toggle");
    require(label_runs == 1, "Switch update reran Checkbox label slot");
    loading.set(true);
    require(f.services.focus().state().focused == sw.interaction,
        "loading Switch lost focus identity");
    f.services.focus().dispatch(key(Key::space, KeyAction::down));
    f.services.focus().dispatch(key(Key::space, KeyAction::up));
    require(switch_changes == 1, "loading Switch toggled");
    loading.set(false);
    require(f.services.focus().request_focus(cb.interaction, FocusModality::keyboard),
        "Checkbox focus failed");
    f.services.focus().dispatch(key(Key::space, KeyAction::down));
    f.services.focus().dispatch(key(Key::space, KeyAction::up));
    require(checkbox_changes == 1 && selection.snapshot(cb.component).checked
        && selection.snapshot(cb.component).indeterminate,
        "Checkbox toggle changed independent indeterminate state");
    disabled.set(true);
    require(!f.services.focus().state().focused,
        "disabled Checkbox stayed in Tab focus");
    require(!selection.snapshot(cb.component).focus.focused,
        "disabled Checkbox retained presentation focus");
}

void pointer_and_lifecycle() {
    Fixture f;
    detail::SelectionComponentHost selection{f.services};
    int changes{};
    f.buttons.mount(Content{[&] {
        Switch(SwitchProps{}.defaultChecked(false).onChange([&](bool) { ++changes; }));
        Checkbox(CheckboxProps{}.defaultChecked(false));
    }});
    f.synchronize();
    const auto sw = selection.mounted()[0];
    const auto inside = center(f, sw.node);
    const runtime::Point outside{310.0F, 230.0F};
    f.services.pointer().dispatch(pointer(PointerAction::down, inside, PointerButton::primary));
    require(selection.snapshot(sw.component).pointer_pressed, "Switch did not capture press");
    f.services.set_window_active(false);
    require(!selection.snapshot(sw.component).pointer_pressed,
        "window deactivation retained Switch capture");
    f.services.pointer().dispatch(pointer(PointerAction::up, inside, PointerButton::primary));
    require(changes == 0, "window-deactivated Switch press activated");
    f.services.set_window_active(true);
    f.services.pointer().dispatch(pointer(PointerAction::down, inside, PointerButton::primary));
    f.services.pointer().dispatch(pointer(PointerAction::move, outside));
    f.services.pointer().dispatch(pointer(PointerAction::up, outside, PointerButton::primary));
    require(changes == 0 && !selection.snapshot(sw.component).pointer_pressed,
        "drag-out Switch release toggled");
    f.services.pointer().dispatch(pointer(PointerAction::down, inside, PointerButton::primary));
    f.services.pointer().dispatch(pointer(PointerAction::up, inside, PointerButton::primary));
    require(changes == 1 && selection.snapshot(sw.component).checked,
        "complete Switch pointer click failed");
    const auto surface = sw.surface;
    require(f.services.surfaces().visual_range(surface).count == 10,
        "Switch scene topology is not retained");
    require(f.services.destroy(sw.component), "Switch destroy failed");
    require(selection.mounted().size() == 1
        && f.services.surfaces().visual_range(selection.mounted()[0].surface).count == 15,
        "Switch destroy damaged sibling scene");
}

void invalid_and_self_destroy() {
    Fixture invalid_switch;
    detail::SelectionComponentHost switch_host{invalid_switch.services};
    bool rejected{};
    try {
        switch_host.mount(Content{[] { Switch(SwitchProps{}.checked(true).defaultChecked(false)); }});
    } catch (const std::invalid_argument&) { rejected = true; }
    require(rejected && switch_host.mounted().empty()
        && invalid_switch.services.interactions().size() == 0,
        "conflicting Switch mode acquired identities");
    Fixture invalid_checkbox;
    detail::SelectionComponentHost checkbox_host{invalid_checkbox.services};
    rejected = false;
    try {
        checkbox_host.mount(Content{[] { Checkbox(CheckboxProps{}.checked(true).defaultChecked(false)); }});
    } catch (const std::invalid_argument&) { rejected = true; }
    require(rejected && checkbox_host.mounted().empty()
        && invalid_checkbox.services.interactions().size() == 0,
        "conflicting Checkbox mode acquired identities");
    Fixture f;
    detail::SelectionComponentHost selection{f.services};
    runtime::ComponentId id;
    int calls{};
    selection.mount(Content{[&] {
        Switch(SwitchProps{}.onChange([&](bool) {
            ++calls;
            require(f.services.destroy(id), "self-destroy failed");
        }));
    }});
    f.synchronize();
    const auto mounted = selection.mounted().front();
    id = mounted.component;
    const auto point = center(f, mounted.node);
    f.services.pointer().dispatch(pointer(PointerAction::down, point, PointerButton::primary));
    f.services.pointer().dispatch(pointer(PointerAction::up, point, PointerButton::primary));
    require(calls == 1 && selection.mounted().empty()
        && f.services.interactions().size() == 0,
        "self-destroy delivered duplicate activation or leaked identity");
    Fixture checkbox_fixture;
    detail::SelectionComponentHost checkbox_selection{checkbox_fixture.services};
    runtime::ComponentId checkbox_id;
    int checkbox_calls{};
    checkbox_selection.mount(Content{[&] {
        Checkbox(CheckboxProps{}.onChange([&](bool) {
            ++checkbox_calls;
            require(checkbox_fixture.services.destroy(checkbox_id),
                "Checkbox self-destroy failed");
        }));
    }});
    checkbox_fixture.synchronize();
    const auto checkbox = checkbox_selection.mounted().front();
    checkbox_id = checkbox.component;
    const auto checkbox_point = center(checkbox_fixture, checkbox.node);
    checkbox_fixture.services.pointer().dispatch(pointer(
        PointerAction::down, checkbox_point, PointerButton::primary));
    checkbox_fixture.services.pointer().dispatch(pointer(
        PointerAction::up, checkbox_point, PointerButton::primary));
    require(checkbox_calls == 1 && checkbox_selection.mounted().empty()
        && checkbox_fixture.services.interactions().size() == 0,
        "Checkbox self-destroy duplicated activation or leaked identity");
}

void checkbox_label_hit_area() {
    Fixture f;
    detail::SelectionComponentHost selection{f.services};
    int changes{};
    selection.mount(Content{[&] {
        Checkbox(CheckboxProps{}.onChange([&](bool) { ++changes; }),
            CheckboxLabel{[] { Text(u8"选择 Choice"); }});
    }});
    f.synchronize();
    const auto cb = selection.mounted().front();
    const auto root = f.nodes.require(cb.node).bounds;
    const auto box = quad_bounds(f.services.surfaces().instances().at(
        f.services.surfaces().visual_range(cb.surface).first));
    require(root.width > box.width + 20.0F,
        "Checkbox label did not extend the interaction area");
    const runtime::Point label_point{root.x + root.width - 4.0F,
        root.y + root.height / 2.0F};
    f.services.pointer().dispatch(pointer(PointerAction::down, label_point,
        PointerButton::primary));
    f.services.pointer().dispatch(pointer(PointerAction::up, label_point,
        PointerButton::primary));
    require(changes == 1 && selection.snapshot(cb.component).checked,
        "clicking Checkbox label did not toggle its control");
}

void generation_reuse() {
    Fixture f;
    detail::SelectionComponentHost selection{f.services};
    detail::MountedSelectionComponent old;
    selection.mount(Content{[&] {
        Switch(SwitchProps{}.defaultChecked(false));
        old = selection.mounted().front();
        require(f.services.destroy(old.component), "old Switch destroy failed");
        Switch(SwitchProps{}.defaultChecked(true));
    }});
    f.synchronize();
    require(selection.mounted().size() == 1, "Switch generation reuse leaked old mount");
    const auto replacement = selection.mounted().front();
    require(replacement.component.index == old.component.index
        && replacement.component.generation != old.component.generation
        && selection.snapshot(replacement.component).checked
        && !f.services.focus().request_focus(old.interaction, FocusModality::keyboard),
        "destroy/reuse accepted a stale Switch generation");
}

void visuals_and_local_updates() {
    for (const float scale : {1.0F, 1.25F, 1.5F, 2.0F}) {
        Fixture f;
        f.font_scale = scale;
        detail::SelectionComponentHost selection{f.services};
        ThemeConfig config;
        Signal<ThemeConfig> theme{config};
        Signal<bool> checked{false};
        int label_runs{};
        f.buttons.mount(Content{[&] {
            Theme(ThemeProps{}.config(theme), ThemeContent{[&] {
                Button(ButtonProps{}, ButtonContent{[] { Text(u8"按钮"); }});
                Input(InputProps{}.defaultValue(u8"Input"));
                Switch(SwitchProps{}.checked(checked));
                Switch(SwitchProps{}.size(SwitchSize::Small).defaultChecked(true));
                Checkbox(CheckboxProps{}.indeterminate(true),
                    CheckboxLabel{[&] { ++label_runs; Text(u8"选择 Choice"); }});
            }});
        }});
        f.synchronize();
        require(selection.mounted().size() == 3 && label_runs == 1,
            "mixed selection controls did not mount once");
        const auto middle = selection.mounted()[0];
        const auto small = selection.mounted()[1];
        const auto mixed = selection.mounted()[2];
        auto& surfaces = f.services.surfaces();
        const auto middle_range = surfaces.visual_range(middle.surface);
        const auto small_range = surfaces.visual_range(small.surface);
        const auto mixed_range = surfaces.visual_range(mixed.surface);
        require(middle_range.count == 10 && small_range.count == 10
            && mixed_range.count == 15, "selection scene layer topology changed");
        const auto middle_track = quad_bounds(surfaces.instances().at(middle_range.first));
        const auto small_track = quad_bounds(surfaces.instances().at(small_range.first));
        require(middle_track.width > small_track.width
            && middle_track.height > small_track.height,
            "Switch source sizes did not produce distinct geometry");
        require(near(surfaces.instances().at(middle_range.first).color[3], 0.25F),
            "unchecked Switch track lost colorTextQuaternary mapping");
        const auto box = quad_bounds(surfaces.instances().at(mixed_range.first));
        const auto square = quad_bounds(surfaces.instances().at(mixed_range.first + 14));
        require(near(square.width, f.services.components().theme_scope(mixed.component)
                ->snapshot().map().font_size_large / 2.0F)
            && near(square.x + square.width / 2.0F, box.x + box.width / 2.0F)
            && near(square.y + square.height / 2.0F, box.y + box.height / 2.0F),
            "indeterminate square is not centered at fontSizeLG / 2");
        surfaces.instances().clear_dirty_ranges();
        const auto mount_runs = f.services.components().mount_runs();
        const auto scene_creates = surfaces.diagnostics().creates;
        checked.set(true);
        f.synchronize();
        require(f.services.components().mount_runs() == mount_runs
            && surfaces.diagnostics().creates == scene_creates && label_runs == 1,
            "checked change remounted a sibling or reran label slot");
        for (const auto range : surfaces.instances().material_dirty_ranges()) {
            require(range.first >= middle_range.first
                && range.first + range.count <= middle_range.first + middle_range.count,
                "checked update uploaded a sibling material range");
        }
        require(f.dirty.layout_roots().empty(), "checked update retained layout dirtiness");
        for (const auto algorithm : {ThemeAlgorithm::Dark, ThemeAlgorithm::Compact}) {
            config.algorithms = {algorithm};
            theme.set(config);
            f.synchronize();
            require(selection.mounted()[0].surface == middle.surface
                && selection.mounted()[2].surface == mixed.surface
                && label_runs == 1,
                "Theme update rebuilt selection identity or label");
        }
        surfaces.instances().clear_dirty_ranges();
        config.switch_.tokens.handle_background = Color::rgba8(245, 180, 55);
        theme.set(config);
        f.synchronize();
        const auto handle = surfaces.instances().at(middle_range.first + 1);
        require(near(handle.color[0], 245.0F / 255.0F),
            "Switch Component Token color override did not reach handle");
        require(f.services.components().mount_runs() == mount_runs,
            "Switch Component Token color override remounted content");
        require(f.dirty.layout_roots().empty(),
            "Switch Component Token color override measured content");
        config.switch_.tokens.track_min_width = dp(middle_track.width + 12.0F);
        theme.set(config);
        f.synchronize();
        const auto widened = quad_bounds(surfaces.instances().at(middle_range.first));
        require(near(widened.width, middle_track.width + 12.0F)
            && surfaces.diagnostics().creates == scene_creates
            && selection.mounted()[2].surface == mixed.surface,
            "Switch Component Token geometry override rebuilt a sibling surface");
    }
}

void loading_spinner_and_idle() {
    Fixture f;
    detail::SelectionComponentHost selection{f.services};
    Signal<bool> loading{true};
    int changes{};
    selection.mount(Content{[&] {
        Switch(SwitchProps{}.loading(loading).onChange([&](bool) { ++changes; }));
        Checkbox(CheckboxProps{}.defaultChecked(true));
    }});
    f.synchronize();
    const auto sw = selection.mounted().front();
    const auto sibling = selection.mounted().back();
    require(!f.services.next_frame_deadline(),
        "reduced-motion loading Switch scheduled a frame");
    f.services.set_motion_preference(animation::MotionPreference::normal);
    require(f.services.next_frame_deadline().has_value(),
        "normal-motion loading Switch did not start spinner");
    f.services.surfaces().instances().clear_dirty_ranges();
    static_cast<void>(f.services.tick_animations(
        animation::AnimationTime::microseconds(200'000)));
    f.synchronize();
    const auto range = f.services.surfaces().visual_range(sw.surface);
    const auto sibling_range = f.services.surfaces().visual_range(sibling.surface);
    require(range.count == 10 && sibling_range.count == 15,
        "spinner changed retained scene topology");
    require(!f.services.surfaces().instances().material_dirty_ranges().empty(),
        "spinner did not update material");
    for (const auto dirty : f.services.surfaces().instances().material_dirty_ranges()) {
        require(dirty.first >= range.first
            && dirty.first + dirty.count <= range.first + range.count,
            "spinner uploaded sibling material");
    }
    loading.set(false);
    f.synchronize();
    require(!f.services.next_frame_deadline(),
        "stopped Switch spinner retained an animation deadline");
    const auto point = center(f, sw.node);
    f.services.pointer().dispatch(pointer(PointerAction::down, point, PointerButton::primary));
    f.services.pointer().dispatch(pointer(PointerAction::up, point, PointerButton::primary));
    require(changes == 1, "Switch did not activate after loading stopped");
}

void mixed_window_journey() {
    Fixture f;
    detail::SelectionComponentHost selection{f.services};
    Signal<bool> switch_checked{false}, checkbox_checked{false};
    Signal<ThemeConfig> theme{ThemeConfig{}};
    int clicks{}, submits{}, switch_changes{}, checkbox_changes{}, label_runs{};
    f.buttons.mount(Content{[&] {
        Theme(ThemeProps{}.config(theme), ThemeContent{[&] {
            Button(ButtonProps{}.onClick([&] { ++clicks; }),
                ButtonContent{[] { Text(u8"按钮"); }});
            Input(InputProps{}.defaultValue(u8"Edit").onSubmit([&](String) { ++submits; }));
            Switch(SwitchProps{}.checked(switch_checked)
                .onChange([&](bool value) { ++switch_changes; switch_checked.set(value); }));
            Checkbox(CheckboxProps{}.checked(checkbox_checked)
                .onChange([&](bool value) { ++checkbox_changes; checkbox_checked.set(value); }),
                CheckboxLabel{[&] { ++label_runs; Text(u8"选择 Choice"); }});
        }});
    }});
    f.synchronize();
    require(f.buttons.mounted_buttons().size() == 1
        && f.inputs.mounted_inputs().size() == 1
        && selection.mounted().size() == 2 && label_runs == 1,
        "four controls did not share one window mount");
    const auto button = f.buttons.mounted_buttons().front();
    const auto input = f.inputs.mounted_inputs().front();
    const auto sw = selection.mounted()[0];
    const auto cb = selection.mounted()[1];
    const std::array<input::InteractionId, 4> order{
        button.interaction, input.interaction, sw.interaction, cb.interaction};
    for (const auto expected : order) {
        f.services.focus().dispatch(key(Key::tab, KeyAction::down));
        require(f.services.focus().state().focused == expected,
            "mixed window Tab order drifted");
    }
    f.services.focus().dispatch(key(Key::enter, KeyAction::down));
    require(checkbox_changes == 0, "Enter toggled Checkbox");
    f.services.focus().dispatch(key(Key::space, KeyAction::down));
    f.services.focus().dispatch(key(Key::space, KeyAction::up));
    require(checkbox_changes == 1 && checkbox_checked.get(),
        "Checkbox Space controlled echo failed");
    f.services.focus().dispatch(key(Key::tab, KeyAction::down));
    require(f.services.focus().state().focused == button.interaction,
        "mixed window Tab did not wrap");
    f.services.focus().dispatch(key(Key::enter, KeyAction::down));
    f.services.focus().dispatch(key(Key::enter, KeyAction::up));
    require(clicks == 1, "Button Enter semantics regressed");
    f.services.focus().dispatch(key(Key::tab, KeyAction::down));
    f.services.focus().dispatch(key(Key::enter, KeyAction::down));
    require(submits == 1 && f.platform.starts > 0,
        "Input session or Enter submit regressed");
    f.services.focus().dispatch(key(Key::tab, KeyAction::down));
    f.services.focus().dispatch(key(Key::space, KeyAction::down));
    f.services.focus().dispatch(key(Key::space, KeyAction::up));
    require(switch_changes == 1 && switch_checked.get(),
        "Switch Space controlled echo failed in mixed window");
    const auto mount_runs = f.services.components().mount_runs();
    const auto scene_rebuilds = f.services.scene_composer().diagnostics().rebuilds;
    const auto surface_creates = f.services.surfaces().diagnostics().creates;
    auto config = theme.get();
    config.seed.color_primary = Color::rgba8(114, 46, 209);
    theme.set(config);
    f.synchronize();
    require(f.services.components().mount_runs() == mount_runs
        && f.services.scene_composer().diagnostics().rebuilds == scene_rebuilds
        && f.services.surfaces().diagnostics().creates == surface_creates
        && label_runs == 1,
        "Theme recolor remounted, reshaped, or rebuilt a mixed sibling");
    require(f.services.destroy(sw.component) && selection.mounted().size() == 1
        && selection.mounted().front().component == cb.component
        && f.services.components().contains(input.component),
        "Switch destroy damaged mixed siblings");
    f.services.focus().clear_focus();
    f.synchronize();
    require(!f.services.next_frame_deadline(),
        "idle mixed window retained animation deadline");
}
} // namespace

int main() {
    try {
        keyboard_and_modes();
        pointer_and_lifecycle();
        invalid_and_self_destroy();
        checkbox_label_hit_area();
        generation_reuse();
        visuals_and_local_updates();
        loading_spinner_and_idle();
        mixed_window_journey();
    } catch (const std::exception& error) {
        std::cerr << error.what() << '\n';
        return 1;
    }
}
