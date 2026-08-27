#include "layout/layout_engine.hpp"

#include <algorithm>
#include <cmath>
#include <limits>
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

float non_negative_finite(float value, const char* message) {
    if (!std::isfinite(value) || value < 0.0F) {
        throw std::invalid_argument(message);
    }
    return value;
}

std::size_t horizontal_item_count(
    const runtime::Node& node,
    const HorizontalContentLayout& layout) noexcept {
    return node.children.size() + (layout.loading ? 1U : 0U);
}

float horizontal_gap_extent(
    const runtime::Node& node,
    const HorizontalContentLayout& layout) noexcept {
    const auto count = horizontal_item_count(node, layout);
    return count > 1 ? static_cast<float>(count - 1) * layout.gap : 0.0F;
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

Constraints external_content_constraints(Constraints outer,
                                         const runtime::ExternalLayoutStyle& style,
                                         std::optional<float> forced_outer_width,
                                         std::optional<float> forced_outer_height) noexcept {
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
    if (forced_outer_width.has_value()) {
        const float fixed = subtract_extent(*forced_outer_width, horizontal);
        content.min_width = fixed;
        content.max_width = fixed;
    }
    if (forced_outer_height.has_value()) {
        const float fixed = subtract_extent(*forced_outer_height, vertical);
        content.min_height = fixed;
        content.max_height = fixed;
    }
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

float stretched_extent(
    float available,
    const std::optional<float>& minimum,
    const std::optional<float>& maximum) noexcept {
    float result = available;
    if (maximum.has_value()) {
        result = std::min(result, *maximum);
    }
    if (minimum.has_value()) {
        result = std::min(available, std::max(result, *minimum));
    }
    return result;
}

runtime::Rect inset_margin(
    runtime::Rect outer,
    const runtime::ExternalLayoutStyle& style,
    runtime::Size measured,
    bool stretch_width,
    bool stretch_height) noexcept {
    const auto& margin = style.margin;
    const float available_width = subtract_extent(
        outer.width,
        horizontal_margin(margin));
    const float available_height = subtract_extent(
        outer.height,
        vertical_margin(margin));
    return {
        outer.x + margin.left,
        outer.y + margin.top,
        stretch_width
            ? stretched_extent(available_width, style.min_width, style.max_width)
            : std::min(measured.width, available_width),
        stretch_height
            ? stretched_extent(available_height, style.min_height, style.max_height)
            : std::min(measured.height, available_height),
    };
}

void validate_padding(const Padding& padding) {
    if (padding.left < 0.0F || padding.top < 0.0F
            || padding.right < 0.0F || padding.bottom < 0.0F
            || !std::isfinite(padding.left) || !std::isfinite(padding.top)
            || !std::isfinite(padding.right) || !std::isfinite(padding.bottom)) {
        throw std::invalid_argument(
            "Layout padding must be finite and non-negative");
    }
}

void validate_flex_layout(const FlexLayout& layout) {
    validate_padding(layout.padding);
    static_cast<void>(non_negative_finite(
        layout.main_gap,
        "Flex main gap must be finite and non-negative"));
    static_cast<void>(non_negative_finite(
        layout.cross_gap,
        "Flex cross gap must be finite and non-negative"));

    switch (layout.direction) {
    case FlexDirection::horizontal:
    case FlexDirection::vertical:
        break;
    default:
        throw std::invalid_argument("Flex direction is invalid");
    }
    switch (layout.wrap) {
    case FlexWrap::no_wrap:
    case FlexWrap::wrap:
        break;
    default:
        throw std::invalid_argument("Flex wrap is invalid");
    }
    switch (layout.justify) {
    case FlexJustify::start:
    case FlexJustify::center:
    case FlexJustify::end:
    case FlexJustify::space_between:
    case FlexJustify::space_around:
    case FlexJustify::space_evenly:
        break;
    default:
        throw std::invalid_argument("Flex justify is invalid");
    }
    switch (layout.align) {
    case FlexAlign::start:
    case FlexAlign::center:
    case FlexAlign::end:
    case FlexAlign::stretch:
        break;
    default:
        throw std::invalid_argument("Flex align is invalid");
    }
}

float main_extent(runtime::Size size, FlexDirection direction) noexcept {
    return direction == FlexDirection::horizontal ? size.width : size.height;
}

float cross_extent(runtime::Size size, FlexDirection direction) noexcept {
    return direction == FlexDirection::horizontal ? size.height : size.width;
}

float main_extent(runtime::Rect bounds, FlexDirection direction) noexcept {
    return direction == FlexDirection::horizontal ? bounds.width : bounds.height;
}

float cross_extent(runtime::Rect bounds, FlexDirection direction) noexcept {
    return direction == FlexDirection::horizontal ? bounds.height : bounds.width;
}

float main_origin(runtime::Rect bounds, FlexDirection direction) noexcept {
    return direction == FlexDirection::horizontal ? bounds.x : bounds.y;
}

float cross_origin(runtime::Rect bounds, FlexDirection direction) noexcept {
    return direction == FlexDirection::horizontal ? bounds.y : bounds.x;
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
        } else if constexpr (
            std::is_same_v<Model, BoxLayout>
            || std::is_same_v<Model, FlexLayout>) {
            if constexpr (std::is_same_v<Model, FlexLayout>) {
                validate_flex_layout(model);
            } else {
                validate_padding(model.padding);
            }
        }
        if constexpr (std::is_same_v<Model, HorizontalContentLayout>) {
            static_cast<void>(non_negative_finite(
                model.control_height,
                "Horizontal content control height must be finite and non-negative"));
            static_cast<void>(non_negative_finite(
                model.padding_inline,
                "Horizontal content padding must be finite and non-negative"));
            static_cast<void>(non_negative_finite(
                model.border_width,
                "Horizontal content border must be finite and non-negative"));
            static_cast<void>(non_negative_finite(
                model.gap,
                "Horizontal content gap must be finite and non-negative"));
            static_cast<void>(non_negative_finite(
                model.loading_indicator_size,
                "Horizontal loading indicator must be finite and non-negative"));
        }
    }, layout);

    if (const auto* leaf = std::get_if<LeafLayout>(&layout)) {
        node.requested_size = leaf->preferred_size;
    }

    if (layouts_.size() <= id.index) {
        layouts_.resize(static_cast<std::size_t>(id.index) + 1);
    }
    layouts_[id.index] = LayoutSlot{
        id.generation,
        std::move(layout),
        std::nullopt,
        {},
    };
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

