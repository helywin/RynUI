#include "support/input_fixture.hpp"
#include "component/color_picker_component.hpp"
#include <cmath>
#include <iostream>
#include <limits>

namespace {
using namespace ryn;
using namespace ryn::input;
using Fixture = ryn_test::input_component::Fixture;

void check(bool v, const char* message) {
    if (!v) {
        throw std::runtime_error(message);
    }
}

void sync(Fixture& f) {
    check(f.buttons.layout_and_synchronize({800, 900}, {0, 0, 800, 900}), "picker layout failed");
}

void pointer(Fixture& f, const detail::ColorPickerSnapshot& s, std::size_t i, PointerAction action, double u,
             double v = .5) {
    const auto& node = f.nodes.require(f.services.components().root(s.fields[i]));
    f.services.pointer().dispatch({PointerIdentity::mouse(), action,
                                   action == PointerAction::move ? PointerButton::none : PointerButton::primary,
                                   node.bounds.x + node.bounds.width * static_cast<float>(u) + node.translation.x,
                                   node.bounds.y + node.bounds.height * static_cast<float>(v) + node.translation.y, 1});
}

void values() {
    const PickerColor precise{ColorRgb{.12345678901234567, 127.98765432101234, 254.12345678901234, .12345678901234567}};
    const auto round_trip = PickerColor::parse(precise.format(ColorFormat::Rgb).bytes());
    check(round_trip && round_trip->rgb() == precise.rgb(), "RGB text silently truncated exact channel values");
    for (const auto source :
         {"#1677ff", "#f008", "rgb(22, 119, 255)", "rgba(0, 128, 255, 0.125)", "hsba(215, 80%, 90%, 0.25)"}) {
        const auto color = PickerColor::parse(source);
        check(bool(color), "valid color rejected");
        for (const auto format : {ColorFormat::Rgb, ColorFormat::Hsb}) {
            const auto back = PickerColor::parse(color->format(format).bytes());
            check(bool(back), "formatted color did not parse");
            const auto a = color->rgb();
            const auto b = back->rgb();
            check(std::abs(a.r - b.r) < 1e-8 && std::abs(a.g - b.g) < 1e-8 && std::abs(a.b - b.b) < 1e-8 &&
                      std::abs(a.a - b.a) < 1e-8,
                  "color conversion lost precision");
        }
    }
    for (const auto s : {"", "#", "#12", "#ggg", "rgb(1,2)", "rgb(1,2,3,4)", "rgba(1,2,3,nan)", "hsb(361, 1%, 1%)",
                         "rgb(-1,0,0)", "#123456 "}) {
        check(!PickerColor::parse(s), "malformed color accepted");
    }
    const auto black = PickerColor{ColorHsb{240, 0, 0, 1}};
    check(black.hsb().h == 240, "achromatic hue lost");
    const auto transparent = PickerColor{ColorRgb{0, 0, 0, 0}};
    const ColorPickerValue gradient{std::vector<ColorStop>{
        {String{u8"b"}, PickerColor{ColorRgb{255, 255, 255, 1}}, 100}, {String{u8"a"}, transparent, 0}}};
    check(gradient.stops().front().key == String{u8"a"} && gradient.sample(50).rgb().a == .5,
          "gradient sort/interpolation failed");
    bool rejected{};
    try {
        ColorPickerValue bad{
            std::vector<ColorStop>{{String{u8"a"}, transparent, 0}, {String{u8"a"}, transparent, 100}}};
    } catch (const std::invalid_argument&) {
        rejected = true;
    }
    check(rejected, "duplicate stops accepted");
}

void activate_label(Fixture& f, String label) {
    for (const auto& text : f.services.text().mounted_texts()) {
        if (f.scene.text_state(text.scene).content().bytes() != label.bytes()) {
            continue;
        }
        for (auto parent = std::optional{text.component}; parent; parent = f.services.components().parent(*parent)) {
            for (auto interaction : f.services.interactions().declaration_order()) {
                const auto& record = f.services.interactions().require(interaction);
                if (record.component == *parent && record.focusable) {
                    check(f.services.focus().state().focused == interaction ||
                              f.services.focus().request_focus(interaction, FocusModality::keyboard),
                          "mode button focus failed");
                    f.services.focus().dispatch({Key::space, KeyAction::down});
                    f.services.focus().dispatch({Key::space, KeyAction::up});
                    return;
                }
            }
        }
    }
    throw std::runtime_error("mode button missing");
}

void mode_cache() {
    Fixture f;
    const ColorPickerValue initial{std::vector<ColorStop>{{String{u8"a"}, PickerColor{ColorRgb{255, 0, 0, .5}}, 0},
                                                          {String{u8"middle"}, PickerColor{ColorRgb{0, 255, 0, 1}}, 40},
                                                          {String{u8"b"}, PickerColor{ColorRgb{0, 0, 255, 1}}, 100}}};
    f.inputs.mount(Content{[&] {
        ColorPicker(ColorPickerProps{}
                        .defaultValue(initial)
                        .modes(std::vector<ColorPickerMode>{ColorPickerMode::Single, ColorPickerMode::Gradient})
                        .defaultOpen(true));
    }});
    sync(f);
    auto& h = f.services.color_picker();
    const auto id = h.mounted().front();
    activate_label(f, String{u8"Gradient"});
    check(h.snapshot(id).value.solid() == initial.stops().front().color, "gradient mode did not choose first stop");
    activate_label(f, String{u8"Single"});
    check(h.snapshot(id).value == initial, "mode round-trip discarded cached gradient stops");
}

void controlled_format_and_css() {
    Fixture f;
    Signal<ColorFormat> format{ColorFormat::Hex};
    ColorFormat requested = ColorFormat::Hex;
    String css;
    f.inputs.mount(Content{[&] {
        ColorPicker(ColorPickerProps{}
                        .defaultValue(PickerColor{ColorRgb{255, 0, 0, .5}})
                        .format(format)
                        .showText(true)
                        .defaultOpen(true)
                        .onFormatChange([&](ColorFormat next) { requested = next; })
                        .onChange([&](ColorPickerValue, String text) { css = text; }));
    }});
    sync(f);
    auto& h = f.services.color_picker();
    const auto id = h.mounted().front();
    activate_label(f, String{u8"HEX / RGB / HSB"});
    check(requested == ColorFormat::Rgb && h.snapshot(id).format == ColorFormat::Hex,
          "controlled format overwrote authority");
    format.set(ColorFormat::Hsb);
    sync(f);
    check(h.snapshot(id).format == ColorFormat::Hsb, "reactive format was ignored");
    const auto s = h.snapshot(id);
    pointer(f, s, 0, PointerAction::down, .4, .4);
    pointer(f, s, 0, PointerAction::up, .4, .4);
    check(css.bytes().starts_with("rgba(") && PickerColor::parse(css.bytes()).has_value(),
          "HSB display emitted invalid CSS or discarded alpha");
}

void retained_text_renderer() {
    Fixture f;
    Signal<ColorPickerValue> value{ColorPickerValue{PickerColor{ColorRgb{255, 0, 0, 1}}}};
    int renders{};
    f.inputs.mount(Content{[&] {
        ColorPickerSlots slots;
        slots.text_render = [&](ColorPickerTextContext context) {
            ++renders;
            Text(TextProps{}.content(context.text));
        };
        ColorPicker(ColorPickerProps{}.value(value), std::move(slots));
    }});
    sync(f);
    value.set(ColorPickerValue{PickerColor{ColorRgb{0, 255, 0, 1}}});
    sync(f);
    check(renders == 1, "text renderer rebuilt on ordinary value update");
    bool found{};
    for (const auto& text : f.services.text().mounted_texts()) {
        found = found || f.scene.text_state(text.scene).content().bytes() == "#00ff00";
    }
    check(found, "typed text renderer did not receive reactive formatted text");
}

void compact_default_hsb_label() {
    Fixture f;
    const ColorPickerValue value{PickerColor{ColorRgb{22, 119, 255, 1}}};
    f.inputs.mount(
        Content{[&] { ColorPicker(ColorPickerProps{}.value(value).format(ColorFormat::Hsb).showText(true)); }});
    sync(f);
    bool found{};
    for (const auto& text : f.services.text().mounted_texts()) {
        const auto label = f.scene.text_state(text.scene).content().bytes();
        if (label.starts_with("hsb(")) {
            found = true;
            check(label.size() < 32 && label.find("38.823529") == std::string_view::npos,
                  "default HSB trigger exposes an unbounded decimal expansion");
        }
    }
    check(found && f.services.color_picker().snapshot(f.services.color_picker().mounted().front()).value == value,
          "compact HSB presentation changed the exact color model");
}

void panel_actions_and_presets() {
    Fixture f;
    Signal<std::vector<ColorPreset>> presets{std::vector<ColorPreset>{
        {String{u8"palette"}, String{u8"Palette"}, {ColorPickerValue{PickerColor{ColorRgb{0, 255, 0, 1}}}}, true}}};
    int clears{};
    f.inputs.mount(Content{[&] {
        ColorPicker(ColorPickerProps{}
                        .defaultValue(PickerColor{})
                        .modes(std::vector<ColorPickerMode>{ColorPickerMode::Single, ColorPickerMode::Gradient})
                        .presets(presets)
                        .allowClear(true)
                        .defaultOpen(true)
                        .onClear([&] { ++clears; }));
    }});
    sync(f);
    auto& h = f.services.color_picker();
    const auto id = h.mounted().front();
    const auto fields = h.snapshot(id).fields;
    activate_label(f, String{u8"#00ff00"});
    check(h.snapshot(id).value.solid()->rgb().g == 255, "preset did not select color");
    presets.set(std::vector<ColorPreset>{
        {String{u8"palette"}, String{u8"Palette"}, {ColorPickerValue{PickerColor{ColorRgb{0, 0, 255, 1}}}}, true}});
    sync(f);
    activate_label(f, String{u8"#0000ff"});
    check(h.snapshot(id).value.solid()->rgb().b == 255 && h.snapshot(id).fields == fields,
          "dynamic presets failed or remounted picker fields");
    activate_label(f, String{u8"Single"});
    sync(f);
    check(h.snapshot(id).value.gradient(), "mode button did not create gradient");
    activate_label(f, String{u8"Add stop"});
    sync(f);
    check(h.snapshot(id).value.stops().size() == 3, "Add stop did not change real gradient");
    activate_label(f, String{u8"Remove"});
    sync(f);
    check(h.snapshot(id).value.stops().size() == 2, "Remove stop did not change real gradient");
    activate_label(f, String{u8"Clear"});
    check(h.snapshot(id).value.empty() && clears == 1, "clear did not publish empty value/callback");
}

void theme_contract() {
    using namespace ryn::theme_runtime;
    check(token_identity_name(TokenIdentity::color_picker_colors) == "ColorPicker.colors" &&
              token_identity_name(TokenIdentity::color_picker_metrics) == "ColorPicker.metrics",
          "ColorPicker token identity names drifted");
    const auto base = resolve_theme();
    ThemeConfig config;
    config.color_picker.tokens.panel_width = dp(300);
    config.color_picker.tokens.focus = Color::rgba8(10, 20, 30);
    const auto parent = resolve_theme(config);
    check(parent.color_picker().panel_width == 300 &&
              resolve_theme({}, &parent).color_picker() == parent.color_picker(),
          "ColorPicker token inheritance failed");
    ThemeConfig reset;
    reset.inherit = false;
    check(resolve_theme(reset, &parent).color_picker() == base.color_picker(), "ColorPicker inherit=false failed");
    check(parent.input_number() == base.input_number() && parent.popover() == base.popover(),
          "ColorPicker token override leaked");
    ThemeConfig dark;
    dark.algorithms = {ThemeAlgorithm::Dark};
    ThemeConfig compact;
    compact.algorithms = {ThemeAlgorithm::Compact};
    check(resolve_theme(dark).color_picker().colors() != base.color_picker().colors() &&
              resolve_theme(compact).color_picker().saturation_height < base.color_picker().saturation_height,
          "ColorPicker Dark/Compact tokens absent");
    ThemeConfig independent;
    independent.color_picker.algorithm = true;
    independent.color_picker.seed.color_primary = Color::rgba8(255, 0, 0);
    check(resolve_theme(independent).color_picker().focus == Color::rgba8(255, 0, 0) &&
              resolve_theme(independent).button() == base.button(),
          "ColorPicker component algorithm leaked");
    auto scope = ThemeScope::create_default();
    int colors{};
    int metrics{};
    auto c = scope->capture(
        [&](DirtyPhase p) {
            check(p == DirtyPhase::paint_material, "picker color remeasures");
            ++colors;
        },
        [&] { static_cast<void>(scope->color_picker_colors()); });
    auto m = scope->capture(
        [&](DirtyPhase p) {
            check(has_any(p, DirtyPhase::measure_layout), "picker metric missed layout");
            ++metrics;
        },
        [&] { static_cast<void>(scope->color_picker_metrics()); });
    ThemeConfig update;
    update.color_picker.tokens.focus = Color::rgba8(4, 5, 6);
    scope->update(update);
    check(colors == 1 && metrics == 0, "picker color dirtied metrics");
    update.color_picker.tokens.panel_width = dp(280);
    scope->update(update);
    check(colors == 1 && metrics == 1, "picker metrics dirtied colors");
    const auto before = scope->snapshot();
    update.color_picker.tokens.rail_height = dp(std::numeric_limits<float>::infinity());
    bool rejected{};
    try {
        scope->update(update);
    } catch (const std::invalid_argument&) {
        rejected = true;
    }
    check(rejected && scope->snapshot() == before, "picker invalid token published partially");
}

std::vector<std::array<float, 4>> field_colors(Fixture& f, const detail::ColorPickerSnapshot& s, std::size_t i) {
    const auto bounds = f.nodes.require(f.services.components().root(s.fields[i])).bounds;
    std::vector<std::array<float, 4>> result;
    for (const auto& effect : f.services.surfaces().effects().packed_instances()) {
        const auto& shape = effect.geometry.shape.rect;
        if (effect.material.visible && effect.material.opacity > 0 && shape == bounds &&
            effect.geometry.ancestor_clip) {
            const auto clip = graphics::intersect_effect_bounds(bounds, effect.geometry.ancestor_clip->bounds);
            if (clip.width > 0 && clip.width < bounds.width / 10 && clip.height > 0 &&
                clip.height < bounds.height / 2) {
                const auto color = effect.material.color;
                result.push_back({color.red(), color.green(), color.blue(), color.alpha()});
            }
        }
    }
    return result;
}

void interactive() {
    Fixture f;
    Signal<bool> open{true};
    Signal<bool> disabled{false};
    Signal<bool> alpha_disabled{false};
    int changes{};
    int complete{};
    f.inputs.mount(Content{[&] {
        ColorPicker(ColorPickerProps{}
                        .defaultValue(PickerColor{ColorHsb{0, 1, 1, 1}})
                        .open(open)
                        .disabled(disabled)
                        .disabledAlpha(alpha_disabled)
                        .modes(std::vector<ColorPickerMode>{ColorPickerMode::Single, ColorPickerMode::Gradient})
                        .allowClear(true)
                        .onChange([&](auto, String) { ++changes; })
                        .onChangeComplete([&](auto) { ++complete; }));
    }});
    sync(f);
    auto& host = f.services.color_picker();
    const auto id = host.mounted().front();
    auto s = host.snapshot(id);
    check(s.fields.size() == 4 && f.inputs.mounted_inputs().size() == 6, "picker did not mount real fields/editors");
    pointer(f, s, 0, PointerAction::down, .5, .25);
    pointer(f, s, 0, PointerAction::move, .25, .5);
    pointer(f, s, 0, PointerAction::up, .25, .5);
    s = host.snapshot(id);
    check(changes >= 2 && complete == 1 && std::abs(s.value.solid()->hsb().s - .25) < .02, "SV did not update color");
    const auto old_sv = field_colors(f, s, 0);
    pointer(f, s, 1, PointerAction::down, 2.0 / 3);
    pointer(f, s, 1, PointerAction::up, 2.0 / 3);
    s = host.snapshot(id);
    check(std::abs(s.value.solid()->hsb().h - 240) < 2, "hue control did not update color");
    sync(f);
    check(!old_sv.empty() && field_colors(f, s, 0) != old_sv, "hue did not update actual logical SV scene");
    pointer(f, s, 2, PointerAction::down, .25);
    pointer(f, s, 2, PointerAction::up, .25);
    s = host.snapshot(id);
    check(std::abs(s.value.solid()->rgb().a - .25) < .02, "alpha control did not update color");
    alpha_disabled.set(true);
    sync(f);
    const auto count = changes;
    pointer(f, s, 2, PointerAction::down, .75);
    pointer(f, s, 2, PointerAction::up, .75);
    check(changes == count, "disabled alpha consumed input");
    pointer(f, s, 1, PointerAction::down, .5);
    pointer(f, s, 1, PointerAction::up, .5);
    check(host.snapshot(id).value.solid()->rgb().a == 1, "disabledAlpha did not normalize changed color to opaque");
    alpha_disabled.set(false);
    sync(f);
    s = host.snapshot(id);
    pointer(f, s, 0, PointerAction::down, .5, .5);
    open.set(false);
    sync(f);
    check(!host.snapshot(id).dragging && !f.services.pointer().state(PointerIdentity::mouse())->capture,
          "hidden panel retained gesture capture");
    for (const auto& input : f.inputs.mounted_inputs()) {
        check(!f.inputs.focused(input.component), "hidden panel retained editor focus");
    }
    open.set(true);
    sync(f);
    s = host.snapshot(id);
    pointer(f, s, 0, PointerAction::down, .5, .5);
    disabled.set(true);
    sync(f);
    check(!host.snapshot(id).dragging, "disabled picker retained gesture");
    check(f.services.destroy(id), "picker destruction failed");
    check(f.inputs.mounted_inputs().empty() && !f.inputs.sessions().active().valid(), "picker editors/session leaked");
}

void controlled_and_gradient() {
    Fixture f;
    Signal<ColorPickerValue> value{
        ColorPickerValue{std::vector<ColorStop>{{String{u8"a"}, PickerColor{ColorRgb{255, 0, 0, 1}}, 0},
                                                {String{u8"b"}, PickerColor{ColorRgb{0, 0, 255, .5}}, 100}}}};
    int requests{};
    ColorPickerValue proposed;
    f.inputs.mount(Content{[&] {
        ColorPicker(ColorPickerProps{}.value(value).defaultOpen(true).onChange([&](auto v, String) {
            ++requests;
            proposed = v;
        }));
    }});
    sync(f);
    auto& h = f.services.color_picker();
    const auto id = h.mounted().front();
    auto s = h.snapshot(id);
    const auto before = value.get();
    pointer(f, s, 3, PointerAction::down, 0);
    pointer(f, s, 3, PointerAction::move, .3);
    pointer(f, s, 3, PointerAction::up, .3);
    check(requests > 0 && h.snapshot(id).value == before && proposed.stops().front().percent > 20,
          "controlled gradient stop refused authority incorrectly");
    value.set(proposed);
    sync(f);
    check(h.snapshot(id).value == proposed, "reactive gradient writeback failed");
    s = h.snapshot(id);
    check(f.services.focus().request_focus(s.interactions[1], FocusModality::keyboard), "hue keyboard focus failed");
    const auto before_key = requests;
    f.services.focus().dispatch({Key::end, KeyAction::down});
    f.services.focus().dispatch({Key::end, KeyAction::up});
    check(requests == before_key && !f.services.next_frame_deadline(),
          "equivalent 360-degree hue emitted a change or busy timer");
    f.services.focus().dispatch({Key::right, KeyAction::down});
    f.services.focus().dispatch({Key::right, KeyAction::up});
    check(requests > before_key, "keyboard hue did not request change");
}

void reentrant_authority() {
    Fixture f;
    Signal<ColorPickerValue> value{ColorPickerValue{PickerColor{ColorRgb{255, 0, 0, 1}}}};
    int complete{};
    const ColorPickerValue authority{PickerColor{ColorRgb{0, 0, 255, 1}}};
    f.inputs.mount(Content{[&] {
        ColorPicker(ColorPickerProps{}
                        .value(value)
                        .defaultOpen(true)
                        .onChange([&](ColorPickerValue, String) { value.set(authority); })
                        .onChangeComplete([&](ColorPickerValue) { ++complete; }));
    }});
    sync(f);
    auto& h = f.services.color_picker();
    const auto id = h.mounted().front();
    const auto s = h.snapshot(id);
    pointer(f, s, 0, PointerAction::down, .4, .4);
    pointer(f, s, 0, PointerAction::up, .4, .4);
    sync(f);
    check(h.snapshot(id).value == authority && !h.snapshot(id).dragging && complete == 0 &&
              !f.services.pointer().state(PointerIdentity::mouse())->capture,
          "reentrant authority left stale candidate/capture/completion");
}

void empty_controlled_gradient() {
    Fixture f;
    Signal<ColorPickerValue> value{ColorPickerValue{}};
    ColorPickerValue requested;
    f.inputs.mount(Content{[&] {
        ColorPicker(ColorPickerProps{}
                        .mode(ColorPickerMode::Gradient)
                        .value(value)
                        .defaultOpen(true)
                        .onChange([&](ColorPickerValue next, String) { requested = next; }));
    }});
    sync(f);
    auto& h = f.services.color_picker();
    const auto id = h.mounted().front();
    const auto s = h.snapshot(id);
    pointer(f, s, 0, PointerAction::down, .4, .4);
    pointer(f, s, 0, PointerAction::up, .4, .4);
    check(requested.gradient() && h.snapshot(id).value.empty(),
          "empty controlled gradient emitted solid or overrode authority");
}

void retained_slots_and_session() {
    Fixture f;
    InputRef extra;
    InputRef trigger;
    Signal<bool> disabled{false};
    Signal<bool> open{true};
    Signal<ColorPickerValue> value{ColorPickerValue{PickerColor{}}};
    int mounts{};
    std::optional<ColorPickerPanelParts> retained_parts;
    f.inputs.mount(Content{[&] {
        ColorPickerSlots slots;
        slots.trigger = ColorPickerTrigger{[&] { Input(InputProps{}.defaultValue(String{u8"trigger"}).ref(trigger)); }};
        slots.panel_render = [&](ColorPickerPanelParts parts) {
            ++mounts;
            retained_parts = parts;
            Flex(FlexProps{}.vertical(true), [parts] {
                parts.renderPicker();
                parts.renderPresets();
            });
        };
        slots.extra = ColorPickerExtra{[&] { Input(InputProps{}.defaultValue(String{u8"draft"}).ref(extra)); }};
        ColorPicker(ColorPickerProps{}.value(value).open(open).disabled(disabled), std::move(slots));
    }});
    sync(f);
    check(mounts == 1 && extra.focus({InputFocusCursor::All}), "retained extra slot did not mount/focus");
    const auto session = f.inputs.sessions().active();
    check(bool(f.inputs.dispatch(TextCommitted{String{u8"retained"}, session})), "extra slot text commit failed");
    const auto editor = session.owner;
    value.set(ColorPickerValue{PickerColor{ColorRgb{255, 0, 0, .5}}});
    sync(f);
    check(mounts == 1 && f.inputs.editors().require(editor).value() == "retained", "color update remounted rich draft");
    check(bool(f.inputs.dispatch(CompositionChanged{String{u8"preedit"}, {7, 0}, f.inputs.sessions().active()})),
          "extra preedit failed");
    open.set(false);
    sync(f);
    check(f.inputs.sessions().active().owner != editor && !f.inputs.editors().require(editor).composition().active,
          "hidden ColorPicker retained preedit/session");
    check(!f.inputs.dispatch(TextCommitted{String{u8"late"}, session}), "hidden picker accepted stale session");
    check(trigger.focus(), "custom trigger focus failed");
    const auto trigger_session = f.inputs.sessions().active();
    check(bool(f.inputs.dispatch(CompositionChanged{String{u8"preedit"}, {7, 0}, trigger_session})),
          "trigger preedit failed");
    disabled.set(true);
    sync(f);
    check(!f.inputs.sessions().active().valid() && !trigger.focus(), "disabled rich trigger retained session/focus");
    disabled.set(false);
    sync(f);
    check(trigger.focus(), "reenabled custom trigger remained blocked");
    const auto id = f.services.color_picker().mounted().front();
    check(f.services.destroy(id), "panel render owner teardown failed");
    const auto live = f.services.components().component_count();
    retained_parts->renderPicker();
    retained_parts->renderPresets();
    check(f.services.components().component_count() == live && !extra.bound() && !trigger.bound(),
          "stale retained panel ticket remounted content");
}

void callbacks() {
    for (bool throwing : {false, true}) {
        Fixture f;
        runtime::ComponentId id;
        f.inputs.mount(Content{[&] {
            ColorPicker(ColorPickerProps{}.defaultOpen(true).onChange([&](auto, String) {
                if (throwing) {
                    throw std::runtime_error("expected");
                }
                f.services.destroy(id);
            }));
        }});
        sync(f);
        id = f.services.color_picker().mounted().front();
        const auto s = f.services.color_picker().snapshot(id);
        try {
            pointer(f, s, 0, PointerAction::down, .3, .4);
        } catch (const std::runtime_error&) {
            check(throwing, "unexpected callback exception");
        }
        if (throwing) {
            check(!f.services.color_picker().snapshot(id).dragging, "throwing callback retained gesture");
        }
        check(!f.services.pointer().state(PointerIdentity::mouse())->capture, "callback retained capture");
    }
}
} // namespace

int main() {
    try {
        values();
        theme_contract();
        mode_cache();
        controlled_format_and_css();
        retained_text_renderer();
        compact_default_hsb_label();
        panel_actions_and_presets();
        interactive();
        controlled_and_gradient();
        reentrant_authority();
        empty_controlled_gradient();
        retained_slots_and_session();
        callbacks();
    } catch (const std::exception& e) {
        std::cerr << e.what() << '\n';
        return 1;
    }
}
