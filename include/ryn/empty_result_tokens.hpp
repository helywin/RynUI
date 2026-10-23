#pragma once
#include <ryn/design_token.hpp>
#include <ryn/layout_style.hpp>
#include <array>
#include <optional>

namespace ryn {
struct EmptyColorToken final {
    Color description;
    Color image;
    Color extra;

    [[nodiscard]] auto values() const {
        return std::array{description, image, extra};
    }

    friend bool operator==(const EmptyColorToken&, const EmptyColorToken&) = default;
};

struct EmptyMetricToken final {
    float padding_inline{8};
    float padding_block{0};
    float compact_padding_block{8};
    float image_size{100};
    float compact_image_size{40};
    float image_gap{8};
    float extra_gap{16};
    float action_gap{8};
    float decoration_padding{4};
    float font_size{14};
    float line_height{22};
    SystemFontFamily font_family{SystemFontFamily::ui_sans};

    [[nodiscard]] auto values() const {
        return std::array{
            padding_inline, padding_block, compact_padding_block, image_size, compact_image_size, image_gap,
            extra_gap,      action_gap,    decoration_padding,    font_size,  line_height};
    }

    friend bool operator==(const EmptyMetricToken&, const EmptyMetricToken&) = default;
};

struct EmptyThemeToken final {
    EmptyColorToken colors;
    EmptyMetricToken metrics;
    friend bool operator==(const EmptyThemeToken&, const EmptyThemeToken&) = default;
};

struct EmptyTokenOverride final {
    std::optional<Color> description;
    std::optional<Color> image;
    std::optional<Color> extra;
    std::optional<LogicalLength> padding_inline;
    std::optional<LogicalLength> padding_block;
    std::optional<LogicalLength> compact_padding_block;
    std::optional<LogicalLength> image_size;
    std::optional<LogicalLength> compact_image_size;
    std::optional<LogicalLength> image_gap;
    std::optional<LogicalLength> extra_gap;
    std::optional<LogicalLength> action_gap;
    std::optional<LogicalLength> decoration_padding;
    std::optional<LogicalLength> font_size;
    std::optional<LogicalLength> line_height;
    friend bool operator==(const EmptyTokenOverride&, const EmptyTokenOverride&) = default;
};

struct ResultColorToken final {
    Color title;
    Color subtitle;
    Color content;
    Color content_background;
    Color extra;
    Color info;
    Color success;
    Color warning;
    Color error;

    [[nodiscard]] auto values() const {
        return std::array{title, subtitle, content, content_background, extra, info, success, warning, error};
    }

    friend bool operator==(const ResultColorToken&, const ResultColorToken&) = default;
};

struct ResultMetricToken final {
    float padding_inline{32};
    float padding_block{48};
    float icon_size{72};
    float icon_gap{24};
    float title_gap{8};
    float extra_gap{24};
    float content_gap{24};
    float content_padding_inline{40};
    float content_padding_block{24};
    float action_gap{8};
    float decoration_padding{4};
    float title_font_size{24};
    float title_line_height{32};
    float font_size{14};
    float line_height{22};
    SystemFontFamily font_family{SystemFontFamily::ui_sans};

    [[nodiscard]] auto values() const {
        return std::array{padding_inline,
                          padding_block,
                          icon_size,
                          icon_gap,
                          title_gap,
                          extra_gap,
                          content_gap,
                          content_padding_inline,
                          content_padding_block,
                          action_gap,
                          decoration_padding,
                          title_font_size,
                          title_line_height,
                          font_size,
                          line_height};
    }

    friend bool operator==(const ResultMetricToken&, const ResultMetricToken&) = default;
};

struct ResultThemeToken final {
    ResultColorToken colors;
    ResultMetricToken metrics;
    friend bool operator==(const ResultThemeToken&, const ResultThemeToken&) = default;
};

struct ResultTokenOverride final {
    std::optional<Color> title;
    std::optional<Color> subtitle;
    std::optional<Color> content;
    std::optional<Color> content_background;
    std::optional<Color> extra;
    std::optional<Color> info;
    std::optional<Color> success;
    std::optional<Color> warning;
    std::optional<Color> error;
    std::optional<LogicalLength> padding_inline;
    std::optional<LogicalLength> padding_block;
    std::optional<LogicalLength> icon_size;
    std::optional<LogicalLength> icon_gap;
    std::optional<LogicalLength> title_gap;
    std::optional<LogicalLength> extra_gap;
    std::optional<LogicalLength> content_gap;
    std::optional<LogicalLength> content_padding_inline;
    std::optional<LogicalLength> content_padding_block;
    std::optional<LogicalLength> action_gap;
    std::optional<LogicalLength> decoration_padding;
    std::optional<LogicalLength> title_font_size;
    std::optional<LogicalLength> title_line_height;
    std::optional<LogicalLength> font_size;
    std::optional<LogicalLength> line_height;
    friend bool operator==(const ResultTokenOverride&, const ResultTokenOverride&) = default;
};

} // namespace ryn
