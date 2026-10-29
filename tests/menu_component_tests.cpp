#include "component/menu_component.hpp"
#include "support/input_fixture.hpp"
#include "support/bundled_icon_checks.hpp"
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
    for (int invalid = 0; invalid < 4; ++invalid) {
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
                    props.items({{u8"sub", u8"Sub", false, false, MenuItemKind::SubMenu, {{u8"c", u8"Child"}}}});
                }
                Menu(props);
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
