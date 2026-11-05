#pragma once
#include <ryn/design_token.hpp>
#include <ryn/layout_style.hpp>
#include <array>

namespace ryn {
struct CarouselColorToken final {
    Color background;
    Color arrow;
    Color dot;
    Color dot_active;
    Color progress;
    Color focus;

    auto values() const {
        return std::array{background, arrow, dot, dot_active, progress, focus};
    }

    friend bool operator==(const CarouselColorToken&, const CarouselColorToken&) = default;
};

struct CarouselMetricToken final {
    float width{600};
    float height{160};
    float dot_width{16};
    float dot_height{3};
    float dot_gap{4};
    float dot_offset{12};
    float dot_active_width{24};
    float arrow_size{16};
    float arrow_hit_size{32};
    float dot_hit_size{24};
    float arrow_offset{8};
    float focus_width{2};
    float font_size{14};
    float line_height{22};
    SystemFontFamily font_family{SystemFontFamily::ui_sans};

    auto values() const {
        return std::array{width,        height,           dot_width,  dot_height,     dot_gap,
                          dot_offset,   dot_active_width, arrow_size, arrow_hit_size, dot_hit_size,
                          arrow_offset, focus_width,      font_size,  line_height};
    }

    friend bool operator==(const CarouselMetricToken&, const CarouselMetricToken&) = default;
};

struct CarouselThemeToken final {
    CarouselColorToken colors;
    CarouselMetricToken metrics;
    ShadowList shadow;
    float disabled_opacity{0.25F};
    friend bool operator==(const CarouselThemeToken&, const CarouselThemeToken&) = default;
};

struct CarouselTokenOverride final {
    std::optional<Color> background;
    std::optional<Color> arrow;
    std::optional<Color> dot;
    std::optional<Color> dot_active;
    std::optional<Color> progress;
    std::optional<Color> focus;
    std::optional<LogicalLength> width;
    std::optional<LogicalLength> height;
    std::optional<LogicalLength> dot_width;
    std::optional<LogicalLength> dot_height;
    std::optional<LogicalLength> dot_gap;
    std::optional<LogicalLength> dot_offset;
    std::optional<LogicalLength> dot_active_width;
    std::optional<LogicalLength> arrow_size;
    std::optional<LogicalLength> arrow_hit_size;
    std::optional<LogicalLength> dot_hit_size;
    std::optional<LogicalLength> arrow_offset;
    std::optional<LogicalLength> focus_width;
    std::optional<LogicalLength> font_size;
    std::optional<LogicalLength> line_height;
    std::optional<SystemFontFamily> font_family;
    std::optional<ShadowList> shadow;
    std::optional<float> disabled_opacity;
    friend bool operator==(const CarouselTokenOverride&, const CarouselTokenOverride&) = default;
};
} // namespace ryn
