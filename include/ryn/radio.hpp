#pragma once

#include <ryn/component.hpp>
#include <ryn/layout_style.hpp>
#include <ryn/prop.hpp>
#include <ryn/string.hpp>

#include <functional>
#include <optional>
#include <utility>
#include <vector>

namespace ryn {
namespace detail { struct RadioPropsAccess; struct RadioGroupPropsAccess; }

class RadioProps final {
public:
    RadioProps& checked(Prop<bool> value) { checked_ = std::move(value); return *this; }
    RadioProps& defaultChecked(bool value) { default_checked_ = value; return *this; }
    RadioProps& disabled(Prop<bool> value) { disabled_ = std::move(value); return *this; }
    RadioProps& value(String value) { value_ = std::move(value); return *this; }
    RadioProps& onChange(std::function<void(bool)> callback) { on_change_ = std::move(callback); return *this; }
    RadioProps& layout(LayoutStyle value) { layout_ = std::move(value); return *this; }

private:
    friend struct detail::RadioPropsAccess;
    std::optional<Prop<bool>> checked_;
    std::optional<bool> default_checked_;
    Prop<bool> disabled_{false};
    std::optional<String> value_;
    std::function<void(bool)> on_change_;
    LayoutStyle layout_;
};

struct RadioLabelSlot final {};
using RadioLabel = SlotContent<RadioLabelSlot>;

void Radio(RadioProps props, std::optional<RadioLabel> label = {});

struct RadioOption final {
    String value;
    String label;
    bool disabled{};
};

enum class RadioGroupOrientation { Horizontal, Vertical };

class RadioGroupProps final {
public:
    RadioGroupProps& options(std::vector<RadioOption> value) { options_ = std::move(value); return *this; }
    RadioGroupProps& value(Prop<std::optional<String>> value) { value_ = std::move(value); return *this; }
    RadioGroupProps& defaultValue(String value) { default_value_ = std::move(value); return *this; }
    RadioGroupProps& disabled(Prop<bool> value) { disabled_ = std::move(value); return *this; }
    RadioGroupProps& orientation(Prop<RadioGroupOrientation> value) { orientation_ = std::move(value); return *this; }
    RadioGroupProps& onChange(std::function<void(const String&)> callback) { on_change_ = std::move(callback); return *this; }
    RadioGroupProps& layout(LayoutStyle value) { layout_ = std::move(value); return *this; }

private:
    friend struct detail::RadioGroupPropsAccess;
    std::vector<RadioOption> options_;
    std::optional<Prop<std::optional<String>>> value_;
    std::optional<String> default_value_;
    Prop<bool> disabled_{false};
    Prop<RadioGroupOrientation> orientation_{RadioGroupOrientation::Horizontal};
    std::function<void(const String&)> on_change_;
    LayoutStyle layout_;
};

void RadioGroup(RadioGroupProps props);

} // namespace ryn
