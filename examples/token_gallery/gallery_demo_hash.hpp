#pragma once

#include <filesystem>
#include <span>
#include <string>

namespace rynui::example {

[[nodiscard]] std::string gallery_demo_sha256(std::span<const unsigned char> bytes);
[[nodiscard]] std::string gallery_demo_file_sha256(const std::filesystem::path& path);

} // namespace rynui::example
