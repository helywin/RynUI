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
    auto literal_value = ryn::SelectProps{}.value(u8"one").notFoundContent(u8"No results");
    auto number_value = ryn::SelectProps{}.value(42);
    auto empty_value = ryn::SelectProps{}.value(std::nullopt);
    ryn::Signal<ryn::SelectValues> multi{ryn::SelectValues{ryn::SelectValue{42}}};
    auto complete = ryn::SelectProps{}
                        .mode(ryn::SelectMode::Tags)
                        .values(multi)
                        .onValuesChange([](ryn::SelectValues) {})
                        .groups(std::vector<ryn::SelectOptionGroup>{{u8"group", u8"Group", {{42, u8"Answer"}}}})
                        .tokenSeparators({u8",", u8"；"})
                        .maxTagCount(std::size_t{2})
                        .responsiveMaxTagCount()
                        .maxTagTextLength(std::size_t{8})
                        .maxTagPlaceholder([](const ryn::SelectValues&) { return ryn::String{u8"More"}; })
                        .maxCount(std::size_t{4})
                        .filterOption(ryn::SelectFilter{false})
                        .optionFilterProp(ryn::SelectOptionField::Value)
                        .optionLabelProp(ryn::SelectOptionField::Label)
                        .filterSort(ryn::SelectSort{[](const auto&, const auto&, ryn::StringView) { return false; }})
                        .optionRender(ryn::SelectOptionRenderer{[](ryn::SelectRenderInfo) {}})
                        .tagRender(ryn::SelectTagRenderer{[](ryn::SelectRenderInfo) {}})
                        .labelRender(ryn::SelectLabelRenderer{[](ryn::SelectRenderInfo) {}})
                        .popupRender(ryn::SelectPopupRenderer{[](std::function<void()> origin) { origin(); }})
                        .source(ryn::SelectSource{})
                        .searchValue(ryn::String{})
                        .autoClearSearchValue(false)
                        .hideSelected()
                        .virtualList()
                        .direction(ryn::TextDirection::RightToLeft)
                        .motion()
                        .placement(ryn::SelectPlacement::TopRight)
                        .popupMatchSelectWidth(false)
                        .popupWidth(320.0F)
                        .onInputKeyDown([](ryn::SelectKeyEvent) { return false; });
    auto labeled = ryn::SelectProps{}
                       .labeledValue(ryn::SelectLabeledSelection{{42, u8"Custom"}})
                       .onLabeledChange([](ryn::SelectLabeledValues) {});
    static_assert(!std::is_convertible_v<ryn::SelectTagRenderer, ryn::SelectOptionRenderer>);
    static_cast<void>(complete);
    static_cast<void>(labeled);
    static_cast<void>(props);
    static_cast<void>(defaults);
    static_cast<void>(string_value);
    static_cast<void>(literal_value);
    static_cast<void>(number_value);
    static_cast<void>(empty_value);
    return reference.bound() ? 1 : 0;
}
