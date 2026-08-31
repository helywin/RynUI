#pragma once

#include "gallery_document_model.hpp"
#include "runtime/invalidation.hpp"
#include "runtime/node_store.hpp"

#include <array>
#include <cstdint>
#include <optional>
#include <span>

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
    bool jump_to(GalleryDocumentAnchorId anchor);
    [[nodiscard]] GalleryDocumentResizeAnchor capture_resize_anchor() const;
    bool restore_resize_anchor(const GalleryDocumentResizeAnchor& anchor);

    bool apply_subtree_translation(
        ryn::runtime::NodeId root,
        ryn::runtime::NodeStore& nodes,
        ryn::runtime::DirtyQueues& dirty) const;

    [[nodiscard]] GalleryDocumentViewportSnapshot snapshot() const noexcept;

private:
    static constexpr std::size_t section_count = 6;
    static constexpr std::size_t category_count = 7;
    static constexpr std::size_t anchor_count = section_count + category_count;

    [[nodiscard]] static std::size_t section_index(
        GalleryDocumentSectionKind section) noexcept;
    [[nodiscard]] float maximum_offset() const noexcept;
    [[nodiscard]] float clamped(float value) const noexcept;
    [[nodiscard]] GalleryDocumentSectionKind current_section() const noexcept;
    static void translate_subtree(
        ryn::runtime::NodeId root,
        ryn::runtime::Point translation,
        ryn::runtime::NodeStore& nodes,
        ryn::runtime::NodePropertyWriter& writer);

    float viewport_extent_{1.0F};
    float content_extent_{};
    float offset_{};
    std::array<float, anchor_count> anchors_{};
    std::array<bool, anchor_count> anchor_present_{};
    std::uint32_t anchor_generation_{1};
};

} // namespace rynui::example
