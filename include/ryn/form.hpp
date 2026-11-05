#pragma once

#include <ryn/form_values.hpp>
#include <ryn/form_list.hpp>
#include <ryn/form_validation.hpp>
#include <ryn/form_presentation.hpp>
#include <ryn/input_number.hpp>
#include <ryn/text_area.hpp>
#include <ryn/switch.hpp>
#include <ryn/checkbox.hpp>

#include <concepts>
#include <cstdint>
#include <functional>
#include <memory>
#include <optional>
#include <variant>
#include <vector>

namespace ryn {

namespace detail {
struct FormFieldState;
struct FormRefState;
class FormComponentHost;
std::shared_ptr<FormFieldState> active_form_control(std::size_t);
void attach_form_control(const std::shared_ptr<FormFieldState>&, const InputRef&);
void attach_form_control(const std::shared_ptr<FormFieldState>&, const CheckboxRef&);
void attach_form_control(const std::shared_ptr<FormFieldState>&, const SwitchRef&);
void attach_form_control(const std::shared_ptr<FormFieldState>&, FormControlTarget);
FormPresentation active_form_presentation();
Binding<FormPresentation> form_control_presentation(const std::shared_ptr<FormFieldState>&);
std::function<void()> form_enter_handler();

struct FormPropBridge final {
    template <class T> static T read(const Binding<T>& binding) {
        return binding.get();
    }

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
void configure_form_rules(const std::shared_ptr<FormFieldState>&, std::vector<FormRule>, bool);
void configure_form_async(const std::shared_ptr<FormFieldState>&,
                          std::function<AsyncOptionsSource<FormIssue>(FormValue, FormValidationContext)>);
void configure_form_triggers(const std::shared_ptr<FormFieldState>&, FormValidateTrigger, std::uint32_t,
                             std::vector<FormNamePath>);
InputStatus form_field_status(const std::shared_ptr<FormFieldState>&);
FormFieldMeta form_field_meta(const std::shared_ptr<FormFieldState>&);
std::function<void()> form_blur_handler(const std::shared_ptr<FormFieldState>&);
void configure_form_required(const std::shared_ptr<FormFieldState>&, String);
void configure_form_validator(const std::shared_ptr<FormFieldState>&, std::function<String(const FormValue&)>);
std::function<void(FormValue)> form_change_handler(const std::shared_ptr<FormFieldState>&);
String form_field_error(const std::shared_ptr<FormFieldState>&);
bool form_field_disabled(const std::shared_ptr<FormFieldState>&);
bool form_field_read_only(const std::shared_ptr<FormFieldState>&);
} // namespace detail

template <class T>
concept FormFieldType =
    std::same_as<T, String> || std::same_as<T, std::optional<double>> || std::same_as<T, bool> ||
    std::same_as<T, SelectSelection> || std::same_as<T, SelectValues> || std::same_as<T, std::optional<Decimal>>;

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

    FormField& rules(std::vector<FormRule> rules, bool validate_first = false) {
        detail::configure_form_rules(state_, std::move(rules), validate_first);
        return *this;
    }

    FormField& asyncValidator(FormAsyncValidator<T> provider, OwnerThreadPost post) {
        if (!provider || !post) {
            throw std::invalid_argument("Form async validation requires a provider and owner-thread post");
        }
        detail::configure_form_async(state_, [provider = std::move(provider),
                                              post = std::move(post)](FormValue value, FormValidationContext context) {
            return AsyncOptionsSource<FormIssue>{
                [provider, value = std::get<T>(std::move(value)),
                 context = std::move(context)](AsyncOptionsRequest request, auto complete) mutable {
                    context.generation = request.generation;
                    context.cancelled = request.cancelled;
                    return provider(value, context, [complete = std::move(complete)](std::vector<FormIssue> issues) {
                        complete({std::move(issues), {}});
                    });
                },
                post};
        });
        return *this;
    }

    FormField& validation(FormValidateTrigger trigger, std::uint32_t debounce_ms = 0,
                          std::vector<FormNamePath> dependencies = {}) {
        detail::configure_form_triggers(state_, trigger, debounce_ms, std::move(dependencies));
        return *this;
    }

