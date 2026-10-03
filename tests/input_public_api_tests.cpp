#include <ryn/input.hpp>
#include <ryn/password.hpp>
#include <ryn/search.hpp>
#include <ryn/text_area.hpp>
#include <ryn/text.hpp>
#include <ryn/typography.hpp>
#include <ryn/text_direction.hpp>
#include <concepts>
#include <stdexcept>
#include <string>
template <class T>
concept NarrowValue = requires(T value) { ryn::InputProps{}.value(value); };
template <class T>
concept NarrowCallback = requires(T callback) { ryn::InputProps{}.onChange(callback); };
static_assert(!NarrowValue<const char*>);
static_assert(std::same_as<decltype(ryn::InputProps{}.direction(ryn::TextDirection::RightToLeft)), ryn::InputProps&>);
static_assert(std::same_as<decltype(ryn::PasswordProps{}.direction(ryn::TextDirection::Auto)), ryn::PasswordProps&>);
static_assert(std::same_as<decltype(ryn::SearchProps{}.direction(ryn::TextDirection::LeftToRight)), ryn::SearchProps&>);
static_assert(std::same_as<decltype(ryn::TextAreaProps{}.direction(ryn::TextDirection::Auto)), ryn::TextAreaProps&>);
static_assert(std::same_as<decltype(ryn::TextProps{}.direction(ryn::TextDirection::Auto)), ryn::TextProps&>);
static_assert(
    std::same_as<decltype(ryn::TypographyProps{}.direction(ryn::TextDirection::Auto)), ryn::TypographyProps&>);
static_assert(std::same_as<decltype(ryn::TitleProps{}.direction(ryn::TextDirection::Auto)), ryn::TitleProps&>);
static_assert(!NarrowValue<std::string>);
static_assert(!NarrowCallback<std::function<void(std::string)>>);
static_assert(!std::constructible_from<ryn::InputPrefix, ryn::InputSuffix>);
static_assert(!std::constructible_from<ryn::InputPrefix, ryn::Content>);
static_assert(std::same_as<decltype(ryn::PasswordProps{}.ref(std::declval<ryn::InputRef>()).disabled(true)),
                           ryn::PasswordProps&>);
static_assert(
    std::same_as<decltype(ryn::SearchProps{}.purpose(ryn::InputPurpose::Email).loading(true)), ryn::SearchProps&>);

int main() {
    ryn::Signal<ryn::String> value{ryn::String{u8"值"}};
    ryn::Signal<std::size_t> limit{100};
    ryn::Signal<ryn::ControlSize> size{ryn::ControlSize::Small};
    ryn::Signal<ryn::InputStatus> status{ryn::InputStatus::Warning};
    ryn::Signal<bool> disabled{false};
    ryn::InputRef reference;
    auto declare = [&] {
        ryn::Input(ryn::InputProps{}
                       .value(value)
                       .ref(reference)
                       .autoFocus()
                       .purpose(ryn::InputPurpose::Name)
                       .capitalization(ryn::InputCapitalization::Words)
                       .autocorrect(false)
                       .onFocus([] {})
                       .onBlur([] {})
                       .placeholder(u8"请输入")
                       .size(size)
                       .status(status)
                       .disabled(disabled)
                       .readOnly(false)
                       .allowClear(true)
                       .clearDisabled(disabled)
                       .clearIcon(ryn::IconSource{ryn::IconName::CloseOutlined})
                       .onClear([] {})
                       .showCount()
                       .count(ryn::InputCountOptions{20, ryn::InputCountUnit::Grapheme})
                       .countStrategy([](ryn::StringView text) { return text.size_bytes(); })
                       .countFormatter([](ryn::InputCountInfo) { return ryn::String{u8"count"}; })
                       .exceedFormatter([](ryn::String text, std::size_t) { return text; })
                       .maxLength(limit)
                       .onChange([](ryn::String) {})
                       .onSubmit([](ryn::String) {})
                       .layout(ryn::LayoutStyle{}.width(ryn::dp(160))),
                   ryn::InputPrefix{[] {}}, ryn::InputSuffix{[] {}});
        ryn::Input(ryn::InputProps{}.defaultValue(u8"初始值"));
    };
    static_cast<void>(declare);
    try {
        ryn::Input(ryn::InputProps{});
    } catch (const std::logic_error&) {
        return 0;
    }
    return 1;
}
