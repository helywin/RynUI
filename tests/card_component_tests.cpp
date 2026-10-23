#include "component/card_component.hpp"
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

void check(bool condition, const char* message) {
    if (!condition) {
        throw std::runtime_error(message);
    }
}

runtime::Rect bounds(Fixture& f, runtime::ComponentId id) {
    return f.nodes.require(f.services.components().root(id)).bounds;
}

void click(Fixture& f, runtime::ComponentId id) {
    const auto rect = bounds(f, id);
    for (const auto action : {PointerAction::down, PointerAction::up}) {
        f.services.pointer().dispatch({PointerIdentity::mouse(), action, PointerButton::primary,
                                       rect.x + rect.width / 2, rect.y + rect.height / 2});
    }
    f.synchronize();
}

void regions_loading_and_nested_controls() {
    Signal<bool> loading{false};
    Signal<CardSize> size{CardSize::Default};
    Signal<CardVariant> variant{CardVariant::Outlined};
    Signal<String> title{String{u8"Profile / 账户"}};
    Signal<ThemeConfig> theme{ThemeConfig{}};
    int root_runs{};
    int body_runs{};
    int extra_runs{};
    int cover_runs{};
    int action_runs{};
    int clicks{};
    Fixture f;
    f.services.set_motion_preference(animation::MotionPreference::reduced);
    f.services.mount(Content{[&] {
        ++root_runs;
        Theme(
            ThemeProps{}.config(theme), ThemeContent{[&] {
                Card(CardProps{}.title(title).size(size).variant(variant).hoverable().loading(loading).layout(
                         LayoutStyle{}.width(dp(320))),
                     CardContent{[&] {
                         ++body_runs;
                         Flex(FlexProps{}.vertical(true).gap(dp(8)), FlexContent{[] {
                                  Input(
                                      InputProps{}.defaultValue(u8"Draft / 草稿").layout(LayoutStyle{}.width(dp(180))));
                                  Select(SelectProps{}
                                             .options({{1, u8"One"}, {2, u8"Two"}})
                                             .layout(LayoutStyle{}.width(dp(180))));
                              }});
                     }},
                     CardSlots{.extra = CardExtra{[&] {
                                   ++extra_runs;
                                   Button(ButtonProps{}.onClick([&] { ++clicks; }), [] { Text(u8"Edit"); });
                               }},
                               .cover = CardCover{[&] {
                                   ++cover_runs;
                                   Text(u8"Cover / 封面");
                               }},
                               .actions = CardActions{[&] {
                                   ++action_runs;
                                   Button(ButtonProps{}.onClick([&] { ++clicks; }), [] { Text(u8"Save"); });
                               }}});
            }});
    }});
    f.synchronize();
    const auto id = f.services.card().mounted()[0];
    const auto initial = f.services.card().snapshot(id);
    const auto input = f.inputs.mounted_inputs()[0];
    const auto select_id = f.services.select().mounted()[0];
    const auto input_bounds = bounds(f, input.component);
    check(initial.has_header && initial.header_bounds.height == 56 &&
              bounds(f, initial.cover).y == initial.header_bounds.y + initial.header_bounds.height &&
              initial.body_bounds.y == bounds(f, initial.cover).y + bounds(f, initial.cover).height &&
              initial.actions_bounds.y == initial.body_bounds.y + initial.body_bounds.height &&
              input_bounds.x == initial.body_bounds.x + 24 && input_bounds.y == initial.body_bounds.y + 24,
          "Card regions/padding/order differ");
    check(std::ranges::count_if(f.services.interactions().declaration_order(),
                                [&](auto hit) {
                                    const auto& record = f.services.interactions().require(hit);
                                    return record.eligible && record.focusable && record.tab_stop &&
                                           f.services.interactions().branch_active(hit);
                                }) == 4,
          "Card added a Tab stop");
    click(f, f.buttons.mounted_buttons()[0].component);
    check(clicks == 1 && f.services.card().snapshot(id).hovered, "Card swallowed a nested action or missed hover");
    f.services.pointer().dispatch({PointerIdentity::mouse(), PointerAction::move, PointerButton::none, 799, 599});
    check(!f.services.card().snapshot(id).hovered, "Card hover stuck outside");
    f.services.pointer().dispatch({PointerIdentity::mouse(), PointerAction::move, PointerButton::none,
                                   initial.body_bounds.x + 5, initial.body_bounds.y + 5});
    check(f.services.card().snapshot(id).hovered, "Card background did not participate in hover hit testing");
    click(f, f.services.select().snapshot(select_id).selector);
    check(f.services.select().snapshot(select_id).visible, "Card nested Select did not open");
    loading.set(true);
    f.synchronize();
    check(!f.services.select().snapshot(select_id).visible && !f.inputs.sessions().active().valid() &&
              !f.services.components().branch_active(initial.body),
          "Card loading retained a nested popup/session");
    click(f, f.buttons.mounted_buttons()[0].component);
    check(clicks == 2 && f.services.components().branch_active(initial.extra) &&
              f.services.components().branch_active(initial.actions),
          "Card loading hid header or actions");
    loading.set(false);
    f.synchronize();
    check(f.services.focus().request_focus(input.interaction, FocusModality::keyboard), "Card Input focus failed");
    static_cast<void>(f.inputs.editors().require(input.editor).select_all());
    check(bool(f.inputs.dispatch(TextCommitted{String{u8"保留编辑 / Retained"}, f.inputs.sessions().active()})),
          "Card Input edit failed");
    f.synchronize();
    const auto count = f.services.components().component_count();
    const auto text_count = f.scene.size();
    loading.set(true);
    f.synchronize();
    check(f.services.card().snapshot(id).loading && !f.services.focus().state().focused &&
              !f.inputs.sessions().active().valid(),
          "Card loading did not clear body focus/IME");
    const auto hit = f.services.hit_test().hit_test({input_bounds.x + 10, input_bounds.y + 10});
    check(!hit || *hit != input.interaction, "hidden Card body retained hit geometry");
    loading.set(false);
    f.synchronize();
    check(f.inputs.editors().require(input.editor).value() == String{u8"保留编辑 / Retained"}.bytes() &&
              f.services.card().snapshot(id).body == initial.body &&
              f.inputs.mounted_inputs()[0].editor == input.editor &&
              f.services.components().component_count() == count && f.scene.size() == text_count,
          "Card loading replaced the editor or user data");
    size.set(CardSize::Small);
    variant.set(CardVariant::Borderless);
    title.set(String{u8"Updated"});
    f.synchronize();
    check(f.services.card().snapshot(id).header_bounds.height == 38 &&
              bounds(f, input.component).x == bounds(f, initial.body).x + 12,
          "Card small size did not update padding/header");
    const auto root_node = f.services.components().root(id);
    const auto measures = f.nodes.require(root_node).measure_count;
    const auto body_measures = f.nodes.require(f.services.components().root(initial.body)).measure_count;
    ThemeConfig custom;
    custom.card.tokens.background = Color::rgba8(30, 50, 80);
    theme.set(custom);
    f.synchronize();
    check(f.nodes.require(root_node).measure_count == measures &&
              f.nodes.require(f.services.components().root(initial.body)).measure_count == body_measures,
          "Card color change remeasured content");
    custom.card.tokens.border_width = dp(0);
    theme.set(custom);
    f.synchronize();
    check(root_runs == 1 && body_runs == 1 && extra_runs == 1 && cover_runs == 1 && action_runs == 1,
          "Card ordinary update reran slots");
    f.services.set_window_active(false);
    f.synchronize();
    (void)f.frames.consume_request();
    const auto effects = f.services.rounded_effects().diagnostics();
    f.synchronize();
    check(!f.frames.pending() && !f.services.next_frame_deadline() &&
              f.services.rounded_effects().diagnostics().material_updates == effects.material_updates &&
              f.services.rounded_effects().diagnostics().geometry_updates == effects.geometry_updates,
          "idle Card requested frames or changed effects");
    check(f.services.destroy(id), "Card destroy failed");
    f.synchronize();
    check(f.services.card().mounted().empty() && f.inputs.mounted_inputs().empty() &&
              f.services.select().mounted().empty() && f.services.interactions().size() == 0 &&
              f.services.rounded_effects().live_count() == 0,
          "Card destruction leaked nested resources");
}

