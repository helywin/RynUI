#pragma once
#include <ryn/design_token.hpp>
#include <ryn/layout_style.hpp>
#include <array>

namespace ryn {
struct TransferColorToken final {
    Color background;
    Color header;
    Color footer;
    Color border;
    Color hover;
    Color selected;
    Color text;
    Color secondary;
    Color disabled;
    Color disabled_background;
    Color error;
    Color warning;
    Color focus;
    Color primary;
    Color primary_text;
    Color primary_hover;
    Color primary_active;

    [[nodiscard]] auto values() const {
        return std::array{
            background,          header, footer,  border, hover,   selected,     text,          secondary,     disabled,
            disabled_background, error,  warning, focus,  primary, primary_text, primary_hover, primary_active};
    }

    friend bool operator==(const TransferColorToken&, const TransferColorToken&) = default;
};

struct TransferMetricToken final {
    float header_height{32};
    float item_height{32};
    float item_padding_block{5};
    float list_width{180};
    float list_height{200};
    float list_width_large{250};
    float footer_height{40};
    float padding_inline{12};
    float control_height{32};
    float gap{8};
    float actions_width{40};
    float radius{6};
    float border_width{1};

    [[nodiscard]] auto values() const {
        return std::array{header_height,    item_height,   item_padding_block, list_width,     list_height,
                          list_width_large, footer_height, padding_inline,     control_height, gap,
                          actions_width,    radius,        border_width};
    }

    friend bool operator==(const TransferMetricToken&, const TransferMetricToken&) = default;
};

struct TransferTypographyToken final {
    float font_size{14};
    float line_height{22};
    SystemFontFamily font_family{SystemFontFamily::ui_sans};
    friend bool operator==(const TransferTypographyToken&, const TransferTypographyToken&) = default;
};

struct TransferEffectToken final {
    float focus_width{3};
    float focus_offset{1};
    friend bool operator==(const TransferEffectToken&, const TransferEffectToken&) = default;
};

struct TransferThemeToken final {
    TransferColorToken colors;
    TransferMetricToken metrics;
    TransferTypographyToken typography;
    TransferEffectToken effects;
    int order{};
    friend bool operator==(const TransferThemeToken&, const TransferThemeToken&) = default;
};

struct TransferTokenOverride final {
    std::optional<Color> background;
    std::optional<Color> header;
    std::optional<Color> footer;
    std::optional<Color> border;
    std::optional<Color> hover;
    std::optional<Color> selected;
    std::optional<Color> text;
    std::optional<Color> secondary;
    std::optional<Color> disabled;
    std::optional<Color> disabled_background;
    std::optional<Color> error;
    std::optional<Color> warning;
    std::optional<Color> focus;
    std::optional<Color> primary;
    std::optional<Color> primary_text;
    std::optional<Color> primary_hover;
    std::optional<Color> primary_active;
    std::optional<LogicalLength> header_height;
    std::optional<LogicalLength> item_height;
    std::optional<LogicalLength> item_padding_block;
    std::optional<LogicalLength> list_width;
    std::optional<LogicalLength> list_height;
    std::optional<LogicalLength> list_width_large;
    std::optional<LogicalLength> footer_height;
    std::optional<LogicalLength> padding_inline;
    std::optional<LogicalLength> control_height;
    std::optional<LogicalLength> gap;
    std::optional<LogicalLength> actions_width;
    std::optional<LogicalLength> radius;
    std::optional<LogicalLength> border_width;
    std::optional<LogicalLength> font_size;
    std::optional<LogicalLength> line_height;
    std::optional<SystemFontFamily> font_family;
    std::optional<LogicalLength> focus_width;
    std::optional<LogicalLength> focus_offset;
    std::optional<int> order;
    friend bool operator==(const TransferTokenOverride&, const TransferTokenOverride&) = default;
};
} // namespace ryn
