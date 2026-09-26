#pragma once

#include "gallery_document_model.hpp"
#include "gallery_scroll_region.hpp"

#include <array>
#include <cstdint>
#include <optional>
#include <span>
#include <string_view>
#include <vector>

namespace rynui::example {

struct GalleryDocumentAnchorId final {
    std::uint32_t index{};
    std::uint32_t generation{};

    [[nodiscard]] constexpr bool valid() const noexcept {
        return generation != 0;
    }

    friend constexpr bool operator==(
        GalleryDocumentAnchorId,
        GalleryDocumentAnchorId) = default;
};

struct GalleryComponentAnchor final {
    std::string_view identity;
    float offset{};

    friend constexpr bool operator==(GalleryComponentAnchor,
        GalleryComponentAnchor) = default;
};

struct GalleryDocumentResizeAnchor final {
    GalleryDocumentSectionKind section{GalleryDocumentSectionKind::header_source};
    float distance{};
};

struct GalleryDocumentViewportSnapshot final {
    float viewport_extent{};
    float content_extent{};
    float maximum_offset{};
    float offset{};
    GalleryDocumentSectionKind current_section{
        GalleryDocumentSectionKind::header_source};
    std::uint32_t anchor_generation{};
};

struct GalleryDocumentViewportDiagnostics final {
    std::uint64_t extent_updates{};
    std::uint64_t scroll_updates{};
    std::uint64_t anchor_updates{};
    std::uint64_t navigation_jumps{};
    std::uint64_t translation_passes{};
    std::uint64_t translated_nodes{};
};

class GalleryDocumentViewport final {
public:
    GalleryDocumentViewport() = default;

    bool set_extents(float viewport_extent, float content_extent);
    bool scroll_to(float offset);
    bool scroll_by(float delta);

    bool replace_anchors(std::span<const float> offsets);
    [[nodiscard]] std::optional<GalleryDocumentAnchorId> anchor(
        GalleryDocumentSectionKind section) const noexcept;
    bool replace_category_anchors(std::span<const float> offsets);
    [[nodiscard]] std::optional<GalleryDocumentAnchorId> category_anchor(
        AntDesignGalleryCategory category) const noexcept;
    bool replace_component_anchors(std::span<const GalleryComponentAnchor> anchors);
    [[nodiscard]] std::optional<GalleryDocumentAnchorId> component_anchor(
        std::string_view identity) const noexcept;
    bool jump_to(GalleryDocumentAnchorId anchor);
    [[nodiscard]] GalleryDocumentResizeAnchor capture_resize_anchor() const;
    bool restore_resize_anchor(const GalleryDocumentResizeAnchor& anchor);

    bool apply_subtree_translation(
        ryn::runtime::NodeId root,
        ryn::runtime::NodeStore& nodes,
        ryn::runtime::DirtyQueues& dirty) const;

    [[nodiscard]] GalleryDocumentViewportSnapshot snapshot() const noexcept;
    [[nodiscard]] const GalleryDocumentViewportDiagnostics& diagnostics()
        const noexcept;

private:
    static constexpr std::size_t section_count = 6;
    static constexpr std::size_t category_count = 7;
    static constexpr std::size_t anchor_count = section_count + category_count;

    [[nodiscard]] static std::size_t section_index(
        GalleryDocumentSectionKind section) noexcept;
    [[nodiscard]] GalleryDocumentSectionKind current_section() const noexcept;

    GalleryScrollRange scroll_;
    mutable GalleryScrollTranslation translation_;
    std::array<float, anchor_count> anchors_{};
    std::array<bool, anchor_count> anchor_present_{};
    std::vector<GalleryComponentAnchor> component_anchors_;
    std::uint32_t anchor_generation_{1};
    mutable GalleryDocumentViewportDiagnostics diagnostics_;
};

} // namespace rynui::example
