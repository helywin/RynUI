#include "component/select_component.hpp"
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

void key(Fixture& f, Key value, KeyModifier modifiers = KeyModifier::none) {
    f.services.focus().dispatch({value, KeyAction::down, modifiers});
    f.services.focus().dispatch({value, KeyAction::up, modifiers});
    f.synchronize();
}

void click(Fixture& f, runtime::Rect bounds) {
    const auto x = bounds.x + bounds.width / 2;
    const auto y = bounds.y + bounds.height / 2;
    f.services.pointer().dispatch({PointerIdentity::mouse(), PointerAction::down, PointerButton::primary, x, y});
    f.services.pointer().dispatch({PointerIdentity::mouse(), PointerAction::up, PointerButton::primary, x, y});
    f.synchronize();
}

runtime::Rect bounds(Fixture& f, runtime::ComponentId id) {
    return f.nodes.require(f.services.components().root(id)).bounds;
}

void direction_and_rich_affix_size() {
    Fixture f;
    Signal<TextDirection> direction{TextDirection::RightToLeft};
    int clicks = 0;
    f.inputs.mount(Content{[&] {
        Select(SelectProps{}.showSearch().direction(direction).layout(LayoutStyle{}.width(dp(180))), SelectPrefix{[&] {
                   Button(ButtonProps{}.size(ControlSize::Small).onClick([&] { ++clicks; }),
                          ButtonSlots{.icon = ButtonIcon{[] { Icon(IconProps{}.name(IconName::SearchOutlined)); }}});
               }},
               SelectSuffix{[] {
                   Button(ButtonProps{}.size(ControlSize::Small),
                          ButtonSlots{.icon = ButtonIcon{[] { Icon(IconProps{}.name(IconName::CheckOutlined)); }}});
               }});
        Select(SelectProps{}.layout(LayoutStyle{}.width(dp(180)).height(dp(32))), SelectPrefix{[] {
                   Button(ButtonProps{}.size(ControlSize::Small),
                          ButtonSlots{.icon = ButtonIcon{[] { Icon(IconProps{}.name(IconName::SearchOutlined)); }}});
               }});
    }});
    f.synchronize();
    const auto prefix = f.buttons.mounted_buttons()[0].component;
    const auto suffix = f.buttons.mounted_buttons()[1].component;
    const auto limited_prefix = f.buttons.mounted_buttons()[2].component;
    const auto input = f.inputs.mounted_inputs().front();
    const auto selector = bounds(f, input.component);
    const auto viewport = f.inputs.layout_snapshot(input.component).viewport;
    const auto rtl_prefix = bounds(f, prefix);
    const auto rtl_suffix = bounds(f, suffix);
    check(selector.height == 34 && rtl_prefix.width == 24 && rtl_prefix.height == 24 && rtl_suffix.height == 24 &&
              viewport.x >= rtl_suffix.x + rtl_suffix.width && viewport.x + viewport.width <= rtl_prefix.x,
          "RTL Select truncated its affixes or placed an affix on the wrong side");
    check(bounds(f, limited_prefix).height == 22, "Explicit Select height no longer constrains its affixes");
    click(f, rtl_prefix);
    check(clicks == 1, "Mirrored Select prefix did not receive pointer input");

    direction.set(TextDirection::LeftToRight);
    f.synchronize();
    const auto ltr_prefix = bounds(f, prefix);
    const auto ltr_suffix = bounds(f, suffix);
    const auto ltr_viewport = f.inputs.layout_snapshot(input.component).viewport;
    check(ltr_prefix.x == selector.x + selector.width - (rtl_prefix.x - selector.x) - rtl_prefix.width &&
              ltr_prefix.x + ltr_prefix.width <= ltr_viewport.x &&
              ltr_viewport.x + ltr_viewport.width <= ltr_suffix.x &&
              f.inputs.mounted_inputs().front().editor == input.editor,
          "Changing Select direction did not mirror the existing affixes and editor");
    click(f, ltr_prefix);
    check(clicks == 2, "Retained Select prefix did not receive input after changing direction");
}

