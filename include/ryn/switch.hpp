#pragma once

#include <ryn/component.hpp>
#include <ryn/form_geometry.hpp>
#include <ryn/layout_style.hpp>
#include <ryn/prop.hpp>

#include <functional>
#include <memory>
#include <optional>
#include <utility>

namespace ryn {
class SwitchRef;

namespace detail {
struct FormFocusContext;
std::pair<std::function<bool(const FormFocusContext&)>,
          std::function<std::optional<FormControlGeometry>(const FormFocusContext&)>>
capture_form_switch(const SwitchRef&);
struct FormAutoControlAccess;
struct SwitchPropsAccess;
struct SwitchRefState;
} // namespace detail

enum class SwitchSize { Middle, Small };

enum class SwitchDirection { LeftToRight, RightToLeft };

class SwitchRef final {
public:
    SwitchRef();
    [[nodiscard]] bool bound() const;
    [[nodiscard]] bool focus() const;
    [[nodiscard]] bool blur() const;

private:
    friend struct detail::SwitchPropsAccess;
    friend std::pair<std::function<bool(const detail::FormFocusContext&)>,
                     std::function<std::optional<FormControlGeometry>(const detail::FormFocusContext&)>>
    detail::capture_form_switch(const SwitchRef&);
    std::shared_ptr<detail::SwitchRefState> state_;
};

class SwitchProps final {
public:
    SwitchProps& checked(Prop<bool> value) {
        checked_ = std::move(value);
        return *this;
    }

    SwitchProps& defaultChecked(bool value) {
        default_checked_ = value;
        return *this;
    }

    SwitchProps& disabled(Prop<bool> value) {
        disabled_ = std::move(value);
        return *this;
    }

    SwitchProps& loading(Prop<bool> value) {
        loading_ = std::move(value);
        return *this;
    }

    SwitchProps& size(Prop<SwitchSize> value) {
        size_ = std::move(value);
        return *this;
    }

    SwitchProps& onChange(std::function<void(bool)> callback) {
        on_change_ = std::move(callback);
        return *this;
    }

    SwitchProps& direction(Prop<SwitchDirection> value) {
        direction_ = std::move(value);
        return *this;
    }

    SwitchProps& wave(Prop<bool> value) {
        wave_ = std::move(value);
        return *this;
    }

    SwitchProps& ref(SwitchRef value) {
        ref_ = std::move(value);
        return *this;
    }

    SwitchProps& autoFocus(bool value) {
        auto_focus_ = value;
        return *this;
    }

    SwitchProps& onClick(std::function<void(bool)> callback) {
        on_click_ = std::move(callback);
        return *this;
    }

    SwitchProps& onFocus(std::function<void()> callback) {
        on_focus_ = std::move(callback);
        return *this;
    }

    SwitchProps& onBlur(std::function<void()> callback) {
        on_blur_ = std::move(callback);
        return *this;
    }

    SwitchProps& layout(LayoutStyle value) {
        layout_ = std::move(value);
        return *this;
    }

private:
    friend struct detail::FormAutoControlAccess;
    friend struct detail::SwitchPropsAccess;
    std::optional<Prop<bool>> checked_;
    std::optional<bool> default_checked_;
    Prop<bool> disabled_{false};
    Prop<bool> loading_{false};
    Prop<SwitchSize> size_{SwitchSize::Middle};
    std::function<void(bool)> on_change_;
    std::function<void(bool)> on_click_;
    std::function<void()> on_focus_;
    std::function<void()> on_blur_;
    Prop<SwitchDirection> direction_{SwitchDirection::LeftToRight};
    Prop<bool> wave_{true};
    std::optional<SwitchRef> ref_;
    bool auto_focus_{};
    LayoutStyle layout_;
};

void Switch(SwitchProps props);

struct SwitchCheckedContentSlot final {};

struct SwitchUncheckedContentSlot final {};

using SwitchCheckedContent = SlotContent<SwitchCheckedContentSlot>;
using SwitchUncheckedContent = SlotContent<SwitchUncheckedContentSlot>;

struct SwitchSlots final {
    std::optional<SwitchCheckedContent> checked;
    std::optional<SwitchUncheckedContent> unchecked;
};

void Switch(SwitchProps props, SwitchSlots slots);

} // namespace ryn
