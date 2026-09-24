#include <ryn/checkbox.hpp>
#include <ryn/control_size.hpp>
#include <ryn/switch.hpp>

#include <concepts>
#include <functional>

template<class T> concept SwitchSizeArgument = requires(T value) { ryn::SwitchProps{}.size(value); };
template<class T> concept HasSize = requires(T value) { value.size(ryn::SwitchSize::Small); };
template<class T> concept HasVisualColor = requires(T value) { value.color(0); };
static_assert(SwitchSizeArgument<ryn::SwitchSize>);
static_assert(!SwitchSizeArgument<ryn::ControlSize>);
static_assert(!HasSize<ryn::CheckboxProps>);
static_assert(!HasVisualColor<ryn::SwitchProps>);
static_assert(!HasVisualColor<ryn::CheckboxProps>);
static_assert(!std::constructible_from<ryn::CheckboxLabel, ryn::Content>);

int main() {
    ryn::Signal<bool> checked{false};
    ryn::Signal<bool> busy{false};
    ryn::Signal<ryn::SwitchSize> size{ryn::SwitchSize::Small};
    auto declare = [&] {
        ryn::Switch(ryn::SwitchProps{}.checked(checked).disabled(false).loading(busy)
            .size(size).onChange([](bool) {}).layout(ryn::LayoutStyle{}.width(ryn::dp(42))));
        ryn::Switch(ryn::SwitchProps{}.defaultChecked(true));
        ryn::Checkbox(ryn::CheckboxProps{}.checked(checked).indeterminate(false)
            .disabled(false).onChange([](bool) {}), ryn::CheckboxLabel{[] {}});
        ryn::Checkbox(ryn::CheckboxProps{}.defaultChecked(true));
    };
    static_cast<void>(declare);
}
