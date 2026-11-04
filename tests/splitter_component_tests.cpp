#include "component/splitter_component.hpp"
#include "component/splitter_model.hpp"
#include "support/input_fixture.hpp"

#include <array>
#include <cmath>
#include <iostream>
#include <limits>
#include <numeric>
#include <string_view>

namespace {
using namespace ryn;
using namespace ryn::input;
using Fixture = ryn_test::input_component::Fixture;

void check(bool value, const char* message) {
    if (!value) {
        throw std::runtime_error(message);
    }
}

void near(float value, float expected, const char* message) {
    check(std::abs(value - expected) < 0.04F, message);
}

void sync(Fixture& f, float width = 600, float height = 240) {
    check(f.services.layout_and_synchronize({width, height}, {0, 0, width, height}), "Splitter synchronization failed");
}

runtime::Rect bounds(Fixture& f, runtime::ComponentId id) {
    return f.nodes.require(f.services.components().root(id)).bounds;
}

runtime::ComponentId owner(Fixture& f) {
    return f.services.splitter().mounted().back();
}

SplitterPanel plain(const char8_t* label) {
    return SplitterPanel{SplitterPanelContent{
        [label] { Text(TextProps{}.content(String::from_utf8(reinterpret_cast<const char*>(label)).value())); }}};
}

runtime::Point bar_point(Fixture& f, runtime::ComponentId id, std::size_t i = 0) {
    const auto s = f.services.splitter().snapshot(id);
    const auto r = bounds(f, s.bars[i].component);
    return s.orientation == SplitterOrientation::Vertical ? runtime::Point{r.x + 20, r.y + r.height / 2}
                                                          : runtime::Point{r.x + r.width / 2, r.y + 20};
}

void pointer(Fixture& f, PointerAction action, runtime::Point point, std::uint8_t clicks = 0) {
    f.services.pointer().dispatch({PointerIdentity::mouse(), action,
                                   action == PointerAction::move || action == PointerAction::cancel
                                       ? PointerButton::none
                                       : PointerButton::primary,
                                   point.x, point.y, clicks});
}

void drag(Fixture& f, runtime::ComponentId id, float delta, bool finish = true, std::size_t i = 0) {
    const auto p = bar_point(f, id, i);
    const bool vertical = f.services.splitter().snapshot(id).orientation == SplitterOrientation::Vertical;
    const runtime::Point end{p.x + (vertical ? 0 : delta), p.y + (vertical ? delta : 0)};
    pointer(f, PointerAction::down, p);
    pointer(f, PointerAction::move, end);
    if (finish) {
        pointer(f, PointerAction::up, end);
    }
    sync(f);
}

void click(Fixture& f, runtime::ComponentId id) {
    const auto r = bounds(f, id);
    const runtime::Point p{r.x + r.width / 2, r.y + r.height / 2};
    pointer(f, PointerAction::down, p);
    pointer(f, PointerAction::up, p);
    sync(f);
}

void key(Fixture& f, Key value) {
    f.services.focus().dispatch({value, KeyAction::down});
    f.services.focus().dispatch({value, KeyAction::up});
    sync(f);
}

void focus_bar(Fixture& f, runtime::ComponentId id, std::size_t i = 0) {
    check(f.services.focus().request_focus(f.services.splitter().snapshot(id).bars[i].interaction,
                                           FocusModality::keyboard),
          "Splitter bar cannot focus");
}

void sum(const SplitterSizes& values, float total) {
    near(std::accumulate(values.begin(), values.end(), 0.0F), total, "Splitter sizes do not conserve total");
    check(std::ranges::all_of(values, [](float value) { return std::isfinite(value) && value >= 0; }),
          "Splitter has invalid geometry");
}

void model() {
    using detail::SplitterLimits;
    for (std::size_t count = 1; count <= 12; ++count) {
        for (float total : {0.0F, 1.0F, 100.0F, 10000.0F}) {
            SplitterSizes desired(count);
            std::vector<SplitterLimits> limits(count);
            for (std::size_t i = 0; i < count; ++i) {
                desired[i] = static_cast<float>((i + 1) * 37);
                limits[i] = {static_cast<float>(i * 11), static_cast<float>(i * 23 + 50)};
            }
            const auto values = detail::solve_splitter_sizes(total, desired, limits);
            sum(values, total);
            const auto repeated = detail::solve_splitter_sizes(total, values, limits);
            sum(repeated, total);
        }
    }
    const std::array desired{300.0F, 300.0F, 0.0F};
    const std::array limits{SplitterLimits{100, 400}, SplitterLimits{50, 500}, SplitterLimits{0, 0, true}};
    const auto values = detail::solve_splitter_sizes(600, desired, limits);
    check(values[2] == 0, "Solver resurrected a collapsed final panel");
    const auto changed = detail::resize_splitter_pair(values, 0, 1000, limits);
    near(changed[0], 400, "Pair max not applied");
    near(changed[1], 200, "Pair did not preserve adjacent total");
    check(changed[2] == 0, "Pair changed an unrelated panel");
}

void sizes() {
    Fixture f;
    Signal<LogicalLength> width{dp(600)};
    int runs{};
    f.services.mount(Content{[&] {
        Splitter(SplitterProps{}.layout(LayoutStyle{}.width(width)),
                 {plain(u8"Fixed default").defaultSize(SplitterSize{dp(100)}),
                  plain(u8"Percent").defaultSize(SplitterSize::percent(25)), SplitterPanel{SplitterPanelContent{[&] {
                      ++runs;
                      Input(InputProps{}.defaultValue(u8"Draft"));
                  }}}});
    }});
    sync(f);
    const auto id = owner(f);
    const auto initial = f.services.splitter().snapshot(id);
    check(initial.panels.size() == 3 && initial.bars.size() == 2, "Splitter only supports two panels");
    near(initial.sizes[0], 100, "Logical defaultSize lost");
    near(initial.sizes[1], 150, "Percent defaultSize lost");
    near(initial.sizes[2], 350, "Automatic remaining size lost");
    const auto editor = f.inputs.mounted_inputs()[0].editor;
    width.set(dp(1200));
    sync(f, 1200);
    const auto scaled = f.services.splitter().snapshot(id);
    near(scaled.sizes[0], 100, "Logical defaultSize did not retain logical units");
    near(scaled.sizes[1], 300, "Percentage defaultSize did not use new extent");
    near(scaled.sizes[2], 800, "Automatic panel did not absorb resize");
    check(runs == 1 && f.inputs.mounted_inputs()[0].editor == editor &&
              scaled.panels[2].content == initial.panels[2].content,
          "Window resize remounted panel");
    sum(scaled.sizes, 1200);
    width.set(dp(0));
    sync(f);
    width.set(dp(600));
    sync(f);
    near(f.services.splitter().snapshot(id).sizes[0], 100, "Zero-sized container lost original logical size");
    check(runs == 1, "Zero-sized container remounted retained content");
    Fixture g;
    g.services.mount(Content{[] {
        Splitter(SplitterProps{},
                 {plain(u8"A").size(SplitterSize::percent(0)), plain(u8"B").size(SplitterSize::percent(0))});
    }});
    sync(g);
    near(g.services.splitter().snapshot(owner(g)).sizes[0], 300,
         "All-zero controlled sizes did not normalize to equal panels");
    Fixture h;
    h.services.mount(Content{[] {
        Splitter(SplitterProps{}, {plain(u8"A").defaultSize(SplitterSize::percent(40)),
                                   plain(u8"B").defaultSize(SplitterSize::percent(80))});
    }});
    sync(h);
    near(h.services.splitter().snapshot(owner(h)).sizes[0], 200,
         "Overspecified percentages did not normalize proportionally");
}

void drag_constraints() {
    Fixture f;
    std::vector<SplitterSizes> starts;
    std::vector<SplitterSizes> changes;
    std::vector<SplitterSizes> ends;
    f.services.mount(Content{[&] {
        Splitter(SplitterProps{}
                     .onResizeStart([&](auto v) { starts.push_back(v); })
                     .onResize([&](auto v) { changes.push_back(v); })
                     .onResizeEnd([&](auto v) { ends.push_back(v); }),
                 {plain(u8"Left").min(SplitterSize::percent(20)).max(SplitterSize::percent(70)),
                  plain(u8"Right").min(SplitterSize{dp(100)}).max(SplitterSize{dp(400)})});
    }});
    sync(f);
    const auto id = owner(f);
    const auto start = bar_point(f, id);
    pointer(f, PointerAction::down, start);
    check(f.services.pointer().state(PointerIdentity::mouse())->capture ==
              f.services.splitter().snapshot(id).bars[0].interaction,
          "Splitter did not capture pointer");
    pointer(f, PointerAction::move, {start.x - 1000, start.y});
    sync(f);
    near(f.services.splitter().snapshot(id).sizes[0], 200, "Neighbor max was not enforced");
    pointer(f, PointerAction::move, {start.x + 1000, start.y});
    pointer(f, PointerAction::up, {start.x + 1000, start.y});
    sync(f);
    const auto s = f.services.splitter().snapshot(id);
    near(s.sizes[0], 420, "Panel max was not enforced");
    sum(s.sizes, 600);
    check(starts.size() == 1 && changes.size() == 2 && ends.size() == 1 && !s.dragging &&
              !f.services.pointer().state(PointerIdentity::mouse())->capture,
          "Resize events or capture completion wrong");
}

void lazy() {
    Fixture f;
    int starts{};
    int changes{};
    int ends{};
    f.services.mount(Content{[&] {
        Splitter(SplitterProps{}
                     .lazy()
                     .onResizeStart([&](auto) { ++starts; })
                     .onResize([&](auto) { ++changes; })
                     .onResizeEnd([&](auto) { ++ends; }),
                 {plain(u8"A"), plain(u8"B"), plain(u8"C")});
    }});
    sync(f);
    const auto id = owner(f);
    const auto original = f.services.splitter().snapshot(id);
    const auto r = bounds(f, original.panels[0].component);
    drag(f, id, 80, false);
    const auto preview = f.services.splitter().snapshot(id);
    near(preview.preview_sizes[0], 280, "Lazy preview was not constrained/updated");
    check(preview.sizes == original.sizes && preview.visual_sizes == original.visual_sizes &&
              bounds(f, preview.panels[0].component) == r && starts == 1 && changes == 0 && ends == 0,
          "Lazy drag committed panel geometry or callbacks early");
    const auto preview_color = f.services.components().theme_scope(id)->snapshot().splitter().colors.preview;
    const auto effects = f.services.rounded_effects().packed_instances();
    check(std::ranges::any_of(effects,
                              [&](const auto& effect) {
                                  return effect.material.color == preview_color && effect.material.opacity > 0 &&
                                         std::abs(effect.geometry.shape.rect.x - 279) < 2;
                              }),
          "Lazy preview line is absent from the logical scene or not at proposed seam");
    pointer(f, PointerAction::cancel, {280, 20});
    sync(f);
    check(f.services.splitter().snapshot(id).sizes == original.sizes && !f.services.splitter().snapshot(id).dragging &&
              changes == 0 && ends == 0 && !f.services.pointer().state(PointerIdentity::mouse())->capture,
          "Cancelled lazy drag committed or leaked capture");
    drag(f, id, 60);
    near(f.services.splitter().snapshot(id).sizes[0], 260, "Lazy release did not commit");
    near(f.services.splitter().snapshot(id).sizes[1], 140, "Lazy release changed wrong neighbor");
    near(f.services.splitter().snapshot(id).sizes[2], 200, "Lazy changed unrelated panel");
    check(starts == 2 && changes == 1 && ends == 1, "Lazy commit events wrong");
    Fixture g;
    Signal<bool> first_resizable{true};
    g.services.mount(Content{[&] {
        Splitter(SplitterProps{}.lazy(),
                 {plain(u8"A").defaultSize(SplitterSize::percent(50)).resizable(first_resizable),
                  plain(u8"Hidden middle").defaultSize(SplitterSize{dp(0)}), plain(u8"C")});
    }});
    sync(g);
    const auto gid = owner(g);
    drag(g, gid, -50, false, 1);
    const auto coincident = g.services.splitter().snapshot(gid);
    near(coincident.preview_sizes[0], 250, "Collapsed coincident seam did not select preceding live pair");
    near(coincident.preview_sizes[1], 50, "Collapsed middle panel did not preview expansion");
    near(coincident.preview_sizes[2], 300, "Coincident seam changed unrelated panel");
    first_resizable.set(false);
    check(!g.services.splitter().snapshot(gid).dragging &&
              !g.services.pointer().state(PointerIdentity::mouse())->capture,
          "Disabling a direction-resolved pair did not immediately release the original captured bar");
    pointer(g, PointerAction::up, {250, 20});
    sync(g);
    near(g.services.splitter().snapshot(gid).sizes[1], 0, "Disabled coincident gesture committed a lazy proposal");
    first_resizable.set(true);
    sync(g);
    drag(g, gid, -50, false, 1);
    pointer(g, PointerAction::up, {250, 20});
    sync(g);
    near(g.services.splitter().snapshot(gid).sizes[1], 50, "Coincident seam lazy release did not commit resolved pair");
}

void keyboard() {
    Fixture f;
    Signal<SplitterOrientation> orientation{SplitterOrientation::Horizontal};
    Signal<SplitterDirection> direction{SplitterDirection::LeftToRight};
    f.services.mount(Content{[&] {
        Splitter(SplitterProps{}.orientation(orientation).vertical(true).direction(direction),
                 {plain(u8"A").min(SplitterSize::percent(20)).max(SplitterSize::percent(70)),
                  plain(u8"B").min(SplitterSize{dp(100)}).max(SplitterSize{dp(400)})});
    }});
    sync(f);
    const auto id = owner(f);
    check(f.services.splitter().snapshot(id).orientation == SplitterOrientation::Horizontal,
          "orientation did not override vertical");
    focus_bar(f, id);
    key(f, Key::home);
    near(f.services.splitter().snapshot(id).sizes[0], 200, "Home ignored min/neighbor max");
    key(f, Key::end);
    near(f.services.splitter().snapshot(id).sizes[0], 420, "End ignored max/neighbor min");
    key(f, Key::left);
    near(f.services.splitter().snapshot(id).sizes[0], 410, "Left did not resize");
    direction.set(SplitterDirection::RightToLeft);
    sync(f);
    key(f, Key::left);
    near(f.services.splitter().snapshot(id).sizes[0], 420, "RTL Left did not invert");
    orientation.set(SplitterOrientation::Vertical);
    sync(f);
    const auto initial = f.services.splitter().snapshot(id).sizes[0];
    key(f, Key::left);
    near(f.services.splitter().snapshot(id).sizes[0], initial, "Vertical consumed horizontal arrow");
    key(f, Key::up);
    near(f.services.splitter().snapshot(id).sizes[0], initial - 10, "Vertical Up failed or applied RTL inversion");
    sum(f.services.splitter().snapshot(id).sizes, 240);
}

SplitterCollapsible always() {
    return {true, true, SplitterIconVisibility::Always};
}

void collapse_retained() {
    Fixture f;
    InputRef ref;
    int runs{};
    std::vector<SplitterCollapsed> changes;
    f.services.mount(Content{[&] {
        Splitter(SplitterProps{}.onCollapse([&](auto values, auto) { changes.push_back(values); }),
                 {SplitterPanel{SplitterPanelContent{[&] {
                      ++runs;
                      Input(InputProps{}.ref(ref).defaultValue(u8"draft"));
                  }}}.min(SplitterSize::percent(20))
                      .collapsible(always()),
                  plain(u8"B").collapsible(always())});
    }});
    sync(f);
    const auto id = owner(f);
    const auto original = f.services.splitter().snapshot(id);
    const auto editor = f.inputs.mounted_inputs()[0];
    check(ref.focus({InputFocusCursor::All}), "Panel editor could not focus");
    const auto session = f.inputs.sessions().active();
    check(bool(f.inputs.dispatch(TextCommitted{String{u8"saved"}, session})), "Panel input did not commit");
    click(f, original.bars[0].start);
    const auto collapsed = f.services.splitter().snapshot(id);
    check(collapsed.panels[0].collapsed && collapsed.sizes[0] == 0 && !ref.focus() &&
              !f.inputs.sessions().active().valid() && changes == std::vector<SplitterCollapsed>{{true, false}},
          "Collapse min bypass, callback or hidden input boundary failed");
    check(!f.services.focus().request_focus(collapsed.bars[0].interaction, FocusModality::keyboard),
          "Collapsed panel with min allowed drag");
    click(f, collapsed.bars[0].end);
    const auto restored = f.services.splitter().snapshot(id);
    near(restored.sizes[0], 300, "Collapse restore lost last size");
    check(runs == 1 && restored.panels[0].content == original.panels[0].content &&
              f.inputs.editors().require(editor.editor).value() == String{u8"saved"}.bytes(),
          "Default collapse lost retained draft");
    click(f, restored.bars[0].end);
    check(f.services.splitter().snapshot(id).panels[1].collapsed, "End control did not collapse second side");
    click(f, f.services.splitter().snapshot(id).bars[0].start);
    sum(f.services.splitter().snapshot(id).sizes, 600);
    check(changes.size() == 4 && !changes.back()[0] && !changes.back()[1], "Both-side restore events wrong");
    Fixture g;
    g.services.mount(Content{[] {
        Splitter(SplitterProps{},
                 {plain(u8"A").min(SplitterSize::percent(20)).collapsible(), plain(u8"B").collapsible()});
    }});
    sync(g);
    const auto gid = owner(g);
    focus_bar(g, gid);
    sync(g);
    click(g, g.services.splitter().snapshot(gid).bars[0].start);
    pointer(g, PointerAction::move, {580, 220});
    sync(g);
    check(!g.services.splitter().snapshot(gid).bars[0].resizable, "Collapsed min-constrained bar stayed resizable");
    const auto hidden_restore = g.services.splitter().snapshot(gid).bars[0].end;
    check(g.nodes.require(g.services.components().root(hidden_restore)).content_opacity == 0,
          "Auto collapse icon stayed visible after pointer left the seam");
    pointer(g, PointerAction::move, bar_point(g, gid));
    sync(g);
    const auto restore = g.services.splitter().snapshot(gid).bars[0].end;
    check(g.nodes.require(g.services.components().root(restore)).content_opacity == 1,
          "Non-resizable seam did not reveal its Auto restore control on hover");
    click(g, restore);
    check(!g.services.splitter().snapshot(gid).panels[0].collapsed, "Auto hover restore was not operable");
}

void destroy_hidden() {
    Fixture f;
    Signal<bool> global_destroy{true};
    int first_runs{};
    int second_runs{};
    f.services.mount(Content{[&] {
        Splitter(SplitterProps{}.destroyOnHidden(global_destroy), {SplitterPanel{SplitterPanelContent{[&] {
                                                                       ++first_runs;
                                                                       Input(InputProps{}.autoFocus());
                                                                   }}}.defaultSize(SplitterSize{dp(0)})
                                                                       .collapsible(always()),
                                                                   SplitterPanel{SplitterPanelContent{[&] {
                                                                       ++second_runs;
                                                                       Input(InputProps{}.defaultValue(u8"retained"));
                                                                   }}}.destroyOnHidden(false)
                                                                       .collapsible(always())});
    }});
    sync(f);
    const auto id = owner(f);
    auto s = f.services.splitter().snapshot(id);
    check(first_runs == 0 && second_runs == 1 && !s.panels[0].content.valid() && !f.inputs.sessions().active().valid(),
          "Initially destroyed panel mounted or focused content");
    click(f, s.bars[0].end);
    const auto first_content = f.services.splitter().snapshot(id).panels[0].content;
    check(first_runs == 1 && first_content.valid(), "Restore did not mount destroyed panel");
    click(f, f.services.splitter().snapshot(id).bars[0].start);
    check(!f.services.splitter().snapshot(id).panels[0].content.valid() &&
              !f.services.components().contains(first_content),
          "Collapsed panel was not destroyed");
    click(f, f.services.splitter().snapshot(id).bars[0].end);
    s = f.services.splitter().snapshot(id);
    check(first_runs == 2 && s.panels[0].content != first_content, "Destroyed content did not get a new generation");
    const auto second_content = s.panels[1].content;
    click(f, s.bars[0].end);
    check(f.services.splitter().snapshot(id).panels[1].content == second_content && second_runs == 1,
          "Panel destroyOnHidden override was ignored");
    click(f, f.services.splitter().snapshot(id).bars[0].start);
    global_destroy.set(false);
    sync(f);
    click(f, f.services.splitter().snapshot(id).bars[0].start);
    const auto retained = f.services.splitter().snapshot(id).panels[0].content;
    check(retained.valid(), "Updated global retention was ignored");
    global_destroy.set(true);
    sync(f);
    check(!f.services.components().contains(retained), "Changing global destroy policy did not clean hidden content");
}

void controlled() {
    Fixture f;
    Signal<SplitterSizes> sizes{SplitterSizes{150, 450}};
    bool echo{};
    int changes{};
    SplitterSizes proposal;
    f.services.mount(Content{[&] {
        Splitter(SplitterProps{}.onResize([&](auto values) {
            ++changes;
            proposal = values;
            if (echo) {
                sizes.set(values);
            }
        }),
                 {plain(u8"A").size(bind([&] { return SplitterSize{dp(sizes.get()[0])}; })),
                  plain(u8"B").size(bind([&] { return SplitterSize{dp(sizes.get()[1])}; }))});
    }});
    sync(f);
    const auto id = owner(f);
    drag(f, id, 50);
    check(changes == 1 && proposal == SplitterSizes{200, 400} &&
              f.services.splitter().snapshot(id).sizes == SplitterSizes{150, 450},
          "Controlled refusal changed panel sizes");
    echo = true;
    drag(f, id, 50);
    check(f.services.splitter().snapshot(id).sizes == SplitterSizes{200, 400}, "Controlled echo did not commit");
    sizes.set({0, 600});
    sync(f);
    check(f.services.splitter().snapshot(id).panels[0].collapsed, "Controlled zero size did not hide content");
}

void partial_controlled() {
    Fixture f;
    Signal<SplitterSize> size{SplitterSize{dp(100)}};
    f.services.mount(Content{[&] { Splitter(SplitterProps{}, {plain(u8"A").size(size), plain(u8"B")}); }});
    sync(f);
    const auto id = owner(f);
    near(f.services.splitter().snapshot(id).sizes[0], 100, "Controlled logical size was normalized away");
    size.set(SplitterSize{dp(200)});
    sync(f);
    near(f.services.splitter().snapshot(id).sizes[0], 200, "Partial controlled update lost authority");
    near(f.services.splitter().snapshot(id).sizes[1], 400, "Automatic panel did not absorb controlled difference");
    size.set(SplitterSize::percent(25));
    sync(f, 800);
    near(f.services.splitter().snapshot(id).sizes[0], 200, "Controlled percentage used stale container size");
}

void cancel_disabled() {
    Fixture f;
    Signal<bool> resizable{true};
    int ends{};
    f.services.mount(Content{[&] {
        Splitter(SplitterProps{}.lazy().onResizeEnd([&](auto) { ++ends; }),
                 {plain(u8"A"), plain(u8"B").resizable(resizable)});
    }});
    sync(f);
    const auto id = owner(f);
    drag(f, id, 50, false);
    const auto point = bar_point(f, id);
    f.services.pointer().dispatch(
        {PointerIdentity::touch(1, 9), PointerAction::down, PointerButton::primary, point.x, point.y});
    f.services.pointer().dispatch(
        {PointerIdentity::touch(1, 9), PointerAction::cancel, PointerButton::none, point.x, point.y});
    check(f.services.splitter().snapshot(id).dragging && f.services.pointer().state(PointerIdentity::mouse())->capture,
          "A different pointer cancelled the active gesture");
    resizable.set(false);
    sync(f);
    check(!f.services.splitter().snapshot(id).dragging &&
              !f.services.pointer().state(PointerIdentity::mouse())->capture && ends == 0,
          "Disabling resize retained gesture/capture or emitted commit");
    resizable.set(true);
    sync(f);
    drag(f, id, 50, false);
    key(f, Key::escape);
    check(!f.services.splitter().snapshot(id).dragging && ends == 0, "Escape did not cancel lazy gesture");
    drag(f, id, 50, false);
    f.services.set_window_active(false);
    check(!f.services.pointer().state(PointerIdentity::mouse())->capture &&
              !f.services.splitter().snapshot(id).dragging,
          "Window deactivation retained capture");
    f.services.set_window_active(true);
    sync(f);
    drag(f, id, 50, false);
    check(f.services.destroy(id), "Splitter destroy failed");
    check(!f.services.pointer().state(PointerIdentity::mouse())->capture && f.services.splitter().mounted().empty() &&
              f.services.interactions().size() == 0 && ends == 0,
          "Splitter destroy retained capture, callbacks or interactions");
}

void nested() {
    Fixture f;
    f.services.mount(Content{[] {
        Splitter(SplitterProps{}, {plain(u8"Left"), SplitterPanel{SplitterPanelContent{[] {
                                       Splitter(SplitterProps{}.orientation(SplitterOrientation::Vertical),
                                                {plain(u8"Top"), plain(u8"Bottom")});
                                   }}}});
    }});
    sync(f);
    check(f.services.splitter().mounted().size() == 2, "Nested Splitter did not mount");
    const auto outer = owner(f);
    const auto inner = f.services.splitter().mounted().front();
    sum(f.services.splitter().snapshot(inner).sizes, 240);
    drag(f, inner, 30);
    near(f.services.splitter().snapshot(inner).sizes[0], 150, "Nested vertical drag failed");
    drag(f, outer, 80);
    sum(f.services.splitter().snapshot(inner).sizes, 240);
    check(f.services.destroy(outer), "Nested Splitter destroy failed");
    check(f.services.splitter().mounted().empty() && f.services.interactions().size() == 0,
          "Nested Splitter leaked descendants");
}

void custom_double_click() {
    Fixture f;
    int draggers{};
    int starts{};
    int ends{};
    int double_clicks{};
    int icons{};
    f.services.mount(Content{[&] {
        Splitter(SplitterProps{}
                     .draggerIcon(SplitterDraggerIcon{[&] {
                         ++draggers;
                         Icon(IconProps{}.name(IconName::HolderOutlined));
                     }})
                     .startIcon(SplitterStartIcon{[&] {
                         ++icons;
                         Icon(IconProps{}.name(IconName::MinusOutlined));
                     }})
                     .endIcon(SplitterEndIcon{[&] {
                         ++icons;
                         Icon(IconProps{}.name(IconName::PlusOutlined));
                     }})
                     .onDraggerDoubleClick([&](auto i) {
                         check(i == 0, "Wrong double-click index");
                         ++double_clicks;
                     })
                     .onResizeStart([&](auto) { ++starts; })
                     .onResizeEnd([&](auto) { ++ends; }),
                 {plain(u8"A").collapsible(always()), plain(u8"B").collapsible(always())});
    }});
    sync(f);
    const auto id = owner(f);
    const auto p = bar_point(f, id);
    pointer(f, PointerAction::down, p, 2);
    pointer(f, PointerAction::up, p, 2);
    sync(f);
    check(double_clicks == 1 && starts == 0 && ends == 0 && draggers == 1 && icons == 2,
          "Double click started drag or custom icon slots were not retained");
    click(f, f.services.splitter().snapshot(id).bars[0].start);
    click(f, f.services.splitter().snapshot(id).bars[0].end);
    check(draggers == 1 && icons == 2, "Collapse reran custom icon factories");
}

void callbacks() {
    for (bool start : {true, false}) {
        Fixture f;
        runtime::ComponentId id;
        int ends{};
        f.services.mount(Content{[&] {
            auto props = SplitterProps{}.onResizeEnd([&](auto) { ++ends; });
            if (start) {
                props.onResizeStart([&](auto) { f.services.destroy(id); });
            } else {
                props.onResize([&](auto) { f.services.destroy(id); });
            }
            Splitter(props, {plain(u8"A"), plain(u8"B")});
        }});
        sync(f);
        id = owner(f);
        const auto p = bar_point(f, id);
        pointer(f, PointerAction::down, p);
        if (!start) {
            pointer(f, PointerAction::move, {p.x + 50, p.y});
        }
        check(f.services.splitter().mounted().empty() &&
                  !f.services.pointer().state(PointerIdentity::mouse())->capture && ends == 0,
              "Callback destroy used stale owner or delivered end callback");
    }
    Fixture f;
    f.services.mount(Content{[] {
        Splitter(SplitterProps{}.onResizeStart([](auto) { throw std::runtime_error("callback"); }),
                 {plain(u8"A"), plain(u8"B")});
    }});
    sync(f);
    bool threw{};
    try {
        pointer(f, PointerAction::down, bar_point(f, owner(f)));
    } catch (const std::runtime_error&) {
        threw = true;
    }
    check(threw && !f.services.splitter().snapshot(owner(f)).dragging &&
              !f.services.pointer().state(PointerIdentity::mouse())->capture,
          "Throwing callback retained capture");
}

void motion() {
    Fixture f;
    f.services.set_motion_preference(animation::MotionPreference::normal);
    f.services.mount(Content{[] {
        Splitter(SplitterProps{}.collapseMotion(),
                 {plain(u8"A").collapsible(always()), plain(u8"B").collapsible(always())});
    }});
    sync(f);
    const auto id = owner(f);
    click(f, f.services.splitter().snapshot(id).bars[0].start);
    auto s = f.services.splitter().snapshot(id);
    check(s.animating && s.sizes[0] == 0 && s.visual_sizes[0] == 300 && f.services.next_frame_deadline(),
          "Collapse motion did not retain visual geometry/deadline");
    static_cast<void>(f.services.tick_animations(animation::AnimationTime::microseconds(150000)));
    sync(f);
    s = f.services.splitter().snapshot(id);
    check(s.visual_sizes[0] > 0 && s.visual_sizes[0] < 300, "Collapse motion did not interpolate");
    sum(s.visual_sizes, 600);
    static_cast<void>(f.services.tick_animations(animation::AnimationTime::microseconds(1000000)));
    sync(f);
    s = f.services.splitter().snapshot(id);
    check(!s.animating && s.visual_sizes == s.sizes && !f.services.next_frame_deadline(),
          "Finished collapse motion kept scheduling frames");
    f.services.set_motion_preference(animation::MotionPreference::reduced);
    click(f, s.bars[0].end);
    s = f.services.splitter().snapshot(id);
    check(!s.animating && s.visual_sizes == s.sizes, "Reduced motion still animated collapse");
}

void theme_validation() {
    auto scope = theme_runtime::ThemeScope::create_default();
    int colors{};
    int metrics{};
    auto c = scope->capture([&](auto) { ++colors; }, [&] { static_cast<void>(scope->splitter_colors()); });
    auto m = scope->capture([&](auto) { ++metrics; }, [&] { static_cast<void>(scope->splitter_metrics()); });
    ThemeConfig config;
    config.splitter.tokens.bar = Color::rgba8(10, 20, 30);
    scope->update(config);
    check(colors == 1 && metrics == 0, "Splitter token colors not independently reactive");
    config.splitter.tokens.split_trigger_size = dp(12);
    scope->update(config);
    check(metrics == 1, "Splitter metric token did not notify");
    const auto identity = scope->snapshot().identity();
    config.splitter.tokens.split_trigger_size = dp(-1);
    bool threw{};
    try {
        scope->update(config);
    } catch (const std::invalid_argument&) {
        threw = true;
    }
    check(threw && scope->snapshot().identity() == identity, "Invalid Splitter theme did not reject atomically");
    for (int which = 0; which < 5; ++which) {
        Fixture f;
        threw = false;
        try {
            f.services.mount(Content{[&] {
                auto p = plain(u8"A");
                auto props = SplitterProps{};
                if (which == 0) {
                    p.size(SplitterSize{dp(10)}).defaultSize(SplitterSize{dp(20)});
                }
                if (which == 1) {
                    p.min(SplitterSize{dp(200)}).max(SplitterSize{dp(100)});
                }
                if (which == 2) {
                    props.orientation(static_cast<SplitterOrientation>(99));
                }
                if (which == 3) {
                    p.defaultSize(SplitterSize::percent(std::numeric_limits<float>::quiet_NaN()));
                }
                if (which == 4) {
                    p.collapsible(SplitterCollapsible{true, true, static_cast<SplitterIconVisibility>(99)});
                }
                Splitter(props, {p, plain(u8"B")});
            }});
        } catch (const std::invalid_argument&) {
            threw = true;
        }
        check(threw && f.services.components().component_count() == 0, "Invalid Splitter retained partial resources");
    }
}
} // namespace

int main(int argc, char** argv) {
    try {
        const std::string_view name = argc > 1 ? argv[1] : "";
        if (name == "model") {
            model();
        } else if (name == "sizes") {
            sizes();
        } else if (name == "drag_constraints") {
            drag_constraints();
        } else if (name == "lazy") {
            lazy();
        } else if (name == "keyboard") {
            keyboard();
        } else if (name == "collapse_retained") {
            collapse_retained();
        } else if (name == "destroy_hidden") {
            destroy_hidden();
        } else if (name == "controlled") {
            controlled();
        } else if (name == "partial_controlled") {
            partial_controlled();
        } else if (name == "cancel_disabled") {
            cancel_disabled();
        } else if (name == "nested") {
            nested();
        } else if (name == "custom_double_click") {
            custom_double_click();
        } else if (name == "callbacks") {
            callbacks();
        } else if (name == "motion") {
            motion();
        } else if (name == "theme_validation") {
            theme_validation();
        } else {
            throw std::runtime_error("Unknown Splitter case");
        }
        std::cout << "Splitter " << name << " passed\n";
    } catch (const std::exception& error) {
        std::cerr << error.what() << '\n';
        return 1;
    }
}
