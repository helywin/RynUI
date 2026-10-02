#pragma once

#include <algorithm>
#include <cstddef>
#include <cstdint>
#include <limits>
#include <stdexcept>

namespace ryn::detail {

[[nodiscard]] inline std::uint32_t quad_buffer_capacity(
    std::size_t required, std::uint32_t current) {
    constexpr auto maximum = std::numeric_limits<std::uint32_t>::max();
    if (required > maximum) throw std::length_error("Quad GPU capacity exceeds uint32_t");
    if (required <= current) return current;
    return static_cast<std::uint32_t>(std::min<std::uint64_t>(
        std::max<std::uint64_t>(required, std::uint64_t(current) * 2), maximum));
}

} // namespace ryn::detail
