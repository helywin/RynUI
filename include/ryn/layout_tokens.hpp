#pragma once

#include <ryn/design_token.hpp>
#include <ryn/layout_style.hpp>

#include <array>
#include <optional>

namespace ryn {

struct LayoutColorToken final {
    Color body_background;
    Color header_background;
    Color header_text;
    Color footer_background;
    Color footer_text;
    Color sider_background;
    Color sider_text;
    Color trigger_background;
    Color trigger_text;
    Color light_sider_background;
    Color light_sider_text;
    Color light_trigger_background;
    Color light_trigger_text;
    Color focus;

    [[nodiscard]] auto values() const {
        return std::array{body_background,    header_background,
                          header_text,        footer_background,
                          footer_text,        sider_background,
                          sider_text,         trigger_background,
                          trigger_text,       light_sider_background,
                          light_sider_text,   light_trigger_background,
                          light_trigger_text, focus};
    }

    friend bool operator==(const LayoutColorToken&, const LayoutColorToken&) = default;
};

struct LayoutMetricToken final {
    float header_height{64};
    float header_padding_inline{50};
    float footer_padding_block{24};
    float footer_padding_inline{50};
    float trigger_height{48};
    float zero_trigger_width{40};
    float zero_trigger_height{40};
    float zero_trigger_top{64};
    float trigger_icon_size{14};
    float focus_width{2};
    float font_size{14};
    float line_height{22};

    [[nodiscard]] auto values() const {
        return std::array{header_height,
                          header_padding_inline,
                          footer_padding_block,
                          footer_padding_inline,
                          trigger_height,
                          zero_trigger_width,
                          zero_trigger_height,
                          zero_trigger_top,
                          trigger_icon_size,
                          focus_width,
                          font_size,
                          line_height};
    }

    friend bool operator==(const LayoutMetricToken&, const LayoutMetricToken&) = default;
};

struct LayoutThemeToken final {
    LayoutColorToken colors;
    LayoutMetricToken metrics;
    friend bool operator==(const LayoutThemeToken&, const LayoutThemeToken&) = default;
};

struct LayoutTokenOverride final {
    std::optional<Color> body_background;
    std::optional<Color> header_background;
    std::optional<Color> header_text;
    std::optional<Color> footer_background;
    std::optional<Color> footer_text;
    std::optional<Color> sider_background;
    std::optional<Color> sider_text;
    std::optional<Color> trigger_background;
    std::optional<Color> trigger_text;
    std::optional<Color> light_sider_background;
    std::optional<Color> light_sider_text;
    std::optional<Color> light_trigger_background;
    std::optional<Color> light_trigger_text;
    std::optional<Color> focus;
    std::optional<LogicalLength> header_height;
    std::optional<LogicalLength> header_padding_inline;
    std::optional<LogicalLength> footer_padding_block;
    std::optional<LogicalLength> footer_padding_inline;
    std::optional<LogicalLength> trigger_height;
    std::optional<LogicalLength> zero_trigger_width;
    std::optional<LogicalLength> zero_trigger_height;
    std::optional<LogicalLength> zero_trigger_top;
    std::optional<LogicalLength> trigger_icon_size;
    std::optional<LogicalLength> focus_width;
    std::optional<LogicalLength> font_size;
    std::optional<LogicalLength> line_height;
    friend bool operator==(const LayoutTokenOverride&, const LayoutTokenOverride&) = default;
};

} // namespace ryn
