#include "graphics/glyph_scene.hpp"

#include <algorithm>
#include <cmath>
#include <limits>
#include <optional>
#include <stdexcept>
#include <utility>

namespace ryn::graphics {
namespace {

void validate_finite(std::span<const float> values, const char* message) {
    if (!std::ranges::all_of(values, [](float value) { return std::isfinite(value); })) {
        throw std::invalid_argument(message);
    }
}

void validate_placement(const GlyphPlacement& placement) {
    const std::array values{
        placement.origin_pixels.x,
        placement.origin_pixels.y,
        placement.viewport_pixels.width,
        placement.viewport_pixels.height,
        placement.clip_pixels.x,
        placement.clip_pixels.y,
        placement.clip_pixels.width,
        placement.clip_pixels.height,
        placement.translation_pixels.x,
        placement.translation_pixels.y,
        placement.opacity,
    };
    validate_finite(values, "Glyph placement values must be finite");
    validate_finite(placement.color, "Glyph color values must be finite");
    if (placement.viewport_pixels.width <= 0.0F
            || placement.viewport_pixels.height <= 0.0F
            || placement.clip_pixels.width < 0.0F
            || placement.clip_pixels.height < 0.0F
            || placement.opacity < 0.0F
            || placement.opacity > 1.0F) {
        throw std::invalid_argument("Glyph placement dimensions or opacity are invalid");
    }
}

[[nodiscard]] std::array<float, 4> clip_bounds(
    runtime::Rect pixels,
    runtime::Size viewport) noexcept {
    return {
        -1.0F + 2.0F * pixels.x / viewport.width,
        1.0F - 2.0F * pixels.y / viewport.height,
        -1.0F + 2.0F * (pixels.x + pixels.width) / viewport.width,
        1.0F - 2.0F * (pixels.y + pixels.height) / viewport.height,
    };
}

[[nodiscard]] std::array<float, 2> normalized_translation(
    runtime::Point pixels,
    runtime::Size viewport) noexcept {
    return {
        2.0F * pixels.x / viewport.width,
        -2.0F * pixels.y / viewport.height,
    };
}

} // namespace

GlyphInstanceRange GlyphInstanceStore::append(
    std::span<const GlyphInstance> instances) {
    if (instances_.size() + instances.size() > std::numeric_limits<std::uint32_t>::max()) {
        throw std::length_error("GlyphInstanceStore exhausted instance indices");
    }
    const GlyphInstanceRange range{
        static_cast<std::uint32_t>(instances_.size()),
        static_cast<std::uint32_t>(instances.size()),
    };
    instances_.insert(instances_.end(), instances.begin(), instances.end());
    return range;
}

const GlyphInstance& GlyphInstanceStore::at(std::uint32_t index) const {
    return instances_.at(index);
}

GlyphInstance& GlyphInstanceStore::at(std::uint32_t index) {
    return instances_.at(index);
}

std::span<const GlyphInstance> GlyphInstanceStore::instances() const noexcept {
    return instances_;
}

std::size_t GlyphInstanceStore::size() const noexcept {
    return instances_.size();
}

std::span<const std::byte> GlyphInstanceStore::bytes(GlyphInstanceRange range) const {
    require_range(range);
    return std::as_bytes(std::span(instances_).subspan(range.first, range.count));
}

std::size_t GlyphInstanceStore::update_material(
    GlyphInstanceRange range,
    std::array<float, 4> color,
    float opacity) {
    require_range(range);
    validate_finite(color, "Glyph color values must be finite");
    if (!std::isfinite(opacity) || opacity < 0.0F || opacity > 1.0F) {
        throw std::invalid_argument("Glyph opacity must be finite and within [0, 1]");
    }

    std::size_t updated = 0;
    std::optional<std::uint32_t> dirty_start;
    for (std::uint32_t offset = 0; offset < range.count; ++offset) {
        const std::uint32_t index = range.first + offset;
        GlyphInstance& instance = instances_[index];
        if (instance.color == color && instance.translation_opacity[2] == opacity) {
            if (dirty_start) {
                mark_dirty(material_dirty_ranges_, {*dirty_start, index - *dirty_start});
                dirty_start.reset();
            }
            continue;
        }
        instance.color = color;
        instance.translation_opacity[2] = opacity;
        dirty_start = dirty_start.value_or(index);
        ++updated;
    }
    if (dirty_start) {
        mark_dirty(
            material_dirty_ranges_,
            {*dirty_start, range.first + range.count - *dirty_start});
    }
    return updated;
}

std::size_t GlyphInstanceStore::update_geometry(
    GlyphInstanceRange range,
    std::array<float, 4> clip,
    std::array<float, 2> translation) {
    require_range(range);
    validate_finite(clip, "Glyph clip bounds must be finite");
    validate_finite(translation, "Glyph translation must be finite");

    std::size_t updated = 0;
    std::optional<std::uint32_t> dirty_start;
    for (std::uint32_t offset = 0; offset < range.count; ++offset) {
        const std::uint32_t index = range.first + offset;
        GlyphInstance& instance = instances_[index];
        const std::array<float, 2> existing_translation{
            instance.translation_opacity[0],
            instance.translation_opacity[1],
        };
        if (instance.clip_bounds == clip && existing_translation == translation) {
            if (dirty_start) {
                mark_dirty(geometry_dirty_ranges_, {*dirty_start, index - *dirty_start});
                dirty_start.reset();
            }
            continue;
        }
        instance.clip_bounds = clip;
        instance.translation_opacity[0] = translation[0];
        instance.translation_opacity[1] = translation[1];
        dirty_start = dirty_start.value_or(index);
        ++updated;
    }
    if (dirty_start) {
        mark_dirty(
            geometry_dirty_ranges_,
            {*dirty_start, range.first + range.count - *dirty_start});
    }
    return updated;
}

std::span<const GlyphInstanceRange>
GlyphInstanceStore::material_dirty_ranges() const noexcept {
    return material_dirty_ranges_;
}

std::span<const GlyphInstanceRange>
GlyphInstanceStore::geometry_dirty_ranges() const noexcept {
    return geometry_dirty_ranges_;
}

void GlyphInstanceStore::clear_dirty_ranges() noexcept {
    material_dirty_ranges_.clear();
    geometry_dirty_ranges_.clear();
}

void GlyphInstanceStore::mark_dirty(
    std::vector<GlyphInstanceRange>& ranges,
    GlyphInstanceRange range) {
    if (range.count == 0) {
        return;
    }
    ranges.push_back(range);
    std::ranges::sort(ranges, {}, &GlyphInstanceRange::first);
    std::vector<GlyphInstanceRange> merged;
    merged.reserve(ranges.size());
    for (const GlyphInstanceRange candidate : ranges) {
        if (merged.empty()) {
            merged.push_back(candidate);
            continue;
        }
        GlyphInstanceRange& prior = merged.back();
        const std::uint64_t prior_end = static_cast<std::uint64_t>(prior.first) + prior.count;
        const std::uint64_t candidate_end =
            static_cast<std::uint64_t>(candidate.first) + candidate.count;
        if (candidate.first <= prior_end) {
            prior.count = static_cast<std::uint32_t>(
                std::max(prior_end, candidate_end) - prior.first);
        } else {
            merged.push_back(candidate);
        }
    }
    ranges = std::move(merged);
}

void GlyphInstanceStore::require_range(GlyphInstanceRange range) const {
    const std::uint64_t end = static_cast<std::uint64_t>(range.first) + range.count;
    if (end > instances_.size()) {
        throw std::out_of_range("Glyph instance range is out of bounds");
    }
}

GlyphSceneResult GlyphScene::append_text(
    font::FontRuntime& fonts,
    GlyphAtlas& atlas,
    const text::ShapedText& shaped,
    const text::TextMeasurement& measurement,
    GlyphPlacement placement) {
    validate_placement(placement);
    const auto clip = clip_bounds(placement.clip_pixels, placement.viewport_pixels);
    const auto translation = normalized_translation(
        placement.translation_pixels, placement.viewport_pixels);

    std::vector<GlyphInstance> pending_instances;
    std::vector<GlyphDrawRange> pending_ranges;
    for (const text::TextLine& line : measurement.lines) {
        const std::uint64_t line_end =
            static_cast<std::uint64_t>(line.glyph_begin) + line.glyph_count;
        if (line_end > shaped.glyphs.size()) {
            throw std::invalid_argument("TextMeasurement references glyphs outside ShapedText");
        }
        float pen_x = 0.0F;
        for (std::size_t glyph_index = line.glyph_begin;
                glyph_index < line_end; ++glyph_index) {
            const text::ShapedGlyph& glyph = shaped.glyphs[glyph_index];
            const GlyphAtlasResult atlas_result = atlas.ensure(
                fonts, glyph.font, glyph.glyph_id);
            if (!atlas_result) {
                return {{}, atlas_result.error};
            }
            const GlyphAtlasEntry& entry = *atlas_result.entry;
            if (!entry.empty) {
                const float left_pixels = placement.origin_pixels.x
                    + pen_x + glyph.offset_x + static_cast<float>(entry.bearing_x);
                const float top_pixels = placement.origin_pixels.y
                    + line.baseline - glyph.offset_y - static_cast<float>(entry.bearing_y);
                pending_instances.push_back({
                    {
                        -1.0F + 2.0F * left_pixels / placement.viewport_pixels.width,
                        1.0F - 2.0F * top_pixels / placement.viewport_pixels.height,
                        2.0F * entry.coverage_rect.width / placement.viewport_pixels.width,
                        -2.0F * entry.coverage_rect.height / placement.viewport_pixels.height,
                    },
                    {entry.uv.left, entry.uv.top, entry.uv.right, entry.uv.bottom},
                    clip,
                    placement.color,
                    {translation[0], translation[1], placement.opacity, 0.0F},
                });
                const std::uint32_t local_index =
                    static_cast<std::uint32_t>(pending_instances.size() - 1);
                if (!pending_ranges.empty()
                        && pending_ranges.back().atlas_page == entry.page
                        && pending_ranges.back().instances.first
                                + pending_ranges.back().instances.count == local_index) {
                    ++pending_ranges.back().instances.count;
                } else {
                    pending_ranges.push_back({entry.page, {local_index, 1}});
                }
            }
            pen_x += std::abs(glyph.advance_x);
        }
    }

    const GlyphInstanceRange inserted = instances_.append(pending_instances);
    for (GlyphDrawRange& range : pending_ranges) {
        range.instances.first += inserted.first;
    }
    return {{inserted, std::move(pending_ranges)}, {}};
}

GlyphInstanceStore& GlyphScene::instances() noexcept {
    return instances_;
}

const GlyphInstanceStore& GlyphScene::instances() const noexcept {
    return instances_;
}

void OrderedScene::append_quad(
    std::uint32_t first_instance,
    std::uint32_t instance_count) {
    append({
        SceneDrawKind::quad,
        first_instance,
        instance_count,
        invalid_glyph_atlas_page,
    });
}

void OrderedScene::append_glyph(GlyphDrawRange range) {
    append({
        SceneDrawKind::glyph,
        range.instances.first,
        range.instances.count,
        range.atlas_page,
    });
}

void OrderedScene::append_glyph(const GlyphPrimitive& primitive) {
    for (const GlyphDrawRange range : primitive.draw_ranges) {
        append_glyph(range);
    }
}

std::span<const SceneDrawCommand> OrderedScene::commands() const noexcept {
    return commands_;
}

void OrderedScene::clear() noexcept {
    commands_.clear();
}

void OrderedScene::append(SceneDrawCommand command) {
    if (command.instance_count == 0) {
        return;
    }
    if (!commands_.empty()) {
        SceneDrawCommand& previous = commands_.back();
        const bool compatible = previous.kind == command.kind
            && previous.atlas_page == command.atlas_page
            && static_cast<std::uint64_t>(previous.first_instance)
                    + previous.instance_count == command.first_instance;
        if (compatible) {
            const std::uint64_t merged =
                static_cast<std::uint64_t>(previous.instance_count)
                + command.instance_count;
            if (merged > std::numeric_limits<std::uint32_t>::max()) {
                throw std::length_error("Ordered Scene command range exceeds uint32_t");
            }
            previous.instance_count = static_cast<std::uint32_t>(merged);
            return;
        }
    }
    commands_.push_back(command);
}

} // namespace ryn::graphics
