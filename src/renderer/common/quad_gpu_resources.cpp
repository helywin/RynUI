#include "renderer/common/quad_gpu_resources.hpp"
#include "renderer/common/scene_buffer_capacity.hpp"

#include <algorithm>
#include <limits>
#include <stdexcept>

namespace ryn::detail {
namespace {
std::runtime_error upload_error(QuadUploadApi& api, const char* fallback) {
    const char* message = api.last_error();
    return std::runtime_error(message && message[0] ? message : fallback);
}
} // namespace

QuadGpuBuffer::QuadGpuBuffer(QuadUploadApi& api, graphics::QuadInstanceStore& store, SceneDeviceMetrics metrics)
    : api_(&api) {
    if (!store.size()) {
        throw std::invalid_argument("QuadGpuBuffer requires at least one instance");
    }
    synchronize(store, metrics);
}

QuadGpuBuffer::~QuadGpuBuffer() {
    if (handle_) {
        api_->release_buffer(handle_);
    }
}

void QuadGpuBuffer::convert_range(const graphics::QuadInstanceStore& store, graphics::QuadInstanceRange range,
                                  SceneDeviceMetrics metrics) {
    if (std::uint64_t(range.first) + range.count > store.size()) {
        throw std::out_of_range("Quad GPU upload range is out of bounds");
    }
    for (std::uint32_t i = range.first; i < range.first + range.count; ++i) {
        packed_[i] = pack_quad_instance(store.at(i), metrics);
    }
}

void QuadGpuBuffer::synchronize(graphics::QuadInstanceStore& store, SceneDeviceMetrics metrics) {
    validate_scene_device_metrics(metrics);
    if (store.size() > std::numeric_limits<std::uint32_t>::max()) {
        throw std::length_error("Quad GPU capacity exceeds uint32_t");
    }
    const bool growth = store.size() > capacity_;
    const bool first = !handle_;
    const bool full = first || growth || metrics_ != metrics;
    packed_.resize(store.size());
    dirty_.clear();
    if (full) {
        dirty_.append({0, static_cast<std::uint32_t>(store.size())});
    } else {
        for (auto range : store.material_dirty_ranges()) {
            dirty_.append(range);
        }
        for (auto range : store.geometry_dirty_ranges()) {
            dirty_.append(range);
        }
    }
    auto target = handle_;
    auto target_capacity = capacity_;
    try {
        for (auto range : dirty_.ranges()) {
            convert_range(store, range, metrics);
        }
        if (growth) {
            target_capacity = quad_buffer_capacity(store.size(), capacity_);
            target = api_->create_vertex_buffer(std::size_t(target_capacity) * sizeof(QuadGpuInstance));
            if (!target) {
                throw upload_error(*api_, "Failed to create Quad GPU buffer");
            }
        }
        for (auto range : dirty_.ranges()) {
            auto bytes = std::as_bytes(std::span(packed_).subspan(range.first, range.count));
            if (!api_->upload(target, std::size_t(range.first) * sizeof(QuadGpuInstance), bytes)) {
                throw upload_error(*api_, "Failed to upload Quad GPU range");
            }
            if (!first) {
                ++counters_.range_uploads;
            }
            counters_.uploaded_bytes += bytes.size();
        }
    } catch (...) {
        if (growth && target && target != handle_) {
            api_->release_buffer(target);
        }
        metrics_.reset(); // Failed partial/full uploads must be retried even without CPU dirties.
        throw;
    }
    if (growth) {
        if (handle_) {
            api_->release_buffer(handle_);
        }
        handle_ = target;
        capacity_ = target_capacity;
        if (first) {
            ++counters_.initial_uploads;
        } else {
            ++counters_.buffer_reallocations;
        }
    }
    metrics_ = metrics;
    store.clear_dirty_ranges();
}
} // namespace ryn::detail
