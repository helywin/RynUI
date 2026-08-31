#pragma once

#include "ant_design_reference_catalog.hpp"

#include <cstdint>
#include <span>
#include <string_view>

namespace rynui::example {

enum class GalleryDocumentSectionKind : std::uint8_t {
    header_source,
    introduction,
    design_values,
    foundation_tokens,
    component_overview,
    live_samples,
};

struct GalleryDocumentSection final {
    GalleryDocumentSectionKind kind{GalleryDocumentSectionKind::header_source};
    std::string_view identity;
    std::string_view title;
    std::string_view summary;
};

struct GalleryDesignValue final {
    std::string_view identity;
    std::string_view english_name;
    std::string_view chinese_name;
    std::string_view summary;
};

enum class GalleryNavigationTargetKind : std::uint8_t {
    section,
    category,
};

struct GalleryNavigationTarget final {
    GalleryNavigationTargetKind kind{GalleryNavigationTargetKind::section};
    GalleryDocumentSectionKind section{GalleryDocumentSectionKind::header_source};
    AntDesignGalleryCategory category{AntDesignGalleryCategory::general};

    [[nodiscard]] static constexpr GalleryNavigationTarget to_section(
        GalleryDocumentSectionKind value) noexcept {
        return {GalleryNavigationTargetKind::section, value, {}};
    }

    [[nodiscard]] static constexpr GalleryNavigationTarget to_category(
        AntDesignGalleryCategory value) noexcept {
        return {
            GalleryNavigationTargetKind::category,
            GalleryDocumentSectionKind::component_overview,
            value,
        };
    }

    friend constexpr bool operator==(
        GalleryNavigationTarget,
        GalleryNavigationTarget) = default;
};

enum class GallerySupportFilter : std::uint8_t {
    all,
    implemented,
    partial,
    planned,
    web_only,
    deprecated,
    out_of_scope,
};

[[nodiscard]] std::span<const GalleryDocumentSection>
gallery_document_sections() noexcept;
[[nodiscard]] std::span<const GalleryDesignValue>
gallery_design_values() noexcept;
[[nodiscard]] bool gallery_support_filter_matches(
    GallerySupportFilter filter,
    GallerySupportStatus status) noexcept;
[[nodiscard]] std::string_view gallery_category_title(
    AntDesignGalleryCategory category) noexcept;

} // namespace rynui::example
