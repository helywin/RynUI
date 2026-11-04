#include "component/menu_component.hpp"
#include "support/input_fixture.hpp"
#include "support/bundled_icon_checks.hpp"
#include <iostream>
#include <limits>
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

void key(Fixture& f, Key key) {
    f.services.focus().dispatch({key, KeyAction::down});
    f.services.focus().dispatch({key, KeyAction::up});
    f.synchronize();
}

void click(Fixture& f, runtime::ComponentId id) {
    const auto rect = f.nodes.require(f.services.components().root(id)).bounds;
    const float x = rect.x + rect.width / 2;
    const float y = rect.y + rect.height / 2;
    f.services.pointer().dispatch({PointerIdentity::mouse(), PointerAction::down, PointerButton::primary, x, y});
    f.services.pointer().dispatch({PointerIdentity::mouse(), PointerAction::up, PointerButton::primary, x, y});
    f.synchronize();
}

void navigation() {
    Fixture f;
    Signal<String> label{String{u8"First"}};
    Signal<bool> disabled{false};
    Signal<MenuKeys> selected{MenuKeys{String{u8"a"}}};
    Signal<MenuKeys> open{MenuKeys{}};
    std::vector<MenuSelectInfo> requests;
    std::vector<MenuKeys> opens;
    std::vector<MenuClickInfo> clicks;
    int slots = 0;
    f.services.mount(Content{[&] {
        Menu(MenuProps{}
                 .mode(MenuMode::Inline)
                 .items({MenuItem{u8"group",
                                  u8"Navigation",
                                  false,
                                  false,
                                  MenuItemKind::Group,
                                  {{u8"a", label, disabled, false, MenuItemKind::Item, {}, MenuIcon{[&] {
                                        ++slots;
                                        Text(u8"*");
                                    }}}}},
                         MenuItem{u8"disabled", u8"Disabled", true},
                         MenuItem{u8"divider", String{}, false, false, MenuItemKind::Divider},
                         MenuItem{u8"sub", u8"Tools", false, false, MenuItemKind::SubMenu, {{u8"child", u8"Child"}}},
                         MenuItem{u8"z", u8"Last"}})
                 .selectedKeys(selected)
                 .openKeys(open)
                 .onSelect([&](MenuSelectInfo info) { requests.push_back(std::move(info)); })
                 .onOpenChange([&](MenuKeys keys) { opens.push_back(std::move(keys)); })
                 .onClick([&](MenuClickInfo info) { clicks.push_back(std::move(info)); })
                 .layout(LayoutStyle{}.width(dp(220)).height(dp(150))));
        Button(ButtonProps{}, [] { Text(u8"After"); });
    }});
    f.synchronize();
    const auto id = f.services.menu().mounted()[0];
    auto s = f.services.menu().snapshot(id);
    const auto rows = s.rows;
    const auto arrow = ryn_test::require_bundled_icon(f.services, rows[4], IconName::RightOutlined);
    check(s.active_key == String{u8"a"}, "first enabled row missing");
    f.services.menu().focus_menu(id);
    key(f, Key::down);
    check(f.services.menu().snapshot(id).active_key == String{u8"sub"}, "did not skip disabled/group/divider");
    key(f, Key::right);
    check(opens.size() == 1 && f.services.menu().snapshot(id).open_keys.empty(),
          "controlled open refusal mutated state");
    check(ryn_test::require_bundled_icon(f.services, rows[4], IconName::RightOutlined) == arrow,
          "rejected Menu expansion changed the retained icon");
    open.set(MenuKeys{String{u8"sub"}});
    f.synchronize();
    check(ryn_test::require_bundled_icon(f.services, rows[4], IconName::DownOutlined) == arrow,
          "Menu expansion did not update the retained bundled icon");
    key(f, Key::right);
    check(f.services.menu().snapshot(id).active_key == String{u8"child"}, "Right did not enter submenu");
    key(f, Key::enter);
    s = f.services.menu().snapshot(id);
    check(s.selected_keys == MenuKeys{String{u8"a"}} && requests.size() == 1 && requests[0].key == String{u8"child"} &&
              clicks.size() == 1 && clicks[0].key_path == MenuKeys{String{u8"child"}, String{u8"sub"}},
          "controlled selection/path failed");
    selected.set(requests[0].selected_keys);
    label.set(String{u8"Renamed"});
    disabled.set(true);
    f.synchronize();
    key(f, Key::left);
    check(f.services.menu().snapshot(id).active_key == String{u8"sub"}, "Left did not return to parent");
    key(f, Key::left);
    check(opens.size() == 2 && f.services.menu().snapshot(id).open_keys.size() == 1, "collapse refusal changed state");
    key(f, Key::end);
    s = f.services.menu().snapshot(id);
    check(s.active_key == String{u8"z"} && s.scroll > 0 && rows == s.rows && slots == 1, "End/reveal/retention failed");
    key(f, Key::home);
    check(f.services.menu().snapshot(id).active_key == String{u8"sub"}, "Home did not skip disabled first");
    key(f, Key::tab);
    check(f.services.focus().state().focused == f.buttons.mounted_buttons().back().interaction,
          "Menu has extra Tab stops");
    click(f, rows[2]);
    check(clicks.size() == 1, "disabled row activated");
    check(f.services.destroy(id), "Menu destroy failed");
    f.synchronize();
    check(f.services.menu().mounted().empty() && !f.services.next_frame_deadline(), "Menu cleanup/deadline failed");
    f.services.dispose();
    check(f.services.interactions().size() == 0 && f.services.surfaces().size() == 0, "Menu resources leaked");
}

