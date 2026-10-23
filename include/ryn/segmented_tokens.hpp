#pragma once

#include <ryn/design_token.hpp>
#include <ryn/layout_style.hpp>
#include <array>

namespace ryn {
struct SegmentedColorToken final {
    Color track;
    Color item;
    Color hover;
    Color hover_background;
    Color active_background;
    Color selected;
    Color selected_background;
    Color disabled;
    Color focus;

    [[nodiscard]] auto values() const {
        return std::array{track,    item, hover, hover_background, active_background, selected, selected_background,
                          disabled, focus};
    }

    friend bool operator==(const SegmentedColorToken&, const SegmentedColorToken&) = default;
};

struct SegmentedMetricToken final {
    float track_padding{2};
    float height_small{24};
    float height{32};
    float height_large{40};
    float padding_small{7};
    float padding{11};
    float icon_gap{6};
    float radius_small{4};
    float radius{6};
    float radius_large{8};
    float item_radius_small{2};
    float item_radius{4};
    float item_radius_large{6};
    float font_size{14};
    float font_size_large{16};
    float line_height{22};
    float line_height_large{24};
    SystemFontFamily font_family{SystemFontFamily::ui_sans};

    [[nodiscard]] auto values() const {
        return std::array{
            track_padding,     height_small, height,          height_large, padding_small,     padding,
            icon_gap,          radius_small, radius,          radius_large, item_radius_small, item_radius,
            item_radius_large, font_size,    font_size_large, line_height,  line_height_large};
    }

    friend bool operator==(const SegmentedMetricToken&, const SegmentedMetricToken&) = default;
};

struct SegmentedEffectToken final {
    ShadowList selected_shadow;
    float focus_width{3};
    float focus_offset{1};
    friend bool operator==(const SegmentedEffectToken&, const SegmentedEffectToken&) = default;
};

struct SegmentedThemeToken final {
    SegmentedColorToken colors;
    SegmentedMetricToken metrics;
    SegmentedEffectToken effects;
    friend bool operator==(const SegmentedThemeToken&, const SegmentedThemeToken&) = default;
};

struct SegmentedTokenOverride final {
    std::optional<Color> track;
    std::optional<Color> item;
    std::optional<Color> hover;
    std::optional<Color> hover_background;
    std::optional<Color> active_background;
    std::optional<Color> selected;
    std::optional<Color> selected_background;
    std::optional<Color> disabled;
    std::optional<Color> focus;
    std::optional<LogicalLength> track_padding;
    std::optional<LogicalLength> height_small;
    std::optional<LogicalLength> height;
    std::optional<LogicalLength> height_large;
    std::optional<LogicalLength> padding_small;
    std::optional<LogicalLength> padding;
    std::optional<LogicalLength> icon_gap;
    std::optional<LogicalLength> radius_small;
    std::optional<LogicalLength> radius;
    std::optional<LogicalLength> radius_large;
    std::optional<LogicalLength> item_radius_small;
    std::optional<LogicalLength> item_radius;
    std::optional<LogicalLength> item_radius_large;
    std::optional<LogicalLength> font_size;
    std::optional<LogicalLength> font_size_large;
    std::optional<LogicalLength> line_height;
    std::optional<LogicalLength> line_height_large;
    std::optional<SystemFontFamily> font_family;
    std::optional<ShadowList> selected_shadow;
    std::optional<LogicalLength> focus_width;
    std::optional<LogicalLength> focus_offset;
    friend bool operator==(const SegmentedTokenOverride&, const SegmentedTokenOverride&) = default;
};
} // namespace ryn
