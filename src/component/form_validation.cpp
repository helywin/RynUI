#include "component/form_model.hpp"
#include "component/exact_decimal.hpp"
#include "text/text_engine.hpp"
#include <charconv>
#include <limits>

namespace ryn::detail {
namespace {
bool empty_value(const FormValue& value) {
    return std::visit(
        [](const auto& v) {
            using T = std::decay_t<decltype(v)>;
            if constexpr (std::same_as<T, String> || std::same_as<T, SelectValues>) {
                return v.empty();
            } else if constexpr (std::same_as<T, bool>) {
                return !v;
            } else {
                return !v.has_value();
            }
        },
        value);
}

std::optional<ExactDecimal> number(const FormValue& value) {
    if (const auto* v = std::get_if<std::optional<Decimal>>(&value); v && *v) {
        return ExactDecimal::parse((*v)->text().bytes());
    }
    if (const auto* v = std::get_if<std::optional<double>>(&value); v && *v) {
        char buffer[64];
        const auto result = std::to_chars(buffer, buffer + sizeof(buffer), **v, std::chars_format::general,
                                          std::numeric_limits<double>::max_digits10);
        return ExactDecimal::parse(std::string_view{buffer, result.ptr});
    }
    return {};
}

void ensure_configurable(const std::shared_ptr<FormFieldState>& field) {
    if (!field || field->registered) {
        throw std::invalid_argument("Form rules must be configured before registration");
    }
}
} // namespace

void configure_form_rules(const std::shared_ptr<FormFieldState>& field, std::vector<FormRule> rules, bool first) {
    ensure_configurable(field);
    for (const auto& rule : rules) {
        if ((rule.message && rule.message->empty()) ||
            (rule.min_length && rule.max_length && *rule.min_length > *rule.max_length)) {
            throw std::invalid_argument("Invalid Form rule message or length range");
        }
        if (rule.minimum && rule.maximum &&
            ExactDecimal::parse(rule.minimum->text().bytes())
                    ->compare(*ExactDecimal::parse(rule.maximum->text().bytes())) > 0) {
            throw std::invalid_argument("Invalid Form numeric rule range");
        }
    }
    field->rules = std::move(rules);
    field->validate_first = first;
}

void configure_form_async(const std::shared_ptr<FormFieldState>& field,
                          std::function<AsyncOptionsSource<FormIssue>(FormValue, FormValidationContext)> factory) {
    ensure_configurable(field);
    field->async = std::move(factory);
}

void configure_form_triggers(const std::shared_ptr<FormFieldState>& field, FormValidateTrigger trigger,
                             std::uint32_t debounce, std::vector<FormNamePath> dependencies) {
    ensure_configurable(field);
    if (trigger != FormValidateTrigger::Manual && trigger != FormValidateTrigger::Change &&
        trigger != FormValidateTrigger::Blur && trigger != FormValidateTrigger::ChangeAndBlur) {
        throw std::invalid_argument("Invalid Form validation trigger");
    }
    field->trigger = trigger;
    field->debounce_ms = debounce;
    field->dependencies = std::move(dependencies);
}

String project_form_issue(const FormStoredIssue& issue, const FormLocale& locale) {
    switch (issue.source) {
    case FormMessageSource::Required:
        return locale.required;
    case FormMessageSource::Invalid:
        return locale.invalid;
    case FormMessageSource::Literal:
    case FormMessageSource::Validator:
    case FormMessageSource::RuleValidator:
    case FormMessageSource::AsyncValidator:
        return issue.message;
    }
    return {};
}

FormFieldMeta form_field_meta(const std::shared_ptr<FormFieldState>& field) {
    auto meta = field->meta.get();
    const auto issues = field->issues.get();
    // Empty metadata is authoritative after edit/reset/removal. setFields explicitly
    // replaces the sources with literals, even when its text equals a builtin.
    if ((meta.errors.empty() && meta.warnings.empty()) ||
        std::ranges::none_of(issues, [](const auto& issue) { return issue.source != FormMessageSource::Literal; })) {
        return meta;
    }
    const auto locale = field->presentation.get().locale;
    meta.errors.clear();
    meta.warnings.clear();
    for (const auto& issue : issues) {
        (issue.warning ? meta.warnings : meta.errors).push_back(project_form_issue(issue, locale));
    }
    return meta;
}

void project_form_result(FormResult& result, const std::vector<FormPendingIssue>& issues) {
    result.errors.clear();
    result.warnings.clear();
    for (const auto& entry : issues) {
        (entry.issue.warning ? result.warnings : result.errors)
            .push_back(
                {entry.name, project_form_issue(entry.issue, entry.field->presentation.get().locale), entry.path});
    }
}

void publish_form_issues(const std::shared_ptr<FormFieldState>& field, const std::vector<FormPendingIssue>& issues,
                         const FormNamePath& path) {
    auto meta = field->meta.get();
    meta.validating = false;
    meta.out_of_date = false;
    meta.errors.clear();
    meta.warnings.clear();
    std::vector<FormStoredIssue> sources;
    const auto locale = field->presentation.get().locale;
    for (const auto& entry : issues) {
        if (entry.path == path && entry.field == field) {
            sources.push_back(entry.issue);
            (entry.issue.warning ? meta.warnings : meta.errors).push_back(project_form_issue(entry.issue, locale));
        }
    }
    field->validated = true;
    // Publish together: observers can retire the registration at the batch boundary.
    batch([&] {
        field->issues.set(std::move(sources));
        field->error.set(meta.errors.empty() ? String{} : meta.errors.front());
        field->meta.set(meta);
    });
}

std::function<void()> form_blur_handler(const std::shared_ptr<FormFieldState>& field) {
    return form_blur_handler(field, {});
}

std::function<void()> form_blur_handler(const std::shared_ptr<FormFieldState>& field,
                                        const std::shared_ptr<FormControlConnection>& connection) {
    return [weak = std::weak_ptr{field}, generation = FormRegistrationScope::generation(field), connection] {
        const auto field = weak.lock();
        if (!field || !field->registered || field->generation != generation || form_field_disabled(field) ||
            form_field_read_only(field) || !form_control_event_admitted(field, connection)) {
            return;
        }
        if (const auto model = field->owner.lock();
            model && model->available() && (!field->eligible || field->eligible())) {
            model->schedule(field, true);
        }
    };
}

std::vector<FormStoredIssue> FormModel::validate_rules(const std::shared_ptr<FormFieldState>& field,
                                                       const FormValue& original, const FormData& snapshot) {
    std::vector<FormStoredIssue> result;
    const auto revision = revision_;
    const auto generation = field->generation;
    const auto registered = field->registered;
    const auto current = [&] {
        return alive && revision_ == revision && field->generation == generation && field->registered == registered;
    };
    const auto rules = field->rules;
    for (const auto& rule : rules) {
        const auto value = rule.transform ? rule.transform(original) : original;
        if (!current()) {
            return result;
        }
        const auto* string = std::get_if<String>(&value);
        const auto* array = std::get_if<SelectValues>(&value);
        const auto numeric = number(value);
        bool invalid = rule.required && empty_value(value);
        if (!empty_value(value)) {
            invalid |= rule.type == FormRuleType::String && !string;
            invalid |= rule.type == FormRuleType::Number && !numeric;
            invalid |= rule.type == FormRuleType::Boolean && !std::holds_alternative<bool>(value);
            invalid |= rule.type == FormRuleType::Array && !array;
            const auto scalars = string ? text::decode_utf8(string->view()) : std::vector<text::Utf8Scalar>{};
            const auto size = string ? scalars.size() : array ? array->size() : 0;
            invalid |= rule.length && size != *rule.length;
            invalid |= rule.min_length && size < *rule.min_length;
            invalid |= rule.max_length && size > *rule.max_length;
            invalid |=
                rule.whitespace && string && std::ranges::all_of(scalars, [](const auto& v) { return v.whitespace; });
            invalid |=
                rule.minimum && (!numeric || numeric->compare(*ExactDecimal::parse(rule.minimum->text().bytes())) < 0);
            invalid |=
                rule.maximum && (!numeric || numeric->compare(*ExactDecimal::parse(rule.maximum->text().bytes())) > 0);
            invalid |=
                !rule.enumeration.empty() && std::ranges::find(rule.enumeration, value) == rule.enumeration.end();
            invalid |= rule.pattern && (!string || !rule.pattern(*string));
            if (!current()) {
                return result;
            }
        }
        if (invalid) {
            result.push_back(rule.message ? FormStoredIssue{*rule.message, rule.warning_only}
                                          : FormStoredIssue{{}, rule.warning_only, FormMessageSource::Invalid});
        }
        if (rule.validator) {
            auto issues = rule.validator(value, {snapshot, validation_generation_, {}});
            if (!current()) {
                return result;
            }
            for (auto& issue : issues) {
                issue.warning |= rule.warning_only;
                if (!issue.message.empty()) {
                    result.push_back({std::move(issue.message), issue.warning, FormMessageSource::RuleValidator});
                }
            }
        }
        if (field->validate_first && std::ranges::any_of(result, [](const auto& issue) { return !issue.warning; })) {
            break;
        }
    }
    return result;
}

struct FormModel::ValidationRun final {
    std::uint64_t generation{};
    std::uint64_t revision{};
    std::vector<Registration> fields;
    std::vector<bool> eligibility;
    std::vector<Collection> collections;
    FormResult result;
    std::vector<FormPendingIssue> issues;
    FormData context_values;
    FormValidateOptions options;
    std::size_t remaining{};
    bool starting{true};
    bool submit{};
    std::function<void(FormResult)> complete;
};

bool FormModel::cancel_validation(bool notify) {
    const auto run = std::exchange(pending_, {});
    ++validation_generation_;
    if (!run) {
        return false;
    }
    run->result.out_of_date = true;
    project_form_result(run->result, run->issues);
    std::vector<AsyncOptionsCancel> cancellations;
    for (const auto& item : run->fields) {
        if (auto cancel = item.field->session->detach_cancel()) {
            cancellations.push_back(std::move(cancel));
        }
    }
    for (const auto& collection : run->collections) {
        if (collection.validation) {
            if (auto cancel = collection.validation->session->detach_cancel()) {
                cancellations.push_back(std::move(cancel));
            }
        }
    }
    std::exception_ptr publication_error;
    try {
        batch([&] {
            for (const auto& item : run->fields) {
                if (!item.field->registered || item.field->generation != item.generation) {
                    continue;
                }
                auto meta = form_field_meta(item.field);
                meta.validating = false;
                meta.out_of_date = true;
                item.field->timeout.reset();
                item.field->meta.set(meta);
            }
            for (const auto& collection : run->collections) {
                if (!collection.validation) {
                    continue;
                }
                auto meta = form_field_meta(collection.validation);
                meta.validating = false;
                meta.out_of_date = true;
                collection.validation->timeout.reset();
                collection.validation->meta.set(meta);
            }
        });
    } catch (...) {
        publication_error = std::current_exception();
    }
    for (auto& cancel : cancellations) {
        try {
            cancel();
        } catch (...) {
        }
    }
    if (notify && alive) {
        const auto callback = run->complete;
        if (callback) {
            callback(run->result);
        }
        if (alive && run->submit) {
            const auto callback = failed;
            if (callback) {
                callback(run->result);
            }
        }
    }
    if (publication_error) {
        std::rethrow_exception(publication_error);
    }
    return true;
}

bool FormModel::validate_async(std::function<void(FormResult)> callback, FormValidateOptions options, bool submit) {
    const auto keep = shared_from_this();
    if (busy || !available()) {
        return false;
    }
    cancel_validation();
    if (!available() || pending_) {
        return false;
    }
    if (clock) {
        now_ = std::max(now_, clock());
    }
    auto run = std::make_shared<ValidationRun>();
    run->generation = ++validation_generation_;
    run->revision = revision_;
    run->options = std::move(options);
    run->result.values = values();
    run->result.structure = get_values();
    run->context_values = get_values(true);
    run->complete = std::move(callback);
    run->submit = submit;
    for (const auto& item : fields_) {
        const bool named = run->options.names.empty() || std::ranges::any_of(run->options.names, [&](const auto& path) {
                               return run->options.recursive ? item.path.startsWith(path) : item.path == path;
                           });
        const auto meta = form_field_meta(item.field);
        if (named && (!run->options.dirty || meta.touched || item.field->validated || !meta.errors.empty() ||
                      !form_field_error(item.field).empty())) {
            run->fields.push_back(item);
            run->eligibility.push_back(!item.eligible || item.eligible());
        }
    }
    for (const auto& collection : collections) {
        if (!collection.active() || !collection.validation) {
            continue;
        }
        if (run->options.names.empty() || std::ranges::any_of(run->options.names, [&](const auto& path) {
                return run->options.recursive ? collection.path().startsWith(path) : collection.path() == path;
            })) {
            run->collections.push_back(collection);
        }
    }
    pending_ = run;
    try {
        // Legacy synchronous rules keep their exception and transaction semantics.
        busy = true;
        const auto snapshot = run->context_values;
        for (const auto& item : run->fields) {
            std::optional<FormStoredIssue> error;
            const auto value = read_form_field(item.field);
            if (item.field->required && empty_value(value)) {
                error = item.field->required_message ? FormStoredIssue{*item.field->required_message}
                                                     : FormStoredIssue{{}, false, FormMessageSource::Required};
            } else if (const auto validator = item.field->validator) {
                const auto message = validator(value);
                if (!message.empty()) {
                    error = FormStoredIssue{message, false, FormMessageSource::Validator};
                }
            }
            if (pending_ != run || !available() || revision_ != run->revision || !item.field->registered ||
                item.field->generation != item.generation || values() != run->result.values) {
                busy = false;
                if (pending_ == run) {
                    cancel_validation();
                }
                return alive;
            }
            if (error) {
                run->issues.push_back({item.name, item.path, item.field, *error});
            }
            for (auto issue : (error && item.field->validate_first ? std::vector<FormStoredIssue>{}
                                                                   : validate_rules(item.field, value, snapshot))) {
                run->issues.push_back({item.name, item.path, item.field, std::move(issue)});
            }
            project_form_result(run->result, run->issues);
            if (pending_ != run || !available() || revision_ != run->revision || values() != run->result.values) {
                busy = false;
                if (pending_ == run) {
                    cancel_validation();
                }
                return alive;
            }
        }
        for (const auto& collection : run->collections) {
            for (auto issue : validate_rules(collection.validation, FormValue{String{}}, snapshot)) {
                run->issues.push_back({{}, collection.path(), collection.validation, std::move(issue)});
            }
            project_form_result(run->result, run->issues);
            if (pending_ != run || !available() || revision_ != run->revision || values() != run->result.values) {
                busy = false;
                if (pending_ == run) {
                    cancel_validation();
                }
                return alive;
            }
        }
        busy = false;
        // An explicit validation supersedes already scheduled work for its snapshot,
        // including synchronous dependencies. Detach deadlines before meta observers can re-enter.
        for (const auto& item : run->fields) {
            const bool starts_async =
                item.field->async &&
                !(item.field->validate_first &&
                  std::ranges::any_of(run->result.errors, [&](const auto& error) { return error.path == item.path; }));
            if (!run->options.validate_only || starts_async) {
                item.field->due.reset();
            }
        }
        batch([&] {
            for (const auto& collection : run->collections) {
                if (!collection.validation->async) {
                    continue;
                }
                ++run->remaining;
                collection.validation->timeout = now_ + animation::AnimationDuration::milliseconds(30000);
                if (!run->options.validate_only) {
                    auto meta = form_field_meta(collection.validation);
                    meta.validating = true;
                    meta.out_of_date = false;
                    collection.validation->meta.set(meta);
                }
            }
            for (const auto& item : run->fields) {
                if (!item.field->async ||
                    (item.field->validate_first && std::ranges::any_of(run->result.errors, [&](const auto& error) {
                         return error.path == item.path;
                     }))) {
                    continue;
                }
                ++run->remaining;
                item.field->timeout = now_ + animation::AnimationDuration::milliseconds(30000);
                if (!run->options.validate_only) {
                    auto meta = form_field_meta(item.field);
                    meta.validating = true;
                    meta.out_of_date = false;
                    item.field->meta.set(meta);
                }
            }
        });
        for (const auto& item : run->fields) {
            if (pending_ != run) {
                return alive;
            }
            if (!item.field->async ||
                (item.field->validate_first &&
                 std::ranges::any_of(run->result.errors, [&](const auto& error) { return error.path == item.path; }))) {
                continue;
            }
            const auto factory = item.field->async;
            const auto source = factory(read_form_field(item.field), {snapshot, run->generation, {}});
            if (pending_ != run || !alive || !item.field->registered || item.field->generation != item.generation ||
                item.field->owner.lock().get() != this) {
                return alive;
            }
            item.field->session->start(
                source, {}, [weak = weak_from_this(), run, item](AsyncOptionsResult<FormIssue> result) mutable {
                    const auto model = weak.lock();
                    if (!model || model->pending_ != run || !item.field->registered ||
                        item.field->generation != item.generation) {
                        return;
                    }
                    if (!result.error.empty()) {
                        result.options.push_back({result.error, false});
                    }
                    for (auto& issue : result.options) {
                        if (!issue.message.empty()) {
                            run->issues.push_back(
                                {item.name,
                                 item.path,
                                 item.field,
                                 {std::move(issue.message), issue.warning, FormMessageSource::AsyncValidator}});
                        }
                    }
                    item.field->timeout.reset();
                    if (run->remaining) {
                        --run->remaining;
                    }
                    if (!run->starting && !run->remaining) {
                        model->complete_async(run);
                    }
                });
        }
        for (const auto& collection : run->collections) {
            if (pending_ != run) {
                return alive;
            }
            if (!collection.validation->async) {
                continue;
            }
            const auto factory = collection.validation->async;
            const auto source = factory(FormValue{String{}}, {snapshot, run->generation, {}});
            if (pending_ != run || !alive || !collection.active()) {
                return alive;
            }
            collection.validation->session->start(
                source, {}, [weak = weak_from_this(), run, collection](AsyncOptionsResult<FormIssue> result) mutable {
                    const auto model = weak.lock();
                    if (!model || model->pending_ != run || !collection.active()) {
                        return;
                    }
                    if (!result.error.empty()) {
                        result.options.push_back({result.error, false});
                    }
                    for (auto& issue : result.options) {
                        if (!issue.message.empty()) {
                            run->issues.push_back(
                                {{},
                                 collection.path(),
                                 collection.validation,
                                 {std::move(issue.message), issue.warning, FormMessageSource::AsyncValidator}});
                        }
                    }
                    collection.validation->timeout.reset();
                    if (run->remaining) {
                        --run->remaining;
                    }
                    if (!run->starting && !run->remaining) {
                        model->complete_async(run);
                    }
                });
        }
        run->starting = false;
        if (!run->remaining) {
            complete_async(run);
        }
        if (wake) {
            wake();
        }
    } catch (...) {
        busy = false;
        if (pending_ == run) {
            cancel_validation(false);
        }
        throw;
    }
    return alive;
}

void FormModel::complete_async(const std::shared_ptr<ValidationRun>& run) {
    const auto keep = shared_from_this();
    if (pending_ != run) {
        return;
    }
    bool eligibility_changed{};
    for (std::size_t i = 0; i < run->fields.size(); ++i) {
        const auto& field = run->fields[i];
        eligibility_changed |= run->eligibility[i] != (!field.eligible || field.eligible());
    }
    if (!available() || eligibility_changed || revision_ != run->revision || values() != run->result.values) {
        cancel_validation();
        return;
    }
    pending_.reset();
    for (const auto& item : run->fields) {
        item.field->validated = true;
    }
    busy = true;
    try {
        if (!run->options.validate_only) {
            batch([&] {
                for (const auto& item : run->fields) {
                    publish_form_issues(item.field, run->issues, item.path);
                }
            });
        }
        if (!run->options.validate_only) {
            for (const auto& collection : run->collections) {
                if (!alive || revision_ != run->revision || !collection.active()) {
                    continue;
                }
                publish_form_issues(collection.validation, run->issues, collection.path());
            }
        }
        project_form_result(run->result, run->issues);
        run->result.out_of_date = !available() || revision_ != run->revision || values() != run->result.values;
        if (run->result.out_of_date) {
            for (const auto& item : run->fields) {
                if (!item.field->registered || item.field->generation != item.generation ||
                    item.field->owner.lock().get() != this) {
                    continue;
                }
                item.field->error.set({});
                auto meta = form_field_meta(item.field);
                meta.errors.clear();
                meta.warnings.clear();
                meta.out_of_date = true;
                item.field->meta.set(meta);
            }
        }
        if (alive && run->complete) {
            run->complete(run->result);
        }
        if (alive && run->submit) {
            if (!run->result.valid() && !run->result.out_of_date && focus_first_invalid) {
                for (const auto& item : run->fields) {
                    if (!form_field_disabled(item.field) && !form_field_read_only(item.field) && item.focus &&
                        std::ranges::any_of(run->result.errors, [&](const auto& e) { return e.path == item.path; }) &&
                        (!item.eligible || item.eligible()) &&
                        (scroll_first && scroll ? scroll(item.path, {}) : item.focus())) {
                        break;
                    }
                }
            }
            run->result.out_of_date |= !available() || revision_ != run->revision || values() != run->result.values;
            const auto callback = run->result.valid() ? finish : failed;
            if (alive && callback) {
                callback(run->result);
            }
        }
    } catch (...) {
        busy = false;
        throw;
    }
    busy = false;
}

void FormModel::schedule(const std::shared_ptr<FormFieldState>& field, bool blur) {
    if (!alive || !field->registered || field->owner.lock().get() != this || !available()) {
        return;
    }
    if (clock) {
        now_ = std::max(now_, clock());
    }
    const auto trigger = field->trigger;
    if ((blur && (trigger == FormValidateTrigger::Blur || trigger == FormValidateTrigger::ChangeAndBlur)) ||
        (!blur && (trigger == FormValidateTrigger::Change || trigger == FormValidateTrigger::ChangeAndBlur))) {
        field->due = now_ + animation::AnimationDuration::milliseconds(field->debounce_ms);
        if (wake) {
            wake();
        }
    }
}

void FormModel::boundary_changed() {
    bool changed{};
    const auto run = pending_;
    if (run) {
        for (std::size_t i = 0; i < run->fields.size(); ++i) {
            const auto& field = run->fields[i];
            changed |= run->eligibility[i] != (!field.eligible || field.eligible());
        }
    }
    if (!available() || changed) {
        cancel_validation();
        for (const auto& item : fields_) {
            item.field->due.reset();
        }
    }
}

std::size_t FormModel::tick(animation::AnimationTime now) {
    now_ = std::max(now_, now);
    boundary_changed();
    if (!available()) {
        return 0;
    }
    for (const auto& item : fields_) {
        if (item.field->timeout && *item.field->timeout <= now_) {
            cancel_validation();
            return 1;
        }
    }
    for (const auto& collection : collections) {
        if (collection.active() && collection.validation && collection.validation->timeout &&
            *collection.validation->timeout <= now_) {
            cancel_validation();
            return 1;
        }
    }
    FormValidateOptions options;
    for (const auto& item : fields_) {
        if (item.field->due && *item.field->due <= now_) {
            item.field->due.reset();
            options.names.push_back(item.path);
        }
    }
    if (options.names.empty()) {
        return 0;
    }
    return validate_async({}, std::move(options)) ? 1 : 0;
}

std::optional<animation::AnimationTime> FormModel::deadline() const {
    std::optional<animation::AnimationTime> result;
    for (const auto& item : fields_) {
        for (const auto& value : {item.field->due, item.field->timeout}) {
            if (value && (!result || *value < *result)) {
                result = value;
            }
        }
    }
    for (const auto& collection : collections) {
        if (collection.active() && collection.validation && collection.validation->timeout &&
            (!result || *collection.validation->timeout < *result)) {
            result = collection.validation->timeout;
        }
    }
    return result;
}

std::vector<FormFieldData> FormModel::get_fields() const {
    static_cast<void>(topology.get());
    std::vector<FormFieldData> result;
    const auto fields = fields_;
    for (const auto& item : fields) {
        result.push_back({item.path, read_form_field(item.field), form_field_meta(item.field)});
    }
    return result;
}

bool FormModel::set_fields(const std::vector<FormFieldData>& data) {
    FormData updates;
    std::vector<Registration> selected;
    for (std::size_t i = 0; i < data.size(); ++i) {
        const auto found = std::ranges::find(fields_, data[i].path, &Registration::path);
        if (found == fields_.end() ||
            std::any_of(data.begin(), data.begin() + i, [&](const auto& v) { return v.path == data[i].path; })) {
            throw std::invalid_argument("Form setFields needs unique registered paths");
        }
        selected.push_back(*found);
        if (data[i].value) {
            updates.set(data[i].path, FormData{*data[i].value});
        }
    }
    if (!set_values(updates)) {
        return false;
    }
    try {
        batch([&] {
            for (std::size_t i = 0; i < selected.size(); ++i) {
                const auto& item = selected[i];
                if (!alive || !item.field->registered || item.field->generation != item.generation ||
                    item.field->owner.lock().get() != this) {
                    throw std::runtime_error("Form metadata update lost its registration authority");
                }
                auto meta = data[i].meta;
                meta.validating = false;
                item.field->issues.set({});
                item.field->meta.set(meta);
                item.field->error.set(meta.errors.empty() ? String{} : meta.errors.front());
            }
        });
    } catch (...) {
        const auto action = retire;
        if (alive && action) {
            action();
        }
        if (alive) {
            dispose();
        }
        throw;
    }
    return alive;
}

} // namespace ryn::detail