void uncontrolled_theme_validation() {
    Fixture f;
    Signal<ThemeConfig> theme{ThemeConfig{}};
    int clicks = 0;
    f.services.mount(Content{[&] {
        Theme(ThemeProps{}.config(theme), ThemeContent{[&] {
                  Menu(MenuProps{}.multiple().items({{u8"a", u8"A"}, {u8"b", u8"B"}}).onClick([&](MenuClickInfo) {
                      ++clicks;
                  }));
              }});
    }});
    f.synchronize();
    const auto id = f.services.menu().mounted()[0];
    f.services.menu().focus_menu(id);
    key(f, Key::enter);
    key(f, Key::down);
    key(f, Key::space);
    check(f.services.menu().snapshot(id).selected_keys.size() == 2 && clicks == 2, "multiple selection failed");
    key(f, Key::space);
    check(f.services.menu().snapshot(id).selected_keys == MenuKeys{String{u8"a"}}, "multiple deselection failed");
    const auto node = f.services.components().root(id);
    const auto measures = f.nodes.require(node).measure_count;
    const auto rows = f.services.menu().snapshot(id).rows;
    ThemeConfig custom;
    custom.menu.tokens.selected_background = Color::rgba8(100, 10, 90);
    theme.set(custom);
    f.synchronize();
    check(f.nodes.require(node).measure_count == measures && f.services.menu().snapshot(id).rows == rows,
          "color token measured/remounted");
    const auto settled = f.services.surfaces().diagnostics();
    f.synchronize();
    check(f.services.surfaces().diagnostics().geometry_updates == settled.geometry_updates &&
              !f.services.next_frame_deadline(),
          "Menu idle updated geometry");
    for (auto algorithm : {ThemeAlgorithm::Default, ThemeAlgorithm::Dark, ThemeAlgorithm::Compact}) {
        custom.algorithms = {algorithm};
        theme.set(custom);
        f.synchronize();
    }
    const auto base = resolve_theme();
    custom = {};
    custom.menu.tokens.item_height = dp(48);
    const auto changed = resolve_theme(custom);
    check(changed.menu().metrics.item_height == 48 && changed.identity() != base.identity() &&
              changed.diagnostic_json().find("\"menu\":") != std::string::npos,
          "Menu Theme not represented");
    Fixture palette;
    Signal<ThemeConfig> palette_config{ThemeConfig{}};
    Signal<MenuTheme> palette_mode{MenuTheme::Dark};
    palette.services.mount(Content{[&] {
        Theme(ThemeProps{}.config(palette_config),
              ThemeContent{[&] { Menu(MenuProps{}.theme(palette_mode).items({{u8"a", u8"A"}})); }});
    }});
    palette.synchronize();
    const auto palette_id = palette.services.menu().mounted()[0];
    const auto palette_rows = palette.services.menu().snapshot(palette_id).rows;
    ThemeConfig branded;
    branded.seed.color_primary = Color::rgba8(220, 100, 25);
    branded.menu.tokens.text = Color::rgba8(170, 35, 100);
    palette_config.set(branded);
    palette.synchronize();
    check(palette.services.menu().snapshot(palette_id).colors.text == *branded.menu.tokens.text &&
              palette.services.menu().snapshot(palette_id).colors.selected ==
                  resolve_theme(branded).menu().dark_colors.selected,
          "explicit Dark Menu ignored scoped seed/component color tokens");
    palette_config.set(ThemeConfig{});
    palette.synchronize();
    const auto normal_background = resolve_theme().menu().colors.selected_background;
    branded = {};
    branded.menu.tokens.selected_background = normal_background;
    palette_config.set(branded);
    palette.synchronize();
    check(palette.services.menu().snapshot(palette_id).colors.selected_background == normal_background,
          "variant-only token change did not invalidate Dark Menu colors");
    palette_mode.set(MenuTheme::Light);
    palette.synchronize();
    check(palette.services.menu().snapshot(palette_id).colors == resolve_theme(branded).menu().light_colors &&
              palette.services.menu().snapshot(palette_id).rows == palette_rows,
          "reactive Light palette changed retained Menu topology or ignored overrides");
    palette.services.dispose();
    for (int invalid = 0; invalid < 5; ++invalid) {
        Fixture bad;
        bool rejected = false;
        try {
            bad.services.mount(Content{[&] {
                auto props = MenuProps{};
                if (invalid == 0) {
                    props.items({{u8"same", u8"A"}, {u8"same", u8"B"}});
                }
                if (invalid == 1) {
                    props.items({{String{}, u8"Empty"}});
                }
                if (invalid == 2) {
                    props.selectedKeys(MenuKeys{}).defaultSelectedKeys({});
                }
                if (invalid == 3) {
                    props.mode(static_cast<MenuMode>(99));
                }
                if (invalid == 4) {
                    ThemeConfig config;
                    config.dropdown.tokens.z_index_popup = std::numeric_limits<std::int32_t>::max();
                    Theme(ThemeProps{}.config(config),
                          ThemeContent{[] { Menu(MenuProps{}.mode(MenuMode::Horizontal)); }});
                } else {
                    Menu(props);
                }
            }});
        } catch (const std::invalid_argument&) {
            rejected = true;
        }
        check(rejected && bad.services.interactions().size() == 0, "invalid Menu published resources");
    }
}

