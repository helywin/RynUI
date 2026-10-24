#pragma once
#include <ryn/design_token.hpp>
#include <ryn/layout_style.hpp>
#include <array>

namespace ryn {
struct StepsColorToken final {
    Color title;
    Color description;
    Color wait_text;
    Color process_text;
    Color finish_text;
    Color error;
    Color disabled;
    Color wait_background;
    Color process_background;
    Color finish_background;
    Color error_background;
    Color connector;
    Color finished_connector;
    Color focus;
    Color hover;

    [[nodiscard]] auto values() const {
        return std::array{title,
                          description,
                          wait_text,
                          process_text,
                          finish_text,
                          error,
                          disabled,
                          wait_background,
                          process_background,
                          finish_background,
                          error_background,
                          connector,
                          finished_connector,
                          focus,
                          hover};
    }

    friend bool operator==(const StepsColorToken&, const StepsColorToken&) = default;
};

struct StepsMetricToken final {
    float font_size{14};
    float title_font_size{16};
    float title_font_size_small{14};
    float line_height{22};
    float title_line_height{24};
    float title_line_height_small{22};
    float icon_size{32};
    float icon_size_small{24};
    float icon_font_size{14};
    float icon_gap{8};
    float subtitle_gap{8};
    float description_gap{4};
    float item_gap{16};
    float vertical_gap{16};
    float line_width{1};
    SystemFontFamily font_family{SystemFontFamily::ui_sans};

    [[nodiscard]] auto values() const {
        return std::array{font_size,   title_font_size,   title_font_size_small,
                          line_height, title_line_height, title_line_height_small,
                          icon_size,   icon_size_small,   icon_font_size,
                          icon_gap,    subtitle_gap,      description_gap,
                          item_gap,    vertical_gap,      line_width};
    }

    friend bool operator==(const StepsMetricToken&, const StepsMetricToken&) = default;
};

struct StepsEffectToken final {
    float focus_width{3};
    float focus_offset{1};
    friend bool operator==(const StepsEffectToken&, const StepsEffectToken&) = default;
};

struct StepsThemeToken final {
    StepsColorToken colors;
    StepsMetricToken metrics;
    StepsEffectToken effects;
    friend bool operator==(const StepsThemeToken&, const StepsThemeToken&) = default;
};

struct StepsTokenOverride final {
    std::optional<Color> title;
    std::optional<Color> description;
    std::optional<Color> wait_text;
    std::optional<Color> process_text;
    std::optional<Color> finish_text;
    std::optional<Color> error;
    std::optional<Color> disabled;
    std::optional<Color> wait_background;
    std::optional<Color> process_background;
    std::optional<Color> finish_background;
    std::optional<Color> error_background;
    std::optional<Color> connector;
    std::optional<Color> finished_connector;
    std::optional<Color> focus;
    std::optional<Color> hover;
    std::optional<LogicalLength> font_size;
    std::optional<LogicalLength> title_font_size;
    std::optional<LogicalLength> title_font_size_small;
    std::optional<LogicalLength> line_height;
    std::optional<LogicalLength> title_line_height;
    std::optional<LogicalLength> title_line_height_small;
    std::optional<LogicalLength> icon_size;
    std::optional<LogicalLength> icon_size_small;
    std::optional<LogicalLength> icon_font_size;
    std::optional<LogicalLength> icon_gap;
    std::optional<LogicalLength> subtitle_gap;
    std::optional<LogicalLength> description_gap;
    std::optional<LogicalLength> item_gap;
    std::optional<LogicalLength> vertical_gap;
    std::optional<LogicalLength> line_width;
    std::optional<LogicalLength> focus_width;
    std::optional<LogicalLength> focus_offset;
    std::optional<SystemFontFamily> font_family;
    friend bool operator==(const StepsTokenOverride&, const StepsTokenOverride&) = default;
};
} // namespace ryn
