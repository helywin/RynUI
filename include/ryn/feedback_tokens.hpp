#pragma once
#include <ryn/design_token.hpp>
#include <ryn/layout_style.hpp>
#include <array>
#include <optional>

namespace ryn {
struct AlertPalette final {
    Color background;
    Color border;
    Color icon;
    friend bool operator==(const AlertPalette&, const AlertPalette&) = default;
};

struct AlertColorToken final {
    std::array<AlertPalette, 6> statuses;
    Color message;
    Color description;
    Color close;
    Color close_hover;
    Color focus;
    friend bool operator==(const AlertColorToken&, const AlertColorToken&) = default;
};

struct AlertMetricToken final {
    float padding_inline{12};
    float padding_block{8};
    float description_padding_inline{24};
    float description_padding_block{20};
    float gap{8};
    float description_gap{8};
    float icon_size{14};
    float description_icon_size{24};
    float close_size{12};
    float border_width{1};
    float border_radius{8};
    float font_size{14};
    float line_height{22};
    float message_font_size{16};
    float message_line_height{24};
    float focus_width{3};
    float focus_offset{1};
    SystemFontFamily font_family{SystemFontFamily::ui_sans};

    [[nodiscard]] auto values() const {
        return std::array{padding_inline,
                          padding_block,
                          description_padding_inline,
                          description_padding_block,
                          gap,
                          description_gap,
                          icon_size,
                          description_icon_size,
                          close_size,
                          border_width,
                          border_radius,
                          font_size,
                          line_height,
                          message_font_size,
                          message_line_height,
                          focus_width,
                          focus_offset};
    }

    friend bool operator==(const AlertMetricToken&, const AlertMetricToken&) = default;
};

struct AlertThemeToken final {
    AlertColorToken colors;
    AlertMetricToken metrics;
    friend bool operator==(const AlertThemeToken&, const AlertThemeToken&) = default;
};

struct AlertTokenOverride final {
    std::optional<Color> message_color;
    std::optional<Color> description_color;
    std::optional<Color> close_color;
    std::optional<LogicalLength> padding_inline;
    std::optional<LogicalLength> padding_block;
    std::optional<LogicalLength> description_padding_inline;
    std::optional<LogicalLength> description_padding_block;
    std::optional<LogicalLength> icon_size;
    std::optional<LogicalLength> description_icon_size;
    std::optional<LogicalLength> border_radius;
    std::optional<LogicalLength> font_size;
    friend bool operator==(const AlertTokenOverride&, const AlertTokenOverride&) = default;
};

struct SpinColorToken final {
    Color indicator;
    Color mask;
    friend bool operator==(const SpinColorToken&, const SpinColorToken&) = default;
};

struct SpinMetricToken final {
    float dot_size{20};
    float dot_size_small{14};
    float dot_size_large{32};
    float description_gap{8};
    float font_size{14};
    float line_height{22};
    SystemFontFamily font_family{SystemFontFamily::ui_sans};

    [[nodiscard]] auto values() const {
        return std::array{dot_size, dot_size_small, dot_size_large, description_gap, font_size, line_height};
    }

    friend bool operator==(const SpinMetricToken&, const SpinMetricToken&) = default;
};

struct SpinThemeToken final {
    SpinColorToken colors;
    SpinMetricToken metrics;
    friend bool operator==(const SpinThemeToken&, const SpinThemeToken&) = default;
};

struct SpinTokenOverride final {
    std::optional<Color> indicator_color;
    std::optional<Color> mask_color;
    std::optional<LogicalLength> dot_size;
    std::optional<LogicalLength> dot_size_small;
    std::optional<LogicalLength> dot_size_large;
    friend bool operator==(const SpinTokenOverride&, const SpinTokenOverride&) = default;
};

struct ProgressColorToken final {
    Color normal;
    Color success;
    Color exception;
    Color remaining;
    Color text;
    friend bool operator==(const ProgressColorToken&, const ProgressColorToken&) = default;
};

struct ProgressMetricToken final {
    float line_width{6};
    float line_width_small{4};
    float line_width_large{8};
    float line_length{200};
    float circle_size{120};
    float circle_size_small{60};
    float circle_size_large{160};
    float circle_stroke_ratio{0.06F};
    float line_radius{100};
    float info_width{44};
    float info_gap{8};
    float font_size{14};
    float line_height{22};
    SystemFontFamily font_family{SystemFontFamily::ui_sans};

    [[nodiscard]] auto values() const {
        return std::array{line_width,        line_width_small,  line_width_large,    line_length, circle_size,
                          circle_size_small, circle_size_large, circle_stroke_ratio, line_radius, info_width,
                          info_gap,          font_size,         line_height};
    }

    friend bool operator==(const ProgressMetricToken&, const ProgressMetricToken&) = default;
};

struct ProgressThemeToken final {
    ProgressColorToken colors;
    ProgressMetricToken metrics;
    friend bool operator==(const ProgressThemeToken&, const ProgressThemeToken&) = default;
};

struct ProgressTokenOverride final {
    std::optional<Color> default_color;
    std::optional<Color> remaining_color;
    std::optional<Color> text_color;
    std::optional<LogicalLength> line_width;
    std::optional<LogicalLength> line_width_small;
    std::optional<LogicalLength> line_width_large;
    std::optional<LogicalLength> circle_size;
    std::optional<LogicalLength> circle_size_small;
    std::optional<LogicalLength> circle_size_large;
    std::optional<float> circle_stroke_ratio;
    std::optional<LogicalLength> line_border_radius;
    friend bool operator==(const ProgressTokenOverride&, const ProgressTokenOverride&) = default;
};
} // namespace ryn
