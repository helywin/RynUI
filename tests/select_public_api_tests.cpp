#include <ryn/select.hpp>

#include <type_traits>

static_assert(std::is_constructible_v<ryn::SelectValue, std::int64_t>);
static_assert(!std::is_constructible_v<ryn::SelectValue, bool>);
static_assert(!std::is_constructible_v<ryn::SelectValue, double>);
static_assert(!std::is_constructible_v<ryn::SelectValue, unsigned long long>);
static_assert(!std::is_convertible_v<ryn::SelectPrefix, ryn::SelectSuffix>);
static_assert(std::is_same_v<ryn::SelectRef, ryn::InputRef>);

int main() {
    ryn::Signal<ryn::SelectSelection> value{ryn::SelectSelection{42}};
    ryn::Signal<std::vector<ryn::SelectOption>> options{std::vector<ryn::SelectOption>{{42, u8"Answer"}}};
    ryn::Signal<bool> open{false};
    ryn::SelectRef reference;
    auto props = ryn::SelectProps{}
                     .value(value)
                     .options(options)
                     .showSearch()
                     .allowClear()
                     .placeholder(u8"Choose")
                     .disabled(false)
                     .loading(false)
                     .size(ryn::ControlSize::Small)
                     .variant(ryn::InputVariant::Filled)
                     .status(ryn::InputStatus::Warning)
                     .open(open)
                     .notFoundContent(ryn::String{u8"No results"})
                     .listHeight(160.0F)
                     .onChange([](ryn::SelectSelection) {})
                     .onSearch([](ryn::String) {})
                     .onOpenChange([](bool) {})
                     .onFocus([] {})
                     .onBlur([] {})
                     .ref(reference)
                     .autoFocus(false)
                     .layout(ryn::LayoutStyle{});
    auto defaults =
        ryn::SelectProps{}.defaultValue(42).defaultOpen(false).options({{u8"one", u8"One"}, {42, u8"Answer"}});
    auto string_value = ryn::SelectProps{}.value(ryn::SelectValue{u8"one"});
    static_cast<void>(props);
    static_cast<void>(defaults);
    static_cast<void>(string_value);
    return reference.bound() ? 1 : 0;
}
