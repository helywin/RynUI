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
    if (!std::isfinite(viewport_extent) || viewport_extent <= 0.0F
            || !finite_non_negative(content_extent)) {
        throw std::invalid_argument(
            "Gallery document extents must be finite and valid");
    }
    const float previous_offset = offset_;
    const bool changed = viewport_extent_ != viewport_extent
        || content_extent_ != content_extent;
    viewport_extent_ = viewport_extent;
    content_extent_ = content_extent;
    offset_ = clamped(offset_);
    if (changed || offset_ != previous_offset) {
        ++diagnostics_.extent_updates;
    }
    return changed || offset_ != previous_offset;
}

bool GalleryDocumentViewport::scroll_to(float offset) {
    if (!std::isfinite(offset)) {
        throw std::invalid_argument(
            "Gallery document offset must be finite");
    }
    const float next = clamped(offset);
    if (next == offset_) {
        return false;
    }
    offset_ = next;
    ++diagnostics_.scroll_updates;
    return true;
}

bool GalleryDocumentViewport::scroll_by(float delta) {
    if (!std::isfinite(delta)) {
        throw std::invalid_argument(
            "Gallery document scroll delta must be finite");
    }
    return scroll_to(offset_ + delta);
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
    return {
        section,
        anchor_present_[index] ? offset_ - anchors_[index] : offset_,
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
    if (nodes.find(root) == nullptr) {
        return false;
    }
    if (translation_applied_ && applied_root_ == root
            && applied_offset_ == offset_) {
        return true;
    }
    ryn::runtime::NodePropertyWriter writer(nodes, dirty);
    const auto translated = translate_subtree(
        root, {0.0F, -offset_}, nodes, writer);
    applied_root_ = root;
    applied_offset_ = offset_;
    translation_applied_ = true;
    ++diagnostics_.translation_passes;
    diagnostics_.translated_nodes += translated;
    return true;
}

GalleryDocumentViewportSnapshot
GalleryDocumentViewport::snapshot() const noexcept {
    return {
        viewport_extent_,
        content_extent_,
        maximum_offset(),
        offset_,
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

float GalleryDocumentViewport::maximum_offset() const noexcept {
    return std::max(0.0F, content_extent_ - viewport_extent_);
}

float GalleryDocumentViewport::clamped(float value) const noexcept {
    return std::clamp(value, 0.0F, maximum_offset());
}

GalleryDocumentSectionKind
GalleryDocumentViewport::current_section() const noexcept {
    constexpr float bottom_section_tolerance = 32.0F;
    if (maximum_offset() > 0.0F
            && offset_ >= std::max(
                0.0F, maximum_offset() - bottom_section_tolerance)) {
        for (std::size_t index = section_count; index > 0; --index) {
            if (anchor_present_[index - 1]) {
                return static_cast<GalleryDocumentSectionKind>(index - 1);
            }
        }
    }
    std::size_t current = 0;
    for (std::size_t index = 0; index < section_count; ++index) {
        if (anchor_present_[index] && anchors_[index] <= offset_) {
            current = index;
        }
    }
    return static_cast<GalleryDocumentSectionKind>(current);
}

std::size_t GalleryDocumentViewport::translate_subtree(
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

} // namespace rynui::example
