#pragma once

#include <ryn/component.hpp>
#include <ryn/control_size.hpp>
#include <ryn/input_types.hpp>
#include <ryn/layout_style.hpp>
#include <ryn/prop.hpp>
#include <ryn/string.hpp>

#include <cstddef>
#include <functional>
#include <memory>
#include <optional>
#include <utility>

namespace ryn {
namespace detail {
struct InputPropsAccess;
struct PasswordPropsAccess;
struct SearchPropsAccess;
struct InputRefState;

struct InputPropsData final {
    std::optional<Prop<String>> value_;
    std::optional<String> default_value_;
    Prop<String> placeholder_{String{}};
    Prop<ControlSize> size_{ControlSize::Middle};
    bool explicit_size_{};
    Prop<InputStatus> status_{InputStatus::Default};
    Prop<InputVariant> variant_{InputVariant::Outlined};
    Prop<bool> disabled_{false};
    Prop<bool> read_only_{false};
    std::optional<Prop<bool>> allow_clear_;
    std::optional<Prop<std::size_t>> max_length_;
    std::function<void(String)> on_change_;
    std::function<void(String)> on_submit_;
    std::function<void()> on_focus_;
    std::function<void()> on_blur_;
    Prop<InputPurpose> purpose_{InputPurpose::Text};
    Prop<InputCapitalization> capitalization_{InputCapitalization::None};
    Prop<bool> autocorrect_{true};
    bool auto_focus_{};
    std::shared_ptr<InputRefState> reference_;
    LayoutStyle layout_;
};
} // namespace detail

class InputRef final {
public:
    InputRef();
    [[nodiscard]] bool focus(InputFocusOptions options = {}) const;
    [[nodiscard]] bool blur() const;
    // UTF-8 byte offsets must be complete grapheme boundaries.
    [[nodiscard]] bool select(std::size_t anchor, std::size_t caret) const;
    [[nodiscard]] bool bound() const;

private:
    template <class> friend class InputPropsBase;
    std::shared_ptr<detail::InputRefState> state_;
};

template <class Derived> class InputPropsBase {
public:
    Derived& value(Prop<String> value) {
        common_.value_ = std::move(value);
        return self();
    }

    template <std::size_t N> Derived& value(const char8_t (&value)[N]) {
        return this->value(String{value});
    }

    Derived& defaultValue(String value) {
        common_.default_value_ = std::move(value);
        return self();
    }

    template <std::size_t N> Derived& defaultValue(const char8_t (&value)[N]) {
        return defaultValue(String{value});
    }

    Derived& placeholder(Prop<String> value) {
        common_.placeholder_ = std::move(value);
        return self();
    }

    template <std::size_t N> Derived& placeholder(const char8_t (&value)[N]) {
        return placeholder(String{value});
    }

    Derived& size(Prop<ControlSize> value) {
        common_.size_ = std::move(value);
        common_.explicit_size_ = true;
        return self();
    }

    Derived& status(Prop<InputStatus> value) {
        common_.status_ = std::move(value);
        return self();
    }

    Derived& variant(Prop<InputVariant> value) {
        common_.variant_ = std::move(value);
        return self();
    }

    Derived& disabled(Prop<bool> value) {
        common_.disabled_ = std::move(value);
        return self();
    }

    Derived& readOnly(Prop<bool> value) {
        common_.read_only_ = std::move(value);
        return self();
    }

    Derived& allowClear(Prop<bool> value) {
        common_.allow_clear_ = std::move(value);
        return self();
    }

    Derived& maxLength(Prop<std::size_t> value) {
        common_.max_length_ = std::move(value);
        return self();
    }

    Derived& onChange(std::function<void(String)> callback) {
        common_.on_change_ = std::move(callback);
        return self();
    }

    Derived& onSubmit(std::function<void(String)> callback) {
        common_.on_submit_ = std::move(callback);
        return self();
    }

    Derived& onFocus(std::function<void()> callback) {
        common_.on_focus_ = std::move(callback);
        return self();
    }

    Derived& onBlur(std::function<void()> callback) {
        common_.on_blur_ = std::move(callback);
        return self();
    }

    Derived& ref(const InputRef& value) {
        common_.reference_ = value.state_;
        return self();
    }

    Derived& autoFocus(bool value = true) {
        common_.auto_focus_ = value;
        return self();
    }

    Derived& purpose(Prop<InputPurpose> value) {
        common_.purpose_ = std::move(value);
        return self();
    }

    Derived& capitalization(Prop<InputCapitalization> value) {
        common_.capitalization_ = std::move(value);
        return self();
    }

    Derived& autocorrect(Prop<bool> value) {
        common_.autocorrect_ = std::move(value);
        return self();
    }

    Derived& layout(LayoutStyle value) {
        common_.layout_ = std::move(value);
        return self();
    }

protected:
    friend struct detail::InputPropsAccess;
    friend struct detail::PasswordPropsAccess;
    friend struct detail::SearchPropsAccess;
    detail::InputPropsData common_;

private:
    Derived& self() {
        return static_cast<Derived&>(*this);
    }
};

class InputProps final : public InputPropsBase<InputProps> {
private:
    friend struct detail::InputPropsAccess;
    friend struct detail::PasswordPropsAccess;
    std::optional<Prop<bool>> password_visible_;
    std::shared_ptr<void> password_lifetime_;
};

struct InputPrefixSlot final {};

struct InputSuffixSlot final {};

using InputPrefix = SlotContent<InputPrefixSlot>;
using InputSuffix = SlotContent<InputSuffixSlot>;

void Input(InputProps props, std::optional<InputPrefix> prefix = {}, std::optional<InputSuffix> suffix = {});

} // namespace ryn
