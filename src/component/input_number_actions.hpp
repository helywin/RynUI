#pragma once

#include "component/window_component_services.hpp"
#include "component/input_number_model.hpp"

namespace ryn::detail {

struct InputNumberConfig final {
    std::optional<Prop<InputNumberValue>> value;
    InputNumberValue initial;
    Prop<InputNumberOptions> options{InputNumberOptions{}};
    Prop<bool> controls{true};
    Prop<bool> keyboard{true};
    std::function<void(InputNumberValue)> on_change;
    std::function<void(double, InputNumberStepInfo)> on_step;
    std::function<void(InputNumberValue)> on_submit;
};

struct InputNumberActions final {
    runtime::ComponentId group;
    std::array<runtime::ComponentId, 2> buttons;
};

InputNumberActions mount_input_number_actions(WindowComponentServices&, input::InteractionId parent,
                                              std::function<void(InputNumberStepDirection)>);
void configure_input_number_actions(WindowComponentServices&, const InputNumberActions&, const InputNumberThemeToken&,
                                    float height, float border_width, bool visible, std::array<bool, 2> enabled,
                                    InputVariant);
void paint_input_number_actions(WindowComponentServices&, const InputNumberActions&, runtime::Rect clip);

} // namespace ryn::detail
