#pragma once

#include "gallery_document_model.hpp"

#include <ryn/component.hpp>

#include <cstddef>
#include <cstdint>
#include <functional>
#include <optional>
#include <string>
#include <string_view>
#include <vector>

namespace rynui::example {

struct TokenGalleryTelemetry final {
    std::uint64_t content_runs{};
    std::uint64_t theme_content_runs{};
    std::uint64_t theme_updates{};
    std::uint64_t brand_updates{};
    std::uint64_t motion_updates{};
    std::uint64_t viewport_updates{};
    std::uint64_t state_updates{};
    std::uint64_t activations{};
    std::uint64_t document_sections{};
    std::uint64_t component_entries{};
    std::uint64_t reference_surfaces{};
    std::uint64_t reference_content_runs{};
    std::uint64_t live_samples{};
    std::uint64_t navigation_requests{};
    std::uint64_t filter_updates{};
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
    std::function<void(bool)> set_motion_enabled;
    std::function<std::optional<GalleryNavigationTarget>()>
        take_navigation_request;
    std::function<TokenGalleryTelemetry()> telemetry;
    std::vector<std::string_view> stable_test_ids;
    std::size_t navigation_control_count{};
};

[[nodiscard]] TokenGalleryDefinition make_token_gallery_definition();

[[nodiscard]] TokenGalleryViewport token_gallery_logical_viewport(
    int pixel_width,
    int pixel_height,
    float render_scale);

[[nodiscard]] float token_gallery_pointer_to_render_logical(
    float host_logical_coordinate,
    float host_display_scale,
    float render_scale);

int run_token_gallery(int argc, char** argv, TokenGalleryDefinition definition);

} // namespace rynui::example
