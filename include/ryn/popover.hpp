#pragma once

#include <ryn/tooltip.hpp>

namespace ryn {
namespace detail {
class PopoverComponentHost;
}

using PopoverPlacement = TooltipPlacement;

enum class PopoverTriggerMode { Manual, Hover, Focus, HoverFocus, Click };

struct PopoverTriggers final {
    bool hover{};
    bool focus{};
    bool click{};
    friend bool operator==(const PopoverTriggers&, const PopoverTriggers&) = default;
};

struct PopoverTriggerSlot final {};

struct PopoverTitleSlot final {};

struct PopoverContentSlot final {};

using PopoverTrigger = SlotContent<PopoverTriggerSlot>;
using PopoverTitle = SlotContent<PopoverTitleSlot>;
using PopoverContent = SlotContent<PopoverContentSlot>;

struct PopoverSlots final {
    std::optional<PopoverTitle> title;
    std::optional<PopoverContent> content;
};

class PopoverProps final {
public:
    PopoverProps& title(Prop<String> value) {
        title_ = std::move(value);
        explicit_title_ = true;
        return *this;
    }

    template <std::size_t N> PopoverProps& title(const char8_t (&value)[N]) {
        return title(String{value});
    }

    PopoverProps& content(Prop<String> value) {
        content_ = std::move(value);
        explicit_content_ = true;
        return *this;
    }

    template <std::size_t N> PopoverProps& content(const char8_t (&value)[N]) {
        return content(String{value});
    }

    PopoverProps& open(Prop<bool> value = true) {
        open_ = std::move(value);
        return *this;
    }

    PopoverProps& defaultOpen(bool value = true) {
        default_open_ = value;
        return *this;
    }

    PopoverProps& disabled(Prop<bool> value = true) {
        disabled_ = std::move(value);
        return *this;
    }

    PopoverProps& trigger(Prop<PopoverTriggerMode> value) {
        trigger_ = std::move(value);
        explicit_trigger_ = true;
        return *this;
    }

    PopoverProps& triggers(Prop<PopoverTriggers> value) {
        triggers_ = std::move(value);
        return *this;
    }

    PopoverProps& placement(Prop<PopoverPlacement> value) {
        placement_ = std::move(value);
        return *this;
    }

    PopoverProps& arrow(Prop<bool> value) {
        arrow_ = std::move(value);
        return *this;
    }

    PopoverProps& pointAtCenter(Prop<bool> value = true) {
        point_at_center_ = std::move(value);
        return *this;
    }

    PopoverProps& autoAdjustOverflow(Prop<bool> value) {
        adjust_ = std::move(value);
        return *this;
    }

    PopoverProps& mouseEnterDelay(Prop<Duration> value) {
        enter_ = std::move(value);
        return *this;
    }

    PopoverProps& mouseLeaveDelay(Prop<Duration> value) {
        leave_ = std::move(value);
        return *this;
    }

    PopoverProps& onOpenChange(std::function<void(bool)> value) {
        on_open_ = std::move(value);
        return *this;
    }

    PopoverProps& layout(LayoutStyle value) {
        layout_ = std::move(value);
        return *this;
    }

private:
    friend class detail::PopoverComponentHost;
    Prop<String> title_{String{}};
    Prop<String> content_{String{}};
    bool explicit_title_{};
    bool explicit_content_{};
    std::optional<Prop<bool>> open_;
    std::optional<bool> default_open_;
    Prop<bool> disabled_{false};
    Prop<PopoverTriggerMode> trigger_{PopoverTriggerMode::Hover};
    bool explicit_trigger_{};
    std::optional<Prop<PopoverTriggers>> triggers_;
    Prop<PopoverPlacement> placement_{PopoverPlacement::Top};
    Prop<bool> arrow_{true};
    Prop<bool> point_at_center_{false};
    Prop<bool> adjust_{true};
    Prop<Duration> enter_{Duration::milliseconds(100)};
    Prop<Duration> leave_{Duration::milliseconds(100)};
    std::function<void(bool)> on_open_;
    LayoutStyle layout_;
};

void Popover(PopoverProps props, PopoverTrigger trigger, PopoverSlots slots = {});
} // namespace ryn
