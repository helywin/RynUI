#pragma once

#include <cstddef>
#include <span>
#include <string_view>

namespace rynui::example {

enum class AntDesignGalleryCategory {
    general,
    layout,
    navigation,
    data_entry,
    data_display,
    feedback,
    other,
};

enum class GallerySupportStatus {
    implemented,
    partial,
    planned,
    web_only,
    deprecated,
    out_of_scope,
};

struct AntDesignReferenceSource final {
    std::string_view identity;
    std::string_view title;
    std::string_view chinese_title;
    std::string_view source_path;
    std::string_view chinese_source_path;
    std::string_view renderer_source_path;
    std::string_view official_url;
};

struct AntDesignReferenceCategory final {
    AntDesignGalleryCategory category{};
    std::string_view identity;
    std::string_view name;
    std::string_view chinese_name;
    std::size_t order{};
    std::size_t expected_count{};
};

struct AntDesignReferenceEntry final {
    std::string_view identity;
    std::string_view english_name;
    std::string_view chinese_name;
    AntDesignGalleryCategory category{};
    std::string_view source_path;
    std::string_view chinese_source_path;
    GallerySupportStatus support_status{};
    std::string_view summary;
    std::string_view supported_scope;
    std::string_view missing_scope;
    std::string_view evidence_identifiers;
};

[[nodiscard]] std::span<const AntDesignReferenceSource>
ant_design_reference_sources() noexcept;

[[nodiscard]] std::span<const AntDesignReferenceCategory>
ant_design_reference_categories() noexcept;

[[nodiscard]] std::span<const AntDesignReferenceEntry>
ant_design_reference_entries() noexcept;

[[nodiscard]] const AntDesignReferenceEntry*
find_ant_design_reference_entry(std::string_view identity) noexcept;

[[nodiscard]] std::string_view ant_design_reference_catalog_hash() noexcept;
[[nodiscard]] std::string_view ant_design_reference_version() noexcept;
[[nodiscard]] std::string_view ant_design_reference_commit() noexcept;
[[nodiscard]] std::string_view gallery_support_status_name(
    GallerySupportStatus status) noexcept;

} // namespace rynui::example
