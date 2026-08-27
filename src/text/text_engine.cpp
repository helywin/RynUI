#include "text/text_engine.hpp"

#include <algorithm>
#include <cmath>
#include <limits>
#include <optional>
#include <stdexcept>
#include <utility>

namespace ryn::text {
namespace {

[[nodiscard]] std::uint8_t byte_at(StringView text, std::size_t offset) noexcept {
    return static_cast<std::uint8_t>(text.utf8()[offset]);
}

[[nodiscard]] bool is_cjk(char32_t value) noexcept {
    return (value >= 0x3400 && value <= 0x4DBF)
        || (value >= 0x4E00 && value <= 0x9FFF)
        || (value >= 0xF900 && value <= 0xFAFF)
        || (value >= 0x20000 && value <= 0x2FA1F);
}

[[nodiscard]] bool is_rtl_strong(char32_t value) noexcept {
    return (value >= 0x0590 && value <= 0x08FF)
        || (value >= 0xFB1D && value <= 0xFDFF)
        || (value >= 0xFE70 && value <= 0xFEFF);
}

[[nodiscard]] bool is_ltr_strong(char32_t value) noexcept {
    return (value >= U'A' && value <= U'Z')
        || (value >= U'a' && value <= U'z')
        || is_cjk(value);
}

[[nodiscard]] TextError font_error(font::FontError error) {
    const TextErrorKind kind = error.stage == font::FontErrorStage::shaping
        ? TextErrorKind::shaping_failure
        : TextErrorKind::font_failure;
    return {kind, 0, std::move(error)};
}

[[nodiscard]] std::u8string encode_utf8(char32_t value) {
    std::u8string result;
    if (value <= 0x7F) {
        result.push_back(static_cast<char8_t>(value));
    } else if (value <= 0x7FF) {
        result.push_back(static_cast<char8_t>(0xC0 | (value >> 6U)));
        result.push_back(static_cast<char8_t>(0x80 | (value & 0x3FU)));
    } else if (value <= 0xFFFF) {
        result.push_back(static_cast<char8_t>(0xE0 | (value >> 12U)));
        result.push_back(static_cast<char8_t>(0x80 | ((value >> 6U) & 0x3FU)));
        result.push_back(static_cast<char8_t>(0x80 | (value & 0x3FU)));
    } else {
        result.push_back(static_cast<char8_t>(0xF0 | (value >> 18U)));
        result.push_back(static_cast<char8_t>(0x80 | ((value >> 12U) & 0x3FU)));
        result.push_back(static_cast<char8_t>(0x80 | ((value >> 6U) & 0x3FU)));
        result.push_back(static_cast<char8_t>(0x80 | (value & 0x3FU)));
    }
    return result;
}

[[nodiscard]] std::string_view bytes(std::u8string_view value) noexcept {
    return {
        reinterpret_cast<const char*>(value.data()),
        value.size(),
    };
}

[[nodiscard]] const Utf8Scalar* scalar_at_cluster(
    const ShapedText& text,
    std::size_t cluster) noexcept {
    const auto found = std::lower_bound(
        text.scalars.begin(), text.scalars.end(), cluster,
        [](const Utf8Scalar& scalar, std::size_t offset) {
            return scalar.byte_start < offset;
        });
    if (found != text.scalars.end() && found->byte_start == cluster) {
        return &*found;
    }
    return nullptr;
}

struct ClusterUnit {
    std::size_t glyph_begin{};
    std::size_t glyph_end{};
    std::size_t byte_start{};
    std::size_t byte_end{};
    float advance{};
    bool legal_break_after{};
};

[[nodiscard]] std::vector<ClusterUnit> make_cluster_units(
    const ShapedText& text,
    const ShapedParagraph& paragraph) {
    std::vector<ClusterUnit> units;
    const std::size_t glyph_end = paragraph.glyph_begin + paragraph.glyph_count;
    std::vector<std::size_t> logical_clusters;
    logical_clusters.reserve(paragraph.glyph_count);
    for (std::size_t glyph = paragraph.glyph_begin; glyph < glyph_end; ++glyph) {
        logical_clusters.push_back(text.glyphs[glyph].cluster);
    }
    std::ranges::sort(logical_clusters);
    const auto unique_end = std::ranges::unique(logical_clusters).begin();
    logical_clusters.erase(unique_end, logical_clusters.end());

    std::size_t glyph = paragraph.glyph_begin;
    while (glyph < glyph_end) {
        const std::size_t begin = glyph;
        const std::size_t cluster = text.glyphs[glyph].cluster;
        float advance = 0.0F;
        while (glyph < glyph_end && text.glyphs[glyph].cluster == cluster) {
            advance += std::abs(text.glyphs[glyph].advance_x);
            ++glyph;
        }
        const auto next_cluster = std::upper_bound(
            logical_clusters.begin(), logical_clusters.end(), cluster);
        const std::size_t byte_end = next_cluster == logical_clusters.end()
            ? paragraph.byte_end
            : *next_cluster;
        const Utf8Scalar* scalar = scalar_at_cluster(text, cluster);
        units.push_back({
            begin,
            glyph,
            cluster,
            byte_end,
            advance,
            scalar != nullptr && (scalar->whitespace || scalar->cjk),
        });
    }
    return units;
}

} // namespace

std::vector<Utf8Scalar> decode_utf8(StringView text) {
    std::vector<Utf8Scalar> result;
    result.reserve(text.size_bytes());
    std::size_t offset = 0;
    while (offset < text.size_bytes()) {
        const std::size_t start = offset;
        const std::uint8_t lead = byte_at(text, offset++);
        char32_t value = 0;
        std::size_t continuation_count = 0;
        if (lead <= 0x7F) {
            value = lead;
        } else if (lead <= 0xDF) {
            value = lead & 0x1FU;
            continuation_count = 1;
        } else if (lead <= 0xEF) {
            value = lead & 0x0FU;
            continuation_count = 2;
        } else {
            value = lead & 0x07U;
            continuation_count = 3;
        }
        for (std::size_t index = 0; index < continuation_count; ++index) {
            value = (value << 6U) | (byte_at(text, offset++) & 0x3FU);
        }
        result.push_back({
            value,
            start,
            offset,
            value == U' ' || value == U'\t',
            value == U'\n',
            is_cjk(value),
        });
    }
    return result;
}

TextEngine::TextEngine(font::FontRuntime& fonts) noexcept : fonts_(&fonts) {}

TextShapeResult TextEngine::shape(
    StringView text,
    std::span<const font::FontIdentity> fallback_chain) const {
    if (fallback_chain.empty()) {
        return {{}, {TextErrorKind::empty_font_chain, 0, {}}};
    }

    const auto default_metrics = fonts_->metrics(fallback_chain.front());
    if (!default_metrics) {
        return {{}, font_error(default_metrics.error)};
    }

    ShapedText output;
    output.scalars = decode_utf8(text);
    output.default_metrics = default_metrics.metrics;
    output.normalized_size_bytes = text.size_bytes();

    bool paragraph_has_ltr = false;
    bool paragraph_has_rtl = false;
    for (const Utf8Scalar& scalar : output.scalars) {
        if (scalar.newline) {
            paragraph_has_ltr = false;
            paragraph_has_rtl = false;
            continue;
        }
        paragraph_has_ltr = paragraph_has_ltr || is_ltr_strong(scalar.value);
        paragraph_has_rtl = paragraph_has_rtl || is_rtl_strong(scalar.value);
        if (paragraph_has_ltr && paragraph_has_rtl) {
            return {
                {},
                {TextErrorKind::mixed_direction_unsupported, scalar.byte_start, {}},
            };
        }
    }

    std::optional<font::FontIdentity> run_font;
    std::size_t run_start = 0;
    std::size_t run_end = 0;
    std::size_t paragraph_start = 0;
    std::size_t paragraph_glyph_start = 0;

    const auto append_shaped = [&](
        font::FontIdentity selected_font,
        std::string_view shaping_text,
        std::size_t shaping_offset,
        std::size_t shaping_length,
        std::size_t output_byte_start,
        std::size_t output_byte_end,
        bool remap_clusters,
        ShapedText& shaped) -> TextError {
        const auto font_shape = fonts_->shape_utf8_segment(
            selected_font, shaping_text, shaping_offset, shaping_length);
        if (!font_shape) {
            return font_error(font_shape.error);
        }
        const std::size_t glyph_begin = shaped.glyphs.size();
        for (const font::FontShapedGlyph& glyph : font_shape.glyphs) {
            const std::size_t cluster = remap_clusters
                ? output_byte_start
                : glyph.cluster;
            if (cluster < output_byte_start || cluster >= output_byte_end) {
                return {
                    TextErrorKind::shaping_failure,
                    cluster,
                    {},
                };
            }
            shaped.glyphs.push_back({
                selected_font,
                glyph.glyph_id,
                cluster,
                glyph.advance_x,
                glyph.advance_y,
                glyph.offset_x,
                glyph.offset_y,
                glyph.extent_x_bearing,
                glyph.extent_y_bearing,
                glyph.extent_width,
                glyph.extent_height,
            });
        }
        shaped.runs.push_back({
            selected_font,
            output_byte_start,
            output_byte_end,
            glyph_begin,
            shaped.glyphs.size() - glyph_begin,
            font_shape.right_to_left,
        });
        return {};
    };

    const auto flush_run = [&](ShapedText& shaped) -> TextError {
        if (!run_font) {
            return {};
        }
        const TextError error = append_shaped(
            *run_font,
            text.bytes(),
            run_start,
            run_end - run_start,
            run_start,
            run_end,
            false,
            shaped);
        run_font.reset();
        return error;
    };

    for (const Utf8Scalar& scalar : output.scalars) {
        if (scalar.newline) {
            if (TextError error = flush_run(output)) {
                return {{}, std::move(error)};
            }
            output.paragraphs.push_back({
                paragraph_start,
                scalar.byte_start,
                paragraph_glyph_start,
                output.glyphs.size() - paragraph_glyph_start,
            });
            paragraph_start = scalar.byte_end;
            paragraph_glyph_start = output.glyphs.size();
            continue;
        }

        const auto selection = fonts_->find_glyph(fallback_chain, scalar.value);
        if (!selection) {
            TextError error = font_error(selection.error);
            error.byte_offset = scalar.byte_start;
            return {{}, std::move(error)};
        }
        if (selection.glyph.used_replacement) {
            if (TextError error = flush_run(output)) {
                return {{}, std::move(error)};
            }
            const std::u8string replacement = encode_utf8(
                selection.glyph.resolved_codepoint);
            if (TextError error = append_shaped(
                    selection.glyph.font,
                    bytes(replacement),
                    0,
                    replacement.size(),
                    scalar.byte_start,
                    scalar.byte_end,
                    true,
                    output)) {
                return {{}, std::move(error)};
            }
            continue;
        }

        if (!run_font || *run_font != selection.glyph.font) {
            if (TextError error = flush_run(output)) {
                return {{}, std::move(error)};
            }
            run_font = selection.glyph.font;
            run_start = scalar.byte_start;
        }
        run_end = scalar.byte_end;
    }
    if (TextError error = flush_run(output)) {
        return {{}, std::move(error)};
    }
    output.paragraphs.push_back({
        paragraph_start,
        text.size_bytes(),
        paragraph_glyph_start,
        output.glyphs.size() - paragraph_glyph_start,
    });
    return {std::move(output), {}};
}

TextShapeResult TextEngine::shape_utf8_lossy(
    std::string_view raw_bytes,
    std::span<const font::FontIdentity> fallback_chain) const {
    Utf8RepairResult repaired = String::from_utf8_lossy(raw_bytes);
    TextShapeResult result = shape(repaired.value.view(), fallback_chain);
    result.text.replacement_count = repaired.replacement_count;
    result.text.normalized_size_bytes = repaired.value.size_bytes();
    return result;
}

TextMeasureResult TextEngine::measure(
    const ShapedText& text,
    TextLayoutConfig config) const {
    if (!std::isfinite(config.line_height) || config.line_height <= 0.0F) {
        return {{}, {TextErrorKind::invalid_line_height, 0, {}}};
    }
    if (std::isnan(config.max_width) || config.max_width < 0.0F) {
        return {{}, {TextErrorKind::invalid_width_constraint, 0, {}}};
    }

    TextMeasurement measurement;
    bool has_visible_bounds = false;
    float line_top = 0.0F;

    const auto emit_line = [&](
        const ShapedParagraph& paragraph,
        const std::vector<ClusterUnit>& units,
        std::size_t unit_begin,
        std::size_t unit_end,
        bool overflow,
        TextMeasurement& result,
        float& top,
        bool& has_bounds) -> TextError {
        const std::size_t glyph_begin = unit_begin < unit_end
            ? units[unit_begin].glyph_begin
            : paragraph.glyph_begin;
        const std::size_t glyph_end = unit_begin < unit_end
            ? units[unit_end - 1].glyph_end
            : paragraph.glyph_begin;
        std::size_t byte_start = paragraph.byte_start;
        std::size_t byte_end = paragraph.byte_end;
        if (unit_begin < unit_end) {
            byte_start = units[unit_begin].byte_start;
            byte_end = units[unit_begin].byte_end;
            for (std::size_t unit = unit_begin + 1; unit < unit_end; ++unit) {
                byte_start = std::min(byte_start, units[unit].byte_start);
                byte_end = std::max(byte_end, units[unit].byte_end);
            }
        }

        float ascent = text.default_metrics.ascent;
        float descent = -text.default_metrics.descent;
        float width = 0.0F;
        for (std::size_t unit = unit_begin; unit < unit_end; ++unit) {
            width += units[unit].advance;
        }
        for (std::size_t glyph = glyph_begin; glyph < glyph_end; ++glyph) {
            const auto metrics = fonts_->metrics(text.glyphs[glyph].font);
            if (!metrics) {
                return font_error(metrics.error);
            }
            ascent = std::max(ascent, metrics.metrics.ascent);
            descent = std::max(descent, -metrics.metrics.descent);
        }
        const float content_height = ascent + descent;
        const float baseline = top
            + std::max(0.0F, (config.line_height - content_height) * 0.5F)
            + ascent;

        float pen_x = 0.0F;
        for (std::size_t glyph = glyph_begin; glyph < glyph_end; ++glyph) {
            const ShapedGlyph& shaped = text.glyphs[glyph];
            if (shaped.extent_width != 0.0F && shaped.extent_height != 0.0F) {
                const float x1 = pen_x + shaped.offset_x + shaped.extent_x_bearing;
                const float x2 = x1 + shaped.extent_width;
                const float y1 = baseline - shaped.offset_y - shaped.extent_y_bearing;
                const float y2 = y1 - shaped.extent_height;
                const float left = std::min(x1, x2);
                const float right = std::max(x1, x2);
                const float visible_top = std::min(y1, y2);
                const float visible_bottom = std::max(y1, y2);
                if (!has_bounds) {
                    result.content_bounds = {left, visible_top, right, visible_bottom};
                    has_bounds = true;
                } else {
                    result.content_bounds.left = std::min(result.content_bounds.left, left);
                    result.content_bounds.top = std::min(result.content_bounds.top, visible_top);
                    result.content_bounds.right = std::max(result.content_bounds.right, right);
                    result.content_bounds.bottom = std::max(result.content_bounds.bottom, visible_bottom);
                }
            }
            pen_x += std::abs(shaped.advance_x);
        }

        result.lines.push_back({
            glyph_begin,
            glyph_end - glyph_begin,
            byte_start,
            byte_end,
            width,
            baseline,
            overflow,
        });
        result.width = std::max(result.width, width);
        result.overflow = result.overflow || overflow;
        top += config.line_height;
        return {};
    };

    for (const ShapedParagraph& paragraph : text.paragraphs) {
        const std::vector<ClusterUnit> units = make_cluster_units(text, paragraph);
        if (units.empty()) {
            if (TextError error = emit_line(
                    paragraph, units, 0, 0, false,
                    measurement, line_top, has_visible_bounds)) {
                return {{}, std::move(error)};
            }
            continue;
        }

        if (std::isinf(config.max_width)) {
            if (TextError error = emit_line(
                    paragraph, units, 0, units.size(), false,
                    measurement, line_top, has_visible_bounds)) {
                return {{}, std::move(error)};
            }
            continue;
        }

        std::size_t line_begin = 0;
        while (line_begin < units.size()) {
            float width = 0.0F;
            std::optional<std::size_t> last_legal_break;
            std::size_t unit = line_begin;
            while (unit < units.size()) {
                const float next_width = width + units[unit].advance;
                if (next_width > config.max_width) {
                    break;
                }
                width = next_width;
                ++unit;
                if (units[unit - 1].legal_break_after) {
                    last_legal_break = unit;
                }
            }

            if (unit == units.size()) {
                if (TextError error = emit_line(
                        paragraph, units, line_begin, unit, false,
                        measurement, line_top, has_visible_bounds)) {
                    return {{}, std::move(error)};
                }
                break;
            }
            if (unit == line_begin) {
                if (TextError error = emit_line(
                        paragraph, units, line_begin, line_begin + 1, true,
                        measurement, line_top, has_visible_bounds)) {
                    return {{}, std::move(error)};
                }
                ++line_begin;
                continue;
            }

            const std::size_t line_end = last_legal_break.value_or(unit);
            if (TextError error = emit_line(
                    paragraph, units, line_begin, line_end, false,
                    measurement, line_top, has_visible_bounds)) {
                return {{}, std::move(error)};
            }
            line_begin = line_end;
        }
    }

    measurement.height = line_top;
    if (!measurement.lines.empty()) {
        measurement.first_baseline = measurement.lines.front().baseline;
    }
    return {std::move(measurement), {}};
}

TextState::TextState(
    TextEngine& engine,
    String content,
    std::vector<font::FontIdentity> fallback_chain,
    std::uint32_t pixel_size,
    TextLayoutConfig layout,
    std::function<void()> request_frame)
    : engine_(&engine),
      content_(std::move(content)),
      fallback_chain_(std::move(fallback_chain)),
      pixel_size_(pixel_size),
      layout_(layout),
      request_frame_callback_(std::move(request_frame)) {}

bool TextState::set_content(String content) {
    if (content_ == content) {
        return false;
    }
    content_ = std::move(content);
    invalidate_shape();
    return true;
}

bool TextState::set_font_chain(std::vector<font::FontIdentity> fallback_chain) {
    if (fallback_chain_ == fallback_chain) {
        return false;
    }
    fallback_chain_ = std::move(fallback_chain);
    invalidate_shape();
    return true;
}

bool TextState::set_pixel_size(std::uint32_t pixel_size) {
    if (pixel_size_ == pixel_size) {
        return false;
    }
    pixel_size_ = pixel_size;
    invalidate_shape();
    return true;
}

bool TextState::set_line_height(float line_height) {
    if (layout_.line_height == line_height) {
        return false;
    }
    layout_.line_height = line_height;
    invalidate_layout();
    return true;
}

bool TextState::set_width_constraint(float max_width, bool request_frame) {
    if (layout_.max_width == max_width) {
        return false;
    }
    layout_.max_width = max_width;
    invalidate_layout(request_frame);
    return true;
}

bool TextState::set_color(std::array<float, 4> color) {
    if (material_.color == color) {
        return false;
    }
    material_.color = color;
    material_dirty_ = true;
    request_frame();
    return true;
}

bool TextState::set_opacity(float opacity) {
    if (!std::isfinite(opacity) || opacity < 0.0F || opacity > 1.0F) {
        throw std::invalid_argument("Text opacity must be finite and within [0, 1].");
    }
    if (material_.opacity == opacity) {
        return false;
    }
    material_.opacity = opacity;
    material_dirty_ = true;
    request_frame();
    return true;
}

bool TextState::synchronize() {
    last_error_ = {};
    if (shape_dirty_) {
        ++counters_.shape_count;
        TextShapeResult result = engine_->shape(content_.view(), fallback_chain_);
        if (!result) {
            last_error_ = std::move(result.error);
            return false;
        }
        shaped_ = std::move(result.text);
        shape_dirty_ = false;
        layout_dirty_ = true;
    }
    if (layout_dirty_) {
        ++counters_.measure_count;
        TextMeasureResult result = engine_->measure(shaped_, layout_);
        if (!result) {
            last_error_ = std::move(result.error);
            return false;
        }
        measurement_ = std::move(result.measurement);
        ++counters_.layout_count;
        layout_dirty_ = false;
    }
    if (material_dirty_) {
        ++counters_.material_range_updates;
        material_dirty_ = false;
    }
    return true;
}

const ShapedText& TextState::shaped() const noexcept {
    return shaped_;
}

const TextMeasurement& TextState::measurement() const noexcept {
    return measurement_;
}

const TextMaterial& TextState::material() const noexcept {
    return material_;
}

const TextStateCounters& TextState::counters() const noexcept {
    return counters_;
}

const TextError& TextState::last_error() const noexcept {
    return last_error_;
}

void TextState::invalidate_shape() {
    shape_dirty_ = true;
    layout_dirty_ = true;
    request_frame();
}

void TextState::invalidate_layout(bool request_frame) {
    layout_dirty_ = true;
    if (request_frame) {
        this->request_frame();
    }
}

void TextState::request_frame() {
    if (request_frame_callback_) {
        request_frame_callback_();
    }
}

} // namespace ryn::text
