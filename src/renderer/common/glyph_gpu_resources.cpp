#include "renderer/common/glyph_gpu_resources.hpp"

#include <algorithm>
#include <limits>
#include <stdexcept>
#include <string>

namespace ryn::detail {
namespace {

[[nodiscard]] std::runtime_error gpu_failure(
    const GlyphGpuApi& api,
    const char* fallback) {
    const char* error = api.glyph_gpu_error();
    return std::runtime_error(
        error != nullptr && error[0] != '\0' ? error : fallback);
}

void dirty_ranges(const graphics::GlyphInstanceStore& instances,
    std::vector<graphics::GlyphInstanceRange>& ranges) {
    ranges.clear();
    ranges.insert(
        ranges.end(),
        instances.material_dirty_ranges().begin(),
        instances.material_dirty_ranges().end());
    ranges.insert(
        ranges.end(),
        instances.geometry_dirty_ranges().begin(),
        instances.geometry_dirty_ranges().end());
    std::ranges::sort(ranges, {}, &graphics::GlyphInstanceRange::first);

    std::size_t merged_size = 0;
    for (const auto range : ranges) {
        if (range.count == 0) {
            continue;
        }
        if (merged_size == 0) {
            ranges[merged_size++] = range;
            continue;
        }
        auto& previous = ranges[merged_size - 1];
        const std::uint64_t previous_end =
            static_cast<std::uint64_t>(previous.first) + previous.count;
        const std::uint64_t range_end =
            static_cast<std::uint64_t>(range.first) + range.count;
        if (range.first <= previous_end) {
            previous.count = static_cast<std::uint32_t>(
                std::max(previous_end, range_end) - previous.first);
        } else {
            ranges[merged_size++] = range;
        }
    }
    ranges.resize(merged_size);
}

} // namespace

GlyphGpuResources::GlyphGpuResources(GlyphGpuApi& api) : api_(&api) {
    sampler_ = api_->create_glyph_sampler();
    if (sampler_ == nullptr) {
        throw gpu_failure(*api_, "Failed to create Glyph sampler");
    }
}

GlyphGpuResources::~GlyphGpuResources() {
    for (auto texture = textures_.rbegin(); texture != textures_.rend(); ++texture) {
        api_->release_glyph_texture(*texture);
    }
    if (instance_buffer_ != nullptr) {
        api_->release_glyph_buffer(instance_buffer_);
    }
    if (sampler_ != nullptr) {
        api_->release_glyph_sampler(sampler_);
    }
}

void GlyphGpuResources::synchronize(
    graphics::GlyphAtlas& atlas,
    graphics::GlyphInstanceStore& instances, SceneDeviceMetrics metrics) {
    validate_scene_device_metrics(metrics);
    if (instances.size() > std::numeric_limits<std::uint32_t>::max())
        throw std::length_error("Glyph instance buffer exceeds uint32_t capacity");
    const bool full = metrics_ != metrics || instances.size() > instance_capacity_;
    packed_.resize(instances.size());
    try {
        if (full) {
            instances.mark_all_dirty();
            convert_range(instances, {0, static_cast<std::uint32_t>(instances.size())}, metrics);
        } else {
            dirty_ranges(instances, dirty_scratch_);
            for (auto range : dirty_scratch_) convert_range(instances, range, metrics);
        }
        ensure_textures(atlas);
        const bool replaced_buffer = ensure_instance_buffer(instances);
        upload_atlas(atlas);
        if (!replaced_buffer) upload_instance_ranges(instances);
        else instances.clear_dirty_ranges();
    } catch (...) {
        metrics_.reset();
        throw;
    }
    metrics_ = metrics;
}

void GlyphGpuResources::convert_range(const graphics::GlyphInstanceStore& instances,
    graphics::GlyphInstanceRange range, SceneDeviceMetrics metrics) {
    if (std::uint64_t(range.first) + range.count > instances.size())
        throw std::out_of_range("Glyph GPU range exceeds logical store");
    for (std::uint32_t i = range.first; i < range.first + range.count; ++i)
        packed_[i] = pack_glyph_instance(instances.at(i), metrics);
}

void GlyphGpuResources::set_sparse_upload_coalescing_limit(
    std::size_t max_span_bytes) noexcept {
    max_coalesced_upload_bytes_ = max_span_bytes;
}

GlyphGpuSamplerHandle GlyphGpuResources::sampler() const noexcept {
    return sampler_;
}

GlyphGpuTextureHandle GlyphGpuResources::texture(std::uint32_t page) const {
    return textures_.at(page);
}

GlyphGpuBufferHandle GlyphGpuResources::instance_buffer() const noexcept {
    return instance_buffer_;
}

std::uint32_t GlyphGpuResources::instance_capacity() const noexcept {
    return instance_capacity_;
}

const GlyphGpuResourceCounters& GlyphGpuResources::counters() const noexcept {
    return counters_;
}

void GlyphGpuResources::ensure_textures(const graphics::GlyphAtlas& atlas) {
    while (textures_.size() < atlas.page_count()) {
        auto texture = api_->create_glyph_texture(
            atlas.config().page_width,
            atlas.config().page_height);
        if (texture == nullptr) {
            throw gpu_failure(*api_, "Failed to create Glyph atlas texture");
        }
        try { textures_.push_back(texture); }
        catch (...) { api_->release_glyph_texture(texture); throw; }
        ++counters_.textures_created;
    }
}

bool GlyphGpuResources::ensure_instance_buffer(
    const graphics::GlyphInstanceStore& instances) {
    if (instances.size() <= instance_capacity_) {
        return false;
    }
    if (instances.size() > std::numeric_limits<std::uint32_t>::max()) {
        throw std::length_error("Glyph instance buffer exceeds uint32_t capacity");
    }
    const std::size_t byte_count = instances.size() * sizeof(GlyphGpuInstance);
    auto replacement = api_->create_glyph_buffer(byte_count);
    if (replacement == nullptr) {
        throw gpu_failure(*api_, "Failed to create Glyph instance buffer");
    }
    const auto bytes = std::as_bytes(std::span(packed_));
    try {
        if (!api_->upload_glyph_buffer(replacement, 0, bytes))
            throw gpu_failure(*api_, "Failed to upload Glyph instance buffer");
    } catch (...) {
        api_->release_glyph_buffer(replacement);
        throw;
    }
    if (instance_buffer_ != nullptr) {
        api_->release_glyph_buffer(instance_buffer_);
    }
    instance_buffer_ = replacement;
    instance_capacity_ = static_cast<std::uint32_t>(instances.size());
    ++counters_.buffer_reallocations;
    ++counters_.buffer_uploads;
    counters_.buffer_uploaded_bytes += bytes.size();
    return true;
}

void GlyphGpuResources::upload_atlas(graphics::GlyphAtlas& atlas) {
    for (const graphics::GlyphAtlasUploadPlan& plan : atlas.dirty_regions()) {
        const GlyphTextureUpload upload{
            plan.page, plan.rectangle, plan.source_offset, plan.source_row_pitch,
            std::as_bytes(atlas.page_bytes(plan.page)),
        };
        validate_glyph_texture_upload(upload, atlas.config().page_width, atlas.config().page_height);
        if (!api_->upload_glyph_texture(texture(plan.page), upload)) {
            throw gpu_failure(*api_, "Failed to upload Glyph atlas texture");
        }
        ++counters_.texture_uploads;
        counters_.texture_uploaded_bytes += plan.uploaded_bytes;
    }
    atlas.clear_dirty_regions();
}

void GlyphGpuResources::upload_instance_ranges(
    graphics::GlyphInstanceStore& instances) {
    dirty_ranges(instances, dirty_scratch_);
    if (max_coalesced_upload_bytes_ != 0 && dirty_scratch_.size() > 1) {
        const auto first = dirty_scratch_.front().first;
        const auto& last = dirty_scratch_.back();
        const auto end = static_cast<std::uint64_t>(last.first) + last.count;
        const auto span_count = end - first;
        std::uint64_t dirty_count = 0;
        for (const auto range : dirty_scratch_) {
            dirty_count += range.count;
        }
        if (span_count * sizeof(GlyphGpuInstance)
                    <= max_coalesced_upload_bytes_
                && span_count <= dirty_count * 4) {
            dirty_scratch_.front().count = static_cast<std::uint32_t>(span_count);
            dirty_scratch_.resize(1);
            ++counters_.buffer_upload_coalesces;
        }
    }
    for (const auto range : dirty_scratch_) {
        const auto bytes = std::as_bytes(std::span(packed_).subspan(range.first, range.count));
        const std::size_t offset =
            static_cast<std::size_t>(range.first) * sizeof(GlyphGpuInstance);
        if (!api_->upload_glyph_buffer(instance_buffer_, offset, bytes)) {
            throw gpu_failure(*api_, "Failed to upload Glyph instance range");
        }
        ++counters_.buffer_uploads;
        counters_.buffer_uploaded_bytes += bytes.size();
    }
    instances.clear_dirty_ranges();
}

void draw_ordered_scene(const graphics::OrderedScene& scene, SceneDrawApi& api) {
    for (const graphics::SceneDrawCommand command : scene.commands()) {
        switch (command.kind) {
        case graphics::SceneDrawKind::quad:
            api.draw_quad(command.first_instance, command.instance_count);
            break;
        case graphics::SceneDrawKind::glyph:
            api.draw_glyph(
                command.atlas_page,
                command.first_instance,
                command.instance_count);
            break;
        case graphics::SceneDrawKind::rounded_effect:
            api.draw_rounded_effect(command.first_instance, command.instance_count);
            break;
        }
    }
}

} // namespace ryn::detail
