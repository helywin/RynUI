#pragma once

#include <ryn/input.hpp>

namespace ryn {

using InputNumberValue = std::optional<double>;

enum class InputNumberStepDirection { Up, Down };

enum class InputNumberStepSource { Button, Keyboard };

struct InputNumberStepInfo final {
    double offset{};
    InputNumberStepDirection direction{InputNumberStepDirection::Up};
    InputNumberStepSource source{InputNumberStepSource::Button};
};

namespace detail {
struct InputNumberPropsAccess;
}

class InputNumberProps final {
public:
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
        return *this;
    }

    InputNumberProps& max(Prop<double> value) {
        max_ = std::move(value);
        return *this;
    }

    InputNumberProps& step(Prop<double> value) {
        step_ = std::move(value);
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
    friend struct detail::InputNumberPropsAccess;
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
