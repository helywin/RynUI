#pragma once

#include "input/platform_input.hpp"
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
    void invalidate() noexcept {
        applied_root_.reset();
    }

    [[nodiscard]] GalleryScrollTranslationResult
    apply(ryn::runtime::NodeId root, float offset, ryn::runtime::NodeStore& nodes, ryn::runtime::DirtyQueues& dirty);

private:
    std::optional<ryn::runtime::NodeId> applied_root_;
    float applied_offset_{};
};

struct GalleryScrollbarGeometry final {
    ryn::runtime::Rect track;
    ryn::runtime::Rect thumb;
    float maximum_offset{};
};

[[nodiscard]] GalleryScrollbarGeometry gallery_scrollbar_geometry(ryn::runtime::Rect track,
                                                                  GalleryScrollRangeSnapshot range);
[[nodiscard]] float gallery_scrollbar_offset_for_thumb_top(const GalleryScrollbarGeometry& geometry, float thumb_top);
[[nodiscard]] bool gallery_place_scrollbar(ryn::runtime::NodeId node, ryn::runtime::Rect target,
                                           ryn::runtime::NodeStore& nodes, ryn::runtime::DirtyQueues& dirty);

enum class GalleryScrollTarget { none, navigation, document };
[[nodiscard]] GalleryScrollTarget gallery_scroll_target(float x, float y, bool narrow,
                                                        ryn::runtime::Rect navigation_lane,
                                                        ryn::runtime::Rect document_lane);

struct GalleryScrollbarAction final {
    bool consumed{};
    std::optional<float> requested_offset;
};

struct GalleryScrollbarVisualState final {
    bool track_hover{};
    bool thumb_hover{};
    bool track_pressed{};
    bool thumb_pressed{};
    bool dragging{};

    friend constexpr bool operator==(GalleryScrollbarVisualState, GalleryScrollbarVisualState) = default;
};

class GalleryScrollbarController final {
public:
    [[nodiscard]] GalleryScrollbarAction dispatch(const ryn::input::PointerInputEvent& event,
                                                  const GalleryScrollbarGeometry& geometry, float viewport_extent,
                                                  float current_offset);

    [[nodiscard]] bool dragging() const noexcept {
        return dragging_pointer_.has_value();
    }

    [[nodiscard]] GalleryScrollbarVisualState visual_state() const noexcept;
    [[nodiscard]] bool reset() noexcept;
    [[nodiscard]] bool clear_hover() noexcept;

private:
    enum class HoverPart { none, track, thumb };
    std::optional<ryn::input::PointerIdentity> dragging_pointer_;
    std::optional<ryn::input::PointerIdentity> pressed_track_pointer_;
    HoverPart hover_{HoverPart::none};
    float grab_offset_{};
};

} // namespace rynui::example
