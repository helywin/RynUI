#include <ryn/time_picker.hpp>
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

int main() {
    Signal<TimePickerValue> value{TimeOfDay{9, 30}};
    Signal<bool> open{false};
    auto props = TimePickerProps{}.value(value).open(open);
    return std::is_copy_constructible_v<decltype(props)> ? 0 : 1;
}