void pointer_keyboard_and_clear() {
    SelectRef reference;
    int focused = 0;
    int blurred = 0;
    int runs = 0;
    std::vector<SelectSelection> changes;
    std::vector<bool> opened;
    Fixture f;
    f.services.mount(Content{[&] {
        ++runs;
        Select(SelectProps{}
                   .options({{1, u8"One"}, {2, u8"Two", true}, {3, u8"Three"}})
                   .allowClear()
                   .ref(reference)
                   .layout(LayoutStyle{}.width(dp(220)))
                   .onChange([&](SelectSelection value) { changes.push_back(value); })
                   .onOpenChange([&](bool value) { opened.push_back(value); })
                   .onFocus([&] { ++focused; })
                   .onBlur([&] { ++blurred; }));
        Button(ButtonProps{}, ButtonContent{[] { Text(u8"Next"); }});
    }});
    f.synchronize();
    const auto id = f.services.select().mounted()[0];
    auto s = f.services.select().snapshot(id);
    const auto selector = s.selector;
    const auto rows = s.rows;
    check(reference.bound() && !s.visible && !s.value, "Select initial mount");
    click(f, bounds(f, selector));
    s = f.services.select().snapshot(id);
    check(s.visible && focused == 1 && !blurred && opened == std::vector<bool>{true},
          "selector click did not open/focus");
    click(f, bounds(f, s.rows[2]));
    s = f.services.select().snapshot(id);
    check(!s.visible && s.value == SelectSelection{3} && changes == std::vector<SelectSelection>{SelectSelection{3}} &&
              focused == 1 && !blurred,
          "option click lost focus or value");
    key(f, Key::down);
    key(f, Key::home);
    key(f, Key::down);
    check(f.services.select().snapshot(id).active == 2, "keyboard did not skip disabled");
    key(f, Key::home);
    key(f, Key::enter);
    check(f.services.select().snapshot(id).value == SelectSelection{1}, "keyboard selection");
    key(f, Key::down);
    key(f, Key::escape);
    check(!f.services.select().snapshot(id).visible && f.services.select().snapshot(id).value == SelectSelection{1},
          "Escape mutated selection");
    key(f, Key::down);
    key(f, Key::tab);
    check(!f.services.select().snapshot(id).visible && blurred == 1 &&
              f.services.focus().state().focused == f.buttons.mounted_buttons().back().interaction,
          "Select has multiple Tab stops");
    check(reference.focus(), "Select ref focus");
    f.synchronize();
    const auto clear_owner = f.services.select().paint_owners(id)->clear.owner;
    const auto clear_action = f.services.components().children(clear_owner).front();
    auto clear_hit = [&] {
        for (const auto hit : f.services.interactions().declaration_order()) {
            if (const auto* record = f.services.interactions().find(hit); record && record->component == clear_action) {
                return hit;
            }
        }
        throw std::runtime_error("Select clear action was not retained");
    }();
    auto move = [&](runtime::Rect box) {
        f.services.pointer().dispatch({PointerIdentity::mouse(), PointerAction::move, PointerButton::none,
                                       box.x + box.width / 2, box.y + box.height / 2});
        f.synchronize();
    };
    const auto input_viewport = f.inputs.layout_snapshot(selector).viewport;
    move(bounds(f, f.buttons.mounted_buttons().back().component));
    check(!f.services.interactions().require(clear_hit).eligible, "resting Select exposes a hidden clear action");
    move(bounds(f, selector));
    check(f.services.interactions().require(clear_hit).eligible &&
              f.inputs.layout_snapshot(selector).viewport == input_viewport &&
              f.services.components().children(clear_owner).front() == clear_action,
          "Select hover must replace its suffix in place while retaining the editor and clear action");
    // The only eligible non-input interaction inside the selector is clear.
    bool cleared = false;
    const auto interactions = f.services.interactions().declaration_order();
    for (auto hit : interactions) {
        const auto* record = f.services.interactions().find(hit);
        if (!record || record->component == selector || record->focusable || !record->eligible) {
            continue;
        }
        for (auto parent = f.services.components().parent(record->component); parent;
             parent = f.services.components().parent(*parent)) {
            if (*parent == selector) {
                click(f, bounds(f, record->component));
                cleared = true;
                break;
            }
        }
        if (cleared) {
            break;
        }
    }
    s = f.services.select().snapshot(id);
    check(cleared && !s.value && !changes.back() && s.selector == selector && s.rows == rows && runs == 1,
          "clear failed or ordinary updates remounted");
    const auto before = f.services.interactions().size();
    check(f.services.destroy(id), "Select destroy");
    f.synchronize();
    check(!reference.bound() && f.services.select().mounted().empty() && f.inputs.mounted_inputs().empty() &&
              f.services.interactions().size() < before && !f.services.next_frame_deadline(),
          "Select cleanup leak");
}