void narrow_empty_titles_and_typed_title() {
    Signal<String> title{String{}};
    int rich_runs{};
    Fixture f;
    f.services.mount(Content{[&] {
        Card(CardProps{}.title(title).layout(LayoutStyle{}.width(dp(100))), CardContent{[] { Text(u8"Body"); }});
        Card(CardProps{}.title(u8"Ignored text").layout(LayoutStyle{}.width(dp(24))),
             CardContent{[] { Text(u8"Body"); }},
             CardSlots{.title = CardTitle{[&] {
                           ++rich_runs;
                           Text(u8"Rich title");
                       }},
                       .extra = CardExtra{[] { Button(ButtonProps{}, [] { Text(u8"Extra"); }); }}});
    }});
    f.synchronize();
    const auto first = f.services.card().mounted()[0];
    const auto second = f.services.card().mounted()[1];
    check(!f.services.card().snapshot(first).has_header, "empty Card title reserved a header");
    title.set(String{u8"Long header that wraps within constrained width"});
    f.synchronize();
    check(f.services.card().snapshot(first).has_header &&
              f.services.card().snapshot(first).header_bounds.height >= 56 && bounds(f, first).width == 100 &&
              bounds(f, second).width == 24 && rich_runs == 1,
          "Card dynamic title/narrow layout failed");
    const auto rich = f.services.card().snapshot(second);
    check(bounds(f, rich.title).width >= 0 && bounds(f, rich.extra).width >= 0 &&
              bounds(f, rich.extra).x + bounds(f, rich.extra).width <= bounds(f, second).x + 24,
          "narrow Card header escaped available width");
    runtime::NodePropertyWriter writer{f.nodes, f.dirty};
    const auto node = f.services.components().root(first);
    writer.set_translation(node, {17, 21});
    f.synchronize();
    check(std::ranges::any_of(f.services.rounded_effects().packed_instances(),
                              [](const auto& effect) { return effect.geometry.translation == runtime::Point{17, 21}; }),
          "Card effect did not follow logical translation");
    title.set(String{});
    f.synchronize();
    check(!f.services.card().snapshot(first).has_header && rich_runs == 1, "Card empty title did not remove header");
}

