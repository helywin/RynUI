#include <ryn/input_number.hpp>

#include <type_traits>

static_assert(std::is_same_v<ryn::InputNumberValue, std::optional<double>>);
static_assert(requires(ryn::InputNumberProps props, ryn::Signal<ryn::InputNumberValue> value, ryn::Signal<double> bound,
                       ryn::Signal<bool> flag, const ryn::InputRef& reference) {
    props.value(value).min(bound).max(bound).step(bound).precision(2).controls(flag).keyboard(flag);
    props.defaultValue(1.5).placeholder(u8"Amount").size(ryn::ControlSize::Small);
    props.variant(ryn::InputVariant::Filled).status(ryn::InputStatus::Warning).disabled(flag).readOnly(flag);
    props.ref(reference).autoFocus().layout(ryn::LayoutStyle{}.width(ryn::dp(120)));
    props.onChange([](ryn::InputNumberValue) {}).onStep([](double, ryn::InputNumberStepInfo) {});
    props.onSubmit([](ryn::InputNumberValue) {}).onFocus([] {}).onBlur([] {});
});

int main() {
    ryn::InputNumberProps props;
    props.value(1.25).defaultValue(std::nullopt);
    props.value(std::nullopt).precision(std::nullopt);
}
