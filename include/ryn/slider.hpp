#pragma once

#include <ryn/layout_style.hpp>
#include <ryn/prop.hpp>
#include <functional>
#include <optional>
#include <utility>

namespace ryn {
namespace detail { struct SliderPropsAccess; }

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

template<class Value, class Derived>
class SliderPropsBase {
public:
    Derived& value(Prop<Value> value) { value_ = std::move(value); return self(); }
    Derived& defaultValue(Value value) { default_value_ = value; return self(); }
    Derived& limits(Prop<SliderLimits> value) { limits_ = std::move(value); return self(); }
    Derived& disabled(Prop<bool> value) { disabled_ = std::move(value); return self(); }
    Derived& keyboard(Prop<bool> value) { keyboard_ = std::move(value); return self(); }
    Derived& orientation(Prop<SliderOrientation> value) { orientation_ = std::move(value); return self(); }
    Derived& reverse(Prop<bool> value) { reverse_ = std::move(value); return self(); }
    Derived& onChange(std::function<void(Value)> callback) { on_change_ = std::move(callback); return self(); }
    Derived& onChangeComplete(std::function<void(Value)> callback) { on_complete_ = std::move(callback); return self(); }
    Derived& layout(LayoutStyle value) { layout_ = std::move(value); return self(); }

private:
    friend struct detail::SliderPropsAccess;
    Derived& self() { return static_cast<Derived&>(*this); }
    std::optional<Prop<Value>> value_;
    std::optional<Value> default_value_;
    Prop<SliderLimits> limits_{SliderLimits{}};
    Prop<bool> disabled_{false}, keyboard_{true}, reverse_{false};
    Prop<SliderOrientation> orientation_{SliderOrientation::Horizontal};
    std::function<void(Value)> on_change_, on_complete_;
    LayoutStyle layout_;
};

class SliderProps final : public SliderPropsBase<double, SliderProps> {};
class RangeSliderProps final : public SliderPropsBase<SliderRange, RangeSliderProps> {};

void Slider(SliderProps props);
void RangeSlider(RangeSliderProps props);

} // namespace ryn
