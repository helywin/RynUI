#include "theme/theme_runtime.hpp"
#include <iostream>
#include <limits>
#include <stdexcept>
using namespace ryn;
using namespace ryn::theme_runtime;

void check(bool value, const char* message) {
    if (!value) {
        throw std::runtime_error(message);
    }
}

int main() {
    try {
        const auto base = resolve_theme();
        const auto& t = base.date_picker();
        check(t.metrics.cell_height == 24 && t.metrics.header_height == 40 && t.z_index_popup == 1050 &&
                  t.colors.selected == base.map().color_primary,
              "default recipe");
        ThemeConfig config;
        config.date_picker.tokens.text = Color::rgba8(20, 40, 60);
        config.date_picker.tokens.cell_height = dp(26);
        const auto parent = resolve_theme(config);
        check(resolve_theme({}, &parent).date_picker() == parent.date_picker(), "inherit");
        ThemeConfig reset;
        reset.inherit = false;
        check(resolve_theme(reset, &parent).date_picker() == t, "inherit false");
        check(parent.button() == base.button() && parent.segmented() == base.segmented(),
              "DatePicker leaked overrides");
        ThemeConfig local;
        local.date_picker.algorithm = true;
        local.date_picker.seed.color_primary = Color::rgba8(80, 20, 40);
        check(resolve_theme(local).date_picker().colors.selected != t.colors.selected &&
                  resolve_theme(local).button() == base.button(),
              "local algorithm");
        local.date_picker.algorithm = false;
        check(resolve_theme(local).date_picker() == t, "inactive seed");
        ThemeConfig dark;
        dark.algorithms = {ThemeAlgorithm::Dark};
        ThemeConfig compact;
        compact.algorithms = {ThemeAlgorithm::Compact};
        check(resolve_theme(dark).date_picker().colors.background != t.colors.background &&
                  resolve_theme(compact).date_picker().metrics.cell_height < t.metrics.cell_height,
              "Dark/Compact");
        check(parent.identity() != base.identity() &&
                  parent.diagnostic_json().find("\"datePicker\":") != std::string::npos,
              "identity/diagnostic");
        auto scope = ThemeScope::create_default();
        int colors = 0;
        int metrics = 0;
        int effects = 0;
        int order = 0;
        auto c = scope->capture(
            [&](DirtyPhase p) {
                check(p == DirtyPhase::paint_material, "color phase");
                ++colors;
            },
            [&] { (void)scope->date_picker_colors(); });
        auto m = scope->capture(
            [&](DirtyPhase p) {
                check(has_any(p, DirtyPhase::measure_layout | DirtyPhase::text | DirtyPhase::hit_test),
                      "metrics phase");
                ++metrics;
            },
            [&] { (void)scope->date_picker_metrics(); });
        auto e = scope->capture([&](DirtyPhase) { ++effects; }, [&] { (void)scope->date_picker_effects(); });
        auto o = scope->capture([&](DirtyPhase) { ++order; }, [&] { (void)scope->date_picker_order(); });
        ThemeConfig update;
        update.date_picker.tokens.text = Color::rgba8(10, 30, 50);
        check(scope->update(update) && colors == 1 && metrics == 0 && effects == 0 && order == 0, "dirty separation");
        check(!scope->update(update), "equal update");
        update.date_picker.tokens.row_height = dp(38);
        scope->update(update);
        update.date_picker.tokens.shadow = ShadowList{};
        scope->update(update);
        update.date_picker.tokens.z_index_popup = 1061;
        scope->update(update);
        check(metrics == 1 && effects == 1 && order == 1 && colors == 1, "dirty domains");
        const auto snapshot = scope->snapshot();
        for (auto invalid : {dp(0), dp(.5F), dp(-1), dp(std::numeric_limits<float>::infinity()), LogicalLength{}}) {
            auto bad = update;
            bad.date_picker.tokens.font_size = invalid;
            bool threw = false;
            try {
                scope->update(bad);
            } catch (const std::invalid_argument&) {
                threw = true;
            }
            check(threw && scope->snapshot() == snapshot, "invalid metric partially published");
        }
        std::cout << "DatePicker Theme passed\n";
    } catch (const std::exception& e) {
        std::cerr << e.what() << '\n';
        return 1;
    }
}
