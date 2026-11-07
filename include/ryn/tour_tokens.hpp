#pragma once
#include <ryn/design_token.hpp>
#include <ryn/layout_style.hpp>

namespace ryn {
struct TourThemeToken final {
    Color background;
    Color text;
    Color mask;
    Color highlight;
    float width{520};
    float padding{16};
    float gap{8};
    float radius{8};
    float target_gap{6};
    float highlight_width{2};
    float viewport_margin{8};
    std::int32_t z_index{1070};

    auto colors() const {
        return std::array{background, text, mask, highlight};
    }

    auto metrics() const {
        return std::array{width, padding, gap, radius, target_gap, highlight_width, viewport_margin};
    }

    friend bool operator==(const TourThemeToken&, const TourThemeToken&) = default;
};

struct TourTokenOverride final {
    std::optional<Color> background;
    std::optional<Color> text;
    std::optional<Color> mask;
    std::optional<Color> highlight;
    std::optional<LogicalLength> width;
    std::optional<LogicalLength> padding;
    std::optional<LogicalLength> gap;
    std::optional<LogicalLength> radius;
    std::optional<LogicalLength> target_gap;
    std::optional<LogicalLength> highlight_width;
    std::optional<LogicalLength> viewport_margin;
    std::optional<std::int32_t> z_index;
    friend bool operator==(const TourTokenOverride&, const TourTokenOverride&) = default;
};
} // namespace ryn
