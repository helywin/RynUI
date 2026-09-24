#pragma once

#include "component/window_component_services.hpp"

#include <ryn/prop.hpp>
#include <ryn/string.hpp>

#include <functional>

namespace ryn::detail {

// Mounts an input's focusable action in the current typed slot.
void mount_input_affix_action(WindowComponentServices& host, Prop<String> label,
    Prop<bool> disabled, std::function<void()> activate, Prop<bool> visible = true);

} // namespace ryn::detail
