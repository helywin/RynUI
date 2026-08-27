#include "runtime/layout_style_adapter.hpp"

#include <cmath>
#include <stdexcept>
#include <utility>

namespace ryn::runtime {
namespace {

enum class LayoutStyleField {
    width,
    height,
    min_width,
    max_width,
    min_height,
    max_height,
    margin_left,
    margin_top,
    margin_right,
    margin_bottom,
};

bool dimensions_equal(
    const ExternalLayoutStyle& left,
    const ExternalLayoutStyle& right) noexcept {
    return left.width == right.width
        && left.height == right.height
        && left.min_width == right.min_width
        && left.max_width == right.max_width
        && left.min_height == right.min_height
        && left.max_height == right.max_height;
}

void validate_optional_length(
    const std::optional<float>& value,
    const char* name) {
    if (value.has_value() && (!std::isfinite(*value) || *value < 0.0F)) {
        throw std::invalid_argument(name);
    }
}

void validate_margin(float value) {
    if (!std::isfinite(value) || value < 0.0F) {
        throw std::invalid_argument(
            "LayoutStyle margin must be finite and non-negative");
    }
}

void validate_style(const ExternalLayoutStyle& style) {
    validate_optional_length(
        style.width,
        "LayoutStyle width must be finite and non-negative");
    validate_optional_length(
        style.height,
        "LayoutStyle height must be finite and non-negative");
    validate_optional_length(
        style.min_width,
        "LayoutStyle min width must be finite and non-negative");
    validate_optional_length(
        style.max_width,
        "LayoutStyle max width must be finite and non-negative");
    validate_optional_length(
        style.min_height,
        "LayoutStyle min height must be finite and non-negative");
    validate_optional_length(
        style.max_height,
        "LayoutStyle max height must be finite and non-negative");
    validate_margin(style.margin.left);
    validate_margin(style.margin.top);
    validate_margin(style.margin.right);
    validate_margin(style.margin.bottom);
    if (style.min_width.has_value() && style.max_width.has_value()
            && *style.min_width > *style.max_width) {
        throw std::invalid_argument("LayoutStyle min width cannot exceed max width");
    }
    if (style.min_height.has_value() && style.max_height.has_value()
            && *style.min_height > *style.max_height) {
        throw std::invalid_argument("LayoutStyle min height cannot exceed max height");
    }
}

std::optional<float> optional_length(LogicalLength value) {
    if (value.is_auto()) {
        return std::nullopt;
    }
    return value.value();
}

float margin_length(LogicalLength value) {
    if (value.is_auto()) {
        throw std::invalid_argument("LayoutStyle margin cannot be auto");
    }
    return value.value();
}

LogicalLength read_prop(const Prop<LogicalLength>& prop) {
    if (const auto* value = detail::PropAccess::static_value(prop)) {
        return *value;
    }
    return detail::PropAccess::binding(prop)->get();
}

void set_field(
    ExternalLayoutStyle& style,
    LayoutStyleField field,
    LogicalLength value) {
    switch (field) {
    case LayoutStyleField::width:
        style.width = optional_length(value);
        break;
    case LayoutStyleField::height:
        style.height = optional_length(value);
        break;
    case LayoutStyleField::min_width:
        style.min_width = optional_length(value);
        break;
    case LayoutStyleField::max_width:
        style.max_width = optional_length(value);
        break;
    case LayoutStyleField::min_height:
        style.min_height = optional_length(value);
        break;
    case LayoutStyleField::max_height:
        style.max_height = optional_length(value);
        break;
    case LayoutStyleField::margin_left:
        style.margin.left = margin_length(value);
        break;
    case LayoutStyleField::margin_top:
        style.margin.top = margin_length(value);
        break;
    case LayoutStyleField::margin_right:
        style.margin.right = margin_length(value);
        break;
    case LayoutStyleField::margin_bottom:
        style.margin.bottom = margin_length(value);
        break;
    }
}

class LayoutStyleWriter final {
public:
    LayoutStyleWriter(NodeStore& nodes, DirtyQueues& dirty) noexcept
        : nodes_(&nodes), dirty_(&dirty) {}

    bool set(NodeId id, ExternalLayoutStyle value) {
        validate_style(value);
        auto& node = nodes_->require(id);
        if (node.external_layout == value) {
            return false;
        }

        const bool dimension_change = !dimensions_equal(node.external_layout, value);
        const bool margin_change = node.external_layout.margin != value.margin;
        node.external_layout = std::move(value);
        if (margin_change) {
            node.layout_size = {
                node.measured_size.width
                    + node.external_layout.margin.left
                    + node.external_layout.margin.right,
                node.measured_size.height
                    + node.external_layout.margin.top
                    + node.external_layout.margin.bottom,
            };
        }
        if (dimension_change) {
            dirty_->invalidate(
                id,
                DirtyFlags::Measure | DirtyFlags::Layout | DirtyFlags::Geometry);
        } else if (margin_change) {
            dirty_->invalidate(id, DirtyFlags::Placement | DirtyFlags::Geometry);
        }
        return true;
    }

    bool set(NodeId id, LayoutStyleField field, LogicalLength value) {
        auto candidate = nodes_->require(id).external_layout;
        set_field(candidate, field, value);
        return set(id, std::move(candidate));
    }

private:
    NodeStore* nodes_;
    DirtyQueues* dirty_;
};

using PropMember = const std::optional<Prop<LogicalLength>>& (*)(
    const LayoutStyle&) noexcept;

struct FieldBinding final {
    LayoutStyleField field;
    PropMember member;
};

constexpr FieldBinding fields[] = {
    {LayoutStyleField::width, &detail::LayoutStyleAccess::width},
    {LayoutStyleField::height, &detail::LayoutStyleAccess::height},
    {LayoutStyleField::min_width, &detail::LayoutStyleAccess::min_width},
    {LayoutStyleField::max_width, &detail::LayoutStyleAccess::max_width},
    {LayoutStyleField::min_height, &detail::LayoutStyleAccess::min_height},
    {LayoutStyleField::max_height, &detail::LayoutStyleAccess::max_height},
    {LayoutStyleField::margin_left, &detail::LayoutStyleAccess::margin_left},
    {LayoutStyleField::margin_top, &detail::LayoutStyleAccess::margin_top},
    {LayoutStyleField::margin_right, &detail::LayoutStyleAccess::margin_right},
    {LayoutStyleField::margin_bottom, &detail::LayoutStyleAccess::margin_bottom},
};

} // namespace

void connect_layout_style(
    Scope& scope,
    const LayoutStyle& style,
    NodeId node,
    NodeStore& nodes,
    DirtyQueues& dirty) {
    if (!scope.active()) {
        return;
    }

    ExternalLayoutStyle initial;
    for (const auto& field : fields) {
        const auto& prop = field.member(style);
        if (prop.has_value()) {
            set_field(initial, field.field, read_prop(*prop));
        }
    }

    LayoutStyleWriter writer(nodes, dirty);
    static_cast<void>(writer.set(node, initial));

    for (const auto& field : fields) {
        const auto& prop = field.member(style);
        if (!prop.has_value()) {
            continue;
        }
        static_cast<void>(detail::connect_prop(
            scope,
            *prop,
            [writer, node, current_field = field.field](LogicalLength value) mutable {
                static_cast<void>(writer.set(node, current_field, value));
            }));
    }
}

} // namespace ryn::runtime
