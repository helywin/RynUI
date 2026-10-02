#pragma once

#include "renderer/common/scene_packing.hpp"

#include <optional>

namespace ryn::detail {
using QuadGpuBufferHandle = void *;
class QuadUploadApi {
  public:
    virtual ~QuadUploadApi() = default;
    virtual QuadGpuBufferHandle create_vertex_buffer(std::size_t size) = 0;
    virtual void release_buffer(QuadGpuBufferHandle buffer) noexcept = 0;
    virtual bool upload(QuadGpuBufferHandle buffer, std::size_t offset,
                        std::span<const std::byte> bytes) = 0;
    [[nodiscard]] virtual const char *last_error() const noexcept = 0;
};
struct QuadUploadCounters {
    std::uint64_t initial_uploads{}, buffer_reallocations{}, range_uploads{}, uploaded_bytes{};
};
class QuadGpuBuffer final {
  public:
    QuadGpuBuffer(QuadUploadApi &api, graphics::QuadInstanceStore &store,
                  SceneDeviceMetrics metrics);
    QuadGpuBuffer(const QuadGpuBuffer &) = delete;
    QuadGpuBuffer &operator=(const QuadGpuBuffer &) = delete;
    ~QuadGpuBuffer();
    void abandon_device() noexcept {
        handle_ = nullptr;
        capacity_ = 0;
        metrics_.reset();
    }
    void synchronize(graphics::QuadInstanceStore &store, SceneDeviceMetrics metrics);
    [[nodiscard]] QuadGpuBufferHandle handle() const noexcept { return handle_; }
    [[nodiscard]] std::uint32_t capacity() const noexcept { return capacity_; }
    [[nodiscard]] const QuadUploadCounters &counters() const noexcept { return counters_; }

  private:
    void convert_range(const graphics::QuadInstanceStore &store, graphics::QuadInstanceRange range,
                       SceneDeviceMetrics metrics);
    QuadUploadApi *api_;
    QuadGpuBufferHandle handle_{};
    std::uint32_t capacity_{};
    std::optional<SceneDeviceMetrics> metrics_;
    QuadUploadCounters counters_;
    std::vector<QuadGpuInstance> packed_;
    graphics::DirtyRangeAccumulator<graphics::QuadInstanceRange> dirty_;
};
} // namespace ryn::detail
