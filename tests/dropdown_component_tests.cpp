#include "component/dropdown_component.hpp"
#include "component/menu_component.hpp"
#include "component/modal_component.hpp"
#include "component/select_component.hpp"
#include "support/input_fixture.hpp"
#include <iostream>
#include <string_view>
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

runtime::Rect bounds(Fixture& f, runtime::ComponentId id) {
    return f.nodes.require(f.services.components().root(id)).bounds;
}

void sync(Fixture& f, float width = 320, float height = 240, runtime::Point origin = {}) {
    check(f.services.layout_and_synchronize({width, height}, {0, 0, width, height}, origin), "layout failed");
}

void key(Fixture& f, Key value) {
    f.services.focus().dispatch({value, KeyAction::down});
    f.services.focus().dispatch({value, KeyAction::up});
    sync(f);
}

void click_at(Fixture& f, float x, float y, PointerButton button = PointerButton::primary) {
    f.services.pointer().dispatch({PointerIdentity::mouse(), PointerAction::down, button, x, y});
    f.services.pointer().dispatch({PointerIdentity::mouse(), PointerAction::up, button, x, y});
    sync(f);
}

void click(Fixture& f, runtime::ComponentId id, PointerButton button = PointerButton::primary) {
    const auto rect = bounds(f, id);
    click_at(f, rect.x + rect.width / 2, rect.y + rect.height / 2, button);
}

void trigger_actions_and_focus() {
    Fixture f;
    int trigger_clicks = 0;
    int actions = 0;
    std::vector<DropdownOpenInfo> requests;
    f.services.mount(Content{[&] {
        Dropdown(DropdownProps{}
                     .menu(MenuProps{}
                               .items({{u8"a", u8"Run"}, {u8"b", u8"Disabled", true}, {u8"c", u8"Delete", false, true}})
                               .onClick([&](MenuClickInfo) { ++actions; }))
                     .trigger(DropdownTriggerMode::ClickContextMenu)
                     .onOpenChange([&](DropdownOpenInfo info) { requests.push_back(info); }),
                 DropdownTrigger{
                     [&] { Button(ButtonProps{}.onClick([&] { ++trigger_clicks; }), [] { Text(u8"Actions"); }); }});
        Button(ButtonProps{}.layout(LayoutStyle{}.margin_left(dp(220))), [] { Text(u8"Next"); });
    }});
    sync(f);
    const auto id = f.services.dropdown().mounted()[0];
    auto s = f.services.dropdown().snapshot(id);
    const auto trigger = f.buttons.mounted_buttons()[0];
    check(!s.geometry.visible, "Dropdown initially visible");
    check(f.services.focus().request_focus(trigger.interaction, FocusModality::keyboard), "trigger focus failed");
    key(f, Key::enter);
    s = f.services.dropdown().snapshot(id);
    check(trigger_clicks == 1 && s.geometry.visible && requests.size() == 1 && requests[0].open,
          "Enter swallowed Button activation or duplicated open");
    check(f.services.focus().state().focused == f.services.menu().snapshot(s.menu).interaction,
          "open did not focus action menu");
    key(f, Key::down);
    check(f.services.menu().snapshot(s.menu).active_key == String{u8"c"}, "action menu did not skip disabled");
    key(f, Key::enter);
    check(actions == 1 && !f.services.dropdown().snapshot(id).geometry.visible &&
              f.services.focus().state().focused == trigger.interaction,
          "action did not close/restore trigger");
    key(f, Key::space);
    check(trigger_clicks == 2 && f.services.dropdown().snapshot(id).geometry.visible,
          "Space swallowed original trigger callback");
    key(f, Key::escape);
    check(!f.services.dropdown().snapshot(id).geometry.visible &&
              f.services.focus().state().focused == trigger.interaction,
          "Escape did not restore trigger");
    click(f, trigger.component);
    check(trigger_clicks == 3 && f.services.dropdown().snapshot(id).geometry.visible,
          "pointer trigger callback/open mismatch");
    click(f, f.buttons.mounted_buttons()[1].component);
    check(!f.services.dropdown().snapshot(id).geometry.visible &&
              f.services.focus().state().focused == f.buttons.mounted_buttons()[1].interaction,
          "outside click stole external focus");
    click(f, trigger.component, PointerButton::secondary);
    s = f.services.dropdown().snapshot(id);
    const auto trigger_rect = bounds(f, trigger.component);
    check(s.geometry.visible && s.geometry.anchor.width == 0 &&
              s.geometry.anchor.x == trigger_rect.x + trigger_rect.width / 2 && trigger_clicks == 3,
          "context trigger coordinate/open failed");
    key(f, Key::escape);
    key(f, Key::up);
    check(f.services.menu().snapshot(s.menu).active_key == String{u8"c"}, "Up trigger did not focus last action");
    const auto rows = f.services.menu().snapshot(s.menu).rows;
    key(f, Key::escape);
    key(f, Key::down);
    check(f.services.menu().snapshot(s.menu).rows == rows, "reopen remounted action rows");
    const auto settled = f.services.surfaces().diagnostics();
    sync(f);
    sync(f);
    check(f.services.surfaces().diagnostics().geometry_updates == settled.geometry_updates &&
              !f.services.next_frame_deadline(),
          "Dropdown idle updates/deadline");
    key(f, Key::tab);
    check(!f.services.dropdown().snapshot(id).geometry.visible &&
              f.services.focus().state().focused == f.buttons.mounted_buttons()[1].interaction,
          "Tab did not close and continue past the trigger");
    f.services.dispose();
    check(f.services.interactions().size() == 0 && f.services.surfaces().size() == 0, "Dropdown cleanup leak");
}

