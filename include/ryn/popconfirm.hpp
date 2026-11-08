#pragma once

#include <ryn/tooltip.hpp>
#include <ryn/component_locales.hpp>
#include <ryn/confirm_action.hpp>
#include <ryn/button.hpp>

namespace ryn {
namespace detail {
class PopconfirmComponentHost;
class AppPanelService;
struct PopconfirmRefState;
struct PopconfirmAnchorBinding;
} // namespace detail

class PopconfirmRef final {
public:
    PopconfirmRef();
    PopconfirmRef(const PopconfirmRef&);
    PopconfirmRef& operator=(const PopconfirmRef&);
    [[nodiscard]] bool bound() const;

private:
    friend class PopconfirmProps;
    friend class detail::PopconfirmComponentHost;
    friend class detail::AppPanelService;
    std::shared_ptr<detail::PopconfirmRefState> state_;
};

using PopconfirmPlacement = TooltipPlacement;
using PopconfirmTriggerMode = TooltipTriggerMode;
using PopconfirmTriggers = TooltipTriggers;

struct PopconfirmTriggerSlot final {};

struct PopconfirmTitleSlot final {};

struct PopconfirmDescriptionSlot final {};

using PopconfirmTrigger = SlotContent<PopconfirmTriggerSlot>;
using PopconfirmTitle = SlotContent<PopconfirmTitleSlot>;
using PopconfirmDescription = SlotContent<PopconfirmDescriptionSlot>;

struct PopconfirmSlots final {
    std::optional<PopconfirmTitle> title;
    std::optional<PopconfirmDescription> description;
};

class PopconfirmProps final {
public:
    PopconfirmProps& ref(const PopconfirmRef& value) {
        reference_ = value.state_;
        return *this;
    }

    // Reserve a real trigger for App service calls; native trigger buttons
    // keep their own activation without opening a second declarative popup.
    PopconfirmProps& serviceAnchor(const PopconfirmRef& value) {
        reference_ = value.state_;
        service_anchor_only_ = true;
        return *this;
    }

    PopconfirmProps& onConfirmAsync(ConfirmAction value) {
        action_ = std::move(value);
        return *this;
    }

    PopconfirmProps& onActionError(std::function<void(ConfirmActionError)> value) {
        action_error_ = std::move(value);
        return *this;
    }

    PopconfirmProps& locale(Prop<CommonLocale> value) {
        locale_ = std::move(value);
        return *this;
    }

    PopconfirmProps& title(Prop<String> value) {
        title_ = std::move(value);
        explicit_title_ = true;
        return *this;
    }

    template <std::size_t N> PopconfirmProps& title(const char8_t (&value)[N]) {
        return title(String{value});
    }

    PopconfirmProps& description(Prop<String> value) {
        description_ = std::move(value);
        explicit_description_ = true;
        return *this;
    }

    template <std::size_t N> PopconfirmProps& description(const char8_t (&value)[N]) {
        return description(String{value});
    }

    PopconfirmProps& open(Prop<bool> value = true) {
        open_ = std::move(value);
        return *this;
    }

    PopconfirmProps& defaultOpen(bool value = true) {
        default_open_ = value;
        return *this;
    }

    PopconfirmProps& disabled(Prop<bool> value = true) {
        disabled_ = std::move(value);
        return *this;
    }

    PopconfirmProps& showCancel(Prop<bool> value = true) {
        show_cancel_ = std::move(value);
        return *this;
    }

    PopconfirmProps& confirmLoading(Prop<bool> value = true) {
        loading_ = std::move(value);
        return *this;
    }

    PopconfirmProps& okText(Prop<String> value) {
        ok_text_ = std::move(value);
        return *this;
    }

    template <std::size_t N> PopconfirmProps& okText(const char8_t (&value)[N]) {
        return okText(String{value});
    }

    PopconfirmProps& cancelText(Prop<String> value) {
        cancel_text_ = std::move(value);
        return *this;
    }

    template <std::size_t N> PopconfirmProps& cancelText(const char8_t (&value)[N]) {
        return cancelText(String{value});
    }

    PopconfirmProps& placement(Prop<PopconfirmPlacement> value) {
        placement_ = std::move(value);
        return *this;
    }

