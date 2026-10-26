#include "theme/theme_runtime.hpp"
#include <cmath>
#include <filesystem>
#include <fstream>
#include <iostream>
#include <limits>

using namespace ryn;
using namespace ryn::theme_runtime;

namespace {
void check(bool ok, const char* message) {
    if (!ok) {
        throw std::runtime_error(message);
    }
}

template <class F> void invalid(F f) {
    bool rejected = false;
    try {
        f();
    } catch (const std::invalid_argument&) {
        rejected = true;
    }
    check(rejected, "invalid AutoComplete theme accepted");
}

void run() {
    auto base = std::make_unique<ThemeSnapshot>(resolve_theme());
    check(base->auto_complete() == base->select(), "locked shared Select recipe drifted");
    auto config = std::make_unique<ThemeConfig>();
    config->auto_complete.tokens.option_color = Color::rgba8(1, 2, 3);
    config->auto_complete.tokens.option_height = dp(36);
    auto parent = std::make_unique<ThemeSnapshot>(resolve_theme(*config));
    check(parent->identity() != base->identity() && *parent != *base && parent->select() == base->select(),
          "snapshot identity/isolation");
    auto local = std::make_unique<ThemeConfig>();
    local->auto_complete.seed.size_unit = dp(-1);
    check(resolve_theme(*local, parent.get()).auto_complete() == parent->auto_complete(),
          "inactive seed must be ignored");
    local->auto_complete.seed.size_unit.reset();
    local->auto_complete.seed.font_size = dp(20);
    local->auto_complete.algorithm = true;
    check(resolve_theme(*local, parent.get()).auto_complete().typography.font_size == 20, "component algorithm");
    local->auto_complete.algorithm = false;
    local->inherit = false;
    check(resolve_theme(*local, parent.get()).auto_complete() == base->auto_complete(), "inherit false");
    auto root = ThemeScope::create_default();
    int colors = 0;
    int metrics = 0;
    auto color_sub = root->capture(
        [&](DirtyPhase p) {
            check(p == DirtyPhase::paint_material, "color dirty phase");
            ++colors;
        },
        [&] { (void)root->auto_complete_colors(); });
    auto metric_sub = root->capture(
        [&](DirtyPhase p) {
            check(has_any(p, DirtyPhase::measure_layout | DirtyPhase::hit_test), "metric dirty phase");
            ++metrics;
        },
        [&] { (void)root->auto_complete_metrics(); });
    auto changed = std::make_unique<ThemeConfig>();
    changed->auto_complete.tokens.option_color = Color::rgba8(1, 2, 3);
    root->update(*changed);
    check(colors == 1 && metrics == 0 && root->changed_identities().size() == 1, "color subscription isolation");
    changed->auto_complete.tokens.option_height = dp(38);
    root->update(*changed);
    check(metrics == 1, "metric subscription");
    const auto* before = &root->snapshot();
    auto inherited = std::make_unique<ThemeConfig>();
    auto child = ThemeScope::create(root, *inherited);
    auto grand = ThemeScope::create(child, *inherited);
    const auto* child_before = &child->snapshot();
    const auto* grand_before = &grand->snapshot();
    constexpr int limit = std::numeric_limits<int>::max() / 64;
    const float above = static_cast<float>(limit);
    const float below = std::nextafter(above, 0.0F);
    for (float font :
         {.49F, above, 100000000.F, std::numeric_limits<float>::quiet_NaN(), std::numeric_limits<float>::infinity()}) {
        auto bad = std::make_unique<ThemeConfig>(*changed);
        bad->auto_complete.tokens.font_size = dp(font);
        invalid([&] { root->update(*bad); });
        check(before == &root->snapshot() && child_before == &child->snapshot() && grand_before == &grand->snapshot(),
              "font preflight not atomic");
    }
    for (float font : {.5F, below}) {
        auto valid = std::make_unique<ThemeConfig>();
        valid->auto_complete.tokens.font_size = dp(font);
        (void)resolve_theme(*valid);
    }
    auto bad = std::make_unique<ThemeConfig>();
    bad->auto_complete.algorithm = true;
    bad->auto_complete.seed.font_size = dp(100000000);
    invalid([&] { (void)resolve_theme(*bad); });
    auto child_config = std::make_unique<ThemeConfig>();
    child_config->auto_complete.tokens.gap = dp(4e34F);
    child->update(*child_config);
    child_before = &child->snapshot();
    grand_before = &grand->snapshot();
    auto ancestor = std::make_unique<ThemeConfig>(*changed);
    ancestor->auto_complete.tokens.option_height = dp(5e34F);
    invalid([&] { root->update(*ancestor); });
    check(before == &root->snapshot() && child_before == &child->snapshot() && grand_before == &grand->snapshot(),
          "descendant preflight not atomic");
    ancestor->auto_complete.tokens.option_height = dp(40);
    root->update(*ancestor);
    check(grand->snapshot().auto_complete().metrics.option_height == 40, "preflight recovery");
}

void goldens(const std::filesystem::path& output) {
    std::filesystem::create_directories(output);
    const std::array<std::pair<const char*, std::vector<ThemeAlgorithm>>, 5> cases{
        {{"default", {}},
         {"dark", {ThemeAlgorithm::Dark}},
         {"compact", {ThemeAlgorithm::Compact}},
         {"dark-compact", {ThemeAlgorithm::Dark, ThemeAlgorithm::Compact}},
         {"compact-dark", {ThemeAlgorithm::Compact, ThemeAlgorithm::Dark}}}};
    for (const auto& [name, algorithms] : cases) {
        auto config = std::make_unique<ThemeConfig>();
        config->algorithms = algorithms;
        auto snapshot = std::make_unique<ThemeSnapshot>(resolve_theme(*config));
        check(snapshot->auto_complete() == snapshot->select(), "global recipe drifted");
        std::ofstream file(output / (std::string(name) + ".json"), std::ios::binary);
        file << snapshot->diagnostic_json();
        check(file.good(), "golden write failed");
    }
}
} // namespace

int main(int argc, char** argv) {
    try {
        run();
        if (argc == 2) {
            goldens(argv[1]);
        }
        std::cout << "AutoComplete theme passed\n";
        return 0;
    } catch (const std::exception& e) {
        std::cerr << e.what() << '\n';
        return 1;
    }
}