void callback_retirement() {
    Fixture f;
    runtime::ComponentId id;
    f.services.mount(Content{
        [&] { Menu(MenuProps{}.items({{u8"a", u8"A"}}).onSelect([&](MenuSelectInfo) { f.services.destroy(id); })); }});
    f.synchronize();
    id = f.services.menu().mounted()[0];
    f.services.menu().focus_menu(id);
    key(f, Key::enter);
    check(!f.services.components().contains(id) && f.services.surfaces().size() == 0 &&
              f.services.interactions().size() == 0,
          "callback destroy leaked");
    Fixture throwing;
    throwing.services.mount(Content{[&] {
        Menu(MenuProps{}
                 .items({{u8"sub", u8"Sub", false, false, MenuItemKind::SubMenu, {{u8"child", u8"Child"}}}})
                 .onOpenChange([](MenuKeys) { throw std::runtime_error("open callback"); }));
    }});
    throwing.synchronize();
    const auto owner = throwing.services.menu().mounted()[0];
    throwing.services.menu().focus_menu(owner);
    bool caught = false;
    try {
        key(throwing, Key::right);
    } catch (const std::runtime_error&) {
        caught = true;
    }
    throwing.synchronize();
    check(caught && !throwing.services.next_frame_deadline(), "throwing open callback left delayed work");
    throwing.services.destroy(owner);
    check(throwing.services.interactions().size() == 0 && throwing.services.surfaces().size() == 0,
          "throwing open callback leaked resources");
}

