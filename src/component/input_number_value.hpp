#pragma once
#include "component/input_number_model.hpp"
#include "component/exact_decimal.hpp"
#include <variant>

namespace ryn::detail {
using NumberValue = std::variant<InputNumberValue, InputNumberStringValue>;

struct NumberOptions final {
    InputNumberOptions numeric;
    std::optional<Decimal> min;
    std::optional<Decimal> max;
    Decimal step{"1"};
    bool exact{};
    friend bool operator==(const NumberOptions&, const NumberOptions&) = default;
};

class NumberModel final {
public:
    NumberModel(NumberOptions, NumberValue, bool);
    void configure(NumberOptions);
    void reconcile(NumberValue);
    void accept(NumberValue);
    [[nodiscard]] NumberValue value() const;

    [[nodiscard]] const NumberOptions& options() const {
        return options_;
    }

    [[nodiscard]] std::optional<NumberValue> parse(std::string_view, bool finish = false) const;
    [[nodiscard]] NumberValue stepped(InputNumberStepDirection) const;
    [[nodiscard]] static String format(const NumberValue&);

private:
    NumberOptions options_;
    InputNumberModel numeric_;
    InputNumberStringValue exact_;
    std::optional<ExactDecimal> min_;
    std::optional<ExactDecimal> max_;
    ExactDecimal step_;
    bool controlled_{};
};
} // namespace ryn::detail
