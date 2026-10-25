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

void contract() {
    const auto scope = ThemeScope::create_default();
    const auto base = scope->snapshot().popover();
    check(base.padding == 12 && base.title_min_width == 177 && base.title_gap == 8 && base.z_index_popup == 1030 &&
              base.font_size == 14 && base.line_height == 22 &&
              base.background == scope->snapshot().alias().color_background_elevated,
          "locked Popover recipe drifted");
    auto config = std::make_unique<ThemeConfig>();
    config->popover.tokens.padding = dp(20);
    config->popover.tokens.title = Color::rgba8(20, 80, 120);
    scope->update(*config);
    auto local = std::make_unique<ThemeConfig>();
    local->popover.seed.font_size = dp(18);
    auto child = ThemeScope::create(scope, *local);
    check(child->snapshot().popover() == scope->snapshot().popover(), "inactive seed lost parent override");
    local->popover.algorithm = true;
    child->update(*local);
    check(child->snapshot().popover().font_size == 18 && child->snapshot().button() == scope->snapshot().button(),
          "independent seed leaked");
    local->inherit = false;
    local->popover = {};
    child->update(*local);
    check(child->snapshot().popover() == base, "reset failed");
    check(scope->snapshot().identity() != child->snapshot().identity() && scope->snapshot() != child->snapshot() &&
              scope->snapshot().diagnostic_json().find("\"popover\":") != std::string::npos,
          "snapshot chain missing");
    int colors{};
    int metrics{};
    int fonts{};
    int effects{};
    int order{};
    auto c = scope->capture(
        [&](DirtyPhase p) {
            check(p == DirtyPhase::paint_material, "color remeasured");
            ++colors;
        },
        [&] { (void)scope->popover_colors(); });
    auto m = scope->capture([&](DirtyPhase) { ++metrics; }, [&] { (void)scope->popover_metrics(); });
    auto t = scope->capture([&](DirtyPhase) { ++fonts; }, [&] { (void)scope->popover_typography(); });
    auto e = scope->capture([&](DirtyPhase) { ++effects; }, [&] { (void)scope->popover_effects(); });
    auto o = scope->capture([&](DirtyPhase) { ++order; }, [&] { (void)scope->popover_order(); });
    config->popover.tokens.background = Color::rgba8(230, 240, 250);
    scope->update(*config);
    check(colors == 1 && !metrics && !fonts && !effects && !order, "color invalidation leaked");
    config->popover.tokens.padding = dp(14);
    scope->update(*config);
    config->popover.tokens.title_font_weight = 700;
    scope->update(*config);
    config->popover.tokens.shadow = ShadowList{};
    scope->update(*config);
    config->popover.tokens.z_index_popup = 1200;
    scope->update(*config);
    check(metrics == 1 && fonts == 1 && effects == 1 && order == 1 && !scope->update(*config),
          "grouped invalidation failed");
    const auto generation = scope->generation();
    const auto identity = scope->snapshot().identity();
    for (int mode = 0; mode < 7; ++mode) {
        auto invalid = std::make_unique<ThemeConfig>(*config);
        if (mode == 0) {
            invalid->popover.tokens.max_width = dp(0);
        }
        if (mode == 1) {
            invalid->popover.tokens.padding = dp(-1);
        }
        if (mode == 2) {
            invalid->popover.tokens.font_size = dp(0);
        }
        if (mode == 3) {
            invalid->popover.tokens.title_font_weight = 1001;
        }
        if (mode == 4) {
            invalid->popover.tokens.z_index_popup = -1;
        }
        if (mode == 5) {
            invalid->popover.tokens.padding = dp(std::numeric_limits<float>::max());
        }
        if (mode == 6) {
            invalid->popover.tokens.title_min_width = auto_length;
        }
        bool rejected{};
        try {
            scope->update(*invalid);
        } catch (const std::invalid_argument&) {
            rejected = true;
        }
        check(rejected && scope->generation() == generation && scope->snapshot().identity() == identity,
              "invalid theme partially published");
    }
    config->algorithms = {ThemeAlgorithm::Dark, ThemeAlgorithm::Compact};
    scope->update(*config);
    check(scope->snapshot().popover().font_size == 12, "compact font recipe failed");
}
} // namespace

int main() {
    try {
        contract();
    } catch (const std::exception& e) {
        std::cerr << e.what();
        return 1;
    }
}
