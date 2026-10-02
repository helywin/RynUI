#include <ryn/switch.hpp>

#include <concepts>

template <class T>
concept Direction = requires(T value) { ryn::SwitchProps{}.direction(value); };

static_assert(Direction<ryn::SwitchDirection>);
static_assert(Direction<ryn::Signal<ryn::SwitchDirection>>);
static_assert(!Direction<int>);
static_assert(!std::constructible_from<ryn::SwitchCheckedContent, ryn::Content>);
static_assert(!std::constructible_from<ryn::SwitchUncheckedContent, ryn::SwitchCheckedContent>);

int main() {
    ryn::SwitchRef reference;
    auto declaration = [&] {
        ryn::Switch(ryn::SwitchProps{}.ref(reference).autoFocus(true).onClick([](bool) {}),
                    ryn::SwitchSlots{ryn::SwitchCheckedContent{[] {}}, ryn::SwitchUncheckedContent{[] {}}});
        ryn::Switch(ryn::SwitchProps{});
    };
    static_cast<void>(declaration);
    return reference.bound() || reference.focus() || reference.blur() ? 1 : 0;
}
