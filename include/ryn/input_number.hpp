#pragma once

#include <ryn/input.hpp>
#include <ryn/decimal.hpp>

namespace ryn {

using InputNumberValue = std::optional<double>;
using InputNumberStringValue = std::optional<Decimal>;

struct InputNumberFormatInfo final {
    bool user_typing{};
    String input;
};

enum class InputNumberMode { Input, Spinner };

struct InputNumberUpIconSlot final {};

struct InputNumberDownIconSlot final {};

using InputNumberUpIcon = SlotContent<InputNumberUpIconSlot>;
using InputNumberDownIcon = SlotContent<InputNumberDownIconSlot>;

struct InputNumberControls final {
    bool enabled{true};
    std::optional<InputNumberUpIcon> up_icon;
    std::optional<InputNumberDownIcon> down_icon;
};

enum class InputNumberStepDirection { Up, Down };

enum class InputNumberStepSource { Button, Keyboard, Wheel };

struct InputNumberStringStepInfo final {
    Decimal offset;
    InputNumberStepDirection direction;
    InputNumberStepSource source;
};

struct InputNumberStepInfo final {
    double offset{};
    InputNumberStepDirection direction{InputNumberStepDirection::Up};
    InputNumberStepSource source{InputNumberStepSource::Button};
};

namespace detail {
struct FormAutoControlAccess;
struct InputNumberPropsAccess;
} // namespace detail

class InputNumberProps final {
public:
    InputNumberProps& mode(Prop<InputNumberMode> value) {
        mode_ = std::move(value);
        return *this;
    }

    InputNumberProps& decimalSeparator(Prop<String> value) {
        separator_ = std::move(value);
        return *this;
    }

    InputNumberProps& controls(InputNumberControls value) {
        controls_ = value.enabled;
        icons_ = std::move(value);
        return *this;
    }

    InputNumberProps& stringMode(bool value = true) {
        string_mode_ = value;
        return *this;
    }

    InputNumberProps& value(Prop<InputNumberStringValue> value) {
        string_value_ = std::move(value);
        string_mode_ = true;
        return *this;
    }

    InputNumberProps& value(InputNumberStringValue value) {
        return this->value(Prop<InputNumberStringValue>{std::move(value)});
    }

    InputNumberProps& value(Decimal value) {
        return this->value(InputNumberStringValue{std::move(value)});
    }

    InputNumberProps& value(std::nullopt_t) {
        return this->value(InputNumberValue{});
    }

    InputNumberProps& defaultValue(Decimal value) {
        string_default_ = InputNumberStringValue{std::move(value)};
        string_mode_ = true;
        return *this;
    }

    InputNumberProps& defaultValue(InputNumberStringValue value) {
        string_default_ = std::move(value);
        string_mode_ = true;
        return *this;
    }

    InputNumberProps& defaultValue(std::nullopt_t) {
        return defaultValue(InputNumberValue{});
    }

    InputNumberProps& min(Prop<Decimal> value) {
        string_min_ = std::move(value);
        return *this;
    }

    InputNumberProps& max(Prop<Decimal> value) {
        string_max_ = std::move(value);
        return *this;
    }

    InputNumberProps& step(Prop<Decimal> value) {
        string_step_ = std::move(value);
        return *this;
    }

    InputNumberProps& changeOnWheel(Prop<bool> value) {
        wheel_ = std::move(value);
        return *this;
    }

    InputNumberProps& changeOnBlur(Prop<bool> value) {
        change_on_blur_ = std::move(value);
        return *this;
    }

    InputNumberProps& formatter(std::function<String(String, InputNumberFormatInfo)> value) {
        formatter_ = std::move(value);
        return *this;
    }

    InputNumberProps& parser(std::function<String(String)> value) {
        parser_ = std::move(value);
        return *this;
    }

    InputNumberProps& onInput(std::function<void(String)> value) {
        on_input_ = std::move(value);
        return *this;
    }

    InputNumberProps& onStringChange(std::function<void(InputNumberStringValue)> value) {
        string_change_ = std::move(value);
        return *this;
    }

    InputNumberProps& onStringSubmit(std::function<void(InputNumberStringValue)> value) {
        string_submit_ = std::move(value);
        return *this;
    }

    InputNumberProps& onStringStep(std::function<void(Decimal, InputNumberStringStepInfo)> value) {
        string_step_callback_ = std::move(value);
        return *this;
    }

    InputNumberProps& value(Prop<InputNumberValue> value) {
        value_ = std::move(value);
        return *this;
    }

    InputNumberProps& value(double value) {
        return this->value(InputNumberValue{value});
    }

    InputNumberProps& value(InputNumberValue value) {
        return this->value(Prop<InputNumberValue>{value});
    }

    InputNumberProps& defaultValue(InputNumberValue value) {
        default_value_ = value;
        return *this;
    }

    InputNumberProps& min(Prop<double> value) {
        min_ = std::move(value);
        explicit_min_ = true;
        return *this;
    }