void reactive_active_reveal() {
    Fixture f;
    Signal<bool> last_disabled{false};
    std::vector<MenuClickInfo> clicks;
    f.services.mount(Content{[&] {
        Menu(MenuProps{}
                 .items({{u8"a", u8"First"},
                         {u8"b", u8"Second"},
                         {u8"c", u8"Third"},
                         {u8"d", u8"Fourth"},
                         {u8"e", u8"Last", last_disabled}})
                 .onClick([&](MenuClickInfo info) { clicks.push_back(std::move(info)); })
                 .layout(LayoutStyle{}.width(dp(200)).height(dp(96))));
    }});
    f.synchronize();
    const auto id = f.services.menu().mounted()[0];
    const auto rows = f.services.menu().snapshot(id).rows;
    f.services.menu().focus_menu(id);
    key(f, Key::end);
    check(f.services.menu().snapshot(id).active_key == String{u8"e"} && f.services.menu().snapshot(id).scroll > 0,
          "reactive reveal fixture did not reach last row");
    last_disabled.set(true);
    f.synchronize();
    const auto state = f.services.menu().snapshot(id);
    check(state.active_key == String{u8"a"} && state.rows == rows, "active reconciliation changed row identity");
    const auto viewport = f.nodes.require(f.services.components().root(id)).bounds;
    const auto first = f.nodes.require(f.services.components().root(rows[0])).bounds;
    check(first.y >= viewport.y && first.y + first.height <= viewport.y + viewport.height,
          "reconciled active row stayed outside Menu viewport");
    key(f, Key::enter);
    check(clicks.size() == 1 && clicks[0].key == String{u8"a"} &&
              f.services.menu().snapshot(id).selected_keys == MenuKeys{String{u8"a"}},
          "Enter did not activate revealed reconciled row");
}

void dynamic_topology() {
    Fixture f;
    Signal<int> version{0};
    Signal<bool> collapsed_state{false};
    Signal<String> first{String{u8"First"}};
    int slots = 0;
    f.services.mount(Content{[&] {
        Menu(MenuProps{}
                 .mode(MenuMode::Inline)
                 .inlineCollapsed(collapsed_state)
                 .defaultOpenKeys({u8"sub"})
                 .items(bind([&] {
                     const auto v = version.get();
                     MenuItem a{u8"a", first};
                     a.content = MenuLabel{[&] {
                         ++slots;
                         Input(InputProps{}.defaultValue(u8"draft"));
                     }};
                     MenuItem sub{u8"sub",
                                  u8"Sub",
                                  false,
                                  false,
                                  MenuItemKind::SubMenu,
                                  (v == 2 || v == 3) ? std::vector<MenuItem>{}
                                                     : std::vector<MenuItem>{{u8"leaf", u8"Leaf"}}};
                     if (v == 3) {
                         return std::vector<MenuItem>{sub, a, {u8"leaf", u8"Root leaf"}, {u8"new", u8"New"}};
                     }
                     return v == 0 ? std::vector<MenuItem>{a, sub} : std::vector<MenuItem>{sub, a, {u8"new", u8"New"}};
                 }))
                 .layout(LayoutStyle{}.width(dp(240)).height(dp(180))));
    }});
    f.synchronize();
    const auto id = f.services.menu().mounted()[0];
    const auto before = f.services.menu().snapshot(id).rows;
    const auto input = f.inputs.mounted_inputs().front().component;
    const auto editor = f.inputs.mounted_inputs().front();
    check(f.services.focus().request_focus(editor.interaction, FocusModality::keyboard), "rich label focus failed");
    f.synchronize();
    collapsed_state.set(true);
    f.synchronize();
    check(!f.services.components().branch_active(input) && !f.inputs.sessions().active().valid() && slots == 1,
          "collapsed rich label kept active input or remounted content");
    collapsed_state.set(false);
    f.synchronize();
    check(f.services.components().branch_active(input) && f.inputs.mounted_inputs()[0].component == input && slots == 1,
          "expanded rich label lost retained input identity");
    version.set(1);
    f.synchronize();
    const auto after = f.services.menu().snapshot(id).rows;
    check(after[0] == before[1] && after[1] == before[2] && after[2] == before[0] && slots == 1 &&
              f.services.components().contains(input),
          "keyed reorder remounted retained rich content");
    f.services.menu().focus_menu(id);
    key(f, Key::down);
    check(f.services.menu().snapshot(id).active_key == String{u8"leaf"}, "dynamic keyboard index stale");
    version.set(3);
    f.synchronize();
    check(f.services.menu().snapshot(id).rows[2] == before[2] && slots == 1,
          "reparented Menu key lost retained row/slot identity");
    version.set(1);
    f.synchronize();
    const auto leaf_bounds = f.nodes.require(f.services.components().root(before[2])).bounds;
    f.services.pointer().dispatch(
        {PointerIdentity::mouse(), PointerAction::down, PointerButton::primary, leaf_bounds.x + 5, leaf_bounds.y + 5});
    check(f.services.pointer().state(PointerIdentity::mouse())->capture.has_value(),
          "dynamic removal fixture did not capture");
    version.set(2);
    f.synchronize();
    check(!f.services.pointer().state(PointerIdentity::mouse())->capture, "removed row retained pointer capture");
    check(!f.services.components().contains(before[2]) && f.services.menu().snapshot(id).active_key == String{u8"a"},
          "removed active key retained input or failed adjacent fallback");
    first.set(String{u8"Updated"});
    f.synchronize();
    check(slots == 1, "reactive label remounted rich slot");
    f.services.destroy(id);
    check(f.services.surfaces().size() == 0 && f.services.interactions().size() == 0, "dynamic cleanup leaked");
}

