#include "graphics/image_scene.hpp"
#include <algorithm>
#include <array>
#include <cmath>
#include <limits>
#include <numbers>
#include <stdexcept>

namespace ryn::graphics {
void validate_image_instance(const ImageInstance& value) {
    const auto rect_valid = [](runtime::Rect r) {
        return std::isfinite(r.x) && std::isfinite(r.y) && std::isfinite(r.width) && std::isfinite(r.height) &&
               r.width >= 0 && r.height >= 0 && std::isfinite(r.x + r.width) && std::isfinite(r.y + r.height);
    };
    if ((value.circle_clip &&
         (!rect_valid(*value.circle_clip) || value.circle_clip->width <= 0 || value.circle_clip->height <= 0)) ||
        !rect_valid(value.source_uv) || value.source_uv.x < 0 || value.source_uv.y < 0 ||
        value.source_uv.x + value.source_uv.width > 1 || value.source_uv.y + value.source_uv.height > 1 ||
        !value.pixels || !rect_valid(value.bounds) || !rect_valid(value.clip) || !std::isfinite(value.pivot.x) ||
        !std::isfinite(value.pivot.y) || !std::isfinite(value.angle_degrees) || !std::isfinite(value.opacity) ||
        value.opacity < 0 || value.opacity > 1) {
        throw std::invalid_argument("Invalid logical Image instance");
    }
}

std::vector<ImageInstance> decompose_rounded_image(const ImageInstance& image, runtime::Rect canvas, float radius,
                                                   bool top_only) {
    validate_image_instance(image);
    if (!std::isfinite(radius) || radius < 0 || !std::isfinite(canvas.x) || !std::isfinite(canvas.y) ||
        !std::isfinite(canvas.width) || !std::isfinite(canvas.height) || !std::isfinite(canvas.x + canvas.width) ||
        !std::isfinite(canvas.y + canvas.height) || canvas.width < 0 || canvas.height < 0) {
        throw std::invalid_argument("Invalid rounded Image canvas");
    }
    radius = std::min(radius, std::min(canvas.width, canvas.height) / 2);
    if (canvas.width == 0 || canvas.height == 0 || image.bounds.width == 0 || image.bounds.height == 0) {
        return {};
    }
    if (radius <= 0) {
        return {image};
    }
    const auto intersect = [](runtime::Rect a, runtime::Rect b) {
        const float x = std::max(a.x, b.x);
        const float y = std::max(a.y, b.y);
        return runtime::Rect{x, y, std::max(0.F, std::min(a.x + a.width, b.x + b.width) - x),
                             std::max(0.F, std::min(a.y + a.height, b.y + b.height) - y)};
    };
    std::vector<ImageInstance> pieces;
    const auto add = [&](runtime::Rect patch, std::optional<runtime::Rect> circle = {}) {
        const auto bounds = intersect(patch, image.bounds);
        if (bounds.width <= 0 || bounds.height <= 0) {
            return;
        }
        auto piece = image;
        piece.bounds = bounds;
        piece.circle_clip = circle;
        if (circle && image.angle_degrees != 0) {
            const auto angle = std::remainder(double(image.angle_degrees), 360.) * std::numbers::pi / 180.;
            const auto x = circle->x + circle->width / 2 - image.pivot.x;
            const auto y = circle->y + circle->height / 2 - image.pivot.y;
            piece.circle_clip->x = image.pivot.x + float(std::cos(angle) * x - std::sin(angle) * y) - circle->width / 2;
            piece.circle_clip->y =
                image.pivot.y + float(std::sin(angle) * x + std::cos(angle) * y) - circle->height / 2;
        }
        const auto original = image.source_uv;
        const auto source_x = [&](float x) {
            return std::clamp(original.x + (x - image.bounds.x) / image.bounds.width * original.width, original.x,
                              original.x + original.width);
        };
        const auto source_y = [&](float y) {
            return std::clamp(original.y + (y - image.bounds.y) / image.bounds.height * original.height, original.y,
                              original.y + original.height);
        };
        const float u0 = source_x(bounds.x);
        const float v0 = source_y(bounds.y);
        const float u1 = source_x(bounds.x + bounds.width);
        const float v1 = source_y(bounds.y + bounds.height);
        piece.source_uv = {u0, v0, std::max(0.F, u1 - u0), std::max(0.F, v1 - v0)};
        pieces.push_back(std::move(piece));
    };
    // Derive shared edges once so adjacent patch geometry uses the same values.
    const float right_edge = canvas.x + canvas.width;
    const float bottom_edge = canvas.y + canvas.height;
    const float left_cut = std::min(canvas.x + radius, canvas.x + (right_edge - canvas.x) / 2);
    const float top_cut = std::min(canvas.y + radius, canvas.y + (bottom_edge - canvas.y) / 2);
    const std::array xs{canvas.x, left_cut, std::max(left_cut, right_edge - radius), right_edge};
    const std::array ys{canvas.y, top_cut, std::max(top_cut, bottom_edge - radius), bottom_edge};
    add({xs[0], ys[1], xs[3] - xs[0], ys[top_only ? 3 : 2] - ys[1]});
    add({xs[1], ys[0], std::max(0.F, xs[2] - xs[1]), ys[1] - ys[0]});
    if (!top_only) {
        add({xs[1], ys[2], std::max(0.F, xs[2] - xs[1]), ys[3] - ys[2]});
    }
    for (bool right : {false, true}) {
        for (bool bottom : {false, true}) {
            if (top_only && bottom) {
                continue;
            }
            const std::size_t x = right ? 2 : 0;
            const std::size_t y = bottom ? 2 : 0;
            add({xs[x], ys[y], xs[x + 1] - xs[x], ys[y + 1] - ys[y]},
                runtime::Rect{right ? xs[2] - radius : xs[0], bottom ? ys[2] - radius : ys[0], 2 * radius, 2 * radius});
        }
    }
    return pieces;
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
