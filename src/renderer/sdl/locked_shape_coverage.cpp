#include "renderer/sdl/locked_shape_coverage.hpp"

#include "include/core/SkBitmap.h"
#include "include/core/SkBlurTypes.h"
#include "include/core/SkCanvas.h"
#include "include/core/SkColorFilter.h"
#include "include/core/SkColorSpace.h"
#include "include/core/SkMaskFilter.h"
#include "include/core/SkPaint.h"
#include "include/core/SkRRect.h"

#include <algorithm>
#include <cmath>
#include <new>

namespace ryn::detail {
namespace {

constexpr std::uint32_t maximum_mask_extent = 512;

[[nodiscard]] SkRRect rounded_rect(SkRect rect, float radius) {
    return SkRRect::MakeRectXY(rect, std::max(0.0F, radius), std::max(0.0F, radius));
}

} // namespace

std::optional<LockedShapeCoverage> rasterize_locked_shape(const LockedShapeGeometry& shape,
                                                          std::uint32_t viewport_width, std::uint32_t viewport_height) {
    const auto box = shape.bounds;
    if (box.width <= 0 || box.height <= 0 || !viewport_width || !viewport_height) {
        return {};
    }
    const auto rect = SkRect::MakeXYWH(box.x, box.y, box.width, box.height);
    auto draw_bounds = rect;
    if (shape.kind == LockedShapeKind::outline) {
        const auto outset = shape.outline_offset + shape.outline_width;
        draw_bounds.outset(outset, outset);
    } else if (shape.kind == LockedShapeKind::exterior_shadow) {
        draw_bounds.offset(shape.shadow_offset.x, shape.shadow_offset.y);
        const auto outset = shape.shadow_spread + shape.shadow_blur * 2;
        draw_bounds.outset(outset, outset);
    }
    const auto left = std::clamp(std::floor(draw_bounds.left() - 1), 0.0F, float(viewport_width));
    const auto top = std::clamp(std::floor(draw_bounds.top() - 1), 0.0F, float(viewport_height));
    const auto right = std::clamp(std::ceil(draw_bounds.right() + 1), 0.0F, float(viewport_width));
    const auto bottom = std::clamp(std::ceil(draw_bounds.bottom() + 1), 0.0F, float(viewport_height));
    if (!(right > left && bottom > top) || right - left > maximum_mask_extent || bottom - top > maximum_mask_extent) {
        return {};
    }
    const auto width = static_cast<std::uint32_t>(right - left);
    const auto height = static_cast<std::uint32_t>(bottom - top);
    const bool shadow = shape.kind == LockedShapeKind::exterior_shadow || shape.kind == LockedShapeKind::hard_shadow;
    const bool dark = shadow || shape.dark_ink;
    SkBitmap bitmap;
    if (!bitmap.tryAllocPixels(SkImageInfo::MakeN32Premul(static_cast<int>(width), static_cast<int>(height)))) {
        throw std::bad_alloc();
    }
    SkCanvas canvas(bitmap);
    canvas.clear(dark ? SK_ColorWHITE : SK_ColorBLACK);
    canvas.translate(-left, -top);
    SkPaint paint;
    paint.setAntiAlias(true);
    paint.setColor(dark ? SK_ColorBLACK : SK_ColorWHITE);
    if (shadow) {
        paint.setColorFilter(
            SkColorFilters::Blend({0, 0, 0, shape.shadow_alpha}, SkColorSpace::MakeSRGB(), SkBlendMode::kSrcIn));
    }
    if (shape.kind == LockedShapeKind::fill || shape.kind == LockedShapeKind::hard_shadow) {
        canvas.drawRRect(rounded_rect(rect, shape.radius), paint);
    } else if (shape.kind == LockedShapeKind::outline) {
        auto center = rect;
        const auto outset = shape.outline_offset + shape.outline_width * 0.5F;
        center.outset(outset, outset);
        paint.setStyle(SkPaint::kStroke_Style);
        paint.setStrokeWidth(shape.outline_width);
        canvas.drawRRect(rounded_rect(center, shape.radius > 0 ? shape.radius + outset : 0), paint);
    } else {
        auto clip = rounded_rect(rect, shape.radius);
        clip.inset(shape.shadow_caster_inset, shape.shadow_caster_inset);
        canvas.clipRRect(clip, SkClipOp::kDifference, true);
        canvas.translate(shape.shadow_offset.x, shape.shadow_offset.y);
        auto caster = rect;
        caster.outset(shape.shadow_spread, shape.shadow_spread);
        paint.setMaskFilter(SkMaskFilter::MakeBlur(kNormal_SkBlurStyle, shape.shadow_blur * 0.5F));
        canvas.drawRRect(rounded_rect(caster, shape.radius + shape.shadow_spread), paint);
    }
    LockedShapeCoverage result{static_cast<std::uint32_t>(left), static_cast<std::uint32_t>(top), width, height, {}};
    result.pixels.resize(static_cast<std::size_t>(width) * height);
    for (std::uint32_t y = 0; y < height; ++y) {
        for (std::uint32_t x = 0; x < width; ++x) {
            const auto color = SkColorGetR(bitmap.getColor(static_cast<int>(x), static_cast<int>(y)));
            result.pixels[static_cast<std::size_t>(y) * width + x] =
                static_cast<std::uint8_t>(dark ? 255 - color : color);
        }
    }
    return result;
}

} // namespace ryn::detail
