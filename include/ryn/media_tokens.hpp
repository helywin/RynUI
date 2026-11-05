#pragma once
#include <ryn/design_token.hpp>
#include <ryn/layout_style.hpp>
#include <array>

namespace ryn {
struct ImageColorToken final {
    Color background;
    Color text;
    Color secondary;
    Color error;
    Color disabled;
    Color border;
    Color hover;
    Color focus;
    Color cover;
    Color mask;
    Color preview_text;
    Color toolbar;

    auto values() const {
        return std::array{background, text,  secondary, error, disabled,     border,
                          hover,      focus, cover,     mask,  preview_text, toolbar};
    }

    friend bool operator==(const ImageColorToken&, const ImageColorToken&) = default;
};

struct ImageMetricToken final {
    float placeholder_size{120};
    float thumbnail_size{96};
    float preview_width{800};
    float preview_height{480};
    float row_height{32};
    float gap{8};
    float padding{8};
    float radius{6};
    float border_width{1};

    auto values() const {
        return std::array{placeholder_size, thumbnail_size, preview_width, preview_height, row_height, gap,
                          padding,          radius,         border_width};
    }

    friend bool operator==(const ImageMetricToken&, const ImageMetricToken&) = default;
};

struct ImageTypographyToken final {
    float font_size{14};
    float line_height{22};
    SystemFontFamily font_family{SystemFontFamily::ui_sans};
    friend bool operator==(const ImageTypographyToken&, const ImageTypographyToken&) = default;
};

struct ImageEffectToken final {
    float focus_width{3};
    float focus_offset{1};
    friend bool operator==(const ImageEffectToken&, const ImageEffectToken&) = default;
};

struct ImageThemeToken final {
    ImageColorToken colors;
    ImageMetricToken metrics;
    ImageTypographyToken typography;
    ImageEffectToken effects;
    int order{};
    friend bool operator==(const ImageThemeToken&, const ImageThemeToken&) = default;
};

struct ImageTokenOverride final {
    std::optional<Color> background;
    std::optional<Color> text;
    std::optional<Color> secondary;
    std::optional<Color> error;
    std::optional<Color> disabled;
    std::optional<Color> border;
    std::optional<Color> hover;
    std::optional<Color> focus;
    std::optional<Color> cover;
    std::optional<Color> mask;
    std::optional<Color> preview_text;
    std::optional<Color> toolbar;
    std::optional<LogicalLength> placeholder_size;
    std::optional<LogicalLength> thumbnail_size;
    std::optional<LogicalLength> preview_width;
    std::optional<LogicalLength> preview_height;
    std::optional<LogicalLength> row_height;
    std::optional<LogicalLength> gap;
    std::optional<LogicalLength> padding;
    std::optional<LogicalLength> radius;
    std::optional<LogicalLength> border_width;
    std::optional<LogicalLength> font_size;
    std::optional<LogicalLength> line_height;
    std::optional<LogicalLength> focus_width;
    std::optional<LogicalLength> focus_offset;
    std::optional<SystemFontFamily> font_family;
    std::optional<int> order;
    friend bool operator==(const ImageTokenOverride&, const ImageTokenOverride&) = default;
};

struct UploadColorToken final {
    Color background;
    Color text;
    Color secondary;
    Color error;
    Color disabled;
    Color border;
    Color hover;
    Color focus;
    Color cover;
    Color mask;
    Color preview_text;
    Color toolbar;

    auto values() const {
        return std::array{background, text,  secondary, error, disabled,     border,
                          hover,      focus, cover,     mask,  preview_text, toolbar};
    }

    friend bool operator==(const UploadColorToken&, const UploadColorToken&) = default;
};

struct UploadMetricToken final {
    float placeholder_size{120};
    float thumbnail_size{96};
    float preview_width{800};
    float preview_height{480};
    float row_height{32};
    float gap{8};
    float padding{8};
    float radius{6};
    float border_width{1};

    auto values() const {
        return std::array{placeholder_size, thumbnail_size, preview_width, preview_height, row_height, gap,
                          padding,          radius,         border_width};
    }

    friend bool operator==(const UploadMetricToken&, const UploadMetricToken&) = default;
};

struct UploadTypographyToken final {
    float font_size{14};
    float line_height{22};
    SystemFontFamily font_family{SystemFontFamily::ui_sans};
    friend bool operator==(const UploadTypographyToken&, const UploadTypographyToken&) = default;
};

struct UploadEffectToken final {
    float focus_width{3};
    float focus_offset{1};
    friend bool operator==(const UploadEffectToken&, const UploadEffectToken&) = default;
};

struct UploadThemeToken final {
    UploadColorToken colors;
    UploadMetricToken metrics;
    UploadTypographyToken typography;
    UploadEffectToken effects;
    int order{};
    friend bool operator==(const UploadThemeToken&, const UploadThemeToken&) = default;
};

struct UploadTokenOverride final {
    std::optional<Color> background;
    std::optional<Color> text;
    std::optional<Color> secondary;
    std::optional<Color> error;
    std::optional<Color> disabled;
    std::optional<Color> border;
    std::optional<Color> hover;
    std::optional<Color> focus;
    std::optional<Color> cover;
    std::optional<Color> mask;
    std::optional<Color> preview_text;
    std::optional<Color> toolbar;
    std::optional<LogicalLength> placeholder_size;
    std::optional<LogicalLength> thumbnail_size;
    std::optional<LogicalLength> preview_width;
    std::optional<LogicalLength> preview_height;
    std::optional<LogicalLength> row_height;
    std::optional<LogicalLength> gap;
    std::optional<LogicalLength> padding;
    std::optional<LogicalLength> radius;
    std::optional<LogicalLength> border_width;
    std::optional<LogicalLength> font_size;
    std::optional<LogicalLength> line_height;
    std::optional<LogicalLength> focus_width;
    std::optional<LogicalLength> focus_offset;
    std::optional<SystemFontFamily> font_family;
    std::optional<int> order;
    friend bool operator==(const UploadTokenOverride&, const UploadTokenOverride&) = default;
};
} // namespace ryn
