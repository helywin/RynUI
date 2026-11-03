#pragma once

#include "gallery_page_model.hpp"
#include "gallery_page_state.hpp"
#include "token_gallery_definition.hpp"

#include <ryn/component.hpp>

#include <functional>
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
};

struct GallerySamplePage final {
    ryn::Content content{[] {}};
    std::function<void(GalleryPageDraft&)> save;
    std::function<void(float)> set_width;
    std::function<TokenGalleryTelemetry()> telemetry;
    std::function<std::uint64_t(std::string_view)> sample_runs;
    std::function<ryn::runtime::ComponentId(std::string_view)> target;
};

[[nodiscard]] std::span<const GallerySampleDescriptor> gallery_samples() noexcept;
[[nodiscard]] GallerySamplePage make_gallery_sample_page(const GalleryPageRoute& route, const GalleryPageDraft& draft);

} // namespace rynui::example
