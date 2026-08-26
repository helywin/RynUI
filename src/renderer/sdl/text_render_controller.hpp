#pragma once

#include "graphics/glyph_scene.hpp"
#include "runtime/frame_scheduler.hpp"
#include "text/text_engine.hpp"

#include <array>
#include <cstdint>
#include <optional>
#include <vector>

namespace ryn::detail {

struct TextRenderControllerCounters {
    std::uint64_t instance_rebuilds{};
    std::uint64_t material_updates{};
};

class TextRenderController final {
public:
    TextRenderController(
        font::FontRuntime& fonts,
        text::TextEngine& engine,
        runtime::FrameRequestState& frame_requests,
        String content,
        std::vector<font::FontIdentity> fallback_chain,
        std::uint32_t pixel_size,
        text::TextLayoutConfig layout);

    bool set_content(String content);
    bool set_width_constraint(float width);
    bool set_color(std::array<float, 4> color);
    bool set_opacity(float opacity);
    [[nodiscard]] bool synchronize(graphics::GlyphPlacement placement);

    [[nodiscard]] text::TextState& text_state() noexcept;
    [[nodiscard]] const text::TextState& text_state() const noexcept;
    [[nodiscard]] graphics::GlyphAtlas& atlas() noexcept;
    [[nodiscard]] graphics::GlyphScene& glyph_scene() noexcept;
    [[nodiscard]] const graphics::OrderedScene& ordered_scene() const noexcept;
    [[nodiscard]] const graphics::GlyphPrimitive& primitive() const noexcept;
    [[nodiscard]] const graphics::GlyphAtlasError& last_error() const noexcept;
    [[nodiscard]] const TextRenderControllerCounters& counters() const noexcept;

private:
    font::FontRuntime* fonts_;
    runtime::FrameRequestState* frame_requests_;
    text::TextState text_state_;
    graphics::GlyphAtlas atlas_;
    graphics::GlyphScene glyph_scene_;
    graphics::OrderedScene ordered_scene_;
    graphics::GlyphPrimitive primitive_;
    std::optional<graphics::GlyphPlacement> placement_;
    graphics::GlyphAtlasError last_error_{};
    TextRenderControllerCounters counters_;
    bool rebuild_dirty_{true};
    bool material_dirty_{};
};

} // namespace ryn::detail
