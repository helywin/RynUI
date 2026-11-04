#include "component/breadcrumb_component.hpp"
#include "component/dropdown_component.hpp"
#include "support/navigation_fixture.hpp"
#include <iostream>

namespace {
using namespace ryn_test::navigation;

void reactive_navigation_and_flow() {
    Fixture f;
    Signal<bool> first{true};
    Signal<bool> middle{true};
    Signal<bool> last{true};
    Signal<bool> disabled{true};
    Signal<String> label{String{u8"Home"}};
    std::vector<BreadcrumbActivation> changes;
    int rich_runs = 0;
    int separators = 0;
    f.services.mount(Content{[&] {
        Flex(FlexProps{}.vertical(true).gap(dp(17)), FlexContent{[&] {
                 Text(u8"Before");
                 Breadcrumb(BreadcrumbProps{}
                                .onActivate([&](BreadcrumbActivation a) { changes.push_back(a); })
                                .separator(BreadcrumbSeparator{[&] {
                                    ++separators;
                                    Text(u8">");
                                }}),
                            {BreadcrumbItem{u8"home", u8"Home"}.title(label).visible(first).disabled(disabled).icon(
                                 BreadcrumbIcon{[] { Icon(IconProps{}.name(IconName::HomeOutlined)); }}),
                             BreadcrumbItem{u8"middle", u8"Middle"}.visible(middle).richTitle(BreadcrumbTitle{[&] {
                                 ++rich_runs;
                                 Text(u8"Rich middle");
                             }}),
                             BreadcrumbItem{u8"last", u8"Last"}.visible(last)});
                 Text(u8"After");
             }});
    }});
    sync(f);
    const auto id = f.services.breadcrumb().mounted()[0];
    const auto original = f.services.breadcrumb().snapshot(id);
    check(original.current == 2 && rich_runs == 1 && separators == 3, "Breadcrumb initial projection/slots failed");
    click(f, original.items[0]);
    check(changes.empty(), "disabled ancestor activated");
    disabled.set(false);
    sync(f);
    const auto r = bounds(f, original.items[0]);
    check(f.services.hit_test().hit_test({r.x + r.width / 2, r.y + r.height / 2}) == interaction(f, original.items[0]),
          "reenabled Breadcrumb missing real hit");
    click(f, original.items[0]);
    check(changes.size() == 1 && changes.back().key == String{u8"home"} && changes.back().index == 0,
          "ancestor pointer activation wrong");
    key(f, Key::enter);
    check(changes.size() == 2 && f.services.focus().state().focus_visible,
          "ancestor keyboard activation/modality wrong");
    f.services.focus().dispatch({Key::enter, KeyAction::down, KeyModifier::none, true});
    f.services.focus().dispatch({Key::enter, KeyAction::up});
    check(changes.size() == 2, "Breadcrumb repeat/keyup activated");
    click(f, original.items[2]);
    check(changes.size() == 2, "current Breadcrumb activated");
    last.set(false);
    sync(f);
    check(f.services.breadcrumb().snapshot(id).current == 1 &&
              !f.services.components().branch_active(original.separators[1]) &&
              f.services.components().branch_active(original.separators[0]),
          "hidden last left wrong current/separators");
    label.set(String{u8"Updated Home"});
    first.set(false);
    middle.set(false);
    sync(f);
    const auto before = bounds(f, find_text(f, String{u8"Before"}.view()).component);
    const auto after = bounds(f, find_text(f, String{u8"After"}.view()).component);
    check(std::abs(after.y - (before.y + before.height + 17)) < .01F &&
              f.nodes.require(f.services.components().root(id)).out_of_flow && !f.services.focus().state().focused,
          "all-hidden Breadcrumb retained layout gap/focus");
    first.set(true);
    last.set(true);
    sync(f);
    check(f.services.breadcrumb().snapshot(id).items == original.items && rich_runs == 1 && separators == 3 &&
              !f.nodes.require(f.services.components().root(id)).out_of_flow,
          "Breadcrumb restore remounted");
    click(f, original.items[0]);
    check(changes.size() == 3, "restored Breadcrumb not clickable");
    check(glyphs_inside(f, id, bounds(f, id)) > 0, "Breadcrumb has no actual glyphs");
}

void clipping_theme_and_children() {
    Fixture f;
    Signal<LogicalLength> width{dp(280)};
    Signal<LogicalLength> height{dp(48)};
    Signal<ThemeConfig> config{ThemeConfig{}};
    int changes = 0;
    int clicks = 0;
    int runs = 0;
    f.services.mount(Content{[&] {
        Theme(ThemeProps{}.config(config), ThemeContent{[&] {
                  Breadcrumb(BreadcrumbProps{}
                                 .layout(LayoutStyle{}.width(width).height(height))
                                 .onActivate([&](BreadcrumbActivation) { ++changes; }),
                             {BreadcrumbItem{u8"a", u8"A"}.richTitle(BreadcrumbTitle{[&] {
                                  ++runs;
                                  Button(ButtonProps{}.onClick([&] { ++clicks; }),
                                         ButtonContent{[] { Text(u8"Child"); }});
                              }}),
                              BreadcrumbItem{u8"b", u8"Long\nMultiline\nLabel"}.icon(
                                  BreadcrumbIcon{[] { Icon(IconProps{}.name(IconName::SearchOutlined)); }}),
                              {u8"c", u8"Current"}});
              }});
    }});
    sync(f);
    const auto id = f.services.breadcrumb().mounted()[0];
    const auto original = f.services.breadcrumb().snapshot(id);
    const auto button = f.buttons.mounted_buttons()[0];
    click(f, button.component);
    check(clicks == 1 && changes == 0, "Breadcrumb stole child Button pointer activation");
    key(f, Key::space);
    check(clicks == 2 && changes == 0, "Breadcrumb stole child Button keyboard activation");
    const auto root = f.services.components().root(id);
    const auto measured = f.nodes.require(root).measure_count;
    ThemeConfig t;
    t.breadcrumb.tokens.current = Color::rgba8(10, 30, 150);
    config.set(t);
    sync(f);
    check(f.nodes.require(root).measure_count == measured && runs == 1, "Breadcrumb color update remeasured/remounted");
    for (float w : {280.0F, 40.0F, 4.0F, 0.0F}) {
        width.set(dp(w));
        for (float h : {48.0F, 12.0F, 4.0F, 0.0F}) {
            height.set(dp(h));
            sync(f);
            const auto root_bounds = bounds(f, id);
            const auto visible = glyphs_inside(f, id, root_bounds);
            surfaces_and_hits_inside(f, root_bounds);
            if (w == 0 || h == 0) {
                check(visible == 0, "zero Breadcrumb still paints glyphs");
            }
            for (auto item : original.items) {
                glyphs_inside(f, item, bounds(f, item));
            }
        }
    }
    width.set(dp(280));
    height.set(dp(48));
    sync(f);
    check(f.services.breadcrumb().snapshot(id).items == original.items && runs == 1,
          "Breadcrumb clipping rebuilt content");
    static_cast<void>(f.frames.consume_request());
    sync(f);
    check(!f.frames.pending() && !f.services.next_frame_deadline(), "Breadcrumb idle requests frames");
}

void validation_and_callback_lifetime() {
    for (bool duplicate : {false, true}) {
        Fixture f;
        bool rejected = false;
        try {
            f.services.mount(
                Content{[&] { Breadcrumb({}, {{u8"a", u8"A"}, {duplicate ? String{u8"a"} : String{}, u8"B"}}); }});
        } catch (const std::invalid_argument&) {
            rejected = true;
        }
        check(rejected && f.nodes.size() == 0 && f.services.interactions().size() == 0,
              "invalid Breadcrumb partially mounted");
    }
    Fixture f;
    runtime::ComponentId owner;
    f.services.mount(Content{[&] {
        Breadcrumb(
            BreadcrumbProps{}.onActivate([&](BreadcrumbActivation) { static_cast<void>(f.services.destroy(owner)); }),
            {{u8"a", u8"A"}, {u8"b", u8"B"}});
    }});
    sync(f);
    owner = f.services.breadcrumb().mounted()[0];
    click(f, f.services.breadcrumb().snapshot(owner).items[0]);
    check(f.services.breadcrumb().mounted().empty() && f.services.interactions().size() == 0 &&
              f.services.rounded_effects().live_count() == 0,
          "Breadcrumb callback destroy leaked");
}

void hidden_input_cleanup() {
    Fixture f;
    Signal<bool> shown{true};
    runtime::ComponentId owner;
    bool retire = false;
    int navigation = 0;
    f.services.mount(Content{[&] {
        Breadcrumb(BreadcrumbProps{}.onActivate([&](BreadcrumbActivation) { ++navigation; }),
                   {BreadcrumbItem{u8"a", u8"A"}.visible(shown).richTitle(BreadcrumbTitle{[&] {
                        Input(InputProps{}.onBlur([&] {
                            if (retire) {
                                static_cast<void>(f.services.destroy(owner));
                            }
                        }));
                    }}),
                    {u8"b", u8"B"}});
    }});
    sync(f);
    owner = f.services.breadcrumb().mounted()[0];
    const auto input = f.inputs.mounted_inputs()[0];
    click(f, input.component);
    check(f.services.focus().state().focused == input.interaction && f.inputs.sessions().active().valid(),
          "Breadcrumb child Input did not own focus/session");
    key(f, Key::space);
    check(navigation == 0 && f.inputs.sessions().active().valid(), "Breadcrumb stole Input key");
    retire = true;
    shown.set(false);
    sync(f);
    check(f.services.breadcrumb().mounted().empty() && f.inputs.mounted_inputs().empty() &&
              !f.inputs.sessions().active().valid() && f.services.interactions().size() == 0,
          "hidden Breadcrumb blur-destroy left input resources");
}

void dynamic_topology_menu_and_rtl() {
    Fixture f;
    Signal<std::vector<BreadcrumbItem>> source{std::vector<BreadcrumbItem>{}};
    Signal<TextDirection> direction{TextDirection::LeftToRight};
    Signal<bool> wrap{true};
    Signal<LogicalLength> width{dp(560)};
    int title_mounts{};
    std::vector<BreadcrumbActivation> activations;
    std::vector<String> menu_clicks;
    auto home = BreadcrumbItem{u8"home", u8"Home"}.menu(
        MenuProps{}
            .items({{u8"dashboard", u8"Dashboard"}, {u8"settings", u8"Settings"}})
            .onClick([&](MenuClickInfo info) { menu_clicks.push_back(info.key); }));
    auto draft = BreadcrumbItem{u8"draft", u8"Draft"}.richTitle(BreadcrumbTitle{[&] {
        ++title_mounts;
        Input(InputProps{}.defaultValue(u8"retained draft"));
    }});
    auto end = BreadcrumbItem{u8"end", u8"Current"}.separator(BreadcrumbSeparator{[] { Text(u8"|"); }});
    source.set({home, draft, end});
    f.services.mount(Content{[&] {
        Breadcrumb(BreadcrumbProps{}
                       .items(source)
                       .wrap(wrap)
                       .direction(direction)
                       .layout(LayoutStyle{}.width(width))
                       .onActivate([&](BreadcrumbActivation event) { activations.push_back(event); }));
    }});
    sync(f);
    const auto id = f.services.breadcrumb().mounted()[0];
    const auto original = f.services.breadcrumb().snapshot(id);
    const auto input = f.inputs.mounted_inputs()[0];
    source.set({end, home, draft});
    sync(f);
    const auto reordered = f.services.breadcrumb().snapshot(id);
    check(reordered.items[0] == original.items[2] && reordered.items[1] == original.items[0] &&
              reordered.items[2] == original.items[1] && title_mounts == 1,
          "Breadcrumb reorder remounted keyed content");
    click(f, reordered.items[0]);
    check(activations.back().key == String{u8"end"} && activations.back().index == 0,
          "Breadcrumb reordered callback has stale index");
    click(f, reordered.items[1]);
    const auto dropdown = f.services.dropdown().mounted()[0];
    check(f.services.dropdown().snapshot(dropdown).geometry.visible, "Breadcrumb built-in menu did not open");
    const auto menu = f.services.dropdown().snapshot(dropdown).menu;
    const auto popup = f.services.dropdown().snapshot(dropdown).popup;
    check(f.services.components().branch_active(popup), "Breadcrumb popup has no active native branch");
    key(f, Key::enter);
    check(menu_clicks.size() == 1 && menu_clicks.back() == String{u8"dashboard"} &&
              !f.services.dropdown().snapshot(dropdown).geometry.visible,
          "Breadcrumb menu did not preserve native selection callback/close");
    click(f, reordered.items[1]);
    key(f, Key::escape);
    check(!f.services.dropdown().snapshot(dropdown).geometry.visible, "Breadcrumb menu Escape did not close");
    click(f, input.component);
    check(f.inputs.sessions().active().valid(), "Breadcrumb retained editor lost input session");
    direction.set(TextDirection::RightToLeft);
    sync(f);
    check(bounds(f, reordered.items[0]).x > bounds(f, reordered.items[1]).x && f.inputs.sessions().active().valid(),
          "Breadcrumb RTL rebuilt editor or failed mirror");
    width.set(dp(160));
    sync(f);
    const auto root = bounds(f, id);
    check(root.height > 22 && title_mounts == 1, "Breadcrumb did not wrap naturally");
    glyphs_inside(f, id, root);
    width.set(dp(560));
    sync(f);
    click(f, reordered.items[1]);
    check(f.services.dropdown().snapshot(dropdown).geometry.visible, "Breadcrumb menu could not reopen");
    source.set({draft, end});
    sync(f);
    check(f.services.dropdown().mounted().empty() && !f.services.components().contains(popup) &&
              f.services.breadcrumb().snapshot(id).items[0] == original.items[1],
          "Breadcrumb removed menu leaked popup or rebuilt retained key");
    source.set({end});
    sync(f);
    check(f.inputs.mounted_inputs().empty() && !f.inputs.sessions().active().valid(),
          "Breadcrumb removed editor leaked IME");
    static_cast<void>(menu);
}

void dynamic_validation_replacement_and_blur_destroy() {
    Fixture f;
    Signal<std::vector<BreadcrumbItem>> source{std::vector<BreadcrumbItem>{{u8"a", u8"A"}, {u8"b", u8"B"}}};
    f.services.mount(Content{[&] { Breadcrumb(BreadcrumbProps{}.items(source)); }});
    sync(f);
    const auto id = f.services.breadcrumb().mounted()[0];
    const auto original = f.services.breadcrumb().snapshot(id);
    bool slot_failed{};
    source.set({{u8"a", u8"A"}, {u8"b", u8"B"}, BreadcrumbItem{u8"bad", u8"Bad"}.richTitle(BreadcrumbTitle{[] {
                    throw std::runtime_error("slot failure");
                }})});
    try {
        sync(f);
    } catch (const std::runtime_error&) {
        slot_failed = true;
    }
    check(slot_failed && f.services.breadcrumb().snapshot(id).items == original.items,
          "Breadcrumb throwing append corrupted retained keys");
    bool rejected{};
    try {
        source.set({{u8"a", u8"A"}, {u8"a", u8"Duplicate"}});
        sync(f);
    } catch (const std::invalid_argument&) {
        rejected = true;
    }
    check(rejected && f.services.breadcrumb().snapshot(id).items == original.items,
          "Breadcrumb invalid topology corrupted retained tree");
    source.set({BreadcrumbItem{u8"a", u8"Replacement"}.richTitle(BreadcrumbTitle{[] { Text(u8"Replacement"); }}),
                {u8"b", u8"B"}});
    sync(f);
    check(f.services.breadcrumb().snapshot(id).items[0] != original.items[0] &&
              f.services.breadcrumb().snapshot(id).items[1] == original.items[1],
          "Breadcrumb replacement affected unrelated key");
    Fixture g;
    Signal<std::vector<BreadcrumbItem>> entries{std::vector<BreadcrumbItem>{}};
    runtime::ComponentId owner;
    entries.set({BreadcrumbItem{u8"edit", u8"Edit"}.richTitle(
                     BreadcrumbTitle{[&] { Input(InputProps{}.onBlur([&] { g.services.destroy(owner); })); }}),
                 {u8"end", u8"End"}});
    g.services.mount(Content{[&] { Breadcrumb(BreadcrumbProps{}.items(entries)); }});
    sync(g);
    owner = g.services.breadcrumb().mounted()[0];
    click(g, g.inputs.mounted_inputs()[0].component);
    entries.set({{u8"end", u8"End"}});
    sync(g);
    check(g.services.breadcrumb().mounted().empty() && !g.inputs.sessions().active().valid() &&
              g.services.interactions().size() == 0,
          "Breadcrumb dynamic blur self-destruction leaked");
}
} // namespace

int main() {
    try {
        dynamic_topology_menu_and_rtl();
        dynamic_validation_replacement_and_blur_destroy();
        reactive_navigation_and_flow();
        clipping_theme_and_children();
        validation_and_callback_lifetime();
        hidden_input_cleanup();
        std::cout << "Breadcrumb component contracts passed\n";
        return 0;
    } catch (const std::exception& e) {
        std::cerr << e.what() << '\n';
        return 1;
    }
}
