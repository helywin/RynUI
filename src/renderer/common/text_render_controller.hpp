#pragma once

#include "text/text_scene_service.hpp"

#include <array>
#include <cstdint>
#include <vector>

namespace ryn::detail {

using TextRenderControllerCounters = TextSceneRecordCounters;

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
    TextSceneService service_;
    TextSceneId record_;
};

} // namespace ryn::detail
