#pragma once
#include <ryn/input_types.hpp>

#include <ryn/component.hpp>
#include <ryn/control_size.hpp>
#include <ryn/flex.hpp>
#include <ryn/layout_style.hpp>
#include <ryn/prop.hpp>

#include <utility>
#include <optional>

namespace ryn {
namespace detail {
struct SpaceAddonPropsAccess;

struct SpacePropsAccess;
struct SpaceCompactPropsAccess;

} // namespace detail

enum class SpaceOrientation { Horizontal, Vertical };

struct SpaceSeparatorSlot final {};

using SpaceSeparator = SlotContent<SpaceSeparatorSlot>;

class SpaceProps final {
public:
    SpaceProps& vertical(Prop<bool> value) {
        orientation_.reset();
        vertical_ = std::move(value);
        return *this;
    }

    SpaceProps& orientation(Prop<SpaceOrientation> value) {
        orientation_ = std::move(value);
        return *this;
    }

    SpaceProps& direction(Prop<FlexDirection> value) {
        direction_ = std::move(value);
        return *this;
    }

    SpaceProps& wrap(Prop<bool> value) {
        wrap_ = std::move(value);
        return *this;
    }

    SpaceProps& align(Prop<SpaceAlign> value) {
        align_ = std::move(value);
        return *this;
    }

    SpaceProps& size(Prop<LayoutGap> value) {
        size_ = std::move(value);
        return *this;
    }

    SpaceProps& size(SpaceSize value) {
        return size(LayoutGap{value});
    }

    SpaceProps& size(LogicalLength value) {
        return size(LayoutGap{value});
    }

    SpaceProps& size(LogicalLength main, LogicalLength cross) {
        return size(LayoutGap{main, cross});
    }

    SpaceProps& separator(SpaceSeparator value) {
        separator_ = std::move(value);
        return *this;
    }

    SpaceProps& split(SpaceSeparator value) {
        return separator(std::move(value));
    }

    SpaceProps& layout(LayoutStyle value) {
        layout_ = std::move(value);
        return *this;
    }

private:
    friend struct detail::SpacePropsAccess;

    Prop<bool> vertical_{false};
    std::optional<Prop<SpaceOrientation>> orientation_;
    Prop<FlexDirection> direction_{FlexDirection::LeftToRight};
    Prop<bool> wrap_{false};
    Prop<SpaceAlign> align_{SpaceAlign::Auto};
    Prop<LayoutGap> size_{LayoutGap{SpaceSize::Small}};
    std::optional<SpaceSeparator> separator_;
    LayoutStyle layout_;
};

struct SpaceContentSlot final {};

using SpaceContent = SlotContent<SpaceContentSlot>;

void Space(SpaceProps props, SpaceContent content);

class SpaceCompactProps final {
public:
    SpaceCompactProps& orientation(Prop<SpaceOrientation> value) {
        orientation_ = std::move(value);
        return *this;
    }

    SpaceCompactProps& direction(Prop<FlexDirection> value) {
        direction_ = std::move(value);
        return *this;
    }

    SpaceCompactProps& size(Prop<ControlSize> value) {
        size_ = std::move(value);
        explicit_size_ = true;
        return *this;
    }

    SpaceCompactProps& block(Prop<bool> value) {
        block_ = std::move(value);
        return *this;
    }

    SpaceCompactProps& layout(LayoutStyle value) {
        layout_ = std::move(value);
        return *this;
    }

private:
    friend struct detail::SpaceCompactPropsAccess;
    Prop<SpaceOrientation> orientation_{SpaceOrientation::Horizontal};
    Prop<FlexDirection> direction_{FlexDirection::LeftToRight};
    Prop<ControlSize> size_{ControlSize::Middle};
    bool explicit_size_{};
    Prop<bool> block_{false};
    LayoutStyle layout_;
};

struct SpaceCompactContentSlot final {};

using SpaceCompactContent = SlotContent<SpaceCompactContentSlot>;

void SpaceCompact(SpaceCompactProps props, SpaceCompactContent content);

class SpaceAddonProps final {
public:
    SpaceAddonProps& size(Prop<ControlSize> value) {
        size_ = std::move(value);
        return *this;
    }

    SpaceAddonProps& variant(Prop<InputVariant> value) {
        variant_ = std::move(value);
        return *this;
    }

    SpaceAddonProps& status(Prop<InputStatus> value) {
        status_ = std::move(value);
        return *this;
    }

    SpaceAddonProps& disabled(Prop<bool> value) {
        disabled_ = std::move(value);
        return *this;
    }

    SpaceAddonProps& layout(LayoutStyle value) {
        layout_ = std::move(value);
        return *this;
    }

private:
    friend struct detail::SpaceAddonPropsAccess;
    std::optional<Prop<ControlSize>> size_;
    Prop<InputVariant> variant_{InputVariant::Outlined};
    Prop<InputStatus> status_{InputStatus::Default};
    Prop<bool> disabled_{false};
    LayoutStyle layout_;
};

struct SpaceAddonContentSlot final {};

using SpaceAddonContent = SlotContent<SpaceAddonContentSlot>;

void SpaceAddon(SpaceAddonProps props, SpaceAddonContent content);

} // namespace ryn
