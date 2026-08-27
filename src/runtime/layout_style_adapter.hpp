#pragma once

#include "runtime/invalidation.hpp"
#include "runtime/prop_connection.hpp"

#include <ryn/layout_style.hpp>

#include <optional>

namespace ryn::detail {

struct LayoutStyleAccess final {
    [[nodiscard]] static const std::optional<Prop<LogicalLength>>& width(
        const LayoutStyle& style) noexcept {
        return style.width_;
    }

    [[nodiscard]] static const std::optional<Prop<LogicalLength>>& height(
        const LayoutStyle& style) noexcept {
        return style.height_;
    }

    [[nodiscard]] static const std::optional<Prop<LogicalLength>>& min_width(
        const LayoutStyle& style) noexcept {
        return style.min_width_;
    }

    [[nodiscard]] static const std::optional<Prop<LogicalLength>>& max_width(
        const LayoutStyle& style) noexcept {
        return style.max_width_;
    }

    [[nodiscard]] static const std::optional<Prop<LogicalLength>>& min_height(
        const LayoutStyle& style) noexcept {
        return style.min_height_;
    }

    [[nodiscard]] static const std::optional<Prop<LogicalLength>>& max_height(
        const LayoutStyle& style) noexcept {
        return style.max_height_;
    }

    [[nodiscard]] static const std::optional<Prop<LogicalLength>>& margin_left(
        const LayoutStyle& style) noexcept {
        return style.margin_left_;
    }

    [[nodiscard]] static const std::optional<Prop<LogicalLength>>& margin_top(
        const LayoutStyle& style) noexcept {
        return style.margin_top_;
    }

    [[nodiscard]] static const std::optional<Prop<LogicalLength>>& margin_right(
        const LayoutStyle& style) noexcept {
        return style.margin_right_;
    }

    [[nodiscard]] static const std::optional<Prop<LogicalLength>>& margin_bottom(
        const LayoutStyle& style) noexcept {
        return style.margin_bottom_;
    }

    [[nodiscard]] static const std::optional<Prop<float>>&
    flex_grow(const LayoutStyle& style) noexcept {
        return style.flex_grow_;
    }

    [[nodiscard]] static const std::optional<Prop<float>>&
    flex_shrink(const LayoutStyle& style) noexcept {
        return style.flex_shrink_;
    }

    [[nodiscard]] static const std::optional<Prop<LogicalLength>>&
    flex_basis(const LayoutStyle& style) noexcept {
        return style.flex_basis_;
    }

    [[nodiscard]] static const std::optional<Prop<FlexAlignSelf>>&
    align_self(const LayoutStyle& style) noexcept {
        return style.align_self_;
    }

    [[nodiscard]] static const std::optional<Prop<int>>& order(const LayoutStyle& style) noexcept {
        return style.order_;
    }
};

} // namespace ryn::detail

namespace ryn::runtime {

void connect_layout_style(
    Scope& scope,
    const LayoutStyle& style,
    NodeId node,
    NodeStore& nodes,
    DirtyQueues& dirty);

} // namespace ryn::runtime
