#include "theme/theme_runtime.hpp"
#include <cmath>
#include <iostream>
#include <limits>
#include <stdexcept>
#include <thread>

using namespace ryn;
using namespace ryn::theme_runtime;

namespace {
void check(bool condition, const char* message) {
    if (!condition) {
        throw std::runtime_error(message);
    }
}

void run() {
    const auto base = resolve_theme();
    check(base.cascader().metrics.control_width == 184 && base.cascader().metrics.column_width == 111 &&
              base.cascader().metrics.dropdown_height == 180,
          "locked Cascader metrics");
    ThemeConfig config;
    config.cascader.tokens.option_selected_background = Color::rgba8(11, 22, 33);
    const auto parent = resolve_theme(config);
    check(parent != base && parent.identity() != base.identity() && parent.input_number() == base.input_number() &&
              parent.menu() == base.menu() && parent.color_picker() == base.color_picker(),
          "Cascader snapshot isolation");
    config.color_picker.tokens.panel_width = dp(310);
    const auto combined = resolve_theme(config);
    check(combined.cascader() == parent.cascader() && combined.color_picker().panel_width == 310 &&
              combined.identity() != parent.identity(),
          "combined Cascader and ColorPicker Theme identity");
    ThemeConfig local;
    local.cascader.seed.font_size = dp(-1);
    check(resolve_theme(local, &parent).cascader() == parent.cascader(), "inactive component seed");
    local.cascader.seed.font_size = dp(20);
    local.cascader.algorithm = true;
    check(resolve_theme(local, &parent).cascader().typography.font_size == 20, "component seed algorithm");
    local.cascader.seed.size_unit = dp(6);
    const auto spaced = resolve_theme(local, &parent);
    check(
        spaced.cascader().metrics.option_padding_inline == 18 && spaced.cascader().metrics.menu_padding == 6 &&
            spaced.cascader().metrics.option_padding_block ==
                std::max(0.F, std::round((spaced.map().control_height - spaced.cascader().typography.line_height) / 2)),
        "component spacing seed or rounded upstream padding recipe");
    local.cascader.algorithm = false;
    local.inherit = false;
    check(resolve_theme(local, &parent).cascader() == base.cascader(), "inherit false");
    auto root = ThemeScope::create_default();
    std::array<int, 4> calls{};
    const std::array accessors{&ThemeScope::cascader_colors, &ThemeScope::cascader_metrics,
                               &ThemeScope::cascader_typography, &ThemeScope::cascader_effects};
    std::array<bool, 4> thread_rejected{};
    std::thread worker([&] {
        for (std::size_t i = 0; i < accessors.size(); ++i) {
            try {
                (void)((root.get()->*accessors[i])());
            } catch (const std::logic_error&) {
                thread_rejected[i] = true;
            }
        }
    });
    worker.join();
    check(thread_rejected == std::array{true, true, true, true}, "Cascader Theme accessor accepted a foreign thread");
    std::array<Subscription, 4> subscriptions;
    for (std::size_t i = 0; i < 4; ++i) {
        subscriptions[i] = root->capture(
            [&, i](DirtyPhase phase) {
                ++calls[i];
                check(i != 0 || phase == DirtyPhase::paint_material, "palette invalidation");
                check(i != 1 || has_any(phase, DirtyPhase::measure_layout | DirtyPhase::hit_test),
                      "metrics invalidation");
                check(i != 2 || has_any(phase, DirtyPhase::text | DirtyPhase::measure_layout),
                      "typography invalidation");
                check(i != 3 || has_any(phase, DirtyPhase::geometry | DirtyPhase::paint_material),
                      "effects invalidation");
            },
            [&, i] { (void)((root.get()->*accessors[i])()); });
    }
    config = {};
    config.cascader.tokens.option_color = Color::rgba8(1, 2, 3);
    root->update(config);
    check(calls == std::array{1, 0, 0, 0}, "palette changed unrelated domains");
    config.cascader.tokens.option_height = dp(44);
    root->update(config);
    config.cascader.tokens.font_size = dp(18);
    root->update(config);
    config.cascader.tokens.focus_width = dp(5);
    root->update(config);
    check(calls == std::array{1, 1, 1, 1}, "independent Cascader domains");
    auto child = ThemeScope::create(root, ThemeConfig{});
    const auto identity = root->snapshot().identity();
    const auto child_identity = child->snapshot().identity();
    for (float value : {-1.F, std::numeric_limits<float>::infinity(), std::numeric_limits<float>::quiet_NaN(),
                        std::numeric_limits<float>::max()}) {
        auto invalid = config;
        invalid.cascader.tokens.column_width = dp(value);
        bool rejected = false;
        try {
            root->update(invalid);
        } catch (const std::invalid_argument&) {
            rejected = true;
        }
        check(rejected && root->snapshot().identity() == identity && child->snapshot().identity() == child_identity,
              "atomic theme preflight");
    }
    for (auto algorithm : {ThemeAlgorithm::Default, ThemeAlgorithm::Dark, ThemeAlgorithm::Compact}) {
        ThemeConfig c;
        c.algorithms = {algorithm};
        const auto snapshot = resolve_theme(c);
        check(snapshot.diagnostic_json().find("\"cascader\":") != std::string::npos &&
                  snapshot.diagnostic_json().find("\"anchor\":") != std::string::npos,
              "complete diagnostic fields");
    }
}
} // namespace

int main() {
    try {
        run();
        std::cout << "Cascader Theme domains, identity, inheritance and preflight passed\n";
        return 0;
    } catch (const std::exception& e) {
        std::cerr << e.what() << '\n';
        return 1;
    }
}
