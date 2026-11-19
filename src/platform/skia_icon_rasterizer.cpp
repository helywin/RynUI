#include "platform/skia_icon_rasterizer.hpp"

#include "include/core/SkBitmap.h"
#include "include/core/SkCanvas.h"
#include "include/core/SkPaint.h"
#include "include/core/SkPath.h"
#include "include/utils/SkParsePath.h"

#include <algorithm>
#include <cmath>
#include <span>
#include <utility>

namespace ryn::detail {
namespace {

struct SvgIconPath {
    const char* data;
    bool even_odd;
};

struct SvgIconLayer {
    float view_x;
    float view_y;
    float view_width;
    bool composited;
    std::uint16_t first_path;
    std::uint16_t path_count;
};

#include "icons/ant_design_icon_svg_layers.inc"

[[nodiscard]] font::FontError error(const char* message) {
    return {font::FontErrorStage::rasterization, font::FontErrorKind::rasterization_failed, {}, {}, message};
}

[[nodiscard]] bool rasterize_layer(const SvgIconLayer& layer, SkBitmap& mask, float em, float x, float y) {
    SkCanvas canvas(mask);
    canvas.clear(SK_ColorTRANSPARENT);
    canvas.clipRect(SkRect::MakeXYWH(x, y, em, em), SkClipOp::kIntersect, true);
    canvas.translate(x, y);
    canvas.scale(em / layer.view_width, em / layer.view_width);
    canvas.translate(-layer.view_x, -layer.view_y);
    SkPaint paint;
    paint.setAntiAlias(true);
    paint.setColor(SK_ColorWHITE);
    for (const auto& source : std::span{svg_icon_paths}.subspan(layer.first_path, layer.path_count)) {
        SkPath path;
        if (!SkParsePath::FromSVGString(source.data, &path)) {
            return false;
        }
        path.setFillType(source.even_odd ? SkPathFillType::kEvenOdd : SkPathFillType::kWinding);
        canvas.drawPath(path, paint);
    }
    return true;
}

[[nodiscard]] std::uint8_t sample(const SkBitmap& mask, int x, int y) noexcept {
    return x >= 0 && y >= 0 && x < mask.width() && y < mask.height() ? *mask.getAddr8(x, y) : 0;
}

} // namespace

bool has_bundled_svg_glyph(std::uint32_t glyph) noexcept {
    return glyph > 0 && glyph <= std::size(svg_icon_layers);
}

font::FontError rasterize_bundled_svg_glyph(std::uint32_t glyph, float physical_em, float phase_x, float phase_y,
                                            font::GlyphBitmap& result) {
    if (!has_bundled_svg_glyph(glyph) || !(physical_em > 0 && physical_em <= 2048) || !(phase_x >= 0 && phase_x < 1) ||
        !(phase_y >= 0 && phase_y < 1)) {
        return error("Bundled SVG glyph or physical placement is invalid.");
    }
    const auto& layer = svg_icon_layers[glyph - 1];
    const int width = static_cast<int>(std::ceil(physical_em + phase_x));
    const int height = static_cast<int>(std::ceil(physical_em + phase_y));
    SkBitmap mask;
    if (!mask.tryAllocPixels(SkImageInfo::MakeA8(width, height))) {
        return error("Cannot allocate the bundled SVG glyph mask.");
    }
    const bool filter = layer.composited && (phase_x != 0 || phase_y != 0);
    if (!rasterize_layer(layer, mask, physical_em, filter ? 0.0F : phase_x, filter ? 0.0F : phase_y)) {
        return error("Cannot decode a locked bundled SVG path.");
    }
    font::GlyphBitmap bitmap;
    bitmap.width = static_cast<std::uint32_t>(width);
    bitmap.height = static_cast<std::uint32_t>(height);
    bitmap.row_stride = bitmap.width;
    bitmap.advance_x = physical_em;
    bitmap.svg_viewport_origin = true;
    bitmap.coverage.resize(static_cast<std::size_t>(width) * height);
    for (int y = 0; y < height; ++y) {
        auto destination = std::span{bitmap.coverage}.subspan(static_cast<std::size_t>(y) * width, width);
        if (!filter) {
            std::copy_n(mask.getAddr8(0, y), width, destination.begin());
            continue;
        }
        for (int x = 0; x < width; ++x) {
            const float same_row = std::lerp(sample(mask, x, y), sample(mask, x - 1, y), phase_x);
            const float previous_row = std::lerp(sample(mask, x, y - 1), sample(mask, x - 1, y - 1), phase_x);
            destination[x] = static_cast<std::uint8_t>(std::lround(std::lerp(same_row, previous_row, phase_y)));
        }
    }
    result = std::move(bitmap);
    return {};
}

} // namespace ryn::detail
