#include "theme/theme_runtime.hpp"
#include <cmath>
#include <filesystem>
#include <fstream>
#include <iostream>
#include <limits>

namespace {
using namespace ryn;
using namespace ryn::theme_runtime;

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
    check(rejected, "invalid Avatar Theme accepted");
}

void recipe() {
    const auto base = std::make_unique<ThemeSnapshot>(resolve_theme());
    const auto& t = base->avatar();
    check(t.metrics.container_size == 32 && t.metrics.container_size_large == 40 &&
              t.metrics.container_size_small == 24 && t.metrics.text_font_size == 14 &&
              t.metrics.icon_font_size == 18 && t.metrics.icon_font_size_large == 24 &&
              t.metrics.group_overlapping == -8 && t.metrics.group_space == 4 &&
              t.colors.background == Color(0, 0, 0, .25F),
          "locked Avatar recipe drifted");
    auto config = std::make_unique<ThemeConfig>();
    config->avatar.tokens.background = Color::rgba8(12, 34, 56);
    config->avatar.tokens.container_size = dp(36);
    const auto parent = std::make_unique<ThemeSnapshot>(resolve_theme(*config));
    check(*parent != *base && parent->identity() != base->identity() && parent->timeline() == base->timeline() &&
              parent->diagnostic_json().find("\"avatar\":") != std::string::npos,
          "Avatar snapshot identity/isolation failed");
    auto local = std::make_unique<ThemeConfig>();
    local->avatar.seed.font_size = dp(20);
    local->avatar.seed.size_unit = dp(-1);
    check(resolve_theme(*local, parent.get()).avatar() == parent->avatar(),
          "inactive Avatar seed affected inheritance");
    local->avatar.seed.size_unit.reset();
    local->avatar.algorithm = true;
    check(resolve_theme(*local).avatar().metrics.text_font_size == 20, "Avatar independent algorithm missing");
    local->inherit = false;
    local->avatar.algorithm = false;
    check(resolve_theme(*local, parent.get()).avatar() == t, "Avatar inherit=false did not reset");
    for (auto algorithm : {ThemeAlgorithm::Dark, ThemeAlgorithm::Compact}) {
        config = std::make_unique<ThemeConfig>();
        config->algorithms = {algorithm};
        const auto v = std::make_unique<ThemeSnapshot>(resolve_theme(*config));
        check(v->avatar().metrics.container_size == v->map().control_height &&
                  v->avatar().metrics.text_font_size == v->map().font_size &&
                  v->avatar().colors.group_border == v->alias().color_background_container,
              "Avatar global algorithm missing");
    }
}

void validation() {
    auto root = ThemeScope::create_default();
    int colors = 0;
    int metrics = 0;
    auto a = root->capture(
        [&](DirtyPhase p) {
            check(p == DirtyPhase::paint_material, "Avatar color dirtied geometry");
            ++colors;
        },
        [&] { static_cast<void>(root->avatar_colors()); });
    auto b = root->capture(
        [&](DirtyPhase p) {
            check(has_any(p, DirtyPhase::text | DirtyPhase::hit_test | DirtyPhase::measure_layout),
                  "Avatar metric dirty phases missing");
            ++metrics;
        },
        [&] { static_cast<void>(root->avatar_metrics()); });
    auto config = std::make_unique<ThemeConfig>();
    config->avatar.tokens.text = Color::rgba8(11, 22, 33);
    root->update(*config);
    check(colors == 1 && metrics == 0 && root->changed_identities().size() == 1 &&
              root->changed_identities()[0] == TokenIdentity::avatar_colors,
          "Avatar color identity leaked");
    config->avatar.tokens.text_font_size = dp(17);
    root->update(*config);
    check(colors == 1 && metrics == 1, "Avatar metrics subscription missing");
    const auto* old = &root->snapshot();
    for (float value : {.49F, std::numeric_limits<float>::quiet_NaN(), std::numeric_limits<float>::infinity(), 3e9F}) {
        auto bad = std::make_unique<ThemeConfig>(*config);
        bad->avatar.tokens.text_font_size = dp(value);
        invalid([&] { root->update(*bad); });
        check(old == &root->snapshot(), "invalid update published Avatar Theme");
    }
    auto bad = std::make_unique<ThemeConfig>();
    bad->avatar.algorithm = true;
    bad->avatar.seed.font_size = dp(.49F);
    invalid([&] { static_cast<void>(resolve_theme(*bad)); });
    bad = std::make_unique<ThemeConfig>();
    bad->avatar.tokens.text_font_size = dp(.5F);
    check(resolve_theme(*bad).avatar().metrics.text_font_size == .5F, "minimum renderable Avatar font rejected");
    bad->avatar.tokens.container_size = LogicalLength{};
    invalid([&] { static_cast<void>(resolve_theme(*bad)); });
    bad = std::make_unique<ThemeConfig>();
    bad->avatar.tokens.container_size = dp(-8);
    bad->avatar.tokens.group_overlapping = dp(-8);
    invalid([&] { static_cast<void>(resolve_theme(*bad)); });
    auto child_config = std::make_unique<ThemeConfig>();
    child_config->avatar.tokens.group_space = dp(5e34F);
    auto child = ThemeScope::create(root, *child_config);
    auto grand = ThemeScope::create(child, ThemeConfig{});
    const auto* c = &child->snapshot();
    const auto* g = &grand->snapshot();
    auto ancestor = std::make_unique<ThemeConfig>(*config);
    ancestor->avatar.tokens.container_size = dp(1.2e35F);
    invalid([&] { root->update(*ancestor); });
    check(old == &root->snapshot() && c == &child->snapshot() && g == &grand->snapshot(),
          "Avatar descendant preflight was not atomic");
    ancestor->avatar.tokens.container_size = dp(38);
    root->update(*ancestor);
    check(grand->snapshot().avatar().metrics.container_size == 38, "Avatar failed to recover after rejected update");
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
        const auto snapshot = std::make_unique<ThemeSnapshot>(resolve_theme(*config));
        std::ofstream file(output / (std::string(name) + ".json"), std::ios::binary);
        file << snapshot->diagnostic_json();
        check(file.good(), "could not write fresh Avatar golden");
    }
}

