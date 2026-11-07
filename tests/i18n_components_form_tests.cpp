#include "component/form_component.hpp"
#include "support/i18n_components.hpp"

#include <array>

namespace {
using namespace ryn_test::i18n_components;

void sync(Fixture& fixture) {
    require(fixture.services.layout_and_synchronize({800, 1200}, {0, 0, 800, 1200}), "Form i18n layout failed");
}

void error_is(const FormRef& form, const FormNamePath& path, const String& expected) {
    require(form.getFieldError(path) == std::vector<String>{expected}, "current Form error used the wrong source");
}

String result_error(const FormResult& result, const FormNamePath& path) {
    const auto found = std::ranges::find(result.errors, path, &FormError::path);
    require(found != result.errors.end(), "FormResult lost the expected error path");
    return found->message;
}

void clean(Fixture& fixture) {
    fixture.services.dispose();
    require(fixture.nodes.size() == 0 && fixture.services.interactions().size() == 0 &&
                !fixture.services.next_frame_deadline(),
            "retired Form retained nodes, interactions or deadlines");
}

void builtin_literal_and_snapshots() {
    Fixture fixture;
    Signal<LocaleId> language{locales::en_US};
    FormRef form;
    FormField<String> required;
    FormField<String> literal;
    FormField<String> rule_field;
    FormField<String> rule_literal;
    FormField<String> business{u8"value"};
    FormField<String> warning{u8"x"};
    required.required();
    literal.required(u8"This field is required");
    FormRule rule;
    rule.required = true;
    rule_field.rules({rule});
    rule.message = u8"Invalid field value";
    rule_literal.rules({rule});
    FormRule warning_rule;
    warning_rule.min_length = 3;
    warning_rule.warning_only = true;
    warning.rules({warning_rule});
    int validators = 0;
    business.validator([&](String) {
        ++validators;
        return String{u8"This field is required"};
    });
    int mounts = 0;
    int values_changes = 0;
    int fields_changes = 0;
    int provider_changes = 0;
    std::function<FormItemStatus()> current_status;
    fixture.services.mount(Content{[&] {
        ConfigProvider(
            ConfigProviderProps{}.locale(language), ConfigProviderContent{[&] {
                FormProvider(FormProviderProps{}.onFormChange([&](FormProviderEvent) { ++provider_changes; }),
                             FormProviderContent{[&] {
                                 Form(FormProps{}
                                          .ref(form)
                                          .name(u8"localized")
                                          .onValuesChange([&](auto, auto) { ++values_changes; })
                                          .onFieldsChange([&](auto, auto) { ++fields_changes; }),
                                      FormContent{[&] {
                                          ++mounts;
                                          FormItem(FormItemProps{}.field(u8"required", required), FormItemContent{[&] {
                                                       const auto status = useFormStatus();
                                                       current_status = [status] {
                                                           return status.get();
                                                       };
                                                       Input(InputProps{});
                                                   }});
                                          FormItem(FormItemProps{}.field(u8"literal", literal),
                                                   FormItemContent{[] { Input(InputProps{}); }});
                                          FormItem(FormItemProps{}.field(u8"rule", rule_field),
                                                   FormItemContent{[] { Input(InputProps{}); }});
                                          FormItem(FormItemProps{}.field(u8"rule-literal", rule_literal),
                                                   FormItemContent{[] { Input(InputProps{}); }});
                                          FormItem(FormItemProps{}.field(u8"business", business),
                                                   FormItemContent{[] { Input(InputProps{}); }});
                                          FormItem(FormItemProps{}.field(u8"warning", warning),
                                                   FormItemContent{[] { Input(InputProps{}); }});
                                      }});
                             }});
            }});
    }});
    sync(fixture);
    const auto historical = form.validate();
    require(historical && !historical->out_of_date && validators == 1, "initial Form validation failed");
    sync(fixture);
    const auto item = fixture.services.form().items().front();
    const auto feedback = fixture.services.form().snapshot(item).feedback;
    const auto text = text_component(fixture, feedback);
    const auto input = fixture.inputs.mounted_inputs().front();
    const auto values = form.getFieldsValue(true);
    const auto meta = required.meta();
    language.set(locales::zh_CN);
    sync(fixture);
    error_is(form, FormNamePath{u8"required"}, u8"此项为必填项");
    error_is(form, FormNamePath{u8"literal"}, u8"This field is required");
    error_is(form, FormNamePath{u8"rule"}, u8"字段值无效");
    error_is(form, FormNamePath{u8"rule-literal"}, u8"Invalid field value");
    error_is(form, FormNamePath{u8"business"}, u8"This field is required");
    require(form.getFieldWarning(FormNamePath{u8"warning"}) == std::vector<String>{u8"字段值无效"},
            "builtin warnings did not reproject");
    require(required.error() == String{u8"此项为必填项"} &&
                current_status().errors == std::vector<String>{u8"此项为必填项"} &&
                form.getFields().front().meta.errors == current_status().errors &&
                form.getFieldsError().front().meta.errors == current_status().errors &&
                label_text(fixture, feedback) == String{u8"此项为必填项"},
            "field/meta/useFormStatus/retained feedback disagreed on current locale");
    require(result_error(*historical, FormNamePath{u8"required"}) == String{u8"This field is required"} &&
                historical->warnings.front().message == String{u8"Invalid field value"},
            "a previously returned FormResult was mutated by language publication");
    require(required.meta().touched == meta.touched && required.meta().validating == meta.validating &&
                required.meta().out_of_date == meta.out_of_date && form.getFieldsValue(true) == values &&
                validators == 1 && mounts == 1 && values_changes == 0 && fields_changes == 0 && provider_changes == 0 &&
                fixture.inputs.mounted_inputs().front().editor == input.editor &&
                text_component(fixture, feedback).scene == text.scene && !fixture.services.next_frame_deadline(),
            "locale update reran business work, replaced controls, changed metadata/values or scheduled a deadline");
    const auto latest = form.validate();
    require(latest && result_error(*latest, FormNamePath{u8"required"}) == String{u8"此项为必填项"} && validators == 2,
            "subsequent validation did not return a new localized snapshot");
    // Explicit server/setFields metadata is literal even when it equals the current builtin.
    Scope source_observations;
    String observed = required.error();
    static_cast<void>(connect_binding(source_observations, bind([&] { return required.error(); }),
                                      [&](const String& value) { observed = value; }));
    auto fields = form.getFields();
    fields.front().meta.errors = {u8"This field is required"};
    require(form.setFields({fields.front()}), "explicit metadata update failed");
    language.set(locales::en_US);
    language.set(locales::zh_CN);
    error_is(form, FormNamePath{u8"required"}, u8"This field is required");
    language.set(locales::en_US);
    require(form.validate().has_value(), "restoring builtin validation failed");
    language.set(locales::zh_CN);
    require(observed == String{u8"此项为必填项"},
            "same-text literal-to-builtin replacement lost the reactive locale dependency");
    require(form.reset() && required.error().empty() && required.meta().errors.empty(),
            "reset resurrected a stored builtin issue");
    source_observations.dispose();
    clean(fixture);
}

void rejects_explicit_empty_messages() {
    FormField<String> field;
    bool required_rejected = false;
    try {
        field.required(String{});
    } catch (const std::invalid_argument&) {
        required_rejected = true;
    }
    bool rule_rejected = false;
    FormRule rule;
    rule.required = true;
    rule.message = String{};
    try {
        field.rules({rule});
    } catch (const std::invalid_argument&) {
        rule_rejected = true;
    }
    require(required_rejected && rule_rejected, "explicit empty required/rule messages became implicit defaults");
    rule.message.reset();
    field.required().rules({rule});
}

void nested_scopes_windows_and_overrides() {
    auto first = std::make_unique<Fixture>();
    auto second = std::make_unique<Fixture>();
    Signal<LocaleId> language{locales::zh_CN};
    std::array<FormRef, 5> forms;
    std::array<FormField<String>, 5> fields;
    FormField<String> nested_item;
    nested_item.required();
    for (auto& field : fields) {
        field.required();
    }
    const auto mount = [&](std::size_t index, FormProps props = {}) {
        Form(std::move(props.ref(forms[index])), FormContent{[&, index] {
                 FormItem(FormItemProps{}.field(u8"name", fields[index]), FormItemContent{[] { Input(InputProps{}); }});
                 if (index == 0) {
                     ConfigProvider(ConfigProviderProps{}.locale(locales::en_US), ConfigProviderContent{[&] {
                                        FormItem(FormItemProps{}.field(u8"inner-name", nested_item),
                                                 FormItemContent{[] { Input(InputProps{}); }});
                                    }});
                 }
             }});
    };
    first->services.mount(Content{[&] {
        ConfigProvider(ConfigProviderProps{}.locale(language), ConfigProviderContent{[&] {
                           mount(0);
                           ConfigProvider(ConfigProviderProps{}, ConfigProviderContent{[&] { mount(1); }});
                           ConfigProvider(ConfigProviderProps{}.locale(locales::en_US),
                                          ConfigProviderContent{[&] { mount(2); }});
                           mount(3, FormProps{}.locale(FormLocale{}));
                       }});
    }});
    second->services.mount(Content{[&] { mount(4); }});
    sync(*first);
    sync(*second);
    for (const auto& form : forms) {
        require(form.validate().has_value(), "nested/window Form validation failed");
    }
    for (std::size_t index = 0; index < forms.size(); ++index) {
        error_is(forms[index], FormNamePath{u8"name"},
                 index < 2 ? String{u8"此项为必填项"} : String{u8"This field is required"});
    }
    error_is(forms[0], FormNamePath{u8"inner-name"}, u8"This field is required");
    static_cast<void>(second->frames.consume_request());
    const auto requests = second->frames.counters().requests;
    language.set(locales::en_US);
    sync(*first);
    for (const auto& form : forms) {
        error_is(form, FormNamePath{u8"name"}, u8"This field is required");
    }
    require(second->frames.counters().requests == requests, "Form locale update crossed window ownership");
    language.set(locales::zh_CN);
    error_is(forms[0], FormNamePath{u8"name"}, u8"此项为必填项");
    error_is(forms[2], FormNamePath{u8"name"}, u8"This field is required");
    error_is(forms[3], FormNamePath{u8"name"}, u8"This field is required");
    error_is(forms[0], FormNamePath{u8"inner-name"}, u8"This field is required");
    clean(*first);
    clean(*second);
}

struct Async final {
    std::vector<std::function<void()>> posts;
    std::vector<FormValidationComplete> complete;
    std::vector<std::shared_ptr<const std::atomic_bool>> tickets;
    int calls{};
    int cancels{};

