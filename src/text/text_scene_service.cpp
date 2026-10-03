#include "text/text_scene_service.hpp"

#include "icons/ant_design_icon_font.inc"
#include "icons/icon_vector_data.hpp"
#include "text/text_caret_map.hpp"

#include <algorithm>
#include <cmath>
#include <stdexcept>
#include <utility>

namespace ryn::detail {
font::FontMetricsResult TextSceneService::font_metrics(font::FontIdentity font) const {
    ensure_owner_thread();
    return fonts_->metrics(font);
}

namespace {

[[nodiscard]] bool same_position_geometry(const graphics::GlyphPlacement& left,
                                          const graphics::GlyphPlacement& right) noexcept {
    return left.origin_pixels == right.origin_pixels && left.viewport_pixels == right.viewport_pixels &&
           left.translation_pixels == right.translation_pixels;
}

[[nodiscard]] bool same_patchable_geometry(const graphics::GlyphPlacement& left,
                                           const graphics::GlyphPlacement& right) noexcept {
    return left.clip_pixels == right.clip_pixels;
}

} // namespace

struct TextSceneService::Record final {
    runtime::NodeId node;
    std::size_t declaration_order{};
    std::shared_ptr<text::TextState> state;
    bool view{};
    text::TextMaterial view_material;
    std::uint64_t observed_text_revision{};
    graphics::GlyphPrimitive primitive;
    std::vector<runtime::Rect> line_clips;
    std::vector<TextCoverageClip> coverage_clips;
    bool coverage_enabled{};
    std::optional<graphics::GlyphPlacement> placement;
    runtime::Point scroll_translation{};
    graphics::GlyphTransform transform;
    std::uint64_t scroll_scale_revision{};
    float scroll_scale{};
    graphics::GlyphAtlasError last_error{};
    TextSceneRevisions revisions;
    TextSceneRecordCounters counters;
    bool content_dirty{true};
    bool position_geometry_dirty{};
    bool patchable_geometry_dirty{};
    bool material_dirty{};
    bool placement_rebuild_pending{};
    float content_opacity{1};
};

struct TextSceneService::Slot final {
    std::unique_ptr<Record> record;
    std::uint32_t generation{1};
};

TextSceneService::TextSceneService(font::FontRuntime& fonts, text::TextEngine& engine,
                                   runtime::FrameRequestState& frame_requests) noexcept
    : fonts_(&fonts), engine_(&engine), frame_requests_(&frame_requests), owner_thread_(std::this_thread::get_id()) {}

TextSceneService::~TextSceneService() {
    // FontRuntime outlives this service; raster/cache resources belong to the
    // window text service, including custom sources no longer mounted.
    for (const auto& cached : vector_fonts_) {
        static_cast<void>(fonts_->remove_font(cached.font));
    }
    for (const auto& cached : icon_fonts_) {
        static_cast<void>(fonts_->remove_font(cached.font));
    }
}

font::FontIdentity TextSceneService::icon_font(const IconSource& source, font::FontIdentity reference,
                                               std::uint32_t logical_pixel_size) {
    ensure_owner_thread();
    if (source.bundled_name()) {
        static_cast<void>(bundled_icon_entry(*source.bundled_name()));
        return icon_font(reference, logical_pixel_size);
    }
    const auto& vector = IconSourceAccess::vector(source);
    if (!vector || vector->font_bytes.empty() || logical_pixel_size == 0) {
        throw std::invalid_argument("Icon vector source or pixel size is invalid");
    }
    const auto metrics = fonts_->metrics(reference);
    if (!metrics) {
        throw std::runtime_error("Icon vector reference font is invalid: " + metrics.error.diagnostic);
    }
    const auto scale = metrics.metrics.display_scale;
    for (const auto& cached : vector_fonts_) {
        if (cached.source == vector && cached.logical_pixel_size == logical_pixel_size &&
            cached.display_scale == scale) {
            return cached.font;
        }
    }
    font::FontRasterConfig raster{logical_pixel_size, scale};
    raster.policy.hinting = false;
    raster.policy.embedded_bitmap = false;
    const auto result = fonts_->load_font_bytes(vector->font_bytes, 0, raster);
    if (!result) {
        throw std::runtime_error("Cannot load typed Icon vector: " + result.error.diagnostic);
    }
    try {
        vector_fonts_.push_back({vector, logical_pixel_size, scale, result.font});
    } catch (...) {
        static_cast<void>(fonts_->remove_font(result.font));
        throw;
    }
    return result.font;
}

font::FontIdentity TextSceneService::icon_font(font::FontIdentity reference, std::uint32_t logical_pixel_size) {
    ensure_owner_thread();
    if (logical_pixel_size == 0) {
        throw std::invalid_argument("Icon pixel size must be positive");
    }
    const auto reference_metrics = fonts_->metrics(reference);
    if (!reference_metrics) {
        throw std::runtime_error("Icon reference font is invalid: " + reference_metrics.error.diagnostic);
    }
    const float display_scale = reference_metrics.metrics.display_scale;
    for (const auto& cached : icon_fonts_) {
        if (cached.logical_pixel_size == logical_pixel_size && cached.display_scale == display_scale) {
            return cached.font;
        }
    }
    font::FontRasterConfig raster;
    raster.logical_pixel_size = logical_pixel_size;
    raster.display_scale = display_scale;
    raster.policy.hinting = false;
    raster.policy.embedded_bitmap = false;
    const auto* first = reinterpret_cast<const std::byte*>(ant_design_icon_font_bytes);
    const auto result = fonts_->load_font_bytes({first, sizeof(ant_design_icon_font_bytes)}, 0, raster);
    if (!result) {
        throw std::runtime_error("Cannot load embedded Ant Design icons: " + result.error.diagnostic);
    }
    try {
        icon_fonts_.push_back({logical_pixel_size, display_scale, result.font});
    } catch (...) {
        static_cast<void>(fonts_->remove_font(result.font));
        throw;
    }
    return result.font;
}

TextSceneId TextSceneService::create(runtime::NodeId node, String content,
                                     std::vector<font::FontIdentity> fallback_chain, std::uint32_t pixel_size,
                                     text::TextLayoutConfig layout) {
    ensure_owner_thread();
    if (!node.valid()) {
        throw std::invalid_argument("Text scene record requires a valid NodeId");
    }

    const std::uint32_t index = acquire_slot();
    const TextSceneId id{index, slots_[index].generation};
    try {
        auto record = std::make_unique<Record>();
        record->node = node;
        record->declaration_order = next_declaration_order_++;
        record->state =
            std::make_shared<text::TextState>(*engine_, std::move(content), std::move(fallback_chain), pixel_size,
                                              layout, [this] { frame_requests_->request_frame(); });
        record->primitive.instances = {
            static_cast<std::uint32_t>(glyph_scene_.instances().size()),
            0,
        };
        slots_[index].record = std::move(record);
        ordered_ids_.push_back(id);
        paint_ids_.push_back(id);
        ++live_records_;
        ++counters_.creates;
        frame_requests_->request_frame();
        return id;
    } catch (...) {
        std::erase(ordered_ids_, id);
        std::erase(paint_ids_, id);
        slots_[index].record.reset();
        try {
            free_slots_.push_back(index);
        } catch (...) {
        }
        throw;
    }
}

TextSceneId TextSceneService::create_view(TextSceneId source, runtime::NodeId node) {
    ensure_owner_thread();
    if (!node.valid()) {
        throw std::invalid_argument("Text view requires a valid node");
    }
    const auto& original = require_record(source);
    auto state = original.state;
    const auto material = original.view ? original.view_material : original.state->material();
    const auto index = acquire_slot();
    const TextSceneId id{index, slots_[index].generation};
    try {
        auto record = std::make_unique<Record>();
        record->node = node;
        record->declaration_order = next_declaration_order_++;
        record->state = std::move(state);
        record->view = true;
        record->view_material = material;
        record->primitive.instances = {static_cast<std::uint32_t>(glyph_scene_.instances().size()), 0};
        slots_[index].record = std::move(record);
        ordered_ids_.push_back(id);
        paint_ids_.push_back(id);
        ++live_records_;
        ++counters_.creates;
        frame_requests_->request_frame();
        return id;
    } catch (...) {
        std::erase(ordered_ids_, id);
        std::erase(paint_ids_, id);
        slots_[index].record.reset();
        try {
            free_slots_.push_back(index);
        } catch (...) {
        }
        throw;
    }
}

bool TextSceneService::place_after(TextSceneId id, TextSceneId previous) {
    ensure_owner_thread();
    static_cast<void>(require_record(id));
    static_cast<void>(require_record(previous));
    if (id == previous) {
        throw std::invalid_argument("A glyph layer cannot follow itself");
    }
    const auto position = std::ranges::find(paint_ids_, id);
    const auto anchor = std::ranges::find(paint_ids_, previous);
    if (anchor + 1 == position) {
        return false;
    }
    std::erase(paint_ids_, id);
    paint_ids_.insert(std::ranges::find(paint_ids_, previous) + 1, id);
    invalidate_ordered_scene();
    frame_requests_->request_frame();
    return true;
}

bool TextSceneService::destroy(TextSceneId id) {
    ensure_owner_thread();
    auto* record = find_record(id);
    if (record == nullptr) {
        return false;
    }
    const auto range = record->primitive.instances;
    static_cast<void>(glyph_scene_.instances().replace(range, {}));
    remap_following(id, -static_cast<std::int64_t>(range.count));
    std::erase(ordered_ids_, id);
    std::erase(paint_ids_, id);
    release_slot(id);
    ++counters_.destroys;
    if (range.count != 0) {
        ++counters_.range_compactions;
    }
    invalidate_ordered_scene();
    frame_requests_->request_frame();
    return true;
}

bool TextSceneService::set_content(TextSceneId id, String content) {
    ensure_owner_thread();
    auto& record = require_record(id);
    if (record.view) {
        throw std::logic_error("Text views cannot replace shared content");
    }
    if (!record.state->set_content(std::move(content))) {
        return false;
    }
    ++record.revisions.content;
    record.content_dirty = true;
    return true;
}

bool TextSceneService::set_font_chain(TextSceneId id, std::vector<font::FontIdentity> fallback_chain) {
    ensure_owner_thread();
    auto& record = require_record(id);
    if (record.view) {
        throw std::logic_error("Text views cannot replace shared fonts");
    }
    if (!record.state->set_font_chain(std::move(fallback_chain))) {
        return false;
    }
    ++record.revisions.content;
    record.content_dirty = true;
    return true;
}

bool TextSceneService::set_direction(TextSceneId id, TextDirection direction) {
    ensure_owner_thread();
    auto& record = require_record(id);
    if (record.view) {
        throw std::logic_error("Text views cannot replace shared direction");
    }
    if (!record.state->set_direction(direction)) {
        return false;
    }
    ++record.revisions.content;
    record.content_dirty = true;
    return true;
}

bool TextSceneService::set_pixel_size(TextSceneId id, std::uint32_t pixel_size) {
    ensure_owner_thread();
    auto& record = require_record(id);
    if (record.view) {
        throw std::logic_error("Text views cannot replace shared font size");
    }
    if (!record.state->set_pixel_size(pixel_size)) {
        return false;
    }
    ++record.revisions.content;
    record.content_dirty = true;
    return true;
}

bool TextSceneService::set_line_height(TextSceneId id, float line_height) {
    ensure_owner_thread();
    auto& record = require_record(id);
    if (record.view) {
        throw std::logic_error("Text views cannot replace shared line height");
    }
    if (!record.state->set_line_height(line_height)) {
        return false;
    }
    ++record.revisions.layout;
    record.placement_rebuild_pending = true;
    return true;
}

bool TextSceneService::set_ellipsis(TextSceneId id, text::TextEllipsisConfig config, bool request_frame) {
    ensure_owner_thread();
    auto& record = require_record(id);
    if (record.view) {
        throw std::logic_error("Text views cannot replace ellipsis configuration");
    }
    if (!record.state->set_ellipsis(std::move(config), request_frame)) {
        return false;
    }
    ++record.revisions.layout;
    record.content_dirty = true;
    record.placement_rebuild_pending = true;
    return true;
}

void TextSceneService::request_reshape(TextSceneId id) {
    ensure_owner_thread();
    auto& record = require_record(id);
    if (record.view) {
        throw std::logic_error("Text views cannot reshape shared content");
    }
    record.state->request_reshape();
    ++record.revisions.content;
    record.content_dirty = true;
}

bool TextSceneService::set_width_constraint(TextSceneId id, float width) {
    ensure_owner_thread();
    auto& record = require_record(id);
    if (record.view) {
        throw std::logic_error("Text views cannot replace shared width");
    }
    if (!record.state->set_width_constraint(width)) {
        return false;
    }
    ++record.revisions.layout;
    record.placement_rebuild_pending = true;
    return true;
}

bool TextSceneService::set_color(TextSceneId id, std::array<float, 4> color) {
    ensure_owner_thread();
    auto& record = require_record(id);
    if (record.view) {
        if (record.view_material.color == color) {
            return false;
        }
        record.view_material.color = color;
        frame_requests_->request_frame();
    } else if (!record.state->set_color(color)) {
        return false;
    }
    ++record.revisions.tone;
    record.material_dirty = true;
    return true;
}

bool TextSceneService::set_opacity(TextSceneId id, float opacity) {
    ensure_owner_thread();
    auto& record = require_record(id);
    if (record.view) {
        if (!std::isfinite(opacity) || opacity < 0 || opacity > 1) {
            throw std::invalid_argument("Text opacity must be within [0, 1]");
        }
        if (record.view_material.opacity == opacity) {
            return false;
        }
        record.view_material.opacity = opacity;
        frame_requests_->request_frame();
    } else if (!record.state->set_opacity(opacity)) {
        return false;
    }
    ++record.revisions.tone;
    record.material_dirty = true;
    return true;
}

bool TextSceneService::set_content_opacity(TextSceneId id, float opacity) {
    ensure_owner_thread();
    if (!std::isfinite(opacity) || opacity < 0 || opacity > 1) {
        throw std::invalid_argument("Content opacity must be within [0, 1]");
    }
    auto& record = require_record(id);
    if (record.content_opacity == opacity) {
        return false;
    }
    record.content_opacity = opacity;
    ++record.revisions.tone;
    record.material_dirty = true;
    frame_requests_->request_frame();
    return true;
}

bool TextSceneService::set_placement(TextSceneId id, graphics::GlyphPlacement placement) {
    return update_placement(id, std::move(placement), true);
}

bool TextSceneService::set_transform(TextSceneId id, graphics::GlyphTransform transform) {
    ensure_owner_thread();
    auto& record = require_record(id);
    if (!std::isfinite(transform.pivot.x) || !std::isfinite(transform.pivot.y) ||
        !std::isfinite(transform.angle_degrees)) {
        throw std::invalid_argument("Text glyph transform must be finite");
    }
    if (record.transform == transform) {
        return false;
    }
    record.transform = transform;
    record.patchable_geometry_dirty = true;
    ++record.revisions.placement;
    frame_requests_->request_frame();
    return true;
}

bool TextSceneService::set_scroll_translation(TextSceneId id, runtime::Point pixels) {
    ensure_owner_thread();
    auto& record = require_record(id);
    if (!std::isfinite(pixels.x) || !std::isfinite(pixels.y)) {
        throw std::invalid_argument("Text scroll translation must be finite");
    }
    if (record.scroll_translation == pixels) {
        return false;
    }
    record.scroll_translation = pixels;
    record.patchable_geometry_dirty = true;
    ++record.revisions.placement;
    frame_requests_->request_frame();
    return true;
}

runtime::Point TextSceneService::set_phase_preserving_scroll_translation(TextSceneId id, runtime::Point pixels,
                                                                         runtime::Point aligned_offset) {
    ensure_owner_thread();
    if (!std::isfinite(pixels.x) || !std::isfinite(pixels.y) || !std::isfinite(aligned_offset.x) ||
        !std::isfinite(aligned_offset.y)) {
        throw std::invalid_argument("Text translation must be finite");
    }
    auto& record = require_record(id);
    if (record.scroll_scale_revision != record.state->revision()) {
        record.scroll_scale = 0.0F;
        const auto& shaped = record.state->shaped();
        const float scale = shaped.default_metrics.display_scale;
        if (std::isfinite(scale) && scale > 0.0F) {
            record.scroll_scale = scale;
            for (const auto& run : shaped.runs) {
                const auto metrics = fonts_->metrics(run.font);
                if (!metrics || std::abs(metrics.metrics.display_scale - scale) > 0.0001F) {
                    record.scroll_scale = 0.0F;
                    break;
                }
            }
            record.scroll_scale_revision = record.state->revision();
        }
    }
    if (record.scroll_scale <= 0.0F) {
        static_cast<void>(set_scroll_translation(id, aligned_offset));
        return pixels;
    }
    const runtime::Point snapped{
        std::round(pixels.x * record.scroll_scale) / record.scroll_scale,
        std::round(pixels.y * record.scroll_scale) / record.scroll_scale,
    };
    static_cast<void>(set_scroll_translation(id, {
                                                     snapped.x + aligned_offset.x,
                                                     snapped.y + aligned_offset.y,
                                                 }));
    return {pixels.x - snapped.x, pixels.y - snapped.y};
}

std::size_t TextSceneService::patch_geometry(Record& record, const graphics::GlyphPlacement& placement) {
    const auto clip = placement.clip_pixels;
    std::size_t updated{};
    const auto patch = [&](graphics::GlyphInstanceRange range, runtime::Rect coverage) {
        const auto left = std::max(clip.x, coverage.x);
        const auto top = std::max(clip.y, coverage.y);
        const auto right = std::max(left, std::min(clip.x + clip.width, coverage.x + coverage.width));
        const auto bottom = std::max(top, std::min(clip.y + clip.height, coverage.y + coverage.height));
        updated += glyph_scene_.instances().update_geometry(range, {left, top, right, bottom},
                                                            {record.scroll_translation.x, record.scroll_translation.y});
    };
    if (record.coverage_enabled) {
        for (std::size_t index = 0; index < record.primitive.coverage.size(); ++index) {
            const auto& owner = record.primitive.coverage[index];
            runtime::Rect covered{};
            bool found{};
            for (const auto& candidate : record.coverage_clips) {
                const auto owner_x =
                    placement.origin_pixels.x + placement.translation_pixels.x + record.scroll_translation.x + owner.x;
                if (candidate.line != owner.line || candidate.byte_begin >= owner.byte_end ||
                    candidate.byte_end <= owner.byte_begin ||
                    candidate.rect.x + candidate.rect.width <= owner_x + 0.0001F ||
                    candidate.rect.x >= owner_x + owner.width - 0.0001F) {
                    continue;
                }
                if (!found) {
                    covered = candidate.rect;
                    found = true;
                } else {
                    const auto right = std::max(covered.x + covered.width, candidate.rect.x + candidate.rect.width);
                    const auto bottom = std::max(covered.y + covered.height, candidate.rect.y + candidate.rect.height);
                    covered.x = std::min(covered.x, candidate.rect.x);
                    covered.y = std::min(covered.y, candidate.rect.y);
                    covered.width = right - covered.x;
                    covered.height = bottom - covered.y;
                }
            }
            patch({record.primitive.instances.first + static_cast<std::uint32_t>(index), 1}, covered);
        }
    } else if (record.line_clips.empty()) {
        patch(record.primitive.instances, clip);
    } else {
        for (std::size_t line = 0; line < record.primitive.line_ranges.size(); ++line) {
            patch(record.primitive.line_ranges[line],
                  line < record.line_clips.size() ? record.line_clips[line] : runtime::Rect{});
        }
    }
    auto transform = record.transform;
    if (transform != graphics::GlyphTransform{}) {
        transform.pivot.x += placement.origin_pixels.x + placement.translation_pixels.x;
        transform.pivot.y += placement.origin_pixels.y + placement.translation_pixels.y;
    }
    return updated + glyph_scene_.instances().update_transform(record.primitive.instances, transform);
}

bool TextSceneService::set_line_clips(TextSceneId id, std::span<const runtime::Rect> clips) {
    ensure_owner_thread();
    auto& record = require_record(id);
    for (const auto clip : clips) {
        if (!std::isfinite(clip.x) || !std::isfinite(clip.y) || !std::isfinite(clip.width) ||
            !std::isfinite(clip.height) || clip.width < 0 || clip.height < 0) {
            throw std::invalid_argument("Text line coverage must be finite and nonnegative");
        }
    }
    if (std::ranges::equal(record.line_clips, clips)) {
        return false;
    }
    record.line_clips.assign(clips.begin(), clips.end());
    record.patchable_geometry_dirty = true;
    ++record.revisions.placement;
    frame_requests_->request_frame();
    return true;
}

bool TextSceneService::set_coverage_clips(TextSceneId id, std::span<const TextCoverageClip> clips) {
    ensure_owner_thread();
    auto& record = require_record(id);
    for (const auto& coverage : clips) {
        const auto rect = coverage.rect;
        if (coverage.byte_end < coverage.byte_begin || !std::isfinite(rect.x) || !std::isfinite(rect.y) ||
            !std::isfinite(rect.width) || !std::isfinite(rect.height) || rect.width < 0 || rect.height < 0) {
            throw std::invalid_argument("Text coverage must have valid bytes and finite nonnegative dimensions");
        }
    }
    if (record.coverage_enabled && std::ranges::equal(record.coverage_clips, clips)) {
        return false;
    }
    record.coverage_clips.assign(clips.begin(), clips.end());
    record.coverage_enabled = true;
    record.patchable_geometry_dirty = true;
    ++record.revisions.placement;
    frame_requests_->request_frame();
    return true;
}

bool TextSceneService::update_placement(TextSceneId id, graphics::GlyphPlacement placement, bool request_frame) {
    ensure_owner_thread();
    auto& record = require_record(id);
    const bool had_placement = record.placement.has_value();
    const bool position_changed = !had_placement || !same_position_geometry(*record.placement, placement);
    const bool patchable_changed = !had_placement || !same_patchable_geometry(*record.placement, placement);
    const bool layout_requires_geometry = record.placement_rebuild_pending;
    if (!position_changed && !patchable_changed && !layout_requires_geometry) {
        return false;
    }

    if (position_changed || layout_requires_geometry) {
        record.position_geometry_dirty = true;
    } else if (patchable_changed) {
        record.patchable_geometry_dirty = true;
    }
    record.placement_rebuild_pending = false;
    record.placement = std::move(placement);
    if (position_changed || patchable_changed) {
        ++record.revisions.placement;
    }
    if (request_frame) {
        frame_requests_->request_frame();
    }
    return true;
}

bool TextSceneService::synchronize_culled(TextSceneId id) {
    ensure_owner_thread();
    auto& record = require_record(id);
    if (!record.placement || record.primitive.instances.count == 0) {
        return false;
    }
    if (record.placement->clip_pixels != runtime::Rect{}) {
        record.placement->clip_pixels = {};
        ++record.revisions.placement;
    }
    const auto updated = glyph_scene_.instances().update_geometry(
        record.primitive.instances, {}, {record.scroll_translation.x, record.scroll_translation.y});
    if (updated != 0) {
        ++record.counters.geometry_updates;
        ++record.counters.geometry_patches;
    }
    return updated != 0;
}

bool TextSceneService::synchronize(TextSceneId id) {
    ensure_owner_thread();
    auto& record = require_record(id);
    if (!record.placement.has_value()) {
        throw std::logic_error("Text scene record must have placement before synchronization");
    }
    ++record.counters.synchronizations;
    if (!synchronize_measurement(id)) {
        return false;
    }

    auto placement = *record.placement;
    if (record.view && record.observed_text_revision != record.state->revision()) {
        record.observed_text_revision = record.state->revision();
        record.content_dirty = true;
        ++record.revisions.content;
    }
    const auto material = record.view ? record.view_material : record.state->material();
    placement.color = material.color;
    placement.opacity = material.opacity * record.content_opacity;
    const auto old_range = record.primitive.instances;
    if (record.content_dirty || record.position_geometry_dirty) {
        const bool text_rebuild = record.content_dirty;
        auto result = glyph_scene_.replace_text(old_range, *fonts_, atlas_, record.state->shaped(),
                                                record.state->measurement(), placement);
        if (!result) {
            record.last_error = std::move(result.error);
            return false;
        }
        const std::int64_t offset = static_cast<std::int64_t>(result.primitive.instances.count) - old_range.count;
        record.primitive = std::move(result.primitive);
        remap_following(id, offset);
        if (record.scroll_translation != runtime::Point{} || record.transform != graphics::GlyphTransform{} ||
            !record.line_clips.empty() || record.coverage_enabled) {
            static_cast<void>(patch_geometry(record, placement));
        }
        record.content_dirty = false;
        record.position_geometry_dirty = false;
        record.patchable_geometry_dirty = false;
        record.material_dirty = false;
        record.placement = placement;
        if (text_rebuild) {
            ++record.counters.instance_rebuilds;
        } else {
            ++record.counters.geometry_updates;
            ++record.counters.geometry_rebuilds;
        }
        if (offset != 0 && old_range.count != 0) {
            ++counters_.range_compactions;
        }
        invalidate_ordered_scene();
        return true;
    }

    if (record.material_dirty) {
        const auto updated =
            glyph_scene_.instances().update_material(record.primitive.instances, material.color, placement.opacity);
        if (updated != 0) {
            ++record.counters.material_updates;
        }
        record.material_dirty = false;
    }
    if (record.patchable_geometry_dirty) {
        const auto updated = patch_geometry(record, placement);
        if (updated != 0) {
            ++record.counters.geometry_updates;
            ++record.counters.geometry_patches;
        }
        record.patchable_geometry_dirty = false;
    }
    record.placement = placement;
    if (!ordered_scene_batch_active_ && ordered_scene_pending_) {
        rebuild_ordered_scene();
    }
    return true;
}

void TextSceneService::begin_ordered_scene_batch() {
    ensure_owner_thread();
    if (ordered_scene_batch_active_) {
        throw std::logic_error("Text ordered scene batch is already active");
    }
    ordered_scene_batch_active_ = true;
}

void TextSceneService::finish_ordered_scene_batch() {
    ensure_owner_thread();
    if (!ordered_scene_batch_active_) {
        throw std::logic_error("Text ordered scene batch is not active");
    }
    if (ordered_scene_pending_) {
        rebuild_ordered_scene();
    }
    ordered_scene_batch_active_ = false;
}

void TextSceneService::cancel_ordered_scene_batch() noexcept {
    ordered_scene_batch_active_ = false;
}

bool TextSceneService::synchronize_measurement(TextSceneId id) {
    ensure_owner_thread();
    auto& record = require_record(id);
    ++record.counters.measurement_synchronizations;
    if (record.last_error) {
        record.last_error = {};
    }
    return record.state->synchronize();
}

bool TextSceneService::synchronize_caret_map(TextSceneId id, text::TextCaretMap& output) {
    ensure_owner_thread();
    auto& state = *require_record(id).state;
    if (!state.synchronize()) {
        return false;
    }
    return engine_->map_carets(state.shaped(), state.content(), state.revision(), state.measurement().first_baseline,
                               output);
}

bool TextSceneService::synchronize_line_caret_map(TextSceneId id, text::TextCaretMap& output) {
    ensure_owner_thread();
    auto& state = *require_record(id).state;
    if (!state.synchronize()) {
        return false;
    }
    return engine_->map_carets(state.shaped(), state.content(), state.revision(), state.measurement(), output);
}

bool TextSceneService::synchronize_measurement(TextSceneId id, float width_constraint) {
    ensure_owner_thread();
    auto& record = require_record(id);
    if (record.view) {
        throw std::logic_error("Text views cannot replace shared width");
    }
    if (record.state->set_width_constraint(width_constraint, false)) {
        ++record.revisions.layout;
        record.placement_rebuild_pending = true;
    }
    return synchronize_measurement(id);
}

bool TextSceneService::synchronize(TextSceneId id, graphics::GlyphPlacement placement) {
    static_cast<void>(update_placement(id, std::move(placement), false));
    return synchronize(id);
}

bool TextSceneService::synchronize_all() {
    ensure_owner_thread();
    for (const auto id : ordered_ids_) {
        if (!synchronize(id)) {
            return false;
        }
    }
    return true;
}

bool TextSceneService::contains(TextSceneId id) const noexcept {
    return find_record(id) != nullptr;
}

std::size_t TextSceneService::size() const noexcept {
    return live_records_;
}

runtime::NodeId TextSceneService::node(TextSceneId id) const {
    ensure_owner_thread();
    return require_record(id).node;
}

std::size_t TextSceneService::declaration_order(TextSceneId id) const {
    ensure_owner_thread();
    return require_record(id).declaration_order;
}

text::TextState& TextSceneService::text_state(TextSceneId id) {
    ensure_owner_thread();
    if (require_record(id).view) {
        throw std::logic_error("Text views expose only const shared state");
    }
    return *require_record(id).state;
}

const text::TextState& TextSceneService::text_state(TextSceneId id) const {
    ensure_owner_thread();
    return *require_record(id).state;
}

const graphics::GlyphPrimitive& TextSceneService::primitive(TextSceneId id) const {
    ensure_owner_thread();
    return require_record(id).primitive;
}

const TextSceneRevisions& TextSceneService::revisions(TextSceneId id) const {
    ensure_owner_thread();
    return require_record(id).revisions;
}

const TextSceneRecordCounters& TextSceneService::record_counters(TextSceneId id) const {
    ensure_owner_thread();
    return require_record(id).counters;
}

const graphics::GlyphAtlasError& TextSceneService::last_error(TextSceneId id) const {
    ensure_owner_thread();
    return require_record(id).last_error;
}

graphics::GlyphAtlas& TextSceneService::atlas() noexcept {
    return atlas_;
}

const graphics::GlyphAtlas& TextSceneService::atlas() const noexcept {
    return atlas_;
}

graphics::GlyphScene& TextSceneService::glyph_scene() noexcept {
    return glyph_scene_;
}

const graphics::GlyphScene& TextSceneService::glyph_scene() const noexcept {
    return glyph_scene_;
}

const graphics::OrderedScene& TextSceneService::ordered_scene() const noexcept {
    return ordered_scene_;
}

const TextSceneServiceCounters& TextSceneService::counters() const noexcept {
    return counters_;
}

TextSceneService::Record* TextSceneService::find_record(TextSceneId id) noexcept {
    if (!id.valid() || id.index >= slots_.size()) {
        return nullptr;
    }
    auto& slot = slots_[id.index];
    if (slot.generation != id.generation) {
        return nullptr;
    }
    return slot.record.get();
}

const TextSceneService::Record* TextSceneService::find_record(TextSceneId id) const noexcept {
    if (!id.valid() || id.index >= slots_.size()) {
        return nullptr;
    }
    const auto& slot = slots_[id.index];
    if (slot.generation != id.generation) {
        return nullptr;
    }
    return slot.record.get();
}

TextSceneService::Record& TextSceneService::require_record(TextSceneId id) {
    if (auto* record = find_record(id)) {
        return *record;
    }
    throw std::out_of_range("TextSceneId is stale or invalid");
}

const TextSceneService::Record& TextSceneService::require_record(TextSceneId id) const {
    if (const auto* record = find_record(id)) {
        return *record;
    }
    throw std::out_of_range("TextSceneId is stale or invalid");
}

std::uint32_t TextSceneService::acquire_slot() {
    if (!free_slots_.empty()) {
        const auto index = free_slots_.back();
        free_slots_.pop_back();
        return index;
    }
    if (slots_.size() >= TextSceneId::invalid_index) {
        throw std::length_error("TextSceneService exhausted TextSceneId indices");
    }
    slots_.emplace_back();
    return static_cast<std::uint32_t>(slots_.size() - 1);
}

void TextSceneService::release_slot(TextSceneId id) noexcept {
    auto& slot = slots_[id.index];
    slot.record.reset();
    advance_generation(slot);
    try {
        free_slots_.push_back(id.index);
    } catch (...) {
    }
    --live_records_;
}

void TextSceneService::ensure_owner_thread() const {
    if (std::this_thread::get_id() != owner_thread_) {
        throw std::logic_error("TextSceneService can only be used on its owner thread");
    }
}

void TextSceneService::remap_following(TextSceneId id, std::int64_t offset) {
    if (offset == 0) {
        return;
    }
    const auto target = std::ranges::find(ordered_ids_, id);
    if (target == ordered_ids_.end()) {
        throw std::logic_error("Text scene declaration order is inconsistent");
    }
    for (auto iterator = target + 1; iterator != ordered_ids_.end(); ++iterator) {
        shift_primitive(require_record(*iterator).primitive, offset);
    }
}

void TextSceneService::invalidate_ordered_scene() {
    ordered_scene_pending_ = true;
    if (!ordered_scene_batch_active_) {
        rebuild_ordered_scene();
    }
}

void TextSceneService::rebuild_ordered_scene() {
    ordered_scene_.clear();
    for (const auto id : paint_ids_) {
        ordered_scene_.append_glyph(require_record(id).primitive);
    }
    ++counters_.ordered_scene_rebuilds;
    ordered_scene_pending_ = false;
}

void TextSceneService::shift_primitive(graphics::GlyphPrimitive& primitive, std::int64_t offset) {
    const auto shift = [offset](std::uint32_t first) {
        const std::int64_t shifted = static_cast<std::int64_t>(first) + offset;
        if (shifted < 0 || shifted > std::numeric_limits<std::uint32_t>::max()) {
            throw std::logic_error("Text scene range remap exceeded uint32_t");
        }
        return static_cast<std::uint32_t>(shifted);
    };
    primitive.instances.first = shift(primitive.instances.first);
    for (auto& range : primitive.draw_ranges) {
        range.instances.first = shift(range.instances.first);
    }
    for (auto& range : primitive.line_ranges) {
        range.first = shift(range.first);
    }
}

void TextSceneService::advance_generation(Slot& slot) noexcept {
    ++slot.generation;
    if (slot.generation == 0) {
        slot.generation = 1;
    }
}

} // namespace ryn::detail
