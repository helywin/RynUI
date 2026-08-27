#include "renderer/sdl/text_render_controller.hpp"

#include <utility>

namespace ryn::detail {

TextRenderController::TextRenderController(
    font::FontRuntime& fonts,
    text::TextEngine& engine,
    runtime::FrameRequestState& frame_requests,
    String content,
    std::vector<font::FontIdentity> fallback_chain,
    std::uint32_t pixel_size,
    text::TextLayoutConfig layout)
    : service_(fonts, engine, frame_requests),
      record_(service_.create(
          runtime::NodeId{0, 1},
          std::move(content),
          std::move(fallback_chain),
          pixel_size,
          layout)) {}

bool TextRenderController::set_content(String content) {
    return service_.set_content(record_, std::move(content));
}

bool TextRenderController::set_width_constraint(float width) {
    return service_.set_width_constraint(record_, width);
}

bool TextRenderController::set_color(std::array<float, 4> color) {
    return service_.set_color(record_, color);
}

bool TextRenderController::set_opacity(float opacity) {
    return service_.set_opacity(record_, opacity);
}

bool TextRenderController::synchronize(graphics::GlyphPlacement placement) {
    return service_.synchronize(record_, std::move(placement));
}

text::TextState& TextRenderController::text_state() noexcept {
    return service_.text_state(record_);
}

const text::TextState& TextRenderController::text_state() const noexcept {
    return service_.text_state(record_);
}

graphics::GlyphAtlas& TextRenderController::atlas() noexcept {
    return service_.atlas();
}

graphics::GlyphScene& TextRenderController::glyph_scene() noexcept {
    return service_.glyph_scene();
}

const graphics::OrderedScene& TextRenderController::ordered_scene() const noexcept {
    return service_.ordered_scene();
}

const graphics::GlyphPrimitive& TextRenderController::primitive() const noexcept {
    return service_.primitive(record_);
}

const graphics::GlyphAtlasError& TextRenderController::last_error() const noexcept {
    return service_.last_error(record_);
}

const TextRenderControllerCounters& TextRenderController::counters() const noexcept {
    return service_.record_counters(record_);
}

} // namespace ryn::detail
