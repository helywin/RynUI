#include "component/input_number_value.hpp"
#include <stdexcept>

namespace ryn::detail {
namespace {
ExactDecimal exact(const Decimal& v) {
    return *ExactDecimal::parse(v.text().bytes());
}

InputNumberValue numeric(const NumberValue& value) {
    if (const auto* n = std::get_if<InputNumberValue>(&value)) {
        return *n;
    }
    return {};
}
} // namespace

NumberModel::NumberModel(NumberOptions o, NumberValue v, bool controlled)
    : numeric_(o.exact ? InputNumberOptions{} : o.numeric, numeric(v), controlled), controlled_(controlled) {
    configure(o);
    reconcile(std::move(v));
}

void NumberModel::configure(NumberOptions o) {
    if (!o.exact) {
        numeric_.configure(o.numeric);
        options_ = std::move(o);
        return;
    }
    auto lo = o.min ? std::optional{exact(*o.min)} : std::nullopt;
    auto hi = o.max ? std::optional{exact(*o.max)} : std::nullopt;
    auto step = exact(o.step);
    if (step.compare(ExactDecimal{}) <= 0 || (o.numeric.precision && *o.numeric.precision > 16384)) {
        throw std::invalid_argument("Invalid exact InputNumber step or precision");
    }
    if (o.numeric.precision) {
        if (lo) {
            lo = lo->rounded(*o.numeric.precision, 1);
        }
        if (hi) {
            hi = hi->rounded(*o.numeric.precision, -1);
        }
    }
    if (lo && hi && lo->compare(*hi) > 0) {
        throw std::invalid_argument("Invalid exact InputNumber range");
    }
    options_ = std::move(o);
    min_ = lo;
    max_ = hi;
    step_ = step;
}

void NumberModel::reconcile(NumberValue v) {
    if (options_.exact) {
        const auto* p = std::get_if<InputNumberStringValue>(&v);
        if (!p) {
            throw std::invalid_argument("stringMode requires typed Decimal values");
        }
        exact_ = *p;
    } else {
        numeric_.reconcile(std::get<InputNumberValue>(v));
    }
}

void NumberModel::accept(NumberValue v) {
    if (!controlled_) {
        reconcile(std::move(v));
    }
}

NumberValue NumberModel::value() const {
    return options_.exact ? NumberValue{exact_} : NumberValue{numeric_.value()};
}

String NumberModel::format(const NumberValue& value) {
    if (const auto* p = std::get_if<InputNumberValue>(&value)) {
        return InputNumberModel::format(*p);
    }
    const auto& p = std::get<InputNumberStringValue>(value);
    return p ? p->text() : String{};
}

std::optional<NumberValue> NumberModel::parse(std::string_view draft, bool finish) const {
    if (!options_.exact) {
        const auto p = numeric_.parse(draft, finish);
        return p ? std::optional<NumberValue>{*p} : std::nullopt;
    }
    if (draft.empty()) {
        return NumberValue{InputNumberStringValue{}};
    }
    auto p = ExactDecimal::parse(draft, finish);
    if (!p) {
        return {};
    }
    if (!finish && ((min_ && p->compare(*min_) < 0) || (max_ && p->compare(*max_) > 0))) {
        return {};
    }
    if (options_.numeric.precision) {
        p = p->rounded(*options_.numeric.precision);
    }
    if (min_ && p->compare(*min_) < 0) {
        p = min_;
    }
    if (max_ && p->compare(*max_) > 0) {
        p = max_;
    }
    return NumberValue{InputNumberStringValue{Decimal{p->text()}}};
}

NumberValue NumberModel::stepped(InputNumberStepDirection direction) const {
    if (!options_.exact) {
        return numeric_.stepped(direction);
    }
    const auto current = exact_ ? exact(*exact_) : ExactDecimal{};
    auto offset = step_;
    offset.negative = direction == InputNumberStepDirection::Down;
    ExactDecimal next;
    try {
        next = current.add(offset);
    } catch (const std::overflow_error&) {
        return exact_;
    }
    const auto raw = next;
    if (options_.numeric.precision) {
        next = next.rounded(*options_.numeric.precision);
        if ((direction == InputNumberStepDirection::Up && next.compare(current) < 0) ||
            (direction == InputNumberStepDirection::Down && next.compare(current) > 0)) {
            next = raw.rounded(*options_.numeric.precision, direction == InputNumberStepDirection::Up ? 1 : -1);
        }
    }
    if (min_ && next.compare(*min_) < 0) {
        next = *min_;
    }
    if (max_ && next.compare(*max_) > 0) {
        next = *max_;
    }
    if (exact_ && ((direction == InputNumberStepDirection::Up && next.compare(current) < 0) ||
                   (direction == InputNumberStepDirection::Down && next.compare(current) > 0))) {
        return exact_;
    }
    return InputNumberStringValue{Decimal{next.text()}};
}
} // namespace ryn::detail
