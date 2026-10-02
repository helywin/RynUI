#pragma once

#include <ryn/component.hpp>
#include <ryn/layout_style.hpp>
#include <ryn/prop.hpp>
#include <ryn/string.hpp>

#include <functional>
#include <memory>
#include <optional>
#include <utility>
#include <variant>
#include <vector>

namespace ryn {
namespace detail {
struct CheckboxPropsAccess;
struct CheckboxGroupPropsAccess;
struct CheckboxRefState;
} // namespace detail

using CheckboxValue = std::variant<String, double, bool>;
using CheckboxValues = std::vector<CheckboxValue>;

struct CheckboxOption final {
    CheckboxValue value;
    String label;
    bool disabled{};

    friend bool operator==(const CheckboxOption&, const CheckboxOption&) = default;
};

enum class CheckboxGroupOrientation { Horizontal, Vertical };

enum class CheckboxDirection { LeftToRight, RightToLeft };

class CheckboxRef final {
public:
    CheckboxRef();
    [[nodiscard]] bool bound() const;
    [[nodiscard]] bool focus() const;
    [[nodiscard]] bool blur() const;

private:
    friend struct detail::CheckboxPropsAccess;
    std::shared_ptr<detail::CheckboxRefState> state_;
};

class CheckboxProps final {
public:
    CheckboxProps& checked(Prop<bool> value) {
        checked_ = std::move(value);
        return *this;
    }

    CheckboxProps& defaultChecked(bool value) {
        default_checked_ = value;
        return *this;
    }

    CheckboxProps& indeterminate(Prop<bool> value) {
        indeterminate_ = std::move(value);
        return *this;
    }

    CheckboxProps& disabled(Prop<bool> value) {
        disabled_ = std::move(value);
        return *this;
    }

    CheckboxProps& onChange(std::function<void(bool)> callback) {
        on_change_ = std::move(callback);
        return *this;
    }

    CheckboxProps& value(CheckboxValue value) {
        value_ = std::move(value);
        return *this;
    }

    CheckboxProps& skipGroup(bool value) {
        skip_group_ = value;
        return *this;
    }

    CheckboxProps& direction(Prop<CheckboxDirection> value) {
        direction_ = std::move(value);
        return *this;
    }

    CheckboxProps& wave(Prop<bool> value) {
        wave_ = std::move(value);
        return *this;
    }

    CheckboxProps& ref(CheckboxRef value) {
        ref_ = std::move(value);
        return *this;
    }

    CheckboxProps& autoFocus(bool value) {
        auto_focus_ = value;
        return *this;
    }

    CheckboxProps& onClick(std::function<void(bool)> value) {
        on_click_ = std::move(value);
        return *this;
    }

    CheckboxProps& layout(LayoutStyle value) {
        layout_ = std::move(value);
        return *this;
    }

private:
    friend struct detail::CheckboxPropsAccess;
    std::optional<Prop<bool>> checked_;
    std::optional<bool> default_checked_;
    Prop<bool> indeterminate_{false};
    Prop<bool> disabled_{false};
    std::function<void(bool)> on_change_;
    std::optional<CheckboxValue> value_;
    bool skip_group_{};
    std::optional<Prop<CheckboxDirection>> direction_;
    Prop<bool> wave_{true};
    std::optional<CheckboxRef> ref_;
    bool auto_focus_{};
    std::function<void(bool)> on_click_;
    LayoutStyle layout_;
};

struct CheckboxLabelSlot final {};

using CheckboxLabel = SlotContent<CheckboxLabelSlot>;

void Checkbox(CheckboxProps props, std::optional<CheckboxLabel> label = {});

class CheckboxGroupProps final {
public:
    CheckboxGroupProps& options(Prop<std::vector<CheckboxOption>> value) {
        options_ = std::move(value);
        return *this;
    }

    CheckboxGroupProps& value(Prop<CheckboxValues> value) {
        value_ = std::move(value);
        return *this;
    }

    CheckboxGroupProps& defaultValue(CheckboxValues value) {
        default_value_ = std::move(value);
        return *this;
    }

    CheckboxGroupProps& disabled(Prop<bool> value) {
        disabled_ = std::move(value);
        return *this;
    }

    CheckboxGroupProps& orientation(Prop<CheckboxGroupOrientation> value) {
        orientation_ = std::move(value);
        return *this;
    }

    CheckboxGroupProps& direction(Prop<CheckboxDirection> value) {
        direction_ = std::move(value);
        return *this;
    }

    CheckboxGroupProps& onChange(std::function<void(const CheckboxValues&)> callback) {
        on_change_ = std::move(callback);
        return *this;
    }

    CheckboxGroupProps& layout(LayoutStyle value) {
        layout_ = std::move(value);
        return *this;
    }

private:
    friend struct detail::CheckboxGroupPropsAccess;
    std::optional<Prop<std::vector<CheckboxOption>>> options_;
    std::optional<Prop<CheckboxValues>> value_;
    std::optional<CheckboxValues> default_value_;
    Prop<bool> disabled_{false};
    Prop<CheckboxGroupOrientation> orientation_{CheckboxGroupOrientation::Horizontal};
    Prop<CheckboxDirection> direction_{CheckboxDirection::LeftToRight};
    std::function<void(const CheckboxValues&)> on_change_;
    LayoutStyle layout_;
};

struct CheckboxGroupContentSlot final {};

using CheckboxGroupContent = SlotContent<CheckboxGroupContentSlot>;

void CheckboxGroup(CheckboxGroupProps props, std::optional<CheckboxGroupContent> content = {});

} // namespace ryn