void search_ime_and_dynamic_options() {
    SelectRef ref;
    Signal<SelectSelection> value{SelectSelection{1}};
    Signal<std::vector<SelectOption>> options{
        std::vector<SelectOption>{{1, u8"Alpha"}, {2, u8"中文"}, {3, u8"Straße"}}};
    std::vector<SelectSelection> intents;
    std::vector<String> searches;
    Fixture f;
    f.services.mount(Content{[&] {
        Select(SelectProps{}
                   .value(value)
                   .options(options)
                   .showSearch()
                   .ref(ref)
                   .allowClear()
                   .onChange([&](SelectSelection intent) { intents.push_back(intent); })
                   .onSearch([&](String query) { searches.push_back(query); }));
    }});
    f.synchronize();
    const auto id = f.services.select().mounted()[0];
    const auto original = f.services.select().snapshot(id);
    check(ref.focus(), "search ref focus");
    key(f, Key::down);
    const auto stamp = f.inputs.sessions().active();
    check(bool(f.inputs.dispatch(CompositionChanged{String{u8"zhong"}, {}, stamp})), "preedit rejected");
    key(f, Key::down);
    key(f, Key::end);
    key(f, Key::enter);
    check(intents.empty() && searches.empty() && f.services.select().snapshot(id).active == 0,
          "Select stole IME navigation or committed preedit");
    key(f, Key::escape);
    check(f.services.select().snapshot(id).visible && !f.inputs.editors().require(stamp.owner).composition().active,
          "first Escape should cancel IME and retain menu");
    check(bool(f.inputs.dispatch(TextCommitted{String{u8"中"}, stamp})), "search commit rejected");
    f.synchronize();
    auto s = f.services.select().snapshot(id);
    check(s.query == String{u8"中"} && s.value == SelectSelection{1} && s.active == 1 && s.rows == original.rows &&
              searches == std::vector<String>{String{u8"中"}},
          "search changed selection or remounted rows");
    key(f, Key::enter);
    s = f.services.select().snapshot(id);
    check(!s.visible && s.value == SelectSelection{1} && intents == std::vector<SelectSelection>{SelectSelection{2}} &&
              f.inputs.display_snapshot(s.selector).text == "Alpha",
          "controlled rejection changed display");
    value.set(SelectSelection{42});
    f.synchronize();
    check(f.inputs.display_snapshot(s.selector).text == "42", "unknown controlled value lost");
    options.set(std::vector<SelectOption>{{42, u8"Loaded"}, {4, u8"Other"}});
    f.synchronize();
    s = f.services.select().snapshot(id);
    check(s.selector == original.selector && s.rows != original.rows &&
              f.inputs.display_snapshot(s.selector).text == "Loaded",
          "dynamic options failed to retain Input and resolve label");
    const auto stable_rows = s.rows;
    bool rejected = false;
    try {
        options.set(std::vector<SelectOption>{{5, u8"New"}, {5, u8"Duplicate"}});
    } catch (const std::invalid_argument&) {
        rejected = true;
    }
    f.synchronize();
    check(rejected && f.services.select().snapshot(id).rows == stable_rows &&
              f.inputs.display_snapshot(s.selector).text == "Loaded",
          "invalid options changed published UI");
    key(f, Key::down);
    const auto old_row = bounds(f, f.services.select().snapshot(id).rows[0]);
    const float x = old_row.x + 10;
    const float y = old_row.y + 10;
    f.services.pointer().dispatch({PointerIdentity::mouse(), PointerAction::down, PointerButton::primary, x, y});
    check(f.services.pointer().state(PointerIdentity::mouse())->capture.has_value(), "option did not capture press");
    options.set(std::vector<SelectOption>{{99, u8"Replacement"}});
    f.synchronize();
    check(!f.services.pointer().state(PointerIdentity::mouse())->capture, "replaced option retained capture");
    f.services.pointer().dispatch({PointerIdentity::mouse(), PointerAction::up, PointerButton::primary, x, y});
    check(intents.size() == 1 && f.services.select().snapshot(id).value == SelectSelection{42},
          "stale option release selected replacement");
    options.set(std::vector<SelectOption>{});
    f.synchronize();
    key(f, Key::down);
    check(f.services.select().snapshot(id).visible && !f.services.select().snapshot(id).active, "empty options menu");
    key(f, Key::enter);
    check(intents.size() == 1 && f.services.select().snapshot(id).value == SelectSelection{42},
          "empty result selected value");
    f.services.dispose();
    check(!ref.bound(), "search dispose leaked ref");
    check(!f.inputs.sessions().active().owner.valid(), "search dispose leaked session");
    check(f.services.interactions().size() == 0, "search dispose leaked interactions");
}

