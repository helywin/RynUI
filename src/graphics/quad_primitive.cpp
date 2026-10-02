#include "graphics/quad_primitive.hpp"

#include <algorithm>
#include <cmath>
#include <limits>
#include <optional>
#include <stdexcept>

namespace ryn::graphics {

void QuadInstanceStore::reserve(std::size_t instance_capacity, std::size_t dirty_range_capacity) {
    if (instance_capacity > std::numeric_limits<std::uint32_t>::max()) {
        throw std::length_error("Quad instance capacity exceeds uint32_t");
    }
    instances_.reserve(instance_capacity);
    material_dirty_ranges_.reserve(dirty_range_capacity);
    geometry_dirty_ranges_.reserve(dirty_range_capacity);
}

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

QuadInstanceRange QuadInstanceStore::append(std::span<const QuadInstance> instances) {
    if (instances_.size() + instances.size() > std::numeric_limits<std::uint32_t>::max()) {
        throw std::length_error("QuadInstanceStore exhausted instance indices");
    }
    const QuadInstanceRange range{
        static_cast<std::uint32_t>(instances_.size()),
        static_cast<std::uint32_t>(instances.size()),
    };
    instances_.insert(instances_.end(), instances.begin(), instances.end());
    geometry_dirty_ranges_.append(range);
    return range;
}

QuadInstanceRange QuadInstanceStore::replace(QuadInstanceRange range, std::span<const QuadInstance> instances) {
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
    replacement.insert(replacement.end(), instances_.begin(), instances_.begin() + range.first);
    replacement.insert(replacement.end(), instances.begin(), instances.end());
    replacement.insert(replacement.end(), instances_.begin() + range.first + range.count, instances_.end());
    instances_.swap(replacement);

    if (range.count == instances.size()) {
        geometry_dirty_ranges_.append({range.first, static_cast<std::uint32_t>(instances.size())});
    } else {
        material_dirty_ranges_.discard_shifted(range.first);
        geometry_dirty_ranges_.discard_shifted(range.first);
        geometry_dirty_ranges_.append({
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

std::span<const std::byte> QuadInstanceStore::bytes(std::uint32_t first, std::uint32_t count) const {
    const auto end = static_cast<std::size_t>(first) + count;
    if (end > instances_.size()) {
        throw std::out_of_range("Quad instance byte range is out of bounds");
    }
    return std::as_bytes(std::span(instances_).subspan(first, count));
}

std::span<const std::byte> QuadInstanceStore::bytes(QuadInstanceRange range) const {
    require_range(range);
    return std::as_bytes(std::span(instances_).subspan(range.first, range.count));
}

std::size_t QuadInstanceStore::update_material(QuadInstanceRange range, std::span<const QuadMaterial> materials) {
    require_range(range);
    if (materials.size() != range.count) {
        throw std::invalid_argument("Quad material count must match its instance range");
    }
    for (const auto& material : materials) {
        if (!std::ranges::all_of(material.color, [](float value) { return std::isfinite(value); }) ||
            !std::isfinite(material.opacity) || material.opacity < 0.0F || material.opacity > 1.0F) {
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
                material_dirty_ranges_.append({*dirty_start, index - *dirty_start});
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
        material_dirty_ranges_.append({*dirty_start, range.first + range.count - *dirty_start});
    }
    return updated;
}

std::size_t QuadInstanceStore::update_geometry(QuadInstanceRange range, std::span<const QuadGeometry> geometry) {
    require_range(range);
    if (geometry.size() != range.count) {
        throw std::invalid_argument("Quad geometry count must match its instance range");
    }
    for (const auto& value : geometry) {
        if (!std::ranges::all_of(value.bounds, [](float item) { return std::isfinite(item); }) ||
            !std::ranges::all_of(value.translation, [](float item) { return std::isfinite(item); }) ||
            !std::isfinite(value.corner_radius) || value.corner_radius < 0.0F || value.bounds[2] < 0.0F ||
            value.bounds[3] < 0.0F) {
            throw std::invalid_argument("Quad geometry values are invalid");
        }
    }
    std::size_t updated = 0;
    std::optional<std::uint32_t> dirty_start;
    for (std::uint32_t offset = 0; offset < range.count; ++offset) {
        const auto& value = geometry[offset];
        const std::uint32_t index = range.first + offset;
        auto& instance = instances_[index];
        if (instance.bounds == value.bounds && instance.corner_radius == value.corner_radius &&
            instance.translation == value.translation) {
            if (dirty_start.has_value()) {
                geometry_dirty_ranges_.append({*dirty_start, index - *dirty_start});
                dirty_start.reset();
            }
            continue;
        }
        instance.bounds = value.bounds;
        instance.corner_radius = value.corner_radius;
        instance.translation = value.translation;
        dirty_start = dirty_start.value_or(index);
        ++updated;
    }
    if (dirty_start.has_value()) {
        geometry_dirty_ranges_.append({*dirty_start, range.first + range.count - *dirty_start});
    }
    return updated;
}

std::span<const QuadInstanceRange> QuadInstanceStore::material_dirty_ranges() const noexcept {
    return material_dirty_ranges_.ranges();
}

std::span<const QuadInstanceRange> QuadInstanceStore::geometry_dirty_ranges() const noexcept {
    return geometry_dirty_ranges_.ranges();
}

void QuadInstanceStore::clear_dirty_ranges() noexcept {
    material_dirty_ranges_.clear();
    geometry_dirty_ranges_.clear();
}

void QuadInstanceStore::mark_all_dirty() {
    clear_dirty_ranges();
    if (!instances_.empty()) {
        geometry_dirty_ranges_.append({
            0,
            static_cast<std::uint32_t>(instances_.size()),
        });
    }
}

void QuadInstanceStore::require_range(QuadInstanceRange range) const {
    const std::uint64_t end = static_cast<std::uint64_t>(range.first) + range.count;
    if (end > instances_.size()) {
        throw std::out_of_range("Quad instance range is out of bounds");
    }
}

} // namespace ryn::graphics