void flyout_and_overflow() {
    for (auto mode : {MenuMode::Vertical, MenuMode::Horizontal, MenuMode::Inline}) {
        Fixture f;
        Signal<bool> collapsed{mode == MenuMode::Inline};
        int clicked = 0;
        f.services.mount(Content{[&] {
            Menu(MenuProps{}
                     .mode(mode)
                     .inlineCollapsed(collapsed)
                     .direction(TextDirection::RightToLeft)
                     .theme(MenuTheme::Dark)
                     .triggerSubMenuAction(MenuSubMenuTrigger::Click)
                     .items({{u8"sub",
                              u8"Actions",
                              false,
                              false,
                              MenuItemKind::SubMenu,
                              {{u8"a", u8"Command"},
                               {u8"b", u8"Disabled", true},
                               {u8"nested", u8"More", false, false, MenuItemKind::SubMenu, {{u8"last", u8"Last"}}}}},
                             {u8"wide", u8"A very long navigation title"},
                             {u8"end", u8"End"}})
                     .onClick([&](MenuClickInfo) { ++clicked; })
                     .layout(LayoutStyle{}.width(dp(mode == MenuMode::Horizontal ? 190 : 150)).height(dp(64))));
        }});
        f.synchronize();
        const auto id = f.services.menu().mounted()[0];
        if (mode != MenuMode::Inline) {
            const auto first = f.services.menu().snapshot(id).rows[0];
            const auto& children = f.services.components().children(first);
            const auto label = f.nodes.require(f.services.components().root(children[0])).bounds;
            const auto arrow = f.nodes.require(f.services.components().root(children[1])).bounds;
            check(label.x >= arrow.x + arrow.width + 4, "RTL Menu label overlapped its leading submenu arrow");
        }
        f.services.menu().focus_menu(id);
        key(f, mode == MenuMode::Horizontal ? Key::down : Key::left);
        auto snap = f.services.menu().snapshot(id);
        check(!snap.popups.empty() && snap.active_key == String{u8"a"},
              "flyout did not open and enter with RTL keyboard");
        const auto rect = snap.popups.front();
        check(rect.x >= 0 && rect.y >= 0 && rect.x + rect.width <= 800 && rect.y + rect.height <= 600,
              "flyout escaped window bounds");
        key(f, Key::enter);
        check(clicked == 1, "flyout command failed");
        key(f, Key::down);
        check(f.services.menu().snapshot(id).active_key == String{u8"nested"},
              "flyout navigation did not skip disabled");
        key(f, Key::left);
        check(f.services.menu().snapshot(id).popups.size() == 2, "nested flyout missing");
        key(f, Key::escape);
        check(f.services.menu().snapshot(id).popups.size() == 1, "Escape did not close deepest flyout first");
        key(f, Key::escape);
        check(f.services.menu().snapshot(id).popups.empty(), "Escape did not close root flyout");
        if (mode == MenuMode::Horizontal) {
            check(f.services.menu().snapshot(id).overflow, "horizontal overflow missing");
            key(f, Key::end);
            key(f, Key::down);
            check(!f.services.menu().snapshot(id).popups.empty(),
                  "overflow indicator did not open retained overflow rows");
            key(f, Key::enter);
            check(clicked == 2, "overflow command inaccessible");
        }
        f.services.set_window_active(false);
        f.services.set_window_active(true);
        f.synchronize();
        check(f.services.menu().snapshot(id).popups.empty(), "deactivated flyout revived");
        f.services.destroy(id);
        f.synchronize();
        check(f.services.surfaces().size() == 0 && f.services.interactions().size() == 0 &&
                  !f.services.next_frame_deadline(),
              "flyout resources leaked");
    }
    Fixture hidden;
    Signal<bool> visible{false};
    hidden.services.mount(Content{[&] {
        Modal(ModalProps{}.open(visible).footer(ModalFooterMode::None).width(280), ModalContent{[] {
                  Menu(MenuProps{}.defaultOpenKeys({u8"sub"}).items(
                      {{u8"sub", u8"Sub", false, false, MenuItemKind::SubMenu, {{u8"child", u8"Child"}}}}));
              }});
    }});
    hidden.synchronize();
    const auto owner = hidden.services.menu().mounted()[0];
    check(hidden.services.menu().snapshot(owner).popups.empty(), "hidden Modal exposed Menu flyout");
    visible.set(true);
    hidden.synchronize();
    check(hidden.services.menu().snapshot(owner).popups.size() == 1, "initial hidden Menu never positioned flyout");
    visible.set(false);
    hidden.synchronize();
    visible.set(true);
    hidden.synchronize();
    check(hidden.services.menu().snapshot(owner).popups.empty(), "dismissed Menu flyout revived after parent hide");
    hidden.services.dispose();
    Fixture rail;
    Signal<bool> folded{true};
    rail.services.mount(Content{[&] {
        ThemeConfig config;
        config.menu.tokens.collapsed_width = dp(72);
        config.menu.tokens.collapsed_icon_size = dp(18);
        Theme(
            ThemeProps{}.config(config), ThemeContent{[&] {
                Menu(MenuProps{}
                         .mode(MenuMode::Inline)
                         .inlineCollapsed(folded)
                         .items({{u8"text", u8"Álpha"},
                                 {u8"sub", u8"Tools", false, false, MenuItemKind::SubMenu, {{u8"child", u8"Child"}}}}));
            }});
    }});
    rail.synchronize();
    const auto rail_id = rail.services.menu().mounted()[0];
    const auto rail_node = rail.services.components().root(rail_id);
    check(rail.nodes.require(rail_node).bounds.width == 72, "collapsed Menu did not use its Theme rail width");
    bool grapheme = false;
    for (const auto& text : rail.services.text().mounted_texts()) {
        grapheme |= rail.services.components().branch_active(text.component) &&
                    rail.scene.text_state(text.scene).content() == String{u8"Á"}.view();
    }
    check(grapheme, "collapsed Menu split a Unicode grapheme or omitted the text fallback");
    const auto rail_rows = rail.services.menu().snapshot(rail_id).rows;
    folded.set(false);
    rail.synchronize();
    folded.set(true);
    rail.synchronize();
    check(rail.services.menu().snapshot(rail_id).rows == rail_rows, "collapse toggle remounted Menu rows");
    rail.services.dispose();
}

