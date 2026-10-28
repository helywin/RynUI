#include "theme/theme_runtime.hpp"
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

template <class F> void rejects(F f) {
    bool failed = false;
    try {
        f();
    } catch (const std::invalid_argument&) {
        failed = true;
    }
    check(failed, "invalid TreeSelect Theme accepted");
}

void run() {
    static_assert(static_cast<int>(TokenIdentity::tree_select_colors) == 209);
    auto base = std::make_unique<ThemeSnapshot>(resolve_theme());
    check(base->tree_select().tree == base->tree() &&
              base->tree_select().selector.popup_background == base->select().colors.popup_background,
          "TreeSelect locked recipe drifted");
    auto config = std::make_unique<ThemeConfig>();
    config->tree_select.tokens.selector.popup_background = Color::rgba8(1, 2, 3);
    config->tree_select.tokens.tree.text = Color::rgba8(4, 5, 6);
    auto parent = std::make_unique<ThemeSnapshot>(resolve_theme(*config));
    check(*parent != *base && parent->identity() != base->identity() && parent->tree() == base->tree() &&
              parent->select() == base->select() && parent->auto_complete() == base->auto_complete() &&
              parent->diagnostic_json().find("\"treeSelect\":") != std::string::npos,
          "TreeSelect snapshot isolation or serialization failed");
    auto local = std::make_unique<ThemeConfig>();
    local->tree_select.seed.font_size = dp(-1);
    check(resolve_theme(*local, parent.get()).tree_select() == parent->tree_select(),
          "inactive seed changed inheritance");
    local->tree_select.seed.font_size = dp(20);
    local->tree_select.algorithm = true;
    check(resolve_theme(*local, parent.get()).tree_select().tree.metrics.font_size == 20,
          "component algorithm did not apply");
    local->tree_select.algorithm = false;
    local->inherit = false;
    check(resolve_theme(*local, parent.get()).tree_select() == base->tree_select(), "inherit=false failed");
    local = std::make_unique<ThemeConfig>();
    local->alias.color_background_container = Color::rgba8(12, 34, 56);
    check(resolve_theme(*local).tree_select().selector.selector_background == *local->alias.color_background_container,
          "global alias was discarded while deriving TreeSelect");

    auto root = ThemeScope::create_default();
    const std::array accessors{&ThemeScope::tree_select_colors,      &ThemeScope::tree_select_metrics,
                               &ThemeScope::tree_select_effects,     &ThemeScope::tree_select_order,
                               &ThemeScope::tree_select_tree_colors, &ThemeScope::tree_select_tree_metrics};
    std::array<int, 6> notifications{};
    std::array<Subscription, 6> subscriptions;
    for (std::size_t i = 0; i < accessors.size(); ++i) {
        subscriptions[i] = root->capture(
            [&, i](DirtyPhase phase) {
                ++notifications[i];
                const auto id = static_cast<TokenIdentity>(static_cast<int>(TokenIdentity::tree_select_colors) + i);
                check(phase == dirty_phase_for(id), "TreeSelect dirty phase disagrees with identity");
            },
            [root, accessor = accessors[i]] { static_cast<void>((root.get()->*accessor)()); });
    }
    auto update = std::make_unique<ThemeConfig>();
    const auto update_one = [&](std::size_t i) {
        const auto previous = notifications;
        root->update(*update);
        for (std::size_t j = 0; j < notifications.size(); ++j) {
            check(notifications[j] == previous[j] + (i == j ? 1 : 0), "TreeSelect dirty locality failed");
        }
        check(root->changed_identities().size() == 1, "TreeSelect update changed unrelated identities");
    };
    update->tree_select.tokens.selector.popup_background = Color::rgba8(1, 2, 3);
    update_one(0);
    update->tree_select.tokens.selector.gap = dp(9);
    update_one(1);
    update->tree_select.tokens.selector.shadow = ShadowList{};
    update_one(2);
    update->tree_select.tokens.selector.z_index_popup = 1200;
    update_one(3);
    update->tree_select.tokens.tree.text = Color::rgba8(4, 5, 6);
    update_one(4);
    update->tree_select.tokens.tree.font_size = dp(19);
    update_one(5);
    auto child = ThemeScope::create(root, *std::make_unique<ThemeConfig>());
    const auto* before = &root->snapshot();
    const auto* child_before = &child->snapshot();
    for (float font : {.49F, 1e8F, std::numeric_limits<float>::infinity(), std::numeric_limits<float>::quiet_NaN()}) {
        auto bad = std::make_unique<ThemeConfig>(*update);
        bad->tree_select.tokens.tree.font_size = dp(font);
        rejects([&] { root->update(*bad); });
        check(before == &root->snapshot() && child_before == &child->snapshot(),
              "failed font update published snapshot");
    }
    auto bad = std::make_unique<ThemeConfig>(*update);
    bad->tree_select.tokens.selector.gap = LogicalLength{};
    rejects([&] { root->update(*bad); });
    bad->tree_select.tokens.selector.gap = dp(-1);
    rejects([&] { root->update(*bad); });
    bad->tree_select.tokens.selector.gap = dp(4e34F);
    rejects([&] { root->update(*bad); });
    bad = std::make_unique<ThemeConfig>();
    bad->tree_select.tokens.tree.font_size = dp(.5F);
    check(resolve_theme(*bad).tree_select().tree.metrics.font_size == .5F, "threshold font was rejected");

    auto ancestor = ThemeScope::create_default();
    auto small = std::make_unique<ThemeConfig>();
    small->tree_select.algorithm = true;
    small->tree_select.seed.font_size = dp(1);
    auto descendant = ThemeScope::create(ancestor, *small);
    auto compact = std::make_unique<ThemeConfig>();
    compact->algorithms = {ThemeAlgorithm::Compact};
    before = &ancestor->snapshot();
    child_before = &descendant->snapshot();
    for (int retry = 0; retry < 2; ++retry) {
        rejects([&] { ancestor->update(*compact); });
        check(before == &ancestor->snapshot() && child_before == &descendant->snapshot(),
              "descendant algorithm failure was not atomic");
    }
    small->tree_select.seed.font_size = dp(14);
    descendant->update(*small);
    check(ancestor->update(*compact) && descendant->snapshot().tree_select().tree.metrics.font_size > 0,
          "ancestor preflight did not recover");
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
        const auto theme = std::make_unique<ThemeSnapshot>(resolve_theme(*config));
        std::ofstream file{output / (std::string{name} + ".json"), std::ios::binary};
        file << theme->diagnostic_json();
        check(file.good(), "TreeSelect fresh golden write failed");
    }
}
} // namespace

int main(int argc, char** argv) {
    try {
        run();
        if (argc == 2) {
            goldens(argv[1]);
        }
        std::cout << "TreeSelect Theme passed\n";
        return 0;
    } catch (const std::exception& e) {
        std::cerr << e.what() << '\n';
        return 1;
    }
}
