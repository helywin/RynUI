#include "component/tag_badge_component.hpp"
#include "component/form_component.hpp"
#include "component/popover_component.hpp"
#include "support/timeline_fixture.hpp"
#include <iostream>
using namespace timeline_test;

namespace {
int checks{};

void expect(bool value, const char* message) {
    ++checks;
    check(value, message);
}

void tick(Fixture& f, std::int64_t us) {
    f.services.tick_animations(animation::AnimationTime::microseconds(us));
    sync(f);
}

void numeric() {
    Fixture f;
    f.services.set_motion_preference(animation::MotionPreference::normal);
    Signal<int> count{8};
    Signal<int> overflow{99};
    Signal<bool> visible{true};
    Signal<Status> status{Status::Processing};
    int actions{};
    int content_builds{};
    f.services.mount(Content{[&] {
        Badge(BadgeProps{}.count(count).overflowCount(overflow).showZero(), BadgeContent{[&] {
                  ++content_builds;
                  Button(ButtonProps{}.onClick([&] { ++actions; }), [] { Text(u8"Inbox"); });
              }});
        Badge(BadgeProps{}.status(status).visible(visible).text(u8"Synthetic processing"));
    }});
    sync(f);
    const auto id = f.services.tag_badge().badges()[0];
    const auto busy = f.services.tag_badge().badges()[1];
    count.set(12);
    sync(f);
    expect(f.services.tag_badge().badge_snapshot(id).number_motion == 0, "Numeric Badge did not start finite roll");
    const auto duration =
        animation::resolve_motion_tokens(f.services.components().theme_scope(id)->snapshot()).fast.count_microseconds();
    tick(f, duration / 2);
    const auto mid = f.services.tag_badge().badge_snapshot(id);
    expect(mid.number_motion > .1F && mid.number_motion < .9F, "Badge midpoint jumped to target");
    const auto children = f.services.components().children(mid.indicator);
    const auto current = bounds(f, children.front());
    const auto old = bounds(f, mid.previous_count);
    expect(current.y > mid.indicator_bounds.y && old.y < mid.indicator_bounds.y,
           "Badge did not paint separate intermediate rolling digit geometry");
    expect(f.services.components().branch_active(mid.previous_count), "Previous digits were hidden during roll");
    count.set(3);
    sync(f);
    tick(f, duration);
    expect(f.services.tag_badge().badge_snapshot(id).number_motion < 1, "Reverse/new target skipped finite motion");
    tick(f, duration * 3);
    expect(f.services.tag_badge().badge_snapshot(id).display_count == String{u8"3"},
           "Badge reverse lost accepted value");
    count.set(0);
    sync(f);
    tick(f, duration * 5);
    expect(f.services.tag_badge().badge_snapshot(id).display_count == String{u8"0"}, "Badge zero restoration failed");
    count.set(5);
    overflow.set(0);
    sync(f);
    expect(f.services.tag_badge().badge_snapshot(id).display_count == String{u8"0+"},
           "Zero overflow range did not clamp numeric Badge");
    overflow.set(99);
    sync(f);
    expect(f.services.tag_badge().badge_snapshot(id).display_count == String{u8"5"},
           "Restoring Badge range lost accepted numeric target");
    expect(f.services.tag_badge().badge_snapshot(busy).processing_phase > 0 && f.services.next_frame_deadline(),
           "Visible processing Badge did not animate");
    visible.set(false);
    sync(f);
    tick(f, duration * 7);
    expect(!f.services.next_frame_deadline(), "Hidden processing Badge kept an idle deadline");
    static_cast<void>(f.frames.consume_request());
    for (int i = 1; i <= 16; ++i) {
        expect(f.services.tick_animations(animation::AnimationTime::microseconds(duration * 7 + i)) == 0,
               "Hidden Badge produced idle animation work");
    }
    expect(!f.frames.pending(), "Hidden Badge requested idle frames");
    visible.set(true);
    sync(f);
    tick(f, duration * 8);
    expect(f.services.next_frame_deadline().has_value(), "Processing Badge failed to resume");
    f.services.set_window_active(false);
    sync(f);
    expect(!f.services.next_frame_deadline(), "Inactive Badge kept animation deadline");
    f.services.set_window_active(true);
    f.services.set_motion_preference(animation::MotionPreference::reduced);
    sync(f);
    expect(!f.services.next_frame_deadline(), "Reduced motion Badge kept timer");
    click(f, f.buttons.mounted_buttons()[0].component);
    expect(actions == 1 && content_builds == 1, "Badge rebuilt or blocked independent child action");
    f.services.dispose();
    expect(!f.services.next_frame_deadline(), "Badge timer survived retirement");
}

void rich_and_ribbon() {
    Fixture f;
    InputRef ref;
    int builds{};
    int replacement_builds{};
    Signal<BadgeCountValue> count{BadgeCountValue{String{u8"NEW"}}};
    Signal<bool> rtl{false};
    Signal<RibbonPlacement> placement{RibbonPlacement::End};
    auto renderer = std::make_shared<BadgeRenderer>();
    renderer->render = [&] {
        ++builds;
        Input(InputProps{}.ref(ref).defaultValue(u8"draft"));
    };
    Signal<std::shared_ptr<const BadgeRenderer>> factory{renderer};
    f.services.mount(Content{[&] {
        Badge(BadgeProps{}.countValue(count).contentRender(factory));
        BadgeRibbon(RibbonProps{}.text(u8"Native ribbon").placement(placement).rtl(rtl), BadgeContent{[] {
                        Button(ButtonProps{}.layout(LayoutStyle{}.width(dp(160)).height(dp(80))),
                               [] { Text(u8"Body"); });
                    }});
    }});
    sync(f);
    const auto id = f.services.tag_badge().badges()[0];
    const auto ribbon = f.services.tag_badge().badges()[1];
    expect(f.services.tag_badge().badge_snapshot(id).display_count == String{u8"NEW"} && builds == 1 && ref.bound(),
           "Arbitrary Badge count or child renderer missing");
    expect(ref.focus() && ref.select(1, 3), "Rich Badge editor did not focus/select");
    const auto input = f.inputs.mounted_inputs().front().editor;
    const auto draft = std::string(f.inputs.editors().require(input).value());
    count.set(BadgeCountValue{String{u8"任意状态"}});
    sync(f);
    expect(ref.bound() && builds == 1 && f.inputs.editors().require(input).value() == draft,
           "Badge count update rebuilt editor");
    count.set(BadgeCountValue{-3.5});
    sync(f);
    expect(f.services.tag_badge().badge_snapshot(id).display_count == String{u8"-3.5"},
           "Typed fractional/negative Badge count failed");
    count.set(BadgeCountValue{String{u8"999999999999999999999999"}});
    sync(f);
    expect(f.services.tag_badge().badge_snapshot(id).display_count == String{u8"99+"},
           "Numeric String Badge overflow failed");
    auto count_renderer = std::make_shared<BadgeRenderer>();
    count_renderer->render = [] {
        Icon(IconProps{}.name(IconName::ClockCircleOutlined));
    };
    count.set(BadgeCountValue{std::shared_ptr<const BadgeRenderer>{count_renderer}});
    sync(f);
    expect(f.services.tag_badge().badge_snapshot(id).shown && ref.bound(),
           "Typed rich count did not render independently");
    InputRef count_ref;
    auto input_count = std::make_shared<BadgeRenderer>();
    input_count->render = [&] {
        Input(InputProps{}
                  .ref(count_ref)
                  .defaultValue(u8"rich count")
                  .layout(LayoutStyle{}.width(dp(80)).height(dp(32)))
                  .onChange([&](String) { count.set(BadgeCountValue{String{u8"Done"}}); }));
    };
    count.set(BadgeCountValue{std::shared_ptr<const BadgeRenderer>{input_count}});
    sync(f);
    expect(f.services.tag_badge().badge_snapshot(id).indicator_bounds.height >= 32 &&
               count_ref.focus({InputFocusCursor::All}),
           "Rich count did not measure/hit its real native geometry");
    static_cast<void>(f.inputs.dispatch(TextCommitted{String{u8"replace"}, f.inputs.sessions().active()}));
    sync(f);
    expect(!count_ref.bound() && ref.bound() &&
               f.services.tag_badge().badge_snapshot(id).display_count == String{u8"Done"},
           "Reentrant count renderer replacement lost independent child or owner");
    const auto end = f.services.tag_badge().badge_snapshot(ribbon).indicator_bounds;
    rtl.set(true);
    sync(f);
    const auto start = f.services.tag_badge().badge_snapshot(ribbon).indicator_bounds;
    expect(start.x < end.x && f.services.tag_badge().badge_snapshot(ribbon).ribbon,
           "Ribbon did not mirror logical placement");
    placement.set(RibbonPlacement::Start);
    sync(f);
    expect(f.services.tag_badge().badge_snapshot(ribbon).indicator_bounds.x == end.x,
           "Ribbon start/RTL placement failed");
    auto replacement = std::make_shared<BadgeRenderer>();
    replacement->render = [&] {
        ++replacement_builds;
        Text(u8"Replacement");
    };
    factory.set(replacement);
    sync(f);
    expect(!ref.bound() && replacement_builds == 1 && f.services.components().contains(ribbon),
           "Local Badge factory replacement leaked Input or rebuilt sibling");
    auto bad = std::make_shared<BadgeRenderer>();
    bad->render = [] {
        throw std::runtime_error("synthetic renderer failure");
    };
    bool threw = false;
    try {
        factory.set(bad);
    } catch (const std::runtime_error&) {
        threw = true;
    }
    expect(threw && !f.services.components().contains(id) && f.services.components().contains(ribbon),
           "Badge factory failure did not retire exactly its owner");
    f.services.dispose();
    expect(!f.services.next_frame_deadline(), "Ribbon cleanup leaked");
}
} // namespace

int main() {
    try {
        numeric();
        rich_and_ribbon();
        std::cout << checks << " Badge native checks passed\n";
        return 0;
    } catch (const std::exception& e) {
        std::cerr << e.what() << '\n';
        return 1;
    }
}
