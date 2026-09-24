#pragma once

#include <ryn/input.hpp>

#include <functional>
#include <optional>
#include <utility>

namespace ryn {
namespace detail { struct PasswordPropsAccess; }

class PasswordProps final {
public:
    PasswordProps& value(Prop<String> value) { value_ = std::move(value); return *this; }
    template<std::size_t N> PasswordProps& value(const char8_t (&value)[N]) { return this->value(String{value}); }
    PasswordProps& defaultValue(String value) { default_value_ = std::move(value); return *this; }
    template<std::size_t N> PasswordProps& defaultValue(const char8_t (&value)[N]) { return defaultValue(String{value}); }
    PasswordProps& placeholder(Prop<String> value) { placeholder_ = std::move(value); return *this; }
    template<std::size_t N> PasswordProps& placeholder(const char8_t (&value)[N]) { return placeholder(String{value}); }
    PasswordProps& size(Prop<ControlSize> value) { size_ = std::move(value); return *this; }
    PasswordProps& status(Prop<InputStatus> value) { status_ = std::move(value); return *this; }
    PasswordProps& disabled(Prop<bool> value) { disabled_ = std::move(value); return *this; }
    PasswordProps& readOnly(Prop<bool> value) { read_only_ = std::move(value); return *this; }
    PasswordProps& maxLength(Prop<std::size_t> value) { max_length_ = std::move(value); return *this; }
    PasswordProps& visible(Prop<bool> value) { visible_ = std::move(value); return *this; }
    PasswordProps& defaultVisible(bool value) { default_visible_ = value; return *this; }
    PasswordProps& visibilityToggle(bool value) { visibility_toggle_ = value; return *this; }
    PasswordProps& onChange(std::function<void(String)> callback) { on_change_ = std::move(callback); return *this; }
    PasswordProps& onSubmit(std::function<void(String)> callback) { on_submit_ = std::move(callback); return *this; }
    PasswordProps& onVisibleChange(std::function<void(bool)> callback) { on_visible_change_ = std::move(callback); return *this; }
    PasswordProps& layout(LayoutStyle value) { layout_ = std::move(value); return *this; }
private:
    friend struct detail::PasswordPropsAccess;
    std::optional<Prop<String>> value_;
    std::optional<String> default_value_;
    Prop<String> placeholder_{String{}};
    Prop<ControlSize> size_{ControlSize::Middle};
    Prop<InputStatus> status_{InputStatus::Default};
    Prop<bool> disabled_{false}, read_only_{false};
    std::optional<Prop<std::size_t>> max_length_;
    std::optional<Prop<bool>> visible_;
    std::optional<bool> default_visible_;
    bool visibility_toggle_{true};
    std::function<void(String)> on_change_, on_submit_;
    std::function<void(bool)> on_visible_change_;
    LayoutStyle layout_;
};

void Password(PasswordProps props);

} // namespace ryn
