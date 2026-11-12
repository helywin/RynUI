#pragma once

#include "gallery_demo_json.hpp"
#include "gallery_page_model.hpp"

#include <filesystem>
#include <memory>
#include <string>
#include <vector>

namespace rynui::example {

struct GalleryDemoAction final {
    std::string step;
    std::string kind;
    std::string target;
    std::string layer;
    std::int64_t time_us{};
    GalleryDemoJson parameters;
    GalleryDemoJson preconditions;
};

struct GalleryDemoScenario final {
    std::string nonce;
    std::string identity;
    std::string fault;
    GalleryPageRoute route;
    GalleryDemoJson definition;
    GalleryDemoJson font_files;
    std::vector<GalleryDemoAction> actions;
    std::vector<std::string> checkpoints;
};

[[nodiscard]] GalleryDemoScenario read_gallery_demo_scenario(const std::filesystem::path& path,
                                                             const std::filesystem::path& repository);
[[nodiscard]] GalleryDemoJson gallery_demo_native_build_manifest();

} // namespace rynui::example
