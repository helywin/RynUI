#include "demo_differential_shape_blend.hpp"

#include "include/core/SkBitmap.h"
#include "include/core/SkCanvas.h"
#include "include/core/SkColorFilter.h"
#include "include/core/SkColorSpace.h"
#include "include/core/SkPaint.h"
#include "include/core/SkRRect.h"

#include <cstdlib>
#include <stdexcept>

namespace rynui::example {
namespace {

using ryn::detail::LockedShapeCoverage;
using ryn::detail::LockedShapeGeometry;

SkRRect rounded(const LockedShapeGeometry& geometry, float inset = 0, ryn::runtime::Point offset = {}) {
    const auto& box = geometry.bounds;
    auto rect = SkRect::MakeXYWH(box.x + offset.x, box.y + offset.y, box.width, box.height);
    rect.inset(inset, inset);
    return SkRRect::MakeRectXY(rect, geometry.radius - inset, geometry.radius - inset);
}

SkColor rgb(const std::array<std::uint8_t, 3>& color) {
    return SkColorSetRGB(color[0], color[1], color[2]);
}

void draw_web(SkBitmap& bitmap, const ShapeBlendSource& source) {
    SkCanvas canvas(bitmap);
    canvas.clear(SK_ColorWHITE);
    SkPaint paint;
    paint.setAntiAlias(true);
    if (source.caster.shadow_alpha > 0) {
        canvas.save();
        canvas.clipRRect(rounded(source.caster, source.caster.shadow_caster_inset), SkClipOp::kDifference, true);
        paint.setColor(SK_ColorBLACK);
        const auto& c = source.shadow_color;
        paint.setColorFilter(
            SkColorFilters::Blend({c[0] / 255.0F, c[1] / 255.0F, c[2] / 255.0F, source.caster.shadow_alpha},
                                  SkColorSpace::MakeSRGB(), SkBlendMode::kSrcIn));
        canvas.drawRRect(rounded(source.caster, -source.caster.shadow_spread, source.caster.shadow_offset), paint);
        canvas.restore();
        paint.setColorFilter(nullptr);
    }
    paint.setColor(rgb(source.fill_color));
    canvas.drawRRect(rounded(source.fill), paint);
    if (source.border) {
        paint.setColor(rgb(source.border_color));
        const auto gap = source.border->outline_offset;
        if (gap == 0 && source.caster.shadow_alpha > 0 && !source.split_border) {
            canvas.drawDRRect(rounded(source.caster), rounded(*source.border), paint);
        } else {
            const auto width = source.border->outline_width;
            paint.setStyle(SkPaint::kStroke_Style);
            paint.setStrokeWidth(width);
            canvas.drawRRect(rounded(source.caster, gap > 0 ? -(gap + width * 0.5F) : width * 0.5F), paint);
        }
    }
}

unsigned coverage(const LockedShapeCoverage& mask, std::uint32_t x, std::uint32_t y) {
    if (x < mask.left || y < mask.top || x - mask.left >= mask.width || y - mask.top >= mask.height) {
        return 0;
    }
    return mask.pixels[static_cast<std::size_t>(y - mask.top) * mask.width + x - mask.left];
}

unsigned over_rgb8(unsigned source, unsigned dest, unsigned alpha) {
    return (source * alpha + dest * (255 - alpha) + 127) / 255;
}

unsigned over_rgb8_split(unsigned source, unsigned dest, unsigned alpha) {
    return (source * alpha + 127) / 255 + (dest * (255 - alpha) + 127) / 255;
}

} // namespace

std::vector<ShapeBlendSample> shape_blend_samples(const ShapeBlendSource& source) {
    auto caster = source.caster;
    if (caster.kind == ryn::detail::LockedShapeKind::hard_shadow) {
        caster.bounds.x += caster.shadow_offset.x;
        caster.bounds.y += caster.shadow_offset.y;
    }
    const auto shadow =
        caster.shadow_alpha > 0 ? rasterize_locked_shape(caster, source.width, source.height) : std::nullopt;
    const auto fill = rasterize_locked_shape(source.fill, source.width, source.height);
    const auto border =
        source.border ? rasterize_locked_shape(*source.border, source.width, source.height) : std::nullopt;
    if ((caster.shadow_alpha > 0 && !shadow) || !fill || (source.border && !border)) {
        throw std::runtime_error("rounded blend source produced no bounded raster");
    }
    SkBitmap bitmap;
    if (!bitmap.tryAllocPixels(SkImageInfo::MakeN32Premul(static_cast<int>(source.width),
                                                          static_cast<int>(source.height), SkColorSpace::MakeSRGB()))) {
        throw std::runtime_error("rounded blend source could not allocate its bounded raster");
    }
    draw_web(bitmap, source);
    const bool focus = source.border && source.border->outline_offset > 0;
    std::vector<ShapeBlendSample> different;
    for (std::uint32_t y = 0; y < source.height; ++y) {
        for (std::uint32_t x = 0; x < source.width; ++x) {
            const auto pixel = bitmap.getColor(static_cast<int>(x), static_cast<int>(y));
            ShapeBlendSample sample{x,
                                    y,
                                    {static_cast<std::uint8_t>(SkColorGetR(pixel)),
                                     static_cast<std::uint8_t>(SkColorGetG(pixel)),
                                     static_cast<std::uint8_t>(SkColorGetB(pixel))},
                                    {}};
            const auto shadow_alpha = shadow ? coverage(*shadow, x, y) : 0;
            const auto fill_alpha = coverage(*fill, x, y);
            const auto border_alpha = border ? coverage(*border, x, y) : 0;
            bool adjacent_codes = true;
            for (std::size_t i = 0; i < sample.native.size(); ++i) {
                const auto shadowed = over_rgb8(source.shadow_color[i], 255, shadow_alpha);
                const auto filled = over_rgb8(source.fill_color[i], shadowed, fill_alpha);
                const auto border_over_fill = source.split_border
                                                  ? over_rgb8_split(source.border_color[i], filled, border_alpha)
                                                  : over_rgb8(source.border_color[i], filled, border_alpha);
                const auto output =
                    focus ? over_rgb8(source.fill_color[i], over_rgb8(source.border_color[i], shadowed, border_alpha),
                                      fill_alpha)
                          : border_over_fill;
                sample.native[i] = static_cast<std::uint8_t>(output);
                adjacent_codes &= std::abs(int(sample.web[i]) - int(sample.native[i])) <= 1;
            }
            if (sample.web != sample.native && ((source.border && caster.shadow_alpha > 0) || adjacent_codes)) {
                different.push_back(sample);
            }
        }
    }
    return different;
}

} // namespace rynui::example
