#include <ryn/input.hpp>
#include <concepts>
#include <stdexcept>
#include <string>
template<class T> concept NarrowValue = requires(T value) { ryn::InputProps{}.value(value); };
template<class T> concept NarrowCallback = requires(T callback) { ryn::InputProps{}.onChange(callback); };
static_assert(!NarrowValue<const char*>);
static_assert(!NarrowValue<std::string>);
static_assert(!NarrowCallback<std::function<void(std::string)>>);
static_assert(!std::constructible_from<ryn::InputPrefix, ryn::InputSuffix>);
static_assert(!std::constructible_from<ryn::InputPrefix, ryn::Content>);
int main() {
    ryn::Signal<ryn::String> value{ryn::String{u8"值"}};
    ryn::Signal<std::size_t> limit{100};
    ryn::Signal<ryn::ControlSize> size{ryn::ControlSize::Small};
    ryn::Signal<ryn::InputStatus> status{ryn::InputStatus::Warning};
    ryn::Signal<bool> disabled{false};
    auto declare = [&] {
        ryn::Input(ryn::InputProps{}.value(value).placeholder(u8"请输入").size(size)
            .status(status).disabled(disabled).readOnly(false).maxLength(limit)
            .onChange([](ryn::String) {}).onSubmit([](ryn::String) {})
            .layout(ryn::LayoutStyle{}.width(ryn::dp(160))),
            ryn::InputPrefix{[] {}}, ryn::InputSuffix{[] {}});
        ryn::Input(ryn::InputProps{}.defaultValue(u8"初始值"));
    };
    static_cast<void>(declare);
    try { ryn::Input(ryn::InputProps{}); }
    catch(const std::logic_error&) { return 0; }
    return 1;
}
