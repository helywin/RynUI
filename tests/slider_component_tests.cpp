#include "component/slider_component.hpp"
#include "component/slider_value.hpp"
#include "component/tooltip_component.hpp"
#include "support/input_fixture.hpp"
#include <ryn/rynui.hpp>
#include <cmath>
#include <iostream>
#include <limits>
#include <type_traits>
#include <thread>
#if defined(_MSC_VER) && defined(_DEBUG)
#include <crtdbg.h>
#endif

namespace {
using namespace ryn;
using namespace ryn::input;
using Fixture = ryn_test::input_component::Fixture;

void check(bool value, const char* message) {
    if (!value) {
        throw std::runtime_error(message);
    }
}

bool near(double a, double b) {
    return std::abs(a - b) < 1e-8;
}

template <class F> void rejects(F action) {
    bool rejected{};
    try {
        action();
    } catch (const std::invalid_argument&) {
        rejected = true;
    }
    check(rejected, "invalid Slider input accepted");
}

void key(Fixture& f, Key key, KeyAction action = KeyAction::down, bool repeat = false) {
    f.services.focus().dispatch({key, action, KeyModifier::none, repeat});
}

void press_key(Fixture& f, Key value) {
    key(f, value);
    key(f, value, KeyAction::up);
    f.synchronize();
}

void pointer(Fixture& f, PointerAction action, runtime::Point p, PointerIdentity identity = PointerIdentity::mouse()) {
    f.services.pointer().dispatch(
        {identity, action,
         action == PointerAction::down || action == PointerAction::up ? PointerButton::primary : PointerButton::none,
         p.x, p.y});
}

runtime::Point at(Fixture& f, const detail::MountedSliderComponent& m, double ratio) {
    const auto& node = f.nodes.require(m.node);
    const auto& t = f.services.components().theme_scope(m.component)->snapshot().slider().metrics;
    const bool vertical = f.services.slider().snapshot(m.component).orientation == SliderOrientation::Vertical;
    const auto b = node.bounds;
    const float length = vertical ? b.height : b.width;
    const float inset = std::min(length / 2, t.handle_size_hover / 2 + t.handle_line_width_hover);
    const float pos = inset + (length - 2 * inset) * static_cast<float>(ratio);
    return vertical ? runtime::Point{b.x + b.width / 2 + node.translation.x, b.y + pos + node.translation.y}
                    : runtime::Point{b.x + pos + node.translation.x, b.y + b.height / 2 + node.translation.y};
}

void marks_numeric_contracts() {
    static_assert(std::is_same_v<decltype(SliderProps{}
                                              .marks(SliderMarks{})
                                              .marksOnly(true)
                                              .dots(true)
                                              .included(false)
                                              .hint(SliderHintOptions{})),
                                 SliderProps&>);
    const SliderLimits limits{0, 100, 10};
    const auto marks = detail::sorted_slider_marks({{37, String{u8"中文"}}, {20, String{u8"twenty"}}}, limits);
    check(marks.front().value == 20 && marks.back().value == 37, "marks were not sorted");
    check(detail::normalize_slider_value(37, limits, marks) == 37, "mark lost to step grid");
    check(detail::normalize_slider_value(35, limits, marks) == 37, "closest mark was ignored");
    check(detail::normalize_slider_value(10, limits, marks, true) == 20, "marks-only tie did not select larger value");
    check(detail::normalize_slider_value(-1, limits, marks, true) == 0 &&
              detail::normalize_slider_value(200, limits, marks, true) == 100,
          "marks-only boundaries absent");
    check(detail::advance_slider_value(20, limits, marks, true, 1, 1) == 37 &&
              detail::advance_slider_value(37, limits, marks, true, -1, 1) == 20,
          "discrete arrows skipped a mark");
    check(detail::advance_slider_value(30, limits, marks, false, 1, 1) == 37 &&
              detail::advance_slider_value(37, limits, marks, false, -1, 1) == 30,
          "step/marks union skipped nearest point");
    check(detail::advance_slider_value(37, limits, marks, true, 1, 10) == 100, "discrete Page clamp failed");
    check(near(detail::advance_slider_value(1, {0, 1, 0.3}, {}, false, -1, 1), 0.9),
          "extra maximum decrement lost final step");
    check(detail::slider_visual_points(limits, marks, true, true) == std::vector<double>{0, 20, 37, 100},
          "marks-only dots wrong");
    check(detail::slider_visual_points(limits, marks, false, true).size() == 12, "union dots duplicated mark on grid");
    rejects([&] { (void)detail::sorted_slider_marks({{20, {}}, {20, {}}}, limits); });
    rejects([&] { (void)detail::sorted_slider_marks({{NAN, {}}}, limits); });
    rejects([&] { (void)detail::sorted_slider_marks({{101, {}}}, limits); });
    rejects([&] { (void)detail::slider_visual_points({0, 100, 0.001}, {}, false, true); });
    check(detail::slider_visual_points({0, 100, 0.001}, {}, false, false).empty(),
          "dense grid without dots allocated visual points");
    check(detail::slider_visual_points({0, 4095, 1}, {}, false, true).size() == 4096, "dots boundary rejected");
    rejects([&] { (void)detail::sorted_slider_marks(SliderMarks(4097), limits); });
    Fixture f;
    Signal<SliderMarks> reactive{marks};
    Signal<SliderLimits> dynamic_limits{limits};
    Signal<double> value{20};
    int changes{};
    int completes{};
    f.services.mount(Content{[&] {
        Slider(SliderProps{}
                   .value(value)
                   .marks(reactive)
                   .marksOnly(true)
                   .limits(dynamic_limits)
                   .onChange([&](double next) {
                       ++changes;
                       value.set(next);
                   })
                   .onChangeComplete([&](double) { ++completes; }));
    }});
    f.synchronize();
    const auto mounted = f.services.slider().mounted()[0];
    check(f.services.focus().request_focus(mounted.thumbs[0], FocusModality::keyboard), "discrete focus failed");
    press_key(f, Key::right);
    check(value.get() == 37 && changes == 1 && completes == 1, "discrete mounted keyboard failed");
    const auto previous = f.services.slider().snapshot(mounted.component).value;
    rejects([&] { reactive.set(SliderMarks{{200, {}}}); });
    check(f.services.slider().snapshot(mounted.component).value == previous, "invalid marks changed value");
    reactive.set(marks);
    rejects([&] { dynamic_limits.set({0, 30, 10}); });
    check(f.services.slider().snapshot(mounted.component).limits == limits, "invalid limits discarded marks");
    dynamic_limits.set(limits);
    reactive.set(SliderMarks{{50, String{u8"half"}}});
    f.synchronize();
    check(f.services.slider().snapshot(mounted.component).value.lower == 50 && changes == 1,
          "dynamic marks normalization fired change or chose wrong point");
}

void numeric_and_api() {
    static_assert(std::is_same_v<decltype(SliderProps{}.value(1.0).limits(SliderLimits{})), SliderProps&>);
    static_assert(std::is_same_v<decltype(RangeSliderProps{}.value(SliderRange{}).reverse(true)), RangeSliderProps&>);
    const SliderLimits fractional{-1, 1, 0.1};
    detail::validate_slider_limits(fractional);
    check(near(detail::normalize_slider_value(0.26, fractional), 0.3), "fractional step truncated");
    check(near(detail::normalize_slider_value(-10, fractional), -1), "minimum clamp failed");
    check(near(detail::normalize_slider_value(10, fractional), 1), "maximum clamp failed");
    check(near(detail::normalize_slider_value(0.25, {0, 1, 0.5}), 0.5), "tie did not round up");
    check(near(detail::normalize_slider_value(0.95, {0, 1, 0.3}), 1), "extra maximum endpoint missing");
    check(detail::normalize_slider_range({90, 20}, {}) == SliderRange{20, 90}, "range did not sort");
    for (auto invalid : {SliderLimits{0, 0, 1}, SliderLimits{1, 0, 1}, SliderLimits{0, 1, 0}, SliderLimits{0, 1, -1},
                         SliderLimits{0, 1, 1e-30}, SliderLimits{0, INFINITY, 1}, SliderLimits{-1e308, 1e308, 1},
                         SliderLimits{1e20, 1e20 + 1e6, 1}}) {
        rejects([&] { detail::validate_slider_limits(invalid); });
    }
    rejects([] { detail::normalize_slider_value(NAN, {}); });
    rejects([] { detail::normalize_slider_value(INFINITY, {}); });
    Fixture f;
    rejects([&] { f.services.mount(Content{[] { Slider(SliderProps{}.value(1).defaultValue(2)); }}); });
    check(f.services.slider().mounted().empty() && f.services.interactions().size() == 0,
          "invalid mode acquired resources");
    Fixture failed;
    bool aborted{};
    try {
        failed.services.mount(Content{[] {
            RangeSlider(RangeSliderProps{});
            throw std::runtime_error("abort mount");
        }});
    } catch (const std::runtime_error&) {
        aborted = true;
    }
    check(aborted && failed.services.slider().mounted().empty() && failed.services.interactions().size() == 0 &&
              failed.services.surfaces().size() == 0,
          "failed mount retained Slider resources");
}

void multiple_values_and_retained_topology() {
    static_assert(std::is_same_v<decltype(MultiSliderProps{}.value(SliderValues{}).rangeOptions(SliderRangeOptions{})),
                                 MultiSliderProps&>);
    check(detail::normalize_slider_values(SliderValues{90, 21, 21, -1}, {0, 100, 10}) == SliderValues{0, 20, 20, 90},
          "multiple values did not normalize and sort duplicates");
    rejects([] { (void)detail::normalize_slider_values(SliderValues(65), {}); });
    rejects([] { (void)detail::normalize_slider_values(SliderValues{1, NAN}, {}); });
    for (auto options : {SliderRangeOptions{false, false, 3, 2}, SliderRangeOptions{false, false, 0, 65},
                         SliderRangeOptions{true, true, 0, 64}}) {
        rejects([&] { detail::validate_slider_range_options(options, false, 2); });
    }
    Fixture f;
    Signal<SliderValues> values{SliderValues{20, 50, 80}};
    Signal<SliderRangeOptions> options{SliderRangeOptions{false, false, 0, 64}};
    std::vector<SliderValues> changes;
    std::vector<SliderValues> completed;
    int runs{};
    f.services.mount(Content{[&] {
        ++runs;
        MultiSlider(MultiSliderProps{}
                        .value(values)
                        .rangeOptions(options)
                        .onChange([&](SliderValues next) { changes.push_back(std::move(next)); })
                        .onChangeComplete([&](SliderValues next) { completed.push_back(std::move(next)); }));
        Slider(SliderProps{}.defaultValue(10));
    }});
    f.synchronize();
    const auto mounted = f.services.slider().mounted()[0];
    const auto sibling = f.services.slider().mounted()[1];
    f.services.focus().request_focus(mounted.thumbs[1], FocusModality::keyboard);
    key(f, Key::right);
    key(f, Key::right, KeyAction::down, true);
    key(f, Key::right, KeyAction::up);
    check(changes == std::vector<SliderValues>{{20, 51, 80}, {20, 52, 80}} &&
              completed.back() == SliderValues{20, 52, 80} &&
              f.services.slider().snapshot(mounted.component).values == SliderValues{20, 50, 80},
          "multiple controlled keyboard changed display or lost candidates");
    values.set({20, 30, 50, 80});
    f.synchronize();
    auto handles = f.services.slider().mounted()[0].thumbs;
    check(handles.size() == 4 && handles[0] == mounted.thumbs[0] && handles[2] == mounted.thumbs[1] &&
              handles[3] == mounted.thumbs[2] && f.services.focus().state().focused == handles[2] && runs == 1,
          "insertion replaced retained endpoints or focus");
    f.services.focus().clear_focus();
    for (auto handle : handles) {
        key(f, Key::tab);
        check(f.services.focus().state().focused == handle, "dynamic insertion did not preserve sorted Tab order");
    }
    key(f, Key::tab);
    check(f.services.focus().state().focused == sibling.thumbs[0], "dynamic endpoints moved after sibling Tab stop");
    values.set({20, 50, 80});
    f.synchronize();
    check(f.services.slider().mounted()[0].thumbs == mounted.thumbs && !f.services.interactions().contains(handles[1]),
          "removal leaked endpoint or replaced unchanged handles");
    const auto old = f.services.slider().snapshot(mounted.component).values;
    rejects([&] { values.set(SliderValues(65)); });
    check(f.services.slider().snapshot(mounted.component).values == old, "invalid count changed mounted topology");
    values.set(old);
    rejects([&] { options.set({false, false, 4, 64}); });
    check(f.services.slider().mounted()[0].thumbs == mounted.thumbs, "invalid options changed mounted topology");
    options.set({false, false, 0, 64});
    values.set({});
    f.synchronize();
    check(f.services.slider().mounted()[0].thumbs.empty() && f.services.tooltip().mounted().size() == 1,
          "empty multiple slider retained fake endpoints");
    values.set({50, 50, 50});
    f.synchronize();
    handles = f.services.slider().mounted()[0].thumbs;
    f.services.focus().request_focus(handles[0], FocusModality::keyboard);
    pointer(f, PointerAction::down, at(f, mounted, 0.5));
    pointer(f, PointerAction::move, at(f, mounted, 0.2));
    pointer(f, PointerAction::up, at(f, mounted, 0.2));
    check(changes.back() == SliderValues{20, 50, 50}, "multiple overlap changed wrong active endpoint");
    values.set(SliderValues(64, 50));
    f.synchronize();
    check(f.services.slider().mounted()[0].thumbs.size() == 64, "64 endpoints rejected");
    f.services.destroy(mounted.component);
    f.synchronize();
    check(f.services.slider().mounted().size() == 1 && f.services.tooltip().mounted().size() == 1 &&
              f.services.interactions().size() == 4,
          "multiple destroy leaked resources or damaged sibling");
    Fixture defaults;
    defaults.services.mount(Content{[] {
        MultiSlider(MultiSliderProps{}.limits(SliderLimits{10, 100, 1}));
        MultiSlider(MultiSliderProps{}.defaultValue({}));
    }});
    defaults.synchronize();
    check(defaults.services.slider().snapshot(defaults.services.slider().mounted()[0].component).values ==
                  SliderValues{10, 10} &&
              defaults.services.slider().mounted()[1].thumbs.empty(),
          "implicit defaults confused explicit empty values");
    Fixture rollback;
    bool failed{};
    try {
        rollback.services.mount(Content{[] {
            MultiSlider(MultiSliderProps{}.defaultValue({20, 50, 80}));
            throw std::runtime_error("abort");
        }});
    } catch (const std::runtime_error&) {
        failed = true;
    }
    check(failed && rollback.services.slider().mounted().empty() && rollback.services.interactions().size() == 0 &&
              rollback.services.tooltip().mounted().empty(),
          "multiple mount rollback leaked endpoints");
}

void whole_track_drag_contracts() {
    for (auto orientation : {SliderOrientation::Horizontal, SliderOrientation::Vertical}) {
        for (bool reverse : {false, true}) {
            Fixture f;
            int completed{};
            Signal<SliderRangeOptions> options{SliderRangeOptions{true, false, 0, 64}};
            f.services.mount(Content{[&] {
                MultiSlider(MultiSliderProps{}
                                .defaultValue({20, 50, 80})
                                .rangeOptions(options)
                                .orientation(orientation)
                                .reverse(reverse)
                                .limits(SliderLimits{0, 100, 10})
                                .onChangeComplete([&](SliderValues) { ++completed; }));
            }});
            f.synchronize();
            const auto m = f.services.slider().mounted()[0];
            const bool inverted = detail::slider_inverted(orientation, reverse);
            const auto point = [&](double value) {
                return at(f, m, inverted ? 1 - value : value);
            };
            pointer(f, PointerAction::down, point(0.35));
            check(f.services.slider().snapshot(m.component).values == SliderValues{20, 50, 80},
                  "track down jumped one endpoint");
            pointer(f, PointerAction::move, point(0.45));
            check(f.services.slider().snapshot(m.component).values == SliderValues{30, 60, 90},
                  "track offset or orientation mapping wrong");
            pointer(f, PointerAction::move, point(1.2));
            pointer(f, PointerAction::up, point(1.2));
            f.synchronize();
            check(f.services.slider().snapshot(m.component).values == SliderValues{40, 70, 100} && completed == 1,
                  "track boundary changed spacing or completion");
            pointer(f, PointerAction::down, point(0.55));
            pointer(f, PointerAction::move, point(0.45));
            options.set({false, false, 0, 64});
            check(!f.services.slider().snapshot(m.component).dragging && completed == 1 &&
                      !f.services.pointer().state(PointerIdentity::mouse())->capture,
                  "track config change retained capture or completed cancelled gesture");
        }
    }
    Fixture controlled;
    Signal<SliderRange> value{SliderRange{20, 80}};
    std::vector<SliderRange> candidates;
    std::vector<SliderRange> completed;
    bool echo{};
    controlled.services.mount(Content{[&] {
        RangeSlider(RangeSliderProps{}
                        .value(value)
                        .draggableTrack(true)
                        .onChange([&](SliderRange next) {
                            candidates.push_back(next);
                            if (echo) {
                                value.set(next);
                            }
                        })
                        .onChangeComplete([&](SliderRange next) { completed.push_back(next); }));
    }});
    controlled.synchronize();
    const auto range = controlled.services.slider().mounted()[0];
    pointer(controlled, PointerAction::down, at(controlled, range, 0.5));
    pointer(controlled, PointerAction::move, at(controlled, range, 0.6));
    pointer(controlled, PointerAction::move, at(controlled, range, 0.7));
    pointer(controlled, PointerAction::up, at(controlled, range, 0.7));
    check(candidates.back() == SliderRange{40, 100} && completed.back() == SliderRange{40, 100} &&
              controlled.services.slider().snapshot(range.component).value == SliderRange{20, 80},
          "controlled track drag mutated display or reused advancing origin");
    echo = true;
    pointer(controlled, PointerAction::down, at(controlled, range, 0.5));
    pointer(controlled, PointerAction::move, at(controlled, range, 0.6));
    pointer(controlled, PointerAction::move, at(controlled, range, 0.7));
    pointer(controlled, PointerAction::up, at(controlled, range, 0.7));
    check(value.get() == SliderRange{40, 100}, "controlled track echo rebased original snapshot");
    controlled.synchronize();
    pointer(controlled, PointerAction::down, at(controlled, range, 0.6));
    controlled.services.set_window_active(false);
    check(!controlled.services.slider().snapshot(range.component).dragging && completed.size() == 2,
          "window loss completed track drag");
    Fixture irregular;
    irregular.services.mount(Content{[] {
        MultiSlider(MultiSliderProps{}
                        .defaultValue({20, 37, 80})
                        .limits(SliderLimits{0, 100, 10})
                        .marks(SliderMarks{{37, {}}})
                        .rangeOptions(SliderRangeOptions{true, false, 0, 64}));
    }});
    irregular.synchronize();
    const auto m = irregular.services.slider().mounted()[0];
    pointer(irregular, PointerAction::down, at(irregular, m, 0.55));
    pointer(irregular, PointerAction::up, at(irregular, m, 0.65));
    check(irregular.services.slider().snapshot(m.component).values == SliderValues{30, 50, 90},
          "irregular mark drag did not normalize each translated endpoint");
    Fixture invalid;
    rejects([&] {
        invalid.services.mount(Content{[] { RangeSlider(RangeSliderProps{}.marksOnly(true).draggableTrack(true)); }});
    });
    check(invalid.services.interactions().size() == 0, "invalid track configuration acquired resources");
    Fixture dying;
    runtime::ComponentId id;
    dying.services.mount(Content{[&] {
        RangeSlider(RangeSliderProps{}.defaultValue({20, 80}).draggableTrack(true).onChange(
            [&](SliderRange) { dying.services.destroy(id); }));
    }});
    dying.synchronize();
    const auto own = dying.services.slider().mounted()[0];
    id = own.component;
    pointer(dying, PointerAction::down, at(dying, own, 0.5));
    pointer(dying, PointerAction::move, at(dying, own, 0.6));
    check(dying.services.slider().mounted().empty() && dying.services.interactions().size() == 0,
          "reentrant track callback retained component or capture");
}

void editable_and_disabled_contracts() {
    Fixture f;
    Signal<SliderRangeOptions> options{SliderRangeOptions{false, true, 1, 3}};
    Signal<SliderDisabledHandles> disabled{SliderDisabledHandles{}};
    int changes{};
    int completed{};
    f.services.mount(Content{[&] {
        MultiSlider(MultiSliderProps{}
                        .defaultValue({20, 80})
                        .rangeOptions(options)
                        .handleDisabled(disabled)
                        .onChange([&](SliderValues) { ++changes; })
                        .onChangeComplete([&](SliderValues) { ++completed; }));
    }});
    f.synchronize();
    const auto m = f.services.slider().mounted()[0];
    pointer(f, PointerAction::down, at(f, m, 0.5));
    pointer(f, PointerAction::up, at(f, m, 0.5));
    f.synchronize();
    auto handles = f.services.slider().mounted()[0].thumbs;
    check(f.services.slider().snapshot(m.component).values == SliderValues{20, 50, 80} && changes == 1 &&
              completed == 1 && handles[0] == m.thumbs[0] && handles[2] == m.thumbs[1],
          "editable rail did not insert retained endpoint");
    pointer(f, PointerAction::down, at(f, m, 0.65));
    pointer(f, PointerAction::up, at(f, m, 0.65));
    f.synchronize();
    check(f.services.slider().snapshot(m.component).values == SliderValues{20, 65, 80},
          "maxCount did not fall back to nearest movement");
    f.services.focus().request_focus(handles[1], FocusModality::keyboard);
    key(f, Key::delete_forward);
    f.synchronize();
    check(f.services.slider().snapshot(m.component).values == SliderValues{20, 80} && completed == 3 &&
              f.services.focus().state().focused == m.thumbs[1],
          "Delete did not remove and transfer focus once");
    key(f, Key::delete_forward, KeyAction::down, true);
    check(f.services.slider().snapshot(m.component).values.size() == 2 && completed == 3,
          "Delete repeat removed another handle");
    press_key(f, Key::backspace);
    check(f.services.slider().snapshot(m.component).values == SliderValues{20} && completed == 4, "Backspace missing");
    press_key(f, Key::backspace);
    check(f.services.slider().snapshot(m.component).values == SliderValues{20} && completed == 4, "minCount ignored");
    options.set({false, true, 0, 3});
    press_key(f, Key::delete_forward);
    check(f.services.slider().mounted()[0].thumbs.empty() && completed == 5, "delete to empty retained fake handle");
    pointer(f, PointerAction::down, at(f, m, 0.5));
    pointer(f, PointerAction::up, at(f, m, 0.5));
    f.synchronize();
    const auto inserted = f.services.slider().mounted()[0].thumbs[0];
    auto off = at(f, m, 0.5);
    off.y += 131;
    pointer(f, PointerAction::down, at(f, m, 0.5));
    pointer(f, PointerAction::move, off);
    check(f.services.slider().snapshot(m.component).delete_preview, "cross-axis deletion preview missing");
    const auto before = completed;
    pointer(f, PointerAction::cancel, off);
    check(f.services.slider().snapshot(m.component).values == SliderValues{50} && completed == before &&
              !f.services.slider().snapshot(m.component).delete_preview,
          "cancel committed deletion");
    pointer(f, PointerAction::down, at(f, m, 0.5));
    pointer(f, PointerAction::move, off);
    pointer(f, PointerAction::up, off);
    f.synchronize();
    check(f.services.slider().mounted()[0].thumbs.empty() && !f.services.interactions().contains(inserted) &&
              completed == before + 1,
          "drag release did not delete or leaked handle");
    pointer(f, PointerAction::down, at(f, m, 0.5));
    pointer(f, PointerAction::up, at(f, m, 0.5));
    f.synchronize();
    const auto single = f.services.slider().mounted()[0].thumbs[0];
    f.services.focus().request_focus(single, FocusModality::keyboard);
    disabled.set({true});
    check(!f.services.focus().state().focused && !f.services.interactions().require(single).eligible,
          "per-handle disabled retained focus or eligibility");
    const auto changes_before = changes;
    pointer(f, PointerAction::down, at(f, m, 0.25));
    pointer(f, PointerAction::up, at(f, m, 0.25));
    check(changes == changes_before && f.services.slider().snapshot(m.component).values == SliderValues{50},
          "disabled handle allowed editor or rail operation");
    rejects([&] { disabled.set(SliderDisabledHandles(65)); });
    check(!f.services.interactions().require(single).eligible, "invalid disabled list changed eligibility");
    disabled.set({false});
    pointer(f, PointerAction::down, at(f, m, 0.5));
    disabled.set({true});
    check(!f.services.slider().snapshot(m.component).dragging &&
              !f.services.pointer().state(PointerIdentity::mouse())->capture,
          "disabled update did not cancel capture");

    for (bool echo : {false, true}) {
        Fixture controlled;
        Signal<SliderValues> value{SliderValues{20, 80}};
        SliderValues last;
        int done{};
        controlled.services.mount(Content{[&] {
            MultiSlider(MultiSliderProps{}
                            .value(value)
                            .rangeOptions(SliderRangeOptions{false, true, 0, 4})
                            .onChange([&](SliderValues next) {
                                last = next;
                                if (echo) {
                                    value.set(next);
                                }
                            })
                            .onChangeComplete([&](SliderValues next) {
                                ++done;
                                last = next;
                            }));
        }});
        controlled.synchronize();
        const auto own = controlled.services.slider().mounted()[0];
        pointer(controlled, PointerAction::down, at(controlled, own, 0.5));
        pointer(controlled, PointerAction::move, at(controlled, own, 0.6));
        pointer(controlled, PointerAction::up, at(controlled, own, 0.6));
        check(last == SliderValues{20, 60, 80} && done == 1 && value.get() == (echo ? last : SliderValues{20, 80}),
              "controlled insertion echo cancelled drag or changed display without echo");
        controlled.synchronize();
        const auto target = controlled.services.slider().mounted()[0].thumbs[0];
        controlled.services.focus().request_focus(target, FocusModality::keyboard);
        press_key(controlled, Key::delete_forward);
        check(done == 2 && last == (echo ? SliderValues{60, 80} : SliderValues{80}) &&
                  value.get() == (echo ? last : SliderValues{20, 80}),
              "controlled deletion lost completion or changed without echo");
    }
    Fixture pair;
    Fixture labels;
    Signal<SliderDisabledHandles> label_disabled{SliderDisabledHandles{}};
    int label_completed{};
    labels.services.mount(Content{[&] {
        MultiSlider(MultiSliderProps{}
                        .defaultValue({})
                        .marks(SliderMarks{{50, String{u8"中点"}}})
                        .rangeOptions(SliderRangeOptions{false, true, 0, 2})
                        .handleDisabled(label_disabled)
                        .onChangeComplete([&](SliderValues) { ++label_completed; }));
    }});
    labels.synchronize();
    const auto label_root = labels.services.slider().mounted()[0].component;
    const auto label_node = labels.services.components().root(labels.services.components().children(label_root).back());
    const auto label_rect = labels.nodes.require(label_node).bounds;
    pointer(labels, PointerAction::down, {label_rect.x + 2, label_rect.y + 2});
    pointer(labels, PointerAction::up, {label_rect.x + 2, label_rect.y + 2});
    check(labels.services.slider().snapshot(label_root).values == SliderValues{50},
          "mark label did not insert into empty slider");
    label_disabled.set({true});
    labels.synchronize();
    pointer(labels, PointerAction::down, {label_rect.x + 2, label_rect.y + 2});
    pointer(labels, PointerAction::up, {label_rect.x + 2, label_rect.y + 2});
    check(label_completed == 1, "disabled-only label emitted completion");
    Fixture vertical;
    vertical.services.mount(Content{[] {
        MultiSlider(MultiSliderProps{}
                        .defaultValue({20, 80})
                        .orientation(SliderOrientation::Vertical)
                        .reverse(true)
                        .rangeOptions(SliderRangeOptions{false, true, 1, 3}));
    }});
    vertical.synchronize();
    const auto vertical_m = vertical.services.slider().mounted()[0];
    auto vertical_off = at(vertical, vertical_m, 0.2);
    vertical_off.x += 131;
    pointer(vertical, PointerAction::down, at(vertical, vertical_m, 0.2));
    pointer(vertical, PointerAction::move, vertical_off);
    pointer(vertical, PointerAction::up, vertical_off);
    check(vertical.services.slider().snapshot(vertical_m.component).values == SliderValues{80},
          "vertical reverse drag deletion failed");
    vertical.synchronize();
    vertical_off = at(vertical, vertical_m, 0.8);
    vertical_off.x += 131;
    pointer(vertical, PointerAction::down, at(vertical, vertical_m, 0.8));
    pointer(vertical, PointerAction::move, vertical_off);
    pointer(vertical, PointerAction::up, vertical_off);
    check(vertical.services.slider().snapshot(vertical_m.component).values.size() == 1,
          "drag deletion ignored minCount");
    pair.services.mount(Content{[] {
        RangeSlider(
            RangeSliderProps{}.defaultValue({20, 80}).draggableTrack(true).handleDisabled(SliderDisabledHandles{true}));
    }});
    pair.synchronize();
    const auto range = pair.services.slider().mounted()[0];
    key(pair, Key::tab);
    check(pair.services.focus().state().focused == range.thumbs[1], "disabled endpoint stayed in Tab order");
    pointer(pair, PointerAction::down, at(pair, range, 0.5));
    pointer(pair, PointerAction::up, at(pair, range, 0.6));
    check(pair.services.slider().snapshot(range.component).value == SliderRange{20, 60},
          "rail selected disabled endpoint or moved disabled track");
    Fixture dying;
    runtime::ComponentId id;
    dying.services.mount(Content{[&] {
        MultiSlider(MultiSliderProps{}
                        .defaultValue({20, 80})
                        .rangeOptions(SliderRangeOptions{false, true, 0, 4})
                        .onChange([&](SliderValues) { dying.services.destroy(id); }));
    }});
    dying.synchronize();
    const auto own = dying.services.slider().mounted()[0];
    id = own.component;
    dying.services.focus().request_focus(own.thumbs[0], FocusModality::keyboard);
    key(dying, Key::delete_forward);
    check(dying.services.slider().mounted().empty() && dying.services.tooltip().mounted().empty(),
          "reentrant editor destruction leaked resources");
}

void native_refs_and_hint_options() {
    SliderRef ref;
    check(!ref.bound() && !ref.focus() && !ref.blur(), "unbound ref performed an operation");
    Fixture f;
    Signal<bool> disabled{false};
    Signal<SliderValues> values{SliderValues{20, 80}};
    f.services.mount(Content{[&] {
        MultiSlider(MultiSliderProps{}.value(values).ref(ref).autoFocus(true).disabled(disabled).handleDisabled(
            SliderDisabledHandles{true, false}));
        Slider(SliderProps{});
    }});
    f.synchronize();
    const auto m = f.services.slider().mounted()[0];
    const auto sibling = f.services.slider().mounted()[1];
    check(ref.bound() && f.services.focus().state().focused == m.thumbs[1],
          "autoFocus did not choose first enabled handle");
    check(ref.blur() && !f.services.focus().state().focused && ref.focus(), "bound focus/blur failed");
    values.set({30, 70});
    check(f.services.focus().state().focused == m.thumbs[1], "reactive value reran autoFocus");
    f.services.focus().request_focus(sibling.thumbs[0], FocusModality::keyboard);
    check(!ref.blur() && f.services.focus().state().focused == sibling.thumbs[0], "blur cleared unrelated focus");
    disabled.set(true);
    check(!ref.focus(), "ref focused globally disabled slider");
    disabled.set(false);
    bool rejected{};
    std::thread worker([&] {
        try {
            static_cast<void>(ref.focus());
        } catch (const std::logic_error&) {
            rejected = true;
        }
    });
    worker.join();
    check(rejected, "SliderRef accepted wrong owner thread");
    Fixture duplicate;
    rejects([&] { duplicate.services.mount(Content{[&] { Slider(SliderProps{}.ref(ref)); }}); });
    check(duplicate.services.interactions().size() == 0 && ref.bound(),
          "duplicate ref binding leaked or unbound original");
    f.services.destroy(m.component);
    check(!ref.bound() && !ref.focus() && !ref.blur(), "destroyed generation kept callable ref");
    Fixture reuse;
    reuse.services.mount(Content{[&] { Slider(SliderProps{}.ref(ref)); }});
    reuse.synchronize();
    check(ref.focus() && reuse.services.focus().state().focused == reuse.services.slider().mounted()[0].thumbs[0],
          "unbound ref could not be reused");
    SliderRef failed_ref;
    Fixture rollback;
    try {
        rollback.services.mount(Content{[&] {
            Slider(SliderProps{}.ref(failed_ref));
            throw std::runtime_error("rollback");
        }});
    } catch (const std::runtime_error&) {
    }
    check(!failed_ref.bound(), "failed mount retained ref binding");
    SliderRef empty_ref;
    Fixture empty;
    empty.services.mount(
        Content{[&] { MultiSlider(MultiSliderProps{}.defaultValue({}).ref(empty_ref).autoFocus(true)); }});
    check(empty_ref.bound() && !empty_ref.focus() && !empty.services.focus().state().focused,
          "empty slider focused fake handle");
    Fixture hints;
    Signal<SliderHintOptions> hint{SliderHintOptions{SliderHintMode::Always, {}, false}};
    Signal<SliderOrientation> orientation{SliderOrientation::Vertical};
    hints.services.mount(Content{[&] {
        Slider(SliderProps{}.defaultValue(50).hint(hint).orientation(orientation).hintFormatter([](double) {
            return String{u8"long value hint 中文提示"};
        }));
    }});
    hints.synchronize();
    const auto tooltip = hints.services.tooltip().mounted()[0];
    check(hints.services.tooltip().snapshot(tooltip).placement == TooltipPlacement::Right,
          "default vertical hint did not use Right");
    orientation.set(SliderOrientation::Horizontal);
    hints.synchronize();
    const auto unchecked = hints.services.tooltip().snapshot(tooltip);
    check(unchecked.placement == TooltipPlacement::Top && unchecked.bounds.y < 0,
          "horizontal default or overflow=false lost");
    hint.set({SliderHintMode::Always, {}, true});
    hints.synchronize();
    check(hints.services.tooltip().snapshot(tooltip).bounds.y >= 0, "hint overflow=true did not adjust window bounds");
    rejects([&] { hint.set({SliderHintMode::Always, static_cast<TooltipPlacement>(99), true}); });
}

void controlled_keyboard_and_limits() {
    Fixture f;
    Signal<double> value{30};
    Signal<SliderLimits> limits{SliderLimits{}};
    Signal<bool> disabled{false};
    Signal<bool> keyboard{true};
    Signal<bool> reverse{false};
    std::vector<double> changes;
    std::vector<double> completed;
    int content_runs{};
    f.services.mount(Content{[&] {
        ++content_runs;
        Slider(SliderProps{}
                   .value(value)
                   .limits(limits)
                   .disabled(disabled)
                   .keyboard(keyboard)
                   .reverse(reverse)
                   .onChange([&](double v) { changes.push_back(v); })
                   .onChangeComplete([&](double v) { completed.push_back(v); }));
        Slider(SliderProps{}.defaultValue(60));
    }});
    f.synchronize();
    const auto m = f.services.slider().mounted()[0];
    check(f.services.focus().request_focus(m.thumbs[0], FocusModality::keyboard), "Slider focus failed");
    key(f, Key::right);
    key(f, Key::right, KeyAction::down, true);
    key(f, Key::right, KeyAction::up);
    check(changes == std::vector<double>{31, 32} && completed == std::vector<double>{32},
          "controlled repeat/completion candidates wrong");
    check(f.services.slider().snapshot(m.component).value == SliderRange{30, 30},
          "controlled Slider changed without echo");
    value.set(32);
    press_key(f, Key::page_up);
    check(changes.back() == 42 && completed.back() == 42, "page key did not step ten");
    press_key(f, Key::home);
    check(changes.back() == 0, "Home missing");
    press_key(f, Key::end);
    check(changes.back() == 100, "End missing");
    reverse.set(true);
    press_key(f, Key::right);
    check(changes.back() == 31, "reverse direction key wrong");
    keyboard.set(false);
    const auto count = changes.size();
    press_key(f, Key::left);
    check(changes.size() == count, "keyboard=false adjusted value");
    limits.set({40, 80, 5});
    f.synchronize();
    check(f.services.slider().snapshot(m.component).value.lower == 40, "dynamic limits did not normalize");
    const auto old = f.services.slider().snapshot(m.component);
    rejects([&] { limits.set({80, 40, 5}); });
    check(f.services.slider().snapshot(m.component).limits == old.limits, "invalid update mutated component");
    limits.set(old.limits);
    rejects([&] { value.set(NAN); });
    check(f.services.slider().snapshot(m.component).value == old.value, "invalid value mutated component");
    value.set(32);
    disabled.set(true);
    check(!f.services.focus().state().focused, "disabled thumb retained focus");
    check(content_runs == 1, "Slider property update reran content");
}

void range_focus_pointer_cancel_and_lifecycle() {
    Fixture f;
    Signal<bool> disabled{false};
    int changes{};
    int complete{};
    f.services.mount(Content{[&] {
        RangeSlider(RangeSliderProps{}
                        .defaultValue({20, 80})
                        .disabled(disabled)
                        .onChange([&](SliderRange) { ++changes; })
                        .onChangeComplete([&](SliderRange) { ++complete; })
                        .layout(LayoutStyle{}.width(dp(240))));
        Slider(SliderProps{}.defaultValue(50));
    }});
    f.synchronize();
    const auto m = f.services.slider().mounted()[0];
    key(f, Key::tab);
    check(f.services.focus().state().focused == m.thumbs[0], "first range Tab stop missing");
    key(f, Key::tab);
    check(f.services.focus().state().focused == m.thumbs[1], "second range Tab stop missing");
    press_key(f, Key::left);
    check(f.services.slider().snapshot(m.component).value == SliderRange{20, 79}, "upper key changed lower");
    const auto touch = PointerIdentity::touch(1, 7);
    pointer(f, PointerAction::down, at(f, m, 0.2), touch);
    pointer(f, PointerAction::move, at(f, m, 1.2), touch);
    check(f.services.slider().snapshot(m.component).value == SliderRange{79, 79}, "range endpoint crossed");
    const auto done = complete;
    pointer(f, PointerAction::cancel, at(f, m, 1.2), touch);
    check(!f.services.slider().snapshot(m.component).dragging && complete == done &&
              !f.services.pointer().state(touch)->capture,
          "cancel completed or retained capture");
    pointer(f, PointerAction::down, at(f, m, 0.1));
    pointer(f, PointerAction::up, at(f, m, 0.1));
    f.synchronize();
    check(f.services.slider().snapshot(m.component).value.lower == 10 && complete == done + 1,
          "rail click failed after cancel");
    pointer(f, PointerAction::down, at(f, m, 0.1));
    disabled.set(true);
    check(!f.services.slider().snapshot(m.component).dragging &&
              !f.services.pointer().state(PointerIdentity::mouse())->capture,
          "disabled did not cancel capture");
    disabled.set(false);
    f.synchronize();
    pointer(f, PointerAction::down, at(f, m, 0.1));
    f.services.set_window_active(false);
    check(!f.services.slider().snapshot(m.component).dragging, "window loss did not cancel");
    f.services.set_window_active(true);
    pointer(f, PointerAction::down, at(f, m, 0.1));
    check(f.services.destroy(m.component), "Slider destroy failed");
    check(f.services.slider().mounted().size() == 1 && f.services.interactions().size() == 4 &&
              f.services.tooltip().mounted().size() == 1,
          "range destroy damaged sibling or leaked interactions");
    check(changes > 1, "pointer candidates missing");
}

void labels_dots_hints_and_dynamic_slots() {
    Fixture f;
    Signal<SliderMarks> marks{SliderMarks{{0, String{u8"低 Low"}}, {50, String{u8"中"}}, {100, String{u8"高 High"}}}};
    Signal<bool> included{true};
    Signal<bool> disabled{false};
    Signal<SliderHintOptions> hint{SliderHintOptions{SliderHintMode::Auto, TooltipPlacement::Top}};
    int runs{};
    int changes{};
    int completed{};
    int formatted{};
    f.services.mount(Content{[&] {
        ++runs;
        Slider(SliderProps{}
                   .defaultValue(50)
                   .limits(SliderLimits{0, 100, 25})
                   .marks(marks)
                   .dots(true)
                   .included(included)
                   .disabled(disabled)
                   .hint(hint)
                   .hintFormatter([&](double v) {
                       ++formatted;
                       return v == 100 ? String{u8"最大 100"} : String{u8"数值"};
                   })
                   .onChange([&](double) { ++changes; })
                   .onChangeComplete([&](double) { ++completed; })
                   .layout(LayoutStyle{}.width(dp(280))));
        Text(u8"sibling");
    }});
    f.synchronize();
    const auto m = f.services.slider().mounted()[0];
    const auto tooltip = f.services.tooltip().mounted()[0];
    const auto thumb = m.thumbs[0];
    const auto roots = f.services.components().children(m.component);
    check(roots.size() == 4 && f.nodes.require(m.node).bounds.height > 32, "labels missing from slider layout");
    const auto label_node = f.services.components().root(roots.back());
    const auto label_bounds = f.nodes.require(label_node).bounds;
    pointer(f, PointerAction::down, {label_bounds.x + 3, label_bounds.y + 3});
    pointer(f, PointerAction::up, {label_bounds.x + 3, label_bounds.y + 3});
    f.synchronize();
    check(f.services.slider().snapshot(m.component).value.lower == 100 && changes == 1 && completed == 1,
          "mark label click did not select and complete");
    check(!f.services.tooltip().snapshot(tooltip).visible, "pointer focus incorrectly showed Auto hint");
    check(f.services.focus().request_focus(thumb, FocusModality::keyboard), "marked Slider focus failed");
    f.synchronize();
    check(f.services.tooltip().snapshot(tooltip).visible, "keyboard focus did not show Auto hint");
    key(f, Key::escape);
    f.synchronize();
    check(!f.services.tooltip().snapshot(tooltip).visible && f.services.focus().state().focused == thumb,
          "hint Escape did not preserve thumb focus");
    f.synchronize();
    check(!f.services.tooltip().snapshot(tooltip).visible, "dismissed hint reopened during same focus");
    f.services.focus().clear_focus();
    f.services.focus().request_focus(thumb, FocusModality::keyboard);
    f.synchronize();
    check(f.services.tooltip().snapshot(tooltip).visible, "hint did not reopen for new focus");
    const auto before = f.services.surfaces().diagnostics();
    const auto measures = f.layout.generation();
    const auto formatted_before = formatted;
    f.synchronize();
    check(f.layout.generation() == measures && formatted == formatted_before &&
              f.services.surfaces().diagnostics().geometry_updates == before.geometry_updates &&
              f.services.surfaces().diagnostics().material_updates == before.material_updates &&
              !f.services.next_frame_deadline(),
          "idle marked Slider updated retained resources");
    included.set(false);
    f.synchronize();
    const auto range = f.services.surfaces().visual_range(m.surface);
    check(f.services.surfaces().instances().instances()[range.first + 1].bounds[2] == 0,
          "included=false retained selected track");
    marks.set({{25, String{u8"四分之一"}}, {75, String{u8"四分之三"}}, {100, String{}}, {50, String{u8"二分之一"}}});
    f.synchronize();
    check(f.services.components().children(m.component).size() == 5 && runs == 1 &&
              f.services.slider().mounted()[0].thumbs[0] == thumb && f.services.tooltip().mounted()[0] == tooltip &&
              f.services.focus().state().focused == thumb && changes == 1,
          "growing marks remounted thumb, hint or unrelated content");
    marks.set({{0, String{u8"only"}}});
    f.synchronize();
    check(f.services.components().children(m.component).size() == 2 && f.services.focus().state().focused == thumb,
          "shrinking marks damaged focused thumb");
    const auto components = f.services.components().component_count();
    const auto texts = f.scene.size();
    const auto mounted_texts = f.services.text().mounted_texts().size();
    const auto interactions = f.services.interactions().size();
    bool failed{};
    try {
        f.services.append_slot(m.component, Content{[] {
                                   Tooltip(TooltipProps{}.title(String{u8"rollback"}),
                                           TooltipTrigger{[] { Text(u8"temporary"); }});
                                   throw std::runtime_error("rollback");
                               }});
    } catch (const std::runtime_error&) {
        failed = true;
    }
    check(failed && f.services.components().component_count() == components && f.scene.size() == texts &&
              f.services.interactions().size() == interactions && f.services.tooltip().mounted().size() == 1 &&
              f.services.text().mounted_texts().size() == mounted_texts,
          "failed dynamic slot leaked subtree resources");
    marks.set({});
    f.synchronize();
    check(f.nodes.require(m.node).bounds.height == 32 && f.services.components().children(m.component).size() == 1,
          "empty marks retained labels or cross-axis reservation");
    disabled.set(true);
    f.synchronize();
    check(!f.services.tooltip().snapshot(tooltip).visible, "disabled Slider retained hint");
    hint.set({SliderHintMode::Always, TooltipPlacement::Right});
    disabled.set(false);
    f.synchronize();
    check(f.services.tooltip().snapshot(tooltip).visible, "Always hint missing");
    hint.set({SliderHintMode::Hidden, TooltipPlacement::Right});
    f.synchronize();
    check(!f.services.tooltip().snapshot(tooltip).visible, "Hidden hint still visible");
    check(f.services.destroy(m.component), "marked Slider destroy failed");
    f.synchronize();
    check(f.scene.size() == 1 && f.services.interactions().size() == 0 && f.services.tooltip().mounted().empty(),
          "marked Slider destroy leaked labels, hints or interactions");
}

void marked_theme_invalidation() {
    Fixture f;
    ThemeConfig config;
    Signal<ThemeConfig> theme{config};
    f.services.mount(Content{[&] {
        Theme(ThemeProps{}.config(theme),
              ThemeContent{[] { Slider(SliderProps{}.marks(SliderMarks{{50, String{u8"中点"}}}).defaultValue(50)); }});
    }});
    f.synchronize();
    const auto m = f.services.slider().mounted()[0];
    const auto label = f.services.components().children(m.component).back();
    const auto text = f.services.components().children(label).front();
    const auto generation = f.layout.generation();
    const auto before = f.services.surfaces().diagnostics();
    config.slider.tokens.mark_active_text = Color::rgba8(250, 0, 0);
    config.slider.tokens.dot_active_border = Color::rgba8(0, 255, 0);
    theme.set(config);
    f.synchronize();
    check(f.layout.generation() == generation &&
              f.services.surfaces().diagnostics().geometry_updates == before.geometry_updates,
          "mark colors invalidated layout or geometry");
    config.slider.tokens.mark_font_size = dp(20);
    config.slider.tokens.mark_line_height = dp(30);
    config.slider.tokens.mark_gap = dp(12);
    theme.set(config);
    f.synchronize();
    check(f.layout.generation() > generation && f.nodes.require(m.node).bounds.height == 74 &&
              f.services.text().resolved_typography(text).font_size == 20,
          "mark metrics did not remeasure retained label");
    config.typography.tokens.font_weight = 700;
    theme.set(config);
    f.synchronize();
    check(f.services.text().resolved_typography(text).font_weight == 700, "label ignored theme font change");
    rejects([] {
        ThemeConfig invalid;
        invalid.slider.tokens.dot_size = dp(0);
        (void)resolve_theme(invalid);
    });
    rejects([] {
        ThemeConfig invalid;
        invalid.slider.tokens.mark_gap = dp(std::numeric_limits<float>::max());
        invalid.slider.tokens.mark_line_height = dp(std::numeric_limits<float>::max());
        (void)resolve_theme(invalid);
    });
}

void controlled_hint_and_formatter_reentrancy() {
    Fixture f;
    Signal<double> value{25};
    std::vector<double> formatted;
    f.services.mount(Content{[&] {
        Slider(SliderProps{}
                   .value(value)
                   .hint(SliderHintOptions{SliderHintMode::Always, TooltipPlacement::Top})
                   .hintFormatter([&](double v) {
                       formatted.push_back(v);
                       return String{u8"hint"};
                   }));
    }});
    f.synchronize();
    const auto m = f.services.slider().mounted()[0];
    f.services.focus().request_focus(m.thumbs[0], FocusModality::keyboard);
    press_key(f, Key::right);
    check(formatted == std::vector<double>{25}, "controlled hint formatted unacknowledged candidate");
    value.set(26);
    f.synchronize();
    check(formatted == std::vector<double>({25, 26}), "controlled writeback did not update hint");
    runtime::ComponentId dying;
    Fixture self;
    Signal<double> changing{10};
    self.services.mount(Content{[&] {
        Slider(SliderProps{}.value(changing).hintFormatter([&](double) {
            if (dying.valid()) {
                self.services.destroy(dying);
            }
            return String{u8"destroy"};
        }));
    }});
    dying = self.services.slider().mounted()[0].component;
    changing.set(20);
    self.synchronize();
    check(self.services.slider().mounted().empty() && self.services.tooltip().mounted().empty() &&
              self.scene.size() == 0 && self.services.interactions().size() == 0,
          "reentrant hint formatter destruction retained resources");
}

void geometry_theme_and_reentrancy() {
    Fixture f;
    ThemeConfig config;
    Signal<ThemeConfig> theme{config};
    Signal<SliderOrientation> orientation{SliderOrientation::Vertical};
    Signal<bool> reverse{false};
    f.services.mount(Content{[&] {
        Theme(ThemeProps{}.config(theme), ThemeContent{[&] {
                  Slider(SliderProps{}
                             .defaultValue(0)
                             .orientation(orientation)
                             .reverse(reverse)
                             .layout(LayoutStyle{}.height(dp(160))));
              }});
    }});
    f.synchronize();
    const auto m = f.services.slider().mounted()[0];
    const auto zero = f.services.slider().snapshot(m.component).centers[0];
    const auto b = f.nodes.require(m.node).bounds;
    check(zero.y > b.y + b.height / 2, "vertical zero not at bottom");
    pointer(f, PointerAction::down, at(f, m, 0.25));
    pointer(f, PointerAction::up, at(f, m, 0.25));
    f.synchronize();
    check(f.services.slider().snapshot(m.component).value.lower == 75, "vertical coordinate mapping wrong");
    reverse.set(true);
    f.synchronize();
    check(f.services.slider().snapshot(m.component).centers[0].y > b.y + b.height / 2,
          "vertical reverse mapping wrong");
    const auto before = f.services.surfaces().diagnostics();
    const auto generation = f.layout.generation();
    config.slider.tokens.track = Color::rgba8(255, 0, 0);
    theme.set(config);
    f.synchronize();
    check(f.layout.generation() == generation &&
              f.services.surfaces().diagnostics().geometry_updates == before.geometry_updates,
          "color token caused layout/geometry update");
    config.slider.tokens.handle_size_hover = dp(20);
    theme.set(config);
    f.synchronize();
    check(f.layout.generation() > generation, "metric token did not remeasure");
    const auto normal = resolve_theme({});
    check(normal.slider().metrics.handle_size == 10 && normal.slider().metrics.handle_size_hover == 12,
          "Ant Slider default metrics wrong");
    check(normal.slider().colors.track == Color::rgba8(145, 202, 255) &&
              normal.slider().colors.track_hover == Color::rgba8(105, 177, 255),
          "Ant Slider track colors wrong");
    ThemeConfig dark;
    dark.algorithms = {ThemeAlgorithm::Dark};
    check(resolve_theme(dark).slider().colors.rail != normal.slider().colors.rail, "Dark slider did not derive");
    ThemeConfig compact;
    compact.algorithms = {ThemeAlgorithm::Compact};
    check(resolve_theme(compact).slider().metrics != normal.slider().metrics, "Compact slider did not derive");
    const auto overridden = resolve_theme(config);
    check(overridden.identity() != normal.identity() &&
              overridden.diagnostic_json().find("\"slider\"") != std::string::npos,
          "Slider absent from identity/JSON");
    check(resolve_theme({}, &overridden).slider() == overridden.slider(), "Slider token inheritance missing");
    ThemeConfig brand;
    brand.slider.algorithm = true;
    brand.slider.seed.color_primary = Color::rgba8(0, 180, 90);
    check(resolve_theme(brand).slider().colors.handle_active == Color::rgba8(0, 180, 90) &&
              resolve_theme(brand).map() == normal.map(),
          "Slider component algorithm affected global map");
    Fixture overlap;
    overlap.services.mount(Content{[] { RangeSlider(RangeSliderProps{}.defaultValue({50, 50})); }});
    overlap.synchronize();
    const auto pair = overlap.services.slider().mounted()[0];
    check(overlap.services.focus().request_focus(pair.thumbs[0], FocusModality::keyboard),
          "overlap lower focus failed");
    pointer(overlap, PointerAction::down, at(overlap, pair, 0.5));
    pointer(overlap, PointerAction::move, at(overlap, pair, 0.2));
    pointer(overlap, PointerAction::up, at(overlap, pair, 0.2));
    check(overlap.services.slider().snapshot(pair.component).value == SliderRange{20, 50},
          "overlap did not preserve active thumb");
    Fixture self;
    runtime::ComponentId id;
    self.services.mount(Content{[&] { Slider(SliderProps{}.onChange([&](double) { self.services.destroy(id); })); }});
    self.synchronize();
    const auto own = self.services.slider().mounted()[0];
    id = own.component;
    pointer(self, PointerAction::down, at(self, own, 0.5));
    check(self.services.slider().mounted().empty() && self.services.interactions().size() == 0,
          "self-destroy callback leaked Slider");
}
} // namespace

int main() {
#if defined(_MSC_VER) && defined(_DEBUG)
    _CrtSetReportMode(_CRT_ASSERT, _CRTDBG_MODE_FILE);
    _CrtSetReportFile(_CRT_ASSERT, _CRTDBG_FILE_STDERR);
    _set_abort_behavior(0, _WRITE_ABORT_MSG | _CALL_REPORTFAULT);
#endif
    try {
        marks_numeric_contracts();
        numeric_and_api();
        multiple_values_and_retained_topology();
        whole_track_drag_contracts();
        editable_and_disabled_contracts();
        native_refs_and_hint_options();
        controlled_keyboard_and_limits();
        range_focus_pointer_cancel_and_lifecycle();
        geometry_theme_and_reentrancy();
        labels_dots_hints_and_dynamic_slots();
        controlled_hint_and_formatter_reentrancy();
        marked_theme_invalidation();
        std::cout << "Slider contracts passed\n";
        return 0;
    } catch (const std::exception& e) {
        std::cerr << e.what() << '\n';
        return 1;
    }
}
