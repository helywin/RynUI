#pragma once
#include <ryn/design_token.hpp>
#include <ryn/layout_style.hpp>
#include <array>
#include <optional>

namespace ryn {
struct SkeletonColorToken final {
    Color background;
    friend bool operator==(const SkeletonColorToken&, const SkeletonColorToken&) = default;
};

struct SkeletonMetricToken final {
    float title_height{16};
    float block_radius{4};
    float paragraph_margin_top{28};
    float paragraph_line_height{16};
    float title_paragraph_gap{24};
    float row_gap{16};
    float avatar_gap{16};
    float avatar_title_offset{12};
    float control_height{32};
    float control_height_small{24};
    float control_height_large{40};
    float control_radius{4};
    float intrinsic_width{240};

    [[nodiscard]] auto values() const {
        return std::array{title_height,         block_radius,
                          paragraph_margin_top, paragraph_line_height,
                          title_paragraph_gap,  row_gap,
                          avatar_gap,           avatar_title_offset,
                          control_height,       control_height_small,
                          control_height_large, control_radius,
                          intrinsic_width};
    }

    friend bool operator==(const SkeletonMetricToken&, const SkeletonMetricToken&) = default;
};

struct SkeletonThemeToken final {
    SkeletonColorToken colors;
    SkeletonMetricToken metrics;
    friend bool operator==(const SkeletonThemeToken&, const SkeletonThemeToken&) = default;
};

struct SkeletonTokenOverride final {
    std::optional<Color> background;
    std::optional<LogicalLength> title_height;
    std::optional<LogicalLength> block_radius;
    std::optional<LogicalLength> paragraph_margin_top;
    std::optional<LogicalLength> paragraph_line_height;
    std::optional<LogicalLength> title_paragraph_gap;
    std::optional<LogicalLength> row_gap;
    std::optional<LogicalLength> avatar_gap;
    std::optional<LogicalLength> avatar_title_offset;
    std::optional<LogicalLength> control_height;
    std::optional<LogicalLength> control_height_small;
    std::optional<LogicalLength> control_height_large;
    std::optional<LogicalLength> control_radius;
    std::optional<LogicalLength> intrinsic_width;
    friend bool operator==(const SkeletonTokenOverride&, const SkeletonTokenOverride&) = default;
};
} // namespace ryn
