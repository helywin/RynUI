#pragma once

#include <ryn/component.hpp>
#include <ryn/button_types.hpp>
#include <ryn/control_size.hpp>
#include <ryn/design_token.hpp>
#include <ryn/layout_style.hpp>
#include <ryn/prop.hpp>

#include <functional>
#include <memory>
#include <optional>
#include <utility>

namespace ryn {
namespace detail {

struct ButtonPropsAccess;
struct ButtonRefState;

} // namespace detail

class ButtonRef final {
public:
    ButtonRef();
    [[nodiscard]] bool focus() const;
    [[nodiscard]] bool blur() const;
    [[nodiscard]] bool bound() const;

private:
    friend struct detail::ButtonPropsAccess;
    std::shared_ptr<detail::ButtonRefState> state_;
};

class ButtonProps final {
public:
    ButtonProps& type(Prop<ButtonType> value) {
        type_ = std::move(value);
        return *this;
    }

    ButtonProps& size(Prop<ControlSize> value) {
        size_ = std::move(value);
        explicit_size_ = true;
        return *this;
    }

    ButtonProps& color(Prop<ButtonColor> value) {
        color_ = std::move(value);
        return *this;
    }

    ButtonProps& variant(Prop<ButtonVariant> value) {
        variant_ = std::move(value);
        return *this;
    }

    ButtonProps& danger(Prop<bool> value) {
        danger_ = std::move(value);
        return *this;
    }

    ButtonProps& ghost(Prop<bool> value) {
        ghost_ = std::move(value);
        return *this;
    }

    ButtonProps& disabled(Prop<bool> value) {
        disabled_ = std::move(value);
        return *this;
    }

    ButtonProps& loading(Prop<bool> value) {
        loading_ = std::move(value);
        return *this;
    }

    ButtonProps& loadingDelay(Prop<Duration> value) {
        loading_delay_ = std::move(value);
        return *this;
    }

    ButtonProps& iconPlacement(Prop<ButtonIconPlacement> value) {
        icon_placement_ = std::move(value);
        return *this;
    }

    ButtonProps& shape(Prop<ButtonShape> value) {
        shape_ = std::move(value);
        return *this;
    }

    ButtonProps& block(Prop<bool> value) {
        block_ = std::move(value);
        return *this;
    }

    ButtonProps& wave(Prop<bool> value) {
        wave_ = std::move(value);
        return *this;
    }

    ButtonProps& ref(ButtonRef value) {
        ref_ = std::move(value);
        return *this;
    }

    ButtonProps& autoFocus(bool value) {
        auto_focus_ = value;
        return *this;
    }

    ButtonProps& onClick(std::function<void()> callback) {
        on_click_ = std::move(callback);
        return *this;
    }

    ButtonProps& layout(LayoutStyle value) {
        layout_ = std::move(value);
        return *this;
    }

private:
    friend struct detail::ButtonPropsAccess;

    Prop<ButtonType> type_{ButtonType::Default};
    std::optional<Prop<ButtonColor>> color_;
    std::optional<Prop<ButtonVariant>> variant_;
    Prop<bool> danger_{false};
    Prop<bool> ghost_{false};
    Prop<ControlSize> size_{ControlSize::Middle};
    bool explicit_size_{};
    Prop<bool> disabled_{false};
    Prop<bool> loading_{false};
    Prop<Duration> loading_delay_{Duration{}};
    Prop<ButtonIconPlacement> icon_placement_{ButtonIconPlacement::Start};
    Prop<ButtonShape> shape_{ButtonShape::Default};
    Prop<bool> block_{false};
    Prop<bool> wave_{true};
    std::optional<ButtonRef> ref_;
    bool auto_focus_{};
    std::function<void()> on_click_;
    LayoutStyle layout_;
};

struct ButtonContentSlot final {};

struct ButtonIconSlot final {};

struct ButtonLoadingIconSlot final {};

using ButtonContent = SlotContent<ButtonContentSlot>;
using ButtonIcon = SlotContent<ButtonIconSlot>;
using ButtonLoadingIcon = SlotContent<ButtonLoadingIconSlot>;

struct ButtonSlots final {
    std::optional<ButtonContent> content;
    std::optional<ButtonIcon> icon;
    std::optional<ButtonLoadingIcon> loading;
};

void Button(ButtonProps props, ButtonContent content);
void Button(ButtonProps props, ButtonSlots slots);
void Button(ButtonProps props, ButtonContent content, ButtonIcon icon);
void Button(ButtonProps props, ButtonContent content, ButtonIcon icon, ButtonLoadingIcon loading);

} // namespace ryn
