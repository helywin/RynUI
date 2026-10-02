#pragma once

#include "renderer/common/rounded_effect_packing.hpp"

#include <cstddef>
#include <cstdint>
#include <optional>
#include <span>
#include <vector>

namespace ryn::detail {

using RoundedEffectGpuBufferHandle = void*;

class RoundedEffectGpuApi {
public:
    virtual ~RoundedEffectGpuApi() = default;

    virtual RoundedEffectGpuBufferHandle create_effect_buffer(std::size_t size) = 0;
    virtual bool upload_effect_buffer(RoundedEffectGpuBufferHandle buffer, std::size_t offset,
                                      std::span<const std::byte> bytes) = 0;
    virtual void release_effect_buffer(RoundedEffectGpuBufferHandle buffer) noexcept = 0;
    [[nodiscard]] virtual const char* effect_gpu_error() const noexcept = 0;
};

struct RoundedEffectGpuResourceCounters final {
    std::uint64_t buffer_reallocations{};
    std::uint64_t buffer_uploads{};
    std::uint64_t uploaded_bytes{};
    std::uint64_t full_uploads{};
    std::uint64_t partial_uploads{};
    std::uint64_t idle_synchronizations{};
    std::uint64_t zero_effect_synchronizations{};
};

class RoundedEffectGpuResources final {
public:
    explicit RoundedEffectGpuResources(RoundedEffectGpuApi& api) noexcept;
    RoundedEffectGpuResources(const RoundedEffectGpuResources&) = delete;
    RoundedEffectGpuResources& operator=(const RoundedEffectGpuResources&) = delete;
    RoundedEffectGpuResources(RoundedEffectGpuResources&&) = delete;
    RoundedEffectGpuResources& operator=(RoundedEffectGpuResources&&) = delete;
    ~RoundedEffectGpuResources();

    void abandon_device() noexcept {
        buffer_ = nullptr;
        capacity_ = 0;
        instance_count_ = 0;
        metrics_.reset();
        instances_.clear();
    }

    void synchronize(graphics::RoundedEffectStore& store, SceneDeviceMetrics metrics);
    void invalidate_upload() noexcept;

    [[nodiscard]] RoundedEffectGpuBufferHandle buffer() const noexcept;
    [[nodiscard]] std::uint32_t capacity() const noexcept;
    [[nodiscard]] std::uint32_t instance_count() const noexcept;
    [[nodiscard]] std::span<const RoundedEffectGpuInstance> instances() const noexcept;
    [[nodiscard]] const RoundedEffectGpuResourceCounters& counters() const noexcept;

private:
    void synchronize_validated(graphics::RoundedEffectStore& store, SceneDeviceMetrics metrics);
    static void append_dirty_range(std::vector<graphics::RoundedEffectInstanceRange>& ranges,
                                   graphics::RoundedEffectInstanceRange range);
    [[nodiscard]] static std::uint32_t next_capacity(std::size_t required);
    void convert_range(std::span<const graphics::RoundedEffectInstance> source,
                       graphics::RoundedEffectInstanceRange range, SceneDeviceMetrics metrics);

    RoundedEffectGpuApi* api_;
    RoundedEffectGpuBufferHandle buffer_{nullptr};
    std::uint32_t capacity_{};
    std::uint32_t instance_count_{};
    std::vector<RoundedEffectGpuInstance> instances_;
    std::vector<graphics::RoundedEffectInstanceRange> dirty_ranges_;
    std::optional<SceneDeviceMetrics> metrics_;
    RoundedEffectGpuResourceCounters counters_;
};

} // namespace ryn::detail
