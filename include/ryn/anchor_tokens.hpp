#pragma once
#include <ryn/design_token.hpp>
#include <ryn/layout_style.hpp>
#include <array>

namespace ryn {
struct AnchorColorToken final {
    Color item;
    Color hover;
    Color current;
    Color separator;
    Color disabled;
    Color focus;
    Color hover_background;

    [[nodiscard]] auto values() const {
        return std::array{item, hover, current, separator, disabled, focus, hover_background};
    }

    friend bool operator==(const AnchorColorToken&, const AnchorColorToken&) = default;
};

struct AnchorMetricToken final {
    float font_size{14};
    float line_height{22};
    float padding_block{4};
    float indent{16};
    float padding{16};
    float radius{4};
    SystemFontFamily font_family{SystemFontFamily::ui_sans};

    [[nodiscard]] auto values() const {
        return std::array{font_size, line_height, padding_block, indent, padding, radius};
    }

    friend bool operator==(const AnchorMetricToken&, const AnchorMetricToken&) = default;
};

struct AnchorEffectToken final {
    float focus_width{3};
    float focus_offset{1};
    friend bool operator==(const AnchorEffectToken&, const AnchorEffectToken&) = default;
};

struct AnchorThemeToken final {
    AnchorColorToken colors;
    AnchorMetricToken metrics;
    AnchorEffectToken effects;
    friend bool operator==(const AnchorThemeToken&, const AnchorThemeToken&) = default;
};

struct AnchorTokenOverride final {
    std::optional<Color> item;
    std::optional<Color> hover;
    std::optional<Color> current;
    std::optional<Color> separator;
    std::optional<Color> disabled;
    std::optional<Color> focus;
    std::optional<Color> hover_background;
    std::optional<LogicalLength> font_size;
    std::optional<LogicalLength> line_height;
    std::optional<LogicalLength> padding_block;
    std::optional<LogicalLength> indent;
    std::optional<LogicalLength> padding;
    std::optional<LogicalLength> radius;
    std::optional<LogicalLength> focus_width;
    std::optional<LogicalLength> focus_offset;
    std::optional<SystemFontFamily> font_family;
    friend bool operator==(const AnchorTokenOverride&, const AnchorTokenOverride&) = default;
};
} // namespace ryn