void controlled_close_preserves_search_until_accepted() {
    Fixture f;
    SelectRef ref;
    Signal<bool> open{true};
    bool accept_close = false;
    int requests = 0;
    int cancellations = 0;
    std::vector<String> events;
    SelectSource source{[&](AsyncOptionsRequest, auto) {
                            ++requests;
                            return [&] {
                                ++cancellations;
                            };
                        },
                        [](auto) {}};
    f.services.mount(Content{[&] {
        Select(SelectProps{}
                   .source(source)
                   .showSearch()
                   .open(open)
                   .ref(ref)
                   .onOpenChange([&](bool value) {
                       check(!value, "controlled test unexpectedly requested to open");
                       events.emplace_back(u8"close");
                       if (accept_close) {
                           open.set(false);
                       }
                   })
                   .onSearch([&](String query) { events.push_back(std::move(query)); }));
    }});
    f.synchronize();
    const auto id = f.services.select().mounted()[0];
    check(ref.focus(), "controlled searchable Select did not focus");
    f.synchronize();
    const auto session = f.inputs.sessions().active();
    check(bool(f.inputs.dispatch(TextCommitted{String{u8"query"}, session})), "Select query did not dispatch");
    f.synchronize();
    check(f.services.select().snapshot(id).pending && requests == 1, "query did not start async request");

    key(f, Key::escape);
    const auto rejected = f.services.select().snapshot(id);
    check(rejected.visible && rejected.pending && rejected.query == String{u8"query"} && cancellations == 0 &&
              events == std::vector<String>{String{u8"query"}, String{u8"close"}},
          "rejected close discarded search or cancelled active request");

    accept_close = true;
    key(f, Key::escape);
    const auto closed = f.services.select().snapshot(id);
    check(!closed.visible && !closed.pending && closed.query.empty() && requests == 1 && cancellations == 1 &&
              events == std::vector<String>{String{u8"query"}, String{u8"close"}, String{u8"close"}, String{}},
          "accepted close did not clear search after the close callback");
}

void scrolling_edges_and_idle() {
    std::vector<SelectOption> options;
    for (int i = 0; i < 100; ++i) {
        options.push_back({i, String::from_utf8(std::to_string(i)).value()});
    }
    SelectRef ref;
    Fixture f;
    f.services.mount(Content{[&] {
        Select(SelectProps{}.options(options).listHeight(64.0F).ref(ref).layout(LayoutStyle{}.width(dp(220))));
        TextArea(TextAreaProps{}.defaultValue(u8"1\n2\n3\n4\n5\n6\n7\n8\n9\n10\n11\n12").rows(5));
    }});
    f.synchronize();
    const auto id = f.services.select().mounted()[0];
    check(ref.focus(), "scroll Select focus");
    key(f, Key::down);
    key(f, Key::end);
    auto s = f.services.select().snapshot(id);
    check(s.active == 99 && s.scroll > 3000, "active row did not scroll into view");
    const auto found = std::ranges::find(s.row_indices, std::size_t{99});
    check(found != s.row_indices.end() && s.rows.size() <= 6, "Select virtual rows were not bounded");
    const auto last = bounds(f, s.rows[static_cast<std::size_t>(found - s.row_indices.begin())]);
    check(last.y >= s.popup.y && last.y + last.height <= s.popup.y + s.popup.height, "active row outside menu");
    check(f.inputs.dispatch(ScrollInputEvent{0, 1, s.popup.x + 10, s.popup.y + 10}), "menu wheel unhandled");
    f.synchronize();
    check(f.services.select().snapshot(id).scroll < s.scroll, "menu did not scroll");
    const auto area = f.inputs.mounted_inputs().back().component;
    const auto view = f.inputs.layout_snapshot(area).viewport;
    check(f.inputs.dispatch(ScrollInputEvent{0, -1, view.x + 5, view.y + view.height - 5}),
          "TextArea wheel stolen by menu");
    f.synchronize();
    check(f.inputs.layout_snapshot(area).vertical_scroll > 0, "TextArea scroll regression");
    key(f, Key::home);
    check(f.services.select().snapshot(id).scroll == 0, "Home did not reveal first row");
    // A new virtual window resolves child metrics once; settled idle must not repeat work.
    f.synchronize();
    const auto before = f.services.surfaces().diagnostics();
    const auto popup = f.services.components().children(id)[1];
    const auto popup_node = f.services.components().root(popup);
    const auto measures = f.nodes.require(popup_node).measure_count;
    f.synchronize();
    f.synchronize();
    check(f.services.surfaces().diagnostics().geometry_updates == before.geometry_updates &&
              f.services.surfaces().diagnostics().material_updates == before.material_updates,
          "visible idle Select kept updating surfaces");
    check(f.nodes.require(popup_node).measure_count == measures, "visible idle Select kept measuring");
    check(!f.services.next_frame_deadline(), "visible idle Select kept scheduling");
    f.nodes.require(f.services.components().root(id)).clip_content = true;
    check(f.services.layout_and_synchronize({120, 80}, {0, 0, 120, 80}, {0, 50}), "edge layout failed");
    s = f.services.select().snapshot(id);
    check(s.above && s.popup.x >= 0 && s.popup.y >= 0 && s.popup.width <= 120 && s.popup.y + s.popup.height <= 80,
          "menu did not flip or clamp at viewport");
    s = f.services.select().snapshot(id);
    const auto text = f.services.components().children(s.rows[0])[0];
    const auto text_clip = f.nodes.content_clip(f.services.components().root(text), {0, 0, 120, 80});
    check(text_clip.height > 0 && text_clip.y < bounds(f, id).y, "window layer inherited selector clip");
    f.services.set_window_active(false);
    check(!f.services.select().snapshot(id).visible && !f.services.next_frame_deadline(),
          "inactive Select retained menu/deadline");
    f.services.set_window_active(true);
    f.synchronize();
    check(!f.services.select().snapshot(id).visible, "reactivation reopened dismissed menu");
    f.services.dispose();
}

