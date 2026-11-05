#include "graphics/image_scene.hpp"
#include <algorithm>
#include <cmath>
#include <limits>
#include <stdexcept>

namespace ryn::graphics {
void validate_image_instance(const ImageInstance& value) {
    const auto rect_valid = [](runtime::Rect r) {
        return std::isfinite(r.x) && std::isfinite(r.y) && std::isfinite(r.width) && std::isfinite(r.height) &&
               r.width >= 0 && r.height >= 0 && std::isfinite(r.x + r.width) && std::isfinite(r.y + r.height);
    };
    if ((value.circle_clip &&
         (!rect_valid(*value.circle_clip) || value.circle_clip->width <= 0 || value.circle_clip->height <= 0)) ||
        !value.pixels || !rect_valid(value.bounds) || !rect_valid(value.clip) || !std::isfinite(value.pivot.x) ||
        !std::isfinite(value.pivot.y) || !std::isfinite(value.angle_degrees) || !std::isfinite(value.opacity) ||
        value.opacity < 0 || value.opacity > 1) {
        throw std::invalid_argument("Invalid logical Image instance");
    }
}

void ImageStore::require_owner() const {
    if (owner_ != std::this_thread::get_id()) {
        throw std::logic_error("ImageStore requires owner thread");
    }
}

ImageId ImageStore::create(ImageInstance value) {
    require_owner();
    validate_image_instance(value);
    auto it = std::ranges::find_if(slots_, [](const auto& slot) { return !slot.live; });
    if (it == slots_.end()) {
        if (slots_.size() >= std::numeric_limits<std::uint32_t>::max()) {
            throw std::length_error("ImageStore full");
        }
        slots_.emplace_back();
        it = slots_.end() - 1;
    }
    it->instance = std::move(value);
    it->live = true;
    ++it->revision;
    return {static_cast<std::uint32_t>(it - slots_.begin()), it->generation};
}

const ImageSlot* ImageStore::find(ImageId id) const {
    require_owner();
    return id.index < slots_.size() && slots_[id.index].live && slots_[id.index].generation == id.generation
               ? &slots_[id.index]
               : nullptr;
}

bool ImageStore::update(ImageId id, ImageInstance value) {
    require_owner();
    validate_image_instance(value);
    if (!find(id)) {
        return false;
    }
    auto& slot = slots_[id.index];
    if (slot.instance != value) {
        slot.instance = std::move(value);
        ++slot.revision;
    }
    return true;
}

bool ImageStore::destroy(ImageId id) {
    require_owner();
    if (!find(id)) {
        return false;
    }
    auto& slot = slots_[id.index];
    slot.live = false;
    slot.instance = {};
    ++slot.generation;
    ++slot.revision;
    return true;
}

std::size_t ImageStore::live_count() const noexcept {
    return std::ranges::count_if(slots_, [](const auto& slot) { return slot.live; });
}
} // namespace ryn::graphics