    [[nodiscard]] FormFieldMeta meta() const {
        return detail::form_field_meta(state_);
    }

    [[nodiscard]] std::function<void()> onBlur() const {
        return detail::form_blur_handler(state_);
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
        return bind([state = state_] { return detail::form_field_status(state); });
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
    [[nodiscard]] bool validateAsync(std::function<void(FormResult)>, FormValidateOptions = {}) const;
    [[nodiscard]] bool submitAsync() const;
    [[nodiscard]] bool cancelValidation() const;
    [[nodiscard]] std::optional<FormControlTarget> getFieldInstance(const FormNamePath&) const;
    [[nodiscard]] std::vector<FormFieldData> getFieldsError(const std::vector<FormNamePath>& = {}) const;
    [[nodiscard]] bool scrollToField(const FormNamePath&, FormScrollOptions = {}) const;
    [[nodiscard]] bool scrollToFirstError(FormScrollOptions = {}) const;
    [[nodiscard]] std::vector<FormFieldData> getFields() const;
    [[nodiscard]] bool setFields(std::vector<FormFieldData>) const;
    [[nodiscard]] bool isFieldTouched(const FormNamePath&) const;
    [[nodiscard]] bool isFieldValidating(const FormNamePath&) const;
    [[nodiscard]] std::vector<String> getFieldError(const FormNamePath&) const;
    [[nodiscard]] std::vector<String> getFieldWarning(const FormNamePath&) const;
    [[nodiscard]] bool isFieldsTouched(const std::vector<FormNamePath>& = {}, bool all = false) const;
    [[nodiscard]] bool isFieldsValidating(const std::vector<FormNamePath>& = {}) const;
    [[nodiscard]] std::optional<FormValue> getFieldValue(const FormNamePath&) const;
    [[nodiscard]] FormData getFieldsValue(bool preserve = false) const;
    [[nodiscard]] bool setFieldValue(const FormNamePath&, FormValue) const;
    [[nodiscard]] bool setFieldsValue(const FormData&) const;
    [[nodiscard]] Binding<std::optional<FormValue>> watch(FormNamePath) const;
    [[nodiscard]] bool resetFields(const std::vector<FormNamePath>&) const;

private:
    friend class FormProps;
    friend FormRef useFormInstance();
    friend class detail::FormComponentHost;
    std::shared_ptr<detail::FormRefState> state_;
};

struct FormProviderEvent final {
    String name;
    FormRef form;
    std::vector<std::pair<String, FormRef>> forms;
    FormResult result;
    std::vector<FormFieldData> changed_fields;
};

class FormProviderProps final {
public:
    FormProviderProps& onFormChange(std::function<void(FormProviderEvent)> value) {
        change_ = std::move(value);
        return *this;
    }

    FormProviderProps& onFormFinish(std::function<void(FormProviderEvent)> value) {
        finish_ = std::move(value);
        return *this;
    }

private:
    friend class detail::FormComponentHost;
    std::function<void(FormProviderEvent)> change_;
    std::function<void(FormProviderEvent)> finish_;
};

struct FormProviderContentSlot final {};

using FormProviderContent = SlotContent<FormProviderContentSlot>;
void FormProvider(FormProviderProps, FormProviderContent);
[[nodiscard]] FormRef useFormInstance();

[[nodiscard]] inline Binding<std::optional<FormValue>> useWatch(FormNamePath path, FormRef form) {
    return form.watch(std::move(path));
}

[[nodiscard]] Binding<FormItemStatus> useFormStatus();

[[nodiscard]] inline Binding<std::optional<FormValue>> useWatch(FormNamePath path) {
    return useFormInstance().watch(std::move(path));
}

class FormErrorListProps final {
public:
    FormErrorListProps& errors(Prop<std::vector<String>> value) {
        errors_ = std::move(value);
        return *this;
    }

    FormErrorListProps& warnings(Prop<std::vector<String>> value) {
        warnings_ = std::move(value);
        return *this;
    }