void controlled_states_and_theme() {
    Signal<bool> open{false};
    Signal<bool> disabled{false};
    Signal<bool> loading{false};
    Signal<ControlSize> size{ControlSize::Middle};
    Signal<InputVariant> variant{InputVariant::Outlined};
    Signal<InputStatus> status{InputStatus::Default};
    Signal<ThemeConfig> theme{ThemeConfig{}};
    int sibling_runs = 0;
    int changes = 0;
    std::vector<bool> requests;
    Fixture f;
    f.services.mount(Content{[&] {
        Theme(ThemeProps{}.config(theme), ThemeContent{[&] {
                  Select(SelectProps{}
                             .options({{1, u8"One"}, {2, u8"Two"}})
                             .defaultValue(1)
                             .allowClear()
                             .open(open)
                             .disabled(disabled)
                             .loading(loading)
                             .size(size)
                             .variant(variant)
                             .status(status)
                             .onOpenChange([&](bool value) { requests.push_back(value); })
                             .onChange([&](SelectSelection) { ++changes; }),
                         SelectPrefix{[] { Icon(IconProps{}.name(IconName::SearchOutlined)); }},
                         SelectSuffix{[] { Icon(IconProps{}.name(IconName::DownOutlined)); }});
                  ++sibling_runs;
                  Text(u8"Sibling");
              }});
    }});
    f.synchronize();
    const auto id = f.services.select().mounted()[0];
    auto s = f.services.select().snapshot(id);
    click(f, bounds(f, s.selector));
    check(!f.services.select().snapshot(id).visible && requests == std::vector<bool>{true},
          "controlled open committed implicitly");
    open.set(true);
    f.synchronize();
    check(f.services.select().snapshot(id).visible, "controlled open echo missing");
    key(f, Key::escape);
    check(f.services.select().snapshot(id).visible && requests.back() == false, "controlled open refusal ignored");
    loading.set(true);
    f.synchronize();
    key(f, Key::end);
    key(f, Key::enter);
    check(changes == 0 && f.services.select().snapshot(id).value == SelectSelection{1}, "loading allowed selection");
    loading.set(false);
    f.synchronize();
    const auto popup = f.services.components().children(id)[1];
    const auto popup_node = f.services.components().root(popup);
    const auto measure = f.nodes.require(popup_node).measure_count;
    const auto count = f.services.components().component_count();
    ThemeConfig custom;
    custom.select.tokens.option_selected_background = Color::rgba8(140, 10, 50);
    theme.set(custom);
    f.synchronize();
    check(f.nodes.require(popup_node).measure_count == measure && sibling_runs == 1 &&
              f.services.components().component_count() == count,
          "color update measured or remounted unrelated components");
    for (auto control_size : {ControlSize::Small, ControlSize::Middle, ControlSize::Large}) {
        size.set(control_size);
        for (auto input_variant :
             {InputVariant::Outlined, InputVariant::Filled, InputVariant::Borderless, InputVariant::Underlined}) {
            variant.set(input_variant);
            status.set(InputStatus::Warning);
            f.synchronize();
            check(f.inputs.size(s.selector) == control_size && f.inputs.variant(s.selector) == input_variant &&
                      f.inputs.status(s.selector) == InputStatus::Warning &&
                      f.services.select().snapshot(id).selector == s.selector,
                  "Select did not forward size/variant/status or retain Input");
        }
    }
    disabled.set(true);
    f.synchronize();
    check(!f.services.select().snapshot(id).visible && !f.services.focus().state().focused &&
              !f.inputs.sessions().active().owner.valid(),
          "disabled Select retained focus/menu/session");
    disabled.set(false);
    f.synchronize();
    check(!f.services.select().snapshot(id).visible, "enabling reopened suppressed menu");
    open.set(false);
    open.set(true);
    f.synchronize();
    click(f, bounds(f, s.selector));
    click(f, {300, 220, 10, 10});
    check(!f.services.select().snapshot(id).visible, "outside blur did not suppress rejected controlled menu");
    f.services.dispose();
}

