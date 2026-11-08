#pragma once
#include "renderer/common/composition_packing.hpp"
#include <memory>
#include <utility>

namespace ryn::detail {
// Explicit completion leases. Upload commit does not complete a frame.
class CompositionGpuApi {
public:
    virtual ~CompositionGpuApi() = default;

    virtual void* create_composition_target(CompositionTargetSpec) {
        return nullptr;
    }

    virtual void release_composition_target(void*) noexcept {}

    virtual void* create_composition_weights(std::size_t) {
        return nullptr;
    }

    virtual bool upload_composition_weights(void*, std::span<const std::byte>) {
        return false;
    }

    virtual void release_composition_weights(void*) noexcept {}

    virtual bool composition_fence_complete(std::uint64_t) {
        return false;
    }

    virtual void wait_composition_fence(std::uint64_t) noexcept {}

    virtual void trim_idle_composition() noexcept {}
};

struct CompositionFrame final {
    explicit CompositionFrame(CompositionPlan value) : plan(std::move(value)) {}

    const CompositionPlan plan;
    std::vector<void*> targets;
    void* weights{};
    std::uint64_t owner{};
    std::uint64_t epoch{};
    std::uint64_t fence{};
    bool uploaded{};
};

class CompositionResources final {
public:
    explicit CompositionResources(CompositionGpuApi& api) : api_(&api) {}

    ~CompositionResources();
    void preflight(const CompositionPlan&, std::uint64_t owner, std::uint64_t epoch, const SceneBackendCapabilities&);
    void synchronize(CompositionPlan, std::uint64_t owner, std::uint64_t epoch, const SceneBackendCapabilities&);
    void invalidate() noexcept;
    void abandon() noexcept;
    void retire() noexcept;

    [[nodiscard]] const std::shared_ptr<CompositionFrame>& current() const noexcept {
        return current_;
    }

    [[nodiscard]] std::uint64_t allocated_bytes() const noexcept;

    [[nodiscard]] bool waiting_for_fences() const noexcept {
        if (current_) {
            return false;
        }
        try {
            for (const auto& frame : frames_) {
                if (frame->fence && !api_->composition_fence_complete(frame->fence)) {
                    return true;
                }
            }
        } catch (...) {
            return false;
        }
        return false;
    }

private:
    void collect(std::uint64_t epoch);
    void release(CompositionFrame&) noexcept;
    CompositionGpuApi* api_;
    std::vector<std::shared_ptr<CompositionFrame>> frames_;
    std::shared_ptr<CompositionFrame> current_;
};
} // namespace ryn::detail