void hover_lifecycle() {
    Fixture f;
    int opens = 0;
    runtime::ComponentId id;
    f.services.mount(Content{[&] {
        Menu(MenuProps{}
                 .items({{u8"sub", u8"Sub", false, false, MenuItemKind::SubMenu, {{u8"a", u8"A"}}}})
                 .subMenuOpenDelay(0.05F)
                 .subMenuCloseDelay(0.1F)
                 .onOpenChange([&](MenuKeys) { ++opens; })
                 .layout(LayoutStyle{}.width(dp(150))));
    }});
    f.synchronize();
    id = f.services.menu().mounted()[0];
    const auto rows = f.services.menu().snapshot(id).rows;
    const auto bounds = f.nodes.require(f.services.components().root(rows[0])).bounds;
    const auto move = [&](float x, float y) {
        f.services.pointer().dispatch({PointerIdentity::mouse(), PointerAction::move, PointerButton::none, x, y});
    };
    move(bounds.x + 10, bounds.y + 10);
    check(f.services.next_frame_deadline().has_value(), "hover did not schedule bounded wake");
    static_cast<void>(f.services.tick_animations(animation::AnimationTime::microseconds(40000)));
    f.synchronize();
    check(f.services.menu().snapshot(id).popups.empty(), "hover opened before delay");
    static_cast<void>(f.services.tick_animations(animation::AnimationTime::microseconds(50001)));
    f.synchronize();
    check(f.services.menu().snapshot(id).popups.size() == 1 && opens == 1, "hover deadline did not open flyout");
    const auto popup = f.services.menu().snapshot(id).popups[0];
    move(319, 239);
    move(popup.x + 10, popup.y + 10);
    static_cast<void>(f.services.tick_animations(animation::AnimationTime::microseconds(200000)));
    f.synchronize();
    check(f.services.menu().snapshot(id).popups.size() == 1, "crossing trigger-to-popup did not cancel close");
    move(319, 239);
    f.services.set_window_active(false);
    static_cast<void>(f.services.tick_animations(animation::AnimationTime::microseconds(400000)));
    f.services.set_window_active(true);
    f.synchronize();
    check(f.services.menu().snapshot(id).popups.empty() && !f.services.next_frame_deadline(),
          "hidden Menu kept delayed work");
    f.services.destroy(id);
    check(f.services.interactions().size() == 0 && f.services.surfaces().size() == 0, "hover cleanup leaked");
}

