#include "renderer/sdl/text_render_controller.hpp"

#include <utility>

namespace ryn::detail {
namespace {

[[nodiscard]] bool same_geometry(
    const graphics::GlyphPlacement& left,
    const graphics::GlyphPlacement& right) noexcept {
    return left.origin_pixels == right.origin_pixels
        && left.viewport_pixels == right.viewport_pixels
        && left.clip_pixels == right.clip_pixels
        && left.translation_pixels == right.translation_pixels;
}

} // namespace

TextRenderController::TextRenderController(
    font::FontRuntime& fonts,
    text::TextEngine& engine,
    runtime::FrameRequestState& frame_requests,
    String content,
    std::vector<font::FontIdentity> fallback_chain,
    std::uint32_t pixel_size,
    text::TextLayoutConfig layout)
    : fonts_(&fonts),
      frame_requests_(&frame_requests),
      text_state_(
          engine,
          std::move(content),
          std::move(fallback_chain),
          pixel_size,
          layout,
          [&frame_requests] { frame_requests.request_frame(); }) {
    frame_requests_->request_frame();
}

bool TextRenderController::set_content(String content) {
    if (!text_state_.set_content(std::move(content))) {
        return false;
    }
    rebuild_dirty_ = true;
    return true;
}

bool TextRenderController::set_width_constraint(float width) {
    if (!text_state_.set_width_constraint(width)) {
        return false;
    }
    rebuild_dirty_ = true;
    return true;
}

bool TextRenderController::set_color(std::array<float, 4> color) {
    if (!text_state_.set_color(color)) {
        return false;
    }
    material_dirty_ = true;
    return true;
}

bool TextRenderController::set_opacity(float opacity) {
    if (!text_state_.set_opacity(opacity)) {
        return false;
    }
    material_dirty_ = true;
    return true;
}

bool TextRenderController::synchronize(graphics::GlyphPlacement placement) {
    last_error_ = {};
    if (!text_state_.synchronize()) {
        return false;
    }
    placement.color = text_state_.material().color;
    placement.opacity = text_state_.material().opacity;
    if (!placement_ || !same_geometry(*placement_, placement)) {
        rebuild_dirty_ = true;
    }
    if (rebuild_dirty_) {
        auto result = glyph_scene_.append_text(
            *fonts_,
            atlas_,
            text_state_.shaped(),
            text_state_.measurement(),
            placement);
        if (!result) {
            last_error_ = std::move(result.error);
            return false;
        }
        primitive_ = std::move(result.primitive);
        ordered_scene_.clear();
        ordered_scene_.append_glyph(primitive_);
        placement_ = placement;
        rebuild_dirty_ = false;
        material_dirty_ = false;
        ++counters_.instance_rebuilds;
        return true;
    }
    if (material_dirty_) {
        const std::size_t updated = glyph_scene_.instances().update_material(
            primitive_.instances,
            text_state_.material().color,
            text_state_.material().opacity);
        if (updated != 0) {
            ++counters_.material_updates;
        }
        placement_->color = text_state_.material().color;
        placement_->opacity = text_state_.material().opacity;
        material_dirty_ = false;
    }
    return true;
}

text::TextState& TextRenderController::text_state() noexcept {
    return text_state_;
}

const text::TextState& TextRenderController::text_state() const noexcept {
    return text_state_;
}

graphics::GlyphAtlas& TextRenderController::atlas() noexcept {
    return atlas_;
}

graphics::GlyphScene& TextRenderController::glyph_scene() noexcept {
    return glyph_scene_;
}

const graphics::OrderedScene& TextRenderController::ordered_scene() const noexcept {
    return ordered_scene_;
}

const graphics::GlyphPrimitive& TextRenderController::primitive() const noexcept {
    return primitive_;
}

const graphics::GlyphAtlasError& TextRenderController::last_error() const noexcept {
    return last_error_;
}

const TextRenderControllerCounters& TextRenderController::counters() const noexcept {
    return counters_;
}

} // namespace ryn::detail
