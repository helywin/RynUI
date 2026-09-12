#include "text/text_scene_service.hpp"
#include "text/text_caret_map.hpp"

#include <algorithm>
#include <cmath>
#include <stdexcept>
#include <utility>

namespace ryn::detail {
namespace {

[[nodiscard]] bool same_position_geometry(
    const graphics::GlyphPlacement& left,
    const graphics::GlyphPlacement& right) noexcept {
    return left.origin_pixels == right.origin_pixels
        && left.viewport_pixels == right.viewport_pixels
        && left.translation_pixels == right.translation_pixels;
}

[[nodiscard]] bool same_patchable_geometry(
    const graphics::GlyphPlacement& left,
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
    std::optional<graphics::GlyphPlacement> placement;
    runtime::Point scroll_translation{};
    graphics::GlyphAtlasError last_error{};
    TextSceneRevisions revisions;
    TextSceneRecordCounters counters;
    bool content_dirty{true};
    bool position_geometry_dirty{};
    bool patchable_geometry_dirty{};
    bool material_dirty{};
    bool placement_rebuild_pending{};
};

struct TextSceneService::Slot final {
    std::unique_ptr<Record> record;
    std::uint32_t generation{1};
};

TextSceneService::TextSceneService(
    font::FontRuntime& fonts,
    text::TextEngine& engine,
    runtime::FrameRequestState& frame_requests) noexcept
    : fonts_(&fonts),
      engine_(&engine),
      frame_requests_(&frame_requests),
      owner_thread_(std::this_thread::get_id()) {}

TextSceneService::~TextSceneService() = default;

TextSceneId TextSceneService::create(
    runtime::NodeId node,
    String content,
    std::vector<font::FontIdentity> fallback_chain,
    std::uint32_t pixel_size,
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
        record->state = std::make_shared<text::TextState>(
            *engine_,
            std::move(content),
            std::move(fallback_chain),
            pixel_size,
            layout,
            [this] { frame_requests_->request_frame(); });
        record->primitive.instances = {
            static_cast<std::uint32_t>(glyph_scene_.instances().size()),
            0,
        };
        slots_[index].record = std::move(record);
        ordered_ids_.push_back(id);
        ++live_records_;
        ++counters_.creates;
        frame_requests_->request_frame();
        return id;
    } catch (...) {
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
    if(!node.valid()) throw std::invalid_argument("Text view requires a valid node");
    const auto& original = require_record(source);
    auto state = original.state;
    const auto material = original.view ? original.view_material : original.state->material();
    const auto index = acquire_slot();
    const TextSceneId id{index, slots_[index].generation};
    try {
        auto record = std::make_unique<Record>();
        record->node = node; record->declaration_order = next_declaration_order_++;
        record->state = std::move(state); record->view = true; record->view_material = material;
        record->primitive.instances = {static_cast<std::uint32_t>(glyph_scene_.instances().size()), 0};
        slots_[index].record = std::move(record); ordered_ids_.push_back(id);
        ++live_records_; ++counters_.creates; frame_requests_->request_frame();
        return id;
    } catch(...) {
        slots_[index].record.reset();
        try { free_slots_.push_back(index); } catch(...) {}
        throw;
    }
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
    release_slot(id);
    ++counters_.destroys;
    if (range.count != 0) {
        ++counters_.range_compactions;
    }
    rebuild_ordered_scene();
    frame_requests_->request_frame();
    return true;
}

bool TextSceneService::set_content(TextSceneId id, String content) {
    ensure_owner_thread();
    auto& record = require_record(id);
    if(record.view) throw std::logic_error("Text views cannot replace shared content");
    if (!record.state->set_content(std::move(content))) {
        return false;
    }
    ++record.revisions.content;
    record.content_dirty = true;
    return true;
}

bool TextSceneService::set_font_chain(
    TextSceneId id,
    std::vector<font::FontIdentity> fallback_chain) {
    ensure_owner_thread();
    auto& record = require_record(id);
    if(record.view) throw std::logic_error("Text views cannot replace shared fonts");
    if (!record.state->set_font_chain(std::move(fallback_chain))) {
        return false;
    }
    ++record.revisions.content;
    record.content_dirty = true;
    return true;
}

bool TextSceneService::set_pixel_size(
    TextSceneId id,
    std::uint32_t pixel_size) {
    ensure_owner_thread();
    auto& record = require_record(id);
    if(record.view) throw std::logic_error("Text views cannot replace shared font size");
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
    if(record.view) throw std::logic_error("Text views cannot replace shared line height");
    if (!record.state->set_line_height(line_height)) {
        return false;
    }
    ++record.revisions.layout;
    record.placement_rebuild_pending = true;
    return true;
}

void TextSceneService::request_reshape(TextSceneId id) {
    ensure_owner_thread();
    auto& record = require_record(id);
    if(record.view) throw std::logic_error("Text views cannot reshape shared content");
    record.state->request_reshape();
    ++record.revisions.content;
    record.content_dirty = true;
}

bool TextSceneService::set_width_constraint(TextSceneId id, float width) {
    ensure_owner_thread();
    auto& record = require_record(id);
    if(record.view) throw std::logic_error("Text views cannot replace shared width");
    if (!record.state->set_width_constraint(width)) {
        return false;
    }
    ++record.revisions.layout;
    record.placement_rebuild_pending = true;
    return true;
}

bool TextSceneService::set_color(
    TextSceneId id,
    std::array<float, 4> color) {
    ensure_owner_thread();
    auto& record = require_record(id);
    if(record.view) {
        if(record.view_material.color == color) return false;
        record.view_material.color = color; frame_requests_->request_frame();
    } else if(!record.state->set_color(color)) return false;
    ++record.revisions.tone;
    record.material_dirty = true;
    return true;
}

bool TextSceneService::set_opacity(TextSceneId id, float opacity) {
    ensure_owner_thread();
    auto& record = require_record(id);
    if(record.view) {
        if(!std::isfinite(opacity) || opacity < 0 || opacity > 1) throw std::invalid_argument("Text opacity must be within [0, 1]");
        if(record.view_material.opacity == opacity) return false;
        record.view_material.opacity = opacity; frame_requests_->request_frame();
    } else if(!record.state->set_opacity(opacity)) return false;
    ++record.revisions.tone;
    record.material_dirty = true;
    return true;
}

bool TextSceneService::set_placement(
    TextSceneId id,
    graphics::GlyphPlacement placement) {
    return update_placement(id, std::move(placement), true);
}

bool TextSceneService::set_scroll_translation(TextSceneId id, runtime::Point pixels) {
    ensure_owner_thread();
    auto& record = require_record(id);
    if (!std::isfinite(pixels.x) || !std::isfinite(pixels.y)) {
        throw std::invalid_argument("Text scroll translation must be finite");
    }
    if (record.scroll_translation == pixels) return false;
    record.scroll_translation = pixels;
    record.patchable_geometry_dirty = true;
    ++record.revisions.placement;
    frame_requests_->request_frame();
    return true;
}

std::size_t TextSceneService::patch_geometry(
    Record& record, const graphics::GlyphPlacement& placement) {
    const auto clip = placement.clip_pixels;
    const auto viewport = placement.viewport_pixels;
    return glyph_scene_.instances().update_geometry(record.primitive.instances, {
        -1.0F + 2.0F * clip.x / viewport.width,
        1.0F - 2.0F * clip.y / viewport.height,
        -1.0F + 2.0F * (clip.x + clip.width) / viewport.width,
        1.0F - 2.0F * (clip.y + clip.height) / viewport.height,
    }, {
        2.0F * record.scroll_translation.x / viewport.width,
        -2.0F * record.scroll_translation.y / viewport.height,
    });
}

bool TextSceneService::update_placement(
    TextSceneId id,
    graphics::GlyphPlacement placement,
    bool request_frame) {
    ensure_owner_thread();
    auto& record = require_record(id);
    const bool had_placement = record.placement.has_value();
    const bool position_changed = !had_placement
        || !same_position_geometry(*record.placement, placement);
    const bool patchable_changed = !had_placement
        || !same_patchable_geometry(*record.placement, placement);
    const bool layout_requires_geometry = record.placement_rebuild_pending;
    if (!position_changed && !patchable_changed && !layout_requires_geometry) {
        return false;
    }

    if (position_changed || layout_requires_geometry) {
        record.position_geometry_dirty = true;
    } else if (patchable_changed) {
        record.patchable_geometry_dirty =
            true;
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
    if(record.view && record.observed_text_revision != record.state->revision()) {
        record.observed_text_revision = record.state->revision();
        record.content_dirty = true;
        ++record.revisions.content;
    }
    const auto material = record.view ? record.view_material : record.state->material();
    placement.color = material.color;
    placement.opacity = material.opacity;
    const auto old_range = record.primitive.instances;
    if (record.content_dirty || record.position_geometry_dirty) {
        const bool text_rebuild = record.content_dirty;
        auto result = glyph_scene_.replace_text(
            old_range,
            *fonts_,
            atlas_,
            record.state->shaped(),
            record.state->measurement(),
            placement);
        if (!result) {
            record.last_error = std::move(result.error);
            return false;
        }
        const std::int64_t offset =
            static_cast<std::int64_t>(result.primitive.instances.count)
            - old_range.count;
        record.primitive = std::move(result.primitive);
        remap_following(id, offset);
        if (record.scroll_translation != runtime::Point{}) {
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
        }
        if (offset != 0 && old_range.count != 0) {
            ++counters_.range_compactions;
        }
        rebuild_ordered_scene();
        return true;
    }

    if (record.material_dirty) {
        const auto updated = glyph_scene_.instances().update_material(
            record.primitive.instances,
            material.color,
            material.opacity);
        if (updated != 0) {
            ++record.counters.material_updates;
        }
        record.material_dirty = false;
    }
    if (record.patchable_geometry_dirty) {
        const auto updated = patch_geometry(record, placement);
        if (updated != 0) {
            ++record.counters.geometry_updates;
        }
        record.patchable_geometry_dirty = false;
    }
    record.placement = placement;
    return true;
}

bool TextSceneService::synchronize_measurement(TextSceneId id) {
    ensure_owner_thread();
    auto& record = require_record(id);
    ++record.counters.measurement_synchronizations;
    record.last_error = {};
    return record.state->synchronize();
}

bool TextSceneService::synchronize_caret_map(TextSceneId id, text::TextCaretMap& output) {
    ensure_owner_thread();
    auto& state = *require_record(id).state;
    if(!state.synchronize()) return false;
    return engine_->map_carets(state.shaped(), state.content(), state.revision(),
        state.measurement().first_baseline, output);
}

bool TextSceneService::synchronize_measurement(
    TextSceneId id,
    float width_constraint) {
    ensure_owner_thread();
    auto& record = require_record(id);
    if(record.view) throw std::logic_error("Text views cannot replace shared width");
    if (record.state->set_width_constraint(width_constraint, false)) {
        ++record.revisions.layout;
        record.placement_rebuild_pending = true;
    }
    return synchronize_measurement(id);
}

bool TextSceneService::synchronize(
    TextSceneId id,
    graphics::GlyphPlacement placement) {
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
    if(require_record(id).view) throw std::logic_error("Text views expose only const shared state");
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

const TextSceneRecordCounters& TextSceneService::record_counters(
    TextSceneId id) const {
    ensure_owner_thread();
    return require_record(id).counters;
}

const graphics::GlyphAtlasError& TextSceneService::last_error(
    TextSceneId id) const {
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

const TextSceneService::Record* TextSceneService::find_record(
    TextSceneId id) const noexcept {
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

const TextSceneService::Record& TextSceneService::require_record(
    TextSceneId id) const {
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

void TextSceneService::rebuild_ordered_scene() {
    ordered_scene_.clear();
    for (const auto id : ordered_ids_) {
        ordered_scene_.append_glyph(require_record(id).primitive);
    }
    ++counters_.ordered_scene_rebuilds;
}

void TextSceneService::shift_primitive(
    graphics::GlyphPrimitive& primitive,
    std::int64_t offset) {
    const auto shift = [offset](std::uint32_t first) {
        const std::int64_t shifted = static_cast<std::int64_t>(first) + offset;
        if (shifted < 0
                || shifted > std::numeric_limits<std::uint32_t>::max()) {
            throw std::logic_error("Text scene range remap exceeded uint32_t");
        }
        return static_cast<std::uint32_t>(shifted);
    };
    primitive.instances.first = shift(primitive.instances.first);
    for (auto& range : primitive.draw_ranges) {
        range.instances.first = shift(range.instances.first);
    }
}

void TextSceneService::advance_generation(Slot& slot) noexcept {
    ++slot.generation;
    if (slot.generation == 0) {
        slot.generation = 1;
    }
}

} // namespace ryn::detail
