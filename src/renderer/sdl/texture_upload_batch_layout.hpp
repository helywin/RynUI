#pragma once

#include "graphics/glyph_atlas.hpp"

#include <cstddef>
#include <cstdint>
#include <limits>
#include <span>
#include <stdexcept>
#include <vector>

namespace ryn::detail {

struct TextureUploadBatchRegion final {
    void* target{};
    graphics::GlyphAtlasRect rectangle{};
    std::uint32_t source_offset{};
    std::uint32_t pixels_per_row{};
    std::uint32_t rows_per_layer{};
    std::uint32_t byte_count{};
};

class TextureUploadBatchLayout final {
public:
    static constexpr std::uint32_t default_capacity = 1024U * 1024U;
    static constexpr std::uint32_t source_alignment = 512U;

    void reset(std::uint32_t capacity = default_capacity) noexcept {
        capacity_ = capacity;
        used_ = 0;
        regions_.clear();
    }

    [[nodiscard]] bool can_fit(std::size_t byte_count) const noexcept {
        if (byte_count == 0 || byte_count > std::numeric_limits<std::uint32_t>::max()) {
            return false;
        }
        const std::uint64_t aligned =
            (static_cast<std::uint64_t>(used_) + source_alignment - 1U)
            / source_alignment * source_alignment;
        return aligned + byte_count <= capacity_;
    }

    [[nodiscard]] std::uint32_t append(
        void* target,
        graphics::GlyphAtlasRect rectangle,
        std::uint32_t pixels_per_row,
        std::uint32_t rows_per_layer,
        std::uint32_t byte_count) {
        if (target == nullptr || !can_fit(byte_count)) {
            throw std::length_error("Glyph texture region does not fit transfer chunk");
        }
        const auto source_offset = static_cast<std::uint32_t>(
            (static_cast<std::uint64_t>(used_) + source_alignment - 1U)
            / source_alignment * source_alignment);
        regions_.push_back({
            target, rectangle, source_offset, pixels_per_row, rows_per_layer, byte_count,
        });
        used_ = source_offset + byte_count;
        return source_offset;
    }

    [[nodiscard]] std::span<const TextureUploadBatchRegion> regions() const noexcept {
        return regions_;
    }
    [[nodiscard]] std::uint32_t used_bytes() const noexcept { return used_; }
    [[nodiscard]] std::uint32_t capacity() const noexcept { return capacity_; }

private:
    std::uint32_t capacity_{default_capacity};
    std::uint32_t used_{};
    std::vector<TextureUploadBatchRegion> regions_;
};

} // namespace ryn::detail
