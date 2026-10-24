#include "theme/theme_runtime.hpp"
#include <cmath>
#include <iostream>
#include <limits>
#include <stdexcept>

namespace {
using namespace ryn;
using namespace ryn::theme_runtime;

void check(bool value, const char* message) {
    if (!value) {
        throw std::runtime_error(message);
    }
}

void recipe_and_inheritance() {
    const auto base = resolve_theme();
    const auto& d = base.descriptions();
    check(d.colors.label == Color(0, 0, 0, .45F) && d.colors.label_background == Color(0, 0, 0, .02F) &&
              d.colors.content == base.alias().color_text && d.colors.border == base.alias().color_split &&
              d.metrics.padding_inline == 24 && d.metrics.padding_block == 16 && d.metrics.padding_block_middle == 12 &&
              d.metrics.padding_block_small == 8 && d.metrics.colon_margin_left == 2 &&
              d.metrics.colon_margin_right == 8 && d.metrics.font_size == 14 && d.metrics.title_font_size == 16,
          "Descriptions locked recipe drifted");
    ThemeConfig custom;
    custom.descriptions.tokens.label = Color::rgba8(23, 43, 83);
    custom.descriptions.tokens.padding_inline = dp(30);
    const auto parent = resolve_theme(custom);
    ThemeConfig local;
    local.descriptions.seed.font_size = dp(22);
    local.descriptions.seed.size_unit = dp(-3);
    check(resolve_theme(local, &parent).descriptions() == parent.descriptions(), "inactive component seed was applied");
    check(parent.identity() != base.identity() && parent != base && parent.form() == base.form() &&
              parent.diagnostic_json().find("\"descriptions\":") != std::string::npos,
          "Descriptions identity or isolation missing");
    local.descriptions.seed.size_unit.reset();
    local.descriptions.algorithm = true;
    check(resolve_theme(local).descriptions().metrics.font_size == 22, "component algorithm missing");
    local.descriptions.algorithm = false;
    local.inherit = false;
    check(resolve_theme(local, &parent).descriptions() == d, "inherit=false did not reset Descriptions");
    for (auto algorithm : {ThemeAlgorithm::Dark, ThemeAlgorithm::Compact}) {
        ThemeConfig config;
        config.algorithms = {algorithm};
        const auto variant = resolve_theme(config);
        check(variant.descriptions().colors.content == variant.alias().color_text &&
                  variant.descriptions().metrics.font_size == variant.map().font_size,
              "Descriptions did not follow global algorithm");
    }
}

void dirty_and_validation() {
    const auto scope = ThemeScope::create_default();
    int colors = 0;
    int metrics = 0;
    auto c = scope->capture(
        [&](DirtyPhase phase) {
            check(phase == DirtyPhase::paint_material, "color update changed layout");
            ++colors;
        },
        [&] { static_cast<void>(scope->descriptions_colors()); });
    auto m = scope->capture(
        [&](DirtyPhase phase) {
            check(has_any(phase, DirtyPhase::measure_layout | DirtyPhase::text | DirtyPhase::hit_test),
                  "metric phases missing");
            ++metrics;
        },
        [&] { static_cast<void>(scope->descriptions_metrics()); });
    ThemeConfig config;
    config.descriptions.tokens.label = Color::rgba8(30, 70, 90);
    scope->update(config);
    check(colors == 1 && metrics == 0 && scope->changed_identities().size() == 1 &&
              scope->changed_identities()[0] == TokenIdentity::descriptions_colors,
          "color domain leaked");
    const auto* old = &scope->snapshot();
    config.descriptions.seed.font_size = dp(27);
    scope->update(config);
    check(&scope->snapshot() == old && colors == 1 && metrics == 0, "inactive seed published snapshot");
    config.descriptions.tokens.font_size = dp(18);
    scope->update(config);
    check(colors == 1 && metrics == 1, "metric subscription missing");
    const auto before = std::make_unique<ThemeSnapshot>(scope->snapshot());
    for (float value : {.49F, std::numeric_limits<float>::quiet_NaN(), std::numeric_limits<float>::infinity(), 3e9F}) {
        auto invalid = std::make_unique<ThemeConfig>(config);
        invalid->descriptions.tokens.font_size = dp(value);
        bool rejected{};
        try {
            scope->update(*invalid);
        } catch (const std::invalid_argument&) {
            rejected = true;
        }
        check(rejected && scope->snapshot() == *before && metrics == 1, "invalid final font partially published");
    }
    ThemeConfig invalid;
    invalid.descriptions.algorithm = true;
    invalid.descriptions.seed.font_size = dp(.49F);
    bool rejected{};
    try {
        static_cast<void>(resolve_theme(invalid));
    } catch (const std::invalid_argument&) {
        rejected = true;
    }
    check(rejected, "derived font below logical representation accepted");
    invalid = {};
    invalid.descriptions.tokens.font_size = dp(.5F);
    invalid.descriptions.tokens.title_font_size = dp(.5F);
    check(resolve_theme(invalid).descriptions().metrics.font_size == .5F, "usable font threshold rejected");
    invalid = {};
    invalid.descriptions.tokens.padding_block = LogicalLength{};
    rejected = false;
    try {
        static_cast<void>(resolve_theme(invalid));
    } catch (const std::invalid_argument&) {
        rejected = true;
    }
    check(rejected, "auto token accepted");
}

void descendant_preflight_and_recovery() {
    const auto root = ThemeScope::create_default();
    ThemeConfig child_config;
    child_config.descriptions.tokens.padding_block = dp(5e37F);
    const auto child = ThemeScope::create(root, child_config);
    ThemeConfig grandchild_config;
    grandchild_config.descriptions.seed.font_size = dp(28); // inactive; must not discard child overrides
    const auto grandchild = ThemeScope::create(child, grandchild_config);
    const auto* old_root = &root->snapshot();
    const auto* old_child = &child->snapshot();
    ThemeConfig update;
    update.descriptions.tokens.padding_inline = dp(5e37F);
    bool rejected{};
    try {
        root->update(update);
    } catch (const std::invalid_argument&) {
        rejected = true;
    }
    check(rejected && &root->snapshot() == old_root && &child->snapshot() == old_child &&
              grandchild->snapshot().descriptions() == child->snapshot().descriptions(),
          "descendant failure was not atomic");
    update.descriptions.tokens.padding_inline = dp(32);
    root->update(update);
    check(child->snapshot().descriptions().metrics.padding_inline == 32 &&
              grandchild->snapshot().descriptions() == child->snapshot().descriptions(),
          "preflight recovery failed");
}
} // namespace

int main() {
    try {
        recipe_and_inheritance();
        dirty_and_validation();
        descendant_preflight_and_recovery();
        std::cout << "Descriptions Theme contracts passed\n";
        return 0;
    } catch (const std::exception& e) {
        std::cerr << e.what() << '\n';
        return 1;
    }
}
