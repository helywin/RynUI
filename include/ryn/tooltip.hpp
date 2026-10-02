#pragma once

#include <ryn/component.hpp>
#include <ryn/design_token.hpp>
#include <ryn/layout_style.hpp>
#include <ryn/prop.hpp>
#include <ryn/string.hpp>
#include <functional>
#include <optional>
#include <utility>

namespace ryn {
namespace detail {
class TooltipComponentHost;
}

enum class TooltipPlacement {
    Top,
    TopLeft,
    TopRight,
    Bottom,
    BottomLeft,
    BottomRight,
    Left,
    LeftTop,
    LeftBottom,
    Right,
    RightTop,
    RightBottom
};

enum class TooltipTriggerMode { Manual, Hover, Focus, HoverFocus };

struct TooltipTriggerSlot final {};

using TooltipTrigger = SlotContent<TooltipTriggerSlot>;

class TooltipProps final {
public:
    TooltipProps& title(Prop<String> value) {
        title_ = std::move(value);
        return *this;
    }

    TooltipProps& open(Prop<bool> value) {
        open_ = std::move(value);
        return *this;
    }

    TooltipProps& defaultOpen(bool value) {
        default_open_ = value;
        return *this;
    }

    TooltipProps& disabled(Prop<bool> value) {
        disabled_ = std::move(value);
        return *this;
    }

    TooltipProps& placement(Prop<TooltipPlacement> value) {
        placement_ = std::move(value);
        return *this;
    }

    TooltipProps& trigger(Prop<TooltipTriggerMode> value) {
        trigger_ = std::move(value);
        return *this;
    }

    TooltipProps& arrow(Prop<bool> value) {
        arrow_ = std::move(value);
        return *this;
    }

    TooltipProps& autoAdjustOverflow(Prop<bool> value) {
        adjust_ = std::move(value);
        return *this;
    }

    TooltipProps& mouseEnterDelay(Prop<Duration> value) {
        enter_ = std::move(value);
        return *this;
    }

    TooltipProps& mouseLeaveDelay(Prop<Duration> value) {
        leave_ = std::move(value);
        return *this;
    }

    TooltipProps& onOpenChange(std::function<void(bool)> value) {
        on_open_ = std::move(value);
        return *this;
    }

    TooltipProps& layout(LayoutStyle value) {
        layout_ = std::move(value);
        return *this;
    }

private:
    friend class detail::TooltipComponentHost;
    Prop<String> title_{String{}};
    std::optional<Prop<bool>> open_;
    std::optional<bool> default_open_;
    Prop<bool> disabled_{false};
    Prop<TooltipPlacement> placement_{TooltipPlacement::Top};
    Prop<TooltipTriggerMode> trigger_{TooltipTriggerMode::HoverFocus};
    Prop<bool> arrow_{true};
    Prop<bool> adjust_{true};
    Prop<Duration> enter_{Duration::milliseconds(100)};
    Prop<Duration> leave_{Duration::milliseconds(100)};
    std::function<void(bool)> on_open_;
    LayoutStyle layout_;
};

void Tooltip(TooltipProps props, TooltipTrigger trigger);
} // namespace ryn