void font_limits() {
    auto root = ThemeScope::create_default();
    auto inherited = std::make_unique<ThemeConfig>();
    auto child = ThemeScope::create(root, *inherited);
    auto grandchild = ThemeScope::create(child, *inherited);
    const auto* before = &root->snapshot();
    const auto* child_before = &child->snapshot();
    const auto* grandchild_before = &grandchild->snapshot();
    auto config = std::make_unique<ThemeConfig>();
    config->avatar.tokens.text_font_size = dp(100000000);
    bool rejected = false;
    try {
        root->update(*config);
    } catch (const std::invalid_argument&) {
        rejected = true;
    }
    std::cout << "R1 font: rejected=" << rejected << ", snapshot preserved=" << (before == &root->snapshot()) << '\n';
    check(rejected && before == &root->snapshot() && child_before == &child->snapshot() &&
              grandchild_before == &grandchild->snapshot(),
          "R1 font: 100000000 dp was published before font loading");
    constexpr auto limit = std::numeric_limits<int>::max() / 64;
    const float above = static_cast<float>(limit);
    const float below = std::nextafter(above, 0.0F);
    check(std::round(static_cast<double>(below)) <= limit && std::round(static_cast<double>(above)) > limit,
          "font limit fixture does not straddle the quantized FontRuntime limit");
    for (auto field : {&AvatarTokenOverride::text_font_size, &AvatarTokenOverride::text_font_size_large,
                       &AvatarTokenOverride::text_font_size_small, &AvatarTokenOverride::icon_font_size,
                       &AvatarTokenOverride::icon_font_size_large, &AvatarTokenOverride::icon_font_size_small}) {
        auto boundary = std::make_unique<ThemeConfig>();
        boundary->avatar.tokens.*field = dp(above);
        invalid([&] { root->update(*boundary); });
        check(before == &root->snapshot(), "quantized oversized Avatar font changed snapshot");
        boundary->avatar.tokens.*field = dp(below);
        static_cast<void>(resolve_theme(*boundary));
    }
    auto algorithm = std::make_unique<ThemeConfig>();
    algorithm->avatar.algorithm = true;
    algorithm->avatar.seed.font_size = dp(100000000);
    invalid([&] { static_cast<void>(resolve_theme(*algorithm)); });
    invalid([&] { static_cast<void>(ThemeScope::create(root, *algorithm)); });
    algorithm->avatar.seed.font_size = dp(20);
    child->update(*algorithm);
    check(child->snapshot().avatar().metrics.text_font_size == 20 &&
              grandchild->snapshot().avatar() == child->snapshot().avatar(),
          "derived font recovery failed");
    config->avatar.tokens.text_font_size = dp(18);
    root->update(*config);
    check(root->snapshot().avatar().metrics.text_font_size == 18 &&
              child->snapshot().avatar().metrics.text_font_size == 20,
          "valid font recovery or independent component algorithm failed");
}
} // namespace

int main(int argc, char** argv) {
    try {
        recipe();
        validation();
        font_limits();
        if (argc == 2) {
            goldens(argv[1]);
        }
        std::cout << "Avatar Theme passed\n";
        return 0;
    } catch (const std::exception& e) {
        std::cerr << e.what() << '\n';
        return 1;
    }
}
