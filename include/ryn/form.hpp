#pragma once

#include <ryn/select.hpp>
#include <ryn/checkbox.hpp>

#include <concepts>
#include <cstdint>
#include <functional>
#include <memory>
#include <optional>
#include <variant>
#include <vector>

namespace ryn {

using FormValue = std::variant<String, std::optional<double>, bool, SelectSelection>;

struct FormFieldValue final {
    String name;
    FormValue value;
    friend bool operator==(const FormFieldValue&, const FormFieldValue&) = default;
};

struct FormError final {
    String name;
    String message;
    friend bool operator==(const FormError&, const FormError&) = default;
};

struct FormResult final {
    std::vector<FormFieldValue> values;
    std::vector<FormError> errors;
    bool out_of_date{};

    [[nodiscard]] bool valid() const noexcept {
        return !out_of_date && errors.empty();
    }
};

namespace detail {
struct FormFieldState;
struct FormRefState;
class FormComponentHost;

struct FormPropBridge final {
    template <class T> static T read(const Prop<T>& prop) {
        if (const auto* value = std::get_if<typename Prop<T>::StaticValue>(&prop.source_)) {
            if constexpr (Prop<T>::indirect_static_value) {
                return **value;
            } else {
                return *value;
            }
        }
        return std::get<Binding<T>>(prop.source_).get();
    }
};

std::shared_ptr<FormFieldState> make_form_field(FormValue initial, std::optional<Binding<FormValue>> source = {},
                                                std::function<void(FormValue)> change = {});
FormValue read_form_field(const std::shared_ptr<FormFieldState>&);
void write_form_field(const std::shared_ptr<FormFieldState>&, FormValue);
void configure_form_required(const std::shared_ptr<FormFieldState>&, String);
void configure_form_validator(const std::shared_ptr<FormFieldState>&, std::function<String(const FormValue&)>);
std::function<void(FormValue)> form_change_handler(const std::shared_ptr<FormFieldState>&);
String form_field_error(const std::shared_ptr<FormFieldState>&);
bool form_field_disabled(const std::shared_ptr<FormFieldState>&);
bool form_field_read_only(const std::shared_ptr<FormFieldState>&);
} // namespace detail

template <class T>
concept FormFieldType = std::same_as<T, String> || std::same_as<T, std::optional<double>> || std::same_as<T, bool> ||
                        std::same_as<T, SelectSelection>;

// Copying a field retains its value identity. Configure rules before registering it.
template <FormFieldType T> class FormField final {
public:
    explicit FormField(T initial = T{}) : state_(detail::make_form_field(FormValue{std::move(initial)})) {}

    FormField(T initial, Prop<T> source, std::function<void(T)> on_change)
        : state_(detail::make_form_field(FormValue{std::move(initial)}, bind([source = std::move(source)] {
                                             return FormValue{detail::FormPropBridge::read(source)};
                                         }),
                                         [callback = std::move(on_change)](FormValue value) {
                                             if (callback) {
                                                 callback(std::get<T>(std::move(value)));
                                             }
                                         })) {}

    FormField& required(String message = String{u8"This field is required"}) {
        detail::configure_form_required(state_, std::move(message));
        return *this;
    }

    FormField& validator(std::function<String(T)> callback) {
        detail::configure_form_validator(state_, [callback = std::move(callback)](const FormValue& value) {
            return callback ? callback(std::get<T>(value)) : String{};
        });
        return *this;
    }

    [[nodiscard]] T get() const {
        return std::get<T>(detail::read_form_field(state_));
    }

    void set(T value) const {
        detail::write_form_field(state_, FormValue{std::move(value)});
    }

    [[nodiscard]] Prop<T> value() const {
        return bind([field = *this] { return field.get(); });
    }

    // Obtain in the FormItem content slot: the callback belongs to that registration.
    [[nodiscard]] std::function<void(T)> onChange() const {
        return [change = detail::form_change_handler(state_)](T value) {
            change(FormValue{std::move(value)});
        };
    }

    [[nodiscard]] String error() const {
        return detail::form_field_error(state_);
    }

    [[nodiscard]] Prop<InputStatus> status() const {
        return bind([field = *this] { return field.error().empty() ? InputStatus::Default : InputStatus::Error; });
    }

    [[nodiscard]] Prop<bool> disabled() const {
        return bind([state = state_] { return detail::form_field_disabled(state); });
    }

    [[nodiscard]] Prop<bool> readOnly() const {
        return bind([state = state_] { return detail::form_field_read_only(state); });
    }

    // Select/Checkbox have no readOnly prop; use this with their disabled prop.
    [[nodiscard]] Prop<bool> uneditable() const {
        return bind(
            [state = state_] { return detail::form_field_disabled(state) || detail::form_field_read_only(state); });
    }

private:
    friend class FormItemProps;
    std::shared_ptr<detail::FormFieldState> state_;
};

class FormRef final {
public:
    FormRef();
    [[nodiscard]] bool bound() const;
    [[nodiscard]] std::optional<FormResult> validate() const;
    // True means the command was accepted, including a validation failure.
    [[nodiscard]] bool submit() const;
    [[nodiscard]] bool reset() const;

private:
    friend class FormProps;
    friend class detail::FormComponentHost;
    std::shared_ptr<detail::FormRefState> state_;
};

class FormProps final {
public:
    FormProps& ref(const FormRef& value) {
        reference_ = value.state_;
        return *this;
    }

