#include <ryn/theme.hpp>
#include "theme/theme_runtime.hpp"
#include <cmath>
#include <iostream>

namespace {
using namespace ryn;

void check(bool value, const char* message) {
    if (!value) {
        throw std::runtime_error(message);
    }
}

template <class F> void rejects(F f) {
    bool rejected = false;
    try {
        f();
    } catch (const std::invalid_argument&) {
        rejected = true;
    }
    check(rejected, "invalid feedback token accepted");
}
} // namespace

int main() {
    try {
        const auto base = resolve_theme({});
        check(base.alert().metrics.icon_size == 14 && base.alert().metrics.description_icon_size == 24 &&
                  base.alert().metrics.border_radius == 8,
              "Alert baseline differs");
        check(base.spin().metrics.dot_size == 20 && base.spin().metrics.dot_size_small == 14 &&
                  base.spin().metrics.dot_size_large == 32,
              "Spin baseline differs");
        check(base.progress().metrics.circle_size == 120 && base.progress().colors.normal == base.map().color_info,
              "Progress baseline differs");
        ThemeConfig dark;
        dark.algorithms = {ThemeAlgorithm::Dark};
        ThemeConfig compact;
        compact.algorithms = {ThemeAlgorithm::Compact};
        check(resolve_theme(dark).alert().colors != base.alert().colors &&
                  resolve_theme(dark).spin().colors.mask != base.spin().colors.mask,
              "Dark feedback unchanged");
        check(resolve_theme(compact).alert().metrics.font_size < base.alert().metrics.font_size &&
                  resolve_theme(compact).spin().metrics.dot_size < base.spin().metrics.dot_size,
              "Compact feedback unchanged");
        ThemeConfig custom;
        custom.alert.tokens.message_color = Color::rgba8(10, 20, 30);
        custom.spin.tokens.dot_size = dp(28);
        custom.progress.tokens.default_color = Color::rgba8(30, 20, 10);
        const auto parent = resolve_theme(custom);
        const auto nested = resolve_theme({}, &parent);
        check(nested.alert() == parent.alert() && nested.spin() == parent.spin() &&
                  nested.progress() == parent.progress(),
              "nested feedback inheritance lost");
        custom.inherit = false;
        check(resolve_theme(custom, &parent).identity() == parent.identity(), "equivalent theme changed identity");
        custom = ThemeConfig{};
        custom.progress.algorithm = true;
        custom.progress.seed.color_info = Color::rgba8(80, 40, 120);
        check(resolve_theme(custom).progress().colors.normal != base.progress().colors.normal &&
                  resolve_theme(custom).alert() == base.alert(),
              "component algorithm leaked");
        custom = ThemeConfig{};
        custom.spin.tokens.dot_size = dp(-1);
        rejects([&] { static_cast<void>(resolve_theme(custom)); });
        custom = ThemeConfig{};
        custom.alert.tokens.padding_inline = dp(-1);
        rejects([&] { static_cast<void>(resolve_theme(custom)); });
        custom = ThemeConfig{};
        custom.progress.tokens.circle_stroke_ratio = 1.1F;
        rejects([&] { static_cast<void>(resolve_theme(custom)); });
        auto scope = theme_runtime::ThemeScope::create_default();
        int paints = 0;
        int measures = 0;
        auto p = scope->capture([&](auto) { ++paints; }, [&] { static_cast<void>(scope->progress_colors()); });
        auto m = scope->capture([&](auto) { ++measures; }, [&] { static_cast<void>(scope->progress_metrics()); });
        custom = ThemeConfig{};
        custom.progress.tokens.default_color = Color::rgba8(12, 34, 56);
        scope->update(custom);
        if (paints != 1 || measures != 0) {
            std::cerr << "notifications paint=" << paints << " metrics=" << measures
                      << " changed=" << scope->changed_identities().size() << '\n';
        }
        check(paints == 1 && measures == 0, "progress color invalidated metrics");
        scope->update(custom);
        check(paints == 1 && measures == 0, "equivalent theme invalidated feedback");
        custom.progress.tokens.circle_size = dp(140);
        scope->update(custom);
        check(paints == 1 && measures == 1, "progress metrics invalidation missing");
        std::cout << "feedback baseline, algorithms, inheritance, token validation and precise invalidation passed\n";
    } catch (const std::exception& e) {
        std::cerr << e.what() << '\n';
        return 1;
    }
}