const HorizontalContentGeometry& LayoutEngine::horizontal_content_geometry(
    runtime::NodeId id) const {
    static_cast<void>(nodes_->require(id));
    if (id.index >= layouts_.size()) {
        throw std::logic_error("Node has no horizontal content layout");
    }
    const auto& slot = layouts_[id.index];
    if (slot.generation != id.generation
            || !std::holds_alternative<HorizontalContentLayout>(slot.model)
            || !slot.horizontal_content_geometry.has_value()) {
        throw std::logic_error(
            "Horizontal content geometry requires current placement");
    }
    return *slot.horizontal_content_geometry;
}

FlexLayoutDiagnostics LayoutEngine::flex_layout_diagnostics(
    runtime::NodeId id) const {
    static_cast<void>(nodes_->require(id));
    if (id.index >= layouts_.size()) {
        throw std::logic_error("Node has no Flex layout");
    }
    const auto& slot = layouts_[id.index];
    if (slot.generation != id.generation
            || !std::holds_alternative<FlexLayout>(slot.model)
            || slot.flex_scratch.measure_generation != generation_) {
        throw std::logic_error(
            "Flex diagnostics require current measurement");
    }
    return {
        slot.flex_scratch.items.size(),
        slot.flex_scratch.lines.size(),
        slot.flex_scratch.items.capacity(),
        slot.flex_scratch.lines.capacity(),
    };
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

runtime::Size LayoutEngine::measure_node(runtime::NodeId id, Constraints constraints,
                                         std::optional<float> forced_outer_width,
                                         std::optional<float> forced_outer_height) {
    constraints.validate();
    auto& node = nodes_->require(id);
    const auto content_constraint = external_content_constraints(
        constraints, node.external_layout, forced_outer_width, forced_outer_height);
    content_constraint.validate();
    const auto& model = require_layout(id);
    if (std::holds_alternative<HorizontalContentLayout>(model)) {
        layouts_[id.index].horizontal_content_geometry.reset();
    }
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
        } else if constexpr (std::is_same_v<Model, FlexLayout>) {
            const auto child_constraints = content_constraints(
                content_constraint,
                current.padding);
            auto& scratch = layouts_[id.index].flex_scratch;
            scratch.measure_generation = 0;
            scratch.items.clear();
            scratch.lines.clear();

            const float available_main = current.direction
                    == FlexDirection::horizontal
                ? child_constraints.max_width
                : child_constraints.max_height;

            for (std::size_t ordinal = 0; ordinal < node.children.size(); ++ordinal) {
                const auto child = node.children[ordinal];
                const auto& style = nodes_->require(child).external_layout;
                const float margin_main = current.direction == FlexDirection::horizontal
                                              ? horizontal_margin(style.margin)
                                              : vertical_margin(style.margin);
                const auto style_minimum = current.direction == FlexDirection::horizontal
                                               ? style.min_width
                                               : style.min_height;
                const auto style_maximum = current.direction == FlexDirection::horizontal
                                               ? style.max_width
                                               : style.max_height;
                const float minimum_main = style_minimum.value_or(0.0F) + margin_main;
                const float maximum_main = style_maximum.has_value()
                                               ? *style_maximum + margin_main
                                               : std::numeric_limits<float>::infinity();
                std::optional<float> basis_main;
                if (style.flex_basis.has_value()) {
                    basis_main =
                        std::clamp(*style.flex_basis + margin_main, minimum_main, maximum_main);
                }
                const auto child_size =
                    current.direction == FlexDirection::horizontal
                        ? measure_node(child, child_constraints, basis_main, std::nullopt)
                        : measure_node(child, child_constraints, std::nullopt, basis_main);
                const float child_main = main_extent(child_size, current.direction);
                const float child_cross = cross_extent(child_size, current.direction);
                scratch.items.push_back({
                    child,
                    ordinal,
                    child_main,
                    child_cross,
                    child_main,
                    minimum_main,
                    maximum_main,
                    style.flex_grow,
                    style.flex_shrink,
                    style.align_self,
                    false,
                });
            }

            std::sort(scratch.items.begin(), scratch.items.end(),
                      [&](const FlexItem& left, const FlexItem& right) {
                          const auto left_order = nodes_->require(left.id).external_layout.order;
                          const auto right_order = nodes_->require(right.id).external_layout.order;
                          return left_order < right_order ||
                                 (left_order == right_order &&
                                  left.declaration_ordinal < right.declaration_ordinal);
                      });

            FlexLine line;
            auto finish_line = [&] {
                if (line.item_count == 0) {
                    return;
                }
                scratch.lines.push_back(line);
                line = FlexLine{scratch.items.size(), 0, 0.0F, 0.0F};
            };

            for (std::size_t index = 0; index < scratch.items.size(); ++index) {
                auto& item = scratch.items[index];
                if (line.item_count == 0) {
                    line.first_item = index;
                }
                const float candidate_main = line.main_size +
                                             (line.item_count == 0 ? 0.0F : current.main_gap) +
                                             item.main_size;
                if (current.wrap == FlexWrap::wrap
                        && std::isfinite(available_main)
                        && line.item_count > 0
                        && candidate_main > available_main) {
                    finish_line();
                    line.first_item = index;
                }
                if (line.item_count > 0) {
                    line.main_size += current.main_gap;
                }
                line.main_size += item.main_size;
                line.cross_size = std::max(line.cross_size, item.cross_size);
                ++line.item_count;
            }
            finish_line();

            auto natural_size = [&] {
                float natural_main = 0.0F;
                float natural_cross = 0.0F;
                for (std::size_t index = 0; index < scratch.lines.size(); ++index) {
                    const auto& measured_line = scratch.lines[index];
                    natural_main = std::max(natural_main, measured_line.main_size);
                    if (index > 0) {
                        natural_cross += current.cross_gap;
                    }
                    natural_cross += measured_line.cross_size;
                }

                runtime::Size natural;
                if (current.direction == FlexDirection::horizontal) {
                    natural = {
                        natural_main + horizontal_padding(current.padding),
                        natural_cross + vertical_padding(current.padding),
                    };
                } else {
                    natural = {
                        natural_cross + horizontal_padding(current.padding),
                        natural_main + vertical_padding(current.padding),
                    };
                }
                natural.width =
                    filled_size(current.fill_width, content_constraint.max_width, natural.width);
                natural.height =
                    filled_size(current.fill_height, content_constraint.max_height, natural.height);
                return content_constraint.constrain(natural);
            };

            measured = natural_size();
            const float final_available_main =
                current.direction == FlexDirection::horizontal
                    ? subtract_extent(measured.width, horizontal_padding(current.padding))
                    : subtract_extent(measured.height, vertical_padding(current.padding));
            constexpr float distribution_epsilon = 0.0001F;
            for (auto& measured_line : scratch.lines) {
                const float gap_extent =
                    measured_line.item_count > 1
                        ? static_cast<float>(measured_line.item_count - 1) * current.main_gap
                        : 0.0F;
                const float free_space = final_available_main - measured_line.main_size;
                const bool growing = free_space > distribution_epsilon;
                float remaining = std::abs(free_space);
                for (std::size_t index = 0; index < measured_line.item_count; ++index) {
                    scratch.items[measured_line.first_item + index].frozen = false;
                }

                while (remaining > distribution_epsilon) {
                    float total_weight = 0.0F;
                    std::size_t last_adjustable = scratch.items.size();
                    for (std::size_t index = 0; index < measured_line.item_count; ++index) {
                        auto& item = scratch.items[measured_line.first_item + index];
                        const float capacity = growing ? item.max_main_size - item.main_size
                                                       : item.main_size - item.min_main_size;
                        const float weight =
                            growing ? item.grow : item.shrink * item.base_main_size;
                        if (!item.frozen && capacity > distribution_epsilon && weight > 0.0F) {
                            total_weight += weight;
                            last_adjustable = measured_line.first_item + index;
                        } else {
                            item.frozen = true;
                        }
                    }
                    if (last_adjustable == scratch.items.size() || total_weight <= 0.0F) {
                        break;
                    }

                    float distributed = 0.0F;
                    bool clamped = false;
                    for (std::size_t index = 0; index < measured_line.item_count; ++index) {
                        auto& item = scratch.items[measured_line.first_item + index];
                        if (item.frozen) {
                            continue;
                        }
                        const float weight =
                            growing ? item.grow : item.shrink * item.base_main_size;
                        const float requested = remaining * weight / total_weight;
                        const float capacity = growing ? item.max_main_size - item.main_size
                                                       : item.main_size - item.min_main_size;
                        const float delta = std::min(requested, capacity);
                        item.main_size += growing ? delta : -delta;
                        distributed += delta;
                        if (delta + distribution_epsilon < requested) {
                            item.frozen = true;
                            clamped = true;
                        }
                    }
                    if (!clamped) {
                        const float residual = remaining - distributed;
                        if (residual > 0.0F) {
                            auto& item = scratch.items[last_adjustable];
                            const float capacity = growing ? item.max_main_size - item.main_size
                                                           : item.main_size - item.min_main_size;
                            const float delta = std::min(residual, capacity);
                            item.main_size += growing ? delta : -delta;
                            distributed += delta;
                        }
                    }
                    if (distributed <= distribution_epsilon) {
                        break;
                    }
                    remaining = std::max(0.0F, remaining - distributed);
                }

                measured_line.main_size = gap_extent;
                measured_line.cross_size = 0.0F;
                for (std::size_t index = 0; index < measured_line.item_count; ++index) {
                    auto& item = scratch.items[measured_line.first_item + index];
                    if (std::abs(item.main_size - item.base_main_size) > distribution_epsilon) {
                        const auto final_size = current.direction == FlexDirection::horizontal
                                                    ? measure_node(item.id, child_constraints,
                                                                   item.main_size, std::nullopt)
                                                    : measure_node(item.id, child_constraints,
                                                                   std::nullopt, item.main_size);
                        item.main_size = main_extent(final_size, current.direction);
                        item.cross_size = cross_extent(final_size, current.direction);
                    }
                    measured_line.main_size += item.main_size;
                    measured_line.cross_size = std::max(measured_line.cross_size, item.cross_size);
                }
            }
            measured = natural_size();
            scratch.measure_generation = generation_;
        } else {
            const float frame_inline = 2.0F
                * (current.padding_inline + current.border_width);
            const float indicator_inline = current.loading
                ? current.loading_indicator_size
                : 0.0F;
            const float gaps = horizontal_gap_extent(node, current);
            float remaining_width = subtract_extent(
                content_constraint.max_width,
                frame_inline + indicator_inline + gaps);
            const float child_max_height = std::max(
                0.0F,
                current.control_height - 2.0F * current.border_width);
            float children_width = 0.0F;
            float children_height = 0.0F;
            for (const auto child : node.children) {
                const auto child_size = measure_node(child, {
                    0.0F,
                    remaining_width,
                    0.0F,
                    child_max_height,
                });
                children_width += child_size.width;
                children_height = std::max(children_height, child_size.height);
                remaining_width = subtract_extent(
                    remaining_width,
                    child_size.width);
            }
            const runtime::Size natural{
                frame_inline + indicator_inline + gaps + children_width,
                std::max(
                    current.control_height,
                    children_height + 2.0F * current.border_width),
            };
            measured = content_constraint.constrain(natural);
        }
    }, model);

    node.measured_size = measured;
    node.layout_size = outer_size(measured, node.external_layout.margin);
    ++node.measure_count;
    node.measure_generation = generation_;
    return node.layout_size;
}