    FormErrorListProps& layout(LayoutStyle value) {
        layout_ = std::move(value);
        return *this;
    }

private:
    friend class detail::FormComponentHost;
    Prop<std::vector<String>> errors_{std::vector<String>{}};
    Prop<std::vector<String>> warnings_{std::vector<String>{}};
    LayoutStyle layout_;
};

void FormErrorList(FormErrorListProps);

class FormProps final {
public:
    FormProps& preserve(bool value = true) {
        preserve_ = value;
        return *this;
    }

    FormProps& clearOnDestroy(bool value = true) {
        clear_on_destroy_ = value;
        return *this;
    }

    FormProps& name(String value) {
        name_ = std::move(value);
        return *this;
    }

    FormProps& presentation(Prop<FormPresentation> value) {
        presentation_ = std::move(value);
        return *this;
    }

    FormProps& layout(FormLayout value) {
        options_.layout = value;
        return *this;
    }

    FormProps& labelCol(Responsive<FormColumn> value) {
        options_.label_col = std::move(value);
        return *this;
    }

    FormProps& wrapperCol(Responsive<FormColumn> value) {
        options_.wrapper_col = std::move(value);
        return *this;
    }

    FormProps& labelAlign(FormLabelAlign value) {
        options_.label_align = value;
        return *this;
    }

    FormProps& labelWrap(bool value = true) {
        options_.label_wrap = value;
        return *this;
    }

    FormProps& colon(bool value) {
        options_.colon = value;
        return *this;
    }

    FormProps& requiredMark(FormRequiredMark value) {
        options_.required_mark = value;
        return *this;
    }

    FormProps& size(ControlSize value) {
        options_.size = value;
        return *this;
    }

    FormProps& variant(InputVariant value) {
        options_.variant = value;
        return *this;
    }

    FormProps& direction(TextDirection value) {
        options_.direction = value;
        return *this;
    }

    FormProps& locale(FormLocale value) {
        options_.locale = std::move(value);
        return *this;
    }

    FormProps& scrollTarget(FormScrollTarget value) {
        scroll_ = std::move(value);
        return *this;
    }

    FormProps& scrollToFirstError(bool value = true) {
        scroll_first_ = value;
        return *this;
    }

    FormProps& onValuesChange(std::function<void(FormFieldValue, FormData)> value) {
        changed_ = std::move(value);
        return *this;
    }

    FormProps& onFieldsChange(std::function<void(std::vector<FormFieldData>, std::vector<FormFieldData>)> value) {
        fields_changed_ = std::move(value);
        return *this;
    }

    FormProps& ref(const FormRef& value) {
        reference_ = value.state_;
        return *this;
    }

    FormProps& initialValues(FormData value) {
        value.validate();
        initial_values_ = std::move(value);
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
    FormData initial_values_;
    String name_;
    bool preserve_{true};
    bool clear_on_destroy_{};
    FormPresentation options_;
    std::optional<Prop<FormPresentation>> presentation_;
    FormScrollTarget scroll_;
    bool scroll_first_{};
    std::function<void(FormFieldValue, FormData)> changed_;
    std::function<void(std::vector<FormFieldData>, std::vector<FormFieldData>)> fields_changed_;
    Prop<bool> disabled_{false};
    Prop<bool> read_only_{false};
    Prop<bool> focus_first_invalid_{true};
    std::function<void(FormResult)> on_finish_;
    std::function<void(FormResult)> on_failed_;
    LayoutStyle layout_;
};

struct FormItemLabelSlot final {};

struct FormItemHelpSlot final {};

struct FormItemExtraSlot final {};

struct FormItemFeedbackIconSlot final {};

using FormItemLabel = SlotContent<FormItemLabelSlot>;
using FormItemHelp = SlotContent<FormItemHelpSlot>;
using FormItemExtra = SlotContent<FormItemExtraSlot>;
using FormItemFeedbackIcon = SlotContent<FormItemFeedbackIconSlot>;

class FormItemProps final {
public:
    FormItemProps& labelContent(FormItemLabel value) {
        label_slot_ = std::move(value);
        return *this;
    }

