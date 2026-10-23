#pragma once

#include <ryn/menu.hpp>
#include <ryn/tooltip.hpp>

namespace ryn {
namespace detail {
class DropdownComponentHost;
}

using DropdownPlacement = TooltipPlacement;
enum class DropdownTriggerMode { Manual, Click, ContextMenu, ClickContextMenu };
enum class DropdownOpenSource { Trigger, Menu, Escape, Outside, Environment };

struct DropdownOpenInfo final {
    bool open{};
    DropdownOpenSource source{DropdownOpenSource::Trigger};
};

struct DropdownTriggerSlot final {};

using DropdownTrigger = SlotContent<DropdownTriggerSlot>;

class DropdownProps final {
public:
    DropdownProps& menu(MenuProps value) {
        menu_ = std::move(value);
        return *this;
    }

    DropdownProps& open(Prop<bool> value) {
        open_ = std::move(value);
        return *this;
    }

    DropdownProps& defaultOpen(bool value) {
        default_open_ = value;
        return *this;
    }

    DropdownProps& disabled(Prop<bool> value = true) {
        disabled_ = std::move(value);
        return *this;
    }

    DropdownProps& trigger(Prop<DropdownTriggerMode> value) {
        trigger_ = std::move(value);
        return *this;
    }

    DropdownProps& placement(Prop<DropdownPlacement> value) {
        placement_ = std::move(value);
        return *this;
    }

    DropdownProps& autoAdjustOverflow(Prop<bool> value) {
        adjust_ = std::move(value);
        return *this;
    }

    DropdownProps& closeOnSelect(Prop<bool> value) {
        close_on_select_ = std::move(value);
        return *this;
    }

    DropdownProps& onOpenChange(std::function<void(DropdownOpenInfo)> value) {
        on_open_ = std::move(value);
        return *this;
    }

    DropdownProps& layout(LayoutStyle value) {
        layout_ = std::move(value);
        return *this;
    }

private:
    friend class detail::DropdownComponentHost;
    MenuProps menu_{MenuProps{}.mode(MenuMode::Inline).selectable(false)};
    std::optional<Prop<bool>> open_;
    std::optional<bool> default_open_;
    Prop<bool> disabled_{false};
    Prop<DropdownTriggerMode> trigger_{DropdownTriggerMode::Click};
    Prop<DropdownPlacement> placement_{DropdownPlacement::BottomLeft};
    Prop<bool> adjust_{true};
    Prop<bool> close_on_select_{true};
    std::function<void(DropdownOpenInfo)> on_open_;
    LayoutStyle layout_;
};

void Dropdown(DropdownProps props, DropdownTrigger trigger);
} // namespace ryn
