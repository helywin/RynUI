#pragma once

#include <ryn/icon.hpp>
#include <cstddef>
#include <span>
#include <vector>

namespace ryn::font {
// Validates the complete immutable definition before emitting deterministic
// OpenType/CFF bytes. No platform or renderer dependency.
[[nodiscard]] std::vector<std::byte> build_icon_vector_font(IconViewBox view_box, std::span<const IconPath> paths);
} // namespace ryn::font
