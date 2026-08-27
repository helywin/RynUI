#pragma once

#include <ryn/component.hpp>
#include <ryn/flex.hpp>
#include <ryn/layout_style.hpp>
#include <ryn/prop.hpp>

#include <utility>

namespace ryn {
namespace detail {

struct SpacePropsAccess;

} // namespace detail

class SpaceProps final {
public:
    SpaceProps& vertical(Prop<bool> value) {
        vertical_ = std::move(value);
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

    SpaceProps& layout(LayoutStyle value) {
        layout_ = std::move(value);
        return *this;
    }

private:
    friend struct detail::SpacePropsAccess;

    Prop<bool> vertical_{false};
    Prop<bool> wrap_{false};
    Prop<SpaceAlign> align_{SpaceAlign::Start};
    Prop<LayoutGap> size_{LayoutGap{SpaceSize::Small}};
    LayoutStyle layout_;
};

struct SpaceContentSlot final {};
using SpaceContent = SlotContent<SpaceContentSlot>;

void Space(SpaceProps props, SpaceContent content);

} // namespace ryn