    FormProps& disabled(Prop<bool> value = true) {
        disabled_ = std::move(value);
        return *this;
    }

    FormProps& readOnly(Prop<bool> value = true) {
        read_only_ = std::move(value);
        return *this;
    }

    FormProps& focusFirstInvalid(Prop<bool> value = true) {
        focus_first_invalid_ = std::move(value);
        return *this;
    }

    FormProps& onFinish(std::function<void(FormResult)> callback) {
        on_finish_ = std::move(callback);
        return *this;
    }

    FormProps& onFinishFailed(std::function<void(FormResult)> callback) {
        on_failed_ = std::move(callback);
        return *this;
    }

    FormProps& layout(LayoutStyle value) {
        layout_ = std::move(value);
        return *this;
    }

private:
    friend class detail::FormComponentHost;
    std::shared_ptr<detail::FormRefState> reference_;
    Prop<bool> disabled_{false};
    Prop<bool> read_only_{false};
    Prop<bool> focus_first_invalid_{true};
    std::function<void(FormResult)> on_finish_;
    std::function<void(FormResult)> on_failed_;
    LayoutStyle layout_;
};

class FormItemProps final {
public:
    template <FormFieldType T> FormItemProps& field(String name, const FormField<T>& value) {
        name_ = std::move(name);
        field_ = value.state_;
        return *this;
    }

    FormItemProps& label(Prop<String> value) {
        label_ = std::move(value);
        return *this;
    }

    FormItemProps& help(Prop<String> value) {
        help_ = std::move(value);
        return *this;
    }

    FormItemProps& disabled(Prop<bool> value = true) {
        disabled_ = std::move(value);
        return *this;
    }

    FormItemProps& readOnly(Prop<bool> value = true) {
        read_only_ = std::move(value);
        return *this;
    }

    FormItemProps& focus(std::function<bool()> callback) {
        focus_ = std::move(callback);
        control_ = {};
        return *this;
    }

    FormItemProps& control(const InputRef& value) {
        control_ = [value] {
            return detail::capture_form_input(value);
        };
        return *this;
    }

    FormItemProps& control(const CheckboxRef& value) {
        control_ = [value] {
            return std::pair<std::function<bool()>, std::function<void()>>{detail::capture_form_checkbox(value), {}};
        };
        return *this;
    }

    FormItemProps& layout(LayoutStyle value) {
        layout_ = std::move(value);
        return *this;
    }

private:
    friend class detail::FormComponentHost;
    String name_;
    std::shared_ptr<detail::FormFieldState> field_;
    Prop<String> label_{String{}};
    Prop<String> help_{String{}};
    Prop<bool> disabled_{false};
    Prop<bool> read_only_{false};
    std::function<bool()> focus_;
    std::function<std::pair<std::function<bool()>, std::function<void()>>()> control_;
    LayoutStyle layout_;
};

struct FormContentSlot final {};

struct FormItemContentSlot final {};

using FormContent = SlotContent<FormContentSlot>;
using FormItemContent = SlotContent<FormItemContentSlot>;

void Form(FormProps props, FormContent content);
void FormItem(FormItemProps props, FormItemContent content);

} // namespace ryn
