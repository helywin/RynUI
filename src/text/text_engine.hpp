#pragma once

#include "font/font_runtime.hpp"

#include <ryn/string.hpp>

#include <array>
#include <cstddef>
#include <cstdint>
#include <functional>
#include <limits>
#include <optional>
#include <span>
#include <string_view>
#include <vector>

namespace ryn::text {

struct Utf8Scalar {
    char32_t value{};
    std::size_t byte_start{};
    std::size_t byte_end{};
    bool whitespace{};
    bool newline{};
    bool cjk{};

    friend bool operator==(const Utf8Scalar&, const Utf8Scalar&) = default;
};

[[nodiscard]] std::vector<Utf8Scalar> decode_utf8(StringView text);

enum class TextErrorKind : std::uint8_t {
    none,
    empty_font_chain,
    font_failure,
    shaping_failure,
    mixed_direction_unsupported,
    invalid_line_height,
    invalid_width_constraint,
};

struct TextError {
    TextErrorKind kind{TextErrorKind::none};
    std::size_t byte_offset{};
    font::FontError font_error{};

    [[nodiscard]] explicit operator bool() const noexcept {
        return kind != TextErrorKind::none;
    }
};

struct ShapedGlyph {
    font::FontIdentity font{};
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

    friend bool operator==(const ShapedGlyph&, const ShapedGlyph&) = default;
};

struct GlyphRun {
    font::FontIdentity font{};
    std::size_t byte_start{};
    std::size_t byte_end{};
    std::size_t glyph_begin{};
    std::size_t glyph_count{};
    bool right_to_left{};

    friend bool operator==(const GlyphRun&, const GlyphRun&) = default;
};

struct ShapedParagraph {
    std::size_t byte_start{};
    std::size_t byte_end{};
    std::size_t glyph_begin{};
    std::size_t glyph_count{};

    friend bool operator==(const ShapedParagraph&, const ShapedParagraph&) = default;
};

struct ShapedText {
    std::vector<Utf8Scalar> scalars;
    std::vector<ShapedGlyph> glyphs;
    std::vector<GlyphRun> runs;
    std::vector<ShapedParagraph> paragraphs;
    font::FontMetrics default_metrics{};
    std::size_t normalized_size_bytes{};
    std::size_t replacement_count{};

    friend bool operator==(const ShapedText&, const ShapedText&) = default;
};

struct TextShapeResult {
    ShapedText text;
    TextError error{};

    [[nodiscard]] explicit operator bool() const noexcept {
        return !error;
    }
};

struct TextLayoutConfig {
    float line_height{20.0F};
    float max_width{std::numeric_limits<float>::infinity()};

    friend bool operator==(const TextLayoutConfig&, const TextLayoutConfig&) = default;
};

struct TextLine {
    std::size_t glyph_begin{};
    std::size_t glyph_count{};
    std::size_t byte_start{};
    std::size_t byte_end{};
    float width{};
    float baseline{};
    bool overflow{};

    friend bool operator==(const TextLine&, const TextLine&) = default;
};

struct TextBounds {
    float left{};
    float top{};
    float right{};
    float bottom{};

    friend bool operator==(const TextBounds&, const TextBounds&) = default;
};

struct TextMeasurement {
    std::vector<TextLine> lines;
    TextBounds content_bounds{};
    float width{};
    float height{};
    float first_baseline{};
    bool overflow{};

    friend bool operator==(const TextMeasurement&, const TextMeasurement&) = default;
};

struct TextMeasureResult {
    TextMeasurement measurement;
    TextError error{};

    [[nodiscard]] explicit operator bool() const noexcept {
        return !error;
    }
};

class TextEngine final {
public:
    explicit TextEngine(font::FontRuntime& fonts) noexcept;

    [[nodiscard]] TextShapeResult shape(
        StringView text,
        std::span<const font::FontIdentity> fallback_chain) const;

    [[nodiscard]] TextShapeResult shape_utf8_lossy(
        std::string_view bytes,
        std::span<const font::FontIdentity> fallback_chain) const;

    [[nodiscard]] TextMeasureResult measure(
        const ShapedText& text,
        TextLayoutConfig config) const;

private:
    font::FontRuntime* fonts_;
};

struct TextMaterial {
    std::array<float, 4> color{0.0F, 0.0F, 0.0F, 1.0F};
    float opacity{1.0F};

    friend bool operator==(const TextMaterial&, const TextMaterial&) = default;
};

struct TextStateCounters {
    std::size_t shape_count{};
    std::size_t measure_count{};
    std::size_t layout_count{};
    std::size_t material_range_updates{};
};

class TextState final {
public:
    TextState(
        TextEngine& engine,
        String content,
        std::vector<font::FontIdentity> fallback_chain,
        std::uint32_t pixel_size,
        TextLayoutConfig layout,
        std::function<void()> request_frame = {});

    bool set_content(String content);
    bool set_font_chain(std::vector<font::FontIdentity> fallback_chain);
    bool set_pixel_size(std::uint32_t pixel_size);
    bool set_line_height(float line_height);
    bool set_width_constraint(float max_width, bool request_frame = true);
    bool set_color(std::array<float, 4> color);
    bool set_opacity(float opacity);

    [[nodiscard]] bool synchronize();
    [[nodiscard]] const ShapedText& shaped() const noexcept;
    [[nodiscard]] const TextMeasurement& measurement() const noexcept;
    [[nodiscard]] const TextMaterial& material() const noexcept;
    [[nodiscard]] const TextStateCounters& counters() const noexcept;
    [[nodiscard]] const TextError& last_error() const noexcept;

private:
    void invalidate_shape();
    void invalidate_layout(bool request_frame = true);
    void request_frame();

    TextEngine* engine_;
    String content_;
    std::vector<font::FontIdentity> fallback_chain_;
    std::uint32_t pixel_size_{};
    TextLayoutConfig layout_;
    TextMaterial material_;
    std::function<void()> request_frame_callback_;
    ShapedText shaped_;
    TextMeasurement measurement_;
    TextStateCounters counters_;
    TextError last_error_;
    bool shape_dirty_{true};
    bool layout_dirty_{true};
    bool material_dirty_{};
};

} // namespace ryn::text
