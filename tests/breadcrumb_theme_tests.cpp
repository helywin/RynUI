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
    config->breadcrumb.tokens.current = Color::rgba8(10, 20, 30);
    config->breadcrumb.tokens.separator_margin = dp(11);
    parent->update(*config);
    auto local = std::make_unique<ThemeConfig>();
    local->breadcrumb.seed.font_size = dp(28);
    const auto child = ThemeScope::create(parent, *local);
    check(child->snapshot().breadcrumb() == parent->snapshot().breadcrumb(),
          "inactive Breadcrumb seed discarded parent overrides");
    const auto* before = &child->snapshot();
    local->breadcrumb.seed.font_size = dp(24);
    child->update(*local);
    check(&child->snapshot() == before, "inactive Breadcrumb seed published snapshot");
    local->breadcrumb.tokens.link = Color::rgba8(30, 50, 70);
    child->update(*local);
    check(child->snapshot().breadcrumb().colors.current == *config->breadcrumb.tokens.current &&
              child->snapshot().breadcrumb().colors.link == *local->breadcrumb.tokens.link &&
              child->snapshot().breadcrumb().metrics.separator_margin == 11,
          "child Breadcrumb override discarded parent");
    local->breadcrumb.algorithm = true;
    child->update(*local);
    check(child->snapshot().breadcrumb().metrics.font_size == 24 && child->snapshot().button() == base->button(),
          "Breadcrumb algorithm did not isolate seed");
    local->breadcrumb.algorithm = false;
    local->inherit = false;
    child->update(*local);
    check(child->snapshot().breadcrumb().metrics == base->breadcrumb().metrics, "Breadcrumb inherit=false failed");
    check(parent->snapshot() != *base && parent->snapshot().identity() != base->identity() &&
              parent->snapshot().diagnostic_json().find("\"breadcrumb\":") != std::string::npos,
          "Breadcrumb identity/serialization absent");
}

void atomic_and_dirty() {
    const auto scope = ThemeScope::create_default();
    int colors = 0;
    int metrics = 0;
    auto c = scope->capture(
        [&](DirtyPhase phase) {
            check(phase == DirtyPhase::paint_material, "Breadcrumb color invalidated geometry");
            ++colors;
        },
        [&] { static_cast<void>(scope->breadcrumb_colors()); });
    auto m = scope->capture(
        [&](DirtyPhase phase) {
            check(has_any(phase, DirtyPhase::measure_layout | DirtyPhase::text | DirtyPhase::hit_test),
                  "Breadcrumb metric phases absent");
            ++metrics;
        },
        [&] { static_cast<void>(scope->breadcrumb_metrics()); });
    auto config = std::make_unique<ThemeConfig>();
    config->breadcrumb.tokens.link = Color::rgba8(10, 20, 30);
    scope->update(*config);
    check(colors == 1 && metrics == 0 && scope->changed_identities().size() == 1, "Breadcrumb color dirty leaked");
    config->breadcrumb.tokens.separator_margin = dp(9);
    scope->update(*config);
    const auto* before = &scope->snapshot();
    for (int mode = 0; mode < 9; ++mode) {
        auto invalid = std::make_unique<ThemeConfig>(*config);
        if (mode == 0) {
            invalid->breadcrumb.tokens.font_size = dp(.49F);
        }
        if (mode == 1) {
            invalid->breadcrumb.tokens.icon_size = dp(.49F);
        }
        if (mode == 2) {
            invalid->breadcrumb.tokens.line_height = dp(0);
        }
        if (mode == 3) {
            invalid->breadcrumb.tokens.padding = dp(std::numeric_limits<float>::max());
        }
        if (mode == 4) {
            invalid->breadcrumb.tokens.radius = dp(-1);
        }
        if (mode == 5) {
            invalid->breadcrumb.algorithm = true;
            invalid->breadcrumb.seed.font_size = dp(.49F);
        }
        if (mode == 6) {
            invalid->breadcrumb.algorithm = true;
            invalid->breadcrumb.seed.control_height = dp(-1);
            invalid->breadcrumb.tokens.font_size = dp(0);
        }
        if (mode == 7) {
            invalid->breadcrumb.tokens.separator_margin = dp(std::numeric_limits<float>::quiet_NaN());
        }
        if (mode == 8) {
            invalid->breadcrumb.tokens.font_size = dp(2147483648.0F);
        }
        bool rejected = false;
        try {
            scope->update(*invalid);
        } catch (const std::invalid_argument&) {
            rejected = true;
        }
        check(rejected && &scope->snapshot() == before && colors == 1 && metrics == 1,
              "invalid final Breadcrumb token partially published");
    }
}
} // namespace

int main() {
    try {
        inheritance();
        atomic_and_dirty();
        std::cout << "Breadcrumb Theme contracts passed\n";
        return 0;
    } catch (const std::exception& e) {
        std::cerr << e.what() << '\n';
        return 1;
    }
}