void controlled_and_clipping() {
    Fixture f;
    Signal<bool> open{false};
    Signal<bool> disabled{false};
    std::vector<DropdownOpenInfo> requests;
    f.services.mount(Content{[&] {
        Dropdown(DropdownProps{}
                     .open(open)
                     .disabled(disabled)
                     .menu(MenuProps{}.items({{u8"a", u8"Long action label"}, {u8"b", u8"Second"}}))
                     .onOpenChange([&](DropdownOpenInfo info) { requests.push_back(info); }),
                 DropdownTrigger{[] { Button(ButtonProps{}, [] { Text(u8"Open"); }); }});
    }});
    sync(f);
    const auto id = f.services.dropdown().mounted()[0];
    const auto button = f.buttons.mounted_buttons()[0];
    click(f, button.component);
    check(requests.size() == 1 && requests[0].open && !f.services.dropdown().snapshot(id).geometry.visible,
          "controlled open refusal committed");
    open.set(true);
    sync(f);
    key(f, Key::escape);
    check(f.services.dropdown().snapshot(id).geometry.visible && !requests.back().open,
          "controlled close refusal committed");
    f.nodes.require(f.services.components().root(id)).clip_content = true;
    check(f.services.layout_and_synchronize({180, 120}, {0, 90, 40, 20}, {140, 90}), "edge layout failed");
    auto s = f.services.dropdown().snapshot(id);
    check(s.geometry.visible && s.geometry.placement == DropdownPlacement::TopLeft && s.geometry.bounds.x >= 0 &&
              s.geometry.bounds.y >= 0 && s.geometry.bounds.x + s.geometry.bounds.width <= 180 &&
              s.geometry.bounds.y + s.geometry.bounds.height <= 120,
          "popup did not flip/clamp");
    const auto row = f.services.menu().snapshot(s.menu).rows[0];
    const auto text = f.services.components().children(row)[0];
    const auto clip = f.nodes.content_clip(f.services.components().root(text), {0, 0, 180, 120});
    check(clip.width > 40 && clip.height > 0 && clip.y < bounds(f, id).y, "popup text inherited trigger/document clip");
    disabled.set(true);
    sync(f);
    check(!f.services.dropdown().snapshot(id).geometry.visible, "disabled menu stayed interactive");
    disabled.set(false);
    sync(f);
    check(!f.services.dropdown().snapshot(id).geometry.visible, "enable revived suppressed controlled popup");
    open.set(false);
    open.set(true);
    sync(f);
    check(f.services.dropdown().snapshot(id).geometry.visible, "new external open did not clear suppression");
    f.services.set_window_active(false);
    f.services.set_window_active(true);
    sync(f);
    check(!f.services.dropdown().snapshot(id).geometry.visible, "window reactivation revived dismissed popup");
}

