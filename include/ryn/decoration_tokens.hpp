#pragma once
#include <ryn/design_token.hpp>
#include <optional>

namespace ryn {
struct WatermarkThemeToken final {
    Color color;
    float font_size{16};
    float line_height{22};
    SystemFontFamily font_family{SystemFontFamily::ui_sans};
    float image_opacity{0.15F};
    friend bool operator==(const WatermarkThemeToken&, const WatermarkThemeToken&) = default;
};

struct WatermarkTokenOverride final {
    std::optional<Color> color;
    std::optional<float> font_size;
    std::optional<float> line_height;
    std::optional<SystemFontFamily> font_family;
    std::optional<float> image_opacity;
    friend bool operator==(const WatermarkTokenOverride&, const WatermarkTokenOverride&) = default;
};

struct BorderBeamThemeToken final {
    Color color;
    float width{2};
    float radius{8};
    float length{80};
    friend bool operator==(const BorderBeamThemeToken&, const BorderBeamThemeToken&) = default;
};

struct BorderBeamTokenOverride final {
    std::optional<Color> color;
    std::optional<float> width;
    std::optional<float> radius;
    std::optional<float> length;
    friend bool operator==(const BorderBeamTokenOverride&, const BorderBeamTokenOverride&) = default;
};

struct AffixThemeToken final {
    std::int32_t z_index{10};
    friend bool operator==(const AffixThemeToken&, const AffixThemeToken&) = default;
};

struct AffixTokenOverride final {
    std::optional<std::int32_t> z_index;
    friend bool operator==(const AffixTokenOverride&, const AffixTokenOverride&) = default;
};

struct WatermarkThemeConfig final {
    WatermarkTokenOverride tokens;
    friend bool operator==(const WatermarkThemeConfig&, const WatermarkThemeConfig&) = default;
};

struct BorderBeamThemeConfig final {
    BorderBeamTokenOverride tokens;
    friend bool operator==(const BorderBeamThemeConfig&, const BorderBeamThemeConfig&) = default;
};

struct AffixThemeConfig final {
    AffixTokenOverride tokens;
    friend bool operator==(const AffixThemeConfig&, const AffixThemeConfig&) = default;
};
} // namespace ryn