    FormItemProps& helpContent(FormItemHelp value) {
        help_slot_ = std::move(value);
        return *this;
    }

    FormItemProps& extraContent(FormItemExtra value) {
        extra_slot_ = std::move(value);
        return *this;
    }

    FormItemProps& feedbackIcon(FormItemFeedbackIcon value) {
        feedback_icon_ = std::move(value);
        return *this;
    }

    template <FormFieldType T> FormItemProps& field(String name, const FormField<T>& value) {
        name_ = std::move(name);
        path_ = FormNamePath{name_};
        field_ = value.state_;
        return *this;
    }

    template <FormFieldType T> FormItemProps& field(FormNamePath path, const FormField<T>& value) {
        path_ = std::move(path);
        name_ = {};
        field_ = value.state_;
        return *this;
    }

    FormItemProps& preserve(bool value = true) {
        preserve_ = value;
        return *this;
    }

    FormItemProps& layout(FormLayout value) {
        form_layout_ = value;
        return *this;
    }

    FormItemProps& labelCol(Responsive<FormColumn> value) {
        label_col_ = std::move(value);
        return *this;
    }

    FormItemProps& wrapperCol(Responsive<FormColumn> value) {
        wrapper_col_ = std::move(value);
        return *this;
    }

    FormItemProps& labelAlign(FormLabelAlign value) {
        label_align_ = value;
        return *this;
    }

    FormItemProps& labelWrap(bool value = true) {
        label_wrap_ = value;
        return *this;
    }

    FormItemProps& colon(bool value) {
        colon_ = value;
        return *this;
    }

    FormItemProps& required(Prop<bool> value) {
        required_ = std::move(value);
        return *this;
    }

    FormItemProps& presentation(Prop<FormPresentation> value) {
        presentation_ = std::move(value);
        return *this;
    }

    FormItemProps& noStyle(Prop<bool> value = true) {
        no_style_ = std::move(value);
        return *this;
    }

    FormItemProps& hidden(Prop<bool> value = true) {
        hidden_ = std::move(value);
        return *this;
    }

    FormItemProps& extra(Prop<String> value) {
        extra_ = std::move(value);
        return *this;
    }

    FormItemProps& tooltip(Prop<String> value) {
        tooltip_ = std::move(value);
        tooltip_configured_ = true;
        return *this;
    }

    FormItemProps& hasFeedback(Prop<bool> value = true) {
        has_feedback_ = std::move(value);
        return *this;
    }

    FormItemProps& validateStatus(Prop<FormValidateStatus> value) {
        status_ = std::move(value);
        return *this;
    }

    FormItemProps& initialValue(FormValue value) {
        initial_ = std::move(value);
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
        geometry_ = {};
        return *this;
    }

    FormItemProps& control(const InputRef& value) {
        control_ = [value] {
            return detail::capture_form_input(value);
        };
        geometry_ = [value] {
            return detail::capture_form_input_geometry(value);
        };
        return *this;
    }

    FormItemProps& control(const CheckboxRef& value) {
        control_ = [value] {
            return std::pair<std::function<bool(const detail::FormFocusContext&)>, std::function<void()>>{
                detail::capture_form_checkbox(value), {}};
        };
        geometry_ = [value] {
            return detail::capture_form_checkbox_geometry(value);
        };
        return *this;
    }

    FormItemProps& control(const SwitchRef& value) {
        control_ = [value] {
            return std::pair<std::function<bool(const detail::FormFocusContext&)>, std::function<void()>>{
                detail::capture_form_switch(value).first, {}};
        };
        geometry_ = [value] {
            return detail::capture_form_switch(value).second;
        };
        return *this;
    }

