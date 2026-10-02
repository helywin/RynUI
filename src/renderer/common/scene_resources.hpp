#pragma once

#include "renderer/common/scene_backend.hpp"

#include <chrono>
#include <memory>

namespace ryn::detail {

struct SceneCpuData final {
    graphics::QuadInstanceStore *quads;
    graphics::GlyphAtlas &atlas;
    graphics::GlyphInstanceStore &glyphs;
    graphics::RoundedEffectStore *effects;
    graphics::RoundedEffectDeviceMetrics metrics;
};

struct SceneUploadTiming final {
    std::chrono::steady_clock::time_point quads, glyphs, effects, committed;
};

class SceneResources final {
  public:
    explicit SceneResources(SceneBackend &backend);
    SceneResources(const SceneResources &) = delete;
    SceneResources &operator=(const SceneResources &) = delete;
    ~SceneResources();

    // False reports begin/commit failure. Upload exceptions remain exceptions.
    // Every failure invalidates attachments and restores all participating dirty data.
    bool synchronize(SceneCpuData data, SceneUploadTiming *timing = nullptr);
    [[nodiscard]] SceneAttachment attach(const graphics::OrderedScene &scene) const noexcept;
    void retire() noexcept;
    [[nodiscard]] detail::QuadGpuBuffer *quads() const noexcept { return quads_.get(); }
    [[nodiscard]] GlyphGpuResources &glyphs() const noexcept { return *glyphs_; }
    [[nodiscard]] RoundedEffectGpuResources &effects() const noexcept { return *effects_; }

  private:
    void abandon_stale_device() noexcept;
    void rollback(SceneCpuData data) noexcept;
    void mark_retry_data(SceneCpuData data);
    SceneBackend *backend_;
    std::shared_ptr<SceneResourceState> state_;
    std::unique_ptr<detail::QuadGpuBuffer> quads_;
    std::unique_ptr<GlyphGpuResources> glyphs_;
    std::unique_ptr<RoundedEffectGpuResources> effects_;
    std::uint32_t quad_count_{}, glyph_count_{};
    bool retry_required_{true};
};

} // namespace ryn::detail
