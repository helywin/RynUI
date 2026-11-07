#pragma once
#include "gallery_page_model.hpp"
#include <filesystem>
#include <span>
#include <string>
#include <string_view>
#include <vector>
#include <ryn/i18n.hpp>
#include <ryn/text_direction.hpp>

namespace rynui::example {
struct GalleryPageOptions final {
    bool acceptance{};
    bool visual{};
    bool motion_disabled{};
    std::filesystem::path output;
    std::vector<GalleryPageRoute> pages;
    int width{};
    int timeout_seconds{30};
    bool i18n{};
    ryn::LocaleId locale{ryn::locales::zh_CN};
    ryn::LocaleId format_locale{ryn::locales::zh_CN};
    ryn::TextDirection direction{ryn::TextDirection::LeftToRight};
};

struct GalleryPageAcceptanceResult final {
    std::string platform;
    std::string preset;
    std::string configuration;
    std::string toolchain;
    std::string backend;
    std::string shader;
    std::string font_source;
    std::string error;
    bool system_fonts{};
    bool host_input_area{};
    bool host_cursor_sync{};
    bool passed{};
    std::uint64_t checks{};
    std::uint64_t submitted_frames{};
    std::uint64_t cases{};
};

[[nodiscard]] GalleryPageOptions gallery_page_options(std::span<const std::string_view> arguments,
                                                      const std::filesystem::path& repository);
void write_gallery_page_acceptance(const std::filesystem::path& output, const GalleryPageAcceptanceResult& result);
} // namespace rynui::example