void modal_initial_and_nested() {
    Fixture f;
    Signal<bool> modal_open{false};
    Signal<bool> dropdown_open{true};
    f.services.mount(Content{[&] {
        Modal(ModalProps{}.open(modal_open).width(280).footer(ModalFooterMode::None), ModalContent{[&] {
                  Dropdown(DropdownProps{}.open(dropdown_open).menu(MenuProps{}.items({{u8"a", u8"Modal action"}})),
                           DropdownTrigger{[] { Button(ButtonProps{}, [] { Text(u8"Inside Modal"); }); }});
              }});
    }});
    sync(f);
    const auto id = f.services.dropdown().mounted()[0];
    check(!f.services.dropdown().snapshot(id).geometry.visible, "hidden Modal popup visible");
    modal_open.set(true);
    sync(f);
    check(f.services.dropdown().snapshot(id).geometry.visible, "initial hidden Modal popup never refreshed");
    modal_open.set(false);
    sync(f);
    modal_open.set(true);
    sync(f);
    check(!f.services.dropdown().snapshot(id).geometry.visible, "dismissed Modal popup revived");
    dropdown_open.set(false);
    dropdown_open.set(true);
    sync(f);
    check(f.services.dropdown().snapshot(id).geometry.visible, "Modal popup explicit reopen failed");

    Fixture nested;
    int nested_clicks = 0;
    nested.services.mount(Content{[&] {
        MenuItem host{u8"host", u8"Parent action"};
        host.icon = MenuIcon{[&] {
            Dropdown(DropdownProps{}.defaultOpen(true).menu(MenuProps{}.items({{u8"child", u8"Child action"}})),
                     DropdownTrigger{
                         [&] { Button(ButtonProps{}.onClick([&] { ++nested_clicks; }), [] { Text(u8"Child"); }); }});
        }};
        Dropdown(DropdownProps{}.defaultOpen(true).menu(MenuProps{}.items({host})),
                 DropdownTrigger{[] { Button(ButtonProps{}, [] { Text(u8"Parent"); }); }});
    }});
    sync(nested);
    const auto child = nested.services.dropdown().mounted()[0];
    const auto parent = nested.services.dropdown().mounted()[1];
    auto c = nested.services.dropdown().snapshot(child);
    const auto p = nested.services.dropdown().snapshot(parent);
    check(c.geometry.visible && p.geometry.visible && c.geometry.anchor == bounds(nested, child),
          "nested popup owner positioning failed");
    key(nested, Key::escape);
    check(!nested.services.dropdown().snapshot(child).geometry.visible &&
              nested.services.dropdown().snapshot(parent).geometry.visible,
          "Escape closed parent before child");
    const auto child_button = nested.buttons.mounted_buttons()[1];
    click(nested, child_button.component);
    check(nested_clicks == 1 && nested.services.dropdown().snapshot(child).geometry.visible &&
              nested.services.dropdown().snapshot(parent).geometry.visible,
          "child trigger was treated as outside parent");
    c = nested.services.dropdown().snapshot(child);
    click(nested, nested.services.menu().snapshot(c.menu).rows[0]);
    check(!nested.services.dropdown().snapshot(child).geometry.visible &&
              nested.services.dropdown().snapshot(parent).geometry.visible,
          "child command closed wrong popup");
}

