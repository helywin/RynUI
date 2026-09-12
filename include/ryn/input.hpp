#pragma once

#include <ryn/component.hpp>
#include <ryn/control_size.hpp>
#include <ryn/layout_style.hpp>
#include <ryn/prop.hpp>
#include <ryn/string.hpp>

#include <cstddef>
#include <functional>
#include <optional>
#include <utility>

namespace ryn {
namespace detail { struct InputPropsAccess; }

enum class InputStatus { Default, Warning, Error };

class InputProps final {
public:
    InputProps& value(Prop<String> value) { value_ = std::move(value); return *this; }
    template<std::size_t N> InputProps& value(const char8_t (&value)[N]) { return this->value(String{value}); }
    InputProps& defaultValue(String value) { default_value_ = std::move(value); return *this; }
    template<std::size_t N> InputProps& defaultValue(const char8_t (&value)[N]) { return defaultValue(String{value}); }
    InputProps& placeholder(Prop<String> value) { placeholder_ = std::move(value); return *this; }
    template<std::size_t N> InputProps& placeholder(const char8_t (&value)[N]) { return placeholder(String{value}); }
    InputProps& size(Prop<ControlSize> value) { size_ = std::move(value); return *this; }
    InputProps& status(Prop<InputStatus> value) { status_ = std::move(value); return *this; }
    InputProps& disabled(Prop<bool> value) { disabled_ = std::move(value); return *this; }
    InputProps& readOnly(Prop<bool> value) { read_only_ = std::move(value); return *this; }
    InputProps& maxLength(Prop<std::size_t> value) { max_length_ = std::move(value); return *this; }
    InputProps& onChange(std::function<void(String)> callback) { on_change_ = std::move(callback); return *this; }
    InputProps& onSubmit(std::function<void(String)> callback) { on_submit_ = std::move(callback); return *this; }
    InputProps& layout(LayoutStyle value) { layout_ = std::move(value); return *this; }
private:
    friend struct detail::InputPropsAccess;
    std::optional<Prop<String>> value_;
    std::optional<String> default_value_;
    Prop<String> placeholder_{String{}};
    Prop<ControlSize> size_{ControlSize::Middle};
    Prop<InputStatus> status_{InputStatus::Default};
    Prop<bool> disabled_{false}, read_only_{false};
    std::optional<Prop<std::size_t>> max_length_;
    std::function<void(String)> on_change_, on_submit_;
    LayoutStyle layout_;
};

struct InputPrefixSlot final {};
struct InputSuffixSlot final {};
using InputPrefix = SlotContent<InputPrefixSlot>;
using InputSuffix = SlotContent<InputSuffixSlot>;

void Input(InputProps props, std::optional<InputPrefix> prefix = {}, std::optional<InputSuffix> suffix = {});

} // namespace ryn
