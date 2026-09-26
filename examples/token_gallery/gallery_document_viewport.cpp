#include "gallery_document_viewport.hpp"

#include <algorithm>
#include <cmath>
#include <limits>
#include <stdexcept>

namespace rynui::example {
namespace {

bool finite_non_negative(float value) noexcept {
    return std::isfinite(value) && value >= 0.0F;
}

void advance_generation(std::uint32_t& generation) noexcept {
    ++generation;
    if (generation == 0) {
        generation = 1;
    }
}

} // namespace

bool GalleryDocumentViewport::set_extents(
    float viewport_extent,
    float content_extent) {
    const bool changed = scroll_.set_extents(viewport_extent, content_extent);
    if (changed) {
        ++diagnostics_.extent_updates;
    }
    return changed;
}

bool GalleryDocumentViewport::scroll_to(float offset) {
    const bool changed = scroll_.scroll_to(offset);
    if (changed) ++diagnostics_.scroll_updates;
    return changed;
}

bool GalleryDocumentViewport::scroll_by(float delta) {
    const bool changed = scroll_.scroll_by(delta);
    if (changed) ++diagnostics_.scroll_updates;
    return changed;
}

bool GalleryDocumentViewport::replace_anchors(
    std::span<const float> offsets) {
    if (offsets.size() != section_count) {
        throw std::invalid_argument(
            "Gallery document requires one anchor per section");
    }
    std::array<float, section_count> next{};
    for (std::size_t index = 0; index < offsets.size(); ++index) {
        if (!finite_non_negative(offsets[index])
                || (index != 0 && offsets[index] < offsets[index - 1])) {
            throw std::invalid_argument(
                "Gallery document anchors must be finite and ordered");
        }
        next[index] = offsets[index];
    }
    bool unchanged = true;
    for (std::size_t index = 0; index < section_count; ++index) {
        unchanged = unchanged
            && anchor_present_[index] && anchors_[index] == next[index];
    }
    if (unchanged) {
        return false;
    }
    std::copy(next.begin(), next.end(), anchors_.begin());
    std::fill_n(anchor_present_.begin(), section_count, true);
    advance_generation(anchor_generation_);
    ++diagnostics_.anchor_updates;
    return true;
}

bool GalleryDocumentViewport::replace_category_anchors(
    std::span<const float> offsets) {
    if (offsets.size() != category_count) {
        throw std::invalid_argument(
            "Gallery document requires one anchor per component category");
    }
    bool unchanged = true;
    for (std::size_t index = 0; index < offsets.size(); ++index) {
        if (!finite_non_negative(offsets[index])
                || (index != 0 && offsets[index] < offsets[index - 1])) {
            throw std::invalid_argument(
                "Gallery category anchors must be finite and ordered");
        }
        const auto target = section_count + index;
        unchanged = unchanged
            && anchor_present_[target] && anchors_[target] == offsets[index];
    }
    if (unchanged) {
        return false;
    }
    for (std::size_t index = 0; index < offsets.size(); ++index) {
        anchors_[section_count + index] = offsets[index];
        anchor_present_[section_count + index] = true;
    }
    advance_generation(anchor_generation_);
    ++diagnostics_.anchor_updates;
    return true;
}

std::optional<GalleryDocumentAnchorId>
GalleryDocumentViewport::category_anchor(
    AntDesignGalleryCategory category) const noexcept {
    const auto index = section_count + static_cast<std::size_t>(category);
    if (index >= anchor_count || !anchor_present_[index]) {
        return std::nullopt;
    }
    return GalleryDocumentAnchorId{
        static_cast<std::uint32_t>(index),
        anchor_generation_,
    };
}

std::optional<GalleryDocumentAnchorId> GalleryDocumentViewport::anchor(
    GalleryDocumentSectionKind section) const noexcept {
    const auto index = section_index(section);
    if (!anchor_present_[index]) {
        return std::nullopt;
    }
    return GalleryDocumentAnchorId{
        static_cast<std::uint32_t>(index),
        anchor_generation_,
    };
}

bool GalleryDocumentViewport::jump_to(GalleryDocumentAnchorId value) {
    if (!value.valid() || value.generation != anchor_generation_
            || value.index >= anchor_count
            || !anchor_present_[value.index]) {
        return false;
    }
    const bool changed = scroll_to(anchors_[value.index]);
    if (changed) {
        ++diagnostics_.navigation_jumps;
    }
    return changed;
}

GalleryDocumentResizeAnchor
GalleryDocumentViewport::capture_resize_anchor() const {
    const auto section = current_section();
    const auto index = section_index(section);
    const float offset = scroll_.snapshot().offset;
    return {
        section,
        anchor_present_[index] ? offset - anchors_[index] : offset,
    };
}

bool GalleryDocumentViewport::restore_resize_anchor(
    const GalleryDocumentResizeAnchor& value) {
    if (!std::isfinite(value.distance)) {
        throw std::invalid_argument(
            "Gallery resize anchor distance must be finite");
    }
    const auto index = section_index(value.section);
    if (!anchor_present_[index]) {
        return false;
    }
    return scroll_to(anchors_[index] + value.distance);
}

bool GalleryDocumentViewport::apply_subtree_translation(
    ryn::runtime::NodeId root,
    ryn::runtime::NodeStore& nodes,
    ryn::runtime::DirtyQueues& dirty) const {
    const auto result = translation_.apply(
        root, scroll_.snapshot().offset, nodes, dirty);
    if (result.changed) {
        ++diagnostics_.translation_passes;
        diagnostics_.translated_nodes += result.translated_nodes;
    }
    return result.valid;
}

GalleryDocumentViewportSnapshot
GalleryDocumentViewport::snapshot() const noexcept {
    const auto range = scroll_.snapshot();
    return {
        range.viewport_extent,
        range.content_extent,
        range.maximum_offset,
        range.offset,
        current_section(),
        anchor_generation_,
    };
}

const GalleryDocumentViewportDiagnostics&
GalleryDocumentViewport::diagnostics() const noexcept {
    return diagnostics_;
}

std::size_t GalleryDocumentViewport::section_index(
    GalleryDocumentSectionKind section) noexcept {
    return static_cast<std::size_t>(section);
}

GalleryDocumentSectionKind
GalleryDocumentViewport::current_section() const noexcept {
    constexpr float bottom_section_tolerance = 32.0F;
    const auto range = scroll_.snapshot();
    if (range.maximum_offset > 0.0F
            && range.offset >= std::max(
                0.0F, range.maximum_offset - bottom_section_tolerance)) {
        for (std::size_t index = section_count; index > 0; --index) {
            if (anchor_present_[index - 1]) {
                return static_cast<GalleryDocumentSectionKind>(index - 1);
            }
        }
    }
    std::size_t current = 0;
    for (std::size_t index = 0; index < section_count; ++index) {
        if (anchor_present_[index] && anchors_[index] <= range.offset) {
            current = index;
        }
    }
    return static_cast<GalleryDocumentSectionKind>(current);
}

} // namespace rynui::example
