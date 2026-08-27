#pragma once

#include "theme/theme_runtime.hpp"

#include <ryn/flex.hpp>

#include <stdexcept>

namespace ryn::detail {

struct LayoutGapAccess final {
    [[nodiscard]] static const std::optional<SpaceSize>& preset(const LayoutGap& gap) noexcept {
        return gap.preset_;
    }

    [[nodiscard]] static float main(const LayoutGap& gap) noexcept {
        return gap.main_;
    }

    [[nodiscard]] static float cross(const LayoutGap& gap) noexcept {
        return gap.cross_;
    }
};

struct ResolvedLayoutGap final {
    float main{0.0F};
    float cross{0.0F};
};

[[nodiscard]] inline ResolvedLayoutGap resolve_layout_gap(
    const LayoutGap& gap,
    theme_runtime::ThemeScope& theme) {
    if (!LayoutGapAccess::preset(gap).has_value()) {
        return {LayoutGapAccess::main(gap), LayoutGapAccess::cross(gap)};
    }
    switch (*LayoutGapAccess::preset(gap)) {
    case SpaceSize::Small:
        return {theme.layout_gap_small(), theme.layout_gap_small()};
    case SpaceSize::Middle:
        return {theme.layout_gap_middle(), theme.layout_gap_middle()};
    case SpaceSize::Large:
        return {theme.layout_gap_large(), theme.layout_gap_large()};
    }
    throw std::invalid_argument("LayoutGap preset is invalid");
}

} // namespace ryn::detail
