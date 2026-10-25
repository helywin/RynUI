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

void recipe() {
    const auto base = std::make_unique<ThemeSnapshot>(resolve_theme());
    check(base->message().metrics.padding_block == 9 && base->message().metrics.padding_inline == 12 &&
              base->message().metrics.offset == 8 && base->notification().metrics.width == 384 &&
              base->notification().metrics.padding_block == 16 && base->notification().metrics.padding_inline == 24 &&
              base->message().z_index == 2010 && base->notification().z_index == 2050,
          "locked notice recipe drifted");
    auto config = std::make_unique<ThemeConfig>();
    config->message.tokens.text = Color::rgba8(30, 70, 90);
    config->notification.tokens.width = dp(420);
    const auto parent = std::make_unique<ThemeSnapshot>(resolve_theme(*config));
    auto local = std::make_unique<ThemeConfig>();
    local->notification.seed.font_size = dp(22);
    local->message.seed.size_unit = dp(-1);
    check(resolve_theme(*local, parent.get()).message() == parent->message() &&
              resolve_theme(*local, parent.get()).notification() == parent->notification(),
          "inactive seed broke inherited notice overrides");
    local->message.seed.size_unit.reset();
    local->notification.algorithm = true;
    check(resolve_theme(*local).notification().metrics.font_size == 22, "notice component algorithm ignored");
    local->notification.algorithm = false;
    local->inherit = false;
    check(resolve_theme(*local, parent.get()).notification() == base->notification(),
          "inherit false did not reset notice");
    check(parent->identity() != base->identity() && *parent != *base && parent->skeleton() == base->skeleton(),
          "notice identity or isolation missing");
    check(parent->diagnostic_json().find("\"notification\":") != std::string::npos, "notice serialization missing");
    for (const auto algorithms : {std::vector{ThemeAlgorithm::Dark}, std::vector{ThemeAlgorithm::Compact},
                                  std::vector{ThemeAlgorithm::Dark, ThemeAlgorithm::Compact}}) {
        config = std::make_unique<ThemeConfig>();
        config->algorithms = algorithms;
        const auto theme = std::make_unique<ThemeSnapshot>(resolve_theme(*config));
        check(theme->message().colors.background == theme->alias().color_background_elevated &&
                  theme->notification().metrics.font_size == theme->map().font_size,
              "global algorithm omitted notice");
    }
}

void runtime_validation() {
    static_assert(static_cast<int>(TokenIdentity::message_colors) ==
                  static_cast<int>(TokenIdentity::popover_order) + 1);
    const auto root = ThemeScope::create_default();
    int colors = 0;
    int metrics = 0;
    int order = 0;
    auto c = root->capture(
        [&](auto phase) {
            check(phase == DirtyPhase::paint_material, "notice color dirtied layout");
            ++colors;
        },
        [&] { static_cast<void>(root->message_colors()); });
    auto m = root->capture([&](auto) { ++metrics; }, [&] { static_cast<void>(root->notification_metrics()); });
    auto o = root->capture([&](auto) { ++order; }, [&] { static_cast<void>(root->notification_order()); });
    auto config = std::make_unique<ThemeConfig>();
    config->message.tokens.text = Color::rgba8(60, 40, 20);
    root->update(*config);
    check(colors == 1 && metrics == 0 && order == 0, "notice dirty domains leaked");
    config->notification.tokens.width = dp(420);
    config->notification.tokens.z_index = 2147483647;
    root->update(*config);
    check(colors == 1 && metrics == 1 && order == 1, "notice metric/order did not publish");
    const auto before = &root->snapshot();
    for (float value :
         {-.1F, .49F, 3e9F, std::numeric_limits<float>::infinity(), std::numeric_limits<float>::quiet_NaN()}) {
        auto invalid = std::make_unique<ThemeConfig>(*config);
        invalid->message.tokens.font_size = dp(value);
        bool threw = false;
        try {
            root->update(*invalid);
        } catch (const std::invalid_argument&) {
            threw = true;
        }
        check(threw && &root->snapshot() == before, "invalid notice font partially published");
    }
    auto invalid = std::make_unique<ThemeConfig>();
    invalid->notification.tokens.padding_inline = LogicalLength{};
    bool threw = false;
    try {
        static_cast<void>(resolve_theme(*invalid));
    } catch (const std::invalid_argument&) {
        threw = true;
    }
    check(threw, "auto notice metric accepted");
    auto child_config = std::make_unique<ThemeConfig>();
    child_config->message.tokens.padding_block = dp(2e37F);
    const auto child = ThemeScope::create(root, *child_config);
    const auto grandchild = ThemeScope::create(child, *std::make_unique<ThemeConfig>());
    const auto previous = &root->snapshot();
    const auto previous_child = &child->snapshot();
    config->message.tokens.padding_inline = dp(3e37F);
    threw = false;
    try {
        root->update(*config);
    } catch (const std::invalid_argument&) {
        threw = true;
    }
    check(threw && &root->snapshot() == previous && &child->snapshot() == previous_child &&
              grandchild->snapshot().message() == child->snapshot().message(),
          "notice descendant preflight was not atomic");
    config->message.tokens.padding_inline = dp(24);
    root->update(*config);
    check(grandchild->snapshot().message() == child->snapshot().message(), "notice preflight recovery failed");
}
} // namespace

int main() {
    try {
        recipe();
        runtime_validation();
        return 0;
    } catch (const std::exception& e) {
        std::cerr << e.what() << '\n';
        return 1;
    }
}
