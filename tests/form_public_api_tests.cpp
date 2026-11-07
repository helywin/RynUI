#include <ryn/rynui.hpp>

#include <type_traits>

using namespace ryn;
static_assert(FormFieldType<String> && FormFieldType<bool> && FormFieldType<InputNumberValue> &&
              FormFieldType<SelectSelection>);
static_assert(!FormFieldType<int> && !FormFieldType<double>);
static_assert(std::same_as<decltype(std::declval<FormField<String>>().value()), Prop<String>>);
static_assert(std::same_as<decltype(std::declval<FormField<bool>>().onChange()), std::function<void(bool)>>);
static_assert(!std::is_convertible_v<FormContent, FormItemContent>);
static_assert(std::same_as<decltype(FormRule{}.message), std::optional<String>>);

int main() {
    FormRef form;
    FormField<String> name{String{u8"initial"}};
    FormRule literal_rule;
    literal_rule.message = String{u8"Invalid field value"};
    literal_rule.message = u8"Invalid field value";
    const String literal_message = literal_rule.message.value_or(String{});
    static_cast<void>(literal_message);
    name.required().validator([](String value) { return value.empty() ? String{u8"empty"} : String{}; });
    FormField<InputNumberValue> count{2.0};
    FormField<bool> agree{false};
    FormField<SelectSelection> role{SelectValue{u8"native"}};
    Signal<String> external{String{u8"controlled"}};
    FormField<String> controlled{String{}, external, [=](String value) { external.set(std::move(value)); }};
    auto props =
        FormProps{}.ref(form).disabled(false).readOnly(false).focusFirstInvalid(true).onFinish([](FormResult) {});
    auto item = FormItemProps{}.field(String{u8"name"}, name).label(String{u8"Name"}).help(String{u8"Help"});
    auto input = InputProps{}
                     .value(name.value())
                     .onChange(name.onChange())
                     .status(name.status())
                     .disabled(name.disabled())
                     .readOnly(name.readOnly());
    auto select = SelectProps{}.value(role.value()).onChange(role.onChange()).disabled(role.uneditable());
    auto checkbox = CheckboxProps{}.checked(agree.value()).onChange(agree.onChange()).disabled(agree.uneditable());
    auto number = InputNumberProps{}.value(count.value()).onChange(count.onChange()).status(count.status());
    controlled.set(String{u8"updated"});
    return form.bound() || form.submit() || form.reset() || form.validate().has_value() ||
           controlled.get() != String{u8"updated"};
}
