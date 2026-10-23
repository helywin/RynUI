#include "theme/theme_runtime.hpp"
#include <iostream>
#include <limits>
#include <stdexcept>

namespace {
using namespace ryn;
using namespace ryn::theme_runtime;

void check(bool condition, const char* message) {
    if (!condition) {
        throw std::runtime_error(message);
    }
}

void defaults_inheritance_algorithms() {
    const auto base = resolve_theme();
    check(base.empty().metrics.image_size == 100 && base.empty().metrics.compact_image_size == 40 &&
              base.empty().metrics.image_gap == 8 && base.empty().metrics.extra_gap == 16 &&
              base.empty().metrics.font_size == 14 && base.empty().metrics.line_height == 22,
          "Empty locked baseline drifted");
    check(base.result().metrics.icon_size == 72 && base.result().metrics.padding_inline == 32 &&
              base.result().metrics.padding_block == 48 && base.result().metrics.title_font_size == 24 &&
              base.result().metrics.title_line_height == 32 && base.result().metrics.extra_gap == 24 &&
              base.result().metrics.content_padding_inline == 40 && base.result().metrics.content_padding_block == 24 &&
              base.result().colors.success == base.map().color_success &&
              base.result().colors.info == base.map().color_info,
          "Result locked baseline drifted");
    ThemeConfig config;
    config.empty.tokens.description = Color::rgba8(9, 20, 90);
    config.empty.tokens.image_size = dp(66);
    config.result.tokens.extra_gap = dp(30);
    config.result.tokens.error = Color::rgba8(200, 20, 10);
    const auto custom = resolve_theme(config);
    const auto inherited = resolve_theme({}, &custom);
    check(inherited.empty() == custom.empty() && inherited.result() == custom.result(), "presentation did not inherit");
    ThemeConfig child;
    child.empty.tokens.extra_gap = dp(20);
    const auto partial = resolve_theme(child, &custom);
    check(partial.empty().metrics.image_size == 66 && partial.empty().metrics.extra_gap == 20 &&
              partial.result() == custom.result(),
          "partial child override lost parent values");
    child.inherit = false;
    check(resolve_theme(child, &custom).result() == base.result(), "inherit=false retained custom Result");
    check(custom.button() == base.button() && custom.card() == base.card() && custom.table() == base.table(),
          "presentation override changed unrelated component");
    config = {};
    config.empty.algorithm = true;
    config.empty.seed.font_size = dp(20);
    config.result.algorithm = true;
    config.result.seed.color_success = Color::rgba8(20, 40, 60);
    check(resolve_theme(config).empty().metrics.font_size == 20 &&
              resolve_theme(config).result().colors.success == Color::rgba8(20, 40, 60) &&
              resolve_theme(config).button() == base.button(),
          "component algorithm leaked or was ignored");
    config.empty.algorithm = false;
    config.result.algorithm = false;
    check(resolve_theme(config).empty() == base.empty() && resolve_theme(config).result() == base.result(),
          "inactive local seeds applied");
    config = {};
    config.algorithms = {ThemeAlgorithm::Dark};
    const auto dark = resolve_theme(config);
    check(dark.empty().colors.description != base.empty().colors.description &&
              dark.result().colors.content_background != base.result().colors.content_background,
          "dark presentation missing");
    config.algorithms = {ThemeAlgorithm::Compact};
    const auto compact = resolve_theme(config);
    check(compact.empty().metrics.image_size < base.empty().metrics.image_size &&
              compact.result().metrics.icon_size < base.result().metrics.icon_size &&
              compact.result().metrics.padding_block < base.result().metrics.padding_block,
          "compact presentation missing");
    check(custom.identity() != base.identity() && custom != base &&
              custom.diagnostic_json().find("\"empty\":") != std::string::npos &&
              custom.diagnostic_json().find("\"result\":") != std::string::npos,
          "missing identity/equality/diagnostics");
}

void dirty_groups_and_atomic_validation() {
    auto parent = ThemeScope::create_default();
    auto child = ThemeScope::create(parent, {});
    int ec = 0;
    int em = 0;
    int rc = 0;
    int rm = 0;
    auto a = child->capture(
        [&](DirtyPhase phase) {
            check(phase == DirtyPhase::paint_material, "Empty colors triggered layout");
            ++ec;
        },
        [&] { (void)child->empty_colors(); });
    auto b = child->capture(
        [&](DirtyPhase phase) {
            check(has_any(phase, DirtyPhase::measure_layout) && has_any(phase, DirtyPhase::text),
                  "Empty metrics missed text/layout");
            ++em;
        },
        [&] { (void)child->empty_metrics(); });
    auto c = child->capture(
        [&](DirtyPhase phase) {
            check(phase == DirtyPhase::paint_material, "Result colors triggered layout");
            ++rc;
        },
        [&] { (void)child->result_colors(); });
    auto d = child->capture(
        [&](DirtyPhase phase) {
            check(has_any(phase, DirtyPhase::hit_test), "Result metrics missed hit-test");
            ++rm;
        },
        [&] { (void)child->result_metrics(); });
    ThemeConfig config;
    config.empty.tokens.image = Color::rgba8(30, 40, 50);
    check(parent->update(config) && ec == 1 && em == 0 && rc == 0 && rm == 0, "Empty color dirty domains leaked");
    config.result.tokens.subtitle = Color::rgba8(50, 60, 70);
    parent->update(config);
    config.empty.tokens.font_size = dp(18);
    parent->update(config);
    config.result.tokens.action_gap = dp(14);
    parent->update(config);
    check(ec == 1 && em == 1 && rc == 1 && rm == 1, "presentation dirty groups or child propagation drifted");
    const auto before = parent->snapshot();
    check(!parent->update(config), "equal config republished snapshot");
    for (int mode = 0; mode < 5; ++mode) {
        auto invalid = config;
        if (mode == 0) {
            invalid.empty.tokens.image_size = dp(-1);
        }
        if (mode == 1) {
            invalid.result.tokens.title_font_size = dp(0);
        }
        if (mode == 2) {
            invalid.result.tokens.extra_gap = dp(std::numeric_limits<float>::infinity());
        }
        if (mode == 3) {
            invalid.empty.tokens.image_size = dp(std::numeric_limits<float>::max());
        }
        if (mode == 4) {
            invalid.result.tokens.icon_size = dp(std::numeric_limits<float>::quiet_NaN());
        }
        bool rejected = false;
        try {
            parent->update(invalid);
        } catch (const std::invalid_argument&) {
            rejected = true;
        }
        check(rejected && parent->snapshot() == before && child->snapshot().empty() == before.empty(),
              "invalid presentation partially published");
    }
    check(token_identity_name(TokenIdentity::empty_colors) == "Empty.colors" &&
              token_identity_name(TokenIdentity::result_metrics) == "Result.metrics",
          "token identity names drifted");
}
} // namespace

int main() {
    try {
        defaults_inheritance_algorithms();
        dirty_groups_and_atomic_validation();
    } catch (const std::exception& error) {
        std::cerr << error.what() << '\n';
        return 1;
    }
    return 0;
}