void retirement_and_validation() {
    for (int mode = 0; mode < 2; ++mode) {
        Fixture f;
        runtime::ComponentId id;
        f.services.mount(Content{[&] {
            auto props = DropdownProps{}.menu(MenuProps{}.items({{u8"a", u8"Action"}}).onClick([&](MenuClickInfo) {
                if (mode == 0) {
                    f.services.destroy(id);
                }
            }));
            if (mode == 1) {
                props.onOpenChange([&](DropdownOpenInfo) { f.services.destroy(id); });
            }
            Dropdown(props, DropdownTrigger{[] { Button(ButtonProps{}, [] { Text(u8"Open"); }); }});
        }});
        sync(f);
        id = f.services.dropdown().mounted()[0];
        click(f, f.buttons.mounted_buttons()[0].component);
        if (mode == 0) {
            click(f, f.services.menu().snapshot(f.services.dropdown().snapshot(id).menu).rows[0]);
        }
        check(!f.services.components().contains(id) && f.services.interactions().size() == 0 &&
                  f.services.surfaces().size() == 0,
              "Dropdown callback retirement leaked resources");
    }
    Fixture bad;
    bool rejected = false;
    try {
        bad.services.mount(Content{[] {
            Dropdown(DropdownProps{}.open(false).defaultOpen(false), DropdownTrigger{[] { Text(u8"Invalid"); }});
        }});
    } catch (const std::invalid_argument&) {
        rejected = true;
    }
    check(rejected && bad.services.interactions().size() == 0, "invalid Dropdown leaked resources");
}

void nested_select() {
    Fixture f;
    f.services.mount(Content{[&] {
        MenuItem item{u8"host", u8"Select host"};
        item.icon = MenuIcon{
            [] { Select(SelectProps{}.options({{1, u8"One"}, {2, u8"Two"}}).layout(LayoutStyle{}.width(dp(120)))); }};
        Dropdown(DropdownProps{}.defaultOpen(true).menu(MenuProps{}.items({item})),
                 DropdownTrigger{[] { Button(ButtonProps{}, [] { Text(u8"Actions"); }); }});
    }});
    sync(f);
    const auto dropdown = f.services.dropdown().mounted()[0];
    const auto select = f.services.select().mounted()[0];
    click(f, f.services.select().snapshot(select).selector);
    check(f.services.select().snapshot(select).visible && f.services.dropdown().snapshot(dropdown).geometry.visible,
          "nested Select trigger closed Dropdown");
    key(f, Key::escape);
    check(!f.services.select().snapshot(select).visible && f.services.dropdown().snapshot(dropdown).geometry.visible,
          "nested Select Escape closed Dropdown");
    key(f, Key::down);
    auto s = f.services.select().snapshot(select);
    click(f, s.rows[1]);
    check(f.services.select().snapshot(select).value == SelectSelection{2} &&
              f.services.dropdown().snapshot(dropdown).geometry.visible,
          "nested Select action treated as outside Dropdown");
}