namespace ryn {
bool FormRef::validateAsync(std::function<void(FormResult)> callback, FormValidateOptions options) const {
    const auto model = detail::resolve_form_model(state_);
    return model && model->validate_async(std::move(callback), std::move(options));
}

bool FormRef::submitAsync() const {
    const auto model = detail::resolve_form_model(state_);
    return model && model->validate_async({}, {}, true);
}

bool FormRef::cancelValidation() const {
    const auto model = detail::resolve_form_model(state_);
    return model && model->cancel_validation();
}

std::vector<FormFieldData> FormRef::getFields() const {
    const auto model = detail::resolve_form_model(state_);
    return model && model->alive ? model->get_fields() : std::vector<FormFieldData>{};
}

bool FormRef::setFields(std::vector<FormFieldData> fields) const {
    const auto model = detail::resolve_form_model(state_);
    return model && model->set_fields(fields);
}

bool FormRef::isFieldTouched(const FormNamePath& path) const {
    const auto model = detail::resolve_form_model(state_);
    const auto meta = model && model->alive ? model->get_meta(path) : std::nullopt;
    return meta ? meta->touched : false;
}

bool FormRef::isFieldValidating(const FormNamePath& path) const {
    const auto model = detail::resolve_form_model(state_);
    const auto meta = model && model->alive ? model->get_meta(path) : std::nullopt;
    return meta ? meta->validating : false;
}
} // namespace ryn

