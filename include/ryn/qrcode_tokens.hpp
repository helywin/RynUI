#pragma once
#include <ryn/design_token.hpp>
#include <ryn/layout_style.hpp>

namespace ryn {
struct QRCodeColorToken final {
    Color foreground;
    Color background;
    Color border;
    Color cover;
    Color text;

    auto values() const {
        return std::array{foreground, background, border, cover, text};
    }

    friend bool operator==(const QRCodeColorToken&, const QRCodeColorToken&) = default;
};

struct QRCodeMetricToken final {
    float padding{12};
    float radius{8};
    float border_width{1};
    float font_size{14};
    float line_height{22};
    SystemFontFamily font_family{SystemFontFamily::ui_sans};

    auto values() const {
        return std::array{padding, radius, border_width, font_size, line_height};
    }

    friend bool operator==(const QRCodeMetricToken&, const QRCodeMetricToken&) = default;
};

struct QRCodeThemeToken final {
    QRCodeColorToken colors;
    QRCodeMetricToken metrics;
    friend bool operator==(const QRCodeThemeToken&, const QRCodeThemeToken&) = default;
};

struct QRCodeTokenOverride final {
    std::optional<Color> foreground;
    std::optional<Color> background;
    std::optional<Color> border;
    std::optional<Color> cover;
    std::optional<Color> text;
    std::optional<LogicalLength> padding;
    std::optional<LogicalLength> radius;
    std::optional<LogicalLength> border_width;
    std::optional<LogicalLength> font_size;
    std::optional<LogicalLength> line_height;
    std::optional<SystemFontFamily> font_family;
    friend bool operator==(const QRCodeTokenOverride&, const QRCodeTokenOverride&) = default;
};
} // namespace ryn
