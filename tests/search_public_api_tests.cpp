#include <ryn/button.hpp>
#include <ryn/search.hpp>

#include <concepts>
#include <functional>
#include <stdexcept>
#include <string>

template<class T> concept NarrowValue = requires(T value) { ryn::SearchProps{}.value(value); };
template<class T> concept NarrowCallback = requires(T callback) { ryn::SearchProps{}.onSearch(callback); };
template<class T> concept HasVisualColor = requires(T value) { value.color(0); };
static_assert(!NarrowValue<const char*>);
static_assert(!NarrowValue<std::string>);
static_assert(!NarrowCallback<std::function<void(std::string)>>);
static_assert(!HasVisualColor<ryn::SearchProps>);
static_assert(!std::constructible_from<ryn::SearchButtonContent, ryn::ButtonContent>);

int main() {
    ryn::Signal<ryn::String> value{ryn::String{u8"查询"}};
    ryn::Signal<bool> loading{false};
    ryn::Signal<ryn::ControlSize> size{ryn::ControlSize::Middle};
    auto declare = [&] {
        ryn::Search(ryn::SearchProps{}.value(value).placeholder(u8"搜索")
            .size(size).status(ryn::InputStatus::Default).disabled(false)
            .readOnly(false).loading(loading).enterButton(true).maxLength(32)
            .onChange([](ryn::String) {})
            .onSearch([](ryn::String, ryn::SearchSource) {})
            .layout(ryn::LayoutStyle{}.width(ryn::dp(240))),
            ryn::SearchButtonContent{[] {}});
    };
    static_cast<void>(declare);
    try {
        ryn::Search(ryn::SearchProps{}.value(u8"a").defaultValue(u8"b"));
        return 1;
    } catch (const std::invalid_argument&) {}
    try {
        ryn::Search(ryn::SearchProps{}.size(static_cast<ryn::ControlSize>(255)));
        return 2;
    } catch (const std::invalid_argument&) {}
    try {
        ryn::Search(ryn::SearchProps{}.status(static_cast<ryn::InputStatus>(255)));
        return 3;
    } catch (const std::invalid_argument&) {}
    try {
        ryn::Search(ryn::SearchProps{});
        return 4;
    } catch (const std::logic_error&) {}
    return 0;
}
