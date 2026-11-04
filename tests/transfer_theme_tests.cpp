#include "theme/theme_runtime.hpp"
#include <ryn/theme.hpp>
#include <iostream>
#include <stdexcept>
#include <thread>

namespace {
using namespace ryn;
using theme_runtime::TokenIdentity;

void check(bool condition, const char* message) {
    if (!condition) {
        throw std::runtime_error(message);
    }
}

void isolated(const ThemeConfig& config, TokenIdentity identity) {
    const auto baseline = resolve_theme();
    const auto resolved = resolve_theme(config);
    check(resolved.identity() != baseline.identity() && resolved.button() == baseline.button() &&
              resolved.color_picker() == baseline.color_picker() && resolved.cascader() == baseline.cascader() &&
              resolved.menu() == baseline.menu(),
          "Transfer override changed another component or did not enter snapshot hash");
    auto scope = theme_runtime::ThemeScope::create_default();
    check(scope->update(config), "Transfer update was discarded");
    const auto changed = scope->changed_identities();
    check(changed.size() == 1 && changed.front() == identity, "Transfer override dirtied another token domain");
    check(!scope->update(config), "equal Transfer override requested another update");
}

void every_field() {
    const auto colors = std::array{&TransferTokenOverride::background,    &TransferTokenOverride::header,
                                   &TransferTokenOverride::footer,        &TransferTokenOverride::border,
                                   &TransferTokenOverride::hover,         &TransferTokenOverride::selected,
                                   &TransferTokenOverride::text,          &TransferTokenOverride::secondary,
                                   &TransferTokenOverride::disabled,      &TransferTokenOverride::disabled_background,
                                   &TransferTokenOverride::error,         &TransferTokenOverride::warning,
                                   &TransferTokenOverride::focus,         &TransferTokenOverride::primary,
                                   &TransferTokenOverride::primary_text,  &TransferTokenOverride::primary_hover,
                                   &TransferTokenOverride::primary_active};
    for (auto member : colors) {
        ThemeConfig config;
        config.transfer.tokens.*member = Color::rgba8(17, 29, 43);
        isolated(config, TokenIdentity::transfer_colors);
    }
    const auto metrics =
        std::array{&TransferTokenOverride::header_height,      &TransferTokenOverride::item_height,
                   &TransferTokenOverride::item_padding_block, &TransferTokenOverride::list_width,
                   &TransferTokenOverride::list_height,        &TransferTokenOverride::list_width_large,
                   &TransferTokenOverride::footer_height,      &TransferTokenOverride::padding_inline,
                   &TransferTokenOverride::control_height,     &TransferTokenOverride::gap,
                   &TransferTokenOverride::actions_width,      &TransferTokenOverride::radius,
                   &TransferTokenOverride::border_width};
    const auto defaults = resolve_theme().transfer().metrics.values();
    for (std::size_t i = 0; i < metrics.size(); ++i) {
        ThemeConfig config;
        config.transfer.tokens.*metrics[i] = dp(defaults[i] + 1);
        isolated(config, TokenIdentity::transfer_metrics);
    }
    for (auto member : {&TransferTokenOverride::font_size, &TransferTokenOverride::line_height}) {
        ThemeConfig config;
        config.transfer.tokens.*member = dp(19);
        isolated(config, TokenIdentity::transfer_typography);
    }
    ThemeConfig family;
    family.transfer.tokens.font_family = SystemFontFamily::ui_monospace;
    isolated(family, TokenIdentity::transfer_typography);
    for (auto member : {&TransferTokenOverride::focus_width, &TransferTokenOverride::focus_offset}) {
        ThemeConfig config;
        config.transfer.tokens.*member = dp(2);
        isolated(config, TokenIdentity::transfer_effects);
    }
    ThemeConfig order;
    order.transfer.tokens.order = 2;
    isolated(order, TokenIdentity::transfer_order);
}

void accessor_thread_guards() {
    auto scope = theme_runtime::ThemeScope::create_default();
    const auto accessors =
        std::array{&theme_runtime::ThemeScope::transfer_colors, &theme_runtime::ThemeScope::transfer_metrics,
                   &theme_runtime::ThemeScope::transfer_typography, &theme_runtime::ThemeScope::transfer_effects,
                   &theme_runtime::ThemeScope::transfer_order};
    std::array<bool, 5> rejected{};
    std::thread worker([&] {
        for (std::size_t i = 0; i < accessors.size(); ++i) {
            try {
                static_cast<void>((scope.get()->*accessors[i])());
            } catch (const std::logic_error&) {
                rejected[i] = true;
            }
        }
    });
    worker.join();
    check(rejected == std::array{true, true, true, true, true}, "Transfer Theme accessor accepted a foreign thread");
}

void scope_seed_and_validation() {
    const auto baseline = resolve_theme();
    check(baseline.transfer().metrics.header_height == 40 && baseline.transfer().metrics.item_height == 32 &&
              baseline.transfer().metrics.item_padding_block == 5 && baseline.transfer().typography.font_size == 14,
          "locked Transfer default token parity drifted");
    ThemeConfig config;
    config.transfer.tokens.list_width = dp(210);
    config.transfer.seed.color_primary = Color::rgba8(240, 30, 20);
    const auto inactive = resolve_theme(config);
    check(inactive.transfer().colors == baseline.transfer().colors && inactive.transfer().metrics.list_width == 210,
          "Transfer inactive algorithm applied component seed");
    config.transfer.algorithm = true;
    const auto active = resolve_theme(config);
    check(active.transfer().colors.selected != baseline.transfer().colors.selected &&
              active.button() == baseline.button(),
          "Transfer component algorithm leaked or ignored seed");
    check(resolve_theme({}, &active).transfer() == active.transfer(), "Transfer inheritance lost override");
    ThemeConfig reset;
    reset.inherit = false;
    check(resolve_theme(reset, &active).transfer() == baseline.transfer(), "Transfer inherit=false did not reset");
    for (auto member : {&TransferTokenOverride::item_height, &TransferTokenOverride::font_size,
                        &TransferTokenOverride::line_height}) {
        bool threw = false;
        try {
            ThemeConfig invalid;
            invalid.transfer.tokens.*member = dp(0);
            static_cast<void>(resolve_theme(invalid));
        } catch (const std::invalid_argument&) {
            threw = true;
        }
        check(threw, "Transfer invalid final metric was accepted");
    }
}
} // namespace

int main() {
    try {
        every_field();
        scope_seed_and_validation();
        accessor_thread_guards();
        std::cout << "Transfer Theme: 36 field/domain checks and scoped seed/inherit/validation passed\n";
    } catch (const std::exception& e) {
        std::cerr << e.what() << '\n';
        return 1;
    }
}
