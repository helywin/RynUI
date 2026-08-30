#include "ant_design_reference_catalog.hpp"

#include <algorithm>
#include <array>

namespace rynui::example {
namespace {

#include "generated_ant_design_reference_catalog.inc"

} // namespace

std::span<const AntDesignReferenceSource> ant_design_reference_sources() noexcept {
    return kAntDesignReferenceSources;
}

std::span<const AntDesignReferenceCategory> ant_design_reference_categories() noexcept {
    return kAntDesignReferenceCategories;
}

std::span<const AntDesignReferenceEntry> ant_design_reference_entries() noexcept {
    return kAntDesignReferenceEntries;
}

const AntDesignReferenceEntry* find_ant_design_reference_entry(
    std::string_view identity) noexcept {
    const auto found = std::ranges::find(
        kAntDesignReferenceEntries, identity, &AntDesignReferenceEntry::identity);
    return found == kAntDesignReferenceEntries.end() ? nullptr : &*found;
}

std::string_view ant_design_reference_catalog_hash() noexcept {
    return kAntDesignReferenceCatalogHash;
}

std::string_view ant_design_reference_version() noexcept {
    return kAntDesignReferenceVersion;
}

std::string_view ant_design_reference_commit() noexcept {
    return kAntDesignReferenceCommit;
}

std::string_view gallery_support_status_name(GallerySupportStatus status) noexcept {
    switch (status) {
    case GallerySupportStatus::implemented:
        return "implemented";
    case GallerySupportStatus::partial:
        return "partial";
    case GallerySupportStatus::planned:
        return "planned";
    case GallerySupportStatus::web_only:
        return "web-only";
    case GallerySupportStatus::deprecated:
        return "deprecated";
    case GallerySupportStatus::out_of_scope:
        return "out-of-scope";
    }
    return {};
}

} // namespace rynui::example