void menu_in_scrolling_modal() {
    Fixture f;
    int clicks = 0;
    const auto background = Color::rgba8(12, 34, 56);
    ThemeConfig config;
    config.menu.tokens.background = background;
    f.services.mount(Content{[&] {
        Theme(ThemeProps{}.config(config), ThemeContent{[&] {
                  Modal(ModalProps{}.open(true).width(300).footer(ModalFooterMode::None), ModalContent{[&] {
                            Menu(MenuProps{}
                                     .items({{u8"a", u8"A"},
                                             {u8"b", u8"B"},
                                             {u8"c", u8"C"},
                                             {u8"d", u8"D"},
                                             {u8"e", u8"E"},
                                             {u8"f", u8"F"},
                                             {u8"g", u8"G"},
                                             {u8"h", u8"H"}})
                                     .onClick([&](MenuClickInfo) { ++clicks; }));
                        }});
              }});
    }});
    sync(f, 400, 300);
    const auto modal = f.services.modal().mounted()[0];
    const auto menu = f.services.menu().mounted()[0];
    auto shown = f.services.modal().snapshot(modal);
    check(shown.maximum_scroll > 0, "Menu Modal scroll fixture not bounded");
    check(f.services.dispatch_scroll({0, -1, shown.body_bounds.x + 20, shown.body_bounds.y + 20}),
          "Menu swallowed ancestor scrolling");
    sync(f, 400, 300);
    shown = f.services.modal().snapshot(modal);
    check(shown.scroll > 0, "unconstrained Menu captured ancestor wheel");
    bool found = false;
    for (const auto& q : f.services.surfaces().instances().instances()) {
        if (q.opacity > 0 &&
            q.color == std::array{background.red(), background.green(), background.blue(), background.alpha()}) {
            found = true;
            check(q.bounds[1] >= shown.body_bounds.y &&
                      q.bounds[1] + q.bounds[3] <= shown.body_bounds.y + shown.body_bounds.height,
                  "translated Menu background escaped Modal body clip");
        }
    }
    check(found, "Menu background was not realized");
    for (auto row : f.services.menu().snapshot(menu).rows) {
        const auto& node = f.nodes.require(f.services.components().root(row));
        const float y = node.bounds.y + node.translation.y + node.bounds.height / 2;
        if (y > shown.body_bounds.y && y < shown.body_bounds.y + shown.body_bounds.height) {
            click_at(f, node.bounds.x + node.translation.x + node.bounds.width / 2, y);
            break;
        }
    }
    check(clicks == 1, "translated Menu row pointer activation failed");
}

void native_trigger_gates() {
    Fixture f;
    Signal<bool> disabled{true};
    Signal<bool> loading{false};
    int clicks = 0;
    f.services.mount(Content{[&] {
        Dropdown(DropdownProps{}
                     .trigger(DropdownTriggerMode::ClickContextMenu)
                     .menu(MenuProps{}.items({{u8"a", u8"Action"}})),
                 DropdownTrigger{[&] {
                     Button(ButtonProps{}.disabled(disabled).loading(loading).onClick([&] { ++clicks; }),
                            [] { Text(u8"Native gate"); });
                 }});
    }});
    sync(f);
    const auto id = f.services.dropdown().mounted()[0];
    const auto button = f.buttons.mounted_buttons()[0];
    click(f, button.component);
    click(f, button.component, PointerButton::secondary);
    check(clicks == 0 && !f.services.dropdown().snapshot(id).geometry.visible,
          "disabled Button wrapper opened Dropdown");
    disabled.set(false);
    loading.set(true);
    sync(f);
    click(f, button.component);
    check(f.services.focus().request_focus(button.interaction, FocusModality::keyboard),
          "loading trigger focus failed");
    key(f, Key::enter);
    key(f, Key::space);
    check(clicks == 0 && !f.services.dropdown().snapshot(id).geometry.visible, "loading Button gate opened Dropdown");
    key(f, Key::down);
    key(f, Key::up);
    check(!f.services.dropdown().snapshot(id).geometry.visible, "loading Button arrow bypassed activation gate");
    loading.set(false);
    sync(f);
    key(f, Key::enter);
    check(clicks == 1 && f.services.dropdown().snapshot(id).geometry.visible,
          "enabled Button did not recover trigger activation");
}

bool descendant(Fixture& f, runtime::ComponentId id, runtime::ComponentId ancestor) {
    for (auto current = std::optional{id}; current; current = f.services.components().parent(*current)) {
        if (*current == ancestor) {
            return true;
        }
    }
    return false;
}

