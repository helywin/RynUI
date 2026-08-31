#include "ant_design_reference_catalog.hpp"
#include "gallery_document_model.hpp"

#include <array>
#include <cstddef>
#include <iostream>
#include <stdexcept>
#include <string_view>
#include <unordered_set>

namespace {

void require(bool condition, const char* message) {
    if (!condition) {
        throw std::runtime_error(message);
    }
}

void test_document_section_order_and_owned_copy() {
    using namespace rynui::example;
    constexpr std::array expected{
        GalleryDocumentSectionKind::header_source,
        GalleryDocumentSectionKind::introduction,
        GalleryDocumentSectionKind::design_values,
        GalleryDocumentSectionKind::foundation_tokens,
        GalleryDocumentSectionKind::component_overview,
        GalleryDocumentSectionKind::live_samples,
    };
    const auto sections = gallery_document_sections();
    require(sections.size() == expected.size(),
            "Gallery document section count drifted");
    std::unordered_set<std::string_view> identities;
    for (std::size_t index = 0; index < sections.size(); ++index) {
        const auto& section = sections[index];
        require(section.kind == expected[index]
                    && !section.identity.empty()
                    && !section.title.empty()
                    && !section.summary.empty()
                    && identities.insert(section.identity).second,
                "Gallery document section order or owned summary drifted");
        require(section.summary.find("import React") == std::string_view::npos
                    && section.summary.find("export default") == std::string_view::npos
                    && section.summary.find("<img") == std::string_view::npos
                    && section.summary.find(".ant-") == std::string_view::npos,
                "Gallery document copied implementation source into its summary");
    }
    require(gallery_design_values().size() == 4,
            "Gallery Design Values inventory drifted");
}

void test_official_sources_do_not_require_remote_images() {
    using namespace rynui::example;
    for (const auto& source : ant_design_reference_sources()) {
        require(source.official_url.starts_with("https://ant.design/")
                    && !source.official_url.ends_with(".png")
                    && !source.official_url.ends_with(".jpg")
                    && !source.official_url.ends_with(".svg"),
                "Gallery source is unofficial or requires a remote image");
    }
}

void test_typed_status_filter_covers_every_catalog_entry() {
    using namespace rynui::example;
    constexpr std::array filters{
        GallerySupportFilter::implemented,
        GallerySupportFilter::partial,
        GallerySupportFilter::planned,
        GallerySupportFilter::web_only,
        GallerySupportFilter::deprecated,
        GallerySupportFilter::out_of_scope,
    };
    constexpr std::array statuses{
        GallerySupportStatus::implemented,
        GallerySupportStatus::partial,
        GallerySupportStatus::planned,
        GallerySupportStatus::web_only,
        GallerySupportStatus::deprecated,
        GallerySupportStatus::out_of_scope,
    };
    std::array<std::size_t, statuses.size()> counts{};
    for (const auto& entry : ant_design_reference_entries()) {
        require(gallery_support_filter_matches(
                    GallerySupportFilter::all, entry.support_status),
                "Gallery all-status filter rejected a catalog entry");
        for (std::size_t index = 0; index < filters.size(); ++index) {
            const bool expected = entry.support_status == statuses[index];
            require(gallery_support_filter_matches(filters[index], entry.support_status)
                        == expected,
                    "Gallery typed status filter accepted the wrong status");
            counts[index] += expected ? 1U : 0U;
        }
    }
    require(counts[1] == 5 && counts[2] == 67,
            "Gallery initial partial/planned support overlay drifted");
    require(counts[0] == 0 && counts[3] == 0
                && counts[4] == 0 && counts[5] == 0,
            "Gallery initial support overlay invented another status");
}

void test_complete_category_heading_mapping() {
    using namespace rynui::example;
    for (const auto& category : ant_design_reference_categories()) {
        const auto title = gallery_category_title(category.category);
        require(!title.empty() && title.find(category.name) != std::string_view::npos,
                "Gallery category heading lost its English catalog identity");
    }
}

} // namespace

int main() {
    try {
        test_document_section_order_and_owned_copy();
        test_official_sources_do_not_require_remote_images();
        test_typed_status_filter_covers_every_catalog_entry();
        test_complete_category_heading_mapping();
    } catch (const std::exception& error) {
        std::cerr << error.what() << '\n';
        return 1;
    }
    return 0;
}
