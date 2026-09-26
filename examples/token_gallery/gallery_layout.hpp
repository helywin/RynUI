#pragma once

#include "runtime/geometry.hpp"

#include <algorithm>
#include <cmath>
#include <stdexcept>

namespace rynui::example {

// Shared by retained layout, scroll hit regions, and scrollbar placement.
struct GalleryLayoutMetrics final {
    static constexpr float left = 24.0F;
    static constexpr float top = 20.0F;
    static constexpr float header_height = 56.0F;
    static constexpr float body_gap = 24.0F;
    static constexpr float body_top = top + header_height + body_gap;
    static constexpr float column_gap = 40.0F;
    static constexpr float scrollbar_width = 8.0F;
    static constexpr float scrollbar_gutter = 24.0F;
    static constexpr float card_gap = 16.0F;
    bool narrow{};
    float gallery_width{};
    float navigation_width{};
    float document_width{};
    float cell_width{};
    ryn::runtime::Rect navigation_lane;
    ryn::runtime::Rect document_lane;
    ryn::runtime::Rect navigation_track;
    ryn::runtime::Rect document_track;
};

inline GalleryLayoutMetrics gallery_layout_metrics(ryn::runtime::Size viewport) {
    if (!std::isfinite(viewport.width) || !std::isfinite(viewport.height)
            || viewport.width <= 0.0F || viewport.height <= 0.0F) {
        throw std::invalid_argument("Gallery viewport must be finite and positive");
    }
    GalleryLayoutMetrics result;
    result.gallery_width = std::max(256.0F, viewport.width - 2.0F * result.left);
    result.narrow = viewport.width < 960.0F;
    result.navigation_width = result.narrow ? result.gallery_width : 216.0F;
    const float document_left = result.narrow ? result.left
        : result.left + result.navigation_width + result.column_gap;
    const float lane_width = result.narrow ? result.gallery_width
        : result.gallery_width - result.navigation_width - result.column_gap;
    result.document_width = std::min(960.0F, lane_width - result.scrollbar_gutter);
    const int columns = result.document_width >= 900.0F ? 3
        : result.document_width >= 560.0F ? 2 : 1;
    result.cell_width = (result.document_width - (columns - 1) * result.card_gap) / columns;
    const float height = std::max(1.0F, viewport.height - result.body_top - 16.0F);
    result.navigation_lane = {result.left, result.body_top, result.navigation_width, height};
    result.document_lane = {document_left, result.body_top, lane_width, height};
    result.navigation_track = {
        result.left + result.navigation_width - result.scrollbar_width,
        result.body_top, result.scrollbar_width, height};
    result.document_track = {
        document_left + lane_width - result.scrollbar_width,
        result.body_top, result.scrollbar_width, height};
    return result;
}

} // namespace rynui::example
