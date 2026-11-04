#pragma once
#include <ryn/design_token.hpp>
#include <array>

namespace ryn {
struct CascaderColorToken final {
    Color selector_background{};
    Color selector_color{};
    Color placeholder{};
    Color border{};
    Color hover_border{};
    Color active_border{};
    Color disabled_background{};
    Color disabled_color{};
    Color popup_background{};
    Color option_color{};
    Color option_hover_background{};
    Color option_selected_background{};
    Color option_selected_color{};
    Color separator{};
    Color arrow{};
    Color clear{};
    Color tag_background{};
    Color tag_color{};
    Color search_highlight{};
    Color empty{};
    Color focus{};

    [[nodiscard]] auto values() const {
        return std::array{selector_background,
                          selector_color,
                          placeholder,
                          border,
                          hover_border,
                          active_border,
                          disabled_background,
                          disabled_color,
                          popup_background,
                          option_color,
                          option_hover_background,
                          option_selected_background,
                          option_selected_color,
                          separator,
                          arrow,
                          clear,
                          tag_background,
                          tag_color,
                          search_highlight,
                          empty,
                          focus};
    }

    friend bool operator==(const CascaderColorToken&, const CascaderColorToken&) = default;
};

struct CascaderMetricToken final {
    float control_width{};
    float column_width{};
    float dropdown_height{};
    float option_height{};
    float option_padding_inline{};
    float option_padding_block{};
    float menu_padding{};
    float selector_height{};
    float selector_height_small{};
    float selector_height_large{};
    float selector_padding{};
    float tag_height{};
    float tag_gap{};
    float tag_padding{};
    float radius{};

    [[nodiscard]] auto values() const {
        return std::array{
            control_width,        column_width, dropdown_height, option_height,         option_padding_inline,
            option_padding_block, menu_padding, selector_height, selector_height_small, selector_height_large,
            selector_padding,     tag_height,   tag_gap,         tag_padding,           radius};
    }

    friend bool operator==(const CascaderMetricToken&, const CascaderMetricToken&) = default;
};

struct CascaderTypographyToken final {
    SystemFontFamily font_family{SystemFontFamily::ui_sans};
    std::uint32_t font_weight{400};
    std::uint32_t selected_font_weight{600};
    float font_size{};
    float line_height{};

    [[nodiscard]] auto values() const {
        return std::array{font_size, line_height};
    }

    friend bool operator==(const CascaderTypographyToken&, const CascaderTypographyToken&) = default;
};

struct CascaderEffectToken final {
    float focus_width{};
    float focus_offset{};

    [[nodiscard]] auto values() const {
        return std::array{focus_width, focus_offset};
    }

    friend bool operator==(const CascaderEffectToken&, const CascaderEffectToken&) = default;
};

struct CascaderThemeToken final {
    CascaderColorToken colors;
    CascaderMetricToken metrics;
    CascaderTypographyToken typography;
    CascaderEffectToken effects;
    ShadowList shadow;
    std::int32_t z_index_popup{};
    friend bool operator==(const CascaderThemeToken&, const CascaderThemeToken&) = default;
};

struct CascaderTokenOverride final {
    std::optional<Color> selector_background;
    std::optional<Color> selector_color;
    std::optional<Color> placeholder;
    std::optional<Color> border;
    std::optional<Color> hover_border;
    std::optional<Color> active_border;
    std::optional<Color> disabled_background;
    std::optional<Color> disabled_color;
    std::optional<Color> popup_background;
    std::optional<Color> option_color;
    std::optional<Color> option_hover_background;
    std::optional<Color> option_selected_background;
    std::optional<Color> option_selected_color;
    std::optional<Color> separator;
    std::optional<Color> arrow;
    std::optional<Color> clear;
    std::optional<Color> tag_background;
    std::optional<Color> tag_color;
    std::optional<Color> search_highlight;
    std::optional<Color> empty;
    std::optional<Color> focus;
    std::optional<LogicalLength> control_width;
    std::optional<LogicalLength> column_width;
    std::optional<LogicalLength> dropdown_height;
    std::optional<LogicalLength> option_height;
    std::optional<LogicalLength> option_padding_inline;
    std::optional<LogicalLength> option_padding_block;
    std::optional<LogicalLength> menu_padding;
    std::optional<LogicalLength> selector_height;
    std::optional<LogicalLength> selector_height_small;
    std::optional<LogicalLength> selector_height_large;
    std::optional<LogicalLength> selector_padding;
    std::optional<LogicalLength> tag_height;
    std::optional<LogicalLength> tag_gap;
    std::optional<LogicalLength> tag_padding;
    std::optional<LogicalLength> radius;
    std::optional<LogicalLength> font_size;
    std::optional<LogicalLength> line_height;
    std::optional<LogicalLength> focus_width;
    std::optional<LogicalLength> focus_offset;
    std::optional<SystemFontFamily> font_family;
    std::optional<std::uint32_t> font_weight;
    std::optional<std::uint32_t> selected_font_weight;
    std::optional<ShadowList> shadow;
    std::optional<std::int32_t> z_index_popup;
    friend bool operator==(const CascaderTokenOverride&, const CascaderTokenOverride&) = default;
};
} // namespace ryn
