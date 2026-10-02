#pragma once

#include <ryn/layout_style.hpp>
#include <ryn/prop.hpp>
#include <ryn/string.hpp>
#include <ryn/tooltip.hpp>
#include <functional>
#include <optional>
#include <utility>
#include <vector>

namespace ryn {
namespace detail {
struct SliderPropsAccess;
}

enum class SliderOrientation { Horizontal, Vertical };

struct SliderLimits final {
    double minimum{0.0};
    double maximum{100.0};
    double step{1.0};
    friend constexpr bool operator==(SliderLimits, SliderLimits) = default;
};

struct SliderRange final {
    double lower{};
    double upper{};
    friend constexpr bool operator==(SliderRange, SliderRange) = default;
};

struct SliderMark final {
    double value{};
    String label;
    friend bool operator==(const SliderMark&, const SliderMark&) = default;
};

using SliderMarks = std::vector<SliderMark>;

enum class SliderHintMode { Auto, Always, Hidden };

struct SliderHintOptions final {
    SliderHintMode mode{SliderHintMode::Auto};
    TooltipPlacement placement{TooltipPlacement::Top};
    friend bool operator==(const SliderHintOptions&, const SliderHintOptions&) = default;
};

template <class Value, class Derived> class SliderPropsBase {
public:
    Derived& value(Prop<Value> value) {
        value_ = std::move(value);
        return self();
    }

    Derived& defaultValue(Value value) {
        default_value_ = value;
        return self();
    }

    Derived& limits(Prop<SliderLimits> value) {
        limits_ = std::move(value);
        return self();
    }

    Derived& disabled(Prop<bool> value) {
        disabled_ = std::move(value);
        return self();
    }

    Derived& keyboard(Prop<bool> value) {
        keyboard_ = std::move(value);
        return self();
    }

    Derived& orientation(Prop<SliderOrientation> value) {
        orientation_ = std::move(value);
        return self();
    }

    Derived& reverse(Prop<bool> value) {
        reverse_ = std::move(value);
        return self();
    }

    Derived& marks(Prop<SliderMarks> value) {
        marks_ = std::move(value);
        return self();
    }

    Derived& marksOnly(Prop<bool> value) {
        marks_only_ = std::move(value);
        return self();
    }

    Derived& dots(Prop<bool> value) {
        dots_ = std::move(value);
        return self();
    }

    Derived& included(Prop<bool> value) {
        included_ = std::move(value);
        return self();
    }

    Derived& hint(Prop<SliderHintOptions> value) {
        hint_ = std::move(value);
        return self();
    }

    Derived& hintFormatter(std::function<String(double)> formatter) {
        hint_formatter_ = std::move(formatter);
        return self();
    }

    Derived& onChange(std::function<void(Value)> callback) {
        on_change_ = std::move(callback);
        return self();
    }

    Derived& onChangeComplete(std::function<void(Value)> callback) {
        on_complete_ = std::move(callback);
        return self();
    }

    Derived& layout(LayoutStyle value) {
        layout_ = std::move(value);
        return self();
    }

private:
    friend struct detail::SliderPropsAccess;

    Derived& self() {
        return static_cast<Derived&>(*this);
    }

    std::optional<Prop<Value>> value_;
    std::optional<Value> default_value_;
    Prop<SliderLimits> limits_{SliderLimits{}};
    Prop<bool> disabled_{false};
    Prop<bool> keyboard_{true};
    Prop<bool> reverse_{false};
    Prop<SliderOrientation> orientation_{SliderOrientation::Horizontal};
    Prop<SliderMarks> marks_{SliderMarks{}};
    Prop<bool> marks_only_{false};
    Prop<bool> dots_{false};
    Prop<bool> included_{true};
    Prop<SliderHintOptions> hint_{SliderHintOptions{}};
    std::function<String(double)> hint_formatter_;
    std::function<void(Value)> on_change_;
    std::function<void(Value)> on_complete_;
    LayoutStyle layout_;
};

class SliderProps final : public SliderPropsBase<double, SliderProps> {};

class RangeSliderProps final : public SliderPropsBase<SliderRange, RangeSliderProps> {};

void Slider(SliderProps props);
void RangeSlider(RangeSliderProps props);

} // namespace ryn
