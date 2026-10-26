#pragma once

#include <cstddef>
#include <span>
#include <string_view>

namespace ryn::detail {

// The bytes belong to the executable and remain valid for its lifetime.
[[nodiscard]] std::span<const std::byte> builtin_shader_bytes(std::string_view filename);

} // namespace ryn::detail
