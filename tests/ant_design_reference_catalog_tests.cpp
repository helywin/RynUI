#include "ant_design_reference_catalog.hpp"

#include <algorithm>
#include <array>
#include <cctype>
#include <cstddef>
#include <iostream>
#include <stdexcept>
#include <string_view>
#include <unordered_set>

namespace {

using rynui::example::AntDesignGalleryCategory;
using rynui::example::GallerySupportStatus;

void require(bool condition, const char* message) {
    if (!condition) {
        throw std::runtime_error(message);
    }
}

constexpr std::size_t category_index(AntDesignGalleryCategory category) {
    return static_cast<std::size_t>(category);
}

void test_snapshot_identity() {
    using namespace rynui::example;
    require(ant_design_reference_version() == "6.5.0",
            "Gallery catalog version drifted");
    require(ant_design_reference_commit()
                == "740ad964dc2397f33e40944367b0536a7314cc32",
            "Gallery catalog commit drifted");
    const auto hash = ant_design_reference_catalog_hash();
    require(hash.size() == 64
                && std::ranges::all_of(hash, [](char value) {
                    return std::isxdigit(static_cast<unsigned char>(value)) != 0;
                }),
            "Gallery catalog hash is not a SHA256 identity");
}

void test_document_sources() {
    using namespace rynui::example;
    const auto sources = ant_design_reference_sources();
    require(sources.size() == 4, "Gallery document source count drifted");
    require(sources[0].identity == "ant.document.introduction"
                && sources[0].source_path == "docs/spec/introduce.en-US.md",
            "Introduction source drifted");
    require(sources[1].identity == "ant.document.design-values"
                && sources[2].identity == "ant.document.resources"
                && sources[3].identity == "ant.document.components-overview",
            "Design Values, Resources, or Components Overview source drifted");
    for (const auto& source : sources) {
        require(!source.title.empty() && !source.chinese_title.empty()
                    && !source.source_path.empty()
                    && !source.chinese_source_path.empty()
                    && source.official_url.starts_with("https://ant.design/"),
                "Gallery document source is incomplete or not official");
    }
}

void test_categories_and_entries() {
    using namespace rynui::example;
    constexpr std::array<std::size_t, 7> expected_counts{4, 7, 7, 18, 20, 11, 5};
    constexpr std::array<std::string_view, 7> expected_names{
        "General", "Layout", "Navigation", "Data Entry",
        "Data Display", "Feedback", "Other"};
    const auto categories = ant_design_reference_categories();
    const auto entries = ant_design_reference_entries();
    require(categories.size() == 7 && entries.size() == 72,
            "Gallery category or component count drifted");
    std::array<std::size_t, 7> actual_counts{};
    std::unordered_set<std::string_view> identities;
    std::unordered_set<std::string_view> names;
    for (std::size_t index = 0; index < categories.size(); ++index) {
        const auto& category = categories[index];
        require(category.order == index && category.name == expected_names[index]
                    && category.expected_count == expected_counts[index]
                    && !category.identity.empty() && !category.chinese_name.empty(),
                "Gallery category order, identity, or expected count drifted");
    }
    for (const auto& entry : entries) {
        require(!entry.identity.empty() && !entry.english_name.empty()
                    && !entry.chinese_name.empty() && !entry.source_path.empty()
                    && !entry.chinese_source_path.empty() && !entry.summary.empty()
                    && !entry.supported_scope.empty() && !entry.missing_scope.empty()
                    && !entry.evidence_identifiers.empty(),
                "Gallery component metadata or support overlay is incomplete");
        require(identities.insert(entry.identity).second
                    && names.insert(entry.english_name).second,
                "Gallery component identity or English name is duplicated");
        ++actual_counts[category_index(entry.category)];
        require(find_ant_design_reference_entry(entry.identity) == &entry,
                "Gallery component lookup does not preserve generated identity");
    }
    require(actual_counts == expected_counts,
            "Gallery per-category component counts drifted");
    require(find_ant_design_reference_entry("ant.component.not-real") == nullptr,
            "unknown Gallery component lookup did not fail closed");
}

void require_partial(std::string_view identity) {
    using namespace rynui::example;
    const auto* entry = find_ant_design_reference_entry(identity);
    require(entry != nullptr && entry->support_status == GallerySupportStatus::partial
                && entry->evidence_identifiers.find("openspec:") != std::string_view::npos
                && entry->evidence_identifiers.find("test:") != std::string_view::npos,
            "initial RynUI subset is not represented as evidence-backed partial support");
}

void test_typed_support_status() {
    using namespace rynui::example;
    require_partial("ant.component.button");
    require_partial("ant.component.typography");
    require_partial("ant.component.flex");
    require_partial("ant.component.space");
    require_partial("ant.component.config-provider");
    require(gallery_support_status_name(GallerySupportStatus::implemented) == "implemented"
                && gallery_support_status_name(GallerySupportStatus::partial) == "partial"
                && gallery_support_status_name(GallerySupportStatus::planned) == "planned"
                && gallery_support_status_name(GallerySupportStatus::web_only) == "web-only"
                && gallery_support_status_name(GallerySupportStatus::deprecated) == "deprecated"
                && gallery_support_status_name(GallerySupportStatus::out_of_scope) == "out-of-scope",
            "typed GallerySupportStatus string mapping is incomplete");
}

} // namespace

int main() {
    try {
        test_snapshot_identity();
        test_document_sources();
        test_categories_and_entries();
        test_typed_support_status();
    } catch (const std::exception& error) {
        std::cerr << error.what() << '\n';
        return 1;
    }
    return 0;
}
