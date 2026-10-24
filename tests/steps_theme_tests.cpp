#include "theme/theme_runtime.hpp"
#include <iostream>
#include <limits>
#include <memory>

namespace {
using namespace ryn;
using namespace ryn::theme_runtime;

void check(bool value, const char* message) {
    if (!value) {
        throw std::runtime_error(message);
    }
}

void inheritance() {
    const auto base = std::make_unique<ThemeSnapshot>(resolve_theme());
    const auto parent = ThemeScope::create_default();
    auto config = std::make_unique<ThemeConfig>();
    config->steps.tokens.title = Color::rgba8(10, 20, 30);
    config->steps.tokens.item_gap = dp(11);
    parent->update(*config);
    auto local = std::make_unique<ThemeConfig>();
    local->steps.seed.font_size = dp(28);
    const auto child = ThemeScope::create(parent, *local);
    check(child->snapshot().steps() == parent->snapshot().steps(), "inactive Steps seed discarded parent overrides");
    const auto* before = &child->snapshot();
    local->steps.seed.font_size = dp(24);
    child->update(*local);
    check(&child->snapshot() == before, "inactive Steps seed published snapshot");
    local->steps.tokens.description = Color::rgba8(30, 50, 70);
    child->update(*local);
    check(child->snapshot().steps().colors.title == *config->steps.tokens.title &&
              child->snapshot().steps().colors.description == *local->steps.tokens.description &&
              child->snapshot().steps().metrics.item_gap == 11,
          "child Steps override discarded parent");
    local->steps.algorithm = true;
    child->update(*local);
    check(child->snapshot().steps().metrics.font_size == 24 && child->snapshot().button() == base->button(),
          "Steps algorithm did not isolate seed");
    local->steps.algorithm = false;
    local->inherit = false;
    child->update(*local);
    check(child->snapshot().steps().metrics == base->steps().metrics, "Steps inherit=false failed");
    check(parent->snapshot() != *base && parent->snapshot().identity() != base->identity() &&
              parent->snapshot().diagnostic_json().find("\"steps\":") != std::string::npos,
          "Steps identity/serialization absent");
}

void atomic_and_dirty() {
    const auto scope = ThemeScope::create_default();
    int colors = 0;
    int metrics = 0;
    auto c = scope->capture(
        [&](DirtyPhase phase) {
            check(phase == DirtyPhase::paint_material, "Steps color invalidated geometry");
            ++colors;
        },
        [&] { static_cast<void>(scope->steps_colors()); });
    auto m = scope->capture(
        [&](DirtyPhase phase) {
            check(has_any(phase, DirtyPhase::measure_layout | DirtyPhase::text | DirtyPhase::hit_test),
                  "Steps metric phases absent");
            ++metrics;
        },
        [&] { static_cast<void>(scope->steps_metrics()); });
    auto config = std::make_unique<ThemeConfig>();
    config->steps.tokens.description = Color::rgba8(10, 20, 30);
    scope->update(*config);
    check(colors == 1 && metrics == 0 && scope->changed_identities().size() == 1, "Steps color dirty leaked");
    config->steps.tokens.item_gap = dp(9);
    scope->update(*config);
    const auto* before = &scope->snapshot();
    for (int mode = 0; mode < 11; ++mode) {
        auto invalid = std::make_unique<ThemeConfig>(*config);
        if (mode == 0) {
            invalid->steps.tokens.font_size = dp(.49F);
        }
        if (mode == 1) {
            invalid->steps.tokens.icon_font_size = dp(.49F);
        }
        if (mode == 2) {
            invalid->steps.tokens.line_height = dp(0);
        }
        if (mode == 3) {
            invalid->steps.tokens.item_gap = dp(std::numeric_limits<float>::max());
        }
        if (mode == 4) {
            invalid->steps.tokens.description_gap = dp(-1);
        }
        if (mode == 5) {
            invalid->steps.algorithm = true;
            invalid->steps.seed.font_size = dp(.49F);
        }
        if (mode == 6) {
            invalid->steps.algorithm = true;
            invalid->steps.seed.control_height = dp(-1);
            invalid->steps.tokens.font_size = dp(0);
        }
        if (mode == 7) {
            invalid->steps.tokens.item_gap = dp(std::numeric_limits<float>::quiet_NaN());
        }
        if (mode == 8) {
            invalid->steps.tokens.font_size = dp(2147483648.0F);
        }
        if (mode == 9) {
            invalid->steps.tokens.title_font_size_small = dp(.49F);
        }
        if (mode == 10) {
            invalid->steps.algorithm = true;
            invalid->steps.seed.control_height = dp(1);
            invalid->algorithms = {ThemeAlgorithm::Compact};
        }
        bool rejected = false;
        try {
            scope->update(*invalid);
        } catch (const std::invalid_argument&) {
            rejected = true;
        }
        check(rejected && &scope->snapshot() == before && colors == 1 && metrics == 1,
              "invalid final Steps token partially published");
    }
}
} // namespace

int main() {
    try {
        inheritance();
        atomic_and_dirty();
        std::cout << "Steps Theme contracts passed\n";
        return 0;
    } catch (const std::exception& e) {
        std::cerr << e.what() << '\n';
        return 1;
    }
}
