#pragma once

#include <ryn/breakpoints.hpp>
#include <ryn/component.hpp>
#include <ryn/flex.hpp>
#include <ryn/layout_style.hpp>
#include <ryn/prop.hpp>

#include <optional>
#include <utility>

namespace ryn {
namespace detail {
struct GridPropsAccess;
}

enum class RowAlign { Top, Middle, Bottom, Stretch };
enum class RowJustify { Start, End, Center, SpaceAround, SpaceBetween, SpaceEvenly };

class GridFlex final {
public:
    GridFlex() = default;

    explicit GridFlex(float factor) : GridFlex(factor, factor, auto_length) {}

    GridFlex(float grow, float shrink, LogicalLength basis) : grow_(grow), shrink_(shrink), basis_(basis) {
        if (!std::isfinite(grow) || grow < 0 || !std::isfinite(shrink) || shrink < 0 ||
            (!basis.is_auto() && (!std::isfinite(basis.value()) || basis.value() < 0))) {
            throw std::invalid_argument("Grid flex factors and basis must be finite and non-negative");
        }
    }

    [[nodiscard]] static GridFlex fixed(LogicalLength basis) {
        return {0, 0, basis};
    }

    [[nodiscard]] static GridFlex fill(float factor = 1) {
        return {factor, 1, dp(0)};
    }

    [[nodiscard]] static GridFlex automatic() {
        return {1, 1, auto_length};
    }

    [[nodiscard]] static GridFlex none() {
        return {0, 0, auto_length};
    }

    [[nodiscard]] float grow() const noexcept {
        return grow_;
    }

    [[nodiscard]] float shrink() const noexcept {
        return shrink_;
    }

    [[nodiscard]] LogicalLength basis() const noexcept {
        return basis_;
    }

    friend bool operator==(const GridFlex&, const GridFlex&) = default;

private:
    float grow_{};
    float shrink_{1};
    LogicalLength basis_{auto_length};
};

struct GridGutter final {
    Responsive<LogicalLength> horizontal{dp(0)};
    Responsive<LogicalLength> vertical{dp(0)};
    friend bool operator==(const GridGutter&, const GridGutter&) = default;
};

struct ColOptions final {
    std::optional<int> span;
    std::optional<int> offset;
    std::optional<int> order;
    std::optional<int> push;
    std::optional<int> pull;
    std::optional<GridFlex> flex;
    friend bool operator==(const ColOptions&, const ColOptions&) = default;
};

class RowProps final {
public:
    RowProps& align(Prop<Responsive<RowAlign>> value) {
        align_ = std::move(value);
        return *this;
    }

    RowProps& align(RowAlign value) {
        return align(Responsive<RowAlign>{value});
    }

    RowProps& justify(Prop<Responsive<RowJustify>> value) {
        justify_ = std::move(value);
        return *this;
    }

    RowProps& justify(RowJustify value) {
        return justify(Responsive<RowJustify>{value});
    }

    RowProps& gutter(Prop<GridGutter> value) {
        gutter_ = std::move(value);
        return *this;
    }

    RowProps& gutter(LogicalLength horizontal, LogicalLength vertical = dp(0)) {
        return gutter(GridGutter{Responsive<LogicalLength>{horizontal}, Responsive<LogicalLength>{vertical}});
    }

    RowProps& wrap(Prop<bool> value) {
        wrap_ = std::move(value);
        return *this;
    }

    RowProps& direction(Prop<FlexDirection> value) {
        direction_ = std::move(value);
        return *this;
    }

    RowProps& breakpoints(Prop<Breakpoints> value) {
        breakpoints_ = std::move(value);
        return *this;
    }

    RowProps& layout(LayoutStyle value) {
        layout_ = std::move(value);
        return *this;
    }

private:
    friend struct detail::GridPropsAccess;
    Prop<Responsive<RowAlign>> align_{Responsive<RowAlign>{RowAlign::Top}};
    Prop<Responsive<RowJustify>> justify_{Responsive<RowJustify>{RowJustify::Start}};
    Prop<GridGutter> gutter_{GridGutter{}};
    Prop<bool> wrap_{true};
    Prop<FlexDirection> direction_{FlexDirection::LeftToRight};
    Prop<Breakpoints> breakpoints_{Breakpoints{}};
    LayoutStyle layout_;
};

class ColProps final {
public:
    ColProps& span(Prop<int> value) {
        span_ = std::move(value);
        return *this;
    }

    ColProps& offset(Prop<int> value) {
        offset_ = std::move(value);
        return *this;
    }

    ColProps& order(Prop<int> value) {
        order_ = std::move(value);
        return *this;
    }

    ColProps& push(Prop<int> value) {
        push_ = std::move(value);
        return *this;
    }

    ColProps& pull(Prop<int> value) {
        pull_ = std::move(value);
        return *this;
    }

    ColProps& flex(Prop<GridFlex> value) {
        flex_ = std::move(value);
        return *this;
    }

    ColProps& responsive(Prop<Responsive<ColOptions>> value) {
        responsive_ = std::move(value);
        return *this;
    }

    ColProps& layout(LayoutStyle value) {
        layout_ = std::move(value);
        return *this;
    }

private:
    friend struct detail::GridPropsAccess;
    std::optional<Prop<int>> span_;
    Prop<int> offset_{0};
    Prop<int> order_{0};
    Prop<int> push_{0};
    Prop<int> pull_{0};
    std::optional<Prop<GridFlex>> flex_;
    Prop<Responsive<ColOptions>> responsive_{Responsive<ColOptions>{}};
    LayoutStyle layout_;
};

struct RowContentSlot final {};

using RowContent = SlotContent<RowContentSlot>;

struct ColContentSlot final {};

using ColContent = SlotContent<ColContentSlot>;

void Row(RowProps props, RowContent content);
void Col(ColProps props, ColContent content);

} // namespace ryn
