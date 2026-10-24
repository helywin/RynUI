#pragma once
#include <ryn/design_token.hpp>
#include <ryn/layout_style.hpp>
#include <array>

namespace ryn {
struct DescriptionsColorToken final {
    Color label_background;
    Color label;
    Color bordered_label;
    Color content;
    Color title;
    Color extra;
    Color border;

    [[nodiscard]] auto values() const {
        return std::array{label_background, label, bordered_label, content, title, extra, border};
    }

    friend bool operator==(const DescriptionsColorToken&, const DescriptionsColorToken&) = default;
};

struct DescriptionsMetricToken final {
    float title_margin_bottom{20};
    float item_padding_bottom{16};
    float item_padding_bottom_middle{12};
    float item_padding_bottom_small{8};
    float item_padding_end{16};
    float colon_margin_left{2};
    float colon_margin_right{8};
    float padding_block{16};
    float padding_block_middle{12};
    float padding_block_small{8};
    float padding_inline{24};
    float padding_inline_small{16};
    float border_width{1};
    float border_radius{8};
    float font_size{14};
    float line_height{22};
    float title_font_size{16};
    float title_line_height{24};
    SystemFontFamily font_family{SystemFontFamily::ui_sans};
    std::uint32_t title_font_weight{600};

    [[nodiscard]] auto values() const {
        return std::array{title_margin_bottom,
                          item_padding_bottom,
                          item_padding_bottom_middle,
                          item_padding_bottom_small,
                          item_padding_end,
                          colon_margin_left,
                          colon_margin_right,
                          padding_block,
                          padding_block_middle,
                          padding_block_small,
                          padding_inline,
                          padding_inline_small,
                          border_width,
                          border_radius,
                          font_size,
                          line_height,
                          title_font_size,
                          title_line_height};
    }

    friend bool operator==(const DescriptionsMetricToken&, const DescriptionsMetricToken&) = default;
};

struct DescriptionsThemeToken final {
    DescriptionsColorToken colors;
    DescriptionsMetricToken metrics;
    friend bool operator==(const DescriptionsThemeToken&, const DescriptionsThemeToken&) = default;
};

struct DescriptionsTokenOverride final {
    std::optional<Color> label_background;
    std::optional<Color> label;
    std::optional<Color> bordered_label;
    std::optional<Color> content;
    std::optional<Color> title;
    std::optional<Color> extra;
    std::optional<Color> border;
    std::optional<LogicalLength> title_margin_bottom;
    std::optional<LogicalLength> item_padding_bottom;
    std::optional<LogicalLength> item_padding_bottom_middle;
    std::optional<LogicalLength> item_padding_bottom_small;
    std::optional<LogicalLength> item_padding_end;
    std::optional<LogicalLength> colon_margin_left;
    std::optional<LogicalLength> colon_margin_right;
    std::optional<LogicalLength> padding_block;
    std::optional<LogicalLength> padding_block_middle;
    std::optional<LogicalLength> padding_block_small;
    std::optional<LogicalLength> padding_inline;
    std::optional<LogicalLength> padding_inline_small;
    std::optional<LogicalLength> border_width;
    std::optional<LogicalLength> border_radius;
    std::optional<LogicalLength> font_size;
    std::optional<LogicalLength> line_height;
    std::optional<LogicalLength> title_font_size;
    std::optional<LogicalLength> title_line_height;
    std::optional<SystemFontFamily> font_family;
    std::optional<std::uint32_t> title_font_weight;
    friend bool operator==(const DescriptionsTokenOverride&, const DescriptionsTokenOverride&) = default;
};
} // namespace ryn
