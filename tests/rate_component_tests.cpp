#include "support/rate_fixture.hpp"
#include <limits>

namespace {
using namespace rate_test;

void events() {
    Fixture f;
    Signal<float> authority{2};
    Signal<bool> half{true};
    Signal<bool> clear{true};
    Signal<int> count{5};
    Signal<bool> keyboard{true};
    std::vector<float> changes;
    std::vector<std::optional<float>> hovers;
    f.services.mount(Content{[&] {
        Rate(RateProps{}
                 .value(authority)
                 .allowHalf(half)
                 .allowClear(clear)
                 .count(count)
                 .keyboard(keyboard)
                 .onChange([&](float value) { changes.push_back(value); })
                 .onHoverChange([&](auto value) { hovers.push_back(value); }));
    }});
    sync(f);
    const auto id = f.services.rate().mounted()[0];
    const auto original = f.services.rate().snapshot(id);
    pointer(f, original.items[2], PointerAction::move);
    sync(f);
    check(f.services.rate().snapshot(id).hover == 2.5F && hovers.back() == 2.5F, "left-half hover missing");
    pointer(f, original.items[2], PointerAction::move, .75F);
    check(f.services.rate().snapshot(id).hover == 3, "right-half hover missing");
    click(f, original.items[2]);
    check(changes == std::vector<float>{2.5F} && f.services.rate().snapshot(id).value == 2 &&
              !f.services.rate().snapshot(id).hover && !hovers.back(),
          "controlled refusal fabricated authority/preview");
    authority.set(2.5F);
    sync(f);
    click(f, original.items[2]);
    check(changes.back() == 0 && f.services.rate().snapshot(id).value == 2.5F, "allowClear candidate missing");
    clear.set(false);
    const auto n = changes.size();
    click(f, original.items[2]);
    check(changes.size() == n, "unchanged value emitted misleading onChange");
    count.set(2);
    sync(f);
    check(f.services.rate().snapshot(id).value == 2 && changes.size() == n, "count shrink emitted change");
    count.set(5);
    sync(f);
    check(f.services.rate().snapshot(id).value == 2.5F &&
              f.services.rate().snapshot(id).items[2].component == original.items[2].component,
          "count restoration lost original authority or retained identity");
    half.set(false);
    sync(f);
    check(f.services.rate().snapshot(id).value == 3, "allowHalf change did not normalize");
    bool rejected = false;
    try {
        authority.set(std::numeric_limits<float>::quiet_NaN());
    } catch (const std::invalid_argument&) {
        rejected = true;
    }
    check(rejected && f.services.rate().snapshot(id).value == 3, "NaN partially published");
    authority.set(1);
    sync(f);
    key(f, Key::tab);
    check(f.services.focus().state().focused == original.focus, "Rate missing single Tab entry");
    std::size_t stops = 0;
    for (auto hit : f.services.interactions().declaration_order()) {
        const auto& r = f.services.interactions().require(hit);
        stops += r.eligible && r.focusable && r.tab_stop && f.services.interactions().branch_active(hit);
    }
    check(stops == 1, "Rate added per-star Tab stops");
    key(f, Key::right);
    check(changes.back() == 2 && f.services.rate().snapshot(id).value == 1, "keyboard controlled rejection failed");
    key(f, Key::end);
    check(changes.back() == 5, "End missing");
    key(f, Key::home);
    check(changes.back() == 0, "Home missing");
    keyboard.set(false);
    sync(f);
    check(!f.services.focus().state().focused, "keyboard=false retained focus");
    click(f, original.items[3], .75F);
    check(changes.back() == 4, "keyboard=false also disabled pointer");
}

void scenes() {
    Fixture f;
    Signal<RateCharacter> character{RateCharacter{IconName::StarFilled}};
    Signal<ControlSize> size{ControlSize::Middle};
    Signal<LogicalLength> width{dp(132)};
    Signal<LogicalLength> height{dp(20)};
    f.services.mount(Content{[&] {
        Rate(RateProps{}.defaultValue(2.5F).allowHalf().character(character).size(size).layout(
            LayoutStyle{}.width(width).height(height)));
    }});
    sync(f);
    const auto id = f.services.rate().mounted()[0];
    const auto original = f.services.rate().snapshot(id);
    const auto root = f.services.components().root(id);
    auto half = original.items[2];
    auto clip = bounds(f, half.foreground);
    check(clip.width == 10 && clip.height == 20 && glyphs(f, half.foreground, clip) > 0,
          "half icon has no actual clipped glyphs");
    check(glyphs(f, original.items[3].foreground, bounds(f, original.items[3].foreground)) == 0,
          "unselected item has visible foreground");
    for (const auto& candidate : {RateCharacter{u8"W"}, RateCharacter{IconName::HeartFilled}}) {
        character.set(candidate);
        for (auto control_size : {ControlSize::Small, ControlSize::Middle, ControlSize::Large}) {
            size.set(control_size);
            width.set(dp(61));
            height.set(dp(30));
            sync(f);
            check(glyphs(f, root, bounds(f, root)) > 0, "finite-width Rate lost actual glyphs");
            height.set(dp(7));
            const runtime::Rect document{2, 1, 56, 5};
            sync(f, document);
            const auto allowed = graphics::intersect_effect_bounds(bounds(f, root), document);
            static_cast<void>(glyphs(f, root, allowed));
            check(!f.services.hit_test().hit_test({62, 3}), "Rate hit escaped finite width/document clip");
            check(!f.services.hit_test().hit_test({4, 7}), "Rate hit escaped short height/document clip");
            width.set(dp(0));
            height.set(dp(0));
            sync(f);
            check(glyphs(f, root, {0, 0, 0, 0}) == 0 && !f.services.hit_test().hit_test({1, 1}),
                  "zero Rate leaked scene/hit");
            width.set(dp(132));
            height.set(dp(30));
            sync(f);
            check(glyphs(f, root, bounds(f, root)) > 0 &&
                      f.services.rate().snapshot(id).items[2].component == half.component,
                  "zero-size restore lost glyphs/identity");
        }
    }
}

void uncontrolled_tooltips() {
    Fixture f;
    Signal<bool> read_only{false};
    f.services.mount(Content{[&] {
        Rate(RateProps{}.defaultValue(1).allowHalf().readOnly(read_only).tooltips({u8"Poor", u8"Good", u8"Great"}));
    }});
    sync(f);
    const auto id = f.services.rate().mounted()[0];
    const auto rows = f.services.rate().snapshot(id).items;
    pointer(f, rows[1], PointerAction::move, .75F);
    sync(f);
    check(f.services.tooltip().snapshot(f.services.tooltip().mounted()[1]).visible, "per-item Tooltip did not open");
    click(f, rows[1], .75F);
    check(f.services.rate().snapshot(id).value == 2, "uncontrolled click failed");
    key(f, Key::left);
    check(f.services.rate().snapshot(id).value == 1.5F, "half keyboard step failed");
    key(f, Key::space);
    check(f.services.rate().snapshot(id).value == 0, "Space allowClear failed");
    key(f, Key::right);
    key(f, Key::enter);
    check(f.services.rate().snapshot(id).value == 0, "Enter allowClear failed");
    read_only.set(true);
    sync(f);
    click(f, rows[2]);
    check(f.services.rate().snapshot(id).value == 0 && !f.services.rate().snapshot(id).hover, "readOnly edited value");
    pointer(f, rows[1], PointerAction::move);
    sync(f);
    check(f.services.tooltip().snapshot(f.services.tooltip().mounted()[1]).visible,
          "readOnly swallowed native Tooltip");
}

void popup_scene() {
    Fixture f;
    InputRef ref;
    f.services.mount(Content{[&] {
        Rate(RateProps{}.readOnly().tooltips({RateTooltip{TooltipTitle{[&] {
            Flex(FlexProps{}.vertical(true), [&] {
                Text(TypographyProps{}.content(u8"Popup mark").mark(true));
                Input(InputProps{}.defaultValue(u8"Native child").ref(ref));
            });
        }}}}));
    }});
    const runtime::Rect document{0, 0, 20, 8};
    sync(f, document);
    const auto id = f.services.rate().mounted()[0];
    f.services.pointer().dispatch({PointerIdentity::mouse(), PointerAction::move, PointerButton::none, 5, 4});
    sync(f, document);
    const auto popup = f.services.tooltip().snapshot(f.services.tooltip().mounted()[0]);
    check(popup.visible && popup.bounds.y >= document.height, "Tooltip lost independent window layer");
    std::size_t visible = 0;
    for (const auto& text : f.services.text().mounted_texts()) {
        if (f.scene.text_state(text.scene).content() == String{u8"Popup mark"}.view()) {
            visible += glyphs(f, f.services.components().root(text.component), popup.bounds);
        }
    }
    check(visible > 0, "rich Tooltip has no actual visible glyphs");
    const auto marker = f.services.components().theme_scope(id)->snapshot().typography().colors.mark_background;
    std::size_t quads = 0;
    for (const auto& q : f.services.surfaces().instances().instances()) {
        if (q.opacity <= 0 || q.color != std::array{marker.red(), marker.green(), marker.blue(), marker.alpha()}) {
            continue;
        }
        ++quads;
        const runtime::Rect b{q.bounds[0] + q.translation[0], q.bounds[1] + q.translation[1], q.bounds[2], q.bounds[3]};
        check(b.x >= popup.bounds.x && b.y >= popup.bounds.y &&
                  b.x + b.width <= popup.bounds.x + popup.bounds.width + .01F &&
                  b.y + b.height <= popup.bounds.y + popup.bounds.height + .01F,
              "native title quad escaped popup");
    }
    check(quads > 0, "rich Tooltip has no actual quad content");
    const auto input = f.inputs.mounted_inputs()[0];
    const auto b = f.inputs.layout_snapshot(input.component).viewport;
    const auto hit = f.services.hit_test().hit_test({b.x + 5, b.y + b.height / 2});
    check(hit == input.interaction, "readOnly/document clip swallowed native child hit in window layer");
    f.services.pointer().dispatch(
        {PointerIdentity::mouse(), PointerAction::down, PointerButton::primary, b.x + 5, b.y + b.height / 2});
    f.services.pointer().dispatch(
        {PointerIdentity::mouse(), PointerAction::up, PointerButton::primary, b.x + 5, b.y + b.height / 2});
    check(f.services.focus().state().focused == input.interaction, "native child pointer could not focus");
}
} // namespace

int main() {
    try {
        events();
        scenes();
        uncontrolled_tooltips();
        popup_scene();
        std::cout << "Rate events, actual scene and Tooltip passed\n";
        return 0;
    } catch (const std::exception& e) {
        std::cerr << e.what() << '\n';
        return 1;
    }
}
