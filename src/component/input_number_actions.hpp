#pragma once

#include "component/window_component_services.hpp"
#include "component/input_number_value.hpp"

namespace ryn::detail {

struct InputNumberConfig final {
    std::optional<Prop<NumberValue>> value;
    NumberValue initial;
    Prop<NumberOptions> options{NumberOptions{}};
    Prop<InputNumberMode> mode{InputNumberMode::Input};
    Prop<StatisticLocale> locale{StatisticLocale{}};
    std::optional<Prop<String>> separator;
    InputNumberControls icons;
    Prop<bool> controls{true};
    Prop<bool> keyboard{true};
    Prop<bool> wheel{false};
    Prop<bool> change_on_blur{true};
    std::function<String(String, InputNumberFormatInfo)> formatter;
    std::function<String(String)> parser;
    std::function<void(String)> on_input;
    std::function<void(InputNumberStringValue)> string_change;
    std::function<void(InputNumberStringValue)> string_submit;
    std::function<void(Decimal, InputNumberStringStepInfo)> string_step;
    std::function<void(InputNumberValue)> on_change;
    std::function<void(double, InputNumberStepInfo)> on_step;
    std::function<void(InputNumberValue)> on_submit;
};

struct InputNumberActions final {
    runtime::ComponentId group;
    std::array<runtime::ComponentId, 2> buttons;
};

InputNumberActions mount_input_number_actions(WindowComponentServices&, input::InteractionId parent,
                                              std::function<void(InputNumberStepDirection)>,
                                              const InputNumberControls& = {}, int single = -1);
std::size_t tick_input_number_actions(WindowComponentServices&, const InputNumberActions&, animation::AnimationTime);
std::optional<animation::AnimationTime> input_number_actions_deadline(WindowComponentServices&,
                                                                      const InputNumberActions&);
void cancel_input_number_actions(WindowComponentServices&, const InputNumberActions&);
void configure_input_number_actions(WindowComponentServices&, const InputNumberActions&, const InputNumberThemeToken&,
                                    float height, float border_width, float border_radius, bool visible,
                                    std::array<bool, 2> enabled, InputVariant, bool spinner = false);
void paint_input_number_actions(WindowComponentServices&, const InputNumberActions&, runtime::Rect clip);

} // namespace ryn::detail