void invalid_props_roll_back() {
    for (int mode = 0; mode < 2; ++mode) {
        Fixture f;
        bool rejected = false;
        try {
            f.services.mount(Content{[&] {
                CardProps props;
                if (mode == 0) {
                    props.size(static_cast<CardSize>(99));
                } else {
                    props.variant(static_cast<CardVariant>(99));
                }
                Card(props, CardContent{[] { Text(u8"Unreachable"); }});
            }});
        } catch (const std::invalid_argument&) {
            rejected = true;
        }
        check(rejected && f.services.card().mounted().empty() && f.nodes.size() == 0 &&
                  f.services.interactions().size() == 0,
              "invalid Card left partially mounted state");
    }
}

void loading_blur_can_retire_owner() {
    Signal<bool> loading{false};
    runtime::ComponentId owner;
    bool retire = false;
    Fixture f;
    f.services.mount(Content{[&] {
        Card(CardProps{}.loading(loading), CardContent{[&] {
                 Input(InputProps{}.onBlur([&] {
                     if (retire) {
                         static_cast<void>(f.services.destroy(owner));
                     }
                 }));
             }});
    }});
    f.synchronize();
    owner = f.services.card().mounted().front();
    check(f.services.focus().request_focus(f.inputs.mounted_inputs()[0].interaction, FocusModality::keyboard),
          "Card blur-retirement focus failed");
    retire = true;
    loading.set(true);
    f.synchronize();
    check(f.services.card().mounted().empty() && f.inputs.mounted_inputs().empty() && f.nodes.size() == 0 &&
              f.services.interactions().size() == 0 && !f.inputs.sessions().active().valid(),
          "Card loading callback retirement retained resources");
}
} // namespace

int main() {
#if defined(_MSC_VER) && defined(_DEBUG)
    _CrtSetReportMode(_CRT_ASSERT, _CRTDBG_MODE_FILE);
    _CrtSetReportFile(_CRT_ASSERT, _CRTDBG_FILE_STDERR);
#endif
    try {
        regions_loading_and_nested_controls();
        narrow_empty_titles_and_typed_title();
        invalid_props_roll_back();
        loading_blur_can_retire_owner();
    } catch (const std::exception& error) {
        std::cerr << error.what() << '\n';
        return 1;
    }
    return 0;
}
