#include "theme/theme_runtime.hpp"
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

void recipe() {
    const auto base = std::make_unique<ThemeSnapshot>(resolve_theme());
    const auto& t = base->timeline();
    check(t.metrics.dot_size == 10 && t.metrics.tail_width == 2 && t.metrics.item_padding_bottom == 20 &&
              t.colors.node == base->map().color_primary && t.colors.tail == base->alias().color_split,
          "locked Timeline recipe drifted");
    auto config = std::make_unique<ThemeConfig>();
    config->timeline.tokens.node = Color::rgba8(21, 42, 63);
    config->timeline.tokens.dot_size = dp(12);
    const auto parent = std::make_unique<ThemeSnapshot>(resolve_theme(*config));
    check(*parent != *base && parent->identity() != base->identity() && parent->steps() == base->steps() &&
              parent->diagnostic_json().find("\"timeline\":") != std::string::npos,
          "Timeline identity/isolation missing");
    auto local = std::make_unique<ThemeConfig>();
    local->timeline.seed.font_size = dp(23);
    local->timeline.seed.size_unit = dp(-3);
    check(resolve_theme(*local, parent.get()).timeline() == parent->timeline(),
          "inactive component seed changed timeline");
    local->timeline.seed.size_unit.reset();
    local->timeline.algorithm = true;
    check(resolve_theme(*local).timeline().metrics.font_size == 23, "component algorithm missing");
    local->timeline.algorithm = false;
    local->inherit = false;
    check(resolve_theme(*local, parent.get()).timeline() == t, "inherit=false did not reset Timeline");
    for (auto algorithm : {ThemeAlgorithm::Dark, ThemeAlgorithm::Compact}) {
        config = std::make_unique<ThemeConfig>();
        config->algorithms = {algorithm};
        const auto variant = std::make_unique<ThemeSnapshot>(resolve_theme(*config));
        check(variant->timeline().colors.node == variant->map().color_primary &&
                  variant->timeline().metrics.font_size == variant->map().font_size,
              "global algorithm missing");
    }
}

void dirty_validation() {
    auto scope = ThemeScope::create_default();
    int colors = 0;
    int metrics = 0;
    auto c = scope->capture(
        [&](DirtyPhase p) {
            check(p == DirtyPhase::paint_material, "color dirtied layout");
            ++colors;
        },
        [&] { static_cast<void>(scope->timeline_colors()); });
    auto m = scope->capture(
        [&](DirtyPhase p) {
            check(has_any(p, DirtyPhase::text | DirtyPhase::measure_layout | DirtyPhase::hit_test),
                  "metric phases missing");
            ++metrics;
        },
        [&] { static_cast<void>(scope->timeline_metrics()); });
    auto config = std::make_unique<ThemeConfig>();
    config->timeline.tokens.text = Color::rgba8(45, 67, 89);
    scope->update(*config);
    check(colors == 1 && metrics == 0 && scope->changed_identities().size() == 1 &&
              scope->changed_identities()[0] == TokenIdentity::timeline_colors,
          "color domain leaked");
    const auto* old = &scope->snapshot();
    config->timeline.seed.font_size = dp(26);
    scope->update(*config);
    check(old == &scope->snapshot(), "inactive seed changed snapshot");
    config->timeline.tokens.font_size = dp(18);
    scope->update(*config);
    check(colors == 1 && metrics == 1, "metric subscription missing");
    const auto before = std::make_unique<ThemeSnapshot>(scope->snapshot());
    for (float value : {.49F, std::numeric_limits<float>::quiet_NaN(), std::numeric_limits<float>::infinity(), 3e9F}) {
        auto bad = std::make_unique<ThemeConfig>(*config);
        bad->timeline.tokens.font_size = dp(value);
        bool rejected = false;
        try {
            scope->update(*bad);
        } catch (const std::invalid_argument&) {
            rejected = true;
        }
        check(rejected && scope->snapshot() == *before && metrics == 1, "invalid final font partially published");
    }
    auto bad = std::make_unique<ThemeConfig>();
    bad->timeline.algorithm = true;
    bad->timeline.seed.font_size = dp(.49F);
    bool rejected = false;
    try {
        static_cast<void>(resolve_theme(*bad));
    } catch (const std::invalid_argument&) {
        rejected = true;
    }
    check(rejected, "invalid derived font accepted");
    bad = std::make_unique<ThemeConfig>();
    bad->timeline.tokens.font_size = dp(.5F);
    check(resolve_theme(*bad).timeline().metrics.font_size == .5F, "font threshold rejected");
    bad->timeline.tokens.dot_size = LogicalLength{};
    rejected = false;
    try {
        static_cast<void>(resolve_theme(*bad));
    } catch (const std::invalid_argument&) {
        rejected = true;
    }
    check(rejected, "auto metric accepted");
}

void preflight() {
    auto root = ThemeScope::create_default();
    auto config = std::make_unique<ThemeConfig>();
    config->timeline.tokens.content_gap = dp(5e37F);
    auto child = ThemeScope::create(root, *config);
    auto grandchild = ThemeScope::create(child, ThemeConfig{});
    const auto* r = &root->snapshot();
    const auto* c = &child->snapshot();
    const auto* g = &grandchild->snapshot();
    config = std::make_unique<ThemeConfig>();
    config->timeline.tokens.item_padding_bottom = dp(5e37F);
    bool rejected = false;
    try {
        root->update(*config);
    } catch (const std::invalid_argument&) {
        rejected = true;
    }
    check(rejected && r == &root->snapshot() && c == &child->snapshot() && g == &grandchild->snapshot(),
          "descendant update was not atomic");
    config->timeline.tokens.item_padding_bottom = dp(28);
    root->update(*config);
    check(child->snapshot().timeline().metrics.item_padding_bottom == 28 &&
              grandchild->snapshot().timeline() == child->snapshot().timeline(),
          "descendant recovery failed");
}
} // namespace

int main() {
    try {
        recipe();
        dirty_validation();
        preflight();
        std::cout << "Timeline Theme passed\n";
        return 0;
    } catch (const std::exception& e) {
        std::cerr << e.what() << '\n';
        return 1;
    }
}
