#pragma once

#include "renderer/common/scene_backend.hpp"

#include <chrono>
#include <memory>

namespace ryn::detail {

struct SceneCpuData final {
    graphics::QuadInstanceStore* quads;
    graphics::GlyphAtlas& atlas;
    graphics::GlyphInstanceStore& glyphs;
    graphics::RoundedEffectStore* effects;
    SceneDeviceMetrics metrics;
    graphics::ImageStore* images{};
    const graphics::OrderedScene* composition_scene{};
};

struct SceneUploadTiming final {
    std::chrono::steady_clock::time_point quads;
    std::chrono::steady_clock::time_point glyphs;
    std::chrono::steady_clock::time_point effects;
    std::chrono::steady_clock::time_point committed;
};

class SceneResources final {
public:
    explicit SceneResources(SceneBackend& backend);
    SceneResources(const SceneResources&) = delete;
    SceneResources& operator=(const SceneResources&) = delete;
    ~SceneResources();

    // False reports begin/commit failure. Upload exceptions remain exceptions.
    // Every failure invalidates attachments and restores all participating dirty data.
    bool synchronize(SceneCpuData data, SceneUploadTiming* timing = nullptr);

    // Frame time is supplied by the existing monotonic host. A ticket is only
    // issued while a concrete old frame fence blocks capacity or cold cleanup.
    void set_frame_time(animation::AnimationTime value) noexcept {
        frame_time_ = value;
    }

    [[nodiscard]] std::optional<animation::AnimationTime> next_deadline() const noexcept {
        return fence_retry_;
    }

    [[nodiscard]] bool waiting_for_composition_fence() const noexcept {
        return fence_retry_.has_value();
    }

    [[nodiscard]] SceneAttachment attach(const graphics::OrderedScene& scene) const noexcept;
    void retire() noexcept;

    [[nodiscard]] detail::QuadGpuBuffer* quads() const noexcept {
        return quads_.get();
    }

    [[nodiscard]] GlyphGpuResources& glyphs() const noexcept {
        return *glyphs_;
    }

    [[nodiscard]] RoundedEffectGpuResources& effects() const noexcept {
        return *effects_;
    }

private:
    void abandon_stale_device() noexcept;
    void refresh_fence_retry() noexcept;
    void rollback(SceneCpuData data) noexcept;
    void mark_retry_data(SceneCpuData data);
    SceneBackend* backend_;
    std::shared_ptr<SceneResourceState> state_;
    std::unique_ptr<detail::QuadGpuBuffer> quads_;
    std::unique_ptr<GlyphGpuResources> glyphs_;
    std::unique_ptr<ImageGpuResources> images_;
    std::unique_ptr<RoundedEffectGpuResources> effects_;
    CompositionResources composition_;
    std::uint32_t quad_count_{};
    std::uint32_t glyph_count_{};
    bool retry_required_{true};
    animation::AnimationTime frame_time_;
    std::optional<animation::AnimationTime> fence_retry_;
};

} // namespace ryn::detail
