#include "component/input_component.hpp"
#include "support/input_fixture.hpp"
#include <ryn/rynui.hpp>
#include <iostream>
#include <array>
#include <cmath>
#include <memory>
#include <map>
#include <stdexcept>

namespace {
using namespace ryn;
using namespace ryn::input;
void require(bool value, const char* message) { if(!value) throw std::runtime_error(message); }
using Fixture = ryn_test::input_component::Fixture;
void lifecycle() {
    Fixture f;
    Signal<String> value{String{u8"a"}};
    Signal<bool> disabled{false}, read_only{false};
    int changes{}, prefix_runs{}, suffix_runs{};
    f.inputs.mount(Content{[&] {
        Input(InputProps{}.value(value).disabled(disabled).readOnly(read_only)
            .onChange([&](String next) { ++changes; value.set(std::move(next)); }),
            InputPrefix{[&] { ++prefix_runs; }}, InputSuffix{[&] { ++suffix_runs; }});
    }});
    const auto mounted = f.inputs.mounted_inputs().front();
    require(f.buttons.focus().request_focus(mounted.interaction, FocusModality::keyboard), "Input cannot focus");
    const auto stamp = f.inputs.sessions().active();
    require(f.inputs.set_caret_deadline(mounted.component, animation::AnimationTime::microseconds(500000)), "deadline not attached");
    auto& editor = f.inputs.editors().require(mounted.editor);
    require(bool(editor.move(TextCaretMove::end)), "end failed");
    require(bool(f.inputs.dispatch(TextCommitted{String{u8"b"}, stamp})), "Input commit failed");
    require(changes == 1 && editor.value() == "ab" && editor.history().undo_count == 1, "controlled echo lost edit/history");
    require(prefix_runs == 1 && suffix_runs == 1, "controlled echo reran slots");
    require(bool(f.inputs.dispatch(CompositionChanged{String{u8"ni"}, {2, 0}, stamp})), "composition failed");
    read_only.set(true);
    require(!f.inputs.next_caret_deadline(), "readOnly retained caret deadline");
    require(!f.inputs.sessions().active().valid() && !editor.composition().active, "readOnly retained composition/session");
    require(f.buttons.focus().state().focused == mounted.interaction, "readOnly removed selection focus");
    read_only.set(false);
    require(f.inputs.sessions().active().valid(), "editable session not restored");
    require(f.inputs.set_caret_deadline(mounted.component, animation::AnimationTime::microseconds(500000)), "editable deadline not attached");
    f.inputs.set_window_active(false);
    require(!f.inputs.next_caret_deadline() && !f.inputs.sessions().active().valid(), "window focus loss retained deadline/session");
    f.inputs.set_window_active(true);
    require(f.inputs.sessions().active().valid() && !f.inputs.next_caret_deadline(), "window restoration revived stale deadline");
    require(f.inputs.set_caret_deadline(mounted.component, animation::AnimationTime::microseconds(500000)), "restored deadline not attached");
    disabled.set(true);
    require(!f.inputs.next_caret_deadline(), "disabled retained caret deadline");
    require(!f.buttons.focus().state().focused && !f.inputs.sessions().active().valid(), "disabled retained focus/session");
    require(f.buttons.destroy(mounted.component), "Input destroy failed");
    value.set(String{u8"late"});
    require(f.inputs.editors().size() == 0 && f.inputs.mounted_inputs().empty(), "Input resources leaked");
    require(!f.inputs.dispatch(TextCommitted{String{u8"late"}, stamp}), "stale commit delivered");
    require(f.platform.starts == f.platform.stops, "session starts/stops unbalanced");
}
void invalid_mount() {
    Fixture f;
    bool rejected{};
    try { f.inputs.mount(Content{[] { Input(InputProps{}.value(u8"a").defaultValue(u8"b")); }}); }
    catch(const std::invalid_argument&) { rejected = true; }
    require(rejected && f.inputs.editors().size() == 0 && f.buttons.interactions().size() == 0
        && f.buttons.components().root_components().empty(), "dual mode acquired identities");
}
void self_destroy(bool submit) {
    Fixture f;
    runtime::ComponentId id;
    String delivered;
    auto callback = [&](String next) { delivered = std::move(next); require(f.buttons.destroy(id), "self destroy failed"); };
    f.inputs.mount(Content{[&] { Input(InputProps{}.defaultValue(u8"start").onChange(callback).onSubmit(callback)); }});
    const auto mounted = f.inputs.mounted_inputs().front(); id = mounted.component;
    require(f.buttons.focus().request_focus(mounted.interaction, FocusModality::keyboard), "focus failed");
    if(submit) f.inputs.submit(id);
    else require(bool(f.inputs.dispatch(TextCommitted{String{u8"x"}, f.inputs.sessions().active()})), "commit failed");
    require(!delivered.empty() && f.inputs.mounted_inputs().empty() && f.inputs.editors().size() == 0,
        "callback self destroy retained Input resources");
}
void reuse_and_rollback() {
    Fixture f;
    detail::MountedInputComponent old;
    f.inputs.mount(Content{[&] {
        Input(InputProps{}.defaultValue(u8"old"));
        old = f.inputs.mounted_inputs().front();
        require(f.buttons.destroy(old.component), "mount-time destroy failed");
        Input(InputProps{}.defaultValue(u8"new"));
    }});
    const auto fresh = f.inputs.mounted_inputs().front();
    f.synchronize();
    require(old.component.index == fresh.component.index && old.component.generation != fresh.component.generation
        && old.editor.index == fresh.editor.index && old.editor.generation != fresh.editor.generation,
        "Input identities not generation safe");
    require(!f.inputs.editors().find(old.editor) && f.inputs.editors().require(fresh.editor).value() == "new", "stale editor resolved");
    Fixture failing;
    bool rejected{};
    try { failing.inputs.mount(Content{[] { Input(InputProps{}, InputPrefix{[] { throw std::runtime_error("slot failed"); }}); }}); }
    catch(const std::runtime_error&) { rejected = true; }
    require(rejected && failing.inputs.editors().size() == 0 && failing.buttons.interactions().size() == 0
        && failing.nodes.size() == 0 && failing.scene.size() == 0
        && failing.buttons.button_scene().size() == 0
        && failing.buttons.rounded_effects().live_count() == 0, "throwing slot leaked resources");
}
void readonly_blur() {
    Fixture f;
    Signal<bool> read_only{true};
    f.inputs.mount(Content{[&] { Input(InputProps{}.readOnly(read_only)); Button(ButtonProps{}, [] {}); }});
    const auto input = f.inputs.mounted_inputs().front();
    require(f.buttons.focus().request_focus(input.interaction, FocusModality::keyboard), "readOnly focus rejected");
    require(f.buttons.focus().request_focus(f.buttons.mounted_buttons().front().interaction, FocusModality::keyboard), "button focus rejected");
    read_only.set(false);
    require(!f.inputs.sessions().active().valid(), "unfocused readOnly transition restarted native session");
}
void capture_teardown() {
    Fixture f;
    f.inputs.mount(Content{[] { Input(InputProps{}); }}); f.synchronize();
    const auto mounted = f.inputs.mounted_inputs().front();
    const auto bounds = f.nodes.require(mounted.node).bounds;
    input::InteractionHandlers handlers;
    handlers.target = [](input::PointerDispatchContext& event) {
        if(event.kind() == input::PointerEventKind::down)
            require(event.capture_pointer(), "Input capture failed");
    };
    f.buttons.interactions().set_handlers(mounted.interaction, std::move(handlers));
    const std::array entries{input::HitTestPaintEntry{mounted.interaction, bounds}};
    f.buttons.hit_test().rebuild(entries, {0, 0, 320, 240});
    const auto mouse = input::PointerIdentity::mouse();
    f.buttons.pointer().dispatch({mouse, input::PointerAction::down, input::PointerButton::primary,
        bounds.x + 1, bounds.y + 1});
    require(f.buttons.pointer().state(mouse)->capture == mounted.interaction, "capture not retained");
    require(f.inputs.set_caret_deadline(mounted.component, animation::AnimationTime::microseconds(500000)), "captured Input deadline not attached");
    require(f.buttons.destroy(mounted.component), "captured Input destroy failed");
    const auto pointer = f.buttons.pointer().state(mouse);
    require(pointer && !pointer->capture && !pointer->press_origin
        && !f.buttons.focus().state().focused && !f.inputs.sessions().active().valid()
        && !f.inputs.next_caret_deadline(), "capture/focus/session/deadline survived destroy");
    require(!f.inputs.set_caret_deadline(mounted.component, animation::AnimationTime::microseconds(600000)), "stale component acquired deadline");
}
void layout_matrix() {
    std::size_t cases{};
    for(auto size : {ControlSize::Small, ControlSize::Middle, ControlSize::Large}) {
        for(int slots = 0; slots < 4; ++slots) {
            Fixture f;
            Signal<String> value{String{}};
            Signal<String> placeholder{String{u8"请输入 Input"}};
            Signal<LogicalLength> width{dp(180)};
            f.inputs.mount(Content{[&] {
                Input(InputProps{}.value(value).placeholder(placeholder).size(size)
                    .layout(LayoutStyle{}.width(width)),
                    slots & 1 ? std::optional<InputPrefix>{InputPrefix{[] { Text(u8"前"); }}} : std::nullopt,
                    slots & 2 ? std::optional<InputSuffix>{InputSuffix{[] { Text(u8"后"); }}} : std::nullopt);
            }});
            const auto mounted = f.inputs.mounted_inputs().front();
            for(const auto text : {String{}, String{u8"Latin"}, String{u8"中文混合 abc"},
                String{u8"a very long line that must stay on one line and scroll horizontally past both affixes"}}) {
                value.set(text);
                for(float logical_width : {0.0F, 7.0F, 40.0F, 180.0F}) {
                    width.set(dp(logical_width)); f.synchronize();
                    const auto root = f.nodes.require(mounted.node).bounds;
                    const auto geometry = f.inputs.layout_snapshot(mounted.component);
                    const auto& measurement = f.scene.text_state(f.inputs.text_scene(mounted.component)).measurement();
                    require(std::abs(root.width - logical_width) < 0.001F, "Input width constraint ignored");
                    const float expected_height = size == ControlSize::Small ? 24.0F : size == ControlSize::Large ? 40.0F : 32.0F;
                    require(std::abs(root.height - expected_height) < 0.001F, "Input control height mismatch");
                    require(geometry.viewport.x >= root.x && geometry.viewport.x + geometry.viewport.width <= root.x + root.width + 0.001F
                        && geometry.viewport.width >= 0 && geometry.clip.width >= 0, "narrow Input viewport escaped frame");
                    require(measurement.lines.size() <= 1 && std::isfinite(geometry.baseline), "Input wrapped text or lost baseline");
                    f.inputs.set_horizontal_scroll(mounted.component, 10000);
                    require(f.inputs.layout_snapshot(mounted.component).scroll_offset
                        <= std::max(0.0F, geometry.text_width - geometry.viewport.width), "Input scroll escaped range");
                    for(float scale : {1.0F, 1.25F, 1.5F, 2.0F}) {
                        require(geometry.viewport.width * scale <= root.width * scale + 0.001F,
                            "simulated scale viewport escaped frame"); ++cases;
                    }
                }
            }
            value.set(String{}); placeholder.set(String{}); width.set(dp(180)); f.synchronize();
            require(f.inputs.layout_snapshot(mounted.component).scroll_offset == 0, "empty value did not clamp scroll");
            require(f.inputs.layout_snapshot(mounted.component).viewport.height > 0, "empty editor lost line height");
            f.synchronize(320, {50, 5, 20, 8});
            const auto clipped = f.inputs.layout_snapshot(mounted.component).clip;
            require(clipped.x >= 50 && clipped.y >= 5 && clipped.width <= 20 && clipped.height <= 8,
                "Input viewport ignored outer clip");
        }
    }
    Fixture limits;
    limits.inputs.mount(Content{[] { Input(InputProps{}.defaultValue(u8"text")
        .layout(LayoutStyle{}.min_width(dp(80)).max_width(dp(120)))); }});
    limits.synchronize(320);
    const auto root = limits.inputs.mounted_inputs().front().node;
    require(limits.nodes.require(root).bounds.width == 120, "Input max width ignored");
    static_cast<void>(limits.layout.layout(root, {0, 60, 0, 100}));
    require(limits.nodes.require(root).bounds.width == 60, "parent constraint did not dominate min width");
    std::cout << "Input layout simulated-scale cases=" << cases << '\n';
}
void reactive_phases() {
    Fixture f;
    Signal<String> value{String{u8"value"}}, placeholder{String{u8"hint"}};
    Signal<String> prefix{String{u8"P"}};
    Signal<InputStatus> status{InputStatus::Default};
    Signal<bool> disabled{false}, read_only{false};
    Signal<ControlSize> size{ControlSize::Middle};
    Signal<ThemeConfig> config{ThemeConfig{}};
    int parent_runs{}, slot_runs{}, sibling_runs{};
    f.inputs.mount(Content{[&] {
        ++parent_runs;
        Theme(ThemeProps{}.config(config), ThemeContent{[&] {
            Input(InputProps{}.value(value).placeholder(placeholder).status(status)
                .disabled(disabled).readOnly(read_only).size(size), InputPrefix{[&] { ++slot_runs; Text(TextProps{}.content(prefix)); }});
        }});
        ++sibling_runs; Text(u8"unrelated sibling");
    }});
    f.synchronize();
    const auto component = f.inputs.mounted_inputs().front().component;
    const auto scene = f.inputs.text_scene(component);
    const auto sibling = f.buttons.text().mounted_texts().back().scene;
    const auto shapes = f.scene.text_state(scene).counters().shape_count;
    const auto sibling_shapes = f.scene.text_state(sibling).counters().shape_count;
    batch([&] { status.set(InputStatus::Error); disabled.set(true); read_only.set(true); });
    require(f.dirty.layout_roots().empty() && f.dirty.text_nodes().empty(), "interaction/material props triggered text/layout");
    f.synchronize();
    require(f.scene.text_state(scene).counters().shape_count == shapes, "status reshaped Input");
    placeholder.set(String{u8"hidden hint"});
    require(f.dirty.layout_roots().empty(), "hidden placeholder invalidated layout");
    prefix.set(String{u8"prefix grew"}); f.synchronize();
    require(f.scene.text_state(scene).counters().shape_count == shapes, "slot content update reshaped editable value");
    batch([&] { value.set(String{u8"intermediate"}); value.set(String{u8"final"}); }); f.synchronize();
    require(f.scene.text_state(scene).counters().shape_count == shapes + 1, "batch value did not coalesce shaping");
    ThemeConfig next; next.seed.control_height = dp(44); next.text.tokens.font_size = dp(18);
    next.text.tokens.line_height = dp(26); config.set(next); f.synchronize();
    require(f.nodes.require(f.inputs.mounted_inputs().front().node).bounds.height == 44, "nested Theme control height ignored");
    size.set(ControlSize::Large); f.synchronize();
    require(parent_runs == 1 && slot_runs == 1 && sibling_runs == 1
        && f.scene.text_state(sibling).counters().shape_count == sibling_shapes, "Input Prop/Theme update reran unrelated content");
    require(f.buttons.destroy(component), "reactive Input unmount failed");
    config.set(ThemeConfig{}); value.set(String{u8"late"});
    require(f.inputs.editors().size() == 0, "unmounted bindings recreated editor");
}
void retained_scene_layers() {
    Fixture f;
    Signal<String> value{String{u8"selectable text"}};
    Signal<ThemeConfig> config{ThemeConfig{}};
    f.inputs.mount(Content{[&] { Theme(ThemeProps{}.config(config), ThemeContent{[&] {
        Input(InputProps{}.value(value).placeholder(u8"placeholder").layout(LayoutStyle{}.width(dp(120))));
    }}); }});
    f.synchronize();
    const auto mounted = f.inputs.mounted_inputs().front();
    const auto layers = f.inputs.text_layers(mounted.component);
    require(f.scene.size() == 3 && f.buttons.button_scene().instances().size() == 3
        && f.buttons.rounded_effects().live_count() == 4, "Input retained topology is incomplete");
    require(f.buttons.hit_test().hit_test({20, 15}) == mounted.interaction,
        "Input container was not registered for hit testing");
    const auto& read = std::as_const(f.scene);
    require(&read.text_state(layers.base) == &read.text_state(layers.selected)
        && &read.text_state(layers.base) == &read.text_state(layers.placeholder), "Input duplicated shaping state");
    const auto commands = f.buttons.scene_composer().ordered_scene().commands();
    require(commands.size() >= 4 && commands.front().kind == graphics::SceneDrawKind::rounded_effect
        && commands[1].kind == graphics::SceneDrawKind::quad
        && commands[2].kind == graphics::SceneDrawKind::glyph
        && commands.back().kind == graphics::SceneDrawKind::quad
        && commands.back().instance_count == 2, "Input layer order does not enclose glyphs with selection/caret");
    require(f.buttons.focus().request_focus(mounted.interaction, FocusModality::keyboard), "retained Input focus failed");
    auto& editor = f.inputs.editors().require(mounted.editor);
    require(bool(editor.select({0, 3})), "selection failed"); f.synchronize();
    const auto rebuilds = f.buttons.scene_composer().diagnostics().rebuilds;
    const auto text_rebuilds = f.scene.counters().ordered_scene_rebuilds;
    const auto shape_count = read.text_state(layers.base).counters().shape_count;
    for(std::size_t index = 0; index < 100; ++index) {
        require(bool(editor.select({0, index % 4})), "selection update failed"); f.synchronize();
    }
    require(f.inputs.text_layers(mounted.component).selected == layers.selected
        && f.scene.size() == 3 && f.buttons.button_scene().instances().size() == 3
        && f.buttons.rounded_effects().live_count() == 4
        && f.buttons.scene_composer().diagnostics().rebuilds == rebuilds
        && f.scene.counters().ordered_scene_rebuilds == text_rebuilds
        && read.text_state(layers.base).counters().shape_count == shape_count,
        "selection changed retained topology or shaping");
    ThemeConfig recolor;
    recolor.alias.color_border = Color::rgba8(255, 0, 0);
    recolor.alias.color_background_container = Color::rgba8(0, 255, 0);
    config.set(recolor); f.synchronize();
    const auto effects = f.buttons.rounded_effects().packed_instances();
    require(effects[1].material.color == *recolor.alias.color_border
        && effects[2].material.color == *recolor.alias.color_background_container
        && effects[0].material.opacity == 0 && effects[3].material.opacity == 0
        && f.buttons.scene_composer().diagnostics().rebuilds == rebuilds
        && read.text_state(layers.base).counters().shape_count == shape_count,
        "Theme recolor rebuilt topology or lost independent effect materials");
    f.synchronize(320, {20, 0, 40, 32});
    for(const auto& effect : f.buttons.rounded_effects().packed_instances()) {
        require(effect.geometry.shape.rect.x < 20 && effect.geometry.ancestor_clip
            && effect.geometry.ancestor_clip->bounds == runtime::Rect{20, 0, 40, 32},
            "container clipping changed original rounded geometry");
    }
    require(!f.buttons.hit_test().hit_test({10, 15}) && f.buttons.hit_test().hit_test({25, 15}) == mounted.interaction,
        "Input hit clip does not match supplied ancestor clip");
    value.set(String{}); f.synchronize();
    const auto opacity = [&](detail::TextSceneId id) {
        return f.scene.glyph_scene().instances().at(f.scene.primitive(id).instances.first).translation_opacity[2];
    };
    require(opacity(layers.base) == 0 && opacity(layers.selected) == 0 && opacity(layers.placeholder) > 0,
        "placeholder visibility reused or leaked another layer");
    require(f.buttons.destroy(mounted.component), "retained Input destruction failed");
    require(f.scene.size() == 0 && f.buttons.button_scene().instances().size() == 0
        && f.buttons.rounded_effects().live_count() == 0, "retained Input resources leaked");
}

void retained_range_remapping() {
    Fixture f;
    Signal<String> first{String{u8"A"}};
    f.inputs.mount(Content{[&] {
        Input(InputProps{}.value(first));
        Input(InputProps{}.defaultValue(u8"second"), InputPrefix{[] { Text(u8"prefix"); }});
        Text(u8"following text");
    }}); f.synchronize();
    const auto a = f.inputs.mounted_inputs()[0];
    const auto b = f.inputs.mounted_inputs()[1];
    const auto layers = f.inputs.text_layers(b.component);
    const auto before = f.scene.primitive(layers.base).instances.first;
    const auto validate = [&] {
        std::size_t glyphs{};
        for(const auto command : f.buttons.scene_composer().ordered_scene().commands()) {
            if(command.kind == graphics::SceneDrawKind::glyph) {
                require(command.first_instance + command.instance_count <= f.scene.glyph_scene().instances().size(),
                    "compacted fragment has stale glyph range");
                glyphs += command.instance_count;
            } else if(command.kind == graphics::SceneDrawKind::quad) {
                require(command.first_instance + command.instance_count <= f.buttons.button_scene().instances().size(),
                    "compacted fragment has stale quad range");
            } else require(command.first_instance + command.instance_count <= f.buttons.rounded_effects().packed_instances().size(),
                "compacted fragment has stale effect range");
        }
        require(glyphs == f.scene.glyph_scene().instances().size(), "compaction lost or duplicated glyph layers");
    };
    first.set(String{u8"A much longer first input 中文"}); f.synchronize(); validate();
    require(f.scene.primitive(layers.base).instances.first > before, "following Input range did not remap");
    require(f.buttons.destroy(a.component), "first Input removal failed"); f.synchronize(); validate();
    require(f.inputs.text_layers(b.component).base == layers.base
        && f.buttons.hit_test().hit_test({20, 15}) == b.interaction,
        "remaining Input identity or relocated hit bounds became stale");
}

void input_pixel_grid() {
    for(const float scale : {1.0F, 1.25F, 1.5F, 2.0F}) {
        Fixture f; f.font_scale = scale; f.inputs.set_display_scale(scale);
        f.inputs.mount(Content{[] { Input(InputProps{}.layout(LayoutStyle{}.width(dp(80.3F)))); }});
        f.synchronize();
        const auto mounted = f.inputs.mounted_inputs().front();
        require(f.buttons.focus().request_focus(mounted.interaction, FocusModality::keyboard), "scaled Input focus failed");
        require(bool(f.inputs.dispatch(CompositionChanged{String{u8"中文输入测试很长"}, {8, 0}, f.inputs.sessions().active()})),
            "scaled composition failed"); f.synchronize();
        const auto geometry = f.inputs.layout_snapshot(mounted.component);
        const auto on_grid = [scale](float logical) { return std::abs(logical * scale - std::round(logical * scale)) < 0.0001F; };
        require(on_grid(geometry.scroll_offset) && on_grid(geometry.caret.x) && on_grid(geometry.caret.y)
            && on_grid(geometry.caret.width) && on_grid(geometry.caret.height)
            && on_grid(geometry.underline.x) && on_grid(geometry.underline.y)
            && on_grid(geometry.underline.width) && on_grid(geometry.underline.height), "Input geometry is off physical pixel grid");
        const auto expected = std::max(1.0F, std::round(scale));
        require(std::abs(geometry.caret.width * scale - expected) < 0.0001F
            && std::abs(geometry.underline.height * scale - expected) < 0.0001F
            && geometry.caret.x >= geometry.clip.x
            && geometry.caret.x + geometry.caret.width <= geometry.clip.x + geometry.clip.width + 0.0001F,
            "scaled End caret/underline lost thickness or was clipped");
        const auto scene = f.inputs.text_scene(mounted.component);
        const auto& instance = f.scene.glyph_scene().instances().at(f.scene.primitive(scene).instances.first);
        require(on_grid(instance.translation_opacity[0] * 320 / 2), "glyph scroll changed physical raster phase");
        require(bool(f.inputs.dispatch(TextCommitted{String{u8"中文输入测试很长"}, f.inputs.sessions().active()})), "scaled commit failed");
        require(bool(f.inputs.editors().require(mounted.editor).move(TextCaretMove::home)), "scaled Home failed"); f.synchronize();
        require(f.inputs.layout_snapshot(mounted.component).scroll_offset == 0, "scaled Home retained scroll");
        const auto& first = f.scene.glyph_scene().instances().at(f.scene.primitive(scene).instances.first);
        const float guard = graphics::glyph_atlas_padding / scale;
        const float ink_left = (first.position_size[0] + 1) * 160 + guard;
        const float ink_top = (1 - first.position_size[1]) * 120 + guard;
        const float ink_bottom = ink_top - first.position_size[3] * 120 - 2 * guard;
        const auto visible_clip = f.inputs.layout_snapshot(mounted.component).clip;
        require(ink_left >= visible_clip.x - 0.001F && ink_top >= visible_clip.y - 0.001F
            && ink_bottom <= visible_clip.y + visible_clip.height + 0.001F, "scaled visible CJK glyph was clipped");
    }
}

void composition_display() {
    Fixture f; Signal<String> value{String{}}; int changes{};
    f.inputs.mount(Content{[&] {
        Input(InputProps{}.value(value).placeholder(u8"hint").layout(LayoutStyle{}.width(dp(80)))
            .onChange([&](String next) { ++changes; value.set(std::move(next)); }));
    }}); f.synchronize();
    const auto mounted = f.inputs.mounted_inputs().front();
    const auto scene = f.inputs.text_scene(mounted.component);
    require(f.inputs.display_snapshot(mounted.component).placeholder, "empty Input did not show placeholder");
    require(f.buttons.focus().request_focus(mounted.interaction, FocusModality::keyboard), "composition Input focus failed");
    const auto stamp = f.inputs.sessions().active();
    const auto measures = f.nodes.require(mounted.node).measure_count;
    require(bool(f.inputs.dispatch(CompositionChanged{String{u8"中文输入很长"}, {6, 0}, stamp})), "Input preedit rejected");
    require(f.dirty.layout_roots().empty(), "preedit changed external layout"); f.synchronize();
    const auto display = f.inputs.display_snapshot(mounted.component);
    auto geometry = f.inputs.layout_snapshot(mounted.component);
    require(!display.placeholder && display.composing && value.get().empty() && changes == 0, "preedit changed authoritative value");
    require(geometry.scroll_offset > 0 && geometry.caret_x + 1 <= geometry.viewport.x + geometry.viewport.width + 0.001F,
        "preedit End caret was clipped");
    const auto shapes = f.scene.text_state(scene).counters().shape_count;
    const auto map_revision = f.inputs.caret_map(mounted.component).revision();
    require(bool(f.inputs.dispatch(CompositionChanged{String{u8"中文输入很长"}, {0, 1}, stamp})), "composition range update failed");
    require(f.dirty.layout_roots().empty() && f.dirty.text_nodes().empty(), "composition range invalidated layout/shape"); f.synchronize();
    require(f.scene.text_state(scene).counters().shape_count == shapes
        && f.inputs.caret_map(mounted.component).revision() == map_revision
        && f.nodes.require(mounted.node).measure_count == measures, "composition range reshaped/remeasured Input");
    require(bool(f.inputs.dispatch(TextCommitted{String{u8"中文输入很长"}, stamp})), "composition commit failed"); f.synchronize();
    require(changes == 1 && value.get() == String{u8"中文输入很长"}
        && !f.inputs.display_snapshot(mounted.component).composing
        && f.scene.text_state(scene).counters().shape_count == shapes, "commit/echo needlessly reshaped identical displayed text");
    auto& editor = f.inputs.editors().require(mounted.editor);
    require(bool(editor.move(TextCaretMove::home)), "Input Home failed"); f.synchronize();
    require(f.inputs.layout_snapshot(mounted.component).scroll_offset == 0, "Input Home did not reveal start");
    require(bool(editor.move(TextCaretMove::end)), "Input End failed"); f.synchronize();
    require(f.inputs.layout_snapshot(mounted.component).scroll_offset > 0, "Input End did not reveal end");
    value.set(String{u8"短"}); f.synchronize();
    require(f.inputs.layout_snapshot(mounted.component).scroll_offset == 0, "controlled shorter value kept offset");
    require(bool(f.inputs.dispatch(CompositionChanged{String{u8"new"}, {3, 0}, stamp})), "second composition failed"); f.synchronize();
    require(bool(f.inputs.dispatch(TextCommitted{String{}, stamp})), "empty commit cancellation failed"); f.synchronize();
    require(!f.inputs.display_snapshot(mounted.component).composing && value.get() == String{u8"短"}, "empty commit deleted value or left preedit");
}
}
int main() {
    try { lifecycle(); invalid_mount(); self_destroy(false); self_destroy(true); reuse_and_rollback(); readonly_blur(); capture_teardown();
        layout_matrix(); reactive_phases(); retained_scene_layers(); retained_range_remapping(); input_pixel_grid(); composition_display(); std::cout << "Input lifecycle, layout and controlled callbacks passed\n"; }
    catch(const std::exception& error) { std::cerr << error.what() << '\n'; return 1; }
}
