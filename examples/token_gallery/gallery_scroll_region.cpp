#include "gallery_scroll_region.hpp"

#include <algorithm>
#include <cmath>
#include <stdexcept>

namespace rynui::example {
namespace {

std::size_t translate_subtree(
    ryn::runtime::NodeId root,
    ryn::runtime::Point translation,
    ryn::runtime::NodeStore& nodes,
    ryn::runtime::NodePropertyWriter& writer) {
    const auto& children = nodes.require(root).children;
    static_cast<void>(writer.set_translation(root, translation));
    std::size_t translated = 1;
    for (const auto child : children) {
        translated += translate_subtree(child, translation, nodes, writer);
    }
    return translated;
}

void validate_track(ryn::runtime::Rect track) {
    if (!std::isfinite(track.x) || !std::isfinite(track.y)
            || !std::isfinite(track.width) || !std::isfinite(track.height)
            || track.width < 0.0F || track.height < 0.0F) {
        throw std::invalid_argument("Gallery scrollbar track must be finite and non-negative");
    }
}

bool contains(ryn::runtime::Rect bounds, float x, float y) noexcept {
    return x >= bounds.x && y >= bounds.y
        && x < bounds.x + bounds.width
        && y < bounds.y + bounds.height;
}

} // namespace

bool GalleryScrollRange::set_extents(
    float viewport_extent,
    float content_extent) {
    if (!std::isfinite(viewport_extent) || viewport_extent <= 0.0F
            || !std::isfinite(content_extent) || content_extent < 0.0F) {
        throw std::invalid_argument("Gallery scroll extents must be finite and valid");
    }
    const float previous_offset = offset_;
    const bool changed = viewport_extent_ != viewport_extent
        || content_extent_ != content_extent;
    viewport_extent_ = viewport_extent;
    content_extent_ = content_extent;
    offset_ = std::clamp(offset_, 0.0F, maximum_offset());
    return changed || offset_ != previous_offset;
}

bool GalleryScrollRange::scroll_to(float offset) {
    if (!std::isfinite(offset)) {
        throw std::invalid_argument("Gallery scroll offset must be finite");
    }
    const float next = std::clamp(offset, 0.0F, maximum_offset());
    if (next == offset_) {
        return false;
    }
    offset_ = next;
    return true;
}

bool GalleryScrollRange::scroll_by(float delta) {
    if (!std::isfinite(delta)) {
        throw std::invalid_argument("Gallery scroll delta must be finite");
    }
    return scroll_to(offset_ + delta);
}

GalleryScrollRangeSnapshot GalleryScrollRange::snapshot() const noexcept {
    return {viewport_extent_, content_extent_, maximum_offset(), offset_};
}

float GalleryScrollRange::maximum_offset() const noexcept {
    return std::max(0.0F, content_extent_ - viewport_extent_);
}

GalleryScrollTranslationResult GalleryScrollTranslation::apply(
    ryn::runtime::NodeId root,
    float offset,
    ryn::runtime::NodeStore& nodes,
    ryn::runtime::DirtyQueues& dirty) {
    if (!std::isfinite(offset)) {
        throw std::invalid_argument("Gallery translation offset must be finite");
    }
    if (nodes.find(root) == nullptr) {
        return {};
    }
    if (applied_root_ == root && applied_offset_ == offset) {
        return {true, false, 0};
    }
    ryn::runtime::NodePropertyWriter writer(nodes, dirty);
    const auto translated = translate_subtree(
        root, {0.0F, -offset}, nodes, writer);
    applied_root_ = root;
    applied_offset_ = offset;
    return {true, true, translated};
}

GalleryScrollbarGeometry gallery_scrollbar_geometry(
    ryn::runtime::Rect track,
    GalleryScrollRangeSnapshot range) {
    validate_track(track);
    if (!std::isfinite(range.viewport_extent) || range.viewport_extent <= 0.0F
            || !std::isfinite(range.content_extent) || range.content_extent < 0.0F
            || !std::isfinite(range.maximum_offset) || range.maximum_offset < 0.0F
            || !std::isfinite(range.offset) || range.offset < 0.0F
            || range.offset > range.maximum_offset) {
        throw std::invalid_argument("Gallery scrollbar range must be finite and valid");
    }
    if (track.height == 0.0F || range.maximum_offset == 0.0F) {
        return {track, track, range.maximum_offset};
    }
    constexpr float minimum_thumb = 28.0F;
    const float thumb_height = std::clamp(
        track.height * range.viewport_extent / range.content_extent,
        std::min(minimum_thumb, track.height), track.height);
    const float travel = track.height - thumb_height;
    const float top = track.y + travel * range.offset / range.maximum_offset;
    return {track, {track.x, top, track.width, thumb_height}, range.maximum_offset};
}

float gallery_scrollbar_offset_for_thumb_top(
    const GalleryScrollbarGeometry& geometry,
    float thumb_top) {
    validate_track(geometry.track);
    if (!std::isfinite(thumb_top) || !std::isfinite(geometry.maximum_offset)
            || geometry.maximum_offset < 0.0F) {
        throw std::invalid_argument("Gallery scrollbar drag must be finite and valid");
    }
    const float travel = geometry.track.height - geometry.thumb.height;
    if (travel <= 0.0F || geometry.maximum_offset == 0.0F) {
        return 0.0F;
    }
    return std::clamp(
        (thumb_top - geometry.track.y) / travel,
        0.0F, 1.0F) * geometry.maximum_offset;
}

bool gallery_place_scrollbar(
    ryn::runtime::NodeId node,
    ryn::runtime::Rect target,
    ryn::runtime::NodeStore& nodes,
    ryn::runtime::DirtyQueues& dirty) {
    validate_track(target);
    const auto* current = nodes.find(node);
    if (current == nullptr) return false;
    ryn::runtime::NodePropertyWriter writer(nodes, dirty);
    return writer.set_translation(node, {
        target.x - current->bounds.x,
        target.y - current->bounds.y,
    });
}

GalleryScrollTarget gallery_scroll_target(
    float x,
    float y,
    bool narrow,
    ryn::runtime::Rect navigation_lane,
    ryn::runtime::Rect document_lane) {
    validate_track(navigation_lane);
    validate_track(document_lane);
    if (!std::isfinite(x) || !std::isfinite(y)) {
        throw std::invalid_argument("Gallery scroll pointer must be finite");
    }
    if (!narrow && contains(navigation_lane, x, y)) {
        return GalleryScrollTarget::navigation;
    }
    if (contains(document_lane, x, y)) {
        return GalleryScrollTarget::document;
    }
    return GalleryScrollTarget::none;
}

GalleryScrollbarAction GalleryScrollbarController::dispatch(
    const ryn::input::PointerInputEvent& event,
    const GalleryScrollbarGeometry& geometry,
    float viewport_extent,
    float current_offset) {
    validate_track(geometry.track);
    if (!std::isfinite(viewport_extent) || viewport_extent <= 0.0F
            || !std::isfinite(current_offset)
            || !std::isfinite(event.x) || !std::isfinite(event.y)) {
        throw std::invalid_argument("Gallery scrollbar input must be finite and valid");
    }
    if (dragging_pointer_.has_value()) {
        if (event.pointer != *dragging_pointer_) return {};
        switch (event.action) {
        case ryn::input::PointerAction::move:
            return {true, gallery_scrollbar_offset_for_thumb_top(
                geometry, event.y - grab_offset_)};
        case ryn::input::PointerAction::up:
        case ryn::input::PointerAction::cancel:
            dragging_pointer_.reset();
            return {true, std::nullopt};
        default:
            return {true, std::nullopt};
        }
    }
    if (event.action != ryn::input::PointerAction::down
            || event.button != ryn::input::PointerButton::primary
            || !contains(geometry.track, event.x, event.y)) {
        return {};
    }
    if (geometry.maximum_offset <= 0.0F) {
        return {true, std::nullopt};
    }
    if (contains(geometry.thumb, event.x, event.y)) {
        dragging_pointer_ = event.pointer;
        grab_offset_ = event.y - geometry.thumb.y;
        return {true, std::nullopt};
    }
    const float step = viewport_extent * 0.9F;
    const float direction = event.y < geometry.thumb.y ? -1.0F : 1.0F;
    return {true, std::clamp(
        current_offset + direction * step,
        0.0F, geometry.maximum_offset)};
}

} // namespace rynui::example
