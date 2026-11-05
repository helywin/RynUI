#pragma once
#include <ryn/string.hpp>
#include <cstdint>

namespace ryn::input {
enum class FileDropAction : std::uint8_t { Begin, Position, File, Complete };

struct FileDropEvent final {
    FileDropAction action{};
    String locator;
    float x{};
    float y{};
    std::uint32_t window_id{};
    friend bool operator==(const FileDropEvent&, const FileDropEvent&) = default;
};

[[nodiscard]] bool is_valid(const FileDropEvent&) noexcept;
} // namespace ryn::input
