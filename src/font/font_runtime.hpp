#pragma once

#include <cstddef>
#include <cstdint>
#include <filesystem>
#include <memory>
#include <optional>
#include <span>
#include <string>
#include <string_view>
#include <vector>

namespace ryn::font {

enum class FontErrorStage : std::uint8_t {
    none,
    owner_thread,
    library_initialization,
    resource_read,
    face_creation,
    charmap_selection,
    pixel_size_configuration,
    coverage_query,
    rasterization,
    shaping,
    destruction,
};

enum class FontErrorKind : std::uint8_t {
    none,
    wrong_thread,
    runtime_unavailable,
    resource_unavailable,
    invalid_font_data,
    invalid_face_index,
    no_unicode_charmap,
    invalid_pixel_size,
    invalid_identity,
    invalid_codepoint,
    missing_glyph,
    unsupported_bitmap,
    rasterization_failed,
    shaping_failed,
};

struct FontIdentity {
    std::uint32_t slot{};
    std::uint32_t generation{};

    friend bool operator==(FontIdentity, FontIdentity) = default;
};

struct FontError {
    FontErrorStage stage{FontErrorStage::none};
    FontErrorKind kind{FontErrorKind::none};
    FontIdentity font{};
    char32_t codepoint{};
    std::string diagnostic;

    [[nodiscard]] explicit operator bool() const noexcept {
        return kind != FontErrorKind::none;
    }
};

struct FontMetrics {
    float ascent{};
    float descent{};
    float line_gap{};
    std::uint32_t logical_pixel_size{};
    std::uint32_t raster_pixel_size{};
    float raster_scale{1.0F};

    friend bool operator==(const FontMetrics&, const FontMetrics&) = default;
};

struct FontRasterConfig {
    std::uint32_t logical_pixel_size{};
    float display_scale{1.0F};

    friend bool operator==(FontRasterConfig, FontRasterConfig) = default;
};

struct FontLoadResult {
    FontIdentity font{};
    FontError error{};

    [[nodiscard]] explicit operator bool() const noexcept {
        return !error;
    }
};

struct FontMetricsResult {
    FontMetrics metrics{};
    FontError error{};

    [[nodiscard]] explicit operator bool() const noexcept {
        return !error;
    }
};

struct GlyphSelection {
    FontIdentity font{};
    std::uint32_t glyph_id{};
    char32_t requested_codepoint{};
    char32_t resolved_codepoint{};
    bool used_replacement{};
};

struct GlyphLookupResult {
    GlyphSelection glyph{};
    FontError error{};

    [[nodiscard]] explicit operator bool() const noexcept {
        return !error;
    }
};

enum class GlyphRasterMode : std::uint8_t {
    grayscale,
};

struct GlyphBounds {
    int left{};
    int top{};
    std::uint32_t width{};
    std::uint32_t height{};
};

struct GlyphBitmap {
    std::vector<std::uint8_t> coverage;
    std::uint32_t width{};
    std::uint32_t height{};
    std::uint32_t row_stride{};
    int bearing_x{};
    int bearing_y{};
    float advance_x{};
    GlyphBounds visible_bounds{};
    float raster_scale{1.0F};
};

struct GlyphRasterResult {
    const GlyphBitmap* glyph{};
    bool cache_hit{};
    FontError error{};

    [[nodiscard]] explicit operator bool() const noexcept {
        return !error;
    }
};

struct FontShapedGlyph {
    std::uint32_t glyph_id{};
    std::size_t cluster{};
    float advance_x{};
    float advance_y{};
    float offset_x{};
    float offset_y{};
    float extent_x_bearing{};
    float extent_y_bearing{};
    float extent_width{};
    float extent_height{};
};

struct FontShapeResult {
    std::vector<FontShapedGlyph> glyphs;
    bool right_to_left{};
    FontError error{};

    [[nodiscard]] explicit operator bool() const noexcept {
        return !error;
    }
};

struct FontActionResult {
    FontError error{};

