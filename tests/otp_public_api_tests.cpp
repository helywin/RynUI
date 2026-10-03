#include <ryn/otp.hpp>
#include <type_traits>
#include <utility>

using namespace ryn;
static_assert(std::same_as<decltype(std::declval<OTPProps&>().length(6)), OTPProps&>);
static_assert(std::same_as<decltype(std::declval<OTPProps&>().value(u8"123456")), OTPProps&>);
static_assert(std::same_as<decltype(std::declval<OTPProps&>().mask(true)), OTPProps&>);
static_assert(std::same_as<decltype(std::declval<OTPProps&>().mask(u8"*")), OTPProps&>);
static_assert(std::same_as<decltype(std::declval<const OTPRef&>().focus()), bool>);
static_assert(std::copy_constructible<OTPRef> && std::copy_constructible<OTPMask>);

int main() {
    OTPRef reference;
    if (reference.bound() || reference.focus() || reference.blur()) {
        return 1;
    }
    Signal<std::size_t> length{6};
    Signal<OTPMask> mask{OTPMask{}};
    Signal<OTPDirection> direction{OTPDirection::RightToLeft};
    const auto props = OTPProps{}
                           .length(length)
                           .mask(mask)
                           .direction(direction)
                           .size(ControlSize::Small)
                           .variant(InputVariant::Filled)
                           .status(InputStatus::Error)
                           .disabled(false)
                           .readOnly(true)
                           .purpose(InputPurpose::Username)
                           .capitalization(InputCapitalization::Letters)
                           .autocorrect(false)
                           .defaultValue(u8"abcd")
                           .formatter([](String value) { return value; })
                           .onInput([](const std::vector<String>&) {})
                           .onChange([](String) {})
                           .onFocus([](std::size_t) {})
                           .onBlur([](std::size_t) {})
                           .autoFocus()
                           .ref(reference)
                           .layout(LayoutStyle{}.width(dp(200)));
    static_cast<void>(props);
}
