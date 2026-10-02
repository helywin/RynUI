#pragma once

#include <ryn/component.hpp>
#include <ryn/layout_style.hpp>
#include <ryn/prop.hpp>
#include <ryn/string.hpp>

#include <functional>
#include <initializer_list>
#include <memory>
#include <optional>
#include <utility>
#include <variant>
#include <vector>

namespace ryn {
namespace detail {
struct RadioPropsAccess;
struct RadioGroupPropsAccess;
struct RadioRefState;
} // namespace detail

using RadioValue = std::variant<String, double, bool>;
using RadioSelection = std::optional<RadioValue>;

enum class RadioDirection { LeftToRight, RightToLeft };

class RadioRef final {
public:
    RadioRef();
    [[nodiscard]] bool bound() const;
    [[nodiscard]] bool focus() const;
    [[nodiscard]] bool blur() const;

private:
    friend struct detail::RadioPropsAccess;
    std::shared_ptr<detail::RadioRefState> state_;
};

class RadioProps final {
public:
    RadioProps& checked(Prop<bool> value) {
        checked_ = std::move(value);
        return *this;
    }

    RadioProps& defaultChecked(bool value) {
        default_checked_ = value;
        return *this;
    }

    RadioProps& disabled(Prop<bool> value) {
        disabled_ = std::move(value);
        return *this;
    }

    RadioProps& value(RadioValue value) {
        value_ = std::move(value);
        return *this;
    }

    RadioProps& onChange(std::function<void(bool)> callback) {
        on_change_ = std::move(callback);
        return *this;
    }

    RadioProps& direction(Prop<RadioDirection> value) {
        direction_ = std::move(value);
        return *this;
    }

    RadioProps& ref(RadioRef value) {
        ref_ = std::move(value);
        return *this;
    }

    RadioProps& autoFocus(bool value) {
        auto_focus_ = value;
        return *this;
    }

    RadioProps& onClick(std::function<void(bool)> value) {
        on_click_ = std::move(value);
        return *this;
    }

    RadioProps& layout(LayoutStyle value) {
        layout_ = std::move(value);
        return *this;
    }

private:
    friend struct detail::RadioPropsAccess;
    std::optional<Prop<bool>> checked_;
    std::optional<bool> default_checked_;
    Prop<bool> disabled_{false};
    RadioSelection value_;
    std::function<void(bool)> on_change_;
    std::optional<Prop<RadioDirection>> direction_;
    std::optional<RadioRef> ref_;
    bool auto_focus_{};
    std::function<void(bool)> on_click_;
    LayoutStyle layout_;
};

struct RadioLabelSlot final {};

using RadioLabel = SlotContent<RadioLabelSlot>;

void Radio(RadioProps props, std::optional<RadioLabel> label = {});

struct RadioOption final {
    RadioValue value;
    String label;
    bool disabled{};

    friend bool operator==(const RadioOption&, const RadioOption&) = default;
};

enum class RadioGroupOrientation { Horizontal, Vertical };

class RadioGroupProps final {
public:
    RadioGroupProps& options(Prop<std::vector<RadioOption>> value) {
        options_ = std::move(value);
        return *this;
    }

    RadioGroupProps& options(std::initializer_list<RadioOption> value) {
        return options(Prop<std::vector<RadioOption>>{std::vector<RadioOption>{value}});
    }

    RadioGroupProps& value(Prop<std::optional<String>> value) {
        value_ = std::move(value);
        return *this;
    }

    RadioGroupProps& selection(Prop<RadioSelection> value) {
        selection_ = std::move(value);
        return *this;
    }

    RadioGroupProps& defaultValue(RadioValue value) {
        default_value_ = std::move(value);
        return *this;
    }

    RadioGroupProps& disabled(Prop<bool> value) {
        disabled_ = std::move(value);
        return *this;
    }

    RadioGroupProps& orientation(Prop<RadioGroupOrientation> value) {
        orientation_ = std::move(value);
        return *this;
    }

    RadioGroupProps& onChange(std::function<void(const String&)> callback) {
        on_change_ = std::move(callback);
        return *this;
    }

    RadioGroupProps& onValueChange(std::function<void(const RadioValue&)> callback) {
        on_value_change_ = std::move(callback);
        return *this;
    }

    RadioGroupProps& direction(Prop<RadioDirection> value) {
        direction_ = std::move(value);
        return *this;
    }

    RadioGroupProps& layout(LayoutStyle value) {
        layout_ = std::move(value);
        return *this;
    }

private:
    friend struct detail::RadioGroupPropsAccess;
    std::optional<Prop<std::vector<RadioOption>>> options_;
    std::optional<Prop<std::optional<String>>> value_;
    std::optional<Prop<RadioSelection>> selection_;
    RadioSelection default_value_;
    Prop<bool> disabled_{false};
    Prop<RadioGroupOrientation> orientation_{RadioGroupOrientation::Horizontal};
    std::function<void(const String&)> on_change_;
    std::function<void(const RadioValue&)> on_value_change_;
    Prop<RadioDirection> direction_{RadioDirection::LeftToRight};
    LayoutStyle layout_;
};

struct RadioGroupContentSlot final {};

using RadioGroupContent = SlotContent<RadioGroupContentSlot>;

void RadioGroup(RadioGroupProps props, std::optional<RadioGroupContent> content = {});

} // namespace ryn
