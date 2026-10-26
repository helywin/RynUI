#include "theme/theme_runtime.hpp"
#include <cmath>
#include <filesystem>
#include <fstream>
#include <iostream>
#include <limits>

namespace {
using namespace ryn;
using namespace ryn::theme_runtime;

void check(bool ok, const char* text) {
    if (!ok) {
        throw std::runtime_error(text);
    }
}

template <class F> void invalid(F f) {
    bool rejected = false;
    try {
        f();
    } catch (const std::invalid_argument&) {
        rejected = true;
    }
    check(rejected, "invalid Tree Theme accepted");
}

void run() {
    static_assert(static_cast<int>(TokenIdentity::tree_colors) == 198);
    static_assert(static_cast<int>(TokenIdentity::tree_metrics) == 199);
    auto base = std::make_unique<ThemeSnapshot>(resolve_theme());
    check(base->tree().metrics.title_height == 24 && base->tree().metrics.indent_size == 24 &&
              base->tree().metrics.switcher_size == 24 && base->tree().metrics.font_size == 14 &&
              base->tree().colors.node_selected_background == base->select().colors.option_selected_background,
          "locked Tree recipe drifted");
    auto config = std::make_unique<ThemeConfig>();
    config->tree.tokens.text = Color::rgba8(11, 22, 33);
    auto parent = std::make_unique<ThemeSnapshot>(resolve_theme(*config));
    check(*parent != *base && parent->identity() != base->identity() && parent->avatar() == base->avatar() &&
              parent->diagnostic_json().find("\"tree\":") != std::string::npos,
          "Tree snapshot contract failed");
    auto local = std::make_unique<ThemeConfig>();
    local->tree.seed.font_size = dp(20);
    local->tree.seed.size_unit = dp(-1);
    check(resolve_theme(*local, parent.get()).tree() == parent->tree(), "inactive Tree seed changed inheritance");
    local->tree.seed.size_unit.reset();
    local->tree.algorithm = true;
    check(resolve_theme(*local).tree().metrics.font_size == 20, "component Tree algorithm ignored");
    local->tree.algorithm = false;
    local->inherit = false;
    check(resolve_theme(*local, parent.get()).tree() == base->tree(), "inherit=false missed Tree reset");
    auto root = ThemeScope::create_default();
    auto child = ThemeScope::create(root, *std::make_unique<ThemeConfig>());
    auto grand = ThemeScope::create(child, *std::make_unique<ThemeConfig>());
    int colors = 0;
    int metrics = 0;
    auto a = root->capture(
        [&](DirtyPhase p) {
            check(p == DirtyPhase::paint_material, "Tree color dirtied layout");
            ++colors;
        },
        [&] { static_cast<void>(root->tree_colors()); });
    auto b = root->capture(
        [&](DirtyPhase p) {
            check(has_any(p, DirtyPhase::text | DirtyPhase::measure_layout | DirtyPhase::hit_test),
                  "Tree metric dirty phases missing");
            ++metrics;
        },
        [&] { static_cast<void>(root->tree_metrics()); });
    root->update(*config);
    check(colors == 1 && metrics == 0 && root->changed_identities().size() == 1, "Tree identity isolation failed");
    config->tree.tokens.font_size = dp(18);
    root->update(*config);
    check(metrics == 1, "Tree metric subscription missing");
    const auto* old = &root->snapshot();
    const auto* c = &child->snapshot();
    const auto* g = &grand->snapshot();
    for (float font :
         {.49F, 100000000.0F, std::numeric_limits<float>::infinity(), std::numeric_limits<float>::quiet_NaN()}) {
        auto bad = std::make_unique<ThemeConfig>(*config);
        bad->tree.tokens.font_size = dp(font);
        invalid([&] { root->update(*bad); });
        check(old == &root->snapshot() && c == &child->snapshot() && g == &grand->snapshot(),
              "Tree inherited update was not atomic");
    }
    auto bad = std::make_unique<ThemeConfig>();
    bad->tree.tokens.icon_size = dp(100000000);
    invalid([&] { static_cast<void>(resolve_theme(*bad)); });
    bad->tree.tokens.icon_size = dp(.5F);
    bad->tree.tokens.font_size = dp(.5F);
    check(resolve_theme(*bad).tree().metrics.font_size == .5F, "Tree minimum renderable font rejected");
    bad->tree.tokens.font_size = dp(static_cast<float>(std::numeric_limits<int>::max() / 64) - 2);
    static_cast<void>(resolve_theme(*bad));
    bad->tree.tokens.font_size = dp(static_cast<float>(std::numeric_limits<int>::max() / 64));
    invalid([&] { static_cast<void>(resolve_theme(*bad)); });
    bad = std::make_unique<ThemeConfig>();
    bad->tree.tokens.indent_size = LogicalLength{};
    invalid([&] { static_cast<void>(resolve_theme(*bad)); });
    bad = std::make_unique<ThemeConfig>();
    bad->tree.algorithm = true;
    bad->tree.seed.font_size = dp(.49F);
    invalid([&] { static_cast<void>(resolve_theme(*bad)); });
    config->tree.tokens.font_size = dp(16);
    root->update(*config);
    check(grand->snapshot().tree().metrics.font_size == 16, "Tree inherited recovery failed");
    auto child_config = std::make_unique<ThemeConfig>();
    child_config->tree.tokens.indent_size = dp(2e33F);
    auto limited_child = ThemeScope::create(root, *child_config);
    auto limited_grand = ThemeScope::create(limited_child, *std::make_unique<ThemeConfig>());
    const auto* root_before = &root->snapshot();
    const auto* child_before = &limited_child->snapshot();
    const auto* grand_before = &limited_grand->snapshot();
    auto ancestor = std::make_unique<ThemeConfig>(*config);
    ancestor->tree.tokens.title_height = dp(4e33F);
    static_cast<void>(resolve_theme(*ancestor)); // Valid alone; only the descendant combination exceeds the limit.
    invalid([&] { root->update(*ancestor); });
    check(root_before == &root->snapshot() && child_before == &limited_child->snapshot() &&
              grand_before == &limited_grand->snapshot(),
          "Tree descendant preflight published part of the chain");
    ancestor->tree.tokens.title_height = dp(32);
    root->update(*ancestor);
    check(limited_grand->snapshot().tree().metrics.title_height == 32, "Tree descendant preflight failed to recover");
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
        std::ofstream file(output / (std::string(name) + ".json"), std::ios::binary);
        file << snapshot->diagnostic_json();
        check(file.good(), "fresh Tree golden write failed");
    }
}
} // namespace

int main(int argc, char** argv) {
    try {
        run();
        if (argc == 2) {
            goldens(argv[1]);
        }
        std::cout << "Tree Theme passed: recipe, override, algorithm, identities, inheritance, font limits, recovery\n";
        return 0;
    } catch (const std::exception& e) {
        std::cerr << e.what() << '\n';
        return 1;
    }
}
