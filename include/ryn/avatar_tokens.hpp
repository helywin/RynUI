#pragma once
#include <ryn/design_token.hpp>
#include <ryn/layout_style.hpp>
#include <array>

namespace ryn {
struct AvatarColorToken final {
    Color text;
    Color background;
    Color group_border;

    [[nodiscard]] auto values() const {
        return std::array{text, background, group_border};
    }

    friend bool operator==(const AvatarColorToken&, const AvatarColorToken&) = default;
};

struct AvatarMetricToken final {
    float container_size{32};
    float container_size_large{40};
    float container_size_small{24};
    float text_font_size{14};
    float text_font_size_large{14};
    float text_font_size_small{14};
    float icon_font_size{18};
    float icon_font_size_large{24};
    float icon_font_size_small{14};
    float border_radius{6};
    float border_radius_large{8};
    float border_radius_small{4};
    float border_width{1};
    float group_space{4};
    float group_overlapping{-8};
    SystemFontFamily font_family{SystemFontFamily::ui_sans};

    [[nodiscard]] auto values() const {
        return std::array{container_size,       container_size_large, container_size_small, text_font_size,
                          text_font_size_large, text_font_size_small, icon_font_size,       icon_font_size_large,
                          icon_font_size_small, border_radius,        border_radius_large,  border_radius_small,
                          border_width,         group_space,          group_overlapping};
    }

    friend bool operator==(const AvatarMetricToken&, const AvatarMetricToken&) = default;
};

struct AvatarThemeToken final {
    AvatarColorToken colors;
    AvatarMetricToken metrics;
    friend bool operator==(const AvatarThemeToken&, const AvatarThemeToken&) = default;
};

struct AvatarTokenOverride final {
    std::optional<Color> text;
    std::optional<Color> background;
    std::optional<Color> group_border;
    std::optional<LogicalLength> container_size;
    std::optional<LogicalLength> container_size_large;
    std::optional<LogicalLength> container_size_small;
    std::optional<LogicalLength> text_font_size;
    std::optional<LogicalLength> text_font_size_large;
    std::optional<LogicalLength> text_font_size_small;
    std::optional<LogicalLength> icon_font_size;
    std::optional<LogicalLength> icon_font_size_large;
    std::optional<LogicalLength> icon_font_size_small;
    std::optional<LogicalLength> border_radius;
    std::optional<LogicalLength> border_radius_large;
    std::optional<LogicalLength> border_radius_small;
    std::optional<LogicalLength> border_width;
    std::optional<LogicalLength> group_space;
    std::optional<LogicalLength> group_overlapping;
    std::optional<SystemFontFamily> font_family;
    friend bool operator==(const AvatarTokenOverride&, const AvatarTokenOverride&) = default;
};
} // namespace ryn
