#include "graphics/quad_primitive.hpp"

#include <algorithm>
#include <cmath>
#include <limits>
#include <optional>
#include <stdexcept>

namespace ryn::graphics {

void QuadInstanceStore::reserve(
    std::size_t instance_capacity,
    std::size_t dirty_range_capacity) {
    if (instance_capacity > std::numeric_limits<std::uint32_t>::max()) {
        throw std::length_error("Quad instance capacity exceeds uint32_t");
    }
    instances_.reserve(instance_capacity);
    material_dirty_ranges_.reserve(dirty_range_capacity);
    geometry_dirty_ranges_.reserve(dirty_range_capacity);
}

namespace {

std::runtime_error upload_error(QuadUploadApi& api, const char* fallback) {
    const char* message = api.last_error();
    return std::runtime_error(
        message != nullptr && message[0] != '\0' ? message : fallback);
}

void discard_shifted_dirty_ranges(
    std::vector<QuadInstanceRange>& ranges,
    std::uint32_t first) {
    std::vector<QuadInstanceRange> retained;
    retained.reserve(ranges.size());
    for (auto range : ranges) {
        if (range.first >= first) {
            continue;
        }
        const std::uint64_t end =
            static_cast<std::uint64_t>(range.first) + range.count;
        if (end > first) {
            range.count = first - range.first;
        }
        if (range.count != 0) {
            retained.push_back(range);
        }
    }
    ranges = std::move(retained);
}

} // namespace

QuadPrimitive QuadInstanceStore::add(runtime::NodeId node, QuadInstance instance) {
    if (!node.valid()) {
        throw std::invalid_argument("QuadPrimitive requires a valid NodeId");
    }
    if (instances_.size() >= std::numeric_limits<std::uint32_t>::max()) {
        throw std::length_error("QuadInstanceStore exhausted instance indices");
    }
    const auto range = append(std::span<const QuadInstance>{&instance, 1});
    return QuadPrimitive{node, range.first};
}

QuadInstanceRange QuadInstanceStore::append(
    std::span<const QuadInstance> instances) {
    if (instances_.size() + instances.size()
            > std::numeric_limits<std::uint32_t>::max()) {
        throw std::length_error("QuadInstanceStore exhausted instance indices");
    }
    const QuadInstanceRange range{
        static_cast<std::uint32_t>(instances_.size()),
        static_cast<std::uint32_t>(instances.size()),
    };
    instances_.insert(instances_.end(), instances.begin(), instances.end());
    mark_dirty(geometry_dirty_ranges_, range);
    return range;
}

QuadInstanceRange QuadInstanceStore::replace(
    QuadInstanceRange range,
    std::span<const QuadInstance> instances) {
    require_range(range);
    const std::uint64_t replacement_size =
        static_cast<std::uint64_t>(instances_.size()) - range.count + instances.size();
    if (replacement_size > std::numeric_limits<std::uint32_t>::max()) {
        throw std::length_error("QuadInstanceStore exhausted instance indices");
    }
    if (range.count == 0 && instances.empty()) {
        return {range.first, 0};
    }

    std::vector<QuadInstance> replacement;
    replacement.reserve(static_cast<std::size_t>(replacement_size));
    replacement.insert(
        replacement.end(), instances_.begin(), instances_.begin() + range.first);
    replacement.insert(replacement.end(), instances.begin(), instances.end());
    replacement.insert(
        replacement.end(),
        instances_.begin() + range.first + range.count,
        instances_.end());
    instances_.swap(replacement);

    if (range.count == instances.size()) {
        mark_dirty(
            geometry_dirty_ranges_,
            {range.first, static_cast<std::uint32_t>(instances.size())});
    } else {
        discard_shifted_dirty_ranges(material_dirty_ranges_, range.first);
        discard_shifted_dirty_ranges(geometry_dirty_ranges_, range.first);
        mark_dirty(
            geometry_dirty_ranges_,
            {
                range.first,
                static_cast<std::uint32_t>(instances_.size() - range.first),
            });
    }
    return {range.first, static_cast<std::uint32_t>(instances.size())};
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

std::size_t QuadInstanceStore::capacity() const noexcept {
    return instances_.capacity();
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

std::span<const std::byte> QuadInstanceStore::bytes(
    QuadInstanceRange range) const {
    require_range(range);
    return std::as_bytes(std::span(instances_).subspan(range.first, range.count));
}

std::size_t QuadInstanceStore::update_material(
    QuadInstanceRange range,
    std::span<const QuadMaterial> materials) {
    require_range(range);
    if (materials.size() != range.count) {
        throw std::invalid_argument("Quad material count must match its instance range");
    }
    for (const auto& material : materials) {
        if (!std::ranges::all_of(material.color, [](float value) {
                return std::isfinite(value);
            }) || !std::isfinite(material.opacity)
                || material.opacity < 0.0F || material.opacity > 1.0F) {
            throw std::invalid_argument("Quad material values are invalid");
        }
    }
    std::size_t updated = 0;
    std::optional<std::uint32_t> dirty_start;
    for (std::uint32_t offset = 0; offset < range.count; ++offset) {
        const auto& material = materials[offset];
        const std::uint32_t index = range.first + offset;
        auto& instance = instances_[index];
        if (instance.color == material.color && instance.opacity == material.opacity) {
            if (dirty_start.has_value()) {
                mark_dirty(
                    material_dirty_ranges_,
                    {*dirty_start, index - *dirty_start});
                dirty_start.reset();
            }
            continue;
        }
        instance.color = material.color;
        instance.opacity = material.opacity;
        dirty_start = dirty_start.value_or(index);
        ++updated;
    }
    if (dirty_start.has_value()) {
        mark_dirty(
            material_dirty_ranges_,
            {*dirty_start, range.first + range.count - *dirty_start});
    }
    return updated;
}

std::size_t QuadInstanceStore::update_geometry(
    QuadInstanceRange range,
    std::span<const QuadGeometry> geometry) {
    require_range(range);
    if (geometry.size() != range.count) {
        throw std::invalid_argument("Quad geometry count must match its instance range");
    }
    for (const auto& value : geometry) {
        if (!std::ranges::all_of(value.clip_rect, [](float item) {
                return std::isfinite(item);
            }) || !std::ranges::all_of(value.translation, [](float item) {
                return std::isfinite(item);
            }) || !std::isfinite(value.corner_radius)
                || value.corner_radius < 0.0F || value.corner_radius > 0.5F) {
            throw std::invalid_argument("Quad geometry values are invalid");
        }
    }
    std::size_t updated = 0;
    std::optional<std::uint32_t> dirty_start;
    for (std::uint32_t offset = 0; offset < range.count; ++offset) {
        const auto& value = geometry[offset];
        const std::uint32_t index = range.first + offset;
        auto& instance = instances_[index];
        if (instance.clip_rect == value.clip_rect
                && instance.corner_radius == value.corner_radius
                && instance.translation == value.translation) {
            if (dirty_start.has_value()) {
                mark_dirty(
                    geometry_dirty_ranges_,
                    {*dirty_start, index - *dirty_start});
                dirty_start.reset();
            }
            continue;
        }
        instance.clip_rect = value.clip_rect;
        instance.corner_radius = value.corner_radius;
        instance.translation = value.translation;
        dirty_start = dirty_start.value_or(index);
        ++updated;
    }
    if (dirty_start.has_value()) {
        mark_dirty(
            geometry_dirty_ranges_,
            {*dirty_start, range.first + range.count - *dirty_start});
    }
    return updated;
}

std::span<const QuadInstanceRange>
QuadInstanceStore::material_dirty_ranges() const noexcept {
    return material_dirty_ranges_;
}

std::span<const QuadInstanceRange>
QuadInstanceStore::geometry_dirty_ranges() const noexcept {
    return geometry_dirty_ranges_;
}

void QuadInstanceStore::clear_dirty_ranges() noexcept {
    material_dirty_ranges_.clear();
    geometry_dirty_ranges_.clear();
}

void QuadInstanceStore::mark_dirty(
    std::vector<QuadInstanceRange>& ranges,
    QuadInstanceRange range) {
    if (range.count == 0) {
        return;
    }
    ranges.push_back(range);
    std::ranges::sort(ranges, {}, &QuadInstanceRange::first);
    std::size_t merged_size = 0;
    for (const auto candidate : ranges) {
        if (merged_size == 0) {
            ranges[merged_size++] = candidate;
            continue;
        }
        auto& prior = ranges[merged_size - 1];
        const std::uint64_t prior_end =
            static_cast<std::uint64_t>(prior.first) + prior.count;
        const std::uint64_t candidate_end =
            static_cast<std::uint64_t>(candidate.first) + candidate.count;
        if (candidate.first <= prior_end) {
            prior.count = static_cast<std::uint32_t>(
                std::max(prior_end, candidate_end) - prior.first);
        } else {
            ranges[merged_size++] = candidate;
        }
    }
    ranges.resize(merged_size);
}

void QuadInstanceStore::require_range(QuadInstanceRange range) const {
    const std::uint64_t end = static_cast<std::uint64_t>(range.first) + range.count;
    if (end > instances_.size()) {
        throw std::out_of_range("Quad instance range is out of bounds");
    }
}

QuadGpuBuffer::QuadGpuBuffer(QuadUploadApi& api, QuadInstanceStore& store)
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
    store.clear_dirty_ranges();
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

void QuadGpuBuffer::synchronize(QuadInstanceStore& store) {
    if (store.size() > capacity_) {
        const std::uint64_t doubled = static_cast<std::uint64_t>(capacity_) * 2U;
        const std::uint64_t requested = std::max<std::uint64_t>(store.size(), doubled);
        if (requested > std::numeric_limits<std::uint32_t>::max()) {
            throw std::length_error("Quad GPU buffer capacity exceeds uint32_t");
        }
        const auto replacement_capacity = static_cast<std::uint32_t>(requested);
        auto* replacement = api_->create_vertex_buffer(
            static_cast<std::size_t>(replacement_capacity) * sizeof(QuadInstance));
        if (replacement == nullptr) {
            throw upload_error(*api_, "Failed to grow Quad GPU buffer");
        }
        if (!api_->upload(
                replacement,
                0,
                store.bytes(0, static_cast<std::uint32_t>(store.size())))) {
            api_->release_buffer(replacement);
            throw upload_error(*api_, "Failed to upload grown Quad GPU buffer");
        }
        api_->release_buffer(handle_);
        handle_ = replacement;
        capacity_ = replacement_capacity;
        ++counters_.buffer_reallocations;
        ++counters_.range_uploads;
        counters_.uploaded_bytes += store.size() * sizeof(QuadInstance);
        store.clear_dirty_ranges();
        return;
    }

    dirty_scratch_.assign(
        store.material_dirty_ranges().begin(),
        store.material_dirty_ranges().end());
    dirty_scratch_.insert(
        dirty_scratch_.end(),
        store.geometry_dirty_ranges().begin(),
        store.geometry_dirty_ranges().end());
    std::ranges::sort(dirty_scratch_, {}, &QuadInstanceRange::first);
    std::size_t merged_count = 0;
    for (const auto range : dirty_scratch_) {
        if (range.count == 0) {
            continue;
        }
        if (merged_count == 0) {
            dirty_scratch_[merged_count++] = range;
            continue;
        }
        auto& prior = dirty_scratch_[merged_count - 1];
        const std::uint64_t prior_end =
            static_cast<std::uint64_t>(prior.first) + prior.count;
        const std::uint64_t range_end =
            static_cast<std::uint64_t>(range.first) + range.count;
        if (range.first <= prior_end) {
            prior.count = static_cast<std::uint32_t>(
                std::max(prior_end, range_end) - prior.first);
        } else {
            dirty_scratch_[merged_count++] = range;
        }
    }
    for (std::size_t index = 0; index < merged_count; ++index) {
        upload_range(
            store,
            dirty_scratch_[index].first,
            dirty_scratch_[index].count);
    }
    store.clear_dirty_ranges();
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
