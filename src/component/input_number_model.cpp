#include "component/input_number_model.hpp"

#include <algorithm>
#include <array>
#include <charconv>
#include <cmath>
#include <stdexcept>
#include <string>

namespace ryn::detail {
namespace {
using Decimal = InputNumberDecimal;
constexpr std::uint64_t base = 1000000000000ULL;
constexpr std::uint64_t max_whole = 9007199254740991ULL;

Decimal clean(Decimal value) {
    if (!value.whole && !value.fraction) {
        value.negative = false;
    }
    return value;
}

int compare(Decimal a, Decimal b) {
    if (a.negative != b.negative) {
        return a.negative ? -1 : 1;
    }
    const auto magnitude = a.whole != b.whole         ? (a.whole < b.whole ? -1 : 1)
                           : a.fraction != b.fraction ? (a.fraction < b.fraction ? -1 : 1)
                                                      : 0;
    return a.negative ? -magnitude : magnitude;
}

Decimal add(Decimal a, Decimal b) {
    if (a.negative == b.negative) {
        const auto fraction = a.fraction + b.fraction;
        return clean({a.whole + b.whole + fraction / base, fraction % base, a.negative});
    }
    const bool negative = a.negative;
    a.negative = false;
    b.negative = false;
    if (compare(a, b) < 0) {
        std::swap(a, b);
        a.negative = !negative;
    } else {
        a.negative = negative;
    }
    const auto borrow = a.fraction < b.fraction ? 1ULL : 0ULL;
    return clean({a.whole - b.whole - borrow, a.fraction + borrow * base - b.fraction, a.negative});
}

std::uint64_t quantum(unsigned precision) {
    auto result = base;
    for (unsigned i = 0; i < precision; ++i) {
        result /= 10;
    }
    return result;
}

// direction -1/+1 is floor/ceil, zero is half-away-from-zero.
Decimal round(Decimal value, unsigned precision, int direction = 0) {
    const auto unit = quantum(precision);
    const auto remainder = value.fraction % unit;
    value.fraction -= remainder;
    const bool increment = direction == 0  ? remainder >= unit / 2 && remainder != 0
                           : direction > 0 ? !value.negative && remainder != 0
                                           : value.negative && remainder != 0;
    if (increment) {
        value.fraction += unit;
        value.whole += value.fraction / base;
        value.fraction %= base;
    }
    return clean(value);
}

std::optional<Decimal> parse_decimal(std::string_view source, bool finish) {
    if (source.empty()) {
        return {};
    }
    Decimal result;
    if (source.front() == '-' || source.front() == '+') {
        result.negative = source.front() == '-';
        source.remove_prefix(1);
    }
    bool dot{};
    bool digit{};
    unsigned places{};
    std::uint64_t scale = base;
    for (const char ch : source) {
        if (ch == '.' && !dot) {
            dot = true;
            continue;
        }
        if (ch < '0' || ch > '9') {
            return {};
        }
        digit = true;
        const auto number = static_cast<std::uint64_t>(ch - '0');
        if (dot) {
            if (++places > 12) {
                // Extra trailing zeros carry no additional precision.
                if (number) {
                    return {};
                }
            } else {
                scale /= 10;
                result.fraction += number * scale;
            }
        } else {
            if (result.whole > (max_whole - number) / 10) {
                return {};
            }
            result.whole = result.whole * 10 + number;
        }
    }
    if (!digit || (!finish && source.back() == '.') || (result.whole == max_whole && result.fraction)) {
        return {};
    }
    return clean(result);
}

std::string text(Decimal value) {
    std::string result = value.negative ? "-" : "";
    result += std::to_string(value.whole);
    if (value.fraction) {
        auto fraction = std::to_string(base + value.fraction).substr(1);
        while (fraction.back() == '0') {
            fraction.pop_back();
        }
        result += '.';
        result += fraction;
    }
    return result;
}

Decimal decimal(double value) {
    if (!std::isfinite(value) || std::abs(value) > static_cast<double>(max_whole)) {
        throw std::invalid_argument("InputNumber requires a finite value in the safe integer range");
    }
    std::array<char, 350> buffer{};
    const auto converted = std::to_chars(buffer.data(), buffer.data() + buffer.size(), value, std::chars_format::fixed);
    const auto parsed =
        converted.ec == std::errc{} ? parse_decimal({buffer.data(), converted.ptr}, true) : std::optional<Decimal>{};
    if (!parsed) {
        throw std::invalid_argument("InputNumber supports at most 12 fractional decimal places");
    }
    return *parsed;
}

double number(Decimal value) {
    const auto source = text(value);
    double result{};
    const auto converted = std::from_chars(source.data(), source.data() + source.size(), result);
    if (converted.ec != std::errc{}) {
        throw std::overflow_error("InputNumber decimal conversion failed");
    }
    return result;
}
} // namespace

InputNumberModel::InputNumberModel(InputNumberOptions options, InputNumberValue value, bool controlled)
    : controlled_(controlled) {
    validate_value(value);
    configure(options);
    value_ = value;
}

void InputNumberModel::configure(InputNumberOptions options) {
    auto minimum = decimal(options.min);
    auto maximum = decimal(options.max);
    const auto step = decimal(options.step);
    if (options.step <= 0 || options.min > options.max || (options.precision && *options.precision > 12)) {
        throw std::invalid_argument("Invalid InputNumber range, step or precision");
    }
    if (options.precision) {
        minimum = round(minimum, *options.precision, 1);
        maximum = round(maximum, *options.precision, -1);
    }
    if (compare(minimum, maximum) > 0) {
        throw std::invalid_argument("InputNumber range contains no value at this precision");
    }
    options_ = options;
    min_ = minimum;
    max_ = maximum;
    step_ = step;
}

void InputNumberModel::validate_value(InputNumberValue value) {
    if (value) {
        static_cast<void>(decimal(*value));
    }
}

void InputNumberModel::reconcile(InputNumberValue value) {
    validate_value(value);
    value_ = value;
}

void InputNumberModel::accept(InputNumberValue value) {
    validate_value(value);
    if (!controlled_) {
        value_ = value;
    }
}

InputNumberValue InputNumberModel::value() const noexcept {
    return value_;
}

const InputNumberOptions& InputNumberModel::options() const noexcept {
    return options_;
}

std::optional<InputNumberValue> InputNumberModel::parse(std::string_view draft, bool finish) const {
    if (draft.empty()) {
        return InputNumberValue{};
    }
    auto parsed = parse_decimal(draft, finish);
    if (!parsed) {
        return {};
    }
    if (!finish && (compare(*parsed, min_) < 0 || compare(*parsed, max_) > 0)) {
        return {};
    }
    if (options_.precision) {
        *parsed = round(*parsed, *options_.precision);
    }
    *parsed = compare(*parsed, min_) < 0 ? min_ : compare(*parsed, max_) > 0 ? max_ : *parsed;
    return InputNumberValue{number(*parsed)};
}

InputNumberValue InputNumberModel::stepped(InputNumberStepDirection direction) const {
    const auto current = value_ ? decimal(*value_) : Decimal{};
    auto offset = step_;
    offset.negative = direction == InputNumberStepDirection::Down;
    auto next = add(current, offset);
    if (options_.precision) {
        const auto unrounded = next;
        next = round(next, *options_.precision);
        // Nearest rounding can cross an authoritative value that is not on the
        // current precision grid. Quantize along the requested direction then.
        if ((direction == InputNumberStepDirection::Up && compare(next, current) < 0) ||
            (direction == InputNumberStepDirection::Down && compare(next, current) > 0)) {
            next = round(unrounded, *options_.precision, direction == InputNumberStepDirection::Up ? 1 : -1);
        }
    }
    next = compare(next, min_) < 0 ? min_ : compare(next, max_) > 0 ? max_ : next;
    // Clamping an external value outside the effective range must not reverse
    // the direction either. A bound may leave no allowed step in that direction.
    if (value_ && ((direction == InputNumberStepDirection::Up && compare(next, current) < 0) ||
                   (direction == InputNumberStepDirection::Down && compare(next, current) > 0))) {
        return value_;
    }
    return number(next);
}

String InputNumberModel::format(InputNumberValue value) {
    return value ? String::from_utf8(text(decimal(*value))).value() : String{};
}

} // namespace ryn::detail
