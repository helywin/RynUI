#include "renderer/common/image_gpu_resources.hpp"
#include <algorithm>
#include <limits>
#include <stdexcept>

namespace ryn::detail {
ImageGpuInstance pack_image_instance(const graphics::ImageInstance& image, SceneDeviceMetrics metrics) {
    graphics::validate_image_instance(image);
    graphics::GlyphInstance logical;
    const auto b = image.bounds;
    const auto c = image.clip;
    logical.position_size = {b.x, b.y, b.width, b.height};
    logical.clip_bounds = {c.x, c.y, c.x + c.width, c.y + c.height};
    logical.uv_rect = {image.flip_x ? 1.F : 0.F, image.flip_y ? 1.F : 0.F, image.flip_x ? 0.F : 1.F,
                       image.flip_y ? 0.F : 1.F};
    logical.translation_opacity[2] = image.opacity;
    logical.transform = {image.pivot, image.angle_degrees};
    const auto p = pack_glyph_instance(logical, metrics);
    std::array<float, 4> circle{};
    if (image.circle_clip) {
        auto logical_clip = logical;
        const auto clip = *image.circle_clip;
        logical_clip.clip_bounds = {clip.x, clip.y, clip.x + clip.width, clip.y + clip.height};
        circle = pack_glyph_instance(logical_clip, metrics).clip_bounds;
    }
    return {p.position_size, p.uv_rect, p.clip_bounds, p.color, p.translation_opacity, p.rotation_basis, circle};
}

void validate_image_texture_upload(const ImageTextureUpload& u, std::uint32_t width, std::uint32_t height) {
    const auto row = std::uint64_t(u.width) * 4;
    if (!width || !height || u.width != width || u.height != height || u.source_row_pitch < row ||
        u.source_offset > u.bytes.size() || row > u.bytes.size() - u.source_offset ||
        std::uint64_t(u.height - 1) > (u.bytes.size() - u.source_offset - row) / u.source_row_pitch) {
        throw std::invalid_argument("Invalid RGBA texture source extent/stride/bytes");
    }
}

ImageGpuResources::~ImageGpuResources() {
    if (buffer_) {
        api_->release_image_buffer(buffer_);
    }
    for (auto& e : entries_) {
        if (e.texture) {
            api_->release_image_texture(e.texture);
        }
    }
}

void ImageGpuResources::abandon_device() noexcept {
    buffer_ = nullptr;
    capacity_ = 0;
    entries_.clear();
    metrics_.reset();
    retry_ = true;
}

void ImageGpuResources::preflight(const graphics::ImageStore* store, std::uint64_t bytes, std::uint32_t width,
                                  std::uint32_t height) {
    if (!store || store->live_count() == 0) {
        return;
    }
    if (store->slots().size() > bytes / sizeof(ImageGpuInstance)) {
        throw std::length_error("Image buffer exceeds backend limit");
    }
    for (const auto& e : store->slots()) {
        if (e.live && (e.instance.pixels->width() > width || e.instance.pixels->height() > height)) {
            throw std::length_error("Image texture exceeds backend input limit");
        }
    }
}

void ImageGpuResources::synchronize(const graphics::ImageStore* store, SceneDeviceMetrics metrics) {
    const auto slots = store && store->live_count() != 0 ? store->slots() : std::span<const graphics::ImageSlot>{};
    const bool full = retry_ || metrics_ != metrics;
    retry_ = true;
    if (slots.size() > capacity_) {
        auto* next = api_->create_image_buffer(slots.size() * sizeof(ImageGpuInstance));
        if (!next) {
            throw std::runtime_error("Image buffer allocation failed");
        }
        if (buffer_) {
            api_->release_image_buffer(buffer_);
        }
        buffer_ = next;
        capacity_ = slots.size();
        metrics_.reset();
    }
    entries_.resize(std::max(entries_.size(), slots.size()));
    for (std::size_t i = 0; i < entries_.size(); ++i) {
        auto& e = entries_[i];
        if (i >= slots.size() || !slots[i].live) {
            if (e.texture) {
                api_->release_image_texture(e.texture);
            }
            e = {};
            continue;
        }
        const auto& slot = slots[i];
        const auto& pixels = slot.instance.pixels;
        const bool texture_changed = !e.texture || e.pixels != pixels || e.generation != slot.generation;
        if (texture_changed) {
            auto* next = api_->create_image_texture(pixels->width(), pixels->height());
            if (!next) {
                throw std::runtime_error("Image texture allocation failed");
            }
            if (e.texture) {
                api_->release_image_texture(e.texture);
            }
            e.texture = next;
            e.pixels = pixels;
            e.generation = slot.generation;
            e.revision = 0;
        }
        if (texture_changed || full) {
            if (!api_->upload_image_texture(
                    e.texture, {pixels->bytes(), 0, pixels->row_stride(), pixels->width(), pixels->height()})) {
                throw std::runtime_error("Image texture upload failed");
            }
        }
        if (full || !metrics_ || e.revision != slot.revision) {
            const auto packed = pack_image_instance(slot.instance, metrics);
            if (!api_->upload_image_buffer(buffer_, i * sizeof(packed), std::as_bytes(std::span{&packed, 1}))) {
                throw std::runtime_error("Image instance upload failed");
            }
        }
        e.revision = slot.revision;
    }
    metrics_ = metrics;
    retry_ = false;
}

void* ImageGpuResources::texture(std::uint32_t index) const noexcept {
    return index < entries_.size() ? entries_[index].texture : nullptr;
}

bool ImageGpuResources::drawable(std::uint32_t index, std::uint32_t generation) const noexcept {
    return buffer_ && index < entries_.size() && entries_[index].texture && entries_[index].generation == generation;
}
} // namespace ryn::detail
