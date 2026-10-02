#include "renderer/common/scene_resources.hpp"

#include <atomic>
#include <limits>
#include <stdexcept>

namespace ryn::detail {
namespace {
std::atomic<std::uint64_t> next_backend_owner{1};
}

SceneBackend::SceneBackend()
    : owner_id_(next_backend_owner.fetch_add(1, std::memory_order_relaxed)) {}

bool SceneBackend::valid_attachment(const SceneAttachment &value) const noexcept {
    const auto state = value.state_.lock();
    const bool valid = state && state->ready && value.scene_ && value.glyphs_ &&
                       value.owner_ == owner_id() && state->owner == owner_id() &&
                       value.epoch_ == device_epoch() && state->epoch == device_epoch() &&
                       value.revision_ == state->revision;
    if (!valid)
        return false;
    for (const auto &command : value.scene_->commands()) {
        const auto end = std::uint64_t(command.first_instance) + command.instance_count;
        if (!command.instance_count)
            return false;
        switch (command.kind) {
        case graphics::SceneDrawKind::quad:
            if (!value.quads_ || end > value.quad_count_)
                return false;
            break;
        case graphics::SceneDrawKind::glyph:
            if (!value.glyphs_->instance_buffer() || end > value.glyph_count_ ||
                command.atlas_page >= value.glyphs_->texture_count())
                return false;
            break;
        case graphics::SceneDrawKind::rounded_effect:
            if (!value.effects_ || !value.effects_->buffer() ||
                end > value.effects_->instance_count())
                return false;
            break;
        default:
            return false;
        }
    }
    return true;
}

bool SceneBackend::attach_scene(const SceneAttachment &value) noexcept {
    if (!valid_attachment(value)) {
        attachment_ = {};
        return false;
    }
    attachment_ = value;
    return true;
}

SceneResources::SceneResources(SceneBackend &backend)
    : backend_(&backend), state_(std::make_shared<SceneResourceState>()),
      glyphs_(std::make_unique<GlyphGpuResources>(backend)),
      effects_(std::make_unique<RoundedEffectGpuResources>(backend)) {
    state_->owner = backend.owner_id();
    state_->epoch = backend.device_epoch();
}

SceneResources::~SceneResources() { retire(); }

void SceneResources::abandon_stale_device() noexcept {
    if (quads_)
        quads_->abandon_device();
    if (glyphs_)
        glyphs_->abandon_device();
    if (effects_)
        effects_->abandon_device();
}

void SceneResources::retire() noexcept {
    state_->ready = false;
    retry_required_ = true;
    if (state_->epoch != backend_->device_epoch())
        abandon_stale_device();
    quads_.reset();
    glyphs_.reset();
    effects_.reset();
}

void SceneResources::rollback(SceneCpuData data) noexcept {
    state_->ready = false;
    retry_required_ = true;
    if (effects_)
        effects_->invalidate_upload();
    // Retry information survives allocation failure while rebuilding dirty queues.
    try {
        mark_retry_data(data);
    } catch (...) {
    }
}

void SceneResources::mark_retry_data(SceneCpuData data) {
    data.atlas.mark_all_pages_dirty();
    data.glyphs.mark_all_dirty();
    if (data.quads)
        data.quads->mark_all_dirty();
    if (effects_)
        effects_->invalidate_upload();
}

bool SceneResources::synchronize(SceneCpuData data, SceneUploadTiming *timing) {
    state_->ready = false;
    ++state_->revision;
    bool active = false;
    try {
        graphics::validate_rounded_effect_device_metrics(data.metrics);
        if (state_->epoch != backend_->device_epoch() || !glyphs_ || !effects_) {
            abandon_stale_device();
            quads_.reset();
            glyphs_.reset();
            effects_.reset();
            state_->epoch = backend_->device_epoch();
            rollback(data);
            glyphs_ = std::make_unique<GlyphGpuResources>(*backend_);
            effects_ = std::make_unique<RoundedEffectGpuResources>(*backend_);
        }
        if (retry_required_)
            mark_retry_data(data);
        if (data.glyphs.size() > std::numeric_limits<std::uint32_t>::max() ||
            (data.quads && data.quads->size() > std::numeric_limits<std::uint32_t>::max()))
            throw std::length_error("Scene instance count exceeds uint32_t");
        if (!backend_->begin_upload_batch()) {
            rollback(data);
            return false;
        }
        active = true;
        quad_count_ = data.quads ? static_cast<std::uint32_t>(data.quads->size()) : 0;
        glyph_count_ = static_cast<std::uint32_t>(data.glyphs.size());
        if (quad_count_) {
            if (!quads_)
                quads_ = std::make_unique<QuadGpuBuffer>(*backend_, *data.quads, data.metrics);
            else
                quads_->synchronize(*data.quads, data.metrics);
        }
        if (timing)
            timing->quads = std::chrono::steady_clock::now();
        glyphs_->synchronize(data.atlas, data.glyphs, data.metrics);
        if (timing)
            timing->glyphs = std::chrono::steady_clock::now();
        if (data.effects)
            effects_->synchronize(*data.effects, data.metrics);
        if (timing)
            timing->effects = std::chrono::steady_clock::now();
        if (!backend_->finish_upload_batch()) {
            backend_->cancel_upload_batch();
            rollback(data);
            return false;
        }
        active = false;
        state_->ready = true;
        retry_required_ = false;
        if (timing)
            timing->committed = std::chrono::steady_clock::now();
        return true;
    } catch (...) {
        if (active)
            backend_->cancel_upload_batch();
        rollback(data);
        throw;
    }
}

SceneAttachment SceneResources::attach(const graphics::OrderedScene &scene) const noexcept {
    SceneAttachment result;
    result.state_ = state_;
    result.owner_ = state_->owner;
    result.epoch_ = state_->epoch;
    result.revision_ = state_->revision;
    result.quads_ = quads_ ? quads_->handle() : nullptr;
    result.glyphs_ = glyphs_.get();
    result.effects_ = effects_.get();
    result.scene_ = &scene;
    result.quad_count_ = quad_count_;
    result.glyph_count_ = glyph_count_;
    return result;
}
} // namespace ryn::detail
