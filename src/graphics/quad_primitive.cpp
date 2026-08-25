#include "graphics/quad_primitive.hpp"

#include <limits>
#include <stdexcept>

namespace ryn::graphics {
namespace {

std::runtime_error upload_error(QuadUploadApi& api, const char* fallback) {
    const char* message = api.last_error();
    return std::runtime_error(
        message != nullptr && message[0] != '\0' ? message : fallback);
}

} // namespace

QuadPrimitive QuadInstanceStore::add(runtime::NodeId node, QuadInstance instance) {
    if (!node.valid()) {
        throw std::invalid_argument("QuadPrimitive requires a valid NodeId");
    }
    if (instances_.size() >= std::numeric_limits<std::uint32_t>::max()) {
        throw std::length_error("QuadInstanceStore exhausted instance indices");
    }
    const auto index = static_cast<std::uint32_t>(instances_.size());
    instances_.push_back(instance);
    return QuadPrimitive{node, index};
}

const QuadInstance& QuadInstanceStore::at(std::uint32_t index) const {
    return instances_.at(index);
}

QuadInstance& QuadInstanceStore::at(std::uint32_t index) {
    return instances_.at(index);
}

std::span<const QuadInstance> QuadInstanceStore::instances() const noexcept {
    return instances_;
}

std::size_t QuadInstanceStore::size() const noexcept {
    return instances_.size();
}

std::span<const std::byte> QuadInstanceStore::bytes(
    std::uint32_t first,
    std::uint32_t count) const {
    const auto end = static_cast<std::size_t>(first) + count;
    if (end > instances_.size()) {
        throw std::out_of_range("Quad instance byte range is out of bounds");
    }
    return std::as_bytes(std::span(instances_).subspan(first, count));
}

QuadGpuBuffer::QuadGpuBuffer(QuadUploadApi& api, const QuadInstanceStore& store)
    : api_(&api) {
    if (store.size() == 0) {
        throw std::invalid_argument("QuadGpuBuffer requires at least one instance");
    }
    if (store.size() > std::numeric_limits<std::uint32_t>::max()) {
        throw std::length_error("QuadGpuBuffer instance capacity exceeds uint32_t");
    }
    capacity_ = static_cast<std::uint32_t>(store.size());
    const auto byte_count = store.size() * sizeof(QuadInstance);
    handle_ = api_->create_vertex_buffer(byte_count);
    if (handle_ == nullptr) {
        throw upload_error(*api_, "Failed to create Quad GPU buffer");
    }
    if (!api_->upload(handle_, 0, store.bytes(0, capacity_))) {
        api_->release_buffer(handle_);
        handle_ = nullptr;
        throw upload_error(*api_, "Failed to upload initial Quad instances");
    }
    ++counters_.initial_uploads;
    counters_.uploaded_bytes += byte_count;
}

QuadGpuBuffer::~QuadGpuBuffer() {
    if (handle_ != nullptr) {
        api_->release_buffer(handle_);
    }
}

void QuadGpuBuffer::upload_range(
    const QuadInstanceStore& store,
    std::uint32_t first,
    std::uint32_t count) {
    const auto end = static_cast<std::uint64_t>(first) + count;
    if (count == 0 || end > capacity_ || end > store.size()) {
        throw std::out_of_range("Quad GPU upload range is out of bounds");
    }
    const auto data = store.bytes(first, count);
    const auto offset = static_cast<std::size_t>(first) * sizeof(QuadInstance);
    if (!api_->upload(handle_, offset, data)) {
        throw upload_error(*api_, "Failed to upload Quad instance range");
    }
    ++counters_.range_uploads;
    counters_.uploaded_bytes += data.size();
}

QuadGpuBufferHandle QuadGpuBuffer::handle() const noexcept {
    return handle_;
}

std::uint32_t QuadGpuBuffer::capacity() const noexcept {
    return capacity_;
}

const QuadUploadCounters& QuadGpuBuffer::counters() const noexcept {
    return counters_;
}

} // namespace ryn::graphics
