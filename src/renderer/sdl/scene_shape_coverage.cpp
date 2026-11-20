#include "renderer/sdl/scene_renderer.hpp"

#include <SDL3/SDL.h>

#include <algorithm>
#include <array>
#include <bit>
#include <memory>
#include <stdexcept>

namespace ryn::detail {

bool SdlSceneRenderer::upload_quad_coverage(QuadGpuBufferHandle handle, std::uint32_t first,
                                            std::span<const graphics::QuadInstance> quads, SceneDeviceMetrics metrics) {
    if (!locked_coverage_) {
        return true;
    }
    auto* owner = resource(handle, ResourceKind::quad);
    if (!owner) {
        last_error_ = "Locked coverage has no live Quad owner";
        return false;
    }
    const auto scale = metrics.display_scale;
    const std::array full_clip{0.0F, 0.0F, float(metrics.pixel_width), float(metrics.pixel_height)};
    for (std::uint32_t i = 0; i < quads.size(); ++i) {
        const auto& quad = quads[i];
        std::optional<LockedShapeGeometry> geometry;
        if (quad.corner_radius > 0 && quad.opacity > 0 && quad.color[3] > 0) {
            geometry = LockedShapeGeometry{{(quad.bounds[0] + quad.translation[0]) * scale,
                                            (quad.bounds[1] + quad.translation[1]) * scale, quad.bounds[2] * scale,
                                            quad.bounds[3] * scale},
                                           quad.corner_radius * scale};
        }
        if (!update_coverage(*owner, first + i, geometry, metrics, quad.color, quad.opacity, full_clip)) {
            return false;
        }
    }
    return true;
}

bool SdlSceneRenderer::upload_effect_coverage(RoundedEffectGpuBufferHandle handle, std::uint32_t first,
                                              std::span<const RoundedEffectGpuInstance> effects,
                                              SceneDeviceMetrics metrics) {
    if (!locked_coverage_) {
        return true;
    }
    auto* owner = resource(handle, ResourceKind::effect);
    if (!owner) {
        last_error_ = "Locked coverage has no live Effect owner";
        return false;
    }
    for (std::uint32_t i = 0; i < effects.size(); ++i) {
        const auto& effect = effects[i];
        const auto& a = effect.shape_rect;
        const auto& shadow = effect.shadow_params;
        const auto& properties = effect.effect_params;
        const auto kind = properties[3];
        const auto opacity = effect.material_params[0];
        std::optional<LockedShapeGeometry> geometry;
        if (opacity > 0 && effect.color[3] > 0 && effect.clip_rect[2] > 0 && effect.clip_rect[3] < 0) {
            const LockedShapeGeometry base{{a[0], a[1], a[2], a[3]}, shadow[0]};
            if (kind == 0 && shadow[0] > 0 && shadow[1] == 0 && shadow[2] == 0 && shadow[3] == 0 &&
                properties[0] == 0) {
                geometry = base;
            } else if (kind == 2 && shadow[0] > 0) {
                geometry = base;
                geometry->kind = LockedShapeKind::outline;
                geometry->outline_width = properties[1];
                geometry->outline_offset = properties[2];
            } else if (kind == 6 && effect.material_params[2] == 1) {
                geometry = base;
                geometry->kind = LockedShapeKind::exterior_shadow;
                geometry->shadow_offset = {shadow[1], shadow[2]};
                geometry->shadow_blur = shadow[3];
                geometry->shadow_spread = properties[0];
                geometry->shadow_caster_inset = properties[2];
                geometry->shadow_alpha = effect.color[3] * opacity;
            }
        }
        if (!update_coverage(*owner, first + i, geometry, metrics, effect.color, opacity, effect.clip_bounds)) {
            return false;
        }
    }
    return true;
}

bool SdlSceneRenderer::update_coverage(Resource& owner, std::uint32_t index,
                                       std::optional<LockedShapeGeometry> geometry, SceneDeviceMetrics metrics,
                                       std::array<float, 4> color, float opacity, const std::array<float, 4>& clip) {
#if defined(RYNUI_HAS_PINNED_SHAPE_COVERAGE)
    if (geometry) {
        geometry->dark_ink = color[0] * 0.2126F + color[1] * 0.7152F + color[2] * 0.0722F < 0.5F;
    }
    auto mask = geometry ? rasterize_locked_shape(*geometry, metrics.pixel_width, metrics.pixel_height) : std::nullopt;
    if (!mask) {
        if (owner.coverage) {
            const auto old = owner.coverage->find(index);
            if (old != owner.coverage->end()) {
                release_coverage(old->second);
                owner.coverage->erase(old);
            }
            if (owner.coverage->empty()) {
                owner.coverage.reset();
            }
        }
        return true;
    }
    if (!owner.coverage) {
        owner.coverage = std::make_unique<std::unordered_map<std::uint32_t, CoverageDraw>>();
    }
    auto& draw = (*owner.coverage)[index];
    if (draw.width < mask->width || draw.height < mask->height) {
        const auto width = std::max(draw.width, std::bit_ceil(mask->width));
        const auto height = std::max(draw.height, std::bit_ceil(mask->height));
        release_glyph_texture(draw.texture);
        draw.texture = create_glyph_texture(width, height);
        draw.width = width;
        draw.height = height;
    }
    if (!draw.vertices) {
        draw.vertices = create_glyph_buffer(sizeof(GlyphGpuInstance));
    }
    if (!draw.texture || !draw.vertices) {
        release_coverage(draw);
        owner.coverage->erase(index);
        return false;
    }
    for (std::uint32_t y = 0; y < mask->height; ++y) {
        for (std::uint32_t x = 0; x < mask->width; ++x) {
            const auto px = float(mask->left + x) + 0.5F;
            const auto py = float(mask->top + y) + 0.5F;
            if (px < clip[0] || px >= clip[2] || py < clip[1] || py >= clip[3]) {
                mask->pixels[std::size_t(y) * mask->width + x] = 0;
            }
        }
    }
    if (geometry->kind == LockedShapeKind::exterior_shadow) {
        color[3] = 1;
        opacity = 1;
    }
    const auto sx = 2.0F / float(metrics.pixel_width);
    const auto sy = 2.0F / float(metrics.pixel_height);
    const GlyphGpuInstance packed{
        {float(mask->left) * sx - 1, 1 - float(mask->top) * sy, float(mask->width) * sx, -float(mask->height) * sy},
        {0, 0, float(mask->width) / float(draw.width), float(mask->height) / float(draw.height)},
        {-1, 1, 1, -1},
        color,
        {0, 0, opacity, 0}};
    return upload_glyph_texture(
               draw.texture,
               {0, {0, 0, mask->width, mask->height}, 0, mask->width, std::as_bytes(std::span{mask->pixels})}) &&
           upload_glyph_buffer(draw.vertices, 0, std::as_bytes(std::span{&packed, 1}));
#else
    (void)owner;
    (void)index;
    (void)geometry;
    (void)metrics;
    (void)color;
    (void)opacity;
    (void)clip;
    last_error_ = "Pinned rounded coverage was not built";
    return false;
#endif
}

void SdlSceneRenderer::release_coverage(CoverageDraw& draw) noexcept {
    release_glyph_texture(draw.texture);
    release_glyph_buffer(draw.vertices);
    draw = {};
}

void SdlSceneRenderer::draw_coverage(const CoverageDraw& draw) {
    if (!glyph_resources_) {
        throw std::logic_error("Locked coverage sampler is not attached");
    }
    auto* pass = static_cast<SDL_GPURenderPass*>(active_render_pass_);
    SDL_BindGPUGraphicsPipeline(
        pass, static_cast<SDL_GPUGraphicsPipeline*>(composition_active_ ? composition_pipelines_[1] : glyph_pipeline_));
    const SDL_GPUBufferBinding vertices{
        static_cast<SDL_GPUBuffer*>(native_resource(draw.vertices, ResourceKind::glyph)), 0};
    SDL_BindGPUVertexBuffers(pass, 0, &vertices, 1);
    const SDL_GPUTextureSamplerBinding texture{
        static_cast<SDL_GPUTexture*>(native_resource(draw.texture, ResourceKind::texture)),
        static_cast<SDL_GPUSampler*>(native_resource(glyph_resources_->sampler(), ResourceKind::sampler))};
    SDL_BindGPUFragmentSamplers(pass, 0, &texture, 1);
    SDL_DrawGPUPrimitives(pass, glyph_vertex_count, 1, 0, 0);
    ++counters_.locked_coverage_draws;
}

void SdlSceneRenderer::draw_coverage_ranges(const Resource& owner, std::uint32_t first, std::uint32_t count,
                                            void (SdlSceneRenderer::*draw_range)(std::uint32_t, std::uint32_t)) {
    const auto end = first + count;
    auto ordinary = first;
    for (auto i = first; i < end; ++i) {
        const auto found = owner.coverage->find(i);
        if (found == owner.coverage->end()) {
            continue;
        }
        if (ordinary < i) {
            (this->*draw_range)(ordinary, i - ordinary);
        }
        draw_coverage(found->second);
        ordinary = i + 1;
    }
    if (ordinary < end) {
        (this->*draw_range)(ordinary, end - ordinary);
    }
}

} // namespace ryn::detail
