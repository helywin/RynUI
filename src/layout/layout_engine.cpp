#include "layout/layout_engine.hpp"

#include <algorithm>
#include <cmath>
#include <stdexcept>
#include <type_traits>
#include <utility>

namespace ryn::layout {
namespace {

float horizontal_padding(const Padding& padding) noexcept {
    return padding.left + padding.right;
}

float vertical_padding(const Padding& padding) noexcept {
    return padding.top + padding.bottom;
}

float horizontal_margin(const runtime::EdgeInsets& margin) noexcept {
    return margin.left + margin.right;
}

float vertical_margin(const runtime::EdgeInsets& margin) noexcept {
    return margin.top + margin.bottom;
}

float subtract_extent(float value, float extent) noexcept {
    return std::isfinite(value) ? std::max(0.0F, value - extent) : value;
}

void apply_axis_style(
    float& minimum,
    float& maximum,
    const std::optional<float>& fixed,
    const std::optional<float>& style_minimum,
    const std::optional<float>& style_maximum) noexcept {
    if (style_minimum.has_value()) {
        minimum = std::max(minimum, *style_minimum);
    }
    if (style_maximum.has_value()) {
        maximum = std::min(maximum, *style_maximum);
    }
    if (minimum > maximum) {
        minimum = maximum;
    }
    if (fixed.has_value()) {
        const float resolved = std::clamp(*fixed, minimum, maximum);
        minimum = resolved;
        maximum = resolved;
    }
}

Constraints external_content_constraints(
    Constraints outer,
    const runtime::ExternalLayoutStyle& style) noexcept {
    const float horizontal = horizontal_margin(style.margin);
    const float vertical = vertical_margin(style.margin);
    Constraints content{
        subtract_extent(outer.min_width, horizontal),
        subtract_extent(outer.max_width, horizontal),
        subtract_extent(outer.min_height, vertical),
        subtract_extent(outer.max_height, vertical),
    };
    apply_axis_style(
        content.min_width,
        content.max_width,
        style.width,
        style.min_width,
        style.max_width);
    apply_axis_style(
        content.min_height,
        content.max_height,
        style.height,
        style.min_height,
        style.max_height);
    return content;
}

runtime::Size outer_size(
    runtime::Size content,
    const runtime::EdgeInsets& margin) noexcept {
    return {
        content.width + horizontal_margin(margin),
        content.height + vertical_margin(margin),
    };
}

runtime::Rect inset_margin(
    runtime::Rect outer,
    const runtime::EdgeInsets& margin,
    runtime::Size measured) noexcept {
    return {
        outer.x + margin.left,
        outer.y + margin.top,
        std::min(measured.width, subtract_extent(outer.width, horizontal_margin(margin))),
        std::min(measured.height, subtract_extent(outer.height, vertical_margin(margin))),
    };
}

void validate_padding(const Padding& padding) {
    if (padding.left < 0.0F || padding.top < 0.0F
            || padding.right < 0.0F || padding.bottom < 0.0F
            || std::isnan(padding.left) || std::isnan(padding.top)
            || std::isnan(padding.right) || std::isnan(padding.bottom)) {
        throw std::invalid_argument("Layout padding must be non-negative");
    }
}

float filled_size(bool fill, float maximum, float natural) noexcept {
    return fill && std::isfinite(maximum) ? maximum : natural;
}

Constraints content_constraints(Constraints outer, const Padding& padding) {
    const float horizontal = horizontal_padding(padding);
    const float vertical = vertical_padding(padding);
    return {
        0.0F,
        std::max(0.0F, outer.max_width - horizontal),
        0.0F,
        std::max(0.0F, outer.max_height - vertical),
    };
}

runtime::Rect content_bounds(runtime::Rect outer, const Padding& padding) noexcept {
    return {
        outer.x + padding.left,
        outer.y + padding.top,
        std::max(0.0F, outer.width - horizontal_padding(padding)),
        std::max(0.0F, outer.height - vertical_padding(padding)),
    };
}

} // namespace

Constraints Constraints::fixed(float width, float height) {
    Constraints constraints{width, width, height, height};
    constraints.validate();
    return constraints;
}

void Constraints::validate() const {
    if (std::isnan(min_width) || std::isnan(max_width)
            || std::isnan(min_height) || std::isnan(max_height)
            || min_width < 0.0F || min_height < 0.0F
            || min_width > max_width || min_height > max_height) {
        throw std::invalid_argument("Constraints require 0 <= min <= max");
    }
}

runtime::Size Constraints::constrain(runtime::Size size) const {
    validate();
    return {
        std::clamp(size.width, min_width, max_width),
        std::clamp(size.height, min_height, max_height),
    };
}

LayoutEngine::LayoutEngine(runtime::NodeStore& nodes) noexcept : nodes_(&nodes) {}

void LayoutEngine::set_layout(runtime::NodeId id, LayoutModel layout) {
    auto& node = nodes_->require(id);
    std::visit([](const auto& model) {
        using Model = std::decay_t<decltype(model)>;
        if constexpr (std::is_same_v<Model, LeafLayout>) {
            if (model.preferred_size.width < 0.0F || model.preferred_size.height < 0.0F
                    || std::isnan(model.preferred_size.width)
                    || std::isnan(model.preferred_size.height)) {
                throw std::invalid_argument("Leaf size must be non-negative");
            }
        } else {
            validate_padding(model.padding);
            if constexpr (std::is_same_v<Model, FlexLayout>) {
                if (model.gap < 0.0F || std::isnan(model.gap)) {
                    throw std::invalid_argument("Flex gap must be non-negative");
                }
            }
        }
    }, layout);

    if (const auto* leaf = std::get_if<LeafLayout>(&layout)) {
        node.requested_size = leaf->preferred_size;
    }

    if (layouts_.size() <= id.index) {
        layouts_.resize(static_cast<std::size_t>(id.index) + 1);
    }
    layouts_[id.index] = LayoutSlot{id.generation, std::move(layout)};
}

void LayoutEngine::set_intrinsic_measure(
    runtime::NodeId id,
    std::uint64_t revision,
    IntrinsicMeasure measure) {
    static_cast<void>(nodes_->require(id));
    if (!measure) {
        throw std::invalid_argument("Intrinsic measure callback cannot be empty");
    }
    if (intrinsics_.size() <= id.index) {
        intrinsics_.resize(static_cast<std::size_t>(id.index) + 1);
    }
    intrinsics_[id.index] = IntrinsicSlot{
        id.generation,
        revision,
        std::move(measure),
        std::nullopt,
        false,
    };
}

bool LayoutEngine::set_intrinsic_revision(
    runtime::NodeId id,
    std::uint64_t revision) {
    auto* slot = find_intrinsic(id);
    if (slot == nullptr) {
        return false;
    }
    if (slot->revision == revision) {
        return false;
    }
    slot->revision = revision;
    slot->cache.reset();
    return true;
}

bool LayoutEngine::remove_intrinsic_measure(runtime::NodeId id) noexcept {
    auto* slot = find_intrinsic(id);
    if (slot == nullptr) {
        return false;
    }
    *slot = {};
    return true;
}

runtime::Size LayoutEngine::measure(runtime::NodeId root, Constraints constraints) {
    constraints.validate();
    ++generation_;
    if (generation_ == 0) {
        ++generation_;
    }
    return measure_node(root, constraints);
}

void LayoutEngine::place(runtime::NodeId root, runtime::Point origin) {
    auto& node = nodes_->require(root);
    if (node.measure_generation != generation_ || generation_ == 0) {
        throw std::logic_error("Node must be measured in the current layout generation");
    }
    place_node(root, {origin.x, origin.y, node.layout_size.width, node.layout_size.height});
}

runtime::Size LayoutEngine::layout(
    runtime::NodeId root,
    Constraints constraints,
    runtime::Point origin) {
    const auto size = measure(root, constraints);
    place(root, origin);
    return size;
}

std::uint64_t LayoutEngine::generation() const noexcept {
    return generation_;
}

const LayoutModel& LayoutEngine::require_layout(runtime::NodeId id) const {
    static_cast<void>(nodes_->require(id));
    if (id.index >= layouts_.size()) {
        throw std::logic_error("Node has no layout model");
    }
    const auto& slot = layouts_[id.index];
    if (slot.generation != id.generation) {
        throw std::logic_error("Node has no layout model for its current generation");
    }
    return slot.model;
}

LayoutEngine::IntrinsicSlot* LayoutEngine::find_intrinsic(
    runtime::NodeId id) noexcept {
    if (nodes_->find(id) == nullptr || id.index >= intrinsics_.size()) {
        return nullptr;
    }
    auto& slot = intrinsics_[id.index];
    if (slot.generation != id.generation || !slot.measure) {
        return nullptr;
    }
    return &slot;
}

runtime::Size LayoutEngine::measure_node(runtime::NodeId id, Constraints constraints) {
    constraints.validate();
    auto& node = nodes_->require(id);
    const auto content_constraint = external_content_constraints(
        constraints,
        node.external_layout);
    content_constraint.validate();
    const auto& model = require_layout(id);
    runtime::Size measured{};

    std::visit([&](const auto& current) {
        using Model = std::decay_t<decltype(current)>;
        if constexpr (std::is_same_v<Model, LeafLayout>) {
            if (auto* intrinsic = find_intrinsic(id)) {
                if (intrinsic->measuring) {
                    throw std::logic_error("Intrinsic measurement cannot recurse");
                }
                if (intrinsic->cache.has_value()
                        && intrinsic->cache->revision == intrinsic->revision
                        && intrinsic->cache->constraints == content_constraint) {
                    measured = intrinsic->cache->result;
                } else {
                    intrinsic->measuring = true;
                    try {
                        measured = intrinsic->measure(content_constraint);
                    } catch (...) {
                        intrinsic->measuring = false;
                        throw;
                    }
                    intrinsic->measuring = false;
                    if (measured.width < 0.0F || measured.height < 0.0F
                            || !std::isfinite(measured.width)
                            || !std::isfinite(measured.height)) {
                        throw std::invalid_argument(
                            "Intrinsic measure must return a finite non-negative size");
                    }
                    measured = content_constraint.constrain(measured);
                    intrinsic->cache = IntrinsicCache{
                        intrinsic->revision,
                        content_constraint,
                        measured,
                    };
                }
            } else {
                measured = content_constraint.constrain(node.requested_size);
            }
        } else if constexpr (std::is_same_v<Model, BoxLayout>) {
            const auto child_constraints = content_constraints(
                content_constraint,
                current.padding);
            runtime::Size content{};
            for (const auto child : node.children) {
                const auto child_size = measure_node(child, child_constraints);
                content.width = std::max(content.width, child_size.width);
                content.height = std::max(content.height, child_size.height);
            }
            runtime::Size natural{
                content.width + horizontal_padding(current.padding),
                content.height + vertical_padding(current.padding),
            };
            natural.width = filled_size(
                current.fill_width,
                content_constraint.max_width,
                natural.width);
            natural.height = filled_size(
                current.fill_height,
                content_constraint.max_height,
                natural.height);
            measured = content_constraint.constrain(natural);
        } else {
            const auto child_constraints = content_constraints(
                content_constraint,
                current.padding);
            float main_size = 0.0F;
            float cross_size = 0.0F;
            bool first = true;
            for (const auto child : node.children) {
                const auto child_size = measure_node(child, child_constraints);
                if (!first) {
                    main_size += current.gap;
                }
                first = false;
                if (current.direction == FlexDirection::horizontal) {
                    main_size += child_size.width;
                    cross_size = std::max(cross_size, child_size.height);
                } else {
                    main_size += child_size.height;
                    cross_size = std::max(cross_size, child_size.width);
                }
            }
            runtime::Size natural;
            if (current.direction == FlexDirection::horizontal) {
                natural = {
                    main_size + horizontal_padding(current.padding),
                    cross_size + vertical_padding(current.padding),
                };
            } else {
                natural = {
                    cross_size + horizontal_padding(current.padding),
                    main_size + vertical_padding(current.padding),
                };
            }
            natural.width = filled_size(
                current.fill_width,
                content_constraint.max_width,
                natural.width);
            natural.height = filled_size(
                current.fill_height,
                content_constraint.max_height,
                natural.height);
            measured = content_constraint.constrain(natural);
        }
    }, model);

    node.measured_size = measured;
    node.layout_size = outer_size(measured, node.external_layout.margin);
    ++node.measure_count;
    node.measure_generation = generation_;
    return node.layout_size;
}

void LayoutEngine::place_node(runtime::NodeId id, runtime::Rect bounds) {
    auto& node = nodes_->require(id);
    if (node.measure_generation != generation_) {
        throw std::logic_error("Layout child was not measured in the current generation");
    }
    node.bounds = inset_margin(
        bounds,
        node.external_layout.margin,
        node.measured_size);
    ++node.place_count;
    node.place_generation = generation_;

    std::visit([&](const auto& current) {
        using Model = std::decay_t<decltype(current)>;
        if constexpr (std::is_same_v<Model, LeafLayout>) {
            return;
        } else if constexpr (std::is_same_v<Model, BoxLayout>) {
            const auto content = content_bounds(node.bounds, current.padding);
            for (const auto child : node.children) {
                const auto child_size = nodes_->require(child).layout_size;
                place_node(child, {
                    content.x,
                    content.y,
                    std::min(child_size.width, content.width),
                    std::min(child_size.height, content.height),
                });
            }
        } else {
            const auto content = content_bounds(node.bounds, current.padding);
            float cursor = current.direction == FlexDirection::horizontal
                ? content.x
                : content.y;
            const float end = current.direction == FlexDirection::horizontal
                ? content.x + content.width
                : content.y + content.height;

            for (std::size_t index = 0; index < node.children.size(); ++index) {
                const auto child = node.children[index];
                const auto child_size = nodes_->require(child).layout_size;
                const float remaining = std::max(0.0F, end - cursor);
                if (current.direction == FlexDirection::horizontal) {
                    const float width = std::min(child_size.width, remaining);
                    place_node(child, {
                        cursor,
                        content.y,
                        width,
                        std::min(child_size.height, content.height),
                    });
                    cursor += width;
                } else {
                    const float height = std::min(child_size.height, remaining);
                    place_node(child, {
                        content.x,
                        cursor,
                        std::min(child_size.width, content.width),
                        height,
                    });
                    cursor += height;
                }
                if (index + 1 < node.children.size()) {
                    cursor = std::min(end, cursor + current.gap);
                }
            }
        }
    }, require_layout(id));
}

} // namespace ryn::layout
