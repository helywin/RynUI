#pragma once

#include "runtime/invalidation.hpp"
#include "runtime/node_store.hpp"

#include <cstddef>
#include <optional>

namespace rynui::example {

struct GalleryScrollRangeSnapshot final {
    float viewport_extent{};
    float content_extent{};
    float maximum_offset{};
    float offset{};
};

class GalleryScrollRange final {
public:
    bool set_extents(float viewport_extent, float content_extent);
    bool scroll_to(float offset);
    bool scroll_by(float delta);
    [[nodiscard]] GalleryScrollRangeSnapshot snapshot() const noexcept;

private:
    [[nodiscard]] float maximum_offset() const noexcept;

    float viewport_extent_{1.0F};
    float content_extent_{};
    float offset_{};
};

struct GalleryScrollTranslationResult final {
    bool valid{};
    bool changed{};
    std::size_t translated_nodes{};
};

class GalleryScrollTranslation final {
public:
    [[nodiscard]] GalleryScrollTranslationResult apply(
        ryn::runtime::NodeId root,
        float offset,
        ryn::runtime::NodeStore& nodes,
        ryn::runtime::DirtyQueues& dirty);

private:
    std::optional<ryn::runtime::NodeId> applied_root_;
    float applied_offset_{};
};

struct GalleryScrollbarGeometry final {
    ryn::runtime::Rect track;
    ryn::runtime::Rect thumb;
    float maximum_offset{};
};

[[nodiscard]] GalleryScrollbarGeometry gallery_scrollbar_geometry(
    ryn::runtime::Rect track,
    GalleryScrollRangeSnapshot range);
[[nodiscard]] float gallery_scrollbar_offset_for_thumb_top(
    const GalleryScrollbarGeometry& geometry,
    float thumb_top);

} // namespace rynui::example