    [[nodiscard]] explicit operator bool() const noexcept {
        return !error;
    }
};

struct CoverageNormalizationResult {
    std::vector<std::uint8_t> coverage;
    FontError error{};

    [[nodiscard]] explicit operator bool() const noexcept {
        return !error;
    }
};

enum class FontFailurePoint : std::uint8_t {
    none,
    library_initialization,
    after_font_bytes,
    after_face_creation,
    charmap_selection,
    pixel_size_configuration,
    rasterization,
};

struct FontRuntimeCounters {
    std::size_t libraries_acquired{};
    std::size_t libraries_released{};
    std::size_t byte_resources_acquired{};
    std::size_t byte_resources_released{};
    std::size_t faces_acquired{};
    std::size_t faces_released{};
    std::size_t coverage_queries{};
    std::size_t rasterizations{};
    std::size_t cache_hits{};
};

struct FontRuntimeOptions {
    FontFailurePoint failure_point{FontFailurePoint::none};
    std::shared_ptr<FontRuntimeCounters> counters;
};

class FontRuntime;

struct FontRuntimeCreateResult {
    std::unique_ptr<FontRuntime> runtime;
    FontError error{};

    [[nodiscard]] explicit operator bool() const noexcept {
        return runtime != nullptr;
    }
};

class FontRuntime final {
public:
    [[nodiscard]] static FontRuntimeCreateResult create(
        FontRuntimeOptions options = {});

    FontRuntime(const FontRuntime&) = delete;
    FontRuntime& operator=(const FontRuntime&) = delete;
    ~FontRuntime();

    [[nodiscard]] FontLoadResult load_font_file(
        const std::filesystem::path& path,
        long face_index,
        std::uint32_t pixel_size,
        FontFailurePoint failure_point = FontFailurePoint::none);

    [[nodiscard]] FontLoadResult load_font_file(
        const std::filesystem::path& path,
        long face_index,
        FontRasterConfig raster,
        FontFailurePoint failure_point = FontFailurePoint::none);

    [[nodiscard]] FontLoadResult load_font_bytes(
        std::span<const std::byte> bytes,
        long face_index,
        std::uint32_t pixel_size,
        FontFailurePoint failure_point = FontFailurePoint::none);

    [[nodiscard]] FontLoadResult load_font_bytes(
        std::span<const std::byte> bytes,
        long face_index,
        FontRasterConfig raster,
        FontFailurePoint failure_point = FontFailurePoint::none);

    [[nodiscard]] FontMetricsResult metrics(FontIdentity font) const;

    [[nodiscard]] GlyphLookupResult glyph_index(
        FontIdentity font,
        char32_t codepoint) const;

    [[nodiscard]] GlyphLookupResult find_glyph(
        std::span<const FontIdentity> fallback_chain,
        char32_t codepoint,
        std::optional<char32_t> replacement = U'\uFFFD') const;

    [[nodiscard]] GlyphRasterResult rasterize(
        FontIdentity font,
        std::uint32_t glyph_id,
        GlyphRasterMode mode = GlyphRasterMode::grayscale,
        FontFailurePoint failure_point = FontFailurePoint::none);

    [[nodiscard]] FontShapeResult shape_utf8_segment(
        FontIdentity font,
        std::string_view normalized_utf8,
        std::size_t byte_offset,
        std::size_t byte_length) const;

    [[nodiscard]] FontActionResult remove_font(FontIdentity font);
    [[nodiscard]] FontActionResult shutdown();

    [[nodiscard]] const FontRuntimeCounters& counters() const noexcept;
    [[nodiscard]] std::size_t glyph_cache_size() const noexcept;

private:
    struct Impl;

    explicit FontRuntime(std::unique_ptr<Impl> implementation) noexcept;

    std::unique_ptr<Impl> impl_;
};

[[nodiscard]] CoverageNormalizationResult normalize_gray_coverage(
    std::span<const std::uint8_t> source,
    std::uint32_t width,
    std::uint32_t height,
    int pitch);

} // namespace ryn::font
