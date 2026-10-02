#pragma once

#include "renderer/common/glyph_texture_upload.hpp"

#include <cstring>
#include <limits>

namespace ryn::detail {

struct SdlGlyphTextureLayout final {
    std::uint32_t pixels_per_row{};
    std::uint32_t rows_per_layer{};
    std::uint32_t byte_count{};
};

// SDL's conservative D3D12-compatible row packing stays in the SDL adapter.
[[nodiscard]] inline SdlGlyphTextureLayout sdl_glyph_texture_layout(std::uint32_t width, std::uint32_t height) {
    constexpr auto maximum = std::numeric_limits<std::uint32_t>::max();
    const auto pitch = (std::uint64_t(width) + 255) / 256 * 256;
    if (!width || !height || pitch > maximum || pitch > maximum / height) {
        throw std::length_error("SDL Glyph texture transfer exceeds uint32_t");
    }
    return {static_cast<std::uint32_t>(pitch), height, static_cast<std::uint32_t>(pitch * height)};
}

[[nodiscard]] inline SdlGlyphTextureLayout sdl_glyph_texture_layout(const GlyphTextureUpload& source) {
    constexpr auto maximum = std::numeric_limits<std::uint32_t>::max();
    validate_glyph_texture_upload(source, maximum, maximum);
    return sdl_glyph_texture_layout(source.rectangle.width, source.rectangle.height);
}

inline void pack_sdl_glyph_texture_rows(const GlyphTextureUpload& source, std::span<std::byte> destination) {
    const auto layout = sdl_glyph_texture_layout(source);
    if (destination.size() < layout.byte_count) {
        throw std::out_of_range("SDL Glyph texture transfer destination is truncated");
    }
    for (std::uint32_t row = 0; row < source.rectangle.height; ++row) {
        auto* target = destination.data() + std::size_t(row) * layout.pixels_per_row;
        std::memcpy(target, source.bytes.data() + source.source_offset + std::size_t(row) * source.source_row_pitch,
                    source.rectangle.width);
        std::memset(target + source.rectangle.width, 0, layout.pixels_per_row - source.rectangle.width);
    }
}

} // namespace ryn::detail