void callback_retirement_and_validation() {
    for (int mode = 0; mode < 5; ++mode) {
        runtime::ComponentId id;
        SelectRef ref;
        Fixture f;
        f.services.mount(Content{[&] {
            auto props = SelectProps{}.options({{1, u8"One"}}).showSearch().ref(ref);
            const auto retire = [&] {
                f.services.destroy(id);
            };
            if (mode == 0) {
                props.onChange([retire](SelectSelection) { retire(); });
            }
            if (mode == 1) {
                props.onOpenChange([retire](bool) { retire(); });
            }
            if (mode == 2) {
                props.onSearch([retire](String) { retire(); });
            }
            if (mode == 3) {
                props.onFocus(retire);
            }
            if (mode == 4) {
                props.onBlur(retire);
            }
            Select(props);
        }});
        f.synchronize();
        id = f.services.select().mounted()[0];
        const bool focused = ref.focus();
        if (mode != 3) {
            check(focused, "retirement focus request rejected");
        }
        if (mode != 3) {
            key(f, Key::down);
        }
        if (mode == 0) {
            key(f, Key::enter);
        }
        if (mode == 2) {
            static_cast<void>(f.inputs.dispatch(TextCommitted{String{u8"One"}, f.inputs.sessions().active()}));
        }
        if (mode == 4) {
            static_cast<void>(ref.blur());
        }
        f.synchronize();
        check(!f.services.components().contains(id) && !ref.bound() && f.services.interactions().size() == 0 &&
                  f.services.surfaces().size() == 0 && !f.inputs.sessions().active().owner.valid(),
              "callback retirement leaked resources");
    }
    for (int mode = 0; mode < 4; ++mode) {
        Fixture f;
        bool rejected = false;
        try {
            f.services.mount(Content{[&] {
                auto props = SelectProps{};
                if (mode == 0) {
                    props.value(SelectValue{1}).defaultValue(1);
                }
                if (mode == 1) {
                    props.open(false).defaultOpen(false);
                }
                if (mode == 2) {
                    props.options({{1, u8"One"}, {1, u8"Duplicate"}});
                }
                if (mode == 3) {
                    props.listHeight(0.0F);
                }
                if (mode == 4) {
                    Select(props,
                           SelectPrefix{[] { Button(ButtonProps{}, ButtonContent{[] { Text(u8"Invalid"); }}); }});
                } else {
                    Select(props);
                }
            }});
        } catch (const std::invalid_argument&) {
            rejected = true;
        }
        check(rejected && f.services.select().mounted().empty() && f.services.interactions().size() == 0,
              "invalid Select leaked resources");
    }
}
} // namespace

int main() {
#if defined(_MSC_VER) && defined(_DEBUG)
    _CrtSetReportMode(_CRT_ASSERT, _CRTDBG_MODE_FILE);
    _CrtSetReportFile(_CRT_ASSERT, _CRTDBG_FILE_STDERR);
#endif
    try {
        pointer_keyboard_and_clear();
        direction_and_rich_affix_size();
        search_ime_and_dynamic_options();
        controlled_close_preserves_search_until_accepted();
        scrolling_edges_and_idle();
        controlled_states_and_theme();
        callback_retirement_and_validation();
    } catch (const std::exception& error) {
        std::cerr << error.what() << '\n';
        return 1;
    }
    return 0;
}
