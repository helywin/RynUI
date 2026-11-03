#pragma once

#include "gallery_page_model.hpp"
#include "gallery_page_state.hpp"
#include "token_gallery_definition.hpp"

#include <ryn/component.hpp>
#include <ryn/theme.hpp>

#include <functional>
#include <optional>
#include <span>
#include <string_view>

namespace ryn::runtime {
struct ComponentId;
}

namespace rynui::example {

struct GallerySampleDescriptor final {
    std::string_view identity;
    std::string_view component_identity;
    GalleryInputFamily family{};
    std::string_view title;
    std::string_view source_function;
    std::string_view public_header;
    GalleryPageKind kind{GalleryPageKind::component};
};

struct GallerySamplePageOptions final {
    bool readable_labels{};
    bool document_spacing{};
    // Optional internal registry view; entries must refer to existing real factories.
    std::optional<std::span<const GallerySampleDescriptor>> registry;
    std::function<void(const GallerySampleDescriptor&, ryn::Signal<ryn::LogicalLength>)> footer;
};

struct GallerySamplePage final {
    ryn::Content content{[] {}};
    std::function<void(GalleryPageDraft&)> save;
    std::function<void(float)> set_width;
    std::function<TokenGalleryTelemetry()> telemetry;
    std::function<std::uint64_t(std::string_view)> sample_runs;
    std::function<ryn::runtime::ComponentId(std::string_view)> target;
    std::function<void(const ryn::ThemeConfig&)> set_theme;
    std::function<std::optional<ryn::ThemeConfig>()> take_theme_request;
    std::function<void()> finish_state_demo;
};

struct GalleryRelatedSample final {
    std::string_view component_identity;
    std::string_view title;
    GalleryPageKind target_kind{GalleryPageKind::component};
    std::string_view target_component;
    std::string_view target_sample;

    [[nodiscard]] GalleryPageRoute route() const;
};

[[nodiscard]] std::span<const GallerySampleDescriptor> gallery_samples() noexcept;
[[nodiscard]] std::span<const GalleryRelatedSample> gallery_related_samples() noexcept;
[[nodiscard]] GallerySamplePage make_gallery_sample_page(const GalleryPageRoute& route, const GalleryPageDraft& draft,
                                                         GallerySamplePageOptions options = {});

} // namespace rynui::example
