#include "theme/theme_runtime.hpp"
#include <limits>
#include <iostream>

namespace {
using namespace ryn;
using namespace ryn::theme_runtime;

void check(bool ok, const char* message) {
    if (!ok) {
        throw std::runtime_error(message);
    }
}

void run() {
    auto base = std::make_unique<ThemeSnapshot>(resolve_theme());
    check(base->mentions().typography.selected_font_weight == 400 &&
              base->mentions().input.input_font_size->value() == 14,
          "locked Mentions editor/menu recipe");
    auto config = std::make_unique<ThemeConfig>();
    config->mentions.tokens.input.background = Color::rgba8(1, 2, 3);
    config->mentions.tokens.suggestions.option_height = dp(42);
    auto changed = std::make_unique<ThemeSnapshot>(resolve_theme(*config));
    check(*base != *changed && base->identity() != changed->identity() &&
              changed->auto_complete() == base->auto_complete() && changed->select() == base->select(),
          "independent identity/Select isolation");
    auto child = std::make_unique<ThemeConfig>();
    check(resolve_theme(*child, changed.get()).mentions() == changed->mentions(), "Mentions inherit");
    child->mentions.seed.font_size = dp(19);
    check(resolve_theme(*child, changed.get()).mentions() == changed->mentions(), "inactive component seed");
    child->mentions.algorithm = true;
    auto algorithm = std::make_unique<ThemeSnapshot>(resolve_theme(*child, changed.get()));
    check(algorithm->mentions().typography.font_size == 19 &&
              algorithm->mentions().input.input_font_size->value() == 19,
          "scoped algorithm does not affect editor");
    child->inherit = false;
    child->mentions.algorithm = false;
    check(resolve_theme(*child, changed.get()).mentions() == base->mentions(), "inherit false");
    auto root = ThemeScope::create_default();
    int input{};
    int colors{};
    int metrics{};
    int typography{};
    int effects{};
    int order{};
    auto a = root->capture(
        [&](DirtyPhase p) {
            check(has_any(p, DirtyPhase::measure_layout | DirtyPhase::text | DirtyPhase::paint_material |
                                 DirtyPhase::hit_test),
                  "input invalidation");
            ++input;
        },
        [&] { (void)root->mentions_input(); });
    auto b = root->capture(
        [&](DirtyPhase p) {
            check(p == DirtyPhase::paint_material, "color phase");
            ++colors;
        },
        [&] { (void)root->mentions_colors(); });
    auto c = root->capture(
        [&](DirtyPhase p) {
            check(has_any(p, DirtyPhase::measure_layout | DirtyPhase::geometry | DirtyPhase::hit_test), "metric phase");
            ++metrics;
        },
        [&] { (void)root->mentions_metrics(); });
    auto d = root->capture(
        [&](DirtyPhase p) {
            check(has_any(p, DirtyPhase::text | DirtyPhase::measure_layout), "typography phase");
            ++typography;
        },
        [&] { (void)root->mentions_typography(); });
    auto e = root->capture(
        [&](DirtyPhase p) {
            check(has_any(p, DirtyPhase::geometry | DirtyPhase::paint_material), "effects phase");
            ++effects;
        },
        [&] { (void)root->mentions_effects(); });
    auto f = root->capture(
        [&](DirtyPhase p) {
            check(has_any(p, DirtyPhase::hit_test | DirtyPhase::paint_material), "order phase");
            ++order;
        },
        [&] { (void)root->mentions_order(); });
    root->update(*config);
    check(input == 1 && metrics == 1 && colors == 0 && typography == 0 && effects == 0 && order == 0,
          "subscription isolation");
    config->mentions.tokens.suggestions.option_color = Color::rgba8(4, 5, 6);
    config->mentions.tokens.suggestions.font_size = dp(18);
    config->mentions.tokens.suggestions.shadow = ShadowList{};
    config->mentions.tokens.suggestions.z_index_popup = 1250;
    root->update(*config);
    check(input == 1 && metrics == 1 && colors == 1 && typography == 1 && effects == 1 && order == 1,
          "complete subscriptions");
    const auto identity = root->snapshot().identity();
    bool rejected{};
    config->mentions.tokens.input.padding_inline = dp(std::numeric_limits<float>::infinity());
    try {
        root->update(*config);
    } catch (const std::invalid_argument&) {
        rejected = true;
    }
    check(rejected && root->snapshot().identity() == identity, "invalid update not atomic");
}
} // namespace

int main() {
    try {
        run();
        std::cout << "Mentions Theme passed\n";
        return 0;
    } catch (const std::exception& e) {
        std::cerr << e.what() << '\n';
        return 1;
    }
}
