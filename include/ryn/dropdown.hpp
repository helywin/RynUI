#pragma once

#include <ryn/menu.hpp>
#include <ryn/button.hpp>
#include <ryn/tooltip.hpp>

namespace ryn {
namespace detail {
class DropdownComponentHost;
}

using DropdownPlacement = TooltipPlacement;
enum class DropdownTriggerMode {
    Manual,
    Click,
    ContextMenu,
    ClickContextMenu,
    Hover,
    HoverClick,
    HoverContextMenu,
    HoverClickContextMenu
};

enum class DropdownTriggerCause { Pointer, Keyboard, Hover, ContextMenu };
enum class DropdownOpenSource { Trigger, Menu, Escape, Outside, Environment };

struct DropdownOpenInfo final {
    bool open{};
    DropdownOpenSource source{DropdownOpenSource::Trigger};
    DropdownTriggerCause cause{DropdownTriggerCause::Pointer};
};

struct DropdownTriggerSlot final {};

using DropdownTrigger = SlotContent<DropdownTriggerSlot>;

struct DropdownPopupSlot final {};

using DropdownPopup = SlotContent<DropdownPopupSlot>;

struct DropdownButtonContentSlot final {};

using DropdownButtonContent = SlotContent<DropdownButtonContentSlot>;

struct DropdownButtonIconSlot final {};

using DropdownButtonIcon = SlotContent<DropdownButtonIconSlot>;

class DropdownMenuContent final {
public:
    void operator()() const {
        mount_();
    }

private:
    friend class detail::DropdownComponentHost;

    explicit DropdownMenuContent(std::function<void()> mount) : mount_(std::move(mount)) {}

    std::function<void()> mount_;
};

struct DropdownArrow final {
    bool visible{true};
    bool point_at_center{};
    friend bool operator==(const DropdownArrow&, const DropdownArrow&) = default;
};

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

    DropdownProps& popupRender(DropdownPopup value) {
        popup_render_ = {};
        popup_ = std::move(value);
        return *this;
    }

    DropdownProps& popupRender(std::function<void(DropdownMenuContent)> value) {
        popup_.reset();
        popup_render_ = std::move(value);
        return *this;
    }

    DropdownProps& arrow(Prop<DropdownArrow> value) {
        arrow_ = std::move(value);
        return *this;
    }

    DropdownProps& arrow(bool value = true) {
        return arrow(DropdownArrow{value, false});
    }

    DropdownProps& mouseEnterDelay(Prop<float> seconds) {
        enter_delay_ = std::move(seconds);
        return *this;
    }

    DropdownProps& mouseLeaveDelay(Prop<float> seconds) {
        leave_delay_ = std::move(seconds);
        return *this;
    }

    DropdownProps& autoFocus(Prop<bool> value) {
        auto_focus_ = std::move(value);
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
    Prop<DropdownArrow> arrow_{DropdownArrow{false, false}};
    Prop<float> enter_delay_{0.15F};
    Prop<float> leave_delay_{0.1F};
    Prop<bool> auto_focus_{true};
    std::optional<DropdownPopup> popup_;
    std::function<void(DropdownMenuContent)> popup_render_;
    std::function<void(DropdownOpenInfo)> on_open_;
    LayoutStyle layout_;
};

void Dropdown(DropdownProps props, DropdownTrigger trigger);

// Native equivalent of Dropdown.Button, composed from existing Button and SpaceCompact.
struct DropdownButtonProps final {
    DropdownProps dropdown;
    ButtonProps button;
    ButtonProps trigger_button;
};

struct DropdownButtonSlots final {
    DropdownButtonContent content;
    std::optional<DropdownButtonIcon> icon;
};

void DropdownButton(DropdownButtonProps props, DropdownButtonSlots slots);
} // namespace ryn
