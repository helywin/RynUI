#include "renderer/sdl/glyph_gpu_resources.hpp"

#include <algorithm>
#include <cstring>
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

[[nodiscard]] std::uint32_t align_up(
    std::uint32_t value,
    std::uint32_t alignment) {
    const std::uint64_t aligned =
        (static_cast<std::uint64_t>(value) + alignment - 1U) / alignment * alignment;
    if (aligned > std::numeric_limits<std::uint32_t>::max()) {
        throw std::length_error("Glyph texture row pitch exceeds uint32_t");
    }
    return static_cast<std::uint32_t>(aligned);
}

[[nodiscard]] std::vector<graphics::GlyphInstanceRange> dirty_ranges(
    const graphics::GlyphInstanceStore& instances) {
    std::vector<graphics::GlyphInstanceRange> ranges;
    ranges.insert(
        ranges.end(),
        instances.material_dirty_ranges().begin(),
        instances.material_dirty_ranges().end());
    ranges.insert(
        ranges.end(),
        instances.geometry_dirty_ranges().begin(),
        instances.geometry_dirty_ranges().end());
    std::ranges::sort(ranges, {}, &graphics::GlyphInstanceRange::first);

    std::vector<graphics::GlyphInstanceRange> merged;
    for (const auto range : ranges) {
        if (range.count == 0) {
            continue;
        }
        if (merged.empty()) {
            merged.push_back(range);
            continue;
        }
        auto& previous = merged.back();
        const std::uint64_t previous_end =
            static_cast<std::uint64_t>(previous.first) + previous.count;
        const std::uint64_t range_end =
            static_cast<std::uint64_t>(range.first) + range.count;
        if (range.first <= previous_end) {
            previous.count = static_cast<std::uint32_t>(
                std::max(previous_end, range_end) - previous.first);
        } else {
            merged.push_back(range);
        }
    }
    return merged;
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
    graphics::GlyphInstanceStore& instances) {
    ensure_textures(atlas);
    const bool replaced_buffer = ensure_instance_buffer(instances);
    upload_atlas(atlas);
    if (!replaced_buffer) {
        upload_instance_ranges(instances);
    } else {
        instances.clear_dirty_ranges();
    }
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
        textures_.push_back(texture);
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
    const std::size_t byte_count = instances.size() * sizeof(graphics::GlyphInstance);
    auto replacement = api_->create_glyph_buffer(byte_count);
    if (replacement == nullptr) {
        throw gpu_failure(*api_, "Failed to create Glyph instance buffer");
    }
    const auto bytes = instances.bytes({
        0,
        static_cast<std::uint32_t>(instances.size()),
    });
    if (!api_->upload_glyph_buffer(replacement, 0, bytes)) {
        api_->release_glyph_buffer(replacement);
        throw gpu_failure(*api_, "Failed to upload Glyph instance buffer");
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
        const std::uint32_t row_pitch = align_up(
            plan.rectangle.width,
            glyph_texture_row_alignment);
        const std::uint64_t transfer_size =
            static_cast<std::uint64_t>(row_pitch) * plan.rectangle.height;
        if (transfer_size > std::numeric_limits<std::size_t>::max()) {
            throw std::length_error("Glyph texture upload exceeds size_t");
        }
        std::vector<std::byte> staging(static_cast<std::size_t>(transfer_size));
        const auto page = atlas.page_bytes(plan.page);
        for (std::uint32_t row = 0; row < plan.rectangle.height; ++row) {
            const std::size_t source = plan.source_offset
                + static_cast<std::size_t>(row) * plan.source_row_pitch;
            const std::size_t destination = static_cast<std::size_t>(row) * row_pitch;
            std::memcpy(
                staging.data() + destination,
                page.data() + source,
                plan.rectangle.width);
        }
        const GlyphTextureUpload upload{
            plan.page,
            plan.rectangle,
            0,
            row_pitch,
            plan.rectangle.height,
            staging,
        };
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
    for (const auto range : dirty_ranges(instances)) {
        const auto bytes = instances.bytes(range);
        const std::size_t offset =
            static_cast<std::size_t>(range.first) * sizeof(graphics::GlyphInstance);
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
