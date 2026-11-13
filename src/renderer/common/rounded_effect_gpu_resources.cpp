#include "renderer/common/rounded_effect_gpu_resources.hpp"
#include "renderer/common/scene_capabilities.hpp"

#include <algorithm>
#include <cmath>
#include <limits>
#include <stdexcept>
#include <string>

namespace ryn::detail {
namespace {

[[nodiscard]] std::runtime_error gpu_failure(const RoundedEffectGpuApi& api, const char* fallback) {
    const char* error = api.effect_gpu_error();
    return std::runtime_error(error != nullptr && error[0] != '\0' ? error : fallback);
}

constexpr std::array table_kinds{EffectPaintBufferKind::link, EffectPaintBufferKind::field,
                                 EffectPaintBufferKind::stop};
constexpr std::array table_strides{effect_paint_link_stride, effect_paint_field_stride, effect_paint_stop_stride};

bool needs_paint_transport(std::span<const graphics::RoundedEffectInstance> source) {
    return std::ranges::any_of(source, [](const auto& instance) {
        const auto* arc = std::get_if<graphics::RoundedArcPath>(&instance.geometry.path);
        return instance.material.paint || (arc && arc->cap != graphics::ArcCap::Round);
    });
}

} // namespace

RoundedEffectGpuResources::RoundedEffectGpuResources(RoundedEffectGpuApi& api) noexcept : api_(&api) {}

RoundedEffectGpuResources::~RoundedEffectGpuResources() {
    release_cap_buffer();
    release_paint_buffers();
    if (buffer_ != nullptr) {
        api_->release_effect_buffer(buffer_);
    }
}

void RoundedEffectGpuResources::release_paint_buffers() noexcept {
    for (std::size_t index = 0; index < paint_buffers_.size(); ++index) {
        if (paint_buffers_[index]) {
            api_->release_effect_paint_buffer(table_kinds[index], paint_buffers_[index]);
        }
    }
    paint_buffers_ = {};
    paint_capacities_ = {};
    paint_tables_.links.clear();
    paint_tables_.fields.clear();
    paint_tables_.stops.clear();
}

void RoundedEffectGpuResources::release_cap_buffer() noexcept {
    if (cap_buffer_) {
        api_->release_cap_geometry_buffer(cap_buffer_);
    }
    cap_buffer_ = nullptr;
    cap_capacity_ = 0;
    cap_geometry_.clear();
}

void RoundedEffectGpuResources::preflight(const graphics::RoundedEffectStore* store, SceneDeviceMetrics metrics,
                                          const SceneBackendCapabilities& capabilities,
                                          const RoundedEffectGpuResources* current) {
    validate_scene_device_metrics(metrics);
    const auto requirements = store ? store->live_requirements() : graphics::RoundedEffectRequirements{};
    validate_scene_effect_requirements(capabilities, requirements);
    static_cast<void>(effect_buffer_capacity(store ? store->live_count() : 0, current ? current->capacity_ : 0,
                                             sizeof(RoundedEffectGpuInstance), capabilities.maximum_buffer_bytes));
    if (!store || (!requirements.paint && !requirements.nonround_caps)) {
        return;
    }
    std::size_t field_count{};
    std::size_t stop_count{};
    for (const auto id : store->live_ids()) {
        const auto& instance = store->at(id);
        const auto check_field = [&](const graphics::EffectPaintField& field) {
            const bool linear = std::holds_alternative<graphics::EffectLinearDomain>(field.domain());
            if ((linear && !capabilities.effect_linear_fields) || (!linear && !capabilities.effect_conic_fields)) {
                throw std::invalid_argument("Backend lacks required Effect paint Linear/Conic field capability");
            }
            if (field_count >= effect_paint_no_field || stop_count > effect_paint_no_field) {
                throw std::length_error("Effect paint table indices exceed uint32_t");
            }
            const auto packed = pack_effect_paint_field(field, instance.geometry.translation, metrics,
                                                        static_cast<std::uint32_t>(stop_count));
            ++field_count;
            // pack_effect_paint_field checked the uint64 span before this addition.
            stop_count += packed.stop_count;
        };
        if (instance.material.paint) {
            check_field(instance.material.paint->foreground);
            if (instance.material.paint->underlay) {
                if (!capabilities.effect_underlays) {
                    throw std::invalid_argument("Backend lacks required Effect paint underlay capability");
                }
                check_field(*instance.material.paint->underlay);
            }
        }
        const auto base = pack_effect_paint_base_instance(instance, metrics);
        for (const auto& group : {base.clip_rect, base.shape_rect, base.clip_bounds, base.color, base.shadow_params,
                                  base.effect_params, base.material_params}) {
            if (!std::ranges::all_of(group, [](float value) { return std::isfinite(value); })) {
                throw std::invalid_argument("Effect paint packed base domain overflows");
            }
        }
    }
    const std::array required{store->live_count(), std::max(std::size_t{1}, field_count),
                              std::max(std::size_t{1}, stop_count)};
    for (std::size_t index = 0; index < required.size(); ++index) {
        static_cast<void>(effect_buffer_capacity(required[index], current ? current->paint_capacities_[index] : 0,
                                                 table_strides[index], capabilities.maximum_buffer_bytes));
    }
    if (requirements.nonround_caps) {
        static_cast<void>(effect_buffer_capacity(store->live_count(), current ? current->cap_capacity_ : 0,
                                                 sizeof(CapGeometryGpuV1), capabilities.maximum_buffer_bytes));
    }
}

void RoundedEffectGpuResources::invalidate_upload() noexcept {
    metrics_.reset();
}

void RoundedEffectGpuResources::synchronize(graphics::RoundedEffectStore& store, SceneDeviceMetrics metrics,
                                            bool paint_transport) {
    try {
        validate_scene_device_metrics(metrics);
        synchronize_validated(store, metrics, paint_transport);
    } catch (...) {
        // A failed upload may already have changed buffer bytes. Even returning
        // to the previous successful metrics must rebuild the entire projection.
        metrics_.reset();
        throw;
    }
}

void RoundedEffectGpuResources::synchronize_validated(graphics::RoundedEffectStore& store, SceneDeviceMetrics metrics,
                                                      bool paint_transport) {
    const bool compacted =
        store.compact(scene_logical_viewport(metrics), std::max(1.0, double(1.0F / metrics.display_scale)));
    const auto source = store.packed_instances();
    if (source.empty()) {
        release_cap_buffer();
        release_paint_buffers();
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

    const bool active_paint = needs_paint_transport(source);
    const bool active_caps = std::ranges::any_of(source, needs_cap_geometry);
    if (active_paint && !paint_transport) {
        throw std::invalid_argument("Legacy Effect resources cannot encode paint/caps");
    }
    const bool metrics_changed = !metrics_.has_value() || *metrics_ != metrics;
    const bool needs_growth = source.size() > capacity_;
    const bool paint_mode_changed = !paint_tables_.links.empty() != active_paint;
    const bool full_upload = compacted || metrics_changed || needs_growth || active_paint || paint_mode_changed;
    instances_.resize(source.size());
    dirty_ranges_.clear();
    // Read both queues before packing/clearing any part of this projection.
    merge_dirty_ranges(store.geometry_dirty_ranges(), store.material_dirty_ranges());
    if (!compacted && !metrics_changed && !needs_growth && dirty_ranges_.empty() && !paint_mode_changed) {
        ++counters_.idle_synchronizations;
        return;
    }
    if (full_upload) {
        dirty_ranges_.clear();
        const graphics::RoundedEffectInstanceRange range{
            0,
            static_cast<std::uint32_t>(source.size()),
        };
        convert_range(source, range, metrics, active_paint);
        dirty_ranges_.push_back(range);
    } else {
        for (const auto range : dirty_ranges_) {
            convert_range(source, range, metrics, false);
        }
    }
    if (active_paint) {
        pack_effect_paint_tables(source, metrics, paint_tables_);
    }
    if (active_caps) {
        cap_geometry_.resize(source.size());
        for (std::size_t index = 0; index < source.size(); ++index) {
            cap_geometry_[index] = pack_cap_geometry(source[index], metrics);
        }
    }

    RoundedEffectGpuBufferHandle upload_buffer = buffer_;
    std::uint32_t upload_capacity = capacity_;
    auto upload_tables = paint_buffers_;
    auto table_capacities = paint_capacities_;
    auto upload_caps = cap_buffer_;
    auto cap_capacity = cap_capacity_;
    try {
        if (needs_growth) {
            upload_capacity = effect_buffer_capacity(source.size(), capacity_, sizeof(RoundedEffectGpuInstance),
                                                     std::numeric_limits<std::size_t>::max());
            upload_buffer = api_->create_effect_buffer(std::size_t(upload_capacity) * sizeof(RoundedEffectGpuInstance));
            if (!upload_buffer) {
                throw gpu_failure(*api_, "Failed to create rounded-effect GPU buffer");
            }
        }
        const std::array table_bytes{std::as_bytes(std::span(paint_tables_.links)),
                                     std::as_bytes(std::span(paint_tables_.fields)),
                                     std::as_bytes(std::span(paint_tables_.stops))};
        if (active_paint) {
            for (std::size_t index = 0; index < upload_tables.size(); ++index) {
                const auto required = table_bytes[index].size() / table_strides[index];
                if (required > table_capacities[index]) {
                    table_capacities[index] =
                        effect_buffer_capacity(required, table_capacities[index], table_strides[index],
                                               std::numeric_limits<std::size_t>::max());
                    upload_tables[index] = api_->create_effect_paint_buffer(
                        table_kinds[index], std::size_t(table_capacities[index]) * table_strides[index]);
                    if (!upload_tables[index]) {
                        throw gpu_failure(*api_, "Failed to create Effect paint table");
                    }
                }
            }
        }
        if (active_caps && source.size() > cap_capacity) {
            cap_capacity = effect_buffer_capacity(source.size(), cap_capacity, sizeof(CapGeometryGpuV1),
                                                  std::numeric_limits<std::size_t>::max());
            upload_caps = api_->create_cap_geometry_buffer(std::size_t(cap_capacity) * sizeof(CapGeometryGpuV1));
            if (!upload_caps) {
                throw gpu_failure(*api_, "Failed to create Cap Geometry buffer");
            }
        }
        for (const auto range : dirty_ranges_) {
            const auto bytes = std::as_bytes(std::span(instances_).subspan(range.first, range.count));
            const auto offset = static_cast<std::size_t>(range.first) * sizeof(RoundedEffectGpuInstance);
            if (!api_->upload_effect_buffer(upload_buffer, offset, bytes)) {
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
        if (active_paint) {
            for (std::size_t index = 0; index < upload_tables.size(); ++index) {
                if (!api_->upload_effect_paint_buffer(table_kinds[index], upload_tables[index], 0,
                                                      table_bytes[index])) {
                    throw gpu_failure(*api_, "Failed to upload Effect paint table");
                }
                ++counters_.buffer_uploads;
                counters_.uploaded_bytes += table_bytes[index].size();
                ++counters_.full_uploads;
            }
        }
        if (active_caps) {
            const auto bytes = std::as_bytes(std::span(cap_geometry_));
            if (!api_->upload_cap_geometry_buffer(upload_caps, 0, bytes)) {
                throw gpu_failure(*api_, "Failed to upload Cap Geometry buffer");
            }
            ++counters_.buffer_uploads;
            counters_.uploaded_bytes += bytes.size();
            ++counters_.full_uploads;
        }
    } catch (...) {
        if (upload_caps && upload_caps != cap_buffer_) {
            api_->release_cap_geometry_buffer(upload_caps);
        }
        if (upload_buffer && upload_buffer != buffer_) {
            api_->release_effect_buffer(upload_buffer);
        }
        for (std::size_t index = 0; index < upload_tables.size(); ++index) {
            if (upload_tables[index] && upload_tables[index] != paint_buffers_[index]) {
                api_->release_effect_paint_buffer(table_kinds[index], upload_tables[index]);
            }
        }
        throw;
    }
    if (active_caps) {
        if (upload_caps != cap_buffer_) {
            if (cap_buffer_) {
                api_->release_cap_geometry_buffer(cap_buffer_);
            }
            ++counters_.buffer_reallocations;
        }
        cap_buffer_ = upload_caps;
        cap_capacity_ = cap_capacity;
    } else {
        release_cap_buffer();
    }
    if (active_paint) {
        for (std::size_t index = 0; index < upload_tables.size(); ++index) {
            if (upload_tables[index] != paint_buffers_[index]) {
                if (paint_buffers_[index]) {
                    api_->release_effect_paint_buffer(table_kinds[index], paint_buffers_[index]);
                }
                ++counters_.buffer_reallocations;
            }
        }
        paint_buffers_ = upload_tables;
        paint_capacities_ = table_capacities;
    } else {
        release_paint_buffers();
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

std::span<const RoundedEffectGpuInstance> RoundedEffectGpuResources::instances() const noexcept {
    return instances_;
}

const RoundedEffectGpuResourceCounters& RoundedEffectGpuResources::counters() const noexcept {
    return counters_;
}

void RoundedEffectGpuResources::merge_dirty_ranges(std::span<const graphics::RoundedEffectInstanceRange> geometry,
                                                   std::span<const graphics::RoundedEffectInstanceRange> material) {
    dirty_ranges_.reserve(geometry.size() + material.size());
    std::size_t geometry_index{};
    std::size_t material_index{};
    while (geometry_index < geometry.size() || material_index < material.size()) {
        const auto candidate =
            material_index == material.size() || (geometry_index < geometry.size() &&
                                                  geometry[geometry_index].first <= material[material_index].first)
                ? geometry[geometry_index++]
                : material[material_index++];
        if (!dirty_ranges_.empty()) {
            auto& prior = dirty_ranges_.back();
            const auto prior_end = static_cast<std::uint64_t>(prior.first) + prior.count;
            const auto candidate_end = static_cast<std::uint64_t>(candidate.first) + candidate.count;
            if (candidate.first <= prior_end) {
                prior.count = static_cast<std::uint32_t>(std::max(prior_end, candidate_end) - prior.first);
                continue;
            }
        }
        dirty_ranges_.push_back(candidate);
    }
}

void RoundedEffectGpuResources::convert_range(std::span<const graphics::RoundedEffectInstance> source,
                                              graphics::RoundedEffectInstanceRange range, SceneDeviceMetrics metrics,
                                              bool paint_transport) {
    const auto end = static_cast<std::uint64_t>(range.first) + range.count;
    if (end > source.size() || end > instances_.size()) {
        throw std::out_of_range("Rounded effect dirty range is out of bounds");
    }
    for (std::uint32_t offset = 0; offset < range.count; ++offset) {
        const auto index = range.first + offset;
        instances_[index] = paint_transport ? pack_effect_paint_base_instance(source[index], metrics)
                                            : pack_rounded_effect_instance(source[index], metrics);
    }
}

RoundedEffectDrawBinding RoundedEffectGpuResources::draw_binding(std::uint32_t first, std::uint32_t count) const {
    if (!count || std::uint64_t(first) + count > instance_count_) {
        throw std::out_of_range("Effect draw binding range exceeds packed instances");
    }
    return {buffer_,
            paint_buffers_[0],
            paint_buffers_[1],
            paint_buffers_[2],
            std::size_t(first) * sizeof(RoundedEffectGpuInstance),
            std::size_t(first) * sizeof(EffectPaintGpuLink),
            count,
            static_cast<std::uint32_t>(paint_tables_.fields.size()),
            static_cast<std::uint32_t>(paint_tables_.stops.size()),
            cap_buffer_,
            cap_buffer_ ? std::size_t(first) * sizeof(CapGeometryGpuV1) : 0};
}

} // namespace ryn::detail
