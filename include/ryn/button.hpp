#pragma once

#include <ryn/component.hpp>
#include <ryn/layout_style.hpp>
#include <ryn/prop.hpp>

#include <functional>
#include <utility>

namespace ryn {
namespace detail {

struct ButtonPropsAccess;

} // namespace detail

enum class ControlSize {
    Small,
    Middle,
    Large,
};

enum class ButtonType {
    Default,
    Primary,
};

class ButtonProps final {
public:
    ButtonProps& type(Prop<ButtonType> value) {
        type_ = std::move(value);
        return *this;
    }

    ButtonProps& size(Prop<ControlSize> value) {
        size_ = std::move(value);
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
    Prop<ControlSize> size_{ControlSize::Middle};
    Prop<bool> disabled_{false};
    Prop<bool> loading_{false};
    std::function<void()> on_click_;
    LayoutStyle layout_;
};

struct ButtonContentSlot final {};
using ButtonContent = SlotContent<ButtonContentSlot>;

void Button(ButtonProps props, ButtonContent content);

} // namespace ryn
