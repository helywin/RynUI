#include "component/slider_component.hpp"
#include "component/slider_value.hpp"
#include "support/input_fixture.hpp"
#include <ryn/rynui.hpp>
#include <cmath>
#include <iostream>
#include <limits>
#include <type_traits>

namespace {
using namespace ryn;
using namespace ryn::input;
using Fixture = ryn_test::input_component::Fixture;
void check(bool value, const char* message) { if (!value) throw std::runtime_error(message); }
bool near(double a, double b) { return std::abs(a - b) < 1e-8; }
template<class F> void rejects(F action) { bool rejected{}; try { action(); } catch (const std::invalid_argument&) { rejected = true; } check(rejected, "invalid Slider input accepted"); }
void key(Fixture& f, Key key, KeyAction action = KeyAction::down, bool repeat = false) { f.services.focus().dispatch({key, action, KeyModifier::none, repeat}); }
void press_key(Fixture& f, Key value) { key(f, value); key(f, value, KeyAction::up); f.synchronize(); }
void pointer(Fixture& f, PointerAction action, runtime::Point p, PointerIdentity identity = PointerIdentity::mouse()) {
    f.services.pointer().dispatch({identity, action, action == PointerAction::down || action == PointerAction::up ? PointerButton::primary : PointerButton::none, p.x, p.y});
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
void numeric_and_api() {
    static_assert(std::is_same_v<decltype(SliderProps{}.value(1.0).limits(SliderLimits{})), SliderProps&>);
    static_assert(std::is_same_v<decltype(RangeSliderProps{}.value(SliderRange{}).reverse(true)), RangeSliderProps&>);
    const SliderLimits fractional{-1, 1, 0.1}; detail::validate_slider_limits(fractional);
    check(near(detail::normalize_slider_value(0.26, fractional), 0.3), "fractional step truncated");
    check(near(detail::normalize_slider_value(-10, fractional), -1), "minimum clamp failed");
    check(near(detail::normalize_slider_value(10, fractional), 1), "maximum clamp failed");
    check(near(detail::normalize_slider_value(0.25, {0, 1, 0.5}), 0.5), "tie did not round up");
    check(near(detail::normalize_slider_value(0.95, {0, 1, 0.3}), 1), "extra maximum endpoint missing");
    check(detail::normalize_slider_range({90, 20}, {}) == SliderRange{20, 90}, "range did not sort");
    for (auto invalid : {SliderLimits{0, 0, 1}, SliderLimits{1, 0, 1}, SliderLimits{0, 1, 0},
        SliderLimits{0, 1, -1}, SliderLimits{0, 1, 1e-30}, SliderLimits{0, INFINITY, 1},
        SliderLimits{-1e308, 1e308, 1}, SliderLimits{1e20, 1e20 + 1e6, 1}})
        rejects([&] { detail::validate_slider_limits(invalid); });
    rejects([] { detail::normalize_slider_value(NAN, {}); });
    rejects([] { detail::normalize_slider_value(INFINITY, {}); });
    Fixture f;
    rejects([&] { f.services.mount(Content{[] { Slider(SliderProps{}.value(1).defaultValue(2)); }}); });
    check(f.services.slider().mounted().empty() && f.services.interactions().size() == 0, "invalid mode acquired resources");
    Fixture failed;
    bool aborted{};
    try { failed.services.mount(Content{[] { RangeSlider(RangeSliderProps{}); throw std::runtime_error("abort mount"); }}); }
    catch (const std::runtime_error&) { aborted = true; }
    check(aborted && failed.services.slider().mounted().empty() && failed.services.interactions().size() == 0
        && failed.services.surfaces().size() == 0, "failed mount retained Slider resources");
}
void controlled_keyboard_and_limits() {
    Fixture f; Signal<double> value{30}; Signal<SliderLimits> limits{SliderLimits{}};
    Signal<bool> disabled{false}, keyboard{true}, reverse{false};
    std::vector<double> changes, completed; int content_runs{};
    f.services.mount(Content{[&] {
        ++content_runs;
        Slider(SliderProps{}.value(value).limits(limits).disabled(disabled).keyboard(keyboard).reverse(reverse)
            .onChange([&](double v) { changes.push_back(v); }).onChangeComplete([&](double v) { completed.push_back(v); }));
        Slider(SliderProps{}.defaultValue(60));
    }}); f.synchronize();
    const auto m = f.services.slider().mounted()[0];
    check(f.services.focus().request_focus(m.thumbs[0], FocusModality::keyboard), "Slider focus failed");
    key(f, Key::right); key(f, Key::right, KeyAction::down, true); key(f, Key::right, KeyAction::up);
    check(changes == std::vector<double>{31, 32} && completed == std::vector<double>{32}, "controlled repeat/completion candidates wrong");
    check(f.services.slider().snapshot(m.component).value == SliderRange{30, 30}, "controlled Slider changed without echo");
    value.set(32); press_key(f, Key::page_up);
    check(changes.back() == 42 && completed.back() == 42, "page key did not step ten");
    press_key(f, Key::home); check(changes.back() == 0, "Home missing");
    press_key(f, Key::end); check(changes.back() == 100, "End missing");
    reverse.set(true); press_key(f, Key::right); check(changes.back() == 31, "reverse direction key wrong");
    keyboard.set(false); const auto count = changes.size(); press_key(f, Key::left); check(changes.size() == count, "keyboard=false adjusted value");
    limits.set({40, 80, 5}); f.synchronize(); check(f.services.slider().snapshot(m.component).value.lower == 40, "dynamic limits did not normalize");
    const auto old = f.services.slider().snapshot(m.component);
    rejects([&] { limits.set({80, 40, 5}); }); check(f.services.slider().snapshot(m.component).limits == old.limits, "invalid update mutated component");
    limits.set(old.limits);
    rejects([&] { value.set(NAN); }); check(f.services.slider().snapshot(m.component).value == old.value, "invalid value mutated component");
    value.set(32);
    disabled.set(true); check(!f.services.focus().state().focused, "disabled thumb retained focus");
    check(content_runs == 1, "Slider property update reran content");
}
void range_focus_pointer_cancel_and_lifecycle() {
    Fixture f; Signal<bool> disabled{false}; int changes{}, complete{};
    f.services.mount(Content{[&] { RangeSlider(RangeSliderProps{}.defaultValue({20, 80}).disabled(disabled)
        .onChange([&](SliderRange) { ++changes; }).onChangeComplete([&](SliderRange) { ++complete; })
        .layout(LayoutStyle{}.width(dp(240)))); Slider(SliderProps{}.defaultValue(50)); }}); f.synchronize();
    const auto m = f.services.slider().mounted()[0];
    key(f, Key::tab); check(f.services.focus().state().focused == m.thumbs[0], "first range Tab stop missing");
    key(f, Key::tab); check(f.services.focus().state().focused == m.thumbs[1], "second range Tab stop missing");
    press_key(f, Key::left); check(f.services.slider().snapshot(m.component).value == SliderRange{20, 79}, "upper key changed lower");
    const auto touch = PointerIdentity::touch(1, 7);
    pointer(f, PointerAction::down, at(f, m, 0.2), touch); pointer(f, PointerAction::move, at(f, m, 1.2), touch);
    check(f.services.slider().snapshot(m.component).value == SliderRange{79, 79}, "range endpoint crossed");
    const auto done = complete; pointer(f, PointerAction::cancel, at(f, m, 1.2), touch);
    check(!f.services.slider().snapshot(m.component).dragging && complete == done && !f.services.pointer().state(touch)->capture, "cancel completed or retained capture");
    pointer(f, PointerAction::down, at(f, m, 0.1)); pointer(f, PointerAction::up, at(f, m, 0.1)); f.synchronize();
    check(f.services.slider().snapshot(m.component).value.lower == 10 && complete == done + 1, "rail click failed after cancel");
    pointer(f, PointerAction::down, at(f, m, 0.1)); disabled.set(true);
    check(!f.services.slider().snapshot(m.component).dragging && !f.services.pointer().state(PointerIdentity::mouse())->capture, "disabled did not cancel capture");
    disabled.set(false); f.synchronize(); pointer(f, PointerAction::down, at(f, m, 0.1)); f.services.set_window_active(false);
    check(!f.services.slider().snapshot(m.component).dragging, "window loss did not cancel"); f.services.set_window_active(true);
    pointer(f, PointerAction::down, at(f, m, 0.1)); check(f.services.destroy(m.component), "Slider destroy failed");
    check(f.services.slider().mounted().size() == 1 && f.services.interactions().size() == 2, "range destroy damaged sibling or leaked interactions");
    check(changes > 1, "pointer candidates missing");
}
void geometry_theme_and_reentrancy() {
    Fixture f; ThemeConfig config; Signal<ThemeConfig> theme{config}; Signal<SliderOrientation> orientation{SliderOrientation::Vertical}; Signal<bool> reverse{false};
    f.services.mount(Content{[&] { Theme(ThemeProps{}.config(theme), ThemeContent{[&] {
        Slider(SliderProps{}.defaultValue(0).orientation(orientation).reverse(reverse).layout(LayoutStyle{}.height(dp(160))));
    }}); }}); f.synchronize();
    const auto m = f.services.slider().mounted()[0];
    const auto zero = f.services.slider().snapshot(m.component).centers[0];
    const auto b = f.nodes.require(m.node).bounds; check(zero.y > b.y + b.height / 2, "vertical zero not at bottom");
    pointer(f, PointerAction::down, at(f, m, 0.25)); pointer(f, PointerAction::up, at(f, m, 0.25)); f.synchronize();
    check(f.services.slider().snapshot(m.component).value.lower == 75, "vertical coordinate mapping wrong");
    reverse.set(true); f.synchronize(); check(f.services.slider().snapshot(m.component).centers[0].y > b.y + b.height / 2, "vertical reverse mapping wrong");
    const auto before = f.services.surfaces().diagnostics(); const auto generation = f.layout.generation();
    config.slider.tokens.track = Color::rgba8(255, 0, 0); theme.set(config); f.synchronize();
    check(f.layout.generation() == generation && f.services.surfaces().diagnostics().geometry_updates == before.geometry_updates,
        "color token caused layout/geometry update");
    config.slider.tokens.handle_size_hover = dp(20); theme.set(config); f.synchronize();
    check(f.layout.generation() > generation, "metric token did not remeasure");
    const auto normal = resolve_theme({}); check(normal.slider().metrics.handle_size == 10 && normal.slider().metrics.handle_size_hover == 12, "Ant Slider default metrics wrong");
    check(normal.slider().colors.track == Color::rgba8(145, 202, 255) && normal.slider().colors.track_hover == Color::rgba8(105, 177, 255), "Ant Slider track colors wrong");
    ThemeConfig dark; dark.algorithms = {ThemeAlgorithm::Dark}; check(resolve_theme(dark).slider().colors.rail != normal.slider().colors.rail, "Dark slider did not derive");
    ThemeConfig compact; compact.algorithms = {ThemeAlgorithm::Compact}; check(resolve_theme(compact).slider().metrics != normal.slider().metrics, "Compact slider did not derive");
    const auto overridden = resolve_theme(config); check(overridden.identity() != normal.identity() && overridden.diagnostic_json().find("\"slider\"") != std::string::npos, "Slider absent from identity/JSON");
    check(resolve_theme({}, &overridden).slider() == overridden.slider(), "Slider token inheritance missing");
    ThemeConfig brand; brand.slider.algorithm = true; brand.slider.seed.color_primary = Color::rgba8(0, 180, 90);
    check(resolve_theme(brand).slider().colors.handle_active == Color::rgba8(0, 180, 90)
        && resolve_theme(brand).map() == normal.map(), "Slider component algorithm affected global map");
    Fixture overlap; overlap.services.mount(Content{[] { RangeSlider(RangeSliderProps{}.defaultValue({50, 50})); }}); overlap.synchronize();
    const auto pair = overlap.services.slider().mounted()[0];
    check(overlap.services.focus().request_focus(pair.thumbs[0], FocusModality::keyboard), "overlap lower focus failed");
    pointer(overlap, PointerAction::down, at(overlap, pair, 0.5)); pointer(overlap, PointerAction::move, at(overlap, pair, 0.2)); pointer(overlap, PointerAction::up, at(overlap, pair, 0.2));
    check(overlap.services.slider().snapshot(pair.component).value == SliderRange{20, 50}, "overlap did not preserve active thumb");
    Fixture self; runtime::ComponentId id; self.services.mount(Content{[&] { Slider(SliderProps{}.onChange([&](double) { self.services.destroy(id); })); }}); self.synchronize();
    const auto own = self.services.slider().mounted()[0]; id = own.component;
    pointer(self, PointerAction::down, at(self, own, 0.5));
    check(self.services.slider().mounted().empty() && self.services.interactions().size() == 0, "self-destroy callback leaked Slider");
}
}
int main() { try { numeric_and_api(); controlled_keyboard_and_limits(); range_focus_pointer_cancel_and_lifecycle(); geometry_theme_and_reentrancy(); std::cout << "Slider contracts passed\n"; return 0; } catch (const std::exception& e) { std::cerr << e.what() << '\n'; return 1; } }
