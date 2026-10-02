#include "layout/layout_engine.hpp"
#include "layout/flex_distribution.hpp"

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

std::size_t horizontal_item_count(const runtime::Node& node, const HorizontalContentLayout& layout) noexcept {
    return node.children.size() - (layout.skip_first && !node.children.empty() ? 1U : 0U) + (layout.loading ? 1U : 0U);
}

float horizontal_gap_extent(const runtime::Node& node, const HorizontalContentLayout& layout) noexcept {
    const auto count = horizontal_item_count(node, layout);
    return count > 1 ? static_cast<float>(count - 1) * layout.gap : 0.0F;
}

void apply_axis_style(float& minimum, float& maximum, const std::optional<float>& fixed,
                      const std::optional<float>& style_minimum, const std::optional<float>& style_maximum) noexcept {
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

Constraints external_content_constraints(Constraints outer, const runtime::ExternalLayoutStyle& style,
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
    apply_axis_style(content.min_width, content.max_width, style.width, style.min_width, style.max_width);
    apply_axis_style(content.min_height, content.max_height, style.height, style.min_height, style.max_height);
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

runtime::Size outer_size(runtime::Size content, const runtime::EdgeInsets& margin) noexcept {
    return {
        content.width + horizontal_margin(margin),
        content.height + vertical_margin(margin),
    };
}

float stretched_extent(float available, const std::optional<float>& minimum,
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

runtime::Rect inset_margin(runtime::Rect outer, const runtime::ExternalLayoutStyle& style, runtime::Size measured,
                           bool stretch_width, bool stretch_height) noexcept {
    const auto& margin = style.margin;
    const float available_width = subtract_extent(outer.width, horizontal_margin(margin));
    const float available_height = subtract_extent(outer.height, vertical_margin(margin));
    return {
        outer.x + margin.left,
        outer.y + margin.top,
        stretch_width ? stretched_extent(available_width, style.min_width, style.max_width)
                      : std::min(measured.width, available_width),
        stretch_height ? stretched_extent(available_height, style.min_height, style.max_height)
                       : std::min(measured.height, available_height),
    };
}

void validate_padding(const Padding& padding) {
    if (padding.left < 0.0F || padding.top < 0.0F || padding.right < 0.0F || padding.bottom < 0.0F ||
        !std::isfinite(padding.left) || !std::isfinite(padding.top) || !std::isfinite(padding.right) ||
        !std::isfinite(padding.bottom)) {
        throw std::invalid_argument("Layout padding must be finite and non-negative");
    }
}

FlexAlign effective_align(FlexAlign parent, runtime::FlexItemAlign item) noexcept {
    switch (item) {
    case runtime::FlexItemAlign::automatic:
        return parent;
    case runtime::FlexItemAlign::start:
        return FlexAlign::start;
    case runtime::FlexItemAlign::center:
        return FlexAlign::center;
    case runtime::FlexItemAlign::end:
        return FlexAlign::end;
    case runtime::FlexItemAlign::stretch:
        return FlexAlign::stretch;
    case runtime::FlexItemAlign::baseline:
        return FlexAlign::baseline;
    }
    return parent;
}

float outer_baseline(const runtime::Node& node) noexcept {
    return node.external_layout.margin.top + node.first_baseline.value_or(node.measured_size.height);
}

void validate_flex_layout(const FlexLayout& layout) {
    validate_padding(layout.padding);
    static_cast<void>(non_negative_finite(layout.main_gap, "Flex main gap must be finite and non-negative"));
    static_cast<void>(non_negative_finite(layout.cross_gap, "Flex cross gap must be finite and non-negative"));

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
    case FlexWrap::wrap_reverse:
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
    case FlexJustify::left:
    case FlexJustify::right:
        break;
    default:
        throw std::invalid_argument("Flex justify is invalid");
    }
    switch (layout.align) {
    case FlexAlign::start:
    case FlexAlign::center:
    case FlexAlign::end:
    case FlexAlign::stretch:
    case FlexAlign::baseline:
        break;
    default:
        throw std::invalid_argument("Flex align is invalid");
    }
    switch (layout.item_policy) {
    case FlexItemPolicy::flex:
    case FlexItemPolicy::sequential:
        break;
    default:
        throw std::invalid_argument("Flex item policy is invalid");
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
    if (std::isnan(min_width) || std::isnan(max_width) || std::isnan(min_height) || std::isnan(max_height) ||
        min_width < 0.0F || min_height < 0.0F || min_width > max_width || min_height > max_height) {
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
    std::visit(
        [](const auto& model) {
            using Model = std::decay_t<decltype(model)>;
            if constexpr (std::is_same_v<Model, ComponentLayout>) {
                if (!model.measure || !model.place) {
                    throw std::invalid_argument("component layout requires both phases");
                }
            } else if constexpr (std::is_same_v<Model, LeafLayout>) {
                if (model.preferred_size.width < 0.0F || model.preferred_size.height < 0.0F ||
                    std::isnan(model.preferred_size.width) || std::isnan(model.preferred_size.height)) {
                    throw std::invalid_argument("Leaf size must be non-negative");
                }
            } else if constexpr (std::is_same_v<Model, BoxLayout> || std::is_same_v<Model, FlexLayout>) {
                if constexpr (std::is_same_v<Model, FlexLayout>) {
                    validate_flex_layout(model);
                } else {
                    validate_padding(model.padding);
                }
            }
            if constexpr (std::is_same_v<Model, HorizontalContentLayout> || std::is_same_v<Model, InputContentLayout>) {
                static_cast<void>(non_negative_finite(
                    model.control_height, "Horizontal content control height must be finite and non-negative"));
                static_cast<void>(non_negative_finite(model.padding_inline,
                                                      "Horizontal content padding must be finite and non-negative"));
                static_cast<void>(non_negative_finite(model.border_width,
                                                      "Horizontal content border must be finite and non-negative"));
                static_cast<void>(
                    non_negative_finite(model.gap, "Horizontal content gap must be finite and non-negative"));
                if constexpr (std::is_same_v<Model, InputContentLayout>) {
                    static_cast<void>(non_negative_finite(model.padding_block,
                                                          "Input block padding must be finite and non-negative"));
                }
                if constexpr (std::is_same_v<Model, HorizontalContentLayout>) {
                    static_cast<void>(non_negative_finite(
                        model.loading_indicator_size, "Horizontal loading indicator must be finite and non-negative"));
                    static_cast<void>(non_negative_finite(model.minimum_width,
                                                          "Horizontal minimum width must be finite and non-negative"));
                }
            }
        },
        layout);

    if (const auto* leaf = std::get_if<LeafLayout>(&layout)) {
        node.requested_size = leaf->preferred_size;
    }

    if (layouts_.size() <= id.index) {
        layouts_.resize(static_cast<std::size_t>(id.index) + 1);
    }
    auto& slot = layouts_[id.index];
    const bool preserve_flex_scratch = slot.generation == id.generation &&
                                       std::holds_alternative<FlexLayout>(slot.model) &&
                                       std::holds_alternative<FlexLayout>(layout);
    auto scratch = preserve_flex_scratch ? std::move(slot.flex_scratch) : FlexScratch{};
    slot = LayoutSlot{
        id.generation,
        std::move(layout),
        std::nullopt,
        std::move(scratch),
    };
}

bool LayoutEngine::remove_layout(runtime::NodeId id) noexcept {
    if (!id.valid() || id.index >= layouts_.size()) {
        return false;
    }
    auto& slot = layouts_[id.index];
    if (slot.generation != id.generation) {
        return false;
    }
    slot = {};
    return true;
}

void LayoutEngine::set_intrinsic_measure(runtime::NodeId id, std::uint64_t revision, IntrinsicMeasure measure) {
    static_cast<void>(nodes_->require(id));
    if (!measure) {
        throw std::invalid_argument("Intrinsic measure callback cannot be empty");
    }
    if (intrinsics_.size() <= id.index) {
        intrinsics_.resize(static_cast<std::size_t>(id.index) + 1);
    }
    intrinsics_[id.index] = IntrinsicSlot{
        id.generation, revision, std::move(measure), std::nullopt, false,
    };
}

bool LayoutEngine::set_intrinsic_revision(runtime::NodeId id, std::uint64_t revision) {
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

runtime::Size LayoutEngine::layout(runtime::NodeId root, Constraints constraints, runtime::Point origin) {
    const auto size = measure(root, constraints);
    place(root, origin);
    return size;
}

std::uint64_t LayoutEngine::generation() const noexcept {
    return generation_;
}

const HorizontalContentGeometry& LayoutEngine::horizontal_content_geometry(runtime::NodeId id) const {
    static_cast<void>(nodes_->require(id));
    if (id.index >= layouts_.size()) {
        throw std::logic_error("Node has no horizontal content layout");
    }
    const auto& slot = layouts_[id.index];
    if (slot.generation != id.generation || !std::holds_alternative<HorizontalContentLayout>(slot.model) ||
        !slot.horizontal_content_geometry.has_value()) {
        throw std::logic_error("Horizontal content geometry requires current placement");
    }
    return *slot.horizontal_content_geometry;
}

FlexLayoutDiagnostics LayoutEngine::flex_layout_diagnostics(runtime::NodeId id) const {
    static_cast<void>(nodes_->require(id));
    if (id.index >= layouts_.size()) {
        throw std::logic_error("Node has no Flex layout");
    }
    const auto& slot = layouts_[id.index];
    if (slot.generation != id.generation || !std::holds_alternative<FlexLayout>(slot.model) ||
        slot.flex_scratch.measure_generation != generation_) {
        throw std::logic_error("Flex diagnostics require current measurement");
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

LayoutEngine::IntrinsicSlot* LayoutEngine::find_intrinsic(runtime::NodeId id) noexcept {
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
    const auto content_constraint =
        external_content_constraints(constraints, node.external_layout, forced_outer_width, forced_outer_height);
    content_constraint.validate();
    const auto& model = require_layout(id);
    if (std::holds_alternative<HorizontalContentLayout>(model)) {
        layouts_[id.index].horizontal_content_geometry.reset();
    }
    runtime::Size measured{};
    node.first_baseline.reset();

    std::visit(
        [&](const auto& current) {
            using Model = std::decay_t<decltype(current)>;
            if constexpr (std::is_same_v<Model, LeafLayout>) {
                if (auto* intrinsic = find_intrinsic(id)) {
                    if (intrinsic->measuring) {
                        throw std::logic_error("Intrinsic measurement cannot recurse");
                    }
                    if (intrinsic->cache.has_value() && intrinsic->cache->revision == intrinsic->revision &&
                        intrinsic->cache->constraints == content_constraint) {
                        measured = intrinsic->cache->result.size;
                        node.first_baseline = intrinsic->cache->result.first_baseline;
                    } else {
                        intrinsic->measuring = true;
                        IntrinsicMeasurement result;
                        try {
                            result = intrinsic->measure(content_constraint);
                        } catch (...) {
                            intrinsic->measuring = false;
                            throw;
                        }
                        intrinsic->measuring = false;
                        measured = result.size;
                        if (measured.width < 0.0F || measured.height < 0.0F || !std::isfinite(measured.width) ||
                            !std::isfinite(measured.height)) {
                            throw std::invalid_argument("Intrinsic measure must return a finite non-negative size");
                        }
                        if (result.first_baseline.has_value()) {
                            static_cast<void>(non_negative_finite(
                                *result.first_baseline, "Intrinsic baseline must be finite and non-negative"));
                        }
                        measured = content_constraint.constrain(measured);
                        node.first_baseline = result.first_baseline;
                        intrinsic->cache = IntrinsicCache{
                            intrinsic->revision,
                            content_constraint,
                            {measured, node.first_baseline},
                        };
                    }
                } else {
                    measured = content_constraint.constrain(node.requested_size);
                }
            } else if constexpr (std::is_same_v<Model, BoxLayout>) {
                const auto child_constraints = content_constraints(content_constraint, current.padding);
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
                natural.width = filled_size(current.fill_width, content_constraint.max_width, natural.width);
                natural.height = filled_size(current.fill_height, content_constraint.max_height, natural.height);
                measured = content_constraint.constrain(natural);
                for (const auto child : node.children) {
                    const auto& child_node = nodes_->require(child);
                    if (child_node.first_baseline.has_value()) {
                        node.first_baseline = current.padding.top + outer_baseline(child_node);
                        break;
                    }
                }
            } else if constexpr (std::is_same_v<Model, ComponentLayout>) {
                measured = current.measure(*this, id, content_constraint);
                if (!std::isfinite(measured.width) || !std::isfinite(measured.height) || measured.width < 0 ||
                    measured.height < 0) {
                    throw std::invalid_argument("component layout returned an invalid size");
                }
                measured = content_constraint.constrain(measured);
            } else if constexpr (std::is_same_v<Model, FlexLayout>) {
                const auto child_constraints = content_constraints(content_constraint, current.padding);
                auto& scratch = layouts_[id.index].flex_scratch;
                scratch.measure_generation = 0;
                scratch.items.clear();
                scratch.lines.clear();

                const float available_main = current.direction == FlexDirection::horizontal
                                                 ? child_constraints.max_width
                                                 : child_constraints.max_height;

                for (std::size_t ordinal = 0; ordinal < node.children.size(); ++ordinal) {
                    const auto child = node.children[ordinal];
                    const auto& style = nodes_->require(child).external_layout;
                    const float margin_main = current.direction == FlexDirection::horizontal
                                                  ? horizontal_margin(style.margin)
                                                  : vertical_margin(style.margin);
                    const auto style_minimum =
                        current.direction == FlexDirection::horizontal ? style.min_width : style.min_height;
                    const auto style_maximum =
                        current.direction == FlexDirection::horizontal ? style.max_width : style.max_height;
                    const float minimum_main = style_minimum.value_or(0.0F) + margin_main;
                    const float maximum_main = style_maximum.has_value() ? *style_maximum + margin_main
                                                                         : std::numeric_limits<float>::infinity();
                    std::optional<float> basis_main;
                    if (current.item_policy == FlexItemPolicy::flex && style.flex_basis.has_value()) {
                        basis_main = std::clamp(*style.flex_basis + margin_main, minimum_main, maximum_main);
                    }
                    const auto child_size = current.direction == FlexDirection::horizontal
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
                        current.item_policy == FlexItemPolicy::flex ? style.flex_grow : 0.0F,
                        current.item_policy == FlexItemPolicy::flex ? style.flex_shrink : 0.0F,
                        current.item_policy == FlexItemPolicy::flex ? style.align_self
                                                                    : runtime::FlexItemAlign::automatic,
                        false,
                    });
                }

                if (current.item_policy == FlexItemPolicy::flex) {
                    std::sort(
                        scratch.items.begin(), scratch.items.end(), [&](const FlexItem& left, const FlexItem& right) {
                            const auto left_order = nodes_->require(left.id).external_layout.order;
                            const auto right_order = nodes_->require(right.id).external_layout.order;
                            return left_order < right_order ||
                                   (left_order == right_order && left.declaration_ordinal < right.declaration_ordinal);
                        });
                }

                const auto update_line_cross = [&](FlexLine& value) {
                    value.cross_size = 0;
                    value.cross_baseline = 0;
                    float descent = 0;
                    for (std::size_t index = 0; index < value.item_count; ++index) {
                        const auto& item = scratch.items[value.first_item + index];
                        auto& child_node = nodes_->require(item.id);
                        child_node.baseline_participant =
                            current.direction == FlexDirection::horizontal &&
                            effective_align(current.align, item.align_self) == FlexAlign::baseline;
                        value.cross_size = std::max(value.cross_size, item.cross_size);
                        if (child_node.baseline_participant) {
                            const auto physical = outer_baseline(child_node);
                            const auto baseline =
                                current.wrap == FlexWrap::wrap_reverse ? item.cross_size - physical : physical;
                            value.cross_baseline = std::max(value.cross_baseline, baseline);
                            descent = std::max(descent, item.cross_size - baseline);
                        }
                    }
                    value.cross_size = std::max(value.cross_size, value.cross_baseline + descent);
                };
                FlexLine line;
                auto finish_line = [&] {
                    if (line.item_count == 0) {
                        return;
                    }
                    update_line_cross(line);
                    scratch.lines.push_back(line);
                    line = FlexLine{scratch.items.size(), 0, 0.0F, 0.0F};
                };

                for (std::size_t index = 0; index < scratch.items.size(); ++index) {
                    auto& item = scratch.items[index];
                    if (line.item_count == 0) {
                        line.first_item = index;
                    }
                    const float candidate_main =
                        line.main_size + (line.item_count == 0 ? 0.0F : current.main_gap) + item.main_size;
                    if (current.wrap != FlexWrap::no_wrap && std::isfinite(available_main) && line.item_count > 0 &&
                        candidate_main > available_main) {
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
                    natural.width = filled_size(current.fill_width, content_constraint.max_width, natural.width);
                    natural.height = filled_size(current.fill_height, content_constraint.max_height, natural.height);
                    return content_constraint.constrain(natural);
                };

                measured = natural_size();
                const float final_available_main =
                    current.direction == FlexDirection::horizontal
                        ? subtract_extent(measured.width, horizontal_padding(current.padding))
                        : subtract_extent(measured.height, vertical_padding(current.padding));
                constexpr float distribution_epsilon = 0.0001F;
                for (auto& measured_line : scratch.lines) {
                    const float gap_extent = measured_line.item_count > 1
                                                 ? static_cast<float>(measured_line.item_count - 1) * current.main_gap
                                                 : 0.0F;
                    const float free_space = final_available_main - measured_line.main_size;
                    distribute_flex_space(
                        std::span{scratch.items}.subspan(measured_line.first_item, measured_line.item_count),
                        free_space);

                    measured_line.main_size = gap_extent;
                    measured_line.cross_size = 0.0F;
                    for (std::size_t index = 0; index < measured_line.item_count; ++index) {
                        auto& item = scratch.items[measured_line.first_item + index];
                        if (std::abs(item.main_size - item.base_main_size) > distribution_epsilon) {
                            const auto final_size =
                                current.direction == FlexDirection::horizontal
                                    ? measure_node(item.id, child_constraints, item.main_size, std::nullopt)
                                    : measure_node(item.id, child_constraints, std::nullopt, item.main_size);
                            item.main_size = main_extent(final_size, current.direction);
                            item.cross_size = cross_extent(final_size, current.direction);
                        }
                        measured_line.main_size += item.main_size;
                        measured_line.cross_size = std::max(measured_line.cross_size, item.cross_size);
                    }
                    update_line_cross(measured_line);
                }
                measured = natural_size();
                // Stretch changes the available cross size of composite controls.
                // Measure that size before exporting their centered text baseline.
                const auto final_content = content_bounds({0, 0, measured.width, measured.height}, current.padding);
                const auto final_cross = cross_extent(final_content, current.direction);
                float stretch_cursor = 0;
                for (auto& measured_line : scratch.lines) {
                    const auto line_cross =
                        scratch.lines.size() == 1
                            ? final_cross
                            : std::min(measured_line.cross_size, std::max(0.0F, final_cross - stretch_cursor));
                    for (std::size_t index = 0; index < measured_line.item_count; ++index) {
                        auto& item = scratch.items[measured_line.first_item + index];
                        const auto& style = nodes_->require(item.id).external_layout;
                        const bool horizontal = current.direction == FlexDirection::horizontal;
                        const bool explicit_cross = horizontal ? style.height.has_value() : style.width.has_value();
                        if (effective_align(current.align, item.align_self) != FlexAlign::stretch || explicit_cross) {
                            continue;
                        }
                        const auto margin =
                            horizontal ? vertical_margin(style.margin) : horizontal_margin(style.margin);
                        const auto target = stretched_extent(subtract_extent(line_cross, margin),
                                                             horizontal ? style.min_height : style.min_width,
                                                             horizontal ? style.max_height : style.max_width) +
                                            margin;
                        if (std::abs(target - item.cross_size) > distribution_epsilon) {
                            const auto size = horizontal
                                                  ? measure_node(item.id, child_constraints, item.main_size, target)
                                                  : measure_node(item.id, child_constraints, target, item.main_size);
                            item.cross_size = cross_extent(size, current.direction);
                        }
                    }
                    stretch_cursor += line_cross + current.cross_gap;
                }
                // Propagate the first real text baseline through nested Flex containers.
                // Icon-only children keep their synthesized border-box baseline local.
                const auto content = content_bounds({0, 0, measured.width, measured.height}, current.padding);
                const float available_cross = cross_extent(content, current.direction);
                const float available_main_final = main_extent(content, current.direction);
                const bool reverse_cross = (current.wrap == FlexWrap::wrap_reverse) !=
                                           (current.direction == FlexDirection::vertical && current.right_to_left);
                float cross_cursor = 0;
                for (const auto& measured_line : scratch.lines) {
                    const auto line_cross =
                        scratch.lines.size() == 1
                            ? available_cross
                            : std::min(measured_line.cross_size, std::max(0.0F, available_cross - cross_cursor));
                    const auto physical_cross =
                        reverse_cross ? available_cross - cross_cursor - line_cross : cross_cursor;
                    const auto free_main = std::max(0.0F, available_main_final - measured_line.main_size);
                    float main_cursor = 0;
                    float gap = current.main_gap;
                    if (current.justify == FlexJustify::center) {
                        main_cursor = free_main * .5F;
                    } else if (current.justify == FlexJustify::end) {
                        main_cursor = free_main;
                    } else if (current.justify == FlexJustify::space_between && measured_line.item_count > 1) {
                        gap += free_main / static_cast<float>(measured_line.item_count - 1);
                    } else if (current.justify == FlexJustify::space_around && measured_line.item_count > 0) {
                        const float distributed = free_main / static_cast<float>(measured_line.item_count);
                        main_cursor = distributed * .5F;
                        gap += distributed;
                    } else if (current.justify == FlexJustify::space_evenly && measured_line.item_count > 0) {
                        const float distributed = free_main / static_cast<float>(measured_line.item_count + 1);
                        main_cursor = distributed;
                        gap += distributed;
                    }
                    for (std::size_t index = 0; index < measured_line.item_count; ++index) {
                        const auto& item = scratch.items[measured_line.first_item + index];
                        const auto& child_node = nodes_->require(item.id);
                        if (child_node.first_baseline.has_value() && !node.first_baseline.has_value()) {
                            float offset = main_cursor;
                            if (current.direction == FlexDirection::horizontal) {
                                const auto align = effective_align(current.align, item.align_self);
                                const auto child_cross = std::min(item.cross_size, line_cross);
                                const auto free_cross = std::max(0.0F, line_cross - child_cross);
                                float leading = 0;
                                if (align == FlexAlign::center) {
                                    leading = free_cross * .5F;
                                } else if (align == FlexAlign::end) {
                                    leading = free_cross;
                                } else if (align == FlexAlign::baseline) {
                                    leading = measured_line.cross_baseline -
                                              (reverse_cross ? child_cross - outer_baseline(child_node)
                                                             : outer_baseline(child_node));
                                }
                                offset =
                                    physical_cross + (reverse_cross ? line_cross - leading - child_cross : leading);
                            }
                            node.first_baseline = current.padding.top + offset + outer_baseline(child_node);
                        }
                        main_cursor += item.main_size + gap;
                    }
                    cross_cursor += line_cross + current.cross_gap;
                }
                scratch.measure_generation = generation_;
            } else if constexpr (std::is_same_v<Model, InputContentLayout>) {
                if (node.children.size() != 3) {
                    throw std::logic_error("Input layout requires three slots");
                }
                const float frame = 2.0F * (current.padding_inline + current.border_width);
                const float inner_width = subtract_extent(content_constraint.max_width, frame);
                const float height = std::min(current.control_height, content_constraint.max_height);
                const float inner_height =
                    subtract_extent(height, 2.0F * (current.border_width + current.padding_block));
                const float gaps =
                    current.gap * (static_cast<float>(current.prefix) + static_cast<float>(current.suffix));
                auto remaining = subtract_extent(inner_width, gaps);
                const auto prefix =
                    measure_node(node.children[0], {0, current.prefix ? remaining : 0, 0, inner_height});
                remaining = subtract_extent(remaining, prefix.width);
                const auto suffix =
                    measure_node(node.children[2], {0, current.suffix ? remaining : 0, 0, inner_height});
                remaining = subtract_extent(remaining, suffix.width);
                const auto editable = measure_node(
                    node.children[1], {std::isfinite(remaining) ? remaining : 0, remaining, 0, inner_height});
                measured = content_constraint.constrain(
                    {prefix.width + editable.width + suffix.width + gaps + frame, current.control_height});
                const auto& viewport = nodes_->require(node.children[1]);
                if (viewport.first_baseline.has_value()) {
                    const auto top = std::min(measured.height * .5F, current.border_width + current.padding_block);
                    const auto inner = std::max(0.0F, measured.height - 2 * top);
                    node.first_baseline =
                        top + (inner - std::min(inner, viewport.layout_size.height)) * .5F + outer_baseline(viewport);
                }
            } else {
                const float frame_inline = 2.0F * (current.padding_inline + current.border_width);
                const float indicator_inline = current.loading ? current.loading_indicator_size : 0.0F;
                const float gaps = horizontal_gap_extent(node, current);
                float remaining_width =
                    subtract_extent(content_constraint.max_width, frame_inline + indicator_inline + gaps);
                const float child_max_height = std::max(0.0F, current.control_height - 2.0F * current.border_width);
                float children_width = 0.0F;
                float children_height = 0.0F;
                for (std::size_t index = 0; index < node.children.size(); ++index) {
                    const auto child = node.children[index];
                    if (index == 0 && current.skip_first) {
                        static_cast<void>(measure_node(child, Constraints::fixed(0, 0)));
                        continue;
                    }
                    const auto child_size = measure_node(child, {
                                                                    0.0F,
                                                                    remaining_width,
                                                                    0.0F,
                                                                    child_max_height,
                                                                });
                    children_width += child_size.width;
                    children_height = std::max(children_height, child_size.height);
                    remaining_width = subtract_extent(remaining_width, child_size.width);
                }
                runtime::Size natural{
                    std::max(current.minimum_width, frame_inline + indicator_inline + gaps + children_width),
                    std::max(current.control_height, children_height + 2.0F * current.border_width),
                };
                natural.width = filled_size(current.fill_width, content_constraint.max_width, natural.width);
                measured = content_constraint.constrain(natural);
                const float content_height = std::max(0.0F, measured.height - 2 * current.border_width);
                for (std::size_t index = 0; index < node.children.size(); ++index) {
                    if (index == 0 && current.skip_first) {
                        continue;
                    }
                    const auto& child_node = nodes_->require(node.children[index]);
                    if (child_node.first_baseline.has_value()) {
                        node.first_baseline =
                            current.border_width +
                            (content_height - std::min(content_height, child_node.layout_size.height)) * .5F +
                            outer_baseline(child_node);
                        break;
                    }
                }
            }
        },
        model);

    node.measured_size = measured;
    node.layout_size = outer_size(measured, node.external_layout.margin);
    ++node.measure_count;
    node.measure_generation = generation_;
    return node.layout_size;
}

void LayoutEngine::place_node(runtime::NodeId id, runtime::Rect bounds, bool stretch_width, bool stretch_height) {
    auto& node = nodes_->require(id);
    if (node.measure_generation != generation_) {
        throw std::logic_error("Layout child was not measured in the current generation");
    }
    node.bounds = inset_margin(bounds, node.external_layout, node.measured_size, stretch_width, stretch_height);
    ++node.place_count;
    node.place_generation = generation_;

    std::visit(
        [&](const auto& current) {
            using Model = std::decay_t<decltype(current)>;
            if constexpr (std::is_same_v<Model, LeafLayout>) {
                return;
            } else if constexpr (std::is_same_v<Model, ComponentLayout>) {
                current.place(*this, id, node.bounds);
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
                    throw std::logic_error("Flex placement requires current line measurement");
                }

                const float available_main = main_extent(content, current.direction);
                const float available_cross = cross_extent(content, current.direction);
                const float main_start = main_origin(content, current.direction);
                const float cross_start = cross_origin(content, current.direction);
                const float cross_end = cross_start + available_cross;
                const bool reverse_main = current.direction == FlexDirection::horizontal && current.right_to_left;
                const bool reverse_cross = (current.wrap == FlexWrap::wrap_reverse) !=
                                           (current.direction == FlexDirection::vertical && current.right_to_left);
                float line_cross_cursor = cross_start;

                for (std::size_t line_index = 0; line_index < scratch.lines.size(); ++line_index) {
                    const auto& line = scratch.lines[line_index];
                    const float remaining_cross = std::max(0.0F, cross_end - line_cross_cursor);
                    const float line_cross =
                        scratch.lines.size() == 1 ? remaining_cross : std::min(line.cross_size, remaining_cross);
                    const float physical_cross_start =
                        reverse_cross ? cross_end - (line_cross_cursor - cross_start) - line_cross : line_cross_cursor;
                    const float free_main = std::max(0.0F, available_main - line.main_size);
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
                    case FlexJustify::left:
                        leading_main = reverse_main ? free_main : 0;
                        break;
                    case FlexJustify::right:
                        leading_main = current.direction == FlexDirection::horizontal && !reverse_main ? free_main : 0;
                        break;
                    case FlexJustify::space_between:
                        if (line.item_count > 1) {
                            between_items += free_main / static_cast<float>(line.item_count - 1);
                        }
                        break;
                    case FlexJustify::space_around:
                        if (line.item_count > 0) {
                            const float distributed = free_main / static_cast<float>(line.item_count);
                            leading_main = distributed * 0.5F;
                            between_items += distributed;
                        }
                        break;
                    case FlexJustify::space_evenly:
                        if (line.item_count > 0) {
                            const float distributed = free_main / static_cast<float>(line.item_count + 1);
                            leading_main = distributed;
                            between_items += distributed;
                        }
                        break;
                    }

                    float item_cursor = main_start + leading_main;
                    for (std::size_t line_item = 0; line_item < line.item_count; ++line_item) {
                        const auto& item = scratch.items[line.first_item + line_item];
                        const auto& child_style = nodes_->require(item.id).external_layout;
                        const auto item_align = effective_align(current.align, item.align_self);
                        const bool explicit_cross = current.direction == FlexDirection::horizontal
                                                        ? child_style.height.has_value()
                                                        : child_style.width.has_value();
                        const bool stretch = item_align == FlexAlign::stretch && !explicit_cross;
                        const float child_main = item.main_size;
                        const float child_cross = stretch ? line_cross : std::min(item.cross_size, line_cross);
                        const float free_cross = std::max(0.0F, line_cross - child_cross);
                        float leading_cross = 0.0F;
                        if (item_align == FlexAlign::center) {
                            leading_cross = free_cross * 0.5F;
                        } else if (item_align == FlexAlign::end) {
                            leading_cross = free_cross;
                        } else if (item_align == FlexAlign::baseline &&
                                   current.direction == FlexDirection::horizontal) {
                            const auto baseline = outer_baseline(nodes_->require(item.id));
                            leading_cross = line.cross_baseline - (reverse_cross ? child_cross - baseline : baseline);
                        }

                        const float item_main =
                            reverse_main ? main_start + available_main - (item_cursor - main_start) - child_main
                                         : item_cursor;
                        const float item_cross =
                            physical_cross_start +
                            (reverse_cross ? line_cross - leading_cross - child_cross : leading_cross);
                        if (current.direction == FlexDirection::horizontal) {
                            place_node(item.id,
                                       {
                                           item_main,
                                           item_cross,
                                           child_main,
                                           child_cross,
                                       },
                                       false, stretch);
                        } else {
                            place_node(item.id,
                                       {
                                           item_cross,
                                           item_main,
                                           child_cross,
                                           child_main,
                                       },
                                       stretch, false);
                        }
                        item_cursor += child_main;
                        if (line_item + 1 < line.item_count) {
                            item_cursor += between_items;
                        }
                    }

                    line_cross_cursor += line_cross;
                    if (line_index + 1 < scratch.lines.size()) {
                        line_cross_cursor = std::min(cross_end, line_cross_cursor + current.cross_gap);
                    }
                }
            } else if constexpr (std::is_same_v<Model, InputContentLayout>) {
                if (node.children.size() != 3) {
                    throw std::logic_error("Input layout requires three slots");
                }
                const auto inset = std::min(node.bounds.width * 0.5F, current.padding_inline + current.border_width);
                const auto top = std::min(node.bounds.height * 0.5F, current.border_width + current.padding_block);
                const auto height = std::max(0.0F, node.bounds.height - 2.0F * top);
                float cursor = node.bounds.x + inset;
                const auto end = node.bounds.x + node.bounds.width - inset;
                for (std::size_t index = 0; index < 3; ++index) {
                    const auto child = node.children[index];
                    const auto size = nodes_->require(child).layout_size;
                    const auto width = std::min(size.width, std::max(0.0F, end - cursor));
                    const auto child_height = std::min(height, size.height);
                    place_node(child,
                               {cursor, node.bounds.y + top + (height - child_height) * 0.5F, width, child_height},
                               index == 1, false);
                    cursor += width;
                    if ((index == 0 && current.prefix) || (index == 1 && current.suffix)) {
                        cursor = std::min(end, cursor + current.gap);
                    }
                }
            } else {
                const runtime::Rect content{
                    node.bounds.x + current.padding_inline + current.border_width,
                    node.bounds.y + current.border_width,
                    std::max(0.0F, node.bounds.width - 2.0F * (current.padding_inline + current.border_width)),
                    std::max(0.0F, node.bounds.height - 2.0F * current.border_width),
                };
                float occupied = current.loading ? current.loading_indicator_size : 0.0F;
                occupied += horizontal_gap_extent(node, current);
                for (std::size_t index = 0; index < node.children.size(); ++index) {
                    if (index != 0 || !current.skip_first) {
                        occupied += nodes_->require(node.children[index]).layout_size.width;
                    }
                }
                float cursor = content.x + std::max(0.0F, (content.width - occupied) * 0.5F);

                HorizontalContentGeometry geometry{content, std::nullopt};
                const float end = content.x + content.width;
                std::size_t remaining = horizontal_item_count(node, current);
                const auto advance = [&](float width) {
                    cursor += width;
                    if (--remaining > 0) {
                        cursor = std::min(end, cursor + current.gap);
                    }
                };
                const auto place_indicator = [&] {
                    if (!current.loading) {
                        return;
                    }
                    const float size =
                        std::min(current.loading_indicator_size, std::min(content.width, content.height));
                    geometry.loading_indicator_bounds = runtime::Rect{
                        cursor,
                        content.y + std::max(0.0F, (content.height - size) * 0.5F),
                        size,
                        size,
                    };
                    advance(size);
                };
                const auto place_content = [&](std::size_t index) {
                    const auto child = node.children[index];
                    if (index == 0 && current.skip_first) {
                        place_node(child, {cursor, content.y, 0, 0});
                        return;
                    }
                    const auto child_size = nodes_->require(child).layout_size;
                    const float width = std::min(child_size.width, std::max(0.0F, end - cursor));
                    const float height = std::min(child_size.height, content.height);
                    place_node(child, {
                                          cursor,
                                          content.y + std::max(0.0F, (content.height - height) * 0.5F),
                                          width,
                                          height,
                                      });
                    advance(width);
                };
                if (!current.end_icon) {
                    place_indicator();
                    for (std::size_t index = 0; index < node.children.size(); ++index) {
                        place_content(index);
                    }
                } else {
                    for (std::size_t index = current.first_is_icon ? 1 : 0; index < node.children.size(); ++index) {
                        place_content(index);
                    }
                    if (current.first_is_icon && !node.children.empty()) {
                        place_content(0);
                    }
                    place_indicator();
                }
                layouts_[id.index].horizontal_content_geometry = geometry;
            }
        },
        require_layout(id));
}

void LayoutEngine::place_retained_child(runtime::NodeId child, runtime::Rect bounds) {
    const auto measured_generation = nodes_->require(child).measure_generation;
    if (measured_generation == 0) {
        throw std::logic_error("Retained child must be measured before placement");
    }
    const auto previous = generation_;
    generation_ = measured_generation;
    try {
        place_node(child, bounds, true, true);
    } catch (...) {
        generation_ = previous;
        throw;
    }
    generation_ = previous;
}

} // namespace ryn::layout
