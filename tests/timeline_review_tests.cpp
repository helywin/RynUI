#include "support/timeline_fixture.hpp"
#include <set>
#include <string_view>
#if defined(_MSC_VER) && defined(_DEBUG)
#include <crtdbg.h>
#endif

namespace {
using namespace timeline_test;
using Rects = std::set<std::array<float, 4>>;

bool same_color(const std::array<float, 4>& value, Color color) {
    return value == std::array{color.red(), color.green(), color.blue(), color.alpha()};
}

float coverage(Fixture& f, Color color, runtime::Point point) {
    float result = 0;
    for (const auto& command : f.services.scene_composer().ordered_scene().commands()) {
        for (auto i = command.first_instance; i < command.first_instance + command.instance_count; ++i) {
            if (command.kind == graphics::SceneDrawKind::quad) {
                const auto& q = f.services.surfaces().instances().at(i);
                if (same_color(q.color, color) && q.opacity > 0) {
                    const runtime::Rect rect{q.bounds[0] + q.translation[0], q.bounds[1] + q.translation[1],
                                             q.bounds[2], q.bounds[3]};
                    if (contains(rect, point)) {
                        result = std::max(result, q.opacity);
                    }
                }
            } else if (command.kind == graphics::SceneDrawKind::rounded_effect) {
                const auto& e = f.services.rounded_effects().packed_instances()[i];
                if (e.material.color == color && e.material.visible && e.material.opacity > 0 &&
                    (!e.geometry.ancestor_clip || contains(e.geometry.ancestor_clip->bounds, point))) {
                    result = std::max(result, graphics::rounded_effect_coverage(point, e) * e.material.opacity);
                }
            }
        }
    }
    return result;
}

Rects actual_tail(Fixture& f, Color color) {
    Rects result;
    for (const auto& command : f.services.scene_composer().ordered_scene().commands()) {
        for (auto i = command.first_instance; i < command.first_instance + command.instance_count; ++i) {
            if (command.kind == graphics::SceneDrawKind::quad) {
                const auto& q = f.services.surfaces().instances().at(i);
                if (same_color(q.color, color) && q.opacity > 0 && q.bounds[2] > 0 && q.bounds[3] > 0) {
                    result.insert(
                        {q.bounds[0] + q.translation[0], q.bounds[1] + q.translation[1], q.bounds[2], q.bounds[3]});
                }
            } else if (command.kind == graphics::SceneDrawKind::rounded_effect) {
                const auto& e = f.services.rounded_effects().packed_instances()[i];
                if (e.material.color == color && e.material.visible && e.material.opacity > 0 &&
                    e.geometry.kind == graphics::RoundedEffectKind::outer_shadow && e.geometry.blur == 0 &&
                    (!e.geometry.ancestor_clip ||
                     (e.geometry.ancestor_clip->bounds.width > 0 && e.geometry.ancestor_clip->bounds.height > 0))) {
                    const auto& r = e.geometry.shape.rect;
                    result.insert({r.x + e.geometry.translation.x, r.y + e.geometry.translation.y, r.width, r.height});
                }
            }
        }
    }
    return result;
}

void expect_dashes(Fixture& f, runtime::ComponentId owner, runtime::Rect clip) {
    const auto parts = f.services.timeline().snapshot(owner);
    const auto& token = f.services.components().theme_scope(owner)->snapshot().timeline();
    Rects expected;
    for (const auto entry : parts.order) {
        const auto& item = parts.items[entry.item];
        const auto b = bounds(f, item.viewport);
        const auto& tail = item.tail;
        const float dash = std::max(2.0F, token.metrics.tail_width * 2);
        if (tail.width <= 0) {
            continue;
        }
        for (double y = b.y + tail.y; y < static_cast<double>(b.y) + tail.y + tail.height; y += dash * 2) {
            const auto rect = graphics::intersect_effect_bounds(
                {b.x + tail.x, static_cast<float>(y), tail.width,
                 static_cast<float>(std::min<double>(dash, static_cast<double>(b.y) + tail.y + tail.height - y))},
                clip);
            if (rect.width > 0 && rect.height > 0) {
                expected.insert({rect.x, rect.y, rect.width, rect.height});
            }
        }
    }
    const auto actual = actual_tail(f, token.colors.tail);
    std::cout << "dash actual=" << actual.size() << " expected=" << expected.size() << '\n';
    check(actual == expected, "actual ordered dash count/endpoints differ from complete visible trajectory");
}

void clipped_document_fill() {
    Fixture f;
    f.services.mount(Content{[] {
        Timeline(TimelineProps{}.layout(LayoutStyle{}.width(dp(64)).height(dp(32))),
                 {TimelineItem{u8"a", u8"Content"}.tone(TimelineTone::Gray).richTitle(TimelineTitle{[] {
                     Button(ButtonProps{}.type(ButtonType::Primary).wave(false),
                            [] { Text(u8"A naturally wide action"); });
                 }})});
    }});
    sync(f);
    const auto owner = f.services.timeline().mounted()[0];
    const auto region = f.services.timeline().snapshot(owner).items[0].title;
    const auto r = bounds(f, region);
    const auto button = bounds(f, f.buttons.mounted_buttons()[0].component);
    const auto primary = f.services.components().theme_scope(owner)->snapshot().button().primary_background;
    check(button.width > 64 && r.width > 0 && r.width < 16 && button.height == 32,
          "overflow repro lacks natural Button dimensions");
    bool seen = false;
    for (float x = .5F; x < 64; x += 1) {
        const runtime::Point p{x, 16};
        const float value = coverage(f, primary, p);
        if (contains(r, p)) {
            seen |= value > .9F;
        } else {
            check(value < .01F, "ordinary document Primary Button fill crossed Timeline region");
        }
    }
    check(seen, "Primary Button fill was removed instead of clipped");
    Fixture plain;
    plain.services.mount(
        Content{[] { Button(ButtonProps{}.type(ButtonType::Primary).wave(false), [] { Text(u8"Plain"); }); }});
    sync(plain);
    bool raw = false;
    for (const auto& command : plain.services.scene_composer().ordered_scene().commands()) {
        raw |= command.kind == graphics::SceneDrawKind::quad;
    }
    check(raw, "normal un-clipped document fill lost original quad path");
}

void adjacent_loading() {
    Fixture f;
    Signal<bool> reverse{false};
    Signal<bool> shown{true};
    f.services.mount(Content{[&] {
        Timeline(TimelineProps{}.reverse(reverse),
                 {TimelineItem{u8"a", u8"A"}, TimelineItem{u8"b", u8"B"}.loading().visible(shown)});
    }});
    sync(f);
    const auto owner = f.services.timeline().mounted()[0];
    expect_dashes(f, owner, {0, 0, 600, 800});
    reverse.set(true);
    sync(f);
    expect_dashes(f, owner, {0, 0, 600, 800});
    shown.set(false);
    sync(f);
    check(actual_tail(f, f.services.components().theme_scope(owner)->snapshot().timeline().colors.tail).empty(),
          "hidden/final item retained a tail");
    shown.set(true);
    sync(f);
    expect_dashes(f, owner, {0, 0, 600, 800});
}

void large_capacity() {
    Fixture f;
    auto config = std::make_shared<ThemeConfig>();
    config->timeline.tokens.item_padding_bottom = dp(40000);
    Signal<ThemeConfig> theme{*config};
    Signal<bool> reverse{false};
    Signal<bool> shown{true};
    f.services.mount(Content{[&] {
        Theme(ThemeProps{}.config(theme), ThemeContent{[&] {
                  Timeline(TimelineProps{}.reverse(reverse).visible(shown),
                           {TimelineItem{u8"a", u8"A"}.loading(), TimelineItem{u8"b", u8"B"}.loading()});
              }});
    }});
    const auto sync_large = [&](runtime::Rect clip) {
        check(f.services.layout_and_synchronize({600, 50000}, clip), "large layout failed");
    };
    sync_large({0, 0, 600, 50000});
    const auto owner = f.services.timeline().mounted()[0];
    const auto large_records = f.services.surfaces().size();
    expect_dashes(f, owner, {0, 0, 600, 50000});
    check(actual_tail(f, f.services.components().theme_scope(owner)->snapshot().timeline().colors.tail).size() >
              component::retained_content_visual_capacity,
          "capacity repro did not exceed one retained range");
    sync_large({0, 0, 600, 100});
    const auto small_records = f.services.surfaces().size();
    expect_dashes(f, owner, {0, 0, 600, 100});
    check(small_records < large_records, "clipped shrink retained surplus ranges");
    sync_large({0, 0, 600, 50000});
    check(f.services.surfaces().size() == large_records, "range restoration drifted");
    reverse.set(true);
    sync_large({0, 0, 600, 50000});
    expect_dashes(f, owner, {0, 0, 600, 50000});
    shown.set(false);
    sync_large({0, 0, 600, 50000});
    check(f.services.surfaces().size() < small_records &&
              actual_tail(f, f.services.components().theme_scope(owner)->snapshot().timeline().colors.tail).empty(),
          "hidden Timeline retained dynamic tail ranges");
    shown.set(true);
    sync_large({0, 0, 600, 50000});
    expect_dashes(f, owner, {0, 0, 600, 50000});
    config->timeline.tokens.tail_width = dp(0);
    theme.set(*config);
    sync_large({0, 0, 600, 50000});
    check(actual_tail(f, f.services.components().theme_scope(owner)->snapshot().timeline().colors.tail).empty(),
          "zero-width tail produced visuals");
    config->timeline.tokens.tail_width = dp(2);
    theme.set(*config);
    sync_large({0, 0, 600, 50000});
    expect_dashes(f, owner, {0, 0, 600, 50000});
    f.services.dispose();
    check(f.services.surfaces().size() == 0 && f.services.rounded_effects().live_count() == 0,
          "tail range/effect teardown leaked");
}
} // namespace

int main(int argc, char** argv) {
#if defined(_MSC_VER) && defined(_DEBUG)
    _CrtSetReportMode(_CRT_ASSERT, _CRTDBG_MODE_FILE);
    _CrtSetReportFile(_CRT_ASSERT, _CRTDBG_FILE_STDERR);
#endif
    try {
        const auto which = argc > 1 ? std::string_view{argv[1]} : std::string_view{};
        if (which == "fill") {
            clipped_document_fill();
        }
        if (which == "loading") {
            adjacent_loading();
        }
        if (which == "capacity") {
            large_capacity();
        }
        return 0;
    } catch (const std::exception& e) {
        std::cerr << e.what() << '\n';
        return 1;
    }
}
