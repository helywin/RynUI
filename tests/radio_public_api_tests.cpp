#include <ryn/radio.hpp>
#include <ryn/text.hpp>

#include <concepts>

template <class T>
concept HasVisualColor = requires(T value) { value.color(0); };
static_assert(!HasVisualColor<ryn::RadioProps>);
static_assert(!HasVisualColor<ryn::RadioGroupProps>);
static_assert(!std::constructible_from<ryn::RadioLabel, ryn::Content>);
static_assert(!std::constructible_from<ryn::RadioGroupContent, ryn::RadioLabel>);

int main() {
    ryn::Signal<bool> checked{false};
    ryn::Signal<std::optional<ryn::String>> selected{std::nullopt};
    ryn::Signal<ryn::RadioSelection> typed{ryn::RadioSelection{false}};
    ryn::Signal<std::vector<ryn::RadioOption>> options{std::vector<ryn::RadioOption>{{false, ryn::String{u8"Bool"}}}};
    ryn::RadioRef reference;
    auto declare = [&] {
        ryn::Radio(ryn::RadioProps{}.checked(checked).disabled(false).onChange([](bool) {}),
                   ryn::RadioLabel{[] { ryn::Text(u8"Choice"); }});
        ryn::RadioGroup(ryn::RadioGroupProps{}
                            .options({
                                {ryn::String{u8"a"}, ryn::String{u8"甲"}},
                            })
                            .value(selected)
                            .orientation(ryn::RadioGroupOrientation::Vertical)
                            .onChange([](const ryn::String&) {}));
        ryn::RadioGroup(ryn::RadioGroupProps{}
                            .options(options)
                            .selection(typed)
                            .direction(ryn::RadioDirection::RightToLeft)
                            .optionType(ryn::RadioOptionType::Button)
                            .buttonStyle(ryn::RadioButtonStyle::Solid)
                            .size(ryn::RadioSize::Large)
                            .block(true)
                            .onValueChange([](const ryn::RadioValue&) {}));
        ryn::RadioGroup(ryn::RadioGroupProps{}, ryn::RadioGroupContent{[&] {
                            ryn::Radio(
                                ryn::RadioProps{}.value(3.0).ref(reference).autoFocus(true).onClick([](bool) {}));
                            ryn::RadioButton(ryn::RadioProps{}.value(false).wave(false),
                                             ryn::RadioLabel{[] { ryn::Text(u8"Button"); }});
                        }});
    };
    static_cast<void>(declare);
}
