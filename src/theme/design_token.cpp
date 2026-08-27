#include <ryn/design_token.hpp>

#include <algorithm>
#include <array>

namespace ryn {
namespace {

[[nodiscard]] constexpr Color black(float alpha) {
    return Color(0.0F, 0.0F, 0.0F, alpha);
}

[[nodiscard]] constexpr ShadowLayer outer(
    float x,
    float y,
    float blur,
    float spread,
    Color color) {
    return {ShadowKind::outer, {x, y}, blur, spread, color};
}

[[nodiscard]] constexpr ShadowLayer inset(
    float x,
    float y,
    float blur,
    float spread,
    Color color) {
    return {ShadowKind::inset, {x, y}, blur, spread, color};
}

constexpr AntDesignDefaultSeed default_seed{
    .color_primary = Color::rgba8(22, 119, 255),
    .color_success = Color::rgba8(82, 196, 26),
    .color_warning = Color::rgba8(250, 173, 20),
    .color_error = Color::rgba8(255, 77, 79),
    .color_info = Color::rgba8(22, 119, 255),
};

constexpr AntDesignShadowSnapshot default_shadows{
    .box_shadow = {{
        outer(0.0F, 6.0F, 16.0F, 0.0F, black(0.08F)),
        outer(0.0F, 3.0F, 6.0F, -4.0F, black(0.12F)),
        outer(0.0F, 9.0F, 28.0F, 8.0F, black(0.05F)),
    }},
    .box_shadow_secondary = {{
        outer(0.0F, 6.0F, 16.0F, 0.0F, black(0.08F)),
        outer(0.0F, 3.0F, 6.0F, -4.0F, black(0.12F)),
        outer(0.0F, 9.0F, 28.0F, 8.0F, black(0.05F)),
    }},
    .box_shadow_tertiary = {{
        outer(0.0F, 1.0F, 2.0F, 0.0F, black(0.05F)),
        outer(0.0F, 1.0F, 6.0F, -1.0F, black(0.03F)),
        outer(0.0F, 2.0F, 4.0F, 0.0F, black(0.03F)),
    }},
    .button_default = {{outer(0.0F, 2.0F, 0.0F, 0.0F, black(0.02F))}},
    .button_primary = {{
        outer(0.0F, 2.0F, 0.0F, 0.0F, Color(5.0F / 255.0F, 145.0F / 255.0F, 1.0F, 0.1F)),
    }},
    .button_danger = {{
        outer(0.0F, 2.0F, 0.0F, 0.0F, Color(1.0F, 38.0F / 255.0F, 5.0F / 255.0F, 0.06F)),
    }},
    .popover_arrow = {{outer(2.0F, 2.0F, 5.0F, 0.0F, black(0.05F))}},
    .popover_drop = {{
        outer(0.0F, 6.0F, 16.0F, 0.0F, black(0.08F)),
        outer(0.0F, 3.0F, 6.0F, 0.0F, black(0.12F)),
        outer(0.0F, 9.0F, 28.0F, 0.0F, black(0.05F)),
    }},
    .card = {{
        outer(0.0F, 1.0F, 2.0F, -2.0F, black(0.16F)),
        outer(0.0F, 3.0F, 6.0F, 0.0F, black(0.12F)),
        outer(0.0F, 5.0F, 12.0F, 4.0F, black(0.09F)),
    }},
    .drawer_right = {{
        outer(-6.0F, 0.0F, 16.0F, 0.0F, black(0.08F)),
        outer(-3.0F, 0.0F, 6.0F, -4.0F, black(0.12F)),
        outer(-9.0F, 0.0F, 28.0F, 8.0F, black(0.05F)),
    }},
    .drawer_left = {{
        outer(6.0F, 0.0F, 16.0F, 0.0F, black(0.08F)),
        outer(3.0F, 0.0F, 6.0F, -4.0F, black(0.12F)),
        outer(9.0F, 0.0F, 28.0F, 8.0F, black(0.05F)),
    }},
    .drawer_up = {{
        outer(0.0F, 6.0F, 16.0F, 0.0F, black(0.08F)),
        outer(0.0F, 3.0F, 6.0F, -4.0F, black(0.12F)),
        outer(0.0F, 9.0F, 28.0F, 8.0F, black(0.05F)),
    }},
    .drawer_down = {{
        outer(0.0F, -6.0F, 16.0F, 0.0F, black(0.08F)),
        outer(0.0F, -3.0F, 6.0F, -4.0F, black(0.12F)),
        outer(0.0F, -9.0F, 28.0F, 8.0F, black(0.05F)),
    }},
    .tabs_overflow_left = {{inset(10.0F, 0.0F, 8.0F, -8.0F, black(0.08F))}},
    .tabs_overflow_right = {{inset(-10.0F, 0.0F, 8.0F, -8.0F, black(0.08F))}},
    .tabs_overflow_top = {{inset(0.0F, 10.0F, 8.0F, -8.0F, black(0.08F))}},
    .tabs_overflow_bottom = {{inset(0.0F, -10.0F, 8.0F, -8.0F, black(0.08F))}},
};

constexpr auto metadata = std::array{
#include "theme/generated_token_metadata.inc"
};

} // namespace

const AntDesignDefaultSeed& ant_design_default_seed() noexcept {
    return default_seed;
}

const AntDesignShadowSnapshot& ant_design_default_shadows() noexcept {
    return default_shadows;
}

std::span<const TokenMetadata> ant_design_token_metadata() noexcept {
    return metadata;
}

const TokenMetadata* find_ant_design_token(std::string_view identity) noexcept {
    const auto found = std::lower_bound(
        metadata.begin(), metadata.end(), identity,
        [](const TokenMetadata& entry, std::string_view value) {
            return entry.identity < value;
        });
    return found != metadata.end() && found->identity == identity ? &*found : nullptr;
}

} // namespace ryn
