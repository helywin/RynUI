#pragma once

#include <ryn/component.hpp>

#include <cstddef>
#include <cstdint>
#include <functional>
#include <string>
#include <string_view>
#include <vector>

namespace rynui::example {

struct TokenGalleryTelemetry final {
    std::uint64_t content_runs{};
    std::uint64_t theme_content_runs{};
    std::uint64_t theme_updates{};
    std::uint64_t brand_updates{};
    std::uint64_t viewport_updates{};
    std::uint64_t state_updates{};
    std::uint64_t activations{};
    std::uint64_t snapshot_identity{};
    std::string snapshot_diagnostic;
};

struct TokenGalleryViewport final {
    float width{};
    float height{};
};

struct TokenGalleryDefinition final {
    ryn::Content content;
    std::function<void(std::size_t)> smoke_step;
    std::function<void(float)> set_viewport_width;
    std::function<TokenGalleryTelemetry()> telemetry;
    std::vector<std::string_view> stable_test_ids;
};

[[nodiscard]] TokenGalleryDefinition make_token_gallery_definition();

[[nodiscard]] TokenGalleryViewport token_gallery_logical_viewport(
    int pixel_width,
    int pixel_height,
    float render_scale);

int run_token_gallery(int argc, char** argv, TokenGalleryDefinition definition);

} // namespace rynui::example
