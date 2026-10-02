#pragma once

#include "graphics/glyph_atlas.hpp"

#include <cstddef>
#include <cstdint>
#include <span>
#include <stdexcept>

namespace ryn::detail {

// R8 source pixels, borrowed only until upload returns. Source offset is relative
// to bytes; destination coordinates and backend transfer layout are independent.
struct GlyphTextureUpload final {
    std::uint32_t page{};
    graphics::GlyphAtlasRect rectangle{};
    std::size_t source_offset{};
    std::uint32_t source_row_pitch{};
    std::span<const std::byte> bytes;
};

inline void validate_glyph_texture_upload(const GlyphTextureUpload& source, std::uint32_t target_width,
                                          std::uint32_t target_height) {
    const auto rect = source.rectangle;
    if (!rect.width || !rect.height || std::uint64_t(rect.x) + rect.width > target_width ||
        std::uint64_t(rect.y) + rect.height > target_height || source.source_row_pitch < rect.width ||
        source.source_offset > source.bytes.size()) {
        throw std::out_of_range("Glyph texture source or destination range is invalid");
    }
    const auto available = source.bytes.size() - source.source_offset;
    // Subtraction/division avoids overflow, and the final row needs no padding.
    if (rect.width > available || std::size_t(rect.height - 1) > (available - rect.width) / source.source_row_pitch) {
        throw std::out_of_range("Glyph texture source bytes are truncated");
    }
}

} // namespace ryn::detail
