#include "component/shell_layout_component.hpp"
#include "support/input_fixture.hpp"

#include <cmath>
#include <iostream>
#include <limits>
#include <string_view>

namespace {
using namespace ryn;
using namespace ryn::input;
using Fixture = ryn_test::input_component::Fixture;
using Role = detail::ShellLayoutRole;

void check(bool value, const char* message) {
    if (!value) {
        throw std::runtime_error(message);
    }
}

void near(float value, float expected, const char* message) {
    check(std::abs(value - expected) < 0.02F, message);
}

void sync(Fixture& f, float width = 900, float height = 600) {
    check(f.services.layout_and_synchronize({width, height}, {0, 0, width, height}), "Layout synchronization failed");
}

runtime::ComponentId find(Fixture& f, Role role, std::size_t ordinal = 0) {
    for (auto id : f.services.shell_layout().mounted()) {
        if (f.services.shell_layout().snapshot(id).role == role && ordinal-- == 0) {
            return id;
        }
    }
    throw std::runtime_error("Layout role missing");
}

runtime::Rect bounds(Fixture& f, runtime::ComponentId id) {
    return f.nodes.require(f.services.components().root(id)).bounds;
}

void click(Fixture& f, runtime::ComponentId id) {
    const auto r = bounds(f, id);
    for (auto action : {PointerAction::down, PointerAction::up}) {
        f.services.pointer().dispatch(
            {PointerIdentity::mouse(), action, PointerButton::primary, r.x + r.width / 2, r.y + r.height / 2});
    }
    sync(f);
}

void nested() {
    Fixture f;
    int runs{};
    f.services.mount(Content{[&] {
        Layout(LayoutProps{}, LayoutContent{[&] {
                   Header(HeaderProps{}, HeaderContent{[] { Text(u8"Header"); }});
                   Layout(LayoutProps{}, LayoutContent{[&] {
                              Sider(SiderProps{}.width(dp(180)).theme(SiderTheme::Light),
                                    SiderContent{[] { Text(u8"Left"); }});
                              ContentArea(ContentProps{}, ContentBody{[&] {
                                              ++runs;
                                              Input(InputProps{}.defaultValue(u8"Draft"));
                                          }});
                              Sider(SiderProps{}.width(dp(120)).reverseArrow().collapsible(),
                                    SiderContent{[] { Text(u8"Right"); }});
                          }});
                   Footer(FooterProps{}, FooterContent{[] { Text(u8"Footer"); }});
               }});
    }});
    sync(f);
    const auto outer = find(f, Role::Layout);
    const auto inner = find(f, Role::Layout, 1);
    const auto content = find(f, Role::Content);
    check(!f.services.shell_layout().snapshot(outer).horizontal && f.services.shell_layout().snapshot(inner).horizontal,
          "Layout did not detect only direct Siders");
    near(bounds(f, find(f, Role::Header)).height, 64, "Header token height lost");
    const auto header_body = f.services.shell_layout().snapshot(find(f, Role::Header)).body;
    const auto& header_text =
        f.nodes.require(f.nodes.require(f.services.components().root(header_body)).children.front());
    near(header_text.bounds.y, 21, "Header did not vertically center its content");
    near(bounds(f, find(f, Role::Footer)).height, 70, "Footer natural content/padding lost");
    near(bounds(f, inner).height, 466, "Nested Layout did not receive remaining height");
    near(bounds(f, content).x, 180, "Content not after Sider");
    near(bounds(f, content).width, 600, "Two Siders did not reserve widths");
    near(bounds(f, find(f, Role::Sider, 1)).x, 780, "Right Sider misplaced");
    const auto editor = f.inputs.mounted_inputs()[0].editor;
    sync(f, 400, 350);
    near(bounds(f, content).width, 100, "Nested layout resize failed");
    check(runs == 1 && f.inputs.mounted_inputs()[0].editor == editor, "Resize remounted content");
}

void responsive() {
    Fixture f;
    std::vector<bool> breakpoints;
    std::vector<std::pair<bool, SiderCollapseType>> changes;
    f.services.mount(Content{[&] {
        Layout(LayoutProps{}.layout(LayoutStyle{}.width(dp(300))), LayoutContent{[&] {
                   Sider(
                       SiderProps{}
                           .breakpoint(SiderBreakpoint::Lg)
                           .onBreakpoint([&](bool value) { breakpoints.push_back(value); })
                           .onCollapse([&](bool value, SiderCollapseType type) { changes.emplace_back(value, type); }),
                       SiderContent{[] { Text(u8"Navigation"); }});
                   ContentArea(ContentProps{}, ContentBody{[] { Text(u8"Main"); }});
               }});
    }});
    sync(f, 1200);
    const auto sider = find(f, Role::Sider);
    check(!f.services.shell_layout().snapshot(sider).collapsed && breakpoints == std::vector{false},
          "Sider used container width");
    sync(f, 991);
    check(f.services.shell_layout().snapshot(sider).collapsed && breakpoints == std::vector{false, true} &&
              changes.size() == 1 && changes[0] == std::pair{true, SiderCollapseType::Responsive},
          "Responsive collapse failed");
    near(bounds(f, sider).width, 80, "Collapsed width default lost");
    sync(f, 800);
    check(changes.size() == 1 && breakpoints.size() == 2, "Repeated breakpoint callbacks");
    sync(f, 992);
    check(!f.services.shell_layout().snapshot(sider).collapsed && changes.size() == 2, "Responsive expand failed");
}

void shared_breakpoints() {
    Fixture f;
    Signal<std::optional<SiderBreakpoint>> selected{std::optional{SiderBreakpoint::Xs}};
    f.services.mount(Content{[&] {
        Layout(LayoutProps{}.layout(LayoutStyle{}.width(dp(200))), LayoutContent{[&] {
                   Sider(SiderProps{}.breakpoint(selected), SiderContent{[] { Text(u8"Side"); }});
                   ContentArea(ContentProps{}, ContentBody{[] { Text(u8"Main"); }});
               }});
    }});
    for (auto point : {Breakpoint::Xs, Breakpoint::Sm, Breakpoint::Md, Breakpoint::Lg, Breakpoint::Xl, Breakpoint::Xxl,
                       Breakpoint::Xxxl}) {
        selected.set(point);
        const float threshold = Breakpoints{}.width(point);
        sync(f, threshold - 1);
        check(f.services.shell_layout().snapshot(find(f, Role::Sider)).collapsed,
              "Sider did not collapse below shared breakpoint width");
        sync(f, threshold);
        check(!f.services.shell_layout().snapshot(find(f, Role::Sider)).collapsed,
              "Sider used Responsive xs default semantics as a threshold");
    }
}

void controlled() {
    Fixture f;
    Signal<bool> collapsed{false};
    Signal<bool> reverse{false};
    bool echo{};
    int requests{};
    f.services.mount(Content{[&] {
        Layout(LayoutProps{}, LayoutContent{[&] {
                   Sider(SiderProps{}.collapsed(collapsed).reverseArrow(reverse).collapsible().onCollapse(
                             [&](bool value, SiderCollapseType type) {
                                 check(type == SiderCollapseType::ClickTrigger, "Trigger callback type wrong");
                                 ++requests;
                                 if (echo) {
                                     collapsed.set(value);
                                 }
                             }),
                         SiderContent{[] { Text(u8"Side"); }});
                   ContentArea(ContentProps{}, ContentBody{[] { Text(u8"Main"); }});
               }});
    }});
    sync(f);
    const auto sider = find(f, Role::Sider);
    const auto snap = f.services.shell_layout().snapshot(sider);
    click(f, snap.trigger);
    check(requests == 1 && !f.services.shell_layout().snapshot(sider).collapsed, "Controlled refusal changed width");
    echo = true;
    f.services.focus().request_focus(snap.interaction, FocusModality::keyboard);
    f.services.focus().dispatch({Key::enter, KeyAction::down});
    f.services.focus().dispatch({Key::enter, KeyAction::up});
    sync(f);
    check(requests == 2 && f.services.shell_layout().snapshot(sider).collapsed, "Keyboard controlled collapse failed");
    reverse.set(true);
    sync(f);
    check(f.services.shell_layout().snapshot(sider).trigger == snap.trigger, "Arrow update remounted trigger");
}

void zero_width() {
    Fixture f;
    Signal<bool> collapsed{false};
    InputRef ref;
    int runs{};
    f.services.mount(Content{[&] {
        Layout(LayoutProps{}, LayoutContent{[&] {
                   Sider(SiderProps{}.collapsed(collapsed).collapsedWidth(dp(0)).collapsible().onCollapse(
                             [&](bool value, SiderCollapseType) { collapsed.set(value); }),
                         SiderContent{[&] {
                             ++runs;
                             Input(InputProps{}.ref(ref).defaultValue(u8"retained"));
                         }});
                   ContentArea(ContentProps{}, ContentBody{[] { Text(u8"Main"); }});
               }});
    }});
    sync(f);
    const auto sider = find(f, Role::Sider);
    const auto original = f.services.shell_layout().snapshot(sider);
    const auto editor = f.inputs.mounted_inputs()[0];
    check(ref.focus({InputFocusCursor::All}), "Sider editor could not focus");
    const auto session = f.inputs.sessions().active();
    check(bool(f.inputs.dispatch(TextCommitted{String{u8"saved"}, session})), "Sider input commit failed");
    const auto r = f.inputs.layout_snapshot(editor.component).viewport;
    f.services.pointer().dispatch(
        {PointerIdentity::mouse(), PointerAction::down, PointerButton::primary, r.x + 3, r.y + 3});
    collapsed.set(true);
    sync(f);
    const auto zero = f.services.shell_layout().snapshot(sider);
    check(zero.zero_width && zero.trigger_visible && bounds(f, sider).width == 0 && bounds(f, zero.trigger).width == 40,
          "Zero width lost reachable trigger");
    check(!f.services.pointer().state(PointerIdentity::mouse())->capture && !f.services.focus().state().focused &&
              !ref.focus() && !f.inputs.sessions().active().valid(),
          "Zero Sider kept input focus/capture/session");
    check(f.services.focus().request_focus(zero.interaction, FocusModality::keyboard), "Zero trigger cannot focus");
    click(f, zero.trigger);
    check(!f.services.shell_layout().snapshot(sider).collapsed && runs == 1 &&
              f.inputs.mounted_inputs()[0].editor == editor.editor &&
              f.inputs.editors().require(editor.editor).value() == String{u8"saved"}.bytes(),
          "Zero trigger failed restore retained draft");
    check(original.body == f.services.shell_layout().snapshot(sider).body, "Sider changed body identity");
}

void theme() {
    const auto normal = resolve_theme();
    check(normal.layout().colors.body_background == Color::rgba8(245, 245, 245) &&
              normal.layout().colors.header_text == normal.alias().color_text,
          "Layout default Ant token colors drifted");
    ThemeConfig compact_config;
    compact_config.algorithms = {ThemeAlgorithm::Compact};
    const auto compact = resolve_theme(compact_config);
    near(compact.layout().metrics.header_height, compact.map().control_height * 2,
         "Compact Layout ignored control height");
    near(compact.layout().metrics.header_padding_inline, compact.map().control_height_large * 1.25F,
         "Compact Layout padding did not derive from controls");
    near(compact.layout().metrics.font_size, compact.map().font_size, "Compact Layout typography drifted");
    ThemeConfig dark_config;
    dark_config.algorithms = {ThemeAlgorithm::Dark};
    const auto dark = resolve_theme(dark_config);
    check(dark.layout().colors.body_background == dark.map().color_background_base &&
              dark.layout().colors.body_background.alpha() == 1,
          "Dark Layout background is not opaque");
    auto scope = theme_runtime::ThemeScope::create_default();
    int colors{};
    int metrics{};
    auto c = scope->capture([&](auto) { ++colors; }, [&] { static_cast<void>(scope->layout_colors()); });
    auto m = scope->capture([&](auto) { ++metrics; }, [&] { static_cast<void>(scope->layout_metrics()); });
    ThemeConfig config;
    config.layout.tokens.header_background = Color::rgba8(25, 30, 35);
    check(scope->update(config) && colors == 1 && metrics == 0, "Layout colors not independently subscribed");
    config.layout.tokens.header_height = dp(72);
    check(scope->update(config) && metrics == 1, "Layout metrics not subscribed");
    const auto identity = scope->snapshot().identity();
    config.layout.tokens.header_height = dp(-1);
    bool threw{};
    try {
        scope->update(config);
    } catch (const std::invalid_argument&) {
        threw = true;
    }
    check(threw && scope->snapshot().identity() == identity, "Invalid Layout theme did not fail atomically");
    Fixture f;
    Signal<ThemeConfig> theme{ThemeConfig{}};
    f.services.mount(Content{[&] {
        Theme(ThemeProps{}.config(theme), ThemeContent{[] {
                  Layout(LayoutProps{}, LayoutContent{[] {
                             Header(HeaderProps{}, HeaderContent{[] { Text(u8"Header"); }});
                             ContentArea(ContentProps{}, ContentBody{[] { Text(u8"Content"); }});
                         }});
              }});
    }});
    sync(f);
    config.layout.tokens.header_height = dp(72);
    config.algorithms = {ThemeAlgorithm::Dark};
    theme.set(config);
    sync(f);
    near(bounds(f, find(f, Role::Header)).height, 72, "Live Layout token failed to remeasure");
    check(scope->snapshot().diagnostic_json().find("\"layout\":") != std::string::npos,
          "Layout tokens missing diagnostics");
}

void lifecycle() {
    Fixture f;
    runtime::ComponentId root;
    int callbacks{};
    f.services.mount(Content{[&] {
        Layout(LayoutProps{}, LayoutContent{[&] {
                   Sider(SiderProps{}.collapsible().onCollapse([&](bool, SiderCollapseType) {
                       ++callbacks;
                       f.services.destroy(root);
                   }),
                         SiderContent{[] { Input(InputProps{}); }});
                   ContentArea(ContentProps{}, ContentBody{[] { Text(u8"Content"); }});
               }});
    }});
    sync(f);
    root = find(f, Role::Layout);
    const auto sider = find(f, Role::Sider);
    const auto trigger = f.services.shell_layout().snapshot(sider).trigger;
    click(f, trigger);
    check(callbacks == 1 && f.services.shell_layout().mounted().empty() && f.inputs.mounted_inputs().empty() &&
              f.services.interactions().size() == 0,
          "Callback destroy leaked Layout resources");
    check(!f.services.pointer().state(PointerIdentity::mouse())->capture, "Destroyed Layout kept capture");
}

void custom() {
    Fixture f;
    int custom_runs{};
    Signal<bool> enabled{true};
    f.services.mount(Content{[&] {
        Layout(LayoutProps{}, LayoutContent{[&] {
                   Sider(SiderProps{}.collapsible(enabled).trigger(SiderTrigger{[&] {
                       ++custom_runs;
                       Text(u8"Toggle");
                   }}),
                         SiderContent{[] { Text(u8"Custom"); }});
                   ContentArea(ContentProps{}, ContentBody{[] { Text(u8"Content"); }});
                   Sider(SiderProps{}.collapsible().trigger(SiderTriggerMode::Hidden),
                         SiderContent{[] { Text(u8"Hidden"); }});
               }});
    }});
    sync(f);
    const auto snap = f.services.shell_layout().snapshot(find(f, Role::Sider));
    check(!f.services.shell_layout().snapshot(find(f, Role::Sider, 1)).trigger_visible, "Hidden trigger is visible");
    const auto r = bounds(f, snap.trigger);
    f.services.pointer().dispatch(
        {PointerIdentity::mouse(), PointerAction::down, PointerButton::primary, r.x + 2, r.y + 2});
    enabled.set(false);
    sync(f);
    check(!f.services.pointer().state(PointerIdentity::mouse())->capture &&
              !f.services.focus().request_focus(snap.interaction, FocusModality::keyboard),
          "Disabled trigger retained capture/focus");
    enabled.set(true);
    sync(f);
    click(f, snap.trigger);
    check(f.services.shell_layout().snapshot(find(f, Role::Sider)).collapsed && custom_runs == 1,
          "Custom trigger failed");
}

void initial_zero() {
    Fixture f;
    Signal<bool> collapsed{true};
    f.services.mount(Content{[&] {
        Layout(LayoutProps{}, LayoutContent{[&] {
                   Sider(SiderProps{}.collapsed(collapsed).collapsedWidth(dp(0)).collapsible(),
                         SiderContent{[] { Input(InputProps{}.autoFocus()); }});
                   ContentArea(ContentProps{}, ContentBody{[] { Text(u8"Main"); }});
               }});
    }});
    sync(f);
    check(!f.inputs.sessions().active().valid() && !f.services.focus().state().focused,
          "Initially zero Sider started a hidden autofocus session");
    collapsed.set(false);
    sync(f);
    check(f.inputs.mounted_inputs().size() == 1, "Initially hidden Sider lost retained child");
}

void validation() {
    for (int which = 0; which < 4; ++which) {
        Fixture f;
        bool threw{};
        try {
            f.services.mount(Content{[&] {
                auto p = SiderProps{};
                if (which == 0) {
                    p.width(dp(-1));
                }
                if (which == 1) {
                    p.collapsed(false).defaultCollapsed(false);
                }
                if (which == 2) {
                    p.theme(static_cast<SiderTheme>(99));
                }
                if (which == 3) {
                    p.breakpoint(static_cast<SiderBreakpoint>(99));
                }
                Sider(p, SiderContent{[] { Text(u8"Invalid"); }});
            }});
        } catch (const std::invalid_argument&) {
            threw = true;
        }
        check(threw && f.services.components().component_count() == 0, "Invalid Sider mounted resources");
    }
}
} // namespace

int main(int argc, char** argv) {
    try {
        const std::string_view name = argc > 1 ? argv[1] : "";
        if (name == "nested") {
            nested();
        } else if (name == "responsive") {
            responsive();
        } else if (name == "shared_breakpoints") {
            shared_breakpoints();
        } else if (name == "controlled") {
            controlled();
        } else if (name == "zero_width") {
            zero_width();
        } else if (name == "theme") {
            theme();
        } else if (name == "lifecycle") {
            lifecycle();
        } else if (name == "custom") {
            custom();
        } else if (name == "initial_zero") {
            initial_zero();
        } else if (name == "validation") {
            validation();
        } else {
            throw std::runtime_error("Unknown Layout test");
        }
        std::cout << "Layout " << name << " passed\n";
    } catch (const std::exception& error) {
        std::cerr << error.what() << '\n';
        return 1;
    }
}
