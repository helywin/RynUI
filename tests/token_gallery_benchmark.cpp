#include "graphics/rounded_effect.hpp"

#include <ryn/rynui.hpp>

#include <array>
#include <chrono>
#include <cstddef>
#include <cstdint>
#include <exception>
#include <iostream>
#include <stdexcept>

namespace {

void require(bool condition, const char* message) {
    if (!condition) {
        throw std::runtime_error(message);
    }
}

void append_shadow_list(
    ryn::graphics::RoundedEffectStore& store,
    const ryn::ShadowList& shadows) {
    const ryn::graphics::LogicalRoundedRect shape{
        {40.0F, 40.0F, 172.0F, 32.0F}, 6.0F,
    };
    for (const auto& layer : shadows.layers()) {
        static_cast<void>(store.add(ryn::graphics::make_shadow_effect(shape, layer)));
    }
}

} // namespace

int main() {
    try {
        const auto started = std::chrono::steady_clock::now();
        constexpr std::size_t iterations = 2'000;
        std::uint64_t checksum = 0;
        for (std::size_t index = 0; index < iterations; ++index) {
            ryn::ThemeConfig dark;
            dark.algorithms = {ryn::ThemeAlgorithm::Dark};
            ryn::ThemeConfig compact;
            compact.algorithms = {ryn::ThemeAlgorithm::Compact};
            ryn::ThemeConfig brand;
            brand.seed.color_primary = ryn::Color::rgba8(114, 46, 209);
            checksum ^= ryn::resolve_theme().identity();
            checksum ^= ryn::resolve_theme(dark).identity();
            checksum ^= ryn::resolve_theme(compact).identity();
            checksum ^= ryn::resolve_theme(brand).identity();
        }

        const auto& shadows = ryn::ant_design_default_shadows();
        const std::array<const ryn::ShadowList*, 17> lists{
            &shadows.box_shadow_tertiary,
            &shadows.box_shadow_secondary,
            &shadows.box_shadow,
            &shadows.button_default,
            &shadows.button_primary,
            &shadows.button_danger,
            &shadows.drawer_left,
            &shadows.drawer_right,
            &shadows.drawer_up,
            &shadows.drawer_down,
            &shadows.popover_arrow,
            &shadows.popover_drop,
            &shadows.card,
            &shadows.tabs_overflow_left,
            &shadows.tabs_overflow_right,
            &shadows.tabs_overflow_top,
            &shadows.tabs_overflow_bottom,
        };
        ryn::graphics::RoundedEffectStore store;
        store.reserve(64);
        for (const auto* list : lists) {
            append_shadow_list(store, *list);
        }
        static_cast<void>(store.add(ryn::graphics::make_outline_effect(
            {{40.0F, 40.0F, 172.0F, 32.0F}, 6.0F},
            3.0F,
            1.0F,
            ryn::Color::rgba8(22, 119, 255))));
        require(store.compact({0.0F, 0.0F, 320.0F, 160.0F}),
                "Token Gallery benchmark did not compact the initial effect set");
        const auto layer_count = store.live_count();
        require(layer_count == 36,
                "Token Gallery benchmark shadow inventory lost a layer");

        const auto elapsed = std::chrono::duration_cast<std::chrono::microseconds>(
            std::chrono::steady_clock::now() - started);
        require(elapsed < std::chrono::seconds(30),
                "Token Gallery benchmark exceeded the hang guard");
        std::cout
            << "theme_resolutions=" << iterations * 4
            << " effect_layers=" << layer_count
            << " elapsed_us=" << elapsed.count()
            << " checksum=" << checksum << '\n';
    } catch (const std::exception& error) {
        std::cerr << error.what() << '\n';
        return 1;
    }
    return 0;
}
