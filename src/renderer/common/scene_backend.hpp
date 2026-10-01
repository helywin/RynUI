#pragma once

#include "graphics/quad_primitive.hpp"
#include "renderer/common/glyph_gpu_resources.hpp"
#include "renderer/common/rounded_effect_gpu_resources.hpp"
#include "runtime/frame_scheduler.hpp"

#include <memory>

namespace ryn::detail {

class SceneResources;
class SceneBackend;

struct SceneResourceState final {
    std::uint64_t owner{};
    std::uint64_t epoch{};
    std::uint64_t revision{};
    bool ready{};
};

// A borrowed scene is consumed before its CPU OrderedScene is changed/destroyed.
// The weak resource stamp prevents access after retirement, failed upload or reset.
class SceneAttachment final {
  public:
    [[nodiscard]] graphics::QuadGpuBufferHandle quads() const noexcept { return quads_; }
    [[nodiscard]] const GlyphGpuResources *glyphs() const noexcept { return glyphs_; }
    [[nodiscard]] const RoundedEffectGpuResources *effects() const noexcept { return effects_; }
    [[nodiscard]] const graphics::OrderedScene *scene() const noexcept { return scene_; }
    [[nodiscard]] std::uint32_t quad_count() const noexcept { return quad_count_; }
    [[nodiscard]] std::uint32_t glyph_count() const noexcept { return glyph_count_; }

  private:
    friend class SceneResources;
    friend class SceneBackend;
    std::weak_ptr<const SceneResourceState> state_;
    std::uint64_t owner_{}, epoch_{}, revision_{};
    graphics::QuadGpuBufferHandle quads_{};
    const GlyphGpuResources *glyphs_{};
    const RoundedEffectGpuResources *effects_{};
    const graphics::OrderedScene *scene_{};
    std::uint32_t quad_count_{}, glyph_count_{};
};

// All upload methods own/copy source bytes before returning success. Batch commit
// accepts GPU work; it is not a GPU completion notification.
class SceneBackend : public graphics::QuadUploadApi,
                     public GlyphGpuApi,
                     public RoundedEffectGpuApi,
                     public SceneDrawApi,
                     public runtime::FrameSubmitter {
  public:
    SceneBackend();
    SceneBackend(const SceneBackend &) = delete;
    SceneBackend &operator=(const SceneBackend &) = delete;
    [[nodiscard]] std::uint64_t owner_id() const noexcept { return owner_id_; }
    [[nodiscard]] virtual std::uint64_t device_epoch() const noexcept = 0;
    virtual bool begin_upload_batch() = 0;
    virtual bool finish_upload_batch() = 0;
    virtual void cancel_upload_batch() noexcept = 0;
    [[nodiscard]] virtual bool attach_scene(const SceneAttachment &attachment) noexcept;
    [[nodiscard]] bool valid_attachment(const SceneAttachment &attachment) const noexcept;

  protected:
    [[nodiscard]] const SceneAttachment &attachment() const noexcept { return attachment_; }
    [[nodiscard]] bool attached_scene_ready() const noexcept {
        return valid_attachment(attachment_);
    }

  private:
    const std::uint64_t owner_id_;
    SceneAttachment attachment_;
};

} // namespace ryn::detail
