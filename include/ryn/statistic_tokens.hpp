#pragma once
#include <ryn/design_token.hpp>
#include <ryn/layout_style.hpp>
#include <array>

namespace ryn {
struct StatisticColorToken final {
    Color title;
    Color content;

    [[nodiscard]] auto values() const {
        return std::array{title, content};
    }

    friend bool operator==(const StatisticColorToken&, const StatisticColorToken&) = default;
};

struct StatisticMetricToken final {
    float title_font_size{14};
    float content_font_size{24};
    float title_line_height{22};
    float content_line_height{32};
    float title_gap{4};
    float affix_gap{4};
    float loading_padding{16};
    SystemFontFamily font_family{SystemFontFamily::ui_sans};

    [[nodiscard]] auto values() const {
        return std::array{title_font_size, content_font_size, title_line_height, content_line_height,
                          title_gap,       affix_gap,         loading_padding};
    }

    friend bool operator==(const StatisticMetricToken&, const StatisticMetricToken&) = default;
};

struct StatisticThemeToken final {
    StatisticColorToken colors;
    StatisticMetricToken metrics;
    friend bool operator==(const StatisticThemeToken&, const StatisticThemeToken&) = default;
};

struct StatisticTokenOverride final {
    std::optional<Color> title;
    std::optional<Color> content;
    std::optional<LogicalLength> title_font_size;
    std::optional<LogicalLength> content_font_size;
    std::optional<LogicalLength> title_line_height;
    std::optional<LogicalLength> content_line_height;
    std::optional<LogicalLength> title_gap;
    std::optional<LogicalLength> affix_gap;
    std::optional<LogicalLength> loading_padding;
    std::optional<SystemFontFamily> font_family;
    friend bool operator==(const StatisticTokenOverride&, const StatisticTokenOverride&) = default;
};
} // namespace ryn