void overlapping_popup_wheel(bool use_select) {
    Fixture f;
    ThemeConfig config;
    config.dropdown.tokens.max_height = dp(160);
    f.services.mount(Content{[&] {
        Theme(ThemeProps{}.config(config), ThemeContent{[&] {
                  MenuItem host{u8"host", u8"Host"};
                  host.icon = MenuIcon{[&] {
                      if (use_select) {
                          Select(SelectProps{}
                                     .options({{1, u8"One"},
                                               {2, u8"Two"},
                                               {3, u8"Three"},
                                               {4, u8"Four"},
                                               {5, u8"Five"},
                                               {6, u8"Six"},
                                               {7, u8"Seven"},
                                               {8, u8"Eight"}})
                                     .listHeight(96)
                                     .layout(LayoutStyle{}.width(dp(120))));
                      } else {
                          Dropdown(DropdownProps{}.defaultOpen(true).menu(MenuProps{}.items({{u8"a", u8"A"},
                                                                                             {u8"b", u8"B"},
                                                                                             {u8"c", u8"C"},
                                                                                             {u8"d", u8"D"},
                                                                                             {u8"e", u8"E"},
                                                                                             {u8"f", u8"F"},
                                                                                             {u8"g", u8"G"},
                                                                                             {u8"h", u8"H"}})),
                                   DropdownTrigger{[] { Button(ButtonProps{}, [] { Text(u8"Child"); }); }});
                      }
                  }};
                  Dropdown(DropdownProps{}.defaultOpen(true).menu(MenuProps{}.items({host,
                                                                                     {u8"b", u8"B"},
                                                                                     {u8"c", u8"C"},
                                                                                     {u8"d", u8"D"},
                                                                                     {u8"e", u8"E"},
                                                                                     {u8"f", u8"F"},
                                                                                     {u8"g", u8"G"},
                                                                                     {u8"h", u8"H"}})),
                           DropdownTrigger{[] { Button(ButtonProps{}, [] { Text(u8"Parent"); }); }});
              }});
    }});
    sync(f);
    const auto parent = f.services.dropdown().mounted().back();
    const auto parent_menu = f.services.dropdown().snapshot(parent).menu;
    runtime::Rect child_bounds;
    runtime::ComponentId child;
    runtime::ComponentId child_menu;
    if (use_select) {
        child = f.services.select().mounted()[0];
        click(f, f.services.select().snapshot(child).selector);
        check(f.services.select().snapshot(child).visible, "overlapping Select did not open");
        child_bounds = f.services.select().snapshot(child).popup;
    } else {
        child = f.services.dropdown().mounted()[0];
        const auto state = f.services.dropdown().snapshot(child);
        check(state.geometry.visible, "overlapping child Dropdown did not open");
        child_menu = state.menu;
        child_bounds = state.geometry.bounds;
    }
    const auto parent_bounds = bounds(f, parent_menu);
    const float left = std::max(parent_bounds.x, child_bounds.x);
    const float top = std::max(parent_bounds.y, child_bounds.y);
    const float right = std::min(parent_bounds.x + parent_bounds.width, child_bounds.x + child_bounds.width);
    const float bottom = std::min(parent_bounds.y + parent_bounds.height, child_bounds.y + child_bounds.height);
    check(right - left > 16 && bottom - top > 16, "wheel fixture popups do not overlap");
    const float x = left + 12;
    const float y = top + 12;
    const auto hit = f.services.hit_test().hit_test({x, y});
    check(hit && descendant(f, f.services.interactions().find(*hit)->component, child),
          "overlap does not hit topmost child popup");
    const auto parent_rows = f.services.menu().snapshot(parent_menu).rows;
    const auto parent_scroll = f.services.menu().snapshot(parent_menu).scroll;
    check(f.services.dispatch_scroll({0, -1, x, y}), "child wheel was not handled");
    sync(f);
    const float child_scroll =
        use_select ? f.services.select().snapshot(child).scroll : f.services.menu().snapshot(child_menu).scroll;
    check(child_scroll > 0 && f.services.menu().snapshot(parent_menu).scroll == parent_scroll,
          use_select ? "parent Menu stole overlapping Select wheel" : "parent Menu stole nested Dropdown wheel");
    check(f.services.menu().snapshot(parent_menu).rows == parent_rows, "child wheel remounted parent rows");
    // A child at its scroll boundary must not leak wheel input to the parent.
    static_cast<void>(f.services.dispatch_scroll({0, -1000, x, y}));
    sync(f);
    const float at_end =
        use_select ? f.services.select().snapshot(child).scroll : f.services.menu().snapshot(child_menu).scroll;
    static_cast<void>(f.services.dispatch_scroll({0, -1, x, y}));
    sync(f);
    check((use_select ? f.services.select().snapshot(child).scroll : f.services.menu().snapshot(child_menu).scroll) ==
                  at_end &&
              f.services.menu().snapshot(parent_menu).scroll == parent_scroll,
          "child popup boundary leaked wheel input to parent Menu");
    // The parent still owns the exposed strip to the left of the child popup.
    const float parent_x = parent_bounds.x + 1;
    check(parent_x < child_bounds.x, "parent wheel fixture lacks exposed area");
    check(f.services.dispatch_scroll({0, -1, parent_x, parent_bounds.y + 12}), "exposed parent wheel not handled");
    sync(f);
    check(f.services.menu().snapshot(parent_menu).scroll > parent_scroll, "exposed parent Menu did not scroll");
    f.services.dispose();
    check(f.services.interactions().size() == 0 && f.services.surfaces().size() == 0, "wheel fixture cleanup leaked");
}

