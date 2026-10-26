#include "theme/theme_runtime.hpp"
#include <iostream>
#include <limits>

namespace {
using namespace ryn;
using namespace ryn::theme_runtime;

void check(bool v, const char* message) {
    if (!v) {
        throw std::runtime_error(message);
    }
}

void recipe() {
    const auto base = std::make_unique<ThemeSnapshot>(resolve_theme());
    const auto& t = base->rate();
    check(t.metrics.star_size == 20 && t.metrics.star_size_small == 15 && t.metrics.star_size_large == 25 &&
              t.metrics.gap == 8 && t.colors.star == Color::rgba8(250, 219, 20) &&
              t.colors.background == Color(0, 0, 0, .06F),
          "locked Rate recipe drifted");
    auto cfg = std::make_unique<ThemeConfig>();
    cfg->rate.tokens.star = Color::rgba8(1, 2, 3);
    auto parent = std::make_unique<ThemeSnapshot>(resolve_theme(*cfg));
    check(*base != *parent && base->identity() != parent->identity() && base->timeline() == parent->timeline() &&
              parent->diagnostic_json().find("\"rate\":") != std::string::npos,
          "Rate identity/isolation failed");
    auto local = std::make_unique<ThemeConfig>();
    local->rate.seed.control_height = dp(-1);
    check(resolve_theme(*local, parent.get()).rate() == parent->rate(), "inactive seed applied");
    local->rate.seed.control_height = dp(48);
    local->rate.algorithm = true;
    check(resolve_theme(*local, parent.get()).rate().metrics.star_size == 30, "independent seed ignored");
    local = std::make_unique<ThemeConfig>();
    local->inherit = false;
    check(resolve_theme(*local, parent.get()).rate() == t, "inherit=false failed");
    cfg = std::make_unique<ThemeConfig>();
    cfg->algorithms = {ThemeAlgorithm::Dark, ThemeAlgorithm::Compact};
    auto dark = std::make_unique<ThemeSnapshot>(resolve_theme(*cfg));
    check(dark->rate().metrics.star_size == dark->map().control_height * .625F &&
              dark->rate().colors.background == Color(1, 1, 1, .12F) &&
              dark->rate().colors.star == Color::rgba8(216, 189, 20),
          "Rate algorithm chain failed");
}

void validation() {
    auto root = ThemeScope::create_default();
    int colors = 0;
    int metrics = 0;
    auto c = root->capture(
        [&](DirtyPhase p) {
            check(p == DirtyPhase::paint_material, "color dirtied layout");
            ++colors;
        },
        [&] { static_cast<void>(root->rate_colors()); });
    auto m = root->capture(
        [&](DirtyPhase p) {
            check(has_any(p, DirtyPhase::text | DirtyPhase::hit_test), "metric phases missing");
            ++metrics;
        },
        [&] { static_cast<void>(root->rate_metrics()); });
    auto cfg = std::make_unique<ThemeConfig>();
    cfg->rate.tokens.star = Color::rgba8(1, 2, 3);
    root->update(*cfg);
    check(colors == 1 && metrics == 0 && root->changed_identities().size() == 1 &&
              root->changed_identities()[0] == TokenIdentity::rate_colors,
          "Rate subscription leaked");
    cfg->rate.tokens.star_size = dp(18);
    root->update(*cfg);
    check(metrics == 1 && colors == 1, "Rate metric subscription missing");
    const auto* before = &root->snapshot();
    for (float value : {.49F, std::numeric_limits<float>::quiet_NaN(), std::numeric_limits<float>::infinity(), 3e9F}) {
        auto bad = std::make_unique<ThemeConfig>(*cfg);
        bad->rate.tokens.star_size = dp(value);
        bool rejected = false;
        try {
            root->update(*bad);
        } catch (const std::invalid_argument&) {
            rejected = true;
        }
        check(rejected && &root->snapshot() == before && metrics == 1, "invalid final font partially published");
    }
    auto bad = std::make_unique<ThemeConfig>();
    bad->rate.algorithm = true;
    bad->rate.seed.control_height = dp(.6F);
    bool rejected = false;
    try {
        static_cast<void>(resolve_theme(*bad));
    } catch (const std::invalid_argument&) {
        rejected = true;
    }
    check(rejected, "invalid derived font accepted");
    cfg->rate.tokens.star_size = dp(.5F);
    check(resolve_theme(*cfg).rate().metrics.star_size == .5F, "font .5 rejected");
    // Individually valid ancestor/descendant metrics produce an invalid final product.
    auto child_cfg = std::make_unique<ThemeConfig>();
    child_cfg->rate.tokens.hover_scale = 1e5F;
    auto child = ThemeScope::create(root, *child_cfg);
    auto grand = ThemeScope::create(child, *std::make_unique<ThemeConfig>());
    const auto* r = &root->snapshot();
    const auto* ch = &child->snapshot();
    const auto* g = &grand->snapshot();
    cfg = std::make_unique<ThemeConfig>();
    cfg->rate.tokens.star_size = dp(30000);
    rejected = false;
    try {
        root->update(*cfg);
    } catch (const std::invalid_argument&) {
        rejected = true;
    }
    check(rejected && r == &root->snapshot() && ch == &child->snapshot() && g == &grand->snapshot(),
          "descendant preflight not atomic");
    cfg->rate.tokens.star_size = dp(22);
    root->update(*cfg);
    check(child->snapshot().rate().metrics.star_size == 22 && grand->snapshot().rate() == child->snapshot().rate(),
          "ancestor recovery failed");
}
} // namespace

int main() {
    try {
        recipe();
        validation();
        std::cout << "Rate Theme passed\n";
        return 0;
    } catch (const std::exception& e) {
        std::cerr << e.what() << '\n';
        return 1;
    }
}
