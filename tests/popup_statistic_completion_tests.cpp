#include "component/statistic_component.hpp"
#include "component/tooltip_component.hpp"
#include "support/popover_fixture.hpp"
#include <string_view>

namespace {
using namespace ryn_test::popover;

void press(Fixture& f, runtime::Rect rect, PointerButton button) {
    for (const auto action : {PointerAction::down, PointerAction::up}) {
        f.services.pointer().dispatch(
            {PointerIdentity::mouse(), action, button, rect.x + rect.width / 2, rect.y + rect.height / 2});
    }
    sync(f);
}

void context() {
    Fixture f;
    Signal<TextDirection> direction{TextDirection::LeftToRight};
    Signal<bool> open{false};
    int mounts{};
    int native_clicks{};
    f.services.mount(Content{[&] {
        Popover(
            PopoverProps{}
                .title(u8"Context")
                .open(open)
                .direction(direction)
                .triggers(PopoverTriggers{true, true, true, true})
                .placement(PopoverPlacement::BottomLeft)
                .onOpenChange([open](bool value) { open.set(value); }),
            PopoverTrigger{[&] { Button(ButtonProps{}.onClick([&] { ++native_clicks; }), [] { Text(u8"Entry"); }); }},
            {.content = PopoverContent{[&] {
                 ++mounts;
                 Text(u8"Retained body");
             }}});
    }});
    sync(f);
    const auto id = f.services.popover().mounted()[0];
    const auto original = f.services.popover().snapshot(id);
    const auto entry = bounds(f, trigger(f, id));
    f.services.pointer().dispatch({PointerIdentity::mouse(), PointerAction::up, PointerButton::secondary,
                                   entry.x + entry.width / 2, entry.y + entry.height / 2});
    sync(f);
    check(!open.get(), "orphan secondary release opened Popover without an owned press");
    press(f, entry, PointerButton::secondary);
    auto view = f.services.popover().snapshot(id);
    check(view.geometry.visible && view.geometry.anchor.width == 0 &&
              view.geometry.anchor.x == entry.x + entry.width / 2,
          "Popover context menu did not use the actual pointer anchor");
    direction.set(TextDirection::RightToLeft);
    sync(f);
    check(f.services.popover().snapshot(id).geometry.placement == PopoverPlacement::BottomRight,
          "Popover RTL did not mirror logical corner placement");
    key(f, Key::escape);
    press(f, entry, PointerButton::primary);
    check(open.get() && native_clicks == 1 && mounts == 1 &&
              f.services.popover().snapshot(id).content == original.content,
          "composed triggers lost Button activation or remounted content");
    check(detail::directional_popup_placement(TooltipPlacement::LeftTop, TextDirection::RightToLeft) ==
              TooltipPlacement::RightTop,
          "Tooltip RTL side mapping is inconsistent");
    ThemeConfig wireframe;
    wireframe.seed.wireframe = true;
    const auto token = resolve_theme(wireframe).popover();
    check(token.padding == 0 && token.title_gap == 0 && token.title_border_width > 0 &&
              token.content_padding_inline > 0 && token.title_padding_top > 0,
          "Popover wireframe did not derive native title/content geometry");
    Fixture tooltip;
    tooltip.services.mount(Content{[] {
        Tooltip(TooltipProps{}.title(String{u8"Context"}).trigger(TooltipTriggerMode::ContextMenu),
                TooltipTrigger{[] { Text(u8"Tooltip entry"); }});
    }});
    sync(tooltip);
    const auto tooltip_id = tooltip.services.tooltip().mounted()[0];
    const auto tooltip_entry = bounds(tooltip, tooltip_id);
    tooltip.services.pointer().dispatch({PointerIdentity::mouse(), PointerAction::up, PointerButton::secondary,
                                         tooltip_entry.x + 1, tooltip_entry.y + 1});
    sync(tooltip);
    check(!tooltip.services.tooltip().snapshot(tooltip_id).visible, "orphan secondary release opened Tooltip");
    press(tooltip, tooltip_entry, PointerButton::secondary);
    check(tooltip.services.tooltip().snapshot(tooltip_id).visible &&
              tooltip.services.tooltip().snapshot(tooltip_id).anchor.width == 0,
          "Tooltip lost a real secondary press because PointerRouter only owns primary presses");
}

void scroll() {
    Fixture f;
    InputRef draft;
    int saved{};
    f.services.mount(Content{[&] {
        Popover(
            PopoverProps{}.defaultOpen().title(u8"Scrollable").placement(PopoverPlacement::BottomLeft),
            PopoverTrigger{[] { Text(u8"Anchor"); }},
            {.content = PopoverContent{[&] {
                 Flex(
                     FlexProps{}.vertical(true).align(FlexAlign::Start).gap(dp(6)).layout(LayoutStyle{}.width(dp(180))),
                     FlexContent{[&] {
                         for (int i = 0; i < 20; ++i) {
                             Text(u8"Content row");
                         }
                         Input(InputProps{}.ref(draft).defaultValue(u8"Retained draft"));
                         Button(ButtonProps{}.onClick([&] { ++saved; }), [] { Text(u8"Save"); });
                     }});
             }}});
    }});
    sync(f, 320, 220);
    const auto id = f.services.popover().mounted()[0];
    const auto view = f.services.popover().snapshot(id);
    const auto input = f.inputs.mounted_inputs()[0];
    check(view.scroll_extent > 0, "Popover content was clipped without a scroll extent");
    check(f.services.dispatch_scroll({0, -100, view.geometry.bounds.x + 1, view.geometry.bounds.y + 1}),
          "Popover did not own the fallback wheel");
    sync(f, 320, 220);
    const auto end = f.services.popover().snapshot(id);
    check(end.scroll_offset == end.scroll_extent && end.scroll_offset > 0 && draft.focus(),
          "Popover scroll did not make its bottom editor reachable");
    const auto button = f.buttons.mounted_buttons().back();
    const auto target = bounds(f, button.component);
    check(contains(end.geometry.bounds, target), "bottom action did not enter the popup clip");
    for (auto action : {PointerAction::down, PointerAction::up}) {
        f.services.pointer().dispatch({PointerIdentity::mouse(), action, PointerButton::primary,
                                       target.x + target.width / 2, target.y + target.height / 2});
    }
    sync(f, 320, 220);
    check(saved == 1 && f.inputs.mounted_inputs()[0].editor == input.editor,
          "scroll moved paint without moving input or replaced the editor");
    check(f.services.destroy(id), "scroll popup failed to destroy");
    check(!draft.focus() && !f.services.next_frame_deadline(), "scroll popup leaked input or a deadline");
}

void popup_motion() {
    for (const bool tooltip : {false, true}) {
        Fixture f;
        f.services.set_motion_preference(animation::MotionPreference::normal);
        Signal<bool> open{false};
        f.services.mount(Content{[&] {
            if (tooltip) {
                Tooltip(TooltipProps{}
                            .title(String{u8"Animated"})
                            .open(open)
                            .animated()
                            .trigger(TooltipTriggerMode::Manual),
                        TooltipTrigger{[] { Text(u8"Anchor"); }});
            } else {
                Popover(PopoverProps{}.content(u8"Animated").open(open).animated().trigger(PopoverTriggerMode::Manual),
                        PopoverTrigger{[] { Text(u8"Anchor"); }});
            }
        }});
        sync(f);
        const auto id = tooltip ? f.services.tooltip().mounted()[0] : f.services.popover().mounted()[0];
        const auto popup = tooltip ? f.services.components().children(id)[1] : f.services.popover().snapshot(id).popup;
        const auto node = f.services.components().root(popup);
        const auto duration = animation::resolve_motion_tokens(f.services.components().theme_scope(id)->snapshot())
                                  .fast.count_microseconds();
        check(duration > 1, "popup animation fixture has no finite motion duration");
        open.set(true);
        sync(f);
        tick(f, duration / 2);
        check(f.nodes.require(node).paint_opacity > 0 && f.nodes.require(node).paint_opacity < 1 &&
                  f.services.next_frame_deadline().has_value(),
              "popup enter did not animate retained paint");
        open.set(false);
        sync(f);
        check(!f.services.interactions().component_active(popup) && f.services.components().branch_active(popup),
              "popup exit did not separate immediate input cleanup from retained paint");
        tick(f, duration / 2 + duration);
        check(!f.services.components().branch_active(popup) && !f.services.next_frame_deadline(),
              "popup exit retained a branch or idle deadline");
        open.set(true);
        sync(f);
        f.services.set_motion_preference(animation::MotionPreference::reduced);
        sync(f);
        check(f.nodes.require(node).paint_opacity == 1 && !f.services.next_frame_deadline(),
              "popup reduced motion did not settle and stop scheduling");
    }
}

void locale() {
    Fixture f;
    Signal<StatisticLocale> locale{StatisticLocale{}};
    f.services.mount(Content{[&] {
        Statistic(StatisticProps{}.value(StatisticValue{u8"1234567.25"}).locale(locale));
        Statistic(StatisticProps{}.value(1234.5).locale(locale).decimalSeparator(u8"/").groupSeparator(u8"_"));
    }});
    sync(f);
    const auto id = f.services.statistic().mounted()[0];
    const auto original = f.services.statistic().snapshot(id);
    auto next = locale.get();
    next.group_separator = u8".";
    next.decimal_separator = u8",";
    next.secondary_group = 2;
    locale.set(next);
    sync(f);
    check(f.services.statistic().snapshot(id).formatted_value == String{u8"12.34.567,25"} &&
              f.services.statistic().snapshot(id).value == original.value,
          "locale update did not retain value identity and secondary grouping");
    check(f.services.statistic().snapshot(f.services.statistic().mounted()[1]).formatted_value == String{u8"1_234/5"},
          "locale overrode explicit separators");
    // Duration stores float milliseconds: keep the day and subsecond inputs exactly representable.
    check(detail::format_statistic_timer(Duration::milliseconds(90061000), String{u8"D HH:mm:ss [ms]"}, next) ==
              String{u8"1 01:01:01 ms"},
          "Timer format did not consume units or preserve escaped text");
    check(detail::format_statistic_timer(Duration::milliseconds(3661007), String{u8"D HH:mm:ss.SSS [ms]"}, next) ==
              String{u8"0 01:01:01.007 ms"},
          "Timer format lost millisecond precision or zero-filled units");
    check(detail::statistic_timer_quantum(String{u8"HH:mm [sSSS]"}) == 60000 &&
              detail::statistic_timer_quantum(String{u8"[YMDHmsS]"}) == 0,
          "escaped Timer units introduced a sampling deadline");
    bool rejected{};
    auto invalid = next;
    invalid.primary_group = 0;
    try {
        locale.set(invalid);
    } catch (const std::invalid_argument&) {
        rejected = true;
    }
    check(rejected && f.services.statistic().snapshot(id).formatted_value == String{u8"12.34.567,25"},
          "invalid locale published a partial value");
    locale.set(next);
    check(!f.services.next_frame_deadline(), "static locale introduced a next frame");
    next.digits = {u8"０", u8"１", u8"２", u8"３", u8"４", u8"５", u8"６", u8"７", u8"８", u8"９"};
    locale.set(next);
    sync(f);
    check(f.services.statistic().snapshot(id).formatted_value == String{u8"１２.３４.５６７,２５"},
          "locale digits did not reach actual retained numeric text");
}

void timer() {
    Fixture f;
    f.services.set_motion_preference(animation::MotionPreference::normal);
    Signal<std::optional<StatisticTimer>> timer{
        StatisticTimer{StatisticTimerType::Countdown, Duration::seconds(5), String{u8"ss"}}};
    Signal<bool> visible{true};
    Signal<bool> loading{false};
    Signal<LogicalLength> margin{dp(0)};
    int finishes{};
    int changes{};
    f.services.mount(Content{[&] {
        Statistic(StatisticProps{}
                      .timer(timer)
                      .visible(visible)
                      .loading(loading)
                      .layout(LayoutStyle{}.margin_top(margin))
                      .onChange([&](Duration) { ++changes; })
                      .onFinish([&] { ++finishes; }));
    }});
    sync(f);
    const auto id = f.services.statistic().mounted()[0];
    check(f.services.next_frame_deadline().has_value(), "live Timer did not schedule a deadline");
    tick(f, 1000000);
    check(f.services.statistic().snapshot(id).formatted_value == String{u8"04"} && changes == 1,
          "Timer did not advance from the monotonic window clock");
    visible.set(false);
    sync(f);
    check(!f.services.next_frame_deadline(), "hidden Timer kept a deadline");
    tick(f, 3000000);
    check(changes == 1 && finishes == 0, "hidden Timer delivered callbacks");
    visible.set(true);
    sync(f);
    check(f.services.statistic().snapshot(id).formatted_value == String{u8"02"}, "restored Timer failed to catch up");
    loading.set(true);
    sync(f);
    check(!f.services.next_frame_deadline(), "loading Timer kept scheduling hidden numeric content");
    loading.set(false);
    margin.set(dp(900));
    sync(f);
    check(!f.services.next_frame_deadline(), "off-page Timer kept scheduling outside its clip");
    margin.set(dp(0));
    sync(f);
    check(bounds(f, id).height > 0, "Timer margin reentry retained a constrained zero-height measurement");
    f.services.set_window_active(false);
    check(!f.services.next_frame_deadline(), "inactive Timer kept scheduling");
    f.services.set_window_active(true);
    sync(f);
    f.services.set_motion_preference(animation::MotionPreference::reduced);
    check(!f.services.next_frame_deadline(), "reduced-motion Timer kept scheduling");
    f.services.set_motion_preference(animation::MotionPreference::normal);
    sync(f);
    tick(f, 6000000);
    tick(f, 7000000);
    check(finishes == 1 && f.services.statistic().snapshot(id).formatted_value == String{u8"00"} &&
              !f.services.next_frame_deadline(),
          "countdown did not finish once and stop");
    timer.set(StatisticTimer{StatisticTimerType::Countup, Duration::seconds(2), String{u8"ss"}});
    sync(f);
    tick(f, 8000000);
    check(finishes == 1 && f.services.statistic().snapshot(id).formatted_value == String{u8"03"},
          "Countup failed to restart from its configured elapsed duration");
    timer.set(std::nullopt);
    sync(f);
    check(!f.services.next_frame_deadline() && f.services.statistic().snapshot(id).formatted_value == String{u8"0"},
          "removing Timer failed to restore static Statistic");
    tick(f, 7000000);
    timer.set(StatisticTimer{StatisticTimerType::Countup, Duration::seconds(2), String{u8"ss"}});
    sync(f);
    check(f.services.statistic().snapshot(id).formatted_value == String{u8"02"},
          "Timer reconfiguration used a regressed raw clock as its origin");
    tick(f, 9000000);
    check(f.services.statistic().snapshot(id).formatted_value == String{u8"03"},
          "Timer reconfiguration did not advance from the clamped monotonic origin");
}

void statistic_motion() {
    Fixture f;
    f.services.set_motion_preference(animation::MotionPreference::normal);
    Signal<StatisticValue> value{StatisticValue{0}};
    Signal<bool> visible{true};
    f.services.mount(
        Content{[&] { Statistic(StatisticProps{}.value(value).animated().visible(visible).precision(0)); }});
    sync(f);
    const auto id = f.services.statistic().mounted()[0];
    value.set(StatisticValue{100});
    sync(f);
    check(f.services.statistic().snapshot(id).animating && f.services.next_frame_deadline(),
          "numeric update did not start an animation");
    tick(f, 100000);
    const auto middle = f.services.statistic().snapshot(id).formatted_value;
    check(middle != String{u8"0"} && middle != String{u8"100"},
          "numeric animation did not publish an intermediate value");
    visible.set(false);
    sync(f);
    check(!f.services.next_frame_deadline() && f.services.statistic().snapshot(id).formatted_value == String{u8"100"},
          "hidden numeric animation did not settle to the authoritative target");
    visible.set(true);
    sync(f);
    value.set(StatisticValue{std::uint64_t{18446744073709551615ULL}});
    sync(f);
    check(f.services.statistic().snapshot(id).formatted_value == String{u8"18,446,744,073,709,551,615"} &&
              !f.services.next_frame_deadline(),
          "animation rounded a large integral target through double");
    Fixture redirected;
    redirected.services.set_motion_preference(animation::MotionPreference::normal);
    Signal<StatisticValue> target{StatisticValue{0}};
    redirected.services.mount(Content{[&] { Statistic(StatisticProps{}.value(target).animated().precision(0)); }});
    sync(redirected);
    const auto redirected_id = redirected.services.statistic().mounted()[0];
    target.set(StatisticValue{100});
    tick(redirected, 100000);
    const auto displayed = redirected.services.statistic().snapshot(redirected_id).formatted_value;
    tick(redirected, 50000);
    target.set(StatisticValue{200});
    sync(redirected);
    check(redirected.services.statistic().snapshot(redirected_id).formatted_value == displayed &&
              redirected.services.statistic().snapshot(redirected_id).animating,
          "redirected numeric animation jumped because its origin used a regressed raw clock");
    tick(redirected, 500000);
    check(redirected.services.statistic().snapshot(redirected_id).formatted_value == String{u8"200"} &&
              !redirected.services.next_frame_deadline(),
          "redirected numeric animation did not finish at its authoritative target");
}

void callback() {
    for (const bool tooltip : {false, true}) {
        Fixture mounting;
        mounting.services.mount(Content{[&] {
            const auto retire = [&] {
                mounting.services.destroy(mounting.services.components().root_components().front());
            };
            if (tooltip) {
                Tooltip(TooltipProps{}, TooltipTrigger{[] { Text(u8"Trigger"); }}, TooltipTitle{retire});
            } else {
                Popover(PopoverProps{}, PopoverTrigger{[] { Text(u8"Trigger"); }}, {.content = PopoverContent{retire}});
            }
            Text(u8"Fallback");
        }});
        sync(mounting);
        check(mounting.services.popover().mounted().empty() && mounting.services.tooltip().mounted().empty() &&
                  mounting.services.surfaces().size() == 0 && mounting.services.interactions().size() == 0,
              "popup rich mount used retired owner state or leaked private input/paint");
    }
    Fixture f;
    f.services.set_motion_preference(animation::MotionPreference::normal);
    runtime::ComponentId owner;
    int finished{};
    f.services.mount(Content{[&] {
        Statistic(StatisticProps{}
                      .timer(StatisticTimer{StatisticTimerType::Countdown, Duration::milliseconds(1), String{u8"SSS"}})
                      .onChange([&](Duration) { f.services.destroy(owner); })
                      .onFinish([&] { ++finished; }));
    }});
    sync(f);
    owner = f.services.statistic().mounted()[0];
    tick(f, 2000);
    check(!f.services.components().contains(owner) && finished == 0 && !f.services.next_frame_deadline(),
          "Timer callback used a retired generation or invoked stale finish");
    Fixture rebound;
    rebound.services.set_motion_preference(animation::MotionPreference::normal);
    Signal<std::optional<StatisticTimer>> timer{
        StatisticTimer{StatisticTimerType::Countdown, Duration::milliseconds(1), String{u8"SSS"}}};
    rebound.services.mount(Content{[&] {
        Statistic(StatisticProps{}
                      .timer(timer)
                      .onChange([timer](Duration duration) {
                          if (duration == Duration{}) {
                              timer.set(
                                  StatisticTimer{StatisticTimerType::Countup, Duration::seconds(2), String{u8"ss"}});
                          }
                      })
                      .onFinish([&] { ++finished; }));
    }});
    sync(rebound);
    tick(rebound, 2000);
    check(finished == 0 && rebound.services.next_frame_deadline(), "old finish survived a reentrant timer replacement");
    tick(rebound, 1002000);
    const auto replacement = rebound.services.statistic().mounted()[0];
    check(rebound.services.statistic().snapshot(replacement).formatted_value == String{u8"03"},
          "reentrant timer replacement lost its monotonic origin");
    Fixture failed;
    failed.services.set_motion_preference(animation::MotionPreference::normal);
    failed.services.mount(Content{[] {
        Statistic(StatisticProps{}
                      .timer(StatisticTimer{StatisticTimerType::Countdown, Duration{}, String{u8"ss"}})
                      .onFinish([] { throw std::runtime_error("finish failure"); }));
    }});
    sync(failed);
    bool threw{};
    try {
        tick(failed, 2000);
    } catch (const std::runtime_error&) {
        threw = true;
    }
    check(threw && failed.services.statistic().mounted().empty() && !failed.services.next_frame_deadline() &&
              failed.services.surfaces().size() == 0,
          "throwing Timer callback failed to retire its owned UI");
}
} // namespace

int main(int argc, char** argv) {
    try {
        const std::string_view name = argc > 1 ? argv[1] : "";
        if (name == "context") {
            context();
        } else if (name == "scroll") {
            scroll();
        } else if (name == "popup_motion") {
            popup_motion();
        } else if (name == "locale") {
            locale();
        } else if (name == "timer") {
            timer();
        } else if (name == "statistic_motion") {
            statistic_motion();
        } else if (name == "callback") {
            callback();
        } else {
            throw std::invalid_argument("unknown completion test case");
        }
    } catch (const std::exception& e) {
        std::cerr << e.what() << '\n';
        return 1;
    }
}
