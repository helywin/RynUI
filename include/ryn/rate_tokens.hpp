#pragma once
#include <ryn/design_token.hpp>
#include <ryn/layout_style.hpp>
#include <array>

namespace ryn {
struct RateColorToken final {
    Color star;
    Color background;

    [[nodiscard]] auto values() const {
        return std::array{star, background};
    }

    friend bool operator==(const RateColorToken&, const RateColorToken&) = default;
};

struct RateMetricToken final {
    float star_size{20};
    float star_size_small{15};
    float star_size_large{25};
    float gap{8};
    float hover_scale{1.1F};
    float focus_width{1};
    SystemFontFamily font_family{SystemFontFamily::ui_sans};

    [[nodiscard]] auto values() const {
        return std::array{star_size, star_size_small, star_size_large, gap, hover_scale, focus_width};
    }

    friend bool operator==(const RateMetricToken&, const RateMetricToken&) = default;
};

struct RateThemeToken final {
    RateColorToken colors;
    RateMetricToken metrics;
    friend bool operator==(const RateThemeToken&, const RateThemeToken&) = default;
};

struct RateTokenOverride final {
    std::optional<Color> star;
    std::optional<Color> background;
    std::optional<LogicalLength> star_size;
    std::optional<LogicalLength> star_size_small;
    std::optional<LogicalLength> star_size_large;
    std::optional<LogicalLength> gap;
    std::optional<float> hover_scale;
    std::optional<LogicalLength> focus_width;
    std::optional<SystemFontFamily> font_family;
    friend bool operator==(const RateTokenOverride&, const RateTokenOverride&) = default;
};
} // namespace ryn
