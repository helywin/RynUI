#pragma once

#include <cstdint>
#include <span>
#include <string_view>

namespace rynui::example {

struct GallerySampleSource final {
    std::string_view identity;
    std::string_view source_path;
    std::string_view source_function;
    std::string_view public_header;
    std::string_view public_props;
    std::string_view public_methods;
    std::string_view public_slots;
    std::string_view theme_path;
    std::string_view token_names;
    std::string_view cpp;
    std::uint32_t source_line{};
};

[[nodiscard]] std::span<const GallerySampleSource> gallery_sample_sources() noexcept;
[[nodiscard]] const GallerySampleSource* gallery_sample_source(std::string_view identity) noexcept;

} // namespace rynui::example
