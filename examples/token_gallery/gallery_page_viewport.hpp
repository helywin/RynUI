#pragma once

#include "gallery_page_state.hpp"
#include "gallery_scroll_region.hpp"

#include <cstdint>
#include <optional>
#include <span>
#include <string>
#include <string_view>
#include <vector>

namespace rynui::example {

struct GalleryPageAnchorNode final {
    std::string identity;
    ryn::runtime::NodeId node;

    friend bool operator==(const GalleryPageAnchorNode&, const GalleryPageAnchorNode&) = default;
};

struct GalleryPageAnchorHandle final {
    std::uint64_t generation{};
    std::size_t index{};

    friend bool operator==(GalleryPageAnchorHandle, GalleryPageAnchorHandle) = default;
};

struct GalleryPageViewportWork final {
    std::uint64_t pages{};
    std::uint64_t anchor_updates{};
    std::uint64_t translation_passes{};
    std::uint64_t translated_nodes{};
};

class GalleryPageViewport final {
public:
    bool set_page(std::uint64_t generation, ryn::runtime::NodeId root, std::span<const GalleryPageAnchorNode> anchors);
    bool refresh(float viewport_extent, const ryn::runtime::NodeStore& nodes);
    bool scroll_to(float offset);
    bool scroll_by(float delta);
    [[nodiscard]] GalleryScrollRangeSnapshot snapshot() const noexcept;
    [[nodiscard]] std::uint64_t generation() const noexcept;
    [[nodiscard]] std::optional<GalleryPageAnchorHandle> anchor(std::string_view identity) const noexcept;
    bool jump(GalleryPageAnchorHandle handle);
    [[nodiscard]] GalleryPageAnchor capture() const;
    bool restore(const GalleryPageAnchor& saved);
    [[nodiscard]] GalleryScrollTranslationResult apply(ryn::runtime::NodeStore& nodes,
                                                       ryn::runtime::DirtyQueues& dirty);
    [[nodiscard]] const GalleryPageViewportWork& work() const noexcept;

private:
    GalleryScrollRange range_;
    GalleryScrollTranslation translation_;
    std::uint64_t generation_{};
    std::uint64_t applied_topology_{};
    ryn::runtime::NodeId root_;
    std::vector<GalleryPageAnchorNode> anchors_;
    std::vector<float> offsets_;
    GalleryPageViewportWork work_;
};

} // namespace rynui::example
