#pragma once

#include "renderer/common/effect_paint_packing.hpp"

#include <cstddef>
#include <cstdint>
#include <optional>
#include <span>
#include <vector>

namespace ryn::detail {

struct SceneBackendCapabilities;

using RoundedEffectGpuBufferHandle = void*;

class RoundedEffectGpuApi {
public:
    virtual ~RoundedEffectGpuApi() = default;

    virtual RoundedEffectGpuBufferHandle create_effect_buffer(std::size_t size) = 0;
    virtual bool upload_effect_buffer(RoundedEffectGpuBufferHandle buffer, std::size_t offset,
                                      std::span<const std::byte> bytes) = 0;
    virtual void release_effect_buffer(RoundedEffectGpuBufferHandle buffer) noexcept = 0;
    [[nodiscard]] virtual const char* effect_gpu_error() const noexcept = 0;

    // Unsupported backends retain their existing implementation and default-off capabilities.
    virtual RoundedEffectGpuBufferHandle create_effect_paint_buffer(EffectPaintBufferKind, std::size_t) {
        return nullptr;
    }

    virtual bool upload_effect_paint_buffer(EffectPaintBufferKind, RoundedEffectGpuBufferHandle, std::size_t,
                                            std::span<const std::byte>) {
        return false;
    }

    virtual void release_effect_paint_buffer(EffectPaintBufferKind, RoundedEffectGpuBufferHandle) noexcept {}
};

struct RoundedEffectDrawBinding final {
    RoundedEffectGpuBufferHandle base{};
    RoundedEffectGpuBufferHandle link{};
    RoundedEffectGpuBufferHandle field{};
    RoundedEffectGpuBufferHandle stop{};
    std::size_t base_byte_offset{};
    std::size_t link_byte_offset{};
    std::uint32_t instance_count{};
    std::uint32_t field_count{};
    std::uint32_t stop_count{};
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
        paint_buffers_ = {};
        paint_capacities_ = {};
        paint_tables_.links.clear();
        paint_tables_.fields.clear();
        paint_tables_.stops.clear();
    }

    static void preflight(const graphics::RoundedEffectStore* store, SceneDeviceMetrics metrics,
                          const SceneBackendCapabilities& capabilities,
                          const RoundedEffectGpuResources* current = nullptr);
    void synchronize(graphics::RoundedEffectStore& store, SceneDeviceMetrics metrics, bool paint_transport = false);
    void invalidate_upload() noexcept;

    [[nodiscard]] RoundedEffectDrawBinding draw_binding(std::uint32_t first, std::uint32_t count) const;

    [[nodiscard]] const EffectPaintPackedTables& paint_tables() const noexcept {
        return paint_tables_;
    }

    [[nodiscard]] const std::array<std::uint32_t, 3>& paint_capacities() const noexcept {
        return paint_capacities_;
    }

    [[nodiscard]] RoundedEffectGpuBufferHandle buffer() const noexcept;
    [[nodiscard]] std::uint32_t capacity() const noexcept;
    [[nodiscard]] std::uint32_t instance_count() const noexcept;
    [[nodiscard]] std::span<const RoundedEffectGpuInstance> instances() const noexcept;
    [[nodiscard]] const RoundedEffectGpuResourceCounters& counters() const noexcept;

private:
    void synchronize_validated(graphics::RoundedEffectStore& store, SceneDeviceMetrics metrics, bool paint_transport);
    void release_paint_buffers() noexcept;
    void merge_dirty_ranges(std::span<const graphics::RoundedEffectInstanceRange> geometry,
                            std::span<const graphics::RoundedEffectInstanceRange> material);
    void convert_range(std::span<const graphics::RoundedEffectInstance> source,
                       graphics::RoundedEffectInstanceRange range, SceneDeviceMetrics metrics, bool paint_transport);

    RoundedEffectGpuApi* api_;
    RoundedEffectGpuBufferHandle buffer_{nullptr};
    std::uint32_t capacity_{};
    std::uint32_t instance_count_{};
    std::vector<RoundedEffectGpuInstance> instances_;
    std::vector<graphics::RoundedEffectInstanceRange> dirty_ranges_;
    std::optional<SceneDeviceMetrics> metrics_;
    std::array<RoundedEffectGpuBufferHandle, 3> paint_buffers_{};
    std::array<std::uint32_t, 3> paint_capacities_{};
    EffectPaintPackedTables paint_tables_;
    RoundedEffectGpuResourceCounters counters_;
};

} // namespace ryn::detail
