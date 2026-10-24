#include "component/time_picker_component.hpp"
#include "support/input_fixture.hpp"
#include <iostream>
#if defined(_MSC_VER) && defined(_DEBUG)
#include <crtdbg.h>
#endif

namespace {
using namespace ryn;
using namespace ryn::input;
using Fixture = ryn_test::input_component::Fixture;

void check(bool value, const char* message) {
    if (!value) {
        throw std::runtime_error(message);
    }
}

void key(Fixture& f, Key key, KeyModifier modifiers = KeyModifier::none) {
    f.services.focus().dispatch({key, KeyAction::down, modifiers});
    f.services.focus().dispatch({key, KeyAction::up, modifiers});
    f.synchronize();
}

runtime::Rect bounds(Fixture& f, runtime::ComponentId id) {
    return f.nodes.require(f.services.components().root(id)).bounds;
}

void click(Fixture& f, runtime::Rect bounds) {
    const float x = bounds.x + bounds.width / 2;
    const float y = bounds.y + bounds.height / 2;
    f.services.pointer().dispatch({PointerIdentity::mouse(), PointerAction::down, PointerButton::primary, x, y});
    f.services.pointer().dispatch({PointerIdentity::mouse(), PointerAction::up, PointerButton::primary, x, y});
    f.synchronize();
}

template <std::size_t N> void type(Fixture& f, TimePickerRef ref, const char8_t (&text)[N]) {
    check(ref.focus(), "ref focus");
    key(f, Key::a, KeyModifier::control);
    const auto stamp = f.inputs.sessions().active();
    check(bool(f.inputs.dispatch(TextCommitted{String{text}, stamp})), "text input");
    f.synchronize();
}

bool contains(runtime::Rect outer, runtime::Rect inner) {
    return inner.x >= outer.x - .001F && inner.y >= outer.y - .001F &&
           inner.x + inner.width <= outer.x + outer.width + .001F &&
           inner.y + inner.height <= outer.y + outer.height + .001F;
}

void verify_column_geometry(Fixture& f, const detail::TimePickerSnapshot& s) {
    std::size_t glyphs = 0;
    for (int column = 0; column < 3; ++column) {
        const auto clip = s.column_clips[column];
        for (auto row : s.rows[column]) {
            for (const auto& text : f.services.text().mounted_texts()) {
                auto ancestor = f.services.components().parent(text.component);
                while (ancestor && *ancestor != row) {
                    ancestor = f.services.components().parent(*ancestor);
                }
                if (!ancestor) {
                    continue;
                }
                const auto range = f.scene.primitive(text.scene).instances;
                for (std::uint32_t i = 0; i < range.count; ++i) {
                    const auto& glyph = f.scene.glyph_scene().instances().at(range.first + i);
                    const auto& p = glyph.position_size;
                    const auto& c = glyph.clip_bounds;
                    const auto& t = glyph.translation_opacity;
                    const auto ink = graphics::intersect_effect_bounds({p[0] + t[0], p[1] + t[1], p[2], p[3]},
                                                                       {c[0], c[1], c[2] - c[0], c[3] - c[1]});
                    if (ink.width <= 0 || ink.height <= 0 || t[2] <= 0 || glyph.color[3] <= 0) {
                        continue;
                    }
                    ++glyphs;
                    check(contains(clip, ink) && contains(bounds(f, row), ink), "actual glyph escaped column/row clip");
                }
            }
        }
    }
    const bool room = s.column_clips[0].height > 0 && s.column_clips[0].width > 0;
    check(!room || glyphs > 0, "no visible column glyphs");
    const auto token = f.services.components().theme_scope(s.selector)->snapshot().time_picker();
    const auto color = token.colors.selected_background;
    std::size_t surfaces = 0;
    for (const auto& command : f.services.scene_composer().ordered_scene().commands()) {
        if (command.kind != graphics::SceneDrawKind::quad) {
            continue;
        }
        for (std::uint32_t i = 0; i < command.instance_count; ++i) {
            const auto& quad = f.services.surfaces().instances().at(command.first_instance + i);
            if (quad.color != std::array{color.red(), color.green(), color.blue(), color.alpha()} ||
                quad.opacity <= 0 || quad.bounds[2] <= 0 || quad.bounds[3] <= 0) {
                continue;
            }
            ++surfaces;
            const runtime::Rect rect{quad.bounds[0], quad.bounds[1], quad.bounds[2], quad.bounds[3]};
            check(std::ranges::any_of(s.column_clips, [&](auto clip) { return contains(clip, rect); }),
                  "selected surface escaped column clip");
        }
    }
    check(!room || surfaces > 0, "no actual selected surfaces");
}

void pointer_keyboard_draft_identity() {
    TimePickerRef ref;
    InputRef footer_ref;
    int runs = 0;
    int footers = 0;
    std::vector<TimePickerValue> changes;
    Fixture f;
    f.services.mount(Content{[&] {
        ++runs;
        TimePicker(TimePickerProps{}
                       .ref(ref)
                       .defaultValue(TimeOfDay{1, 2, 3})
                       .layout(LayoutStyle{}.width(dp(220)))
                       .onChange([&](TimePickerValue value) { changes.push_back(value); }),
                   TimePickerFooter{[&] {
                       ++footers;
                       Input(InputProps{}.ref(footer_ref).defaultValue(u8"footer"));
                   }});
        Button(ButtonProps{}, ButtonContent{[] { Text(u8"Next"); }});
    }});
    f.synchronize();
    const auto id = f.services.time_picker().mounted()[0];
    const auto original = f.services.time_picker().snapshot(id);
    check(ref.bound() && !original.visible && original.value == TimeOfDay{1, 2, 3}, "initial mount");
    click(f, bounds(f, original.selector));
    auto s = f.services.time_picker().snapshot(id);
    check(s.visible && changes.empty(), "pointer open");
    click(f, bounds(f, s.rows[0][3]));
    s = f.services.time_picker().snapshot(id);
    check(s.pending == TimeOfDay{3, 2, 3} && s.value == TimeOfDay{1, 2, 3} && changes.empty(),
          "column committed early");
    click(f, bounds(f, s.cancel));
    s = f.services.time_picker().snapshot(id);
    check(!s.visible && s.value == TimeOfDay{1, 2, 3} && changes.empty(), "cancel changed value");
    check(ref.focus(), "ref focus");
    key(f, Key::down);
    key(f, Key::home);
    key(f, Key::down);
    key(f, Key::right);
    key(f, Key::end);
    key(f, Key::left);
    key(f, Key::down);
    s = f.services.time_picker().snapshot(id);
    check(s.pending == TimeOfDay{2, 59, 3} && s.scroll[1] > 0, "column keyboard/reveal");
    key(f, Key::enter);
    s = f.services.time_picker().snapshot(id);
    check(!s.visible && s.value == TimeOfDay{2, 59, 3} && changes.size() == 1, "Enter confirm");
    type(f, ref, u8"23:59:58");
    key(f, Key::enter);
    check(f.services.time_picker().snapshot(id).value == TimeOfDay{23, 59, 58}, "typed time not committed");
    type(f, ref, u8"25:");
    check(ref.blur(), "ref blur");
    f.synchronize();
    check(f.inputs.display_snapshot(s.selector).text == "23:59:58" && changes.size() == 2, "invalid blur restoration");
    click(f, bounds(f, s.selector));
    s = f.services.time_picker().snapshot(id);
    const auto value = s.value;
    const auto pending = s.pending;
    const auto clip = s.column_clips[0];
    check(f.services.dispatch_scroll({0, -20, clip.x + 5, clip.y + 5}), "column wheel unhandled");
    f.synchronize();
    s = f.services.time_picker().snapshot(id);
    check(s.scroll[0] > 0 && s.value == value && s.pending == pending && changes.size() == 2,
          "wheel selected or submitted");
    check(footer_ref.focus(), "footer focus");
    f.synchronize();
    check(f.services.time_picker().snapshot(id).visible, "footer focus closed panel");
    const auto footer_stamp = f.inputs.sessions().active();
    check(bool(f.inputs.dispatch(CompositionChanged{String{u8"zhong"}, {}, footer_stamp})), "footer preedit");
    key(f, Key::escape);
    check(f.services.time_picker().snapshot(id).visible &&
              !f.inputs.editors().require(footer_stamp.owner).composition().active,
          "TimePicker stole footer preedit Escape");
    key(f, Key::escape);
    s = f.services.time_picker().snapshot(id);
    check(!s.visible && s.selector == original.selector && s.rows == original.rows && s.footer == original.footer &&
              runs == 1 && footers == 1,
          "ordinary updates remounted content");
    click(f, bounds(f, s.clear));
    check(!f.services.time_picker().snapshot(id).value && changes.size() == 3 && !changes.back(), "clear value");
    f.services.destroy(id);
    f.synchronize();
    check(!ref.bound() && !ref.focus() && !ref.blur() && !footer_ref.bound() &&
              f.services.time_picker().mounted().empty(),
          "stale ref/lifetime");
}

void controlled_immediate_preedit_and_rules() {
    TimePickerRef ref;
    Signal<TimePickerValue> value{TimeOfDay{8, 7, 59}};
    Signal<bool> immediate{false};
    Signal<TimePickerPrecision> precision{TimePickerPrecision::Minute};
    Signal<int> rule_mode{0};
    int changes = 0;
    TimePickerValue intent;
    Fixture f;
    f.services.mount(Content{[&] {
        TimePicker(TimePickerProps{}
                       .value(value)
                       .ref(ref)
                       .precision(precision)
                       .needConfirm(bind([&] { return !immediate.get(); }))
                       .rules(bind([&] {
                           TimePickerRules rules;
                           const int mode = rule_mode.get();
                           rules.minute_step = mode == 1 ? 15 : 1;
                           if (mode == 2) {
                               rules.disabled = [](TimeOfDay) {
                                   return true;
                               };
                           }
                           return rules;
                       }))
                       .onChange([&](TimePickerValue next) {
                           ++changes;
                           intent = next;
                       }));
    }});
    f.synchronize();
    const auto id = f.services.time_picker().mounted()[0];
    auto s = f.services.time_picker().snapshot(id);
    check(s.value == TimeOfDay{8, 7, 59} && f.inputs.display_snapshot(s.selector).text == "08:07",
          "minute display authority");
    check(ref.focus(), "focus");
    key(f, Key::down);
    const auto stamp = f.inputs.sessions().active();
    check(bool(f.inputs.dispatch(CompositionChanged{String{u8"zhong"}, {}, stamp})), "preedit rejected");
    key(f, Key::down);
    key(f, Key::enter);
    check(changes == 0 && f.services.time_picker().snapshot(id).pending == TimeOfDay{8, 7}, "preedit changed time");
    key(f, Key::escape);
    check(f.services.time_picker().snapshot(id).visible, "IME Escape closed popup");
    key(f, Key::down);
    key(f, Key::enter);
    s = f.services.time_picker().snapshot(id);
    check(changes == 1 && intent == TimeOfDay{9, 7} && s.value == TimeOfDay{8, 7, 59} &&
              f.inputs.display_snapshot(s.selector).text == "08:07",
          "controlled rejection");
    immediate.set(true);
    rule_mode.set(1);
    key(f, Key::down);
    key(f, Key::down);
    s = f.services.time_picker().snapshot(id);
    check(s.visible && intent && intent->second() == 0 && intent->minute() % 15 == 0 && changes == 2,
          "immediate minute rules");
    rule_mode.set(2);
    for (int i = 0; i < 3; ++i) {
        key(f, Key::down);
        key(f, Key::home);
        key(f, Key::end);
        key(f, Key::right);
    }
    key(f, Key::enter);
    check(changes == 2 && !f.services.time_picker().snapshot(id).confirmable, "all disabled committed");
    key(f, Key::escape);
    check(ref.blur(), "blur");
    f.synchronize();
    static_cast<void>(f.frames.consume_request());
    f.synchronize();
    check(!f.frames.pending() && !f.services.next_frame_deadline(), "idle popup requested ongoing frames");
}

void sizes_variants_and_clips() {
    for (auto size : {ControlSize::Small, ControlSize::Middle, ControlSize::Large}) {
        for (auto variant :
             {InputVariant::Outlined, InputVariant::Filled, InputVariant::Borderless, InputVariant::Underlined}) {
            Fixture f;
            Signal<ThemeConfig> theme{ThemeConfig{}};
            f.services.mount(Content{[&] {
                Theme(ThemeProps{}.config(theme), ThemeContent{[&] {
                          TimePicker(TimePickerProps{}
                                         .defaultOpen()
                                         .size(size)
                                         .variant(variant)
                                         .status(InputStatus::Warning)
                                         .inputReadOnly()
                                         .defaultValue(TimeOfDay{3})
                                         .layout(LayoutStyle{}.width(dp(120))));
                      }});
            }});
            f.synchronize(90);
            const auto id = f.services.time_picker().mounted()[0];
            const auto s = f.services.time_picker().snapshot(id);
            check(f.inputs.size(s.selector) == size && f.inputs.variant(s.selector) == variant &&
                      f.inputs.status(s.selector) == InputStatus::Warning,
                  "Input size/variant/status forwarding");
            check(s.visible && s.bounds.width <= 90 && s.rows[1].size() == 60, "real finite time panel");
            check(f.scene.glyph_scene().instances().size() > 0, "time panel has no glyphs");
            verify_column_geometry(f, s);
            if (size == ControlSize::Small && variant == InputVariant::Outlined) {
                check(f.services.layout_and_synchronize({90, 70}, {0, 0, 90, 70}), "short viewport layout");
                verify_column_geometry(f, f.services.time_picker().snapshot(id));
                f.synchronize(90);
                verify_column_geometry(f, f.services.time_picker().snapshot(id));
                static_cast<void>(f.frames.consume_request());
                f.synchronize(90);
                check(!f.frames.pending() && !f.services.next_frame_deadline(),
                      "unfocused open panel schedules idle frames");
            }
            const auto clip = s.column_clips[0];
            const auto hidden = bounds(f, s.rows[0][23]);
            const auto hit = f.services.hit_test().hit_test({clip.x + 2, std::min(239.0F, hidden.y + 1)});
            if (hit) {
                const auto* target = f.services.interactions().find(*hit);
                check(target && target->component != s.rows[0][23], "clipped row hit");
            }
            ThemeConfig config;
            config.time_picker.tokens.cell_height = dp(31);
            config.time_picker.tokens.font_size = dp(16);
            theme.set(config);
            f.synchronize(90);
            const auto changed = f.services.time_picker().snapshot(id);
            check(changed.selector == s.selector && changed.rows == s.rows && changed.footer == s.footer,
                  "theme remounted");
            if (size == ControlSize::Small && variant == InputVariant::Outlined) {
                static_cast<void>(f.frames.consume_request());
                config.time_picker.tokens.background = Color::rgba8(20, 30, 40);
                theme.set(config);
                check(f.frames.pending(), "panel-only Theme color did not wake a frame");
                f.synchronize(90);
                static_cast<void>(f.frames.consume_request());
                config.time_picker.tokens.z_index_popup = 1100;
                theme.set(config);
                check(f.frames.pending(), "popup order Theme did not wake a frame");
                f.synchronize(90);
            }
        }
    }
}
} // namespace

int main() {
#if defined(_MSC_VER) && defined(_DEBUG)
    _CrtSetReportMode(_CRT_ASSERT, _CRTDBG_MODE_FILE);
    _CrtSetReportFile(_CRT_ASSERT, _CRTDBG_FILE_STDERR);
#endif
    try {
        pointer_keyboard_draft_identity();
        controlled_immediate_preedit_and_rules();
        sizes_variants_and_clips();
        std::cout << "TimePicker retained component contracts passed\n";
        return 0;
    } catch (const std::exception& e) {
        std::cerr << e.what() << '\n';
        return 1;
    }
}
