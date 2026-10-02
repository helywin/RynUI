#include <ryn/checkbox.hpp>

#include <type_traits>

static_assert(std::variant_size_v<ryn::CheckboxValue> == 3);
static_assert(std::is_same_v<ryn::CheckboxValues, std::vector<ryn::CheckboxValue>>);
static_assert(!std::is_same_v<ryn::CheckboxLabel, ryn::CheckboxGroupContent>);

int main() {
    ryn::Signal<std::vector<ryn::CheckboxOption>> options{
        std::vector<ryn::CheckboxOption>{{ryn::String{u8"one"}, ryn::String{u8"选项"}, false}}};
    ryn::Signal<ryn::CheckboxValues> value{ryn::CheckboxValues{1.0, true, ryn::String{u8"one"}}};
    ryn::CheckboxGroupProps props;
    props.options(options).value(value).disabled(false).orientation(ryn::CheckboxGroupOrientation::Vertical);
    ryn::CheckboxProps child;
    ryn::CheckboxRef reference;
    child.value(ryn::String{u8"one"})
        .skipGroup(true)
        .defaultChecked(false)
        .ref(reference)
        .autoFocus(true)
        .direction(ryn::CheckboxDirection::RightToLeft)
        .wave(true)
        .onClick([](bool) {});
    props.direction(ryn::CheckboxDirection::RightToLeft);
    return 0;
}
