#include "theme/theme_runtime.hpp"
#include <iostream>
#include <limits>
#include <stdexcept>

namespace {
using namespace ryn;
using namespace ryn::theme_runtime;

void check(bool ok, const char* message) {
    if (!ok) {
        throw std::runtime_error(message);
    }
}

void recipe() {
    const auto base = std::make_unique<ThemeSnapshot>(resolve_theme());
    const auto& t = base->statistic();
    check(t.metrics.title_font_size == 14 && t.metrics.content_font_size == 24 && t.metrics.title_gap == 4 &&
              t.metrics.affix_gap == 4 && t.metrics.loading_padding == 16,
          "locked recipe drifted");
    auto config = std::make_unique<ThemeConfig>();
    config->statistic.tokens.content = Color::rgba8(12, 34, 56);
    const auto parent = std::make_unique<ThemeSnapshot>(resolve_theme(*config));
    check(*parent != *base && parent->identity() != base->identity() && parent->timeline() == base->timeline() &&
              parent->diagnostic_json().find("\"statistic\":") != std::string::npos,
          "Theme identity/isolation missing");
    auto local = std::make_unique<ThemeConfig>();
    local->statistic.seed.font_size = dp(21);
    local->statistic.seed.size_unit = dp(-3);
    check(resolve_theme(*local, parent.get()).statistic() == parent->statistic(), "inactive seed changed output");
    local->statistic.seed.size_unit.reset();
    local->statistic.algorithm = true;
    check(resolve_theme(*local).statistic().metrics.title_font_size == 21, "independent seed missing");
    local->statistic.algorithm = false;
    local->inherit = false;
    check(resolve_theme(*local, parent.get()).statistic() == t, "inherit false failed");
    for (auto algorithm : {ThemeAlgorithm::Dark, ThemeAlgorithm::Compact}) {
        config = std::make_unique<ThemeConfig>();
        config->algorithms = {algorithm};
        const auto variant = std::make_unique<ThemeSnapshot>(resolve_theme(*config));
        check(variant->statistic().colors.content == variant->alias().color_text &&
                  variant->statistic().metrics.title_font_size == variant->map().font_size,
              "global algorithm missing");
        if (algorithm == ThemeAlgorithm::Compact) {
            check(variant->statistic().metrics.content_font_size == 20, "locked compact heading3 recipe drifted");
        }
    }
}

void validation() {
    auto scope = ThemeScope::create_default();
    int colors = 0;
    int metrics = 0;
    auto c = scope->capture(
        [&](DirtyPhase p) {
            check(p == DirtyPhase::paint_material, "color dirtied layout");
            ++colors;
        },
        [&] { static_cast<void>(scope->statistic_colors()); });
    auto m = scope->capture(
        [&](DirtyPhase p) {
            check(has_any(p, DirtyPhase::text | DirtyPhase::measure_layout | DirtyPhase::hit_test),
                  "metric phases missing");
            ++metrics;
        },
        [&] { static_cast<void>(scope->statistic_metrics()); });
    auto config = std::make_unique<ThemeConfig>();
    config->statistic.tokens.content = Color::rgba8(23, 45, 67);
    scope->update(*config);
    check(colors == 1 && metrics == 0 && scope->changed_identities().size() == 1 &&
              scope->changed_identities()[0] == TokenIdentity::statistic_colors,
          "color subscription leaked");
    config->statistic.tokens.content_font_size = dp(19);
    scope->update(*config);
    check(metrics == 1, "metric subscription missing");
    const auto before = std::make_unique<ThemeSnapshot>(scope->snapshot());
    for (float value : {.49F, std::numeric_limits<float>::quiet_NaN(), std::numeric_limits<float>::infinity(), 3e9F}) {
        auto bad = std::make_unique<ThemeConfig>(*config);
        bad->statistic.tokens.content_font_size = dp(value);
        bool rejected = false;
        try {
            scope->update(*bad);
        } catch (const std::invalid_argument&) {
            rejected = true;
        }
        check(rejected && scope->snapshot() == *before && metrics == 1, "invalid final font published");
    }
    auto bad = std::make_unique<ThemeConfig>();
    bad->statistic.algorithm = true;
    bad->statistic.seed.font_size = dp(.49F);
    bool rejected = false;
    try {
        static_cast<void>(resolve_theme(*bad));
    } catch (const std::invalid_argument&) {
        rejected = true;
    }
    check(rejected, "invalid derived font accepted");
    bad = std::make_unique<ThemeConfig>();
    bad->statistic.tokens.title_font_size = dp(.5F);
    bad->statistic.tokens.content_font_size = dp(.5F);
    check(resolve_theme(*bad).statistic().metrics.content_font_size == .5F, "minimum font rejected");
    config->statistic.tokens.content_font_size = dp(22);
    scope->update(*config);
    check(metrics == 2, "Theme recovery failed");
}

void preflight() {
    auto root = ThemeScope::create_default();
    auto config = std::make_unique<ThemeConfig>();
    config->statistic.tokens.affix_gap = dp(5e37F);
    auto child = ThemeScope::create(root, *config);
    auto grandchild = ThemeScope::create(child, ThemeConfig{});
    const auto* r = &root->snapshot();
    const auto* c = &child->snapshot();
    const auto* g = &grandchild->snapshot();
    config = std::make_unique<ThemeConfig>();
    config->statistic.tokens.loading_padding = dp(5e37F);
    bool rejected = false;
    try {
        root->update(*config);
    } catch (const std::invalid_argument&) {
        rejected = true;
    }
    check(rejected && r == &root->snapshot() && c == &child->snapshot() && g == &grandchild->snapshot(),
          "ancestor preflight not atomic");
    config->statistic.tokens.loading_padding = dp(20);
    root->update(*config);
    check(child->snapshot().statistic().metrics.loading_padding == 20 &&
              grandchild->snapshot().statistic() == child->snapshot().statistic(),
          "ancestor recovery failed");
}
} // namespace

int main() {
    try {
        recipe();
        validation();
        preflight();
        std::cout << "Statistic Theme passed\n";
        return 0;
    } catch (const std::exception& e) {
        std::cerr << e.what() << '\n';
        return 1;
    }
}
