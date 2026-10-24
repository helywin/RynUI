#include <ryn/time_picker.hpp>
#include <ryn/button.hpp>
#include <ryn/text.hpp>
#include <type_traits>

using namespace ryn;
static_assert(std::same_as<decltype(TimePickerValue{}.value()), TimeOfDay&&>);
static_assert(std::same_as<decltype(TimePickerProps{}
                                        .value(TimeOfDay{})
                                        .defaultValue({})
                                        .defaultPanelValue({})
                                        .precision(TimePickerPrecision::Minute)
                                        .rules(TimePickerRules{})
                                        .open(false)
                                        .defaultOpen()
                                        .needConfirm(false)
                                        .allowClear()
                                        .disabled(false)
                                        .inputReadOnly()
                                        .size(ControlSize::Small)
                                        .variant(InputVariant::Underlined)
                                        .status(InputStatus::Error)
                                        .placeholder(u8"Time")
                                        .ref(TimePickerRef{})
                                        .autoFocus()
                                        .onChange([](TimePickerValue) {})
                                        .onOpenChange([](bool) {})
                                        .layout(LayoutStyle{}.width(dp(200)))),
                           TimePickerProps&>);
static_assert(!std::constructible_from<TimePickerFooter, InputSuffix>);
static_assert(TimeOfDay{23, 59, 59}.second() == 59);

// Kept in sync with docs/time-picker.md; this compiles and links the real API.
void time_picker_example(Signal<TimePickerValue> value, TimePickerRef reference) {
    TimePickerRules rules;
    rules.minute_step = 15;
    rules.min = TimeOfDay{8};
    rules.max = TimeOfDay{18};
    TimePicker(TimePickerProps{}
                   .value(value)
                   .ref(reference)
                   .precision(TimePickerPrecision::Minute)
                   .rules(rules)
                   .onChange([value](TimePickerValue next) mutable { value.set(next); })
                   .layout(LayoutStyle{}.width(dp(240))),
               TimePickerFooter{[] { Text(u8"Business hours: 08:00–18:00"); }});
}

int main() {
    Signal<TimePickerValue> value{TimeOfDay{9, 30}};
    Signal<bool> open{false};
    auto props = TimePickerProps{}.value(value).open(open);
    return std::is_copy_constructible_v<decltype(props)> ? 0 : 1;
}
