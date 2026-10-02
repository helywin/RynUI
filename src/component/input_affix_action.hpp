#pragma once

#include "component/window_component_services.hpp"

#include <ryn/prop.hpp>
#include <ryn/icon.hpp>

#include <functional>

namespace ryn::detail {

// Mounts an input's focusable action in the current typed slot.
void mount_input_affix_action(WindowComponentServices& host, Prop<IconSource> icon, Prop<bool> disabled,
                              std::function<void()> activate, Prop<bool> visible = true, Prop<bool> focusable = true,
                              Prop<bool> tab_stop = false, Prop<bool> activate_on_hover = false);

} // namespace ryn::detail
