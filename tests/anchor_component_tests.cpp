#include "component/anchor_component.hpp"
#include "support/navigation_fixture.hpp"
#include <iostream>

namespace {
using namespace ryn_test::navigation;

runtime::ComponentId button(Fixture& f, runtime::ComponentId row) {
    for (const auto& b : f.buttons.mounted_buttons()) {
        if (descendant(f, b.component, row)) {
            return b.component;
        }
    }
    throw std::runtime_error("Anchor button missing");
}

void scrolling_and_motion() {
    Fixture f;
    f.services.set_motion_preference(animation::MotionPreference::normal);
    Signal<AnchorScrollState> scroll{AnchorScrollState{0, 1200, 300}};
    Signal<bool> affix{false};
    Signal<float> target{30};
    Signal<AnchorDirection> direction{AnchorDirection::Vertical};
    std::vector<float> requests;
    std::vector<String> changes;
    int clicks{};
    f.services.mount(Content{[&] {
        Anchor(AnchorProps{}
                   .scroll(scroll)
                   .affix(affix)
                   .targetOffset(target)
                   .direction(direction)
                   .onScrollTo([&](float offset) {
                       requests.push_back(offset);
                       auto s = scroll.get();
                       s.offset = offset;
                       scroll.set(s);
                   })
                   .onChange([&](String key) { changes.push_back(key); })
                   .onClick([&](AnchorClickEvent&) { ++clicks; })
                   .items(std::vector<AnchorItem>{
                       AnchorItem{u8"a", u8"Alpha", 0}.children({AnchorItem{u8"b", u8"Beta", 200}.targetOffset(20)}),
                       {u8"c", u8"Gamma", 1100}}));
    }});
    sync(f);
    const auto id = f.services.anchor().mounted()[0];
    auto view = f.services.anchor().snapshot(id);
    check(view.active == String{u8"a"} && view.keys.size() == 3, "initial Anchor tracking failed");
    auto value = scroll.get();
    value.offset = 176;
    scroll.set(value);
    sync(f);
    check(f.services.anchor().snapshot(id).active == String{u8"b"}, "per-item bounds/scroll tracking failed");
    click(f, button(f, view.items[2]));
    check(clicks == 1 && f.services.anchor().snapshot(id).animating, "Anchor click did not start motion");
    f.services.tick_animations(animation::AnimationTime::microseconds(225000));
    sync(f);
    check(requests.size() == 1 && requests.back() > 176 && requests.back() < 900, "Anchor midpoint invalid");
    click(f, button(f, view.items[2]));
    check(clicks == 2 && requests.size() == 1 && f.services.anchor().snapshot(id).animating,
          "same-target Anchor click duplicated its in-flight scroll");
    f.services.tick_animations(animation::AnimationTime::microseconds(100000));
    sync(f);
    check(requests.back() >= requests.front(), "Anchor time reversal moved backwards");
    f.services.tick_animations(animation::AnimationTime::microseconds(450000));
    sync(f);
    check(requests.back() == 900 && !f.services.anchor().snapshot(id).animating && !f.services.next_frame_deadline(),
          "Anchor bottom clamp or idle failed");
    direction.set(AnchorDirection::Horizontal);
    sync(f);
    check(!f.services.components().branch_active(view.items[1]), "horizontal Anchor exposed nested child");
    direction.set(AnchorDirection::Vertical);
    sync(f);
    check(f.services.components().branch_active(view.items[1]), "vertical Anchor did not restore child");
    f.services.set_motion_preference(animation::MotionPreference::reduced);
    click(f, button(f, view.items[0]));
    check(requests.back() == 0 && !f.services.anchor().snapshot(id).animating, "reduced Anchor motion did not snap");
    affix.set(true);
    sync(f);
    check(glyphs_inside(f, id, {0, 0, 640, 800}) > 0, "Anchor has no actual scene glyphs");
}

void topology_and_retention() {
    Fixture f;
    Signal<std::vector<AnchorItem>> items{std::vector<AnchorItem>{}};
    Signal<float> position{250};
    Signal<bool> shown{true};
    int mounts{};
    auto a = AnchorItem{u8"a", u8"A", 0}.richTitle(AnchorTitle{[&] {
        ++mounts;
        Input(InputProps{}.defaultValue(u8"draft"));
    }});
    auto b = AnchorItem{u8"b", u8"B", position}.visible(shown);
    items.set({a, b});
    f.services.mount(Content{
        [&] { Anchor(AnchorProps{}.items(items).scroll(AnchorScrollState{0, 1000, 200}).onScrollTo([](float) {})); }});
    sync(f);
    const auto id = f.services.anchor().mounted()[0];
    const auto original = f.services.anchor().snapshot(id);
    items.set({a, b, AnchorItem{u8"bad", u8"Bad", 500}.richTitle(AnchorTitle{[] {
                   throw std::runtime_error("slot failure");
               }})});
    bool slot_failed{};
    try {
        sync(f);
    } catch (const std::runtime_error&) {
        slot_failed = true;
    }
    check(slot_failed && f.services.anchor().snapshot(id).items == original.items,
          "Anchor throwing append corrupted retained keys");
    items.set({b, a, {u8"c", u8"C", 600}});
    sync(f);
    auto view = f.services.anchor().snapshot(id);
    check(view.items[1] == original.items[0] && view.items[0] == original.items[1] && mounts == 1,
          "Anchor reorder remounted retained content");
    position.set(400);
    shown.set(false);
    sync(f);
    check(!f.services.components().branch_active(view.items[0]), "Anchor visibility left active input");
    const auto input = f.inputs.mounted_inputs()[0];
    click(f, input.component);
    check(f.inputs.sessions().active().valid(), "Anchor rich Input cannot edit independently");
    items.set({b});
    sync(f);
    check(f.inputs.mounted_inputs().empty() && !f.inputs.sessions().active().valid() &&
              !f.services.focus().state().focused,
          "Anchor remove leaked input/focus");
    items.set({a, b});
    sync(f);
    check(mounts == 2, "Anchor removed slot restored stale native content");
}

void cancellation_and_failures() {
    for (int mode = 0; mode < 6; ++mode) {
        Fixture f;
        f.services.set_motion_preference(animation::MotionPreference::normal);
        Signal<bool> shown{true};
        Signal<bool> disabled{false};
        Signal<AnchorScrollState> scroll{AnchorScrollState{0, 1200, 300}};
        runtime::ComponentId id;
        int requests{};
        f.services.mount(Content{[&] {
            Anchor(AnchorProps{}
                       .scroll(scroll)
                       .visible(shown)
                       .disabled(disabled)
                       .items(std::vector<AnchorItem>{{u8"a", u8"A", 0}, {u8"b", u8"B", 600}})
                       .onClick([&](AnchorClickEvent& event) {
                           if (mode == 0) {
                               event.preventScroll();
                           }
                           if (mode == 1) {
                               f.services.destroy(id);
                           }
                       })
                       .onScrollTo([&](float) {
                           ++requests;
                           if (mode == 5) {
                               throw std::runtime_error("target failure");
                           }
                       }));
        }});
        sync(f);
        id = f.services.anchor().mounted()[0];
        const auto row = f.services.anchor().snapshot(id).items[1];
        click(f, button(f, row));
        if (mode == 0 || mode == 1) {
            check(!f.services.next_frame_deadline() && requests == 0, "prevent/destroy did not cancel Anchor");
            continue;
        }
        if (mode == 2) {
            shown.set(false);
            sync(f);
        }
        if (mode == 3) {
            disabled.set(true);
            sync(f);
        }
        if (mode == 4) {
            auto s = scroll.get();
            s.offset = 70;
            scroll.set(s);
            sync(f);
        }
        bool threw{};
        try {
            f.services.tick_animations(animation::AnimationTime::microseconds(50000));
        } catch (const std::runtime_error&) {
            threw = true;
        }
        check((mode == 5 ? threw && requests == 1 : requests == 0) && !f.services.next_frame_deadline(),
              "Anchor cancelled/throwing motion survived");
    }
    Fixture f;
    bool rejected{};
    try {
        f.services.mount(Content{[] {
            Anchor(AnchorProps{}
                       .onScrollTo([](float) {})
                       .items(std::vector<AnchorItem>{{u8"x", u8"X", 0}, {u8"x", u8"Y", 5}}));
        }});
    } catch (const std::invalid_argument&) {
        rejected = true;
    }
    check(rejected && f.nodes.size() == 0, "invalid Anchor topology partially mounted");
}

void theme_and_controlled() {
    Fixture f;
    Signal<String> active{String{u8"a"}};
    Signal<ThemeConfig> theme{ThemeConfig{}};
    std::vector<String> changes;
    bool accept{};
    f.services.mount(Content{[&] {
        Theme(ThemeProps{}.config(theme), ThemeContent{[&] {
                  Anchor(AnchorProps{}
                             .affix(false)
                             .activeKey(active)
                             .duration(Duration::milliseconds(0))
                             .scroll(AnchorScrollState{0, 1000, 200})
                             .items(std::vector<AnchorItem>{{u8"a", u8"A", 0}, {u8"b", u8"B", 300}})
                             .onScrollTo([](float) {})
                             .onChange([&](String v) {
                                 changes.push_back(v);
                                 if (accept) {
                                     active.set(v);
                                 }
                             }));
              }});
    }});
    sync(f);
    const auto id = f.services.anchor().mounted()[0];
    const auto view = f.services.anchor().snapshot(id);
    click(f, button(f, view.items[1]));
    check(f.services.anchor().snapshot(id).active == String{u8"a"} && changes.back() == String{u8"b"},
          "controlled Anchor committed rejected request");
    const auto requests = changes.size();
    click(f, button(f, view.items[1]));
    check(changes.size() == requests + 1 && f.services.anchor().snapshot(id).active == String{u8"a"},
          "controlled Anchor rejected retry was suppressed");
    accept = true;
    click(f, button(f, view.items[1]));
    check(changes.back() == String{u8"b"}, "controlled Anchor acknowledgement fed back an unrelated scroll request");
    sync(f);
    check(f.services.anchor().snapshot(id).active == String{u8"b"}, "controlled Anchor acknowledgement ignored");
    ThemeConfig t;
    t.anchor.tokens.current = Color::rgba8(250, 0, 0);
    t.anchor.tokens.indent = dp(30);
    theme.set(t);
    sync(f);
    check(f.services.anchor().snapshot(id).items == view.items, "Anchor Theme remounted slots");
    bool invalid{};
    try {
        ThemeConfig bad;
        bad.anchor.tokens.font_size = dp(0);
        static_cast<void>(resolve_theme(bad));
    } catch (const std::invalid_argument&) {
        invalid = true;
    }
    check(invalid, "Anchor accepted invalid typography");
}

void overflow_and_affix_geometry() {
    Fixture f;
    Signal<bool> affix{false};
    f.services.mount(Content{[&] {
        Anchor(AnchorProps{}
                   .affix(affix)
                   .offsetTop(12)
                   .scrollViewport(AnchorScrollViewport{200, 300})
                   .direction(AnchorDirection::Horizontal)
                   .layout(LayoutStyle{}.width(dp(160)))
                   .duration(Duration::milliseconds(0))
                   .scroll(AnchorScrollState{0, 2000, 300})
                   .onScrollTo([](float) {})
                   .items(std::vector<AnchorItem>{{u8"a", u8"Long first heading", 0},
                                                  {u8"b", u8"Long middle heading", 300},
                                                  {u8"c", u8"Long final heading", 600}}));
    }});
    sync(f);
    const auto id = f.services.anchor().mounted()[0];
    const auto view = f.services.anchor().snapshot(id);
    const auto last = button(f, view.items.back());
    check(f.services.focus().request_focus(interaction(f, last), FocusModality::keyboard),
          "overflow Anchor cannot receive keyboard focus");
    sync(f);
    const auto after = bounds(f, last);
    check(after.x >= 0 && after.x + after.width <= 160.01F, "Anchor did not reveal focused overflow item");
    check(f.services.dispatch_scroll({0, 1, after.x + after.width / 2, after.y + after.height / 2}),
          "Anchor wheel did not consume own overflow");
    sync(f);
    check(f.services.anchor().snapshot(id).items == view.items, "Anchor wheel remounted navigation");
    affix.set(true);
    check(f.services.layout_and_synchronize({640, 800}, {0, 0, 640, 800}, {0, -100}), "affixed Anchor layout failed");
    check(f.services.anchor().snapshot(id).affixed && bounds(f, id).y == -100,
          "Anchor affix lost its layout placeholder");
    check(std::abs(bounds(f, view.items[0]).y - 212) < .01F, "Anchor ignored explicit nested scroll viewport origin");
    check(glyphs_inside(f, id, {0, 200, 640, 300}) > 0, "affixed Anchor lost its visible glyphs");
}
} // namespace

int main() {
    try {
        overflow_and_affix_geometry();
        scrolling_and_motion();
        topology_and_retention();
        cancellation_and_failures();
        theme_and_controlled();
        std::cout << "Anchor component contracts passed (5 groups)\n";
        return 0;
    } catch (const std::exception& e) {
        std::cerr << e.what() << '\n';
        return 1;
    }
}
