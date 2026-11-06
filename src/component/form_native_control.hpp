#pragma once

#include <ryn/form_geometry.hpp>
#include "runtime/component_host.hpp"
#include <cstdint>
#include <functional>
#include <optional>

namespace ryn {
class InputRef;
class CheckboxRef;
class SwitchRef;
} // namespace ryn

namespace ryn::detail {
class WindowComponentServices;
struct FormFocusContext;

// Captured only by the native ref implementations. Application geometry and
// FormControlTarget callbacks never construct this binding identity.
struct FormNativeControl final {
    const WindowComponentServices* window{};
    runtime::ComponentId component;
    std::uint64_t ref_generation{};
    bool enter_capable{};
    std::function<bool()> current;
    std::function<std::optional<FormControlGeometry>(const FormFocusContext&)> geometry;
    std::function<bool(const FormFocusContext&)> focus;
    std::function<void()> reset;
    std::function<bool(std::function<void()>)> connect_cleanup;
};

FormNativeControl capture_form_native_input(const InputRef&);
FormNativeControl capture_form_native_checkbox(const CheckboxRef&);
FormNativeControl capture_form_native_switch(const SwitchRef&);
} // namespace ryn::detail
