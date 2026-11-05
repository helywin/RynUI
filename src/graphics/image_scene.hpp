#pragma once
#include <ryn/image_source.hpp>
#include "runtime/geometry.hpp"
#include <memory>
#include <optional>
#include <span>
#include <vector>
#include <thread>

namespace ryn::graphics {
struct ImageId final {
    std::uint32_t index{};
    std::uint32_t generation{};
    friend bool operator==(ImageId, ImageId) = default;
};

struct ImageInstance final {
    std::shared_ptr<const ImagePixels> pixels;
    runtime::Rect bounds;
    runtime::Rect clip;
    runtime::Point pivot;
    float angle_degrees{};
    float opacity{1};
    bool flip_x{};
    bool flip_y{};
    std::optional<runtime::Rect> circle_clip;
    friend bool operator==(const ImageInstance&, const ImageInstance&) = default;
};

struct ImageSlot final {
    std::uint32_t generation{1};
    std::uint64_t revision{};
    bool live{};
    ImageInstance instance;
};

// Sparse retained identities. No GPU fields or backend knowledge.
class ImageStore final {
public:
    ImageId create(ImageInstance instance);
    bool update(ImageId id, ImageInstance instance);
    bool destroy(ImageId id);
    [[nodiscard]] const ImageSlot* find(ImageId id) const;

    [[nodiscard]] std::span<const ImageSlot> slots() const noexcept {
        return slots_;
    }

    [[nodiscard]] std::size_t live_count() const noexcept;

private:
    void require_owner() const;
    std::thread::id owner_{std::this_thread::get_id()};
    std::vector<ImageSlot> slots_;
};

void validate_image_instance(const ImageInstance& instance);
} // namespace ryn::graphics
