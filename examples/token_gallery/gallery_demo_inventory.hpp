#pragma once

#include <filesystem>
#include <optional>
#include <string>

namespace rynui::example {

[[nodiscard]] std::string gallery_demo_inventory_json();
[[nodiscard]] std::optional<int> run_gallery_demo_inventory(int argc, char** argv,
                                                            const std::filesystem::path& repository);

} // namespace rynui::example
