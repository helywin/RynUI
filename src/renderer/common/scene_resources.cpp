#include "renderer/common/scene_resources.hpp"
#include "renderer/common/scene_buffer_capacity.hpp"

#include <atomic>
#include <algorithm>
#include <limits>
#include <stdexcept>

namespace ryn::detail {
namespace {
std::atomic<std::uint64_t> next_backend_owner{1};
}

SceneBackend::SceneBackend() : owner_id_(next_backend_owner.fetch_add(1, std::memory_order_relaxed)) {}

bool SceneBackend::valid_attachment(const SceneAttachment& value) const noexcept {
    const auto state = value.state_.lock();
    const bool valid = state && state->ready && value.scene_ && value.glyphs_ && value.owner_ == owner_id() &&
                       state->owner == owner_id() && value.epoch_ == device_epoch() && state->epoch == device_epoch() &&
                       value.revision_ == state->revision && value.scene_revision_ == value.scene_->revision();
    if (!valid) {
        return false;
    }
    if (value.scene_->composition()) {
        if (!value.composition_ || !value.composition_->uploaded || value.composition_->owner != owner_id() ||
            value.composition_->epoch != device_epoch() ||
            value.composition_->plan.program.scene_revision != value.scene_->revision() ||
            !std::ranges::equal(value.composition_->plan.commands, value.scene_->commands()) ||
            value.composition_->plan.program.operations != value.scene_->composition()->operations ||
            value.composition_->plan.program.requests != value.scene_->composition()->requests) {
            return false;
        }
    } else if (value.composition_) {
        return false;
    }
    for (const auto& command : value.scene_->commands()) {
        const auto end = std::uint64_t(command.first_instance) + command.instance_count;
        if (!command.instance_count) {
            return false;
        }
        switch (command.kind) {
        case graphics::SceneDrawKind::image:
            if (command.instance_count != 1 || !value.images_ ||
                !value.images_->drawable(command.first_instance, command.atlas_page)) {
                return false;
            }
            break;
        case graphics::SceneDrawKind::quad:
            if (!value.quads_ || end > value.quad_count_) {
                return false;
            }
            break;
        case graphics::SceneDrawKind::glyph:
            if (!value.glyphs_->instance_buffer() || end > value.glyph_count_ ||
                command.atlas_page >= value.glyphs_->texture_count()) {
                return false;
            }
            break;
        case graphics::SceneDrawKind::rounded_effect:
            if (!value.effects_ || !value.effects_->buffer() || end > value.effects_->instance_count()) {
                return false;
            }
            break;
        default:
            return false;
        }
    }
    return true;
}

bool SceneBackend::attach_scene(const SceneAttachment& value) noexcept {
    if (!valid_attachment(value)) {
        attachment_ = {};
        return false;
    }
    attachment_ = value;
    return true;
}

SceneResources::SceneResources(SceneBackend& backend)
    : backend_(&backend), state_(std::make_shared<SceneResourceState>()), composition_(backend) {
    validate_scene_capabilities(backend.capabilities());
    glyphs_ = std::make_unique<GlyphGpuResources>(backend);
    images_ = std::make_unique<ImageGpuResources>(backend);
    effects_ = std::make_unique<RoundedEffectGpuResources>(backend);
    state_->owner = backend.owner_id();
    state_->epoch = backend.device_epoch();
}

SceneResources::~SceneResources() {
    retire();
}

void SceneResources::abandon_stale_device() noexcept {
    fence_retry_.reset();
    composition_.abandon();
    if (images_) {
        images_->abandon_device();
    }
    if (quads_) {
        quads_->abandon_device();
    }
    if (glyphs_) {
        glyphs_->abandon_device();
    }
    if (effects_) {
        effects_->abandon_device();
    }
}

void SceneResources::retire() noexcept {
    fence_retry_.reset();
    state_->ready = false;
    retry_required_ = true;
    if (state_->epoch != backend_->device_epoch()) {
        abandon_stale_device();
    }
    composition_.retire();
    quads_.reset();
    images_.reset();
    glyphs_.reset();
    effects_.reset();
}

void SceneResources::rollback(SceneCpuData data) noexcept {
    if (state_->epoch != backend_->device_epoch()) {
        composition_.abandon();
    } else {
        composition_.invalidate();
    }
    refresh_fence_retry();
    state_->ready = false;
    retry_required_ = true;
    if (images_) {
        images_->invalidate_upload();
    }
    if (effects_) {
        effects_->invalidate_upload();
    }
    // Retry information survives allocation failure while rebuilding dirty queues.
    try {
        mark_retry_data(data);
    } catch (...) {
    }
}

void SceneResources::mark_retry_data(SceneCpuData data) {
    data.atlas.mark_all_pages_dirty();
    data.glyphs.mark_all_dirty();
    if (data.quads) {
        data.quads->mark_all_dirty();
    }
    if (effects_) {
        effects_->invalidate_upload();
    }
}

bool SceneResources::synchronize(SceneCpuData data, SceneUploadTiming* timing) {
    state_->ready = false;
    ++state_->revision;
    bool active = false;
    try {
        validate_scene_device_metrics(data.metrics);
        const auto capabilities = backend_->capabilities();
        validate_scene_capabilities(capabilities);
        CompositionPlan composition_plan;
        if (data.composition_scene) {
            composition_plan = pack_composition(*data.composition_scene, data.metrics, capabilities);
        }
        composition_.preflight(composition_plan, backend_->owner_id(), backend_->device_epoch(), capabilities);
        RoundedEffectGpuResources::preflight(data.effects, data.metrics, capabilities,
                                             state_->epoch == backend_->device_epoch() ? effects_.get() : nullptr);
        if (data.images && data.images->live_count() &&
            (!capabilities.images || !capabilities.rgba8_sampling ||
             capabilities.image_abi_version != image_packed_abi_version)) {
            throw std::invalid_argument("Backend lacks required Image RGBA8 ABI/capability");
        }
        ImageGpuResources::preflight(data.images, capabilities.maximum_buffer_bytes, capabilities.maximum_texture_width,
                                     capabilities.maximum_texture_height);
        const auto quad_capacity = quads_ && state_->epoch == backend_->device_epoch() ? quads_->capacity() : 0;
        validate_scene_buffer_requirement(capabilities,
                                          quad_buffer_capacity(data.quads ? data.quads->size() : 0, quad_capacity),
                                          sizeof(QuadGpuInstance), false);
        const auto glyph_capacity =
            glyphs_ && state_->epoch == backend_->device_epoch() ? glyphs_->instance_capacity() : 0;
        validate_scene_buffer_requirement(
            capabilities, glyph_buffer_capacity(data.glyphs.size(), glyph_capacity, capabilities.maximum_buffer_bytes),
            sizeof(GlyphGpuInstance), false);
        if (data.atlas.page_count() && (data.atlas.config().page_width > capabilities.maximum_texture_width ||
                                        data.atlas.config().page_height > capabilities.maximum_texture_height)) {
            throw std::length_error("Scene atlas extent exceeds renderer input limit");
        }
        if (state_->epoch != backend_->device_epoch() || !glyphs_ || !effects_) {
            abandon_stale_device();
            quads_.reset();
            images_.reset();
            glyphs_.reset();
            effects_.reset();
            state_->epoch = backend_->device_epoch();
            rollback(data);
            glyphs_ = std::make_unique<GlyphGpuResources>(*backend_);
            images_ = std::make_unique<ImageGpuResources>(*backend_);
            effects_ = std::make_unique<RoundedEffectGpuResources>(*backend_);
        }
        if (retry_required_) {
            mark_retry_data(data);
        }
        if (data.glyphs.size() > std::numeric_limits<std::uint32_t>::max() ||
            (data.quads && data.quads->size() > std::numeric_limits<std::uint32_t>::max())) {
            throw std::length_error("Scene instance count exceeds uint32_t");
        }
        if (!backend_->begin_upload_batch()) {
            rollback(data);
            return false;
        }
        active = true;
        quad_count_ = data.quads ? static_cast<std::uint32_t>(data.quads->size()) : 0;
        glyph_count_ = static_cast<std::uint32_t>(data.glyphs.size());
        if (quad_count_) {
            if (!quads_) {
                quads_ = std::make_unique<QuadGpuBuffer>(*backend_, *data.quads, data.metrics);
            } else {
                quads_->synchronize(*data.quads, data.metrics);
            }
        }
        if (timing) {
            timing->quads = std::chrono::steady_clock::now();
        }
        glyphs_->synchronize(data.atlas, data.glyphs, data.metrics, capabilities.maximum_buffer_bytes);
        if (timing) {
            timing->glyphs = std::chrono::steady_clock::now();
        }
        if (data.effects) {
            effects_->synchronize(*data.effects, data.metrics,
                                  capabilities.effect_paint_abi_version == effect_paint_abi_version);
        }
        if (timing) {
            timing->effects = std::chrono::steady_clock::now();
        }
        images_->synchronize(data.images, data.metrics);
        composition_.synchronize(std::move(composition_plan), backend_->owner_id(), backend_->device_epoch(),
                                 capabilities);
        if (!backend_->finish_upload_batch()) {
            backend_->cancel_upload_batch();
            rollback(data);
            return false;
        }
        active = false;
        state_->metrics = data.metrics;
        state_->ready = true;
        retry_required_ = false;
        refresh_fence_retry();
        if (timing) {
            timing->committed = std::chrono::steady_clock::now();
        }
        return true;
    } catch (...) {
        if (active) {
            backend_->cancel_upload_batch();
        }
        rollback(data);
        throw;
    }
}

void SceneResources::refresh_fence_retry() noexcept {
    if (!composition_.waiting_for_fences()) {
        fence_retry_.reset();
        return;
    }
    constexpr auto interval = 16000;
    const auto now = frame_time_.count_microseconds();
    const auto maximum = std::numeric_limits<animation::AnimationTime::rep>::max();
    fence_retry_ = animation::AnimationTime::microseconds(now > maximum - interval ? maximum : now + interval);
}

SceneAttachment SceneResources::attach(const graphics::OrderedScene& scene) const noexcept {
    SceneAttachment result;
    result.state_ = state_;
    result.owner_ = state_->owner;
    result.epoch_ = state_->epoch;
    result.revision_ = state_->revision;
    result.quads_ = quads_ ? quads_->handle() : nullptr;
    result.glyphs_ = glyphs_.get();
    result.images_ = images_.get();
    result.effects_ = effects_.get();
    result.scene_ = &scene;
    result.metrics_ = state_->metrics;
    result.scene_revision_ = scene.revision();
    result.quad_count_ = quad_count_;
    result.glyph_count_ = glyph_count_;
    result.composition_ = composition_.current();
    return result;
}
} // namespace ryn::detail