void dynamic_failure_and_retirement() {
    Fixture f;
    Signal<int> version{0};
    runtime::ComponentId id;
    int clicks = 0;
    f.services.mount(Content{[&] {
        Menu(MenuProps{}
                 .items(bind([&] {
                     if (version.get() == 1) {
                         return std::vector<MenuItem>{{u8"bad", u8"Bad"}, {u8"bad", u8"Duplicate"}};
                     }
                     if (version.get() == 2) {
                         MenuItem bad{u8"bad", u8"Bad"};
                         bad.content = MenuLabel{[] { throw std::runtime_error("slot failure"); }};
                         return std::vector<MenuItem>{{u8"b", u8"B"}, {u8"a", u8"A"}, bad};
                     }
                     return std::vector<MenuItem>{{u8"a", u8"A"}, {u8"b", u8"B"}};
                 }))
                 .onSelect([&](MenuSelectInfo) { f.services.destroy(id); })
                 .onClick([&](MenuClickInfo) { ++clicks; }));
    }});
    f.synchronize();
    id = f.services.menu().mounted()[0];
    const auto rows = f.services.menu().snapshot(id).rows;
    bool rejected = false;
    try {
        version.set(1);
        f.synchronize();
    } catch (const std::invalid_argument&) {
        rejected = true;
    }
    check(rejected && f.services.menu().snapshot(id).rows == rows, "invalid dynamic topology mutated rows");
    const auto surfaces = f.services.surfaces().size();
    rejected = false;
    try {
        version.set(2);
        f.synchronize();
    } catch (const std::runtime_error&) {
        rejected = true;
    }
    check(rejected && f.services.menu().snapshot(id).rows == rows && f.services.surfaces().size() == surfaces,
          "throwing dynamic slot mutated retained topology or leaked resources");
    f.services.menu().focus_menu(id);
    key(f, Key::enter);
    check(!f.services.components().contains(id) && clicks == 0, "self-destruction delivered stale click");
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
        run("hover", hover_lifecycle);
        run("dynamic", dynamic_topology);
        run("flyout-overflow", flyout_and_overflow);
        run("dynamic-failure", dynamic_failure_and_retirement);
        run("navigation", navigation);
        run("theme", uncontrolled_theme_validation);
        run("retirement", callback_retirement);
        run("reactive-reveal", reactive_active_reveal);
        check(ran > 0, "unknown Menu test case");
    } catch (const std::exception& e) {
        std::cerr << e.what() << '\n';
        return 1;
    }
}