void LayoutEngine::place_node(
    runtime::NodeId id,
    runtime::Rect bounds,
    bool stretch_width,
    bool stretch_height) {
    auto& node = nodes_->require(id);
    if (node.measure_generation != generation_) {
        throw std::logic_error("Layout child was not measured in the current generation");
    }
    node.bounds = inset_margin(
        bounds,
        node.external_layout,
        node.measured_size,
        stretch_width,
        stretch_height);
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
        } else if constexpr (std::is_same_v<Model, FlexLayout>) {
            const auto content = content_bounds(node.bounds, current.padding);
            const auto& scratch = layouts_[id.index].flex_scratch;
            if (scratch.measure_generation != generation_) {
                throw std::logic_error(
                    "Flex placement requires current line measurement");
            }

            const float available_main = main_extent(content, current.direction);
            const float available_cross = cross_extent(content, current.direction);
            const float main_start = main_origin(content, current.direction);
            const float cross_start = cross_origin(content, current.direction);
            const float cross_end = cross_start + available_cross;
            float line_cross_cursor = cross_start;

            for (std::size_t line_index = 0;
                    line_index < scratch.lines.size();
                    ++line_index) {
                const auto& line = scratch.lines[line_index];
                const float remaining_cross = std::max(
                    0.0F,
                    cross_end - line_cross_cursor);
                const float line_cross = scratch.lines.size() == 1
                    ? remaining_cross
                    : std::min(line.cross_size, remaining_cross);
                const float free_main = std::max(
                    0.0F,
                    available_main - line.main_size);
                float leading_main = 0.0F;
                float between_items = current.main_gap;

                switch (current.justify) {
                case FlexJustify::start:
                    break;
                case FlexJustify::center:
                    leading_main = free_main * 0.5F;
                    break;
                case FlexJustify::end:
                    leading_main = free_main;
                    break;
                case FlexJustify::space_between:
                    if (line.item_count > 1) {
                        between_items += free_main
                            / static_cast<float>(line.item_count - 1);
                    }
                    break;
                case FlexJustify::space_around:
                    if (line.item_count > 0) {
                        const float distributed = free_main
                            / static_cast<float>(line.item_count);
                        leading_main = distributed * 0.5F;
                        between_items += distributed;
                    }
                    break;
                case FlexJustify::space_evenly:
                    if (line.item_count > 0) {
                        const float distributed = free_main
                            / static_cast<float>(line.item_count + 1);
                        leading_main = distributed;
                        between_items += distributed;
                    }
                    break;
                }

                float item_cursor = main_start + leading_main;
                for (std::size_t line_item = 0;
                        line_item < line.item_count;
                        ++line_item) {
                    const auto& item = scratch.items[
                        line.first_item + line_item];
                    const auto& child_style = nodes_->require(item.id).external_layout;
                    FlexAlign item_align = current.align;
                    switch (item.align_self) {
                    case runtime::FlexItemAlign::automatic:
                        break;
                    case runtime::FlexItemAlign::start:
                        item_align = FlexAlign::start;
                        break;
                    case runtime::FlexItemAlign::center:
                        item_align = FlexAlign::center;
                        break;
                    case runtime::FlexItemAlign::end:
                        item_align = FlexAlign::end;
                        break;
                    case runtime::FlexItemAlign::stretch:
                        item_align = FlexAlign::stretch;
                        break;
                    }
                    const bool explicit_cross = current.direction == FlexDirection::horizontal
                                                    ? child_style.height.has_value()
                                                    : child_style.width.has_value();
                    const bool stretch = item_align == FlexAlign::stretch && !explicit_cross;
                    const float child_main = item.main_size;
                    const float child_cross = stretch
                        ? line_cross
                        : std::min(item.cross_size, line_cross);
                    const float free_cross = std::max(
                        0.0F,
                        line_cross - child_cross);
                    float leading_cross = 0.0F;
                    if (item_align == FlexAlign::center) {
                        leading_cross = free_cross * 0.5F;
                    } else if (item_align == FlexAlign::end) {
                        leading_cross = free_cross;
                    }

                    if (current.direction == FlexDirection::horizontal) {
                        place_node(
                            item.id,
                            {
                                item_cursor,
                                line_cross_cursor + leading_cross,
                                child_main,
                                child_cross,
                            },
                            false,
                            stretch);
                    } else {
                        place_node(
                            item.id,
                            {
                                line_cross_cursor + leading_cross,
                                item_cursor,
                                child_cross,
                                child_main,
                            },
                            stretch,
                            false);
                    }
                    item_cursor += child_main;
                    if (line_item + 1 < line.item_count) {
                        item_cursor += between_items;
                    }
                }

                line_cross_cursor += line_cross;
                if (line_index + 1 < scratch.lines.size()) {
                    line_cross_cursor = std::min(
                        cross_end,
                        line_cross_cursor + current.cross_gap);
                }
            }
        } else {
            const runtime::Rect content{
                node.bounds.x + current.padding_inline + current.border_width,
                node.bounds.y + current.border_width,
                std::max(
                    0.0F,
                    node.bounds.width
                        - 2.0F * (current.padding_inline + current.border_width)),
                std::max(
                    0.0F,
                    node.bounds.height - 2.0F * current.border_width),
            };
            float occupied = current.loading
                ? current.loading_indicator_size
                : 0.0F;
            occupied += horizontal_gap_extent(node, current);
            for (const auto child : node.children) {
                occupied += nodes_->require(child).layout_size.width;
            }
            float cursor = content.x
                + std::max(0.0F, (content.width - occupied) * 0.5F);

            HorizontalContentGeometry geometry{content, std::nullopt};
            if (current.loading) {
                const float size = std::min(
                    current.loading_indicator_size,
                    std::min(content.width, content.height));
                geometry.loading_indicator_bounds = runtime::Rect{
                    cursor,
                    content.y + std::max(0.0F, (content.height - size) * 0.5F),
                    size,
                    size,
                };
                cursor += size;
                if (!node.children.empty()) {
                    cursor = std::min(content.x + content.width, cursor + current.gap);
                }
            }

            const float end = content.x + content.width;
            for (std::size_t index = 0; index < node.children.size(); ++index) {
                const auto child = node.children[index];
                const auto child_size = nodes_->require(child).layout_size;
                const float width = std::min(
                    child_size.width,
                    std::max(0.0F, end - cursor));
                const float height = std::min(child_size.height, content.height);
                place_node(child, {
                    cursor,
                    content.y + std::max(0.0F, (content.height - height) * 0.5F),
                    width,
                    height,
                });
                cursor += width;
                if (index + 1 < node.children.size()) {
                    cursor = std::min(end, cursor + current.gap);
                }
            }
            layouts_[id.index].horizontal_content_geometry = geometry;
        }
    }, require_layout(id));
}

} // namespace ryn::layout