    InputNumberProps& max(Prop<double> value) {
        max_ = std::move(value);
        explicit_max_ = true;
        return *this;
    }

    InputNumberProps& step(Prop<double> value) {
        step_ = std::move(value);
        explicit_step_ = true;
        return *this;
    }

    InputNumberProps& precision(Prop<std::optional<unsigned>> value) {
        precision_ = std::move(value);
        return *this;
    }

    InputNumberProps& precision(unsigned value) {
        return precision(std::optional<unsigned>{value});
    }

    InputNumberProps& precision(std::optional<unsigned> value) {
        return precision(Prop<std::optional<unsigned>>{value});
    }

    InputNumberProps& controls(Prop<bool> value) {
        controls_ = std::move(value);
        return *this;
    }

    InputNumberProps& keyboard(Prop<bool> value) {
        keyboard_ = std::move(value);
        return *this;
    }

    InputNumberProps& direction(Prop<TextDirection> value) {
        input_.direction(std::move(value));
        return *this;
    }

    InputNumberProps& placeholder(Prop<String> value) {
        input_.placeholder(std::move(value));
        return *this;
    }

    template <std::size_t N> InputNumberProps& placeholder(const char8_t (&value)[N]) {
        return placeholder(String{value});
    }

    InputNumberProps& size(Prop<ControlSize> value) {
        input_.size(std::move(value));
        return *this;
    }

    InputNumberProps& variant(Prop<InputVariant> value) {
        input_.variant(std::move(value));
        return *this;
    }

    InputNumberProps& status(Prop<InputStatus> value) {
        input_.status(std::move(value));
        return *this;
    }

    InputNumberProps& disabled(Prop<bool> value) {
        input_.disabled(std::move(value));
        return *this;
    }

    InputNumberProps& readOnly(Prop<bool> value) {
        input_.readOnly(std::move(value));
        return *this;
    }

    InputNumberProps& ref(const InputRef& value) {
        input_.ref(value);
        return *this;
    }

    InputNumberProps& autoFocus(bool value = true) {
        input_.autoFocus(value);
        return *this;
    }

    InputNumberProps& onChange(std::function<void(InputNumberValue)> value) {
        on_change_ = std::move(value);
        return *this;
    }

    InputNumberProps& onStep(std::function<void(double, InputNumberStepInfo)> value) {
        on_step_ = std::move(value);
        explicit_step_ = true;
        return *this;
    }

    InputNumberProps& onSubmit(std::function<void(InputNumberValue)> value) {
        on_submit_ = std::move(value);
        return *this;
    }

    InputNumberProps& onFocus(std::function<void()> value) {
        input_.onFocus(std::move(value));
        return *this;
    }

    InputNumberProps& onBlur(std::function<void()> value) {
        input_.onBlur(std::move(value));
        return *this;
    }

    InputNumberProps& layout(LayoutStyle value) {
        input_.layout(std::move(value));
        return *this;
    }

private:
    friend struct detail::FormAutoControlAccess;
    friend struct detail::InputNumberPropsAccess;
    Prop<InputNumberMode> mode_{InputNumberMode::Input};
    std::optional<Prop<String>> separator_;
    InputNumberControls icons_;
    bool explicit_min_{};
    bool explicit_max_{};
    bool explicit_step_{};
    bool string_mode_{};
    std::optional<Prop<InputNumberStringValue>> string_value_;
    std::optional<InputNumberStringValue> string_default_;
    std::optional<Prop<Decimal>> string_min_;
    std::optional<Prop<Decimal>> string_max_;
    std::optional<Prop<Decimal>> string_step_;
    Prop<bool> wheel_{false};
    Prop<bool> change_on_blur_{true};
    std::function<String(String, InputNumberFormatInfo)> formatter_;
    std::function<String(String)> parser_;
    std::function<void(String)> on_input_;
    std::function<void(InputNumberStringValue)> string_change_;
    std::function<void(InputNumberStringValue)> string_submit_;
    std::function<void(Decimal, InputNumberStringStepInfo)> string_step_callback_;
    InputProps input_;
    std::optional<Prop<InputNumberValue>> value_;
    std::optional<InputNumberValue> default_value_;
    Prop<double> min_{-9007199254740991.0};
    Prop<double> max_{9007199254740991.0};
    Prop<double> step_{1.0};
    Prop<std::optional<unsigned>> precision_{std::optional<unsigned>{}};
    Prop<bool> controls_{true};
    Prop<bool> keyboard_{true};
    std::function<void(InputNumberValue)> on_change_;
    std::function<void(double, InputNumberStepInfo)> on_step_;
    std::function<void(InputNumberValue)> on_submit_;
};

// InputNumber shares Input's typed affix slots and InputRef lifecycle.
void InputNumber(InputNumberProps props, std::optional<InputPrefix> prefix = {},
                 std::optional<InputSuffix> suffix = {});

} // namespace ryn
