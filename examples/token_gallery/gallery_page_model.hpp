#pragma once

#include "gallery_document_model.hpp"

#include <array>
#include <optional>
#include <span>
#include <string>
#include <string_view>
#include <vector>

namespace rynui::example {

enum class GalleryPageKind {
    overview,
    introduction,
    tokens,
    component,
};

enum class GalleryInputFamily {
    input,
    search,
    password,
    text_area,
    otp,
};

enum class GalleryPageSection {
    usage,
    examples,
    api,
    support,
    tokens,
    sources,
};

struct GalleryPageRoute final {
    GalleryPageKind kind{GalleryPageKind::overview};
    std::string component_identity;
    GalleryInputFamily family{GalleryInputFamily::input};
    std::string sample_identity;
    std::optional<GalleryPageSection> section{};

    friend bool operator==(const GalleryPageRoute&, const GalleryPageRoute&) = default;
};

struct GalleryPageDescriptor final {
    const AntDesignReferenceEntry* entry{};

    [[nodiscard]] bool native_scope() const noexcept;
    [[nodiscard]] GalleryPageRoute route(GalleryInputFamily family = GalleryInputFamily::input) const;
};

struct GalleryPageSearchResult final {
    const GalleryPageDescriptor* page{};
    GalleryInputFamily family{GalleryInputFamily::input};
};

// This is a distribution of catalog declarations, never a feature completion fraction.
struct GalleryPageCounts final {
    std::array<std::size_t, 6> status{};
    std::array<std::size_t, 7> category{};
};

class GalleryPageModel final {
public:
    explicit GalleryPageModel(std::span<const AntDesignReferenceEntry> entries = ant_design_reference_entries());

    [[nodiscard]] std::span<const GalleryPageDescriptor> pages() const noexcept;
    [[nodiscard]] const GalleryPageDescriptor* find(std::string_view identity) const noexcept;
    [[nodiscard]] const GalleryPageCounts& counts() const noexcept;
    [[nodiscard]] const GalleryPageRoute& current() const noexcept;
    [[nodiscard]] const std::string& query() const noexcept;
    [[nodiscard]] GallerySupportFilter filter() const noexcept;
    [[nodiscard]] bool valid(const GalleryPageRoute& route) const noexcept;
    bool select(GalleryPageRoute route);
    bool set_query(std::string value);
    bool set_filter(GallerySupportFilter value) noexcept;
    [[nodiscard]] std::vector<GalleryPageSearchResult> results() const;
    [[nodiscard]] bool current_matches() const;

private:
    std::vector<GalleryPageDescriptor> pages_;
    GalleryPageCounts counts_;
    GalleryPageRoute current_;
    std::string query_;
    GallerySupportFilter filter_{GallerySupportFilter::all};
};

[[nodiscard]] std::string_view gallery_input_family_name(GalleryInputFamily family) noexcept;
[[nodiscard]] std::string_view gallery_page_empty_message(GallerySupportStatus status) noexcept;
[[nodiscard]] std::string_view gallery_page_status_label(GallerySupportStatus status) noexcept;

} // namespace rynui::example
