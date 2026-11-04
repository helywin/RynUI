#pragma once

#include <ryn/design_token.hpp>
#include <ryn/layout_style.hpp>

#include <array>
#include <optional>

namespace ryn {

struct SplitterColorToken final {
    Color panel_background;
    Color bar;
    Color hover_bar;
    Color active_bar;
    Color icon;
    Color collapse_background;
    Color focus;
    Color preview;

    [[nodiscard]] auto values() const {
        return std::array{panel_background, bar, hover_bar, active_bar, icon, collapse_background, focus, preview};
    }

    friend bool operator==(const SplitterColorToken&, const SplitterColorToken&) = default;
};

struct SplitterMetricToken final {
    float split_bar_size{2};
    float split_trigger_size{6};
    float split_bar_draggable_size{20};
    float collapse_width{12};
    float collapse_height{24};
    float icon_size{12};
    float focus_width{2};
    float keyboard_step{10};

    [[nodiscard]] auto values() const {
        return std::array{split_bar_size, split_trigger_size, split_bar_draggable_size,
                          collapse_width, collapse_height,    icon_size,
                          focus_width,    keyboard_step};
    }

    friend bool operator==(const SplitterMetricToken&, const SplitterMetricToken&) = default;
};

struct SplitterThemeToken final {
    SplitterColorToken colors;
    SplitterMetricToken metrics;
    friend bool operator==(const SplitterThemeToken&, const SplitterThemeToken&) = default;
};

struct SplitterTokenOverride final {
    std::optional<Color> panel_background;
    std::optional<Color> bar;
    std::optional<Color> hover_bar;
    std::optional<Color> active_bar;
    std::optional<Color> icon;
    std::optional<Color> collapse_background;
    std::optional<Color> focus;
    std::optional<Color> preview;
    std::optional<LogicalLength> split_bar_size;
    std::optional<LogicalLength> split_trigger_size;
    std::optional<LogicalLength> split_bar_draggable_size;
    std::optional<LogicalLength> collapse_width;
    std::optional<LogicalLength> collapse_height;
    std::optional<LogicalLength> icon_size;
    std::optional<LogicalLength> focus_width;
    std::optional<LogicalLength> keyboard_step;
    friend bool operator==(const SplitterTokenOverride&, const SplitterTokenOverride&) = default;
};

} // namespace ryn
