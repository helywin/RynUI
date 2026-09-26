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

} // namespace rynui::example
