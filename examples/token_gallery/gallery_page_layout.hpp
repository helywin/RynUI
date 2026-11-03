#pragma once

#include "runtime/geometry.hpp"

#include <algorithm>
#include <cmath>
#include <stdexcept>

namespace rynui::example {

struct GalleryPageLayout final {
    bool narrow{};
    bool wide{};
    float header_height{};
    float gap{};
    float navigation_width{};
    float document_padding{};
    float document_width{};
    ryn::runtime::Rect header;
    ryn::runtime::Rect navigation;
    ryn::runtime::Rect document;
    ryn::runtime::Rect content;
    ryn::runtime::Rect toc;
    ryn::runtime::Rect navigation_track;
    ryn::runtime::Rect document_track;
};

inline GalleryPageLayout gallery_page_layout(ryn::runtime::Size viewport) {
    if (!std::isfinite(viewport.width) || !std::isfinite(viewport.height) || viewport.width <= 0 ||
        viewport.height <= 0) {
        throw std::invalid_argument("Gallery page viewport must be finite and positive");
    }
    GalleryPageLayout result;
    result.narrow = viewport.width < 640;
    result.wide = viewport.width >= 1200;
    result.header_height = result.narrow ? 96.0F : 60.0F;
    result.header = {0, 0, viewport.width, result.header_height};
    result.gap = result.wide ? 24.0F : 16.0F;
    const float outer = viewport.width >= 960 ? 24.0F : 16.0F;
    const float top = result.header_height + 16;
    const float height = std::max(1.0F, viewport.height - top - 16);
    result.navigation_width = result.narrow ? 0 : result.wide ? 240.0F : std::clamp(viewport.width / 4, 184.0F, 216.0F);
    result.navigation = {outer, top, result.navigation_width, height};
    result.toc =
        result.wide ? ryn::runtime::Rect{viewport.width - outer - 160, top, 160, height} : ryn::runtime::Rect{};
    const float left = result.narrow ? 0 : outer + result.navigation_width + result.gap;
    const float right = result.wide ? result.toc.x - result.gap : viewport.width - (result.narrow ? 0 : outer);
    result.document = {left, top, std::max(1.0F, right - left), height};
    result.document_padding = result.narrow ? 16.0F : result.wide ? 32.0F : viewport.width >= 960 ? 24.0F : 20.0F;
    result.document_width = std::min(920.0F, std::max(1.0F, result.document.width - result.document_padding * 2));
    result.content = {left + (result.document.width - result.document_width) / 2, top + 8, result.document_width,
                      std::max(1.0F, height - 16)};
    result.navigation_track = {result.navigation.x + std::max(0.0F, result.navigation.width - 6), top, 6, height};
    result.document_track = {result.document.x + result.document.width - 6, top, 6, height};
    return result;
}

} // namespace rynui::example
