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

void theme_contract() {
    const auto base = resolve_theme();
    const auto& token = base.popconfirm();
    check(token.background == base.alias().color_background_elevated && token.warning == base.map().color_warning &&
              token.padding == 12 && token.message_gap == 8 && token.description_gap == 4 && token.button_gap == 8 &&
              token.border_radius == 8 && token.font_size == 14 && token.line_height == 22 &&
              token.title_font_weight == 600 && token.z_index_popup == 1060,
          "locked Popconfirm recipe drifted");
    ThemeConfig config;
    config.popconfirm.tokens.padding = dp(20);
    config.popconfirm.tokens.title = Color::rgba8(20, 80, 120);
    const auto parent = resolve_theme(config);
    check(parent.popconfirm().padding == 20 && resolve_theme({}, &parent).popconfirm() == parent.popconfirm() &&
              parent.button() == base.button() && parent.modal() == base.modal() && parent.tooltip() == base.tooltip(),
          "Popconfirm inheritance leaked");
    ThemeConfig reset;
    reset.inherit = false;
    check(resolve_theme(reset, &parent).popconfirm() == token, "Popconfirm reset failed");
    ThemeConfig independent;
    independent.popconfirm.algorithm = true;
    independent.popconfirm.seed.font_size = dp(18);
    check(resolve_theme(independent).popconfirm().font_size == 18 &&
              resolve_theme(independent).button() == base.button(),
          "Popconfirm independent seed failed");
    independent.popconfirm.algorithm = false;
    check(resolve_theme(independent).popconfirm() == token, "inactive component seed applied");
    ThemeConfig dark;
    dark.algorithms = {ThemeAlgorithm::Dark};
    check(resolve_theme(dark).popconfirm().background != token.background, "Popconfirm dark recipe failed");
    check(parent.identity() != base.identity() && parent.diagnostic_json().find("\"popconfirm\":") != std::string::npos,
          "Popconfirm missing snapshot identity/diagnostic");
    auto scope = ThemeScope::create_default();
    int colors{};
    int metrics{};
    int typography{};
    int effects{};
    int order{};
    auto c = scope->capture(
        [&](DirtyPhase phase) {
            check(phase == DirtyPhase::paint_material, "colors remeasured Popconfirm");
            ++colors;
        },
        [&] { static_cast<void>(scope->popconfirm_colors()); });
    auto m = scope->capture([&](DirtyPhase) { ++metrics; }, [&] { static_cast<void>(scope->popconfirm_metrics()); });
    auto t =
        scope->capture([&](DirtyPhase) { ++typography; }, [&] { static_cast<void>(scope->popconfirm_typography()); });
    auto e = scope->capture([&](DirtyPhase) { ++effects; }, [&] { static_cast<void>(scope->popconfirm_effects()); });
    auto o = scope->capture([&](DirtyPhase) { ++order; }, [&] { static_cast<void>(scope->popconfirm_order()); });
    ThemeConfig update;
    update.popconfirm.tokens.background = Color::rgba8(240, 250, 255);
    scope->update(update);
    check(colors == 1 && !metrics && !typography && !effects && !order, "color invalidation leaked");
    update.popconfirm.tokens.padding = dp(14);
    scope->update(update);
    update.popconfirm.tokens.title_font_weight = 700;
    scope->update(update);
    update.popconfirm.tokens.shadow = ShadowList{};
    scope->update(update);
    update.popconfirm.tokens.z_index_popup = 1200;
    scope->update(update);
    check(metrics == 1 && typography == 1 && effects == 1 && order == 1 && !scope->update(update),
          "Popconfirm grouped invalidation failed");
    const auto previous = scope->snapshot();
    const auto generation = scope->generation();
    const auto reject = [&](ThemeConfig invalid) {
        bool rejected{};
        try {
            scope->update(invalid);
        } catch (const std::invalid_argument&) {
            rejected = true;
        }
        check(rejected && scope->snapshot() == previous && scope->generation() == generation,
              "invalid Popconfirm theme partially published");
    };
    auto invalid = update;
    invalid.popconfirm.tokens.max_width = dp(0);
    reject(invalid);
    invalid = update;
    invalid.popconfirm.tokens.padding = dp(-1);
    reject(invalid);
    invalid = update;
    invalid.popconfirm.tokens.font_size = dp(0);
    reject(invalid);
    invalid = update;
    invalid.popconfirm.tokens.title_font_weight = 1001;
    reject(invalid);
    invalid = update;
    invalid.popconfirm.tokens.z_index_popup = -1;
    reject(invalid);
    invalid = update;
    invalid.popconfirm.tokens.padding = dp(std::numeric_limits<float>::max());
    reject(invalid);
}
} // namespace

int main() {
    try {
        theme_contract();
    } catch (const std::exception& error) {
        std::cerr << error.what() << '\n';
        return 1;
    }
}
