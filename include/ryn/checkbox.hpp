#pragma once

#include <ryn/component.hpp>
#include <ryn/layout_style.hpp>
#include <ryn/prop.hpp>

#include <functional>
#include <optional>
#include <utility>

namespace ryn {
namespace detail { struct CheckboxPropsAccess; }

class CheckboxProps final {
public:
    CheckboxProps& checked(Prop<bool> value) { checked_ = std::move(value); return *this; }
    CheckboxProps& defaultChecked(bool value) { default_checked_ = value; return *this; }
    CheckboxProps& indeterminate(Prop<bool> value) { indeterminate_ = std::move(value); return *this; }
    CheckboxProps& disabled(Prop<bool> value) { disabled_ = std::move(value); return *this; }
    CheckboxProps& onChange(std::function<void(bool)> callback) { on_change_ = std::move(callback); return *this; }
    CheckboxProps& layout(LayoutStyle value) { layout_ = std::move(value); return *this; }

private:
    friend struct detail::CheckboxPropsAccess;
    std::optional<Prop<bool>> checked_;
    std::optional<bool> default_checked_;
    Prop<bool> indeterminate_{false};
    Prop<bool> disabled_{false};
    std::function<void(bool)> on_change_;
    LayoutStyle layout_;
};

struct CheckboxLabelSlot final {};
using CheckboxLabel = SlotContent<CheckboxLabelSlot>;

void Checkbox(CheckboxProps props, std::optional<CheckboxLabel> label = {});

} // namespace ryn