namespace ryn::detail {
InputStatus form_field_status(const std::shared_ptr<FormFieldState>& field) {
    if (const auto status = field->status_override.get()) {
        return *status == FormValidateStatus::Error     ? InputStatus::Error
               : *status == FormValidateStatus::Warning ? InputStatus::Warning
                                                        : InputStatus::Default;
    }
    const auto meta = form_field_meta(field);
    return !form_field_error(field).empty() || !meta.errors.empty() ? InputStatus::Error
           : !meta.warnings.empty()                                 ? InputStatus::Warning
                                                                    : InputStatus::Default;
}
} // namespace ryn::detail

namespace ryn {
std::vector<String> FormRef::getFieldError(const FormNamePath& path) const {
    const auto model = detail::resolve_form_model(state_);
    const auto meta = model && model->alive ? model->get_meta(path) : std::nullopt;
    return meta ? meta->errors : std::vector<String>{};
}

std::vector<String> FormRef::getFieldWarning(const FormNamePath& path) const {
    const auto model = detail::resolve_form_model(state_);
    const auto meta = model && model->alive ? model->get_meta(path) : std::nullopt;
    return meta ? meta->warnings : std::vector<String>{};
}

bool FormRef::isFieldsTouched(const std::vector<FormNamePath>& paths, bool all) const {
    const auto fields = getFields();
    bool any{};
    for (const auto& field : fields) {
        if (!paths.empty() && std::ranges::find(paths, field.path) == paths.end()) {
            continue;
        }
        if (all && !field.meta.touched) {
            return false;
        }
        any |= field.meta.touched;
    }
    return any;
}

bool FormRef::isFieldsValidating(const std::vector<FormNamePath>& paths) const {
    const auto fields = getFields();
    return std::ranges::any_of(fields, [&](const auto& field) {
        return (paths.empty() || std::ranges::find(paths, field.path) != paths.end()) && field.meta.validating;
    });
}
} // namespace ryn