    PopconfirmProps& arrow(Prop<bool> value) {
        arrow_ = std::move(value);
        return *this;
    }

    PopconfirmProps& pointAtCenter(Prop<bool> value = true) {
        point_at_center_ = std::move(value);
        return *this;
    }

    PopconfirmProps& autoAdjustOverflow(Prop<bool> value) {
        adjust_ = std::move(value);
        return *this;
    }

    PopconfirmProps& trigger(Prop<PopconfirmTriggerMode> value) {
        trigger_ = std::move(value);
        triggers_.reset();
        return *this;
    }

    PopconfirmProps& triggers(Prop<PopconfirmTriggers> value) {
        triggers_ = std::move(value);
        return *this;
    }

    PopconfirmProps& mouseEnterDelay(Prop<Duration> value) {
        enter_ = std::move(value);
        return *this;
    }

    PopconfirmProps& mouseLeaveDelay(Prop<Duration> value) {
        leave_ = std::move(value);
        return *this;
    }

    PopconfirmProps& direction(Prop<TextDirection> value) {
        direction_ = std::move(value);
        direction_set_ = true;
        return *this;
    }

    PopconfirmProps& animated(Prop<bool> value = true) {
        animated_ = std::move(value);
        return *this;
    }

    PopconfirmProps& okButtonProps(ButtonProps value) {
        ok_button_ = std::move(value);
        return *this;
    }

    PopconfirmProps& cancelButtonProps(ButtonProps value) {
        cancel_button_ = std::move(value);
        return *this;
    }

    PopconfirmProps& afterOpenChange(std::function<void(bool)> value) {
        after_open_ = std::move(value);
        return *this;
    }

    PopconfirmProps& onOpenChange(std::function<void(bool)> value) {
        on_open_ = std::move(value);
        return *this;
    }

    PopconfirmProps& onConfirm(std::function<void()> value) {
        on_confirm_ = std::move(value);
        return *this;
    }

    PopconfirmProps& onCancel(std::function<void()> value) {
        on_cancel_ = std::move(value);
        return *this;
    }

    PopconfirmProps& layout(LayoutStyle value) {
        layout_ = std::move(value);
        return *this;
    }

private:
    friend class detail::PopconfirmComponentHost;
    friend class detail::AppPanelService;
    std::shared_ptr<detail::PopconfirmRefState> reference_;
    bool service_anchor_only_{};
    std::shared_ptr<detail::PopconfirmAnchorBinding> service_anchor_;
    ConfirmAction action_;
    std::function<bool()> action_live_;
    std::function<void(ConfirmActionError)> action_error_;
    Prop<String> title_{String{}};
    Prop<String> description_{String{}};
    bool explicit_title_{};
    bool explicit_description_{};
    std::optional<Prop<bool>> open_;
    std::optional<bool> default_open_;
    Prop<bool> disabled_{false};
    Prop<bool> show_cancel_{true};
    Prop<bool> loading_{false};
    std::optional<Prop<String>> ok_text_;
    std::optional<Prop<String>> cancel_text_;
    std::optional<Prop<CommonLocale>> locale_;
    Prop<PopconfirmPlacement> placement_{PopconfirmPlacement::Top};
    Prop<bool> arrow_{true};
    Prop<bool> point_at_center_{false};
    Prop<bool> adjust_{true};
    Prop<PopconfirmTriggerMode> trigger_{PopconfirmTriggerMode::Click};
    std::optional<Prop<PopconfirmTriggers>> triggers_;
    Prop<Duration> enter_{Duration::milliseconds(100)};
    Prop<Duration> leave_{Duration::milliseconds(100)};
    Prop<TextDirection> direction_{TextDirection::LeftToRight};
    bool direction_set_{};
    Prop<bool> animated_{false};
    std::optional<ButtonProps> ok_button_;
    std::optional<ButtonProps> cancel_button_;
    std::function<void(bool)> after_open_;
    std::function<void(bool)> on_open_;
    std::function<void()> on_confirm_;
    std::function<void()> on_cancel_;
    LayoutStyle layout_;
};

void Popconfirm(PopconfirmProps props, PopconfirmTrigger trigger, PopconfirmSlots slots = {});
} // namespace ryn
