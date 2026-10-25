#pragma once
#include <ryn/design_token.hpp>
#include <ryn/layout_style.hpp>
#include <array>

namespace ryn {
struct TimelineColorToken final {
    Color text;
    Color tail;
    Color dot_background;
    Color node;
    Color blue;
    Color red;
    Color green;
    Color gray;

    [[nodiscard]] auto values() const {
        return std::array{text, tail, dot_background, node, blue, red, green, gray};
    }

    friend bool operator==(const TimelineColorToken&, const TimelineColorToken&) = default;
};

struct TimelineMetricToken final {
    float tail_width{2};
    float dot_border_width{2};
    float dot_size{10};
    float icon_size{14};
    float item_padding_bottom{20};
    float content_gap{16};
    float font_size{14};
    float line_height{22};
    SystemFontFamily font_family{SystemFontFamily::ui_sans};

    [[nodiscard]] auto values() const {
        return std::array{tail_width,          dot_border_width, dot_size,  icon_size,
                          item_padding_bottom, content_gap,      font_size, line_height};
    }

    friend bool operator==(const TimelineMetricToken&, const TimelineMetricToken&) = default;
};

struct TimelineThemeToken final {
    TimelineColorToken colors;
    TimelineMetricToken metrics;
    friend bool operator==(const TimelineThemeToken&, const TimelineThemeToken&) = default;
};

struct TimelineTokenOverride final {
    std::optional<Color> text;
    std::optional<Color> tail;
    std::optional<Color> dot_background;
    std::optional<Color> node;
    std::optional<Color> blue;
    std::optional<Color> red;
    std::optional<Color> green;
    std::optional<Color> gray;
    std::optional<LogicalLength> tail_width;
    std::optional<LogicalLength> dot_border_width;
    std::optional<LogicalLength> dot_size;
    std::optional<LogicalLength> icon_size;
    std::optional<LogicalLength> item_padding_bottom;
    std::optional<LogicalLength> content_gap;
    std::optional<LogicalLength> font_size;
    std::optional<LogicalLength> line_height;
    std::optional<SystemFontFamily> font_family;
    friend bool operator==(const TimelineTokenOverride&, const TimelineTokenOverride&) = default;
};
} // namespace ryn
