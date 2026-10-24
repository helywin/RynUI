#pragma once
#include <ryn/design_token.hpp>
#include <ryn/layout_style.hpp>
#include <array>

namespace ryn {
struct BreadcrumbColorToken final {
    Color item;
    Color link;
    Color hover;
    Color current;
    Color separator;
    Color disabled;
    Color focus;
    Color hover_background;

    [[nodiscard]] auto values() const {
        return std::array{item, link, hover, current, separator, disabled, focus, hover_background};
    }

    friend bool operator==(const BreadcrumbColorToken&, const BreadcrumbColorToken&) = default;
};

struct BreadcrumbMetricToken final {
    float font_size{14};
    float line_height{22};
    float icon_size{14};
    float separator_margin{8};
    float icon_gap{4};
    float padding{4};
    float radius{4};
    SystemFontFamily font_family{SystemFontFamily::ui_sans};

    [[nodiscard]] auto values() const {
        return std::array{font_size, line_height, icon_size, separator_margin, icon_gap, padding, radius};
    }

    friend bool operator==(const BreadcrumbMetricToken&, const BreadcrumbMetricToken&) = default;
};

struct BreadcrumbEffectToken final {
    float focus_width{3};
    float focus_offset{1};
    friend bool operator==(const BreadcrumbEffectToken&, const BreadcrumbEffectToken&) = default;
};

struct BreadcrumbThemeToken final {
    BreadcrumbColorToken colors;
    BreadcrumbMetricToken metrics;
    BreadcrumbEffectToken effects;
    friend bool operator==(const BreadcrumbThemeToken&, const BreadcrumbThemeToken&) = default;
};

struct BreadcrumbTokenOverride final {
    std::optional<Color> item;
    std::optional<Color> link;
    std::optional<Color> hover;
    std::optional<Color> current;
    std::optional<Color> separator;
    std::optional<Color> disabled;
    std::optional<Color> focus;
    std::optional<Color> hover_background;
    std::optional<LogicalLength> font_size;
    std::optional<LogicalLength> line_height;
    std::optional<LogicalLength> icon_size;
    std::optional<LogicalLength> separator_margin;
    std::optional<LogicalLength> icon_gap;
    std::optional<LogicalLength> padding;
    std::optional<LogicalLength> radius;
    std::optional<LogicalLength> focus_width;
    std::optional<LogicalLength> focus_offset;
    std::optional<SystemFontFamily> font_family;
    friend bool operator==(const BreadcrumbTokenOverride&, const BreadcrumbTokenOverride&) = default;
};
} // namespace ryn
