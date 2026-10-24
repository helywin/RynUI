#pragma once

#include <ryn/design_token.hpp>
#include <ryn/layout_style.hpp>
#include <array>

namespace ryn {

struct FormColorToken final {
    Color label;
    Color required;
    Color help;
    Color error;

    [[nodiscard]] auto values() const {
        return std::array{label, required, help, error};
    }

    friend bool operator==(const FormColorToken&, const FormColorToken&) = default;
};

struct FormMetricToken final {
    float item_margin_bottom{24};
    float label_gap{8};
    float required_gap{4};
    float font_size{14};
    float line_height{22};
    SystemFontFamily font_family{SystemFontFamily::ui_sans};

    [[nodiscard]] auto values() const {
        return std::array{item_margin_bottom, label_gap, required_gap, font_size, line_height};
    }

    friend bool operator==(const FormMetricToken&, const FormMetricToken&) = default;
};

struct FormThemeToken final {
    FormColorToken colors;
    FormMetricToken metrics;
    friend bool operator==(const FormThemeToken&, const FormThemeToken&) = default;
};

struct FormTokenOverride final {
    std::optional<Color> label;
    std::optional<Color> required;
    std::optional<Color> help;
    std::optional<Color> error;
    std::optional<LogicalLength> item_margin_bottom;
    std::optional<LogicalLength> label_gap;
    std::optional<LogicalLength> required_gap;
    std::optional<LogicalLength> font_size;
    std::optional<LogicalLength> line_height;
    std::optional<SystemFontFamily> font_family;
    friend bool operator==(const FormTokenOverride&, const FormTokenOverride&) = default;
};

} // namespace ryn
