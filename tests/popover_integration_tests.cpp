#include "support/popover_fixture.hpp"
#include "component/select_component.hpp"
#include "component/date_picker_component.hpp"
#include "component/time_picker_component.hpp"
#include "component/table_component.hpp"
#include "component/dropdown_component.hpp"
#include "graphics/rounded_effect.hpp"
#if defined(_MSC_VER) && defined(_DEBUG)
#include <crtdbg.h>
#endif

namespace {
using namespace ryn_test::popover;

std::vector<SelectOption> options() {
    std::vector<SelectOption> values;
    for (int i = 0; i < 30; ++i) {
        values.push_back({i, String{u8"Option"}});
    }
    return values;
}

void table() {
    std::vector<int> rows;
    for (int i = 0; i < 15; ++i) {
        rows.push_back(i);
    }
    Table(TableProps<int>{}
              .dataSource(rows)
              .rowKey([](int i) { return String::from_utf8(std::to_string(i)).value(); })
              .columns({TableColumn<int>{u8"value", u8"Value", [](TableCell<int>) { Text(u8"Cell"); }}.width(500)})
              .scrollY(65)
              .layout(LayoutStyle{}.width(dp(280))));
}

void wheel() {
    auto fixture = std::make_unique<Fixture>();
    auto& f = *fixture;
    const auto sync = [](Fixture& current) {
        ryn_test::popover::sync(current, 900, 900);
    };
    Signal<bool> select_open{false};
    Signal<bool> time_open{false};
    Signal<bool> date_open{false};
    auto theme = std::make_unique<ThemeConfig>();
    theme->time_picker.tokens.column_height = dp(84);
    f.services.mount(Content{[&] {
        Theme(ThemeProps{}.config(*theme), ThemeContent{[&] {
                  Popover(
                      PopoverProps{}.defaultOpen().placement(PopoverPlacement::BottomLeft),
                      PopoverTrigger{[] { Button(ButtonProps{}, [] { Text(u8"Entry"); }); }},
                      {.content = PopoverContent{[&] {
                           Flex(FlexProps{}.vertical(true).gap(dp(8)), FlexContent{[&] {
                                    table();
                                    Select(SelectProps{}
                                               .options(options())
                                               .open(select_open)
                                               .listHeight(70)
                                               .layout(LayoutStyle{}.width(dp(220))));
                                    DatePicker(DatePickerProps{}.open(date_open).layout(LayoutStyle{}.width(dp(220))),
                                               DatePickerFooter{[] { table(); }});
                                    TimePicker(TimePickerProps{}.open(time_open).layout(LayoutStyle{}.width(dp(220))),
                                               TimePickerFooter{[] { table(); }});
                                }});
                       }}});
                  table();
              }});
    }});
    sync(f);
    const auto owner = f.services.popover().mounted()[0];
    const auto parent = f.services.popover().snapshot(owner);
    const auto child_table = f.services.table().mounted()[0];
    const auto sibling = f.services.table().mounted().back();
    const auto original = f.services.table().snapshot(child_table);
    check(original.maximum_y > 0 && original.body.height > 0, "child Table fixture not scrollable");
    check(f.services.dispatch_scroll({0, -1, original.body.x + 10, original.body.y + 10}),
          "child Table wheel unhandled");
    sync(f);
    const auto scroll = f.services.table().snapshot(child_table).scroll_y;
    check(scroll > 0 && f.services.table().snapshot(sibling).scroll_y == 0, "Popover swallowed Table wheel");
    select_open.set(true);
    sync(f);
    const auto select = f.services.select().mounted()[0];
    auto s = f.services.select().snapshot(select);
    check(s.visible && s.popup.height > 0, "child Select popup missing");
    check(f.services.dispatch_scroll({0, -2, s.popup.x + 5, s.popup.y + 5}), "Select wheel unhandled");
    sync(f);
    check(f.services.select().snapshot(select).scroll > s.scroll &&
              f.services.table().snapshot(child_table).scroll_y == scroll &&
              f.services.table().snapshot(sibling).scroll_y == 0,
          "Select lost nearest popup ownership");
    select_open.set(false);
    time_open.set(true);
    sync(f);
    const auto time = f.services.time_picker().mounted()[0];
    auto t = f.services.time_picker().snapshot(time);
    const auto column = t.column_clips[0];
    check(t.visible && column.height > 0, "child Time popup missing");
    check(f.services.dispatch_scroll({0, -2, column.x + 5, column.y + 5}), "Time wheel unhandled");
    sync(f);
    check(f.services.time_picker().snapshot(time).scroll[0] > t.scroll[0] &&
              f.services.table().snapshot(child_table).scroll_y == scroll,
          "Time column wheel stolen");
    const auto time_table = f.services.table().mounted()[2];
    const auto footer = f.services.table().snapshot(time_table);
    check(footer.body.height > 0 && footer.maximum_y > 0, "Time footer Table missing");
    check(f.services.dispatch_scroll({0, -1, footer.body.x + 10, footer.body.y + 10}), "Time footer wheel");
    sync(f);
    check(f.services.table().snapshot(time_table).scroll_y > 0, "Time footer Table swallowed");
    time_open.set(false);
    date_open.set(true);
    sync(f);
    const auto date = f.services.date_picker().mounted()[0];
    const auto d = f.services.date_picker().snapshot(date);
    check(d.visible, "Date popup missing");
    const auto date_table = f.services.table().mounted()[1];
    const auto df = f.services.table().snapshot(date_table);
    check(df.body.height > 0 && f.services.dispatch_scroll({0, -1, df.body.x + 10, df.body.y + 10}),
          "Date footer wheel");
    sync(f);
    check(f.services.table().snapshot(date_table).scroll_y > 0, "Date footer Table swallowed");
    date_open.set(false);
    sync(f);
    check(f.services.dispatch_scroll({0, -1, parent.geometry.bounds.x + 1, parent.geometry.bounds.y + 1}),
          "padding fallback missing");
    check(f.services.table().snapshot(sibling).scroll_y == 0, "padding leaked into sibling");
}

void children() {
    auto fixture = std::make_unique<Fixture>();
    auto& f = *fixture;
    Signal<bool> select_open{false};
    Signal<bool> time_open{false};
    Signal<bool> date_open{false};
    SelectRef select_ref;
    TimePickerRef time_ref;
    DatePickerRef date_ref;
    f.services.mount(Content{[&] {
        Popover(PopoverProps{}.defaultOpen().trigger(PopoverTriggerMode::Click),
                PopoverTrigger{[] { Button(ButtonProps{}, [] { Text(u8"Parent"); }); }},
                {.content = PopoverContent{[&] {
                     Flex(FlexProps{}.vertical(true).gap(dp(8)), FlexContent{[&] {
                              Select(SelectProps{}
                                         .open(select_open)
                                         .ref(select_ref)
                                         .options(options())
                                         .onOpenChange([&](bool value) { select_open.set(value); }));
                              DatePicker(DatePickerProps{}.open(date_open).ref(date_ref).onOpenChange(
                                  [&](bool value) { date_open.set(value); }));
                              TimePicker(TimePickerProps{}.open(time_open).ref(time_ref).onOpenChange(
                                  [&](bool value) { time_open.set(value); }));
                              Popover(PopoverProps{}.content(u8"Nested").trigger(PopoverTriggerMode::Click),
                                      PopoverTrigger{[] { Button(ButtonProps{}, [] { Text(u8"Nested entry"); }); }});
                          }});
                 }}});
    }});
    sync(f);
    const auto parent = f.services.popover().mounted().back();
    const auto child = f.services.popover().mounted().front();
    click(f, trigger(f, child));
    check(f.services.popover().snapshot(child).geometry.visible, "nested click failed");
    key(f, Key::escape);
    check(!f.services.popover().snapshot(child).geometry.visible &&
              f.services.popover().snapshot(parent).geometry.visible,
          "nested Escape closed parent");
    check(time_ref.focus(), "Time focus");
    time_open.set(true);
    sync(f);
    key(f, Key::escape);
    check(!f.services.time_picker().snapshot(f.services.time_picker().mounted()[0]).visible &&
              f.services.popover().snapshot(parent).geometry.visible,
          "Time Escape did not precede parent");
    check(date_ref.focus(), "Date focus");
    date_open.set(true);
    sync(f);
    key(f, Key::escape);
    check(!f.services.date_picker().snapshot(f.services.date_picker().mounted()[0]).visible &&
              f.services.popover().snapshot(parent).geometry.visible,
          "Date Escape did not precede parent");
    check(select_ref.focus(), "Select focus");
    select_open.set(true);
    sync(f);
    key(f, Key::escape);
    check(!f.services.select().snapshot(f.services.select().mounted()[0]).visible &&
              f.services.popover().snapshot(parent).geometry.visible,
          "Select Escape did not precede parent");
}

void covered() {
    auto fixture = std::make_unique<Fixture>();
    auto& f = *fixture;
    f.services.mount(Content{[] {
        Select(SelectProps{}.defaultOpen(true).options(options()).listHeight(100).layout(LayoutStyle{}.width(dp(280))));
        Popover(PopoverProps{}.defaultOpen().placement(PopoverPlacement::TopLeft).content(u8"Cover"),
                PopoverTrigger{[] { Button(ButtonProps{}, [] { Text(u8"Top"); }); }});
        table();
    }});
    sync(f, 400, 240);
    const auto popover = f.services.popover().mounted()[0];
    const auto popup = f.services.popover().snapshot(popover);
    const auto select = f.services.select().mounted()[0];
    const auto before = f.services.select().snapshot(select);
    // Give the Popover explicit higher order and test an actually overlapping hit.
    auto config = std::make_unique<ThemeConfig>();
    config->popover.tokens.z_index_popup = 1200;
    f.services.components().theme_scope(popover)->update(*config);
    sync(f, 400, 240);
    const auto p = f.services.popover().snapshot(popover).geometry.bounds;
    const float x = std::max(p.x, before.popup.x) + 5;
    const float y = std::max(p.y, before.popup.y) + 5;
    check(x < std::min(p.x + p.width, before.popup.x + before.popup.width) &&
              y < std::min(p.y + p.height, before.popup.y + before.popup.height),
          "covered fixture lacks overlap");
    const auto hit = f.services.hit_test().hit_test({x, y});
    check(hit && descendant(f, f.services.interactions().require(*hit).component, popup.popup),
          "Popover not topmost hit");
    check(f.services.dispatch_scroll({0, -1, x, y}), "topmost fallback missing");
    sync(f, 400, 240);
    check(f.services.select().snapshot(select).scroll == before.scroll &&
              f.services.table().snapshot(f.services.table().mounted()[0]).scroll_y == 0,
          "covered sibling scrolled through Popover");
}

void clips() {
    auto fixture = std::make_unique<Fixture>();
    auto& f = *fixture;
    Signal<PopoverPlacement> placement{PopoverPlacement::TopLeft};
    Signal<bool> adjust{false};
    ButtonRef ref;
    f.services.mount(Content{[&] {
        Popover(PopoverProps{}
                    .defaultOpen()
                    .placement(placement)
                    .autoAdjustOverflow(adjust)
                    .title(u8"汉 Title")
                    .layout(LayoutStyle{}.margin_top(dp(60))),
                PopoverTrigger{[] { Text(u8"Anchor"); }},
                {.content = PopoverContent{[&] {
                     Flex(FlexProps{}.vertical(true), FlexContent{[&] {
                              Button(ButtonProps{}.ref(ref), [] { Text(u8"Button"); });
                              Text(TypographyProps{}.content(u8"W\u2060W\u2060W\u2060W\u2060W\u2060W").mark(true));
                          }});
                 }}});
    }});
    std::size_t visible_glyphs{};
    std::size_t visible_marks{};
    for (const auto extent :
         {runtime::Size{120, 70}, runtime::Size{20, 10}, runtime::Size{1, 1}, runtime::Size{400, 300}}) {
        sync(f, extent.width, extent.height);
        const auto owner = f.services.popover().mounted()[0];
        const auto s = f.services.popover().snapshot(owner);
        const runtime::Rect viewport{0, 0, extent.width, extent.height};
        const auto effective = graphics::intersect_effect_bounds(s.geometry.bounds, viewport);
        std::size_t glyphs{};
        for (const auto& text : f.services.text().mounted_texts()) {
            if (!descendant(f, text.component, s.popup)) {
                continue;
            }
            const auto range = f.scene.primitive(text.scene).instances;
            for (std::uint32_t i = 0; i < range.count; ++i) {
                const auto& g = f.scene.glyph_scene().instances().at(range.first + i);
                const auto& p = g.position_size;
                const auto& c = g.clip_bounds;
                const auto& t = g.translation_opacity;
                const auto ink = graphics::intersect_effect_bounds({p[0] + t[0], p[1] + t[1], p[2], p[3]},
                                                                   {c[0], c[1], c[2] - c[0], c[3] - c[1]});
                if (ink.width > 0 && ink.height > 0 && t[2] > 0 && g.color[3] > 0) {
                    ++glyphs;
                    check(contains(effective, ink), "glyph escaped popup viewport");
                }
            }
        }
        if (extent.width == 400) {
            // Turn adjustment on for a visible focus-ring fixture.
            adjust.set(true);
            placement.set(PopoverPlacement::BottomLeft);
            sync(f, 400, 300);
            check(ref.focus(), "focus decoration fixture failed");
            sync(f, 400, 300);
            const auto button = f.buttons.mounted_buttons()[0];
            const auto b = bounds(f, button.component);
            const runtime::Point point{b.x - 2.5F, b.y + b.height / 2};
            const auto coverage = [&] {
                bool result{};
                for (const auto& e : f.services.rounded_effects().packed_instances()) {
                    if (e.geometry.kind == graphics::RoundedEffectKind::outline && e.geometry.outline_width > 1.5F &&
                        e.material.visible && e.material.opacity > 0) {
                        const auto clip = e.geometry.ancestor_clip;
                        const bool inside = !clip || (point.x >= clip->bounds.x && point.y >= clip->bounds.y &&
                                                      point.x < clip->bounds.x + clip->bounds.width &&
                                                      point.y < clip->bounds.y + clip->bounds.height);
                        result |= inside && graphics::rounded_effect_coverage(point, e) > .05F;
                    }
                }
                return result;
            };
            check(coverage(), "external focus decoration clipped to zero");
            const auto external = f.services.components().root(s.content);
            f.nodes.require(external).clip_content = true;
            f.nodes.require(external).clip_decorations = true;
            f.dirty.invalidate(external, runtime::DirtyFlags::Geometry | runtime::DirtyFlags::HitTest);
            sync(f, 400, 300);
            check(!coverage(), "focus decoration escaped effective external clip");
            f.nodes.require(external).clip_content = false;
            f.dirty.invalidate(external, runtime::DirtyFlags::Geometry | runtime::DirtyFlags::HitTest);
            sync(f, 400, 300);
            check(coverage(), "focus decoration did not recover after clip removal");
        }
        static_cast<void>(glyphs);
        visible_glyphs += glyphs;
        const auto mark = f.services.components().theme_scope(owner)->snapshot().typography().colors.mark_background;
        for (const auto& quad : f.services.surfaces().instances().instances()) {
            if (quad.color != std::array{mark.red(), mark.green(), mark.blue(), mark.alpha()} || quad.opacity <= 0) {
                continue;
            }
            const runtime::Rect ink{quad.bounds[0] + quad.translation[0], quad.bounds[1] + quad.translation[1],
                                    quad.bounds[2], quad.bounds[3]};
            if (ink.width > 0 && ink.height > 0) {
                ++visible_marks;
                // The final iteration adjusts the popup below its trigger.
                const auto current =
                    graphics::intersect_effect_bounds(f.services.popover().snapshot(owner).geometry.bounds, viewport);
                check(contains(current, ink), "actual mark quad escaped popup viewport");
            }
        }
        const auto hit = f.services.hit_test().hit_test({-1, 1});
        check(!hit, "hit escaped window clip");
    }
    check(visible_glyphs > 0 && visible_marks > 0, "clip test published no actual visible glyphs/quads");
}
} // namespace

int main(int argc, char** argv) {
#if defined(_MSC_VER) && defined(_DEBUG)
    _CrtSetReportMode(_CRT_ASSERT, _CRTDBG_MODE_FILE);
    _CrtSetReportFile(_CRT_ASSERT, _CRTDBG_FILE_STDERR);
#endif
    try {
        const std::string name = argc > 1 ? argv[1] : "wheel";
        if (name == "wheel") {
            wheel();
        } else if (name == "children") {
            children();
        } else if (name == "covered") {
            covered();
        } else if (name == "clips") {
            clips();
        } else {
            throw std::runtime_error("unknown case");
        }
    } catch (const std::exception& e) {
        std::cerr << e.what() << '\n';
        return 1;
    }
}
