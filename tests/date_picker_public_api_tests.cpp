#include <ryn/date_picker.hpp>
#include <ryn/rynui.hpp>
#include <type_traits>
static_assert(std::is_same_v<ryn::DatePickerValue, std::optional<ryn::CivilDate>>);
static_assert(requires(ryn::DatePickerProps props, ryn::Signal<ryn::DatePickerValue> value, ryn::Signal<bool> flag,
                       ryn::DatePickerRef ref) {
    props.value(value).defaultValue({}).defaultPanelDate({2024, 2, 29}).minDate(value).maxDate(value);
    props.disabledDate(ryn::DisabledDate{[](ryn::CivilDate) { return false; }});
    props.open(flag).defaultOpen().allowClear(flag).disabled(flag).inputReadOnly(flag);
    props.placeholder(u8"Date")
        .size(ryn::ControlSize::Large)
        .variant(ryn::InputVariant::Filled)
        .status(ryn::InputStatus::Warning);
    props.ref(ref).autoFocus().onChange([](ryn::DatePickerValue) {}).onOpenChange([](bool) {});
    props.onFocus([] {}).onBlur([] {}).layout(ryn::LayoutStyle{}.width(ryn::dp(180)));
});

void documented_date_field(ryn::Signal<ryn::DatePickerValue>& selected) {
    using namespace ryn;
    DatePicker(DatePickerProps{}
                   .value(selected)
                   .defaultPanelDate(CivilDate{2026, 10, 1})
                   .minDate(CivilDate{2020, 1, 1})
                   .maxDate(CivilDate{2030, 12, 31})
                   .onChange([&selected](DatePickerValue date) { selected.set(date); }),
               DatePickerFooter{[] { Text(u8"Choose a date"); }});
}

int main() {
    ryn::DatePickerProps{}.value(ryn::CivilDate{2024, 2, 29}).value(std::nullopt);
    ryn::DatePickerFooter footer{[] {}};
}
