#include <ryn/selection_option_disabled.hpp>
#include <ryn/checkbox.hpp>
#include <ryn/radio.hpp>

#include <type_traits>

namespace {
using namespace ryn;

constexpr bool legacy_boolean_operations() {
    SelectionOptionDisabled unset;
    SelectionOptionDisabled enabled = false;
    SelectionOptionDisabled disabled = true;
    if (unset || enabled || !disabled) {
        return false;
    }
    if (!(unset == false) || !(false == enabled) || disabled == false || true != disabled) {
        return false;
    }
    if (unset == enabled || unset.explicitValue().has_value() || enabled.explicitValue() != false ||
        !unset.valueOr(true) || enabled.valueOr(true)) {
        return false;
    }
    unset = false;
    if (unset != enabled) {
        return false;
    }
    enabled = true;
    const bool old_value = enabled;
    return old_value && enabled == disabled;
}

static_assert(legacy_boolean_operations());
static_assert(std::is_aggregate_v<CheckboxOption> && std::is_aggregate_v<RadioOption>);
static_assert(std::is_same_v<decltype(CheckboxOption{}.disabled), SelectionOptionDisabled>);
static_assert(std::is_same_v<decltype(RadioOption{}.disabled), SelectionOptionDisabled>);

// Keep aggregate bool construction, omitted fields, assignment, if/!/== and
// copying into an ordinary bool in an independent public-header-only TU.
[[maybe_unused]] bool aggregate_calls() {
    CheckboxOption missing{String{u8"missing"}, u8"Missing"};
    CheckboxOption enabled{String{u8"enabled"}, u8"Enabled", false};
    RadioOption disabled{String{u8"disabled"}, u8"Disabled", true};
    missing.disabled = false;
    enabled.disabled = true;
    const bool old_value = enabled.disabled;
    return !missing.disabled && missing.disabled == false && false == missing.disabled && old_value &&
           disabled.disabled && disabled.disabled.explicitValue().value_or(false);
}
} // namespace