    void configure(FormField<String>& field) {
        field.asyncValidator(
            [&](String, FormValidationContext context, FormValidationComplete completion) {
                ++calls;
                complete.push_back(std::move(completion));
                tickets.push_back(context.cancelled);
                return [&] {
                    ++cancels;
                };
            },
            [&](auto callback) { posts.push_back(std::move(callback)); });
    }

    void drain() {
        auto callbacks = std::exchange(posts, {});
        for (auto& callback : callbacks) {
            callback();
        }
    }
};

void async_language_preserves_authority_and_snapshot() {
    Fixture fixture;
    Signal<LocaleId> language{locales::en_US};
    FormRef form;
    FormField<String> required;
    FormField<String> pending{u8"valid"};
    required.required();
    Async async;
    async.configure(pending);
    int validators = 0;
    pending.validator([&](String) {
        ++validators;
        return String{};
    });
    fixture.services.mount(Content{[&] {
        ConfigProvider(ConfigProviderProps{}.locale(language), ConfigProviderContent{[&] {
                           Form(FormProps{}.ref(form), FormContent{[&] {
                                    FormItem(FormItemProps{}.field(u8"required", required),
                                             FormItemContent{[] { Input(InputProps{}); }});
                                    FormItem(FormItemProps{}.field(u8"pending", pending),
                                             FormItemContent{[] { Input(InputProps{}); }});
                                }});
                       }});
    }});
    sync(fixture);
    std::vector<FormResult> results;
    require(form.validateAsync([&](FormResult result) { results.push_back(std::move(result)); }),
            "initial async Form validation was refused");
    async.complete[0]({});
    async.drain();
    require(results.size() == 1 && !results.front().out_of_date, "initial async snapshot was not delivered");
    require(form.validateAsync([&](FormResult result) { results.push_back(std::move(result)); }),
            "second async Form validation was refused");
    sync(fixture);
    const auto deadline = fixture.services.form().next_auxiliary_deadline();
    const auto values = form.getFieldsValue(true);
    const auto touched = pending.meta().touched;
    const auto cancellations = async.cancels;
    language.set(locales::zh_CN);
    sync(fixture);
    error_is(form, FormNamePath{u8"required"}, u8"此项为必填项");
    const auto item = fixture.services.form().items()[1];
    require(label_text(fixture, fixture.services.form().snapshot(item).feedback) == String{u8"校验中"},
            "in-flight async feedback did not localize");
    require(async.calls == 2 && validators == 2 && async.cancels == cancellations && !async.tickets.back()->load() &&
                form.isFieldValidating(FormNamePath{u8"pending"}) && pending.meta().touched == touched &&
                form.getFieldsValue(true) == values && results.size() == 1 &&
                fixture.services.form().next_auxiliary_deadline() == deadline,
            "locale change reran/cancelled async work, changed values/meta or added a deadline");
    async.complete[1]({{u8"Invalid field value", false}, {u8"业务警告", true}});
    async.drain();
    require(results.size() == 2 && !results.back().out_of_date && !form.isFieldsValidating() &&
                result_error(results.back(), FormNamePath{u8"required"}) == String{u8"此项为必填项"} &&
                result_error(results.front(), FormNamePath{u8"required"}) == String{u8"This field is required"},
            "async completion lost its latest-locale snapshot or mutated a delivered result");
    language.set(locales::en_US);
    sync(fixture);
    error_is(form, FormNamePath{u8"required"}, u8"This field is required");
    error_is(form, FormNamePath{u8"pending"}, u8"Invalid field value");
    require(form.getFieldWarning(FormNamePath{u8"pending"}) == std::vector<String>{u8"业务警告"} &&
                result_error(results.back(), FormNamePath{u8"required"}) == String{u8"此项为必填项"} &&
                !fixture.services.form().next_auxiliary_deadline() && !fixture.services.next_frame_deadline(),
            "business async text was translated or settled Form retained deadlines");
    clean(fixture);
}

void locale_observer_reentry_and_retirement() {
    Fixture fixture;
    Signal<LocaleId> language{locales::en_US};
    FormRef form;
    FormField<String> field;
    field.required();
    const auto owner =
        mount_owned(fixture, Content{[&] {
                        ConfigProvider(ConfigProviderProps{}.locale(language), ConfigProviderContent{[&] {
                                           Form(FormProps{}.ref(form), FormContent{[&] {
                                                    FormItem(FormItemProps{}.field(u8"name", field),
                                                             FormItemContent{[] { Input(InputProps{}); }});
                                                }});
                                       }});
                    }});
    sync(fixture);
    require(form.validate().has_value(), "reentry validation setup failed");
    Scope observations;
    bool reenter = true;
    static_cast<void>(connect_binding(observations, bind([&] { return field.error(); }), [&](const String& error) {
        if (reenter && error == String{u8"此项为必填项"}) {
            reenter = false;
            language.set(locales::en_US);
        }
    }));
    language.set(locales::zh_CN);
    sync(fixture);
    error_is(form, FormNamePath{u8"name"}, u8"This field is required");
    require(label_text(fixture, fixture.services.form().snapshot(fixture.services.form().items().front()).feedback) ==
                String{u8"This field is required"},
            "older locale callback overwrote the reentrant final language");
    static_cast<void>(connect_binding(observations, bind([&] { return field.meta(); }), [&](const FormFieldMeta& meta) {
        if (meta.errors == std::vector<String>{u8"此项为必填项"}) {
            require(fixture.services.destroy(owner), "locale observer could not retire its Form owner");
        }
    }));
    language.set(locales::zh_CN);
    require(!form.bound() && !fixture.services.components().contains(owner), "retired Form kept its registration");
    observations.dispose();
    language.set(locales::en_US);
    sync(fixture);
    require(field.meta().errors.empty() && field.error().empty() && !fixture.services.next_frame_deadline(),
            "retired locale observer resurrected an error or deadline");
    clean(fixture);
}

void validator_reentry_reads_latest_locale_and_controlled_authority() {
    Fixture fixture;
    Signal<LocaleId> language{locales::en_US};
    Signal<String> authority{u8"authority"};
    int requests = 0;
    FormField<String> controlled{String{}, authority, [&](String) { ++requests; }};
    FormField<String> required;
    required.required();
    int calls = 0;
    controlled.validator([&](String) {
        ++calls;
        language.set(locales::zh_CN);
        return String{u8"Server text"};
    });
    FormRef form;
    std::function<void(String)> change;
    fixture.services.mount(Content{[&] {
        ConfigProvider(ConfigProviderProps{}.locale(language), ConfigProviderContent{[&] {
                           Form(FormProps{}.ref(form), FormContent{[&] {
                                    FormItem(FormItemProps{}.field(u8"required", required),
                                             FormItemContent{[] { Input(InputProps{}); }});
                                    FormItem(FormItemProps{}.field(u8"controlled", controlled), FormItemContent{[&] {
                                                 change = controlled.onChange();
                                                 Input(InputProps{});
                                             }});
                                }});
                       }});
    }});
    sync(fixture);
    change(u8"rejected edit");
    require(requests == 1 && controlled.get() == String{u8"authority"} && controlled.meta().touched,
            "controlled setup forged an accepted value or lost touched");
    const auto result = form.validate();
    require(result && !result->out_of_date && calls == 1 && requests == 1 &&
                result_error(*result, FormNamePath{u8"required"}) == String{u8"此项为必填项"},
            "validator locale reentry invalidated business authority or returned an older locale");
    const auto values = form.getFieldsValue(true);
    language.set(locales::en_US);
    require(controlled.meta().touched && controlled.get() == String{u8"authority"} && requests == 1 && calls == 1 &&
                form.getFieldsValue(true) == values,
            "locale update retried a controlled setter, validator or cleared dirty metadata");
    error_is(form, FormNamePath{u8"controlled"}, u8"Server text");
    clean(fixture);
}

void late_async_after_locale_retirement() {
    Fixture fixture;
    Signal<LocaleId> language{locales::en_US};
    FormRef form;
    FormField<String> field{u8"value"};
    Async async;
    async.configure(field);
    const auto owner =
        mount_owned(fixture, Content{[&] {
                        ConfigProvider(ConfigProviderProps{}.locale(language), ConfigProviderContent{[&] {
                                           Form(FormProps{}.ref(form), FormContent{[&] {
                                                    FormItem(FormItemProps{}.field(u8"name", field),
                                                             FormItemContent{[] { Input(InputProps{}); }});
                                                }});
                                       }});
                    }});
    sync(fixture);
    int callbacks = 0;
    bool cancellation_results = true;
    require(form.validateAsync([&](FormResult result) {
        ++callbacks;
        cancellation_results &= result.out_of_date;
    }),
            "late async validation setup failed");
    language.set(locales::zh_CN);
    require(async.cancels == 0 && !async.tickets.front()->load(), "locale update cancelled an active request");
    require(fixture.services.destroy(owner), "async Form owner retirement failed");
    const auto retirement_callbacks = callbacks;
    require(retirement_callbacks <= 1 && cancellation_results,
            "Form retirement delivered duplicate or authoritative cancellation results");
    async.complete.front()({{u8"late server error", false}});
    async.drain();
    language.set(locales::en_US);
    require(callbacks == retirement_callbacks && async.cancels == 1 && async.tickets.front()->load() && !form.bound() &&
                !field.meta().validating && field.meta().errors.empty(),
            "retired async callback published into an old registration");
    clean(fixture);
}

void dirty_selection_and_rule_callback_retirement() {
    Fixture fixture;
    Signal<LocaleId> language{locales::en_US};
    FormRef form;
    FormField<String> field{u8"value"};
    FormField<String> sibling{u8"sibling"};
    runtime::ComponentId owner;
    int transforms = 0;
    int patterns = 0;
    int validators = 0;
    FormRule rule;
    rule.transform = [&](FormValue value) {
        ++transforms;
        language.set(locales::en_US);
        require(fixture.services.destroy(owner), "rule callback failed to retire the Form owner");
        return value;
    };
    rule.pattern = [&](String) {
        ++patterns;
        return false;
    };
    field.rules({rule});
    sibling.validator([&](String) {
        ++validators;
        return String{u8"must not run after retirement"};
    });
    owner = mount_owned(fixture, Content{[&] {
                            ConfigProvider(ConfigProviderProps{}.locale(language), ConfigProviderContent{[&] {
                                               Form(FormProps{}.ref(form), FormContent{[&] {
                                                        FormItem(FormItemProps{}.field(u8"name", field),
                                                                 FormItemContent{[] { Input(InputProps{}); }});
                                                        FormItem(FormItemProps{}.field(u8"sibling", sibling),
                                                                 FormItemContent{[] { Input(InputProps{}); }});
                                                    }});
                                           }});
                        }});
    sync(fixture);
    const auto meta = field.meta();
    language.set(locales::zh_CN);
    FormValidateOptions options;
    options.dirty = true;
    options.validate_only = true;
    int results = 0;
    require(form.validateAsync(
                [&](FormResult result) {
                    require(result.valid() && !result.out_of_date, "dirty validate-only result was invalid");
                    ++results;
                },
                options) &&
                results == 1 && transforms == 0 && validators == 0 && field.meta() == meta,
            "locale publication changed dirty selection or validate-only metadata");
    const auto result = form.validate();
    require(result && result->out_of_date && transforms == 1 && patterns == 0 && validators == 0 && !form.bound() &&
                field.meta().errors.empty() && field.error().empty(),
            "rule callback continued on a retired generation or published stale errors");
    language.set(locales::zh_CN);
    clean(fixture);
}

void application_error_display_keeps_validation_provenance() {
    Fixture fixture;
    Signal<LocaleId> language{locales::en_US};
    const MessageKey message{
        u8"app.form.issue", String{u8"Application rule rejected {value}"}, {{u8"value", MessageArgType::Text}}};
    const MessageCatalog messages{{{locales::zh_CN, message, String{u8"应用规则拒绝 {value}"}}}};
    FormRef form;
    FormField<String> field{u8"bad"};
    int validators = 0;
    int mounts = 0;
    field.validator([&](String value) {
        ++validators;
        return value == String{u8"ok"} ? String{} : String{u8"literal server error"};
    });
    fixture.services.mount(Content{[&] {
        ConfigProvider(ConfigProviderProps{}.locale(language).messages(messages), ConfigProviderContent{[&] {
                           const auto tr = useI18n();
                           Form(FormProps{}.ref(form), FormContent{[&, tr] {
                                    FormItem(FormItemProps{}
                                                 .field(u8"value", field)
                                                 .label(String{u8"Value"})
                                                 .required(true)
                                                 .errorText(bind([=] {
                                                     return field.errorSource() == FormIssueSource::Validator
                                                                ? tr.translate(
                                                                      message,
                                                                      MessageArgs{{u8"value", String{u8"{payload}"}}})
                                                                : field.error();
                                                 })),
                                             FormItemContent{[&] {
                                                 ++mounts;
                                                 Input(InputProps{});
                                             }});
                                }});
                       }});
    }});
    sync(fixture);
    const auto historical = form.validate();
    sync(fixture);
    require(historical && validators == 1, "application display test did not validate exactly once");
    const auto owner = fixture.services.form().items().front();
    const auto regions = fixture.services.form().snapshot(owner);
    const auto input = fixture.inputs.mounted_inputs().front();
    const auto meta = field.meta();
    require(label_text(fixture, regions.feedback) == String{u8"Application rule rejected {payload}"},
            "explicit application display did not use the public message binding");
    language.set(locales::zh_CN);
    sync(fixture);
    const auto after = fixture.services.form().snapshot(owner);
    require(label_text(fixture, regions.feedback) == String{u8"应用规则拒绝 {payload}"} &&
                field.error() == String{u8"literal server error"} &&
                result_error(*historical, FormNamePath{u8"value"}) == String{u8"literal server error"} &&
                validators == 1 && mounts == 1 && field.meta() == meta && after.label == regions.label &&
                after.feedback == regions.feedback && fixture.inputs.mounted_inputs().front().editor == input.editor,
            "display projection mutated an issue/result, reran validation or replaced Form regions/editor");
    auto explicit_fields = form.getFields();
    explicit_fields.front().meta.errors = {u8"literal server error"};
    require(form.setFields({explicit_fields.front()}), "explicit same-text issue update failed");
    language.set(locales::en_US);
    sync(fixture);
    require(field.errorSource() == FormIssueSource::Literal &&
                label_text(fixture, regions.feedback) == String{u8"literal server error"} && validators == 1,
            "explicit same-text error inherited validator translation provenance");
    field.set(u8"ok");
    require(form.validate()->valid(), "application rule failed to recover");
    sync(fixture);
    require(label_text(fixture, regions.feedback).empty() && validators == 2,
            "an error display override created an issue for a valid field");
    clean(fixture);
}
} // namespace

int main() {
    ryn_test::i18n::suppress_dialogs();
    try {
        rejects_explicit_empty_messages();
        builtin_literal_and_snapshots();
        nested_scopes_windows_and_overrides();
        async_language_preserves_authority_and_snapshot();
        locale_observer_reentry_and_retirement();
        validator_reentry_reads_latest_locale_and_controlled_authority();
        late_async_after_locale_retirement();
        dirty_selection_and_rule_callback_retirement();
        application_error_display_keeps_validation_provenance();
        std::cout << "Form i18n source, snapshot, async and retained-host contracts passed\n";
        return 0;
    } catch (const std::exception& error) {
        std::cerr << error.what() << '\n';
        return 1;
    }
}
