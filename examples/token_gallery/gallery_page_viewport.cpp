#include "gallery_page_viewport.hpp"

#include <algorithm>
#include <cmath>
#include <stdexcept>

namespace rynui::example {

bool GalleryPageViewport::set_page(std::uint64_t generation, ryn::runtime::NodeId root,
                                   std::span<const GalleryPageAnchorNode> anchors) {
    if (generation == 0 || !root.valid()) {
        throw std::invalid_argument("Gallery page anchors require a live generation and root");
    }
    for (std::size_t i = 0; i < anchors.size(); ++i) {
        if (anchors[i].identity.empty() || !anchors[i].node.valid()) {
            throw std::invalid_argument("Gallery page anchor has no identity or node");
        }
        for (std::size_t j = 0; j < i; ++j) {
            if (anchors[j].identity == anchors[i].identity) {
                throw std::invalid_argument("Gallery page anchor identity is duplicated");
            }
        }
    }
    if (generation_ == generation && root_ == root && std::ranges::equal(anchors_, anchors)) {
        return false;
    }
    if (generation_ != 0 && generation <= generation_) {
        throw std::invalid_argument("Gallery page anchor ownership changed without a new generation");
    }
    generation_ = generation;
    root_ = root;
    anchors_.assign(anchors.begin(), anchors.end());
    offsets_.assign(anchors_.size(), 0);
    static_cast<void>(range_.scroll_to(0));
    translation_.invalidate();
    ++work_.pages;
    return true;
}

bool GalleryPageViewport::refresh(float viewport_extent, const ryn::runtime::NodeStore& nodes) {
    const auto& root = nodes.require(root_);
    if (!std::isfinite(root.bounds.height) || root.bounds.height < 0 || !std::isfinite(root.bounds.y)) {
        throw std::invalid_argument("Gallery page extent is not finite and nonnegative");
    }
    std::vector<float> next_offsets;
    next_offsets.reserve(anchors_.size());
    for (const auto& anchor : anchors_) {
        const auto& node = nodes.require(anchor.node);
        auto ancestor = std::optional{anchor.node};
        while (ancestor && *ancestor != root_) {
            ancestor = nodes.require(*ancestor).parent;
        }
        if (!ancestor || !std::isfinite(node.bounds.y)) {
            throw std::invalid_argument("Gallery page anchor does not belong to its live root");
        }
        next_offsets.push_back(std::max(0.0F, node.bounds.y - root.bounds.y));
    }
    bool changed = range_.set_extents(viewport_extent, root.bounds.height);
    for (std::size_t i = 0; i < next_offsets.size(); ++i) {
        if (next_offsets[i] != offsets_[i]) {
            offsets_[i] = next_offsets[i];
            changed = true;
            ++work_.anchor_updates;
        }
    }
    return changed;
}

bool GalleryPageViewport::scroll_to(float offset) {
    return range_.scroll_to(offset);
}

bool GalleryPageViewport::scroll_by(float delta) {
    return range_.scroll_by(delta);
}

GalleryScrollRangeSnapshot GalleryPageViewport::snapshot() const noexcept {
    return range_.snapshot();
}

std::uint64_t GalleryPageViewport::generation() const noexcept {
    return generation_;
}

std::optional<GalleryPageAnchorHandle> GalleryPageViewport::anchor(std::string_view identity) const noexcept {
    const auto found = std::ranges::find(anchors_, identity, &GalleryPageAnchorNode::identity);
    return found == anchors_.end() ? std::nullopt
                                   : std::optional{GalleryPageAnchorHandle{
                                         generation_, static_cast<std::size_t>(found - anchors_.begin())}};
}

bool GalleryPageViewport::jump(GalleryPageAnchorHandle handle) {
    if (handle.generation != generation_ || handle.generation == 0 || handle.index >= offsets_.size()) {
        return false;
    }
    return range_.scroll_to(offsets_[handle.index]);
}

GalleryPageAnchor GalleryPageViewport::capture() const {
    const auto offset = range_.snapshot().offset;
    std::optional<std::size_t> nearest;
    for (std::size_t i = 0; i < offsets_.size(); ++i) {
        if (offsets_[i] <= offset && (!nearest || offsets_[i] > offsets_[*nearest])) {
            nearest = i;
        }
    }
    return nearest ? GalleryPageAnchor{anchors_[*nearest].identity, offset - offsets_[*nearest]} : GalleryPageAnchor{};
}

bool GalleryPageViewport::restore(const GalleryPageAnchor& saved) {
    const auto handle = anchor(saved.identity);
    return range_.scroll_to(handle ? offsets_[handle->index] + saved.distance : 0);
}

GalleryScrollTranslationResult GalleryPageViewport::apply(ryn::runtime::NodeStore& nodes,
                                                          ryn::runtime::DirtyQueues& dirty) {
    if (applied_topology_ != nodes.topology_revision()) {
        translation_.invalidate();
    }
    const auto result = translation_.apply(root_, range_.snapshot().offset, nodes, dirty);
    applied_topology_ = nodes.topology_revision();
    if (result.changed) {
        ++work_.translation_passes;
        work_.translated_nodes += result.translated_nodes;
    }
    return result;
}

const GalleryPageViewportWork& GalleryPageViewport::work() const noexcept {
    return work_;
}

} // namespace rynui::example
