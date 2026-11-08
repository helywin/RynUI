#include "renderer/common/composition_resources.hpp"
#include "renderer/common/scene_capabilities.hpp"
#include <algorithm>
#include <stdexcept>

namespace ryn::detail {
CompositionResources::~CompositionResources() {
    retire();
}

std::uint64_t CompositionResources::allocated_bytes() const noexcept {
    std::uint64_t result = 0;
    for (const auto& frame : frames_) {
        result += frame->plan.allocated_bytes;
    }
    return result;
}

void CompositionResources::release(CompositionFrame& frame) noexcept {
    if (frame.fence) {
        api_->wait_composition_fence(frame.fence);
    }
    for (auto* target : frame.targets) {
        api_->release_composition_target(target);
    }
    if (frame.weights) {
        api_->release_composition_weights(frame.weights);
    }
    frame.targets.clear();
    frame.weights = nullptr;
    frame.uploaded = false;
}

void CompositionResources::collect(std::uint64_t epoch) {
    current_.reset();
    std::erase_if(frames_, [&](const auto& frame) {
        if (frame->epoch != epoch) {
            return true;
        } // Old device: abandon, never release on a new API epoch.
        if (!frame->fence || api_->composition_fence_complete(frame->fence)) {
            release(*frame);
            return true;
        }
        return false;
    });
}

void CompositionResources::preflight(const CompositionPlan& plan, std::uint64_t owner, std::uint64_t epoch,
                                     const SceneBackendCapabilities& caps) {
    (void)owner;
    // Evict completed storage before reserving the exact requested capacities.
    // First implementation intentionally has no size buckets or result cache.
    collect(epoch);
    if (plan.program.requests.empty() && frames_.empty()) {
        api_->trim_idle_composition();
    }
    const auto previous = allocated_bytes();
    if (plan.allocated_bytes > caps.maximum_composition_bytes ||
        previous > caps.maximum_composition_bytes - plan.allocated_bytes ||
        (!plan.targets.empty() && frames_.size() >= caps.maximum_composition_frames)) {
        throw std::length_error("Composition in-flight capacity budget exhausted; await frame completion");
    }
}

void CompositionResources::synchronize(CompositionPlan plan, std::uint64_t owner, std::uint64_t epoch,
                                       const SceneBackendCapabilities&) {
    current_.reset();
    if (plan.program.requests.empty()) {
        return;
    }
    auto frame = std::make_shared<CompositionFrame>(std::move(plan));
    frame->owner = owner;
    frame->epoch = epoch;
    try {
        frame->targets.reserve(frame->plan.targets.size());
        for (const auto spec : frame->plan.targets) {
            auto* target = api_->create_composition_target(spec);
            if (!target) {
                throw std::runtime_error("Composition target allocation failed");
            }
            frame->targets.push_back(target);
        }
        if (!frame->plan.weights.empty()) {
            frame->weights = api_->create_composition_weights(frame->plan.weights.size() * sizeof(GaussianWeightV1));
            if (!frame->weights ||
                !api_->upload_composition_weights(frame->weights, std::as_bytes(std::span(frame->plan.weights)))) {
                throw std::runtime_error("Composition weight upload failed");
            }
        }
        frame->uploaded = true;
        frames_.push_back(frame);
        current_ = std::move(frame);
    } catch (...) {
        release(*frame);
        throw;
    }
}

void CompositionResources::invalidate() noexcept {
    if (current_) {
        current_->uploaded = false;
    }
    current_.reset();
}

void CompositionResources::abandon() noexcept {
    current_.reset();
    frames_.clear();
}

void CompositionResources::retire() noexcept {
    for (auto& frame : frames_) {
        release(*frame);
    }
    frames_.clear();
    current_.reset();
}
} // namespace ryn::detail
