#pragma once
#include <ryn/design_token.hpp>
#include <ryn/layout_style.hpp>
#include <array>

namespace ryn {
struct NoticeColorToken final {
    Color background;
    Color text;
    Color description;
    Color success;
    Color info;
    Color warning;
    Color error;
    Color loading;

    [[nodiscard]] auto values() const {
        return std::array{background, text, description, success, info, warning, error, loading};
    }

    friend bool operator==(const NoticeColorToken&, const NoticeColorToken&) = default;
};

struct NoticeMetricToken final {
    float width{384};
    float padding_block{16};
    float padding_inline{24};
    float gap{8};
    float stack_gap{16};
    float offset{24};
    float border_radius{8};
    float font_size{14};
    float line_height{22};
    float title_font_size{16};
    float title_line_height{24};
    float icon_size{24};
    float close_size{22};
    SystemFontFamily font_family{SystemFontFamily::ui_sans};
    std::uint32_t title_font_weight{600};

    [[nodiscard]] auto values() const {
        return std::array{
            width,       padding_block,   padding_inline,    gap,       stack_gap, offset, border_radius, font_size,
            line_height, title_font_size, title_line_height, icon_size, close_size};
    }

    friend bool operator==(const NoticeMetricToken&, const NoticeMetricToken&) = default;
};

struct NoticeThemeToken final {
    NoticeColorToken colors;
    NoticeMetricToken metrics;
    ShadowList shadow;
    std::int32_t z_index{2050};
    friend bool operator==(const NoticeThemeToken&, const NoticeThemeToken&) = default;
};

struct NoticeTokenOverride final {
    std::optional<Color> background;
    std::optional<Color> text;
    std::optional<Color> description;
    std::optional<Color> success;
    std::optional<Color> info;
    std::optional<Color> warning;
    std::optional<Color> error;
    std::optional<Color> loading;
    std::optional<LogicalLength> width;
    std::optional<LogicalLength> padding_block;
    std::optional<LogicalLength> padding_inline;
    std::optional<LogicalLength> gap;
    std::optional<LogicalLength> stack_gap;
    std::optional<LogicalLength> offset;
    std::optional<LogicalLength> border_radius;
    std::optional<LogicalLength> font_size;
    std::optional<LogicalLength> line_height;
    std::optional<LogicalLength> title_font_size;
    std::optional<LogicalLength> title_line_height;
    std::optional<LogicalLength> icon_size;
    std::optional<LogicalLength> close_size;
    std::optional<SystemFontFamily> font_family;
    std::optional<std::uint32_t> title_font_weight;
    std::optional<ShadowList> shadow;
    std::optional<std::int32_t> z_index;
    friend bool operator==(const NoticeTokenOverride&, const NoticeTokenOverride&) = default;
};

using MessageThemeToken = NoticeThemeToken;
using NotificationThemeToken = NoticeThemeToken;
} // namespace ryn
