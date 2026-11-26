#include "demo_differential_shape_blend.hpp"

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
    canvas.clear(source.isolated_opacity > 0 ? SK_ColorTRANSPARENT : SK_ColorWHITE);
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
    if (source.isolated_opacity > 0) {
        return;
    }
    paint.setColor(rgb(source.fill_color));
    const bool translated_stroke = source.border && source.border->outline_offset == 0 &&
                                   source.caster.kind == ryn::detail::LockedShapeKind::hard_shadow;
    // Blink fills the stroke centerline before drawing its border. The native
    // translated-shadow path fills the full outer box.
    canvas.drawRRect(
        translated_stroke ? rounded(source.caster, source.border->outline_width * .5F) : rounded(source.fill), paint);
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

bool half_code(double value) {
    return std::abs(value - std::floor(value) - .5) <= 0x1p-14;
}

} // namespace

std::vector<ShapeBlendSample> shape_blend_samples(const ShapeBlendSource& source) {
    auto caster = source.caster;
    auto fill_geometry = source.fill;
    const bool isolated = source.isolated_opacity > 0;
    const std::uint32_t native_pad = isolated ? 1 : 0;
    caster.bounds.x += static_cast<float>(native_pad);
    caster.bounds.y += static_cast<float>(native_pad);
    fill_geometry.bounds.x += static_cast<float>(native_pad);
    fill_geometry.bounds.y += static_cast<float>(native_pad);
    if (caster.kind == ryn::detail::LockedShapeKind::hard_shadow) {
        caster.bounds.x += caster.shadow_offset.x;
        caster.bounds.y += caster.shadow_offset.y;
    }
    const auto native_width = source.width + 2 * native_pad;
    const auto native_height = source.height + 2 * native_pad;
    const auto shadow =
        caster.shadow_alpha > 0 ? rasterize_locked_shape(caster, native_width, native_height) : std::nullopt;
    const auto fill = rasterize_locked_shape(fill_geometry, native_width, native_height);
    const auto border =
        source.border ? rasterize_locked_shape(*source.border, source.width, source.height) : std::nullopt;
    if ((caster.shadow_alpha > 0 && !shadow) || !fill || (source.border && !border)) {
        throw std::runtime_error("rounded blend source produced no bounded raster");
    }
    SkBitmap bitmap;
    const auto web_info = SkImageInfo::MakeN32Premul(static_cast<int>(source.width), static_cast<int>(source.height),
                                                     SkColorSpace::MakeSRGB());
    if (!bitmap.tryAllocPixels(web_info)) {
        throw std::runtime_error("rounded blend source could not allocate its bounded raster");
    }
    draw_web(bitmap, source);
    SkBitmap web_fill;
    if (isolated) {
        if (!web_fill.tryAllocPixels(web_info)) {
            throw std::runtime_error("isolated fill source could not allocate its bounded raster");
        }
        SkCanvas canvas(web_fill);
        canvas.clear(SK_ColorTRANSPARENT);
        const auto shape = rounded(source.fill);
        canvas.clipRRect(shape, SkClipOp::kIntersect, true);
        SkPaint paint;
        paint.setAntiAlias(true);
        canvas.drawRect(shape.rect(), paint);
    }
    const bool focus = source.border && source.border->outline_offset > 0;
    const unsigned opacity_scale = 1 + static_cast<unsigned>(source.isolated_opacity * 255);
    std::vector<ShapeBlendSample> different;
    for (std::uint32_t y = 0; y < source.height; ++y) {
        for (std::uint32_t x = 0; x < source.width; ++x) {
            const auto shadow_alpha = shadow ? coverage(*shadow, x + native_pad, y + native_pad) : 0;
            const auto fill_alpha = coverage(*fill, x + native_pad, y + native_pad);
            if (isolated) {
                // Fill colors may animate independently. Prove both fill masks
                // are empty at this sample before comparing just the shadow.
                if (!shadow_alpha || fill_alpha ||
                    SkColorGetA(*web_fill.getAddr32(static_cast<int>(x), static_cast<int>(y))) != 0) {
                    continue;
                }
                const auto pixel = *bitmap.getAddr32(static_cast<int>(x), static_cast<int>(y));
                const auto web_background = 255 - SkColorGetA(pixel) * opacity_scale / 256;
                const auto background_term = 255 - shadow_alpha * double(source.isolated_opacity);
                if (half_code(background_term)) {
                    continue;
                }
                const auto native_background = static_cast<unsigned>(std::lround(background_term));
                const std::array<unsigned, 3> web_premultiplied{SkColorGetR(pixel), SkColorGetG(pixel),
                                                                SkColorGetB(pixel)};
                ShapeBlendSample sample{x, y, {}, {}};
                bool adjacent = true;
                for (std::size_t i = 0; i < sample.native.size(); ++i) {
                    const auto source_byte = over_rgb8(source.shadow_color[i], 0, shadow_alpha);
                    const auto source_term = source_byte * double(source.isolated_opacity);
                    const auto scaled_source = static_cast<unsigned>(std::lround(source_term));
                    sample.web[i] =
                        static_cast<std::uint8_t>(web_background + web_premultiplied[i] * opacity_scale / 256);
                    sample.native[i] = static_cast<std::uint8_t>(std::min(255U, native_background + scaled_source));
                    adjacent &= !half_code(source_term) && std::abs(int(sample.web[i]) - int(sample.native[i])) <= 1;
                }
                if (adjacent && sample.web != sample.native) {
                    different.push_back(sample);
                }
                continue;
            }
            const auto pixel = bitmap.getColor(static_cast<int>(x), static_cast<int>(y));
            ShapeBlendSample sample{x,
                                    y,
                                    {static_cast<std::uint8_t>(SkColorGetR(pixel)),
                                     static_cast<std::uint8_t>(SkColorGetG(pixel)),
                                     static_cast<std::uint8_t>(SkColorGetB(pixel))},
                                    {}};
            const auto border_alpha = border ? coverage(*border, x, y) : 0;
            bool adjacent_codes = true;
            for (std::size_t i = 0; i < sample.native.size(); ++i) {
                const auto shadowed = over_rgb8(source.shadow_color[i], 255, shadow_alpha);
                const auto filled = source.split_fill ? over_rgb8_split(source.fill_color[i], shadowed, fill_alpha)
                                                      : over_rgb8(source.fill_color[i], shadowed, fill_alpha);
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
            if (sample.web != sample.native &&
                ((source.border && caster.shadow_alpha > 0) || source.split_fill || adjacent_codes)) {
                different.push_back(sample);
            }
        }
    }
    return different;
}

std::vector<std::uint8_t> shadow_group_alpha(const ShadowGroupSource& source) {
    SkBitmap bitmap;
    const auto srgb = SkColorSpace::MakeSRGB();
    if (!bitmap.tryAllocPixels(
            SkImageInfo::MakeN32Premul(static_cast<int>(source.width), static_cast<int>(source.height), srgb))) {
        throw std::runtime_error("isolated shadow group could not allocate its bounded raster");
    }
    const LockedShapeGeometry caster{source.caster, source.radius};
    SkCanvas canvas(bitmap);
    canvas.clear(SK_ColorTRANSPARENT);
    canvas.clipRRect(rounded(caster, source.caster_inset), SkClipOp::kDifference, true);
    for (const auto& shadow : source.shadows) {
        SkPaint paint;
        paint.setAntiAlias(true);
        paint.setColor(SK_ColorBLACK);
        paint.setMaskFilter(SkMaskFilter::MakeBlur(kNormal_SkBlurStyle, shadow.blur * .5F));
        paint.setColorFilter(SkColorFilters::Blend({0, 0, 0, shadow.alpha}, srgb, SkBlendMode::kSrcIn));
        canvas.save();
        canvas.translate(shadow.offset.x, shadow.offset.y);
        canvas.drawRRect(rounded(caster, -shadow.spread), paint);
        canvas.restore();
    }
    std::vector<std::uint8_t> alpha(static_cast<std::size_t>(source.width) * source.height);
    for (std::uint32_t y = 0; y < source.height; ++y) {
        for (std::uint32_t x = 0; x < source.width; ++x) {
            alpha[static_cast<std::size_t>(y) * source.width + x] = SkColorGetA(*bitmap.getAddr32(x, y));
        }
    }
    return alpha;
}

} // namespace rynui::example
