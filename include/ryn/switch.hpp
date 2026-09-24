#pragma once

#include <ryn/layout_style.hpp>
#include <ryn/prop.hpp>

#include <functional>
#include <optional>
#include <utility>

namespace ryn {
namespace detail { struct SwitchPropsAccess; }

enum class SwitchSize { Middle, Small };

class SwitchProps final {
public:
    SwitchProps& checked(Prop<bool> value) { checked_ = std::move(value); return *this; }
    SwitchProps& defaultChecked(bool value) { default_checked_ = value; return *this; }
    SwitchProps& disabled(Prop<bool> value) { disabled_ = std::move(value); return *this; }
    SwitchProps& loading(Prop<bool> value) { loading_ = std::move(value); return *this; }
    SwitchProps& size(Prop<SwitchSize> value) { size_ = std::move(value); return *this; }
    SwitchProps& onChange(std::function<void(bool)> callback) { on_change_ = std::move(callback); return *this; }
    SwitchProps& layout(LayoutStyle value) { layout_ = std::move(value); return *this; }

private:
    friend struct detail::SwitchPropsAccess;
    std::optional<Prop<bool>> checked_;
    std::optional<bool> default_checked_;
    Prop<bool> disabled_{false};
    Prop<bool> loading_{false};
    Prop<SwitchSize> size_{SwitchSize::Middle};
    std::function<void(bool)> on_change_;
    LayoutStyle layout_;
};

void Switch(SwitchProps props);

} // namespace ryn
