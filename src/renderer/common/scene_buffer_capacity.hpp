#pragma once

#include "renderer/common/scene_packing.hpp"

#include <algorithm>
#include <cstddef>
#include <cstdint>
#include <limits>
#include <stdexcept>

namespace ryn::detail {

[[nodiscard]] inline std::uint32_t quad_buffer_capacity(std::size_t required, std::uint32_t current) {
    constexpr auto maximum = std::numeric_limits<std::uint32_t>::max();
    if (required > maximum) {
        throw std::length_error("Quad GPU capacity exceeds uint32_t");
    }
    if (required <= current) {
        return current;
    }
    return static_cast<std::uint32_t>(
        std::min<std::uint64_t>(std::max<std::uint64_t>(required, std::uint64_t(current) * 2), maximum));
}

[[nodiscard]] inline std::uint32_t glyph_buffer_capacity(std::size_t required, std::uint32_t current,
                                                         std::uint64_t maximum_buffer_bytes) {
    const auto maximum = std::min({std::uint64_t(std::numeric_limits<std::uint32_t>::max()),
                                   std::uint64_t(std::numeric_limits<std::size_t>::max()) / sizeof(GlyphGpuInstance),
                                   maximum_buffer_bytes / sizeof(GlyphGpuInstance)});
    if (required > maximum || current > maximum) {
        throw std::length_error("Glyph instance buffer exceeds renderer input limit");
    }
    if (required <= current) {
        return current;
    }
    return static_cast<std::uint32_t>(
        std::min<std::uint64_t>(std::max<std::uint64_t>(required, std::uint64_t(current) * 2), maximum));
}

} // namespace ryn::detail
