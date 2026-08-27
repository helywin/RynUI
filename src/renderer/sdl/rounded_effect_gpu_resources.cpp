#include "renderer/sdl/rounded_effect_gpu_resources.hpp"

#include <algorithm>
#include <bit>
#include <limits>
#include <stdexcept>
#include <string>

namespace ryn::detail {
namespace {

[[nodiscard]] std::runtime_error gpu_failure(
    const RoundedEffectGpuApi& api,
    const char* fallback) {
    const char* error = api.effect_gpu_error();
    return std::runtime_error(
        error != nullptr && error[0] != '\0' ? error : fallback);
}

} // namespace

RoundedEffectGpuResources::RoundedEffectGpuResources(
    RoundedEffectGpuApi& api) noexcept
    : api_(&api) {}

RoundedEffectGpuResources::~RoundedEffectGpuResources() {
    if (buffer_ != nullptr) {
        api_->release_effect_buffer(buffer_);
    }
}

void RoundedEffectGpuResources::synchronize(
    graphics::RoundedEffectStore& store,
    graphics::RoundedEffectDeviceMetrics metrics) {
    graphics::validate_rounded_effect_device_metrics(metrics);
    const bool compacted = store.compact(
        graphics::rounded_effect_logical_viewport(metrics));
    const auto source = store.packed_instances();
    if (source.empty()) {
        instances_.clear();
        instance_count_ = 0;
        metrics_ = metrics;
        store.clear_dirty_ranges();
        ++counters_.zero_effect_synchronizations;
        return;
    }
    if (source.size() > std::numeric_limits<std::uint32_t>::max()) {
        throw std::length_error("Rounded effect GPU instance count exceeds uint32_t");
    }

    const bool metrics_changed = !metrics_.has_value() || *metrics_ != metrics;
    const bool needs_growth = source.size() > capacity_;
    const bool full_upload = compacted || metrics_changed || needs_growth;
    instances_.resize(source.size());
    dirty_ranges_.clear();
    if (full_upload) {
        const graphics::RoundedEffectInstanceRange range{
            0,
            static_cast<std::uint32_t>(source.size()),
        };
        convert_range(source, range, metrics);
        dirty_ranges_.push_back(range);
    } else {
        for (const auto range : store.geometry_dirty_ranges()) {
            append_dirty_range(dirty_ranges_, range);
        }
        for (const auto range : store.material_dirty_ranges()) {
            append_dirty_range(dirty_ranges_, range);
        }
        if (dirty_ranges_.empty()) {
            instance_count_ = static_cast<std::uint32_t>(source.size());
            metrics_ = metrics;
            ++counters_.idle_synchronizations;
            return;
        }
        for (const auto range : dirty_ranges_) {
            convert_range(source, range, metrics);
        }
    }

    RoundedEffectGpuBufferHandle upload_buffer = buffer_;
    std::uint32_t upload_capacity = capacity_;
    if (needs_growth) {
        upload_capacity = next_capacity(source.size());
        const auto byte_capacity = static_cast<std::size_t>(upload_capacity)
            * sizeof(graphics::RoundedEffectGpuInstance);
        upload_buffer = api_->create_effect_buffer(byte_capacity);
        if (upload_buffer == nullptr) {
            throw gpu_failure(*api_, "Failed to create rounded-effect GPU buffer");
        }
    }

    for (const auto range : dirty_ranges_) {
        const auto bytes = std::as_bytes(std::span(instances_).subspan(
            range.first, range.count));
        const auto offset = static_cast<std::size_t>(range.first)
            * sizeof(graphics::RoundedEffectGpuInstance);
        if (!api_->upload_effect_buffer(upload_buffer, offset, bytes)) {
            if (needs_growth) {
                api_->release_effect_buffer(upload_buffer);
            }
            throw gpu_failure(*api_, "Failed to upload rounded-effect GPU buffer");
        }
        ++counters_.buffer_uploads;
        counters_.uploaded_bytes += bytes.size();
        if (full_upload) {
            ++counters_.full_uploads;
        } else {
            ++counters_.partial_uploads;
        }
    }

    if (needs_growth) {
        if (buffer_ != nullptr) {
            api_->release_effect_buffer(buffer_);
        }
        buffer_ = upload_buffer;
        capacity_ = upload_capacity;
        ++counters_.buffer_reallocations;
    }
    instance_count_ = static_cast<std::uint32_t>(source.size());
    metrics_ = metrics;
    store.clear_dirty_ranges();
}

RoundedEffectGpuBufferHandle RoundedEffectGpuResources::buffer() const noexcept {
    return buffer_;
}

std::uint32_t RoundedEffectGpuResources::capacity() const noexcept {
    return capacity_;
}

std::uint32_t RoundedEffectGpuResources::instance_count() const noexcept {
    return instance_count_;
}

std::span<const graphics::RoundedEffectGpuInstance>
RoundedEffectGpuResources::instances() const noexcept {
    return instances_;
}

const RoundedEffectGpuResourceCounters&
RoundedEffectGpuResources::counters() const noexcept {
    return counters_;
}

void RoundedEffectGpuResources::append_dirty_range(
    std::vector<graphics::RoundedEffectInstanceRange>& ranges,
    graphics::RoundedEffectInstanceRange range) {
    if (range.count == 0) {
        return;
    }
    ranges.push_back(range);
    std::ranges::sort(ranges, {}, &graphics::RoundedEffectInstanceRange::first);
    std::size_t merged_count = 0;
    for (const auto candidate : ranges) {
        if (merged_count == 0) {
            ranges[merged_count++] = candidate;
            continue;
        }
        auto& prior = ranges[merged_count - 1];
        const auto prior_end = static_cast<std::uint64_t>(prior.first) + prior.count;
        const auto candidate_end =
            static_cast<std::uint64_t>(candidate.first) + candidate.count;
        if (candidate.first <= prior_end) {
            prior.count = static_cast<std::uint32_t>(
                std::max(prior_end, candidate_end) - prior.first);
        } else {
            ranges[merged_count++] = candidate;
        }
    }
    ranges.resize(merged_count);
}

std::uint32_t RoundedEffectGpuResources::next_capacity(std::size_t required) {
    if (required == 0 || required > std::numeric_limits<std::uint32_t>::max()) {
        throw std::length_error("Rounded effect GPU capacity is invalid");
    }
    constexpr auto maximum_power_of_two = std::uint32_t{1} << 31;
    return required > maximum_power_of_two
        ? static_cast<std::uint32_t>(required)
        : std::bit_ceil(static_cast<std::uint32_t>(required));
}

void RoundedEffectGpuResources::convert_range(
    std::span<const graphics::RoundedEffectInstance> source,
    graphics::RoundedEffectInstanceRange range,
    graphics::RoundedEffectDeviceMetrics metrics) {
    const auto end = static_cast<std::uint64_t>(range.first) + range.count;
    if (end > source.size() || end > instances_.size()) {
        throw std::out_of_range("Rounded effect dirty range is out of bounds");
    }
    for (std::uint32_t offset = 0; offset < range.count; ++offset) {
        const auto index = range.first + offset;
        instances_[index] = graphics::pack_rounded_effect_instance(source[index], metrics);
    }
}

} // namespace ryn::detail
