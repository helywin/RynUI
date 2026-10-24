#include "theme/theme_runtime.hpp"
#include <iostream>
#include <limits>

namespace {
using namespace ryn;
using namespace ryn::theme_runtime;

void check(bool value, const char* message) {
    if (!value) {
        throw std::runtime_error(message);
    }
}

void contracts() {
    const auto base = resolve_theme();
    const auto& t = base.time_picker();
    check(t.metrics.column_width == 56 && t.metrics.column_height == 224 && t.metrics.cell_height == 28 &&
              t.metrics.font_size == 14 && t.metrics.line_height == 22 && t.z_index_popup == 1050,
          "locked time tokens");
    ThemeConfig config;
    config.time_picker.tokens.column_width = dp(72);
    config.time_picker.tokens.text = Color::rgba8(12, 30, 90);
    const auto custom = resolve_theme(config);
    ThemeConfig child_config;
    child_config.time_picker.seed.font_size = dp(20);
    check(resolve_theme(child_config, &custom).time_picker() == custom.time_picker(),
          "disabled seed discarded inheritance");
    child_config.time_picker.tokens.gap = dp(7);
    const auto inherited = resolve_theme(child_config, &custom);
    check(inherited.time_picker().metrics.column_width == 72 && inherited.time_picker().metrics.gap == 7 &&
              inherited.time_picker().metrics.font_size == 14,
          "partial inheritance");
    child_config.inherit = false;
    check(resolve_theme(child_config, &custom).time_picker().metrics.column_width == 56, "inherit false");
    config = {};
    config.time_picker.algorithm = true;
    config.time_picker.seed.font_size = dp(20);
    const auto derived = resolve_theme(config);
    check(derived.time_picker().metrics.font_size == 20 && derived.input_number() == base.input_number() &&
              derived.button() == base.button(),
          "local algorithm leaked");
    for (auto algorithm : {ThemeAlgorithm::Dark, ThemeAlgorithm::Compact}) {
        config = {};
        config.algorithms = {algorithm};
        check(resolve_theme(config).time_picker() != base.time_picker(), "algorithm ignored");
    }
    check(custom != base && custom.identity() != base.identity() &&
              custom.diagnostic_json().find("\"timePicker\":") != std::string::npos,
          "theme identity/serialization");
    auto parent = ThemeScope::create_default();
    auto child = ThemeScope::create(parent, {});
    int colors = 0;
    int metrics = 0;
    int effects = 0;
    int order = 0;
    auto a = child->capture(
        [&](DirtyPhase p) {
            check(p == DirtyPhase::paint_material, "color dirty");
            ++colors;
        },
        [&] { (void)child->time_picker_colors(); });
    auto b = child->capture(
        [&](DirtyPhase p) {
            check(has_any(p, DirtyPhase::hit_test) && has_any(p, DirtyPhase::text), "metric dirty");
            ++metrics;
        },
        [&] { (void)child->time_picker_metrics(); });
    auto c = child->capture(
        [&](DirtyPhase p) {
            check(has_any(p, DirtyPhase::geometry), "effect dirty");
            ++effects;
        },
        [&] { (void)child->time_picker_effects(); });
    auto d = child->capture(
        [&](DirtyPhase p) {
            check(p == DirtyPhase::paint_material, "order dirty");
            ++order;
        },
        [&] { (void)child->time_picker_order(); });
    config = {};
    config.time_picker.tokens.text = Color::rgba8(22, 33, 44);
    parent->update(config);
    config.time_picker.tokens.column_width = dp(60);
    parent->update(config);
    config.time_picker.tokens.shadow = ShadowList{};
    parent->update(config);
    config.time_picker.tokens.z_index_popup = 1070;
    parent->update(config);
    check(colors == 1 && metrics == 1 && effects == 1 && order == 1, "theme dirty domains");
    const auto before = parent->snapshot();
    for (int mode = 0; mode < 7; ++mode) {
        auto invalid = config;
        switch (mode) {
        case 0:
            invalid.time_picker.tokens.cell_height = dp(-1);
            break;
        case 1:
            invalid.time_picker.tokens.font_size = dp(0.1F);
            break;
        case 2:
            invalid.time_picker.tokens.column_width = dp(std::numeric_limits<float>::infinity());
            break;
        case 3:
            invalid.time_picker.tokens.column_height = dp(std::numeric_limits<float>::max());
            break;
        case 4:
            invalid.time_picker.tokens.line_height = dp(std::numeric_limits<float>::quiet_NaN());
            break;
        case 5:
            invalid.algorithms = {ThemeAlgorithm::Compact};
            invalid.time_picker.algorithm = true;
            invalid.time_picker.seed.size_step = dp(2);
            break;
        case 6:
            invalid.time_picker.tokens.font_size = dp(65536);
            break;
        }
        bool rejected = false;
        try {
            parent->update(invalid);
        } catch (const std::invalid_argument&) {
            rejected = true;
        }
        check(rejected && parent->snapshot() == before && child->snapshot().time_picker() == before.time_picker(),
              "invalid final theme partially published");
    }
    // A descendant failure must be preflighted before changing the parent.
    auto descendant_config = ThemeConfig{};
    descendant_config.time_picker.algorithm = true;
    descendant_config.time_picker.seed.size_step = dp(2);
    auto grandchild = ThemeScope::create(child, descendant_config);
    const auto grandchild_before = grandchild->snapshot();
    config.algorithms = {ThemeAlgorithm::Compact};
    bool rejected = false;
    try {
        parent->update(config);
    } catch (const std::invalid_argument&) {
        rejected = true;
    }
    check(rejected && parent->snapshot() == before && grandchild->snapshot() == grandchild_before,
          "inherited Theme preflight changed state");
}
} // namespace

int main() {
    try {
        contracts();
        std::cout << "TimePicker Theme contracts passed\n";
        return 0;
    } catch (const std::exception& e) {
        std::cerr << e.what() << '\n';
        return 1;
    }
}