void overlapping_select_wheel() {
    overlapping_popup_wheel(true);
}

void overlapping_dropdown_wheel() {
    overlapping_popup_wheel(false);
}

void short_label_full_width() {
    Fixture f;
    int clicks = 0;
    f.services.mount(Content{[&] {
        Dropdown(
            DropdownProps{}.defaultOpen(true).menu(MenuProps{}.items({{u8"a", u8"A"}}).onClick([&](MenuClickInfo info) {
                check(info.key == String{u8"a"}, "right-side activation used wrong key");
                ++clicks;
            })),
            DropdownTrigger{[] { Button(ButtonProps{}, [] { Text(u8"Open"); }); }});
    }});
    sync(f);
    const auto id = f.services.dropdown().mounted()[0];
    const auto state = f.services.dropdown().snapshot(id);
    const auto row = f.services.menu().snapshot(state.menu).rows[0];
    const auto& theme = f.services.components().theme_scope(id)->snapshot();
    const float padding = theme.dropdown().metrics.padding;
    const float margin = theme.menu().metrics.item_margin;
    const auto menu_bounds = bounds(f, state.menu);
    const auto row_bounds = bounds(f, row);
    check(state.geometry.bounds.width == theme.dropdown().metrics.min_width, "short label fixture lost min width");
    check(menu_bounds.width == state.geometry.bounds.width - padding * 2 &&
              row_bounds.x == state.geometry.bounds.x + padding + margin &&
              row_bounds.width == state.geometry.bounds.width - (padding + margin) * 2,
          "short label row did not fill popup inner width");
    click_at(f, state.geometry.bounds.x + state.geometry.bounds.width - padding - margin - 2,
             row_bounds.y + row_bounds.height / 2);
    check(clicks == 1 && !f.services.dropdown().snapshot(id).geometry.visible,
          "right side of short-label row did not activate");
}
} // namespace

int main(int argc, char** argv) {
#if defined(_MSC_VER) && defined(_DEBUG)
    _CrtSetReportMode(_CRT_ASSERT, _CRTDBG_MODE_FILE);
    _CrtSetReportFile(_CRT_ASSERT, _CRTDBG_FILE_STDERR);
#endif
    try {
        int ran = 0;
        const auto run = [&](std::string_view name, void (*test)()) {
            if (argc == 1 || name == argv[1]) {
                test();
                ++ran;
            }
        };
        run("trigger", trigger_actions_and_focus);
        run("controlled", controlled_and_clipping);
        run("modal-nested", modal_initial_and_nested);
        run("retirement", retirement_and_validation);
        run("select", nested_select);
        run("scroll-modal", menu_in_scrolling_modal);
        run("trigger-gates", native_trigger_gates);
        run("wheel-select", overlapping_select_wheel);
        run("wheel-dropdown", overlapping_dropdown_wheel);
        run("short-label-width", short_label_full_width);
        check(ran > 0, "unknown Dropdown test case");
    } catch (const std::exception& error) {
        std::cerr << error.what() << '\n';
        return 1;
    }
}
