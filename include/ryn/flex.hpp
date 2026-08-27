#pragma once

#include <ryn/component.hpp>
#include <ryn/layout_style.hpp>
#include <ryn/prop.hpp>

#include <cmath>
#include <optional>
#include <stdexcept>
#include <utility>

namespace ryn {
namespace detail {

struct FlexPropsAccess;
struct LayoutGapAccess;

} // namespace detail

enum class SpaceSize {
    Small,
    Middle,
    Large,
};

enum class FlexJustify {
    Start,
    Center,
    End,
    SpaceBetween,
    SpaceAround,
    SpaceEvenly,
};

enum class FlexAlign {
    Start,
    Center,
    End,
    Stretch,
};

enum class SpaceAlign {
    Start,
    Center,
    End,
};

class LayoutGap final {
public:
    constexpr LayoutGap() noexcept = default;

    explicit constexpr LayoutGap(SpaceSize preset) : preset_(preset) {
        switch (preset) {
        case SpaceSize::Small:
        case SpaceSize::Middle:
        case SpaceSize::Large:
            return;
        }
        throw std::invalid_argument("LayoutGap preset is invalid");
    }

    explicit LayoutGap(LogicalLength value) : LayoutGap(value, value) {}

    LayoutGap(LogicalLength main, LogicalLength cross)
        : main_(validate(main)), cross_(validate(cross)) {}

    friend constexpr bool operator==(LayoutGap, LayoutGap) = default;

private:
    friend struct detail::LayoutGapAccess;

    [[nodiscard]] static float validate(LogicalLength value) {
        if (value.is_auto() || !std::isfinite(value.value()) || value.value() < 0.0F) {
            throw std::invalid_argument(
                "LayoutGap values must be finite, non-negative logical lengths");
        }
        return value.value();
    }

    std::optional<SpaceSize> preset_;
    float main_{0.0F};
    float cross_{0.0F};
};

class FlexProps final {
public:
    FlexProps& vertical(Prop<bool> value) {
        vertical_ = std::move(value);
        return *this;
    }

    FlexProps& wrap(Prop<bool> value) {
        wrap_ = std::move(value);
        return *this;
    }

    FlexProps& justify(Prop<FlexJustify> value) {
        justify_ = std::move(value);
        return *this;
    }

    FlexProps& align(Prop<FlexAlign> value) {
        align_ = std::move(value);
        return *this;
    }

    FlexProps& gap(Prop<LayoutGap> value) {
        gap_ = std::move(value);
        return *this;
    }

    FlexProps& gap(SpaceSize value) {
        return gap(LayoutGap{value});
    }

    FlexProps& gap(LogicalLength value) {
        return gap(LayoutGap{value});
    }

    FlexProps& gap(LogicalLength main, LogicalLength cross) {
        return gap(LayoutGap{main, cross});
    }

    FlexProps& layout(LayoutStyle value) {
        layout_ = std::move(value);
        return *this;
    }

private:
    friend struct detail::FlexPropsAccess;

    Prop<bool> vertical_{false};
    Prop<bool> wrap_{false};
    Prop<FlexJustify> justify_{FlexJustify::Start};
    Prop<FlexAlign> align_{FlexAlign::Start};
    Prop<LayoutGap> gap_{LayoutGap{}};
    LayoutStyle layout_;
};

struct FlexContentSlot final {};
using FlexContent = SlotContent<FlexContentSlot>;

void Flex(FlexProps props, FlexContent content);

} // namespace ryn
