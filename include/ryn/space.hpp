#pragma once

#include <ryn/component.hpp>
#include <ryn/flex.hpp>
#include <ryn/layout_style.hpp>
#include <ryn/prop.hpp>

#include <utility>
#include <optional>

namespace ryn {
namespace detail {

struct SpacePropsAccess;

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

} // namespace ryn
