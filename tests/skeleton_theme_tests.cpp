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

void defaults_and_inheritance() {
    const auto root = ThemeScope::create_default();
    const auto base = root->snapshot().skeleton();
    check(base.metrics == SkeletonMetricToken{} && base.colors.background == Color(0, 0, 0, .06F),
          "Skeleton default locked tokens drifted");
    auto config = std::make_unique<ThemeConfig>();
    config->algorithms = {ThemeAlgorithm::Dark, ThemeAlgorithm::Compact};
    root->update(*config);
    check(root->snapshot().skeleton().colors.background == Color(1, 1, 1, .12F) &&
              root->snapshot().skeleton().metrics.title_height == 14 &&
              root->snapshot().skeleton().metrics.paragraph_margin_top == 20,
          "Skeleton dark/compact recipe failed");
    *config = ThemeConfig{};
    config->skeleton.tokens.background = Color::rgba8(10, 20, 30);
    config->skeleton.tokens.avatar_gap = dp(9);
    root->update(*config);
    auto local = std::make_unique<ThemeConfig>();
    local->skeleton.seed.control_height = dp(60);
    const auto child = ThemeScope::create(root, *local);
    check(child->snapshot().skeleton() == root->snapshot().skeleton(), "inactive seed discarded inheritance");
    const auto* before = &child->snapshot();
    local->skeleton.seed.control_height = dp(64);
    child->update(*local);
    check(&child->snapshot() == before, "inactive seed published a snapshot");
    local->skeleton.tokens.title_height = dp(21);
    child->update(*local);
    check(child->snapshot().skeleton().colors == root->snapshot().skeleton().colors &&
              child->snapshot().skeleton().metrics.avatar_gap == 9 &&
              child->snapshot().skeleton().metrics.title_height == 21,
          "Skeleton child token override discarded parent");
    local->skeleton.algorithm = true;
    child->update(*local);
    check(child->snapshot().skeleton().metrics.control_height == 64 &&
              child->snapshot().button() == root->snapshot().button(),
          "Skeleton algorithm seed leaked");
    local->inherit = false;
    local->skeleton.algorithm = false;
    local->skeleton.tokens = {};
    child->update(*local);
    check(child->snapshot().skeleton() == base, "Skeleton inherit=false did not reset");
    check(root->snapshot() != child->snapshot() && root->snapshot().identity() != child->snapshot().identity() &&
              root->snapshot().diagnostic_json().find("\"skeleton\":") != std::string::npos,
          "Skeleton equality/hash/serialization missing");
}

void validation_and_dirty() {
    const auto scope = ThemeScope::create_default();
    int colors{};
    int metrics{};
    auto color = scope->capture(
        [&](DirtyPhase p) {
            check(p == DirtyPhase::paint_material, "Skeleton color dirtied layout");
            ++colors;
        },
        [&] { static_cast<void>(scope->skeleton_colors()); });
    auto metric = scope->capture(
        [&](DirtyPhase p) {
            check(p == (DirtyPhase::measure_layout | DirtyPhase::geometry | DirtyPhase::hit_test),
                  "Skeleton metric dirty contract failed");
            ++metrics;
        },
        [&] { static_cast<void>(scope->skeleton_metrics()); });
    auto config = std::make_unique<ThemeConfig>();
    config->skeleton.tokens.background = Color::rgba8(10, 20, 30);
    scope->update(*config);
    check(colors == 1 && metrics == 0 && scope->changed_identities().size() == 1, "Skeleton color invalidation leaked");
    config->skeleton.tokens.row_gap = dp(7);
    scope->update(*config);
    check(metrics == 1 && !scope->update(*config), "Skeleton equal update invalidated");
    const auto* before = &scope->snapshot();
    for (int mode = 0; mode < 3; ++mode) {
        auto invalid = std::make_unique<ThemeConfig>(*config);
        if (mode == 0) {
            invalid->skeleton.tokens.paragraph_line_height = dp(-1);
        } else if (mode == 1) {
            invalid->skeleton.tokens.row_gap = dp(std::numeric_limits<float>::max());
        } else {
            invalid->skeleton.algorithm = true;
            invalid->skeleton.seed.control_height = dp(1);
            invalid->algorithms = {ThemeAlgorithm::Compact};
        }
        bool rejected = false;
        try {
            scope->update(*invalid);
        } catch (const std::invalid_argument&) {
            rejected = true;
        }
        check(rejected && &scope->snapshot() == before && colors == 1 && metrics == 1,
              "Skeleton invalid final metric partially published");
    }
    config->skeleton.tokens.row_gap = dp(8);
    check(scope->update(*config) && metrics == 2, "Skeleton failed to recover after rejection");
}

void descendant_atomic_preflight() {
    const auto parent = ThemeScope::create_default();
    auto local = std::make_unique<ThemeConfig>();
    local->skeleton.algorithm = true;
    local->skeleton.seed.control_height = dp(1);
    const auto child = ThemeScope::create(parent, *local);
    const auto descendant = ThemeScope::create(child, detail::default_theme_config());
    const auto* p = &parent->snapshot();
    const auto* c = &child->snapshot();
    const auto* d = &descendant->snapshot();
    auto config = std::make_unique<ThemeConfig>();
    config->algorithms = {ThemeAlgorithm::Compact};
    bool rejected = false;
    try {
        parent->update(*config);
    } catch (const std::invalid_argument&) {
        rejected = true;
    }
    check(rejected && p == &parent->snapshot() && c == &child->snapshot() && d == &descendant->snapshot(),
          "Skeleton descendant failure was not atomic");
    local->skeleton.seed.control_height = dp(32);
    child->update(*local);
    check(parent->update(*config) && child->snapshot().skeleton().metrics.control_height == 28 &&
              descendant->snapshot().skeleton() == child->snapshot().skeleton(),
          "Skeleton descendant preflight did not recover");
}
} // namespace

int main() {
    try {
        defaults_and_inheritance();
        validation_and_dirty();
        descendant_atomic_preflight();
        std::cout << "Skeleton Theme contracts passed\n";
    } catch (const std::exception& e) {
        std::cerr << e.what() << '\n';
        return 1;
    }
}
