#include "component/date_picker_component.hpp"
#include "component/select_component.hpp"
#include "component/modal_component.hpp"
#include "support/input_fixture.hpp"
#include <cmath>
#include <iostream>
#if defined(_MSC_VER) && defined(_DEBUG)
#include <crtdbg.h>
#endif
namespace {
using namespace ryn;
using namespace ryn::input;
using Fixture = ryn_test::input_component::Fixture;

void check(bool v, const char* m) {
    if (!v) {
        throw std::runtime_error(m);
    }
}

void sync(Fixture& f, float w = 800, float h = 700) {
    check(f.services.layout_and_synchronize({w, h}, {0, 0, w, h}), "sync");
}

runtime::Rect bounds(Fixture& f, runtime::ComponentId id) {
    return f.nodes.require(f.services.components().root(id)).bounds;
}

bool descendant(Fixture& f, runtime::ComponentId child, runtime::ComponentId ancestor) {
    for (auto p = std::optional{child}; p; p = f.services.components().parent(*p)) {
        if (*p == ancestor) {
            return true;
        }
    }
    return false;
}

void key(Fixture& f, Key key, KeyModifier modifiers = KeyModifier::none) {
    f.services.focus().dispatch({key, KeyAction::down, modifiers});
    f.services.focus().dispatch({key, KeyAction::up, modifiers});
    sync(f);
}

void click(Fixture& f, runtime::Rect rect) {
    for (auto action : {PointerAction::down, PointerAction::up}) {
        f.services.pointer().dispatch({PointerIdentity::mouse(), action, PointerButton::primary,
                                       rect.x + rect.width / 2, rect.y + rect.height / 2});
    }
    sync(f);
}

void edit(Fixture& f, ryn::String value) {
    const auto stamp = f.inputs.sessions().active();
    (void)f.inputs.editors().require(stamp.owner).select_all();
    check(bool(f.inputs.dispatch(TextCommitted{std::move(value), stamp})), "text commit");
    sync(f);
}

void selection_and_identity() {
    DatePickerRef ref;
    Signal<ThemeConfig> theme{ThemeConfig{}};
    Signal<InputStatus> status{InputStatus::Default};
    Signal<ControlSize> size{ControlSize::Middle};
    Signal<InputVariant> variant{InputVariant::Outlined};
    int runs = 0;
    int footer_runs = 0;
    std::vector<DatePickerValue> changes;
    Fixture f;
    f.services.mount(Content{[&] {
        Theme(ThemeProps{}.config(theme), ThemeContent{[&] {
                  ++runs;
                  DatePicker(DatePickerProps{}
                                 .defaultPanelDate({2024, 2, 1})
                                 .ref(ref)
                                 .size(size)
                                 .variant(variant)
                                 .status(status)
                                 .layout(LayoutStyle{}.width(dp(210)))
                                 .onChange([&](DatePickerValue d) { changes.push_back(d); }),
                             DatePickerFooter{[&] {
                                 ++footer_runs;
                                 Text(u8"Footer");
                             }});
                  Button(ButtonProps{}, [] { Text(u8"Next"); });
              }});
    }});
    sync(f);
    const auto id = f.services.date_picker().mounted()[0];
    const auto original = f.services.date_picker().snapshot(id);
    click(f, bounds(f, original.input));
    check(f.services.date_picker().snapshot(id).visible && original.cells.size() == 42, "pointer open");
    click(f, bounds(f, original.cells[32]));
    auto s = f.services.date_picker().snapshot(id);
    check(s.value == DatePickerValue{CivilDate{2024, 2, 29}} && !s.visible && changes.size() == 1 &&
              f.services.focus().state().focused == f.inputs.mounted_inputs()[0].interaction,
          "pointer leap selection/focus restore");
    key(f, Key::down);
    key(f, Key::page_down);
    key(f, Key::home);
    key(f, Key::right);
    key(f, Key::enter);
    check(f.services.date_picker().snapshot(id).value == DatePickerValue{CivilDate{2024, 3, 2}}, "keyboard selection");
    key(f, Key::down);
    key(f, Key::page_up, KeyModifier::shift);
    key(f, Key::end);
    key(f, Key::up);
    key(f, Key::left);
    check(f.services.date_picker().snapshot(id).active == CivilDate{2023, 3, 23}, "year/week/day/boundary navigation");
    key(f, Key::page_down, KeyModifier::shift);
    key(f, Key::escape);
    check(f.services.date_picker().snapshot(id).value == DatePickerValue{CivilDate{2024, 3, 2}},
          "browsing changed selection");
    for (auto control : {ControlSize::Small, ControlSize::Middle, ControlSize::Large}) {
        size.set(control);
        for (auto style :
             {InputVariant::Outlined, InputVariant::Filled, InputVariant::Borderless, InputVariant::Underlined}) {
            variant.set(style);
            status.set(InputStatus::Warning);
            sync(f);
            check(f.inputs.size(s.input) == control && f.inputs.variant(s.input) == style &&
                      f.inputs.status(s.input) == InputStatus::Warning,
                  "Input forwarding");
        }
    }
    ThemeConfig config;
    config.algorithms = {ThemeAlgorithm::Dark};
    theme.set(config);
    sync(f);
    config.algorithms = {ThemeAlgorithm::Compact};
    theme.set(config);
    sync(f);
    s = f.services.date_picker().snapshot(id);
    check(s.input == original.input && s.footer == original.footer && s.cells == original.cells && runs == 1 &&
              footer_runs == 1,
          "ordinary update remounted");
    bool cleared = false;
    for (auto hit : f.services.interactions().declaration_order()) {
        const auto* record = f.services.interactions().find(hit);
        if (record && record->eligible && !record->focusable && descendant(f, record->component, s.input)) {
            click(f, bounds(f, record->component));
            cleared = true;
            break;
        }
    }
    check(cleared && !f.services.date_picker().snapshot(id).value && !changes.back(), "clear");
    key(f, Key::down);
    key(f, Key::tab);
    check(!f.services.date_picker().snapshot(id).visible &&
              f.services.focus().state().focused == f.buttons.mounted_buttons().back().interaction,
          "42 cells became Tab stops");
}

void drafts_controlled_and_preedit() {
    DatePickerRef ref;
    Signal<DatePickerValue> value{CivilDate{2024, 2, 29}};
    Signal<ThemeConfig> theme{ThemeConfig{}};
    Signal<InputStatus> status{InputStatus::Default};
    std::vector<DatePickerValue> requests;
    Fixture f;
    f.services.mount(Content{[&] {
        Theme(ThemeProps{}.config(theme), ThemeContent{[&] {
                  DatePicker(DatePickerProps{}
                                 .value(value)
                                 .defaultPanelDate({2024, 1, 1})
                                 .status(status)
                                 .ref(ref)
                                 .onChange([&](DatePickerValue d) { requests.push_back(d); }));
                  Button(ButtonProps{}, [] { Text(u8"Other"); });
              }});
    }});
    sync(f);
    const auto id = f.services.date_picker().mounted()[0];
    const auto original = f.services.date_picker().snapshot(id);
    check(ref.focus(), "ref focus");
    edit(f, u8"2023-02-29");
    key(f, Key::enter);
    check(requests.empty() && f.services.date_picker().snapshot(id).draft == String{u8"2023-02-29"},
          "invalid draft published");
    const auto session = f.inputs.sessions().active();
    status.set(InputStatus::Error);
    ThemeConfig config;
    config.date_picker.tokens.text = Color::rgba8(70, 10, 90);
    theme.set(config);
    sync(f);
    check(f.inputs.sessions().active() == session &&
              f.services.date_picker().snapshot(id).draft == String{u8"2023-02-29"},
          "status/theme reset draft/session");
    key(f, Key::tab);
    check(f.services.date_picker().snapshot(id).draft == String{u8"2024-02-29"} &&
              f.services.focus().state().focused == f.buttons.mounted_buttons().back().interaction,
          "blur failed restore or stole focus");
    check(ref.focus(), "ref refocus");
    edit(f, u8"2024-03-03");
    key(f, Key::enter);
    check(requests == std::vector<DatePickerValue>{CivilDate{2024, 3, 3}} &&
              f.services.date_picker().snapshot(id).draft == String{u8"2024-02-29"},
          "controlled rejection display");
    value.set(CivilDate{2024, 3, 3});
    sync(f);
    check(f.services.date_picker().snapshot(id).draft == String{u8"2024-03-03"}, "controlled echo");
    key(f, Key::down);
    const auto active = f.services.date_picker().snapshot(id).active;
    const auto stamp = f.inputs.sessions().active();
    check(bool(f.inputs.dispatch(CompositionChanged{String{u8"2025"}, {}, stamp})), "preedit");
    key(f, Key::down);
    key(f, Key::page_down);
    key(f, Key::enter);
    check(requests.size() == 1 && f.services.date_picker().snapshot(id).active == active,
          "preedit published or calendar stole IME");
    key(f, Key::escape);
    check(f.services.date_picker().snapshot(id).visible, "IME Escape closed panel");
    key(f, Key::escape);
    check(!f.services.date_picker().snapshot(id).visible, "Escape close");
    check(f.services.date_picker().snapshot(id).input == original.input, "Input identity");
}

void range_readonly_and_no_search() {
    DatePickerRef ref;
    Signal<DatePickerValue> min{CivilDate{2024, 2, 10}};
    Signal<DatePickerValue> max{CivilDate{2024, 3, 20}};
    Signal<DisabledDate> disabled{DisabledDate{[](CivilDate d) { return d.day() == 10; }}};
    Signal<bool> read_only{false};
    std::vector<DatePickerValue> changes;
    Fixture f;
    f.services.mount(Content{[&] {
        DatePicker(DatePickerProps{}
                       .defaultPanelDate({2024, 2, 10})
                       .minDate(min)
                       .maxDate(max)
                       .disabledDate(disabled)
                       .inputReadOnly(read_only)
                       .ref(ref)
                       .onChange([&](DatePickerValue d) { changes.push_back(d); }));
    }});
    sync(f);
    const auto id = f.services.date_picker().mounted()[0];
    check(ref.focus(), "range focus");
    edit(f, u8"2024-02-09");
    key(f, Key::enter);
    edit(f, u8"2024-02-10");
    key(f, Key::enter);
    check(changes.empty(), "range/disabled input accepted");
    (void)ref.blur();
    sync(f);
    read_only.set(true);
    sync(f);
    check(ref.focus(), "readonly focus");
    const auto stamp = f.inputs.sessions().active();
    (void)f.inputs.dispatch(TextCommitted{String{u8"2024-02-11"}, stamp});
    sync(f);
    check(f.services.date_picker().snapshot(id).draft.empty(), "readonly edited");
    key(f, Key::down);
    key(f, Key::right);
    key(f, Key::enter);
    check(changes.size() == 1 && changes.back() == DatePickerValue{CivilDate{2024, 2, 11}},
          "readonly blocked calendar");
    min.set(CivilDate{2024, 3, 1});
    sync(f);
    check(f.services.date_picker().snapshot(id).value == DatePickerValue{CivilDate{2024, 2, 11}},
          "range coerced selection");
    int calls = 0;
    disabled.set(DisabledDate{[&](CivilDate) {
        ++calls;
        return true;
    }});
    sync(f);
    key(f, Key::down);
    const auto before = calls;
    key(f, Key::page_up);
    key(f, Key::home);
    key(f, Key::enter);
    check(calls - before < 500 && changes.size() == 1, "unbounded disabled-date search");
}

void controlled_open_and_boundaries() {
    DatePickerRef ref;
    Signal<bool> open{false};
    Signal<bool> disabled{false};
    std::vector<bool> requests;
    Fixture f;
    f.services.mount(Content{[&] {
        DatePicker(DatePickerProps{}
                       .defaultPanelDate({2024, 2, 1})
                       .open(open)
                       .disabled(disabled)
                       .ref(ref)
                       .onOpenChange([&](bool b) { requests.push_back(b); }));
    }});
    sync(f);
    const auto id = f.services.date_picker().mounted()[0];
    check(ref.focus(), "controlled focus");
    key(f, Key::down);
    check(!f.services.date_picker().snapshot(id).visible && requests == std::vector<bool>{true},
          "controlled open mutated");
    open.set(true);
    sync(f);
    key(f, Key::escape);
    check(f.services.date_picker().snapshot(id).visible && requests.back() == false,
          "controlled close refusal ignored");
    edit(f, u8"invalid");
    const auto outside_requests = requests.size();
    click(f, {700, 600, 10, 10});
    check(f.services.date_picker().snapshot(id).visible && requests.size() == outside_requests + 1,
          "controlled outside blur duplicated close or ignored refusal");
    check(ref.focus(), "outside refocus");
    edit(f, u8"2024-02-12");
    const auto close_requests = requests.size();
    disabled.set(true);
    sync(f);
    check(!f.services.date_picker().snapshot(id).visible && !f.inputs.sessions().active().owner.valid() &&
              !f.services.date_picker().snapshot(id).value && requests.size() == close_requests + 1,
          "disabled retained panel/session");
    disabled.set(false);
    sync(f);
    check(!f.services.date_picker().snapshot(id).visible, "disabled recovery reopened");
    open.set(false);
    open.set(true);
    sync(f);
    check(f.services.date_picker().snapshot(id).visible, "explicit reopen");
    f.services.components().set_branch_active(id, false);
    sync(f);
    f.services.components().set_branch_active(id, true);
    sync(f);
    check(!f.services.date_picker().snapshot(id).visible, "hidden recovery resurrected controlled popup");
    check(ref.focus(), "hidden refocus");
    key(f, Key::down);
    check(f.services.date_picker().snapshot(id).visible, "explicit action did not clear suppression");
    f.services.set_window_active(false);
    f.services.set_window_active(true);
    sync(f);
    check(!f.services.date_picker().snapshot(id).visible, "window recovery reopened");
}

void spin_modal_and_child_popup() {
    Signal<bool> busy{false};
    Signal<bool> modal{false};
    Signal<bool> owner{true};
    DatePickerRef ref;
    SelectRef child;
    int footer_runs = 0;
    Fixture f;
    f.services.mount(Content{[&] {
        Modal(ModalProps{}.open(owner).footer(ModalFooterMode::None), ModalContent{[&] {
                  Spin(SpinProps{}.spinning(busy), SpinContent{[&] {
                           DatePicker(DatePickerProps{}.defaultPanelDate({2024, 2, 1}).defaultOpen().ref(ref),
                                      DatePickerFooter{[&] {
                                          ++footer_runs;
                                          Select(SelectProps{}.options({{1, u8"One"}, {2, u8"Two"}}).ref(child));
                                          TextArea(
                                              TextAreaProps{}.defaultValue(u8"1\n2\n3\n4\n5\n6\n7\n8\n9\n10").rows(2));
                                      }});
                       }});
              }});
        Modal(ModalProps{}.open(modal).footer(ModalFooterMode::None), ModalContent{[] { Text(u8"Higher Modal"); }});
    }});
    sync(f);
    const auto id = f.services.date_picker().mounted()[0];
    check(f.services.date_picker().snapshot(id).visible, "Modal DatePicker initial open");
    check(child.focus(), "footer child focus");
    key(f, Key::down);
    const auto child_id = f.services.select().mounted()[0];
    check(f.services.select().snapshot(child_id).visible && f.services.date_picker().snapshot(id).visible,
          "child popup open closed owner");
    const auto child_popup = f.services.select().snapshot(child_id).popup;
    const ScrollInputEvent wheel{0, -1, child_popup.x + 5, child_popup.y + 5};
    check(!f.services.date_picker().consume_scroll_boundary(wheel) && f.inputs.dispatch(wheel),
          "nearest child popup wheel ownership");
    key(f, Key::escape);
    check(!f.services.select().snapshot(child_id).visible && f.services.date_picker().snapshot(id).visible,
          "owner stole child Escape");
    const auto area = f.inputs.mounted_inputs().back().component;
    const auto view = f.inputs.layout_snapshot(area).viewport;
    check(f.inputs.dispatch(ScrollInputEvent{0, -1, view.x + 5, view.y + view.height / 2}), "footer wheel unhandled");
    sync(f);
    check(f.inputs.layout_snapshot(area).vertical_scroll > 0, "calendar fallback stole footer wheel");
    busy.set(true);
    sync(f);
    busy.set(false);
    sync(f);
    check(!f.services.date_picker().snapshot(id).visible, "Spin recovery resurrected panel");
    check(ref.focus(), "Spin recovery focus");
    key(f, Key::down);
    modal.set(true);
    sync(f);
    check(!f.services.date_picker().snapshot(id).visible, "higher Modal failed suppression");
    modal.set(false);
    sync(f);
    check(!f.services.date_picker().snapshot(id).visible && footer_runs == 1, "Modal recovery resurrected/remounted");
    owner.set(false);
    sync(f);
    owner.set(true);
    sync(f);
    check(!f.services.date_picker().snapshot(id).visible && footer_runs == 1, "own Modal recovery reopened");
}

bool within(runtime::Rect r, runtime::Rect outer) {
    return r.x >= outer.x - .02F && r.y >= outer.y - .02F && r.x + r.width <= outer.x + outer.width + .02F &&
           r.y + r.height <= outer.y + outer.height + .02F;
}

void retained_footer_editor_and_color() {
    Signal<ThemeConfig> theme{ThemeConfig{}};
    Signal<InputStatus> status{InputStatus::Default};
    InputRef footer_ref;
    int runs = 0;
    Fixture f;
    f.services.mount(Content{[&] {
        Theme(ThemeProps{}.config(theme), ThemeContent{[&] {
                  DatePicker(DatePickerProps{}.defaultPanelDate({2024, 2, 1}).defaultOpen().status(status),
                             DatePickerFooter{[&] {
                                 ++runs;
                                 Input(InputProps{}.defaultValue(u8"Footer").ref(footer_ref));
                             }});
              }});
    }});
    sync(f);
    const auto id = f.services.date_picker().mounted()[0];
    const auto original = f.services.date_picker().snapshot(id);
    const auto input = f.inputs.mounted_inputs().back();
    check(footer_ref.focus(), "footer editor focus");
    edit(f, u8"Draft");
    const auto session = f.inputs.sessions().active();
    click(f, bounds(f, original.navigation[2]));
    check(f.inputs.sessions().active() == session && f.inputs.editors().require(input.editor).value() == "Draft",
          "month update replaced footer editor");
    const auto node = f.services.components().root(original.popup);
    const auto measured = f.nodes.require(node).measure_count;
    ThemeConfig config;
    config.date_picker.tokens.text = Color::rgba8(18, 40, 70);
    theme.set(config);
    sync(f);
    check(f.nodes.require(node).measure_count == measured, "DatePicker color remeasured panel");
    bool glyph = false;
    for (auto text : f.services.text().mounted_texts()) {
        if (!descendant(f, text.component, original.cells[20])) {
            continue;
        }
        const auto primitive = f.scene.primitive(text.scene);
        if (primitive.instances.count) {
            const auto& g = f.scene.glyph_scene().instances().at(primitive.instances.first);
            glyph = g.color == std::array{18 / 255.0F, 40 / 255.0F, 70 / 255.0F, 1.0F};
        }
    }
    check(glyph, "DatePicker color did not reach actual glyph");
    status.set(InputStatus::Error);
    sync(f);
    for (auto algorithm : {ThemeAlgorithm::Dark, ThemeAlgorithm::Compact, ThemeAlgorithm::Default}) {
        config.algorithms = {algorithm};
        theme.set(config);
        sync(f);
    }
    const auto after = f.services.date_picker().snapshot(id);
    check(after.input == original.input && after.footer == original.footer && after.cells == original.cells &&
              f.inputs.mounted_inputs().back().editor == input.editor && f.inputs.sessions().active() == session &&
              runs == 1 && f.inputs.editors().require(input.editor).value() == "Draft",
          "Theme/status update replaced retained footer");
    check(bool(f.inputs.dispatch(CompositionChanged{String{u8"preedit"}, {}, session})), "footer preedit");
    key(f, Key::escape);
    check(f.services.date_picker().snapshot(id).visible &&
              !f.inputs.editors().require(input.editor).composition().active,
          "DatePicker stole footer IME Escape");
    key(f, Key::escape);
    check(!f.services.date_picker().snapshot(id).visible &&
              f.services.focus().state().focused == f.inputs.mounted_inputs()[0].interaction,
          "footer Escape did not restore DatePicker focus");
}

void finite_scene_and_idle() {
    Fixture f;
    f.services.mount(Content{[] { DatePicker(DatePickerProps{}.defaultPanelDate({2024, 2, 1}).defaultOpen()); }});
    sync(f);
    const auto id = f.services.date_picker().mounted()[0];
    const auto original = f.services.date_picker().snapshot(id);
    const auto measures = f.nodes.require(f.services.components().root(original.popup)).measure_count;
    const auto surface = f.services.surfaces().diagnostics();
    (void)f.frames.consume_request();
    sync(f);
    sync(f);
    check(!f.frames.pending() && !f.services.next_frame_deadline() &&
              f.nodes.require(f.services.components().root(original.popup)).measure_count == measures &&
              f.services.surfaces().diagnostics().geometry_updates == surface.geometry_updates,
          "visible idle work");
    for (auto size : {runtime::Size{130, 100}, runtime::Size{12, 40}, runtime::Size{12, 32}, runtime::Size{800, 700}}) {
        sync(f, size.width, size.height);
        const auto s = f.services.date_picker().snapshot(id);
        check(within(s.bounds, {0, 0, size.width, size.height}) && s.cells == original.cells,
              "popup finite bounds/identity");
        std::size_t visible = 0;
        for (auto text : f.services.text().mounted_texts()) {
            if (!descendant(f, text.component, s.popup)) {
                continue;
            }
            const auto primitive = f.scene.primitive(text.scene);
            for (std::uint32_t i = 0; i < primitive.instances.count; ++i) {
                const auto& g = f.scene.glyph_scene().instances().at(primitive.instances.first + i);
                for (auto v : g.position_size) {
                    check(std::isfinite(v), "nonfinite glyph");
                }
                const float x = std::max(g.position_size[0] + g.translation_opacity[0], g.clip_bounds[0]);
                const float y = std::max(g.position_size[1] + g.translation_opacity[1], g.clip_bounds[1]);
                const float right =
                    std::min(g.position_size[0] + g.translation_opacity[0] + g.position_size[2], g.clip_bounds[2]);
                const float bottom =
                    std::min(g.position_size[1] + g.translation_opacity[1] + g.position_size[3], g.clip_bounds[3]);
                if (right > x && bottom > y && g.translation_opacity[2] > 0) {
                    ++visible;
                    check(within({x, y, right - x, bottom - y}, s.bounds), "actual glyph escaped popup clip");
                }
            }
        }
        if (s.bounds.height == 0) {
            check(visible == 0, "zero popup glyph leak");
        }
        if (size.width == 800) {
            check(visible > 42, "real calendar glyphs absent");
        }
        for (const auto& record : f.services.hit_test().records()) {
            const auto* interaction = f.services.interactions().find(record.interaction);
            if (interaction && descendant(f, interaction->component, s.popup)) {
                const auto a = record.translated_bounds;
                const auto b = record.effective_clip;
                const float x = std::max(a.x, b.x);
                const float y = std::max(a.y, b.y);
                const float w = std::max(0.0F, std::min(a.x + a.width, b.x + b.width) - x);
                const float h = std::max(0.0F, std::min(a.y + a.height, b.y + b.height) - y);
                check(!record.effective_eligible || w == 0 || h == 0 || within({x, y, w, h}, s.bounds),
                      "hit escaped popup");
                if (s.bounds.height == 0) {
                    check(!f.services.hit_test().hit_test({s.bounds.x + 1, s.bounds.y + 1}), "zero popup hit");
                }
            }
        }
        for (auto surface_id : s.surfaces) {
            const auto range = f.services.surfaces().visual_range(surface_id);
            for (std::uint32_t i = 0; i < range.count; ++i) {
                const auto& q = f.services.surfaces().instances().at(range.first + i);
                for (auto v : q.bounds) {
                    check(std::isfinite(v), "nonfinite surface");
                }
                if (q.opacity > 0 && q.bounds[2] > 0 && q.bounds[3] > 0) {
                    check(within({q.bounds[0] + q.translation[0], q.bounds[1] + q.translation[1], q.bounds[2],
                                  q.bounds[3]},
                                 s.bounds),
                          "actual surface escaped popup");
                }
            }
        }
    }
}

void callback_retirement_and_validation() {
    for (int mode = 0; mode < 4; ++mode) {
        Fixture f;
        DatePickerRef ref;
        runtime::ComponentId id;
        f.services.mount(Content{[&] {
            auto props = DatePickerProps{}.defaultPanelDate({2024, 2, 1}).ref(ref);
            const auto retire = [&] {
                f.services.destroy(id);
            };
            if (mode == 0) {
                props.onChange([retire](DatePickerValue) { retire(); });
            }
            if (mode == 1) {
                props.onOpenChange([retire](bool) { retire(); });
            }
            if (mode == 2) {
                props.onFocus(retire);
            }
            if (mode == 3) {
                props.onBlur(retire);
            }
            DatePicker(props);
        }});
        sync(f);
        id = f.services.date_picker().mounted()[0];
        (void)ref.focus();
        if (mode != 2) {
            key(f, Key::down);
        }
        if (mode == 0) {
            key(f, Key::enter);
        }
        if (mode == 3) {
            (void)ref.blur();
        }
        sync(f);
        check(!f.services.components().contains(id) && !ref.bound() && f.services.interactions().size() == 0 &&
                  f.services.surfaces().size() == 0 && !f.inputs.sessions().active().owner.valid(),
              "callback destruction leaked");
    }
    for (int mode = 0; mode < 3; ++mode) {
        Fixture f;
        bool rejected = false;
        try {
            f.services.mount(Content{[&] {
                auto p = DatePickerProps{}.defaultPanelDate({2024, 2, 1});
                if (mode == 0) {
                    p.value(CivilDate{2024, 2, 1}).defaultValue({});
                }
                if (mode == 1) {
                    p.open(false).defaultOpen();
                }
                if (mode == 2) {
                    p.minDate({2025, 1, 1}).maxDate({2024, 1, 1});
                }
                DatePicker(p);
            }});
        } catch (const std::invalid_argument&) {
            rejected = true;
        }
        check(rejected && f.services.date_picker().mounted().empty() && f.services.interactions().size() == 0,
              "invalid props leaked");
    }
}
} // namespace

int main() {
#if defined(_MSC_VER) && defined(_DEBUG)
    _CrtSetReportMode(_CRT_ASSERT, _CRTDBG_MODE_FILE);
    _CrtSetReportFile(_CRT_ASSERT, _CRTDBG_FILE_STDERR);
#endif
    try {
        selection_and_identity();
        std::cerr << "selection passed\n";
        drafts_controlled_and_preedit();
        std::cerr << "drafts passed\n";
        range_readonly_and_no_search();
        std::cerr << "range passed\n";
        controlled_open_and_boundaries();
        std::cerr << "controlled passed\n";
        spin_modal_and_child_popup();
        std::cerr << "nested passed\n";
        retained_footer_editor_and_color();
        finite_scene_and_idle();
        std::cerr << "geometry passed\n";
        callback_retirement_and_validation();
        std::cout << "DatePicker retained contracts passed\n";
    } catch (const std::exception& e) {
        std::cerr << e.what() << '\n';
        return 1;
    }
}