    FormItemProps& layout(LayoutStyle value) {
        layout_ = std::move(value);
        return *this;
    }

private:
    friend class detail::FormComponentHost;
    std::optional<FormItemLabel> label_slot_;
    std::optional<FormItemHelp> help_slot_;
    std::optional<FormItemExtra> extra_slot_;
    std::optional<FormItemFeedbackIcon> feedback_icon_;
    String name_;
    std::optional<bool> preserve_;
    FormNamePath path_;
    std::optional<FormValue> initial_;
    std::shared_ptr<detail::FormFieldState> field_;
    std::optional<FormLayout> form_layout_;
    std::optional<Responsive<FormColumn>> label_col_;
    std::optional<Responsive<FormColumn>> wrapper_col_;
    std::optional<FormLabelAlign> label_align_;
    std::optional<bool> label_wrap_;
    std::optional<bool> colon_;
    std::optional<Prop<bool>> required_;
    std::optional<Prop<FormPresentation>> presentation_;
    Prop<bool> no_style_{false};
    Prop<bool> hidden_{false};
    Prop<bool> has_feedback_{false};
    std::optional<Prop<FormValidateStatus>> status_;
    Prop<String> extra_{String{}};
    Prop<String> tooltip_{String{}};
    bool tooltip_configured_{};
    Prop<String> label_{String{}};
    Prop<String> help_{String{}};
    Prop<bool> disabled_{false};
    Prop<bool> read_only_{false};
    std::function<bool()> focus_;
    std::function<std::pair<std::function<bool(const detail::FormFocusContext&)>, std::function<void()>>()> control_;
    std::function<std::function<std::optional<FormControlGeometry>(const detail::FormFocusContext&)>()> geometry_;
    LayoutStyle layout_;
};

struct FormContentSlot final {};

struct FormItemContentSlot final {};

using FormContent = SlotContent<FormContentSlot>;
using FormItemContent = SlotContent<FormItemContentSlot>;

template <FormFieldType T> class FormControlBinding final {
public:
    explicit FormControlBinding() : state_(detail::active_form_control(FormValue{T{}}.index())) {}

    [[nodiscard]] Prop<T> value() const {
        return bind([state = state_] { return std::get<T>(detail::read_form_field(state)); });
    }

    [[nodiscard]] std::function<void(T)> onChange() const {
        return [change = detail::form_change_handler(state_)](T value) {
            change(FormValue{std::move(value)});
        };
    }

    [[nodiscard]] Prop<InputStatus> status() const {
        return bind([state = state_] { return detail::form_field_status(state); });
    }

    [[nodiscard]] Prop<bool> disabled() const {
        return bind([state = state_] { return detail::form_field_disabled(state); });
    }

    [[nodiscard]] Prop<bool> readOnly() const {
        return bind([state = state_] { return detail::form_field_read_only(state); });
    }

    [[nodiscard]] Prop<bool> uneditable() const {
        return bind(
            [state = state_] { return detail::form_field_disabled(state) || detail::form_field_read_only(state); });
    }

    [[nodiscard]] std::function<void()> onBlur() const {
        return detail::form_blur_handler(state_);
    }

    [[nodiscard]] Binding<FormPresentation> presentation() const {
        return detail::form_control_presentation(state_);
    }

    void control(const InputRef& ref) const {
        detail::attach_form_control(state_, ref);
    }

    void control(const CheckboxRef& ref) const {
        detail::attach_form_control(state_, ref);
    }

    void control(const SwitchRef& ref) const {
        detail::attach_form_control(state_, ref);
    }

    void control(FormControlTarget target) const {
        detail::attach_form_control(state_, std::move(target));
    }

private:
    std::shared_ptr<detail::FormFieldState> state_;
};

template <FormFieldType T> struct FormControlAdapter final {
    std::function<void(FormControlBinding<T>)> mount;
};

template <FormFieldType T> void FormControl(FormControlAdapter<T> adapter) {
    if (!adapter.mount) {
        throw std::invalid_argument("FormControl requires a typed adapter");
    }
    adapter.mount(FormControlBinding<T>{});
}

void FormControl(InputProps);
void FormControl(TextAreaProps);
void FormControl(InputNumberProps);
void FormControl(SelectProps);
void FormControl(CheckboxProps, std::optional<CheckboxLabel> = {});
void FormControl(SwitchProps);

void Form(FormProps props, FormContent content);
void FormItem(FormItemProps props, FormItemContent content);

} // namespace ryn
