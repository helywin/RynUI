#include "component/form_model.hpp"

#include <algorithm>
#include <cmath>
#include <stdexcept>

namespace ryn::detail {
namespace {
void merge_data(FormData& target, const FormData& source) {
    if (const auto* object = std::get_if<FormData::Object>(&source.value)) {
        if (std::holds_alternative<std::monostate>(target.value)) {
            target.value = FormData::Object{};
        }
        auto* destination = std::get_if<FormData::Object>(&target.value);
        if (!destination) {
            target = source;
            return;
        }
        for (const auto& [key, value] : *object) {
            auto found = std::ranges::find(*destination, key, &FormData::Object::value_type::first);
            if (found == destination->end()) {
                destination->emplace_back(key, value);
            } else {
                merge_data(found->second, value);
            }
        }
    } else if (!std::holds_alternative<std::monostate>(source.value)) {
        target = source;
    }
}

void erase_data(FormData& data, const FormNamePath& path, std::size_t depth = 0) {
    if (depth == path.segments().size()) {
        data = {};
        return;
    }
    const auto& segment = path.segments()[depth].value();
    if (const auto* key = std::get_if<String>(&segment)) {
        if (auto* object = std::get_if<FormData::Object>(&data.value)) {
            const auto found = std::ranges::find(*object, *key, &FormData::Object::value_type::first);
            if (found != object->end()) {
                if (depth + 1 == path.segments().size()) {
                    object->erase(found);
                } else {
                    erase_data(found->second, path, depth + 1);
                }
            }
        }
    } else if (auto* array = std::get_if<FormData::Array>(&data.value)) {
        const auto index = std::get<std::size_t>(segment);
        if (index < array->size()) {
            erase_data((*array)[index], path, depth + 1);
        }
    }
}

void valid_value(const FormValue& value) {
    if (const auto* number = std::get_if<std::optional<double>>(&value);
        number && *number && !std::isfinite(**number)) {
        throw std::invalid_argument("Form numeric values must be finite");
    }
}

bool empty(const FormValue& value) {
    return std::visit(
        [](const auto& item) {
            using T = std::decay_t<decltype(item)>;
            if constexpr (std::same_as<T, String>) {
                return item.empty();
            } else if constexpr (std::same_as<T, bool>) {
                return !item;
            } else if constexpr (std::same_as<T, SelectValues>) {
                return item.empty();
            } else {
                return !item.has_value();
            }
        },
        value);
}

struct Busy final {
    explicit Busy(FormModel& model) : model(model) {
        model.busy = true;
    }

    ~Busy() {
        model.busy = false;
    }

    FormModel& model;
};

void discard_errors(const std::vector<FormModel::Registration>& fields) {
    batch([&] {
        for (const auto& item : fields) {
            if (item.field->registered && item.field->generation == item.generation) {
                item.field->error.set(String{});
                auto meta = item.field->meta.get();
                meta.errors.clear();
                meta.warnings.clear();
                meta.out_of_date = true;
                item.field->meta.set(meta);
            }
        }
    });
}
} // namespace

thread_local FormRegistrationScope* FormRegistrationScope::current_{};

FormRegistrationScope::FormRegistrationScope(const std::shared_ptr<FormFieldState>& field, std::uint64_t generation)
    : field_(field.get()), generation_(generation), previous_(current_) {
    current_ = this;
}

FormRegistrationScope::~FormRegistrationScope() {
    current_ = previous_;
}

std::uint64_t FormRegistrationScope::generation(const std::shared_ptr<FormFieldState>& field) {
    for (auto* scope = current_; scope; scope = scope->previous_) {
        if (scope->field_ == field.get()) {
            return scope->generation_;
        }
    }
    return field->generation;
}

std::shared_ptr<FormFieldState> make_form_field(FormValue initial, std::optional<Binding<FormValue>> source,
                                                std::function<void(FormValue)> change) {
    valid_value(initial);
    auto state = std::make_shared<FormFieldState>(std::move(initial));
    state->source = std::move(source);
    state->change = std::move(change);
    return state;
}

FormValue read_form_field(const std::shared_ptr<FormFieldState>& state) {
    auto result = state->source ? state->source->get() : state->value.get();
    valid_value(result);
    if (result.index() != state->initial.index()) {
        throw std::invalid_argument("Form field value type changed");
    }
    return result;
}

void write_form_field(const std::shared_ptr<FormFieldState>& state, FormValue value) {
    valid_value(value);
    if (value.index() != state->initial.index()) {
        throw std::invalid_argument("Form field value type mismatch");
    }
    if (state->source) {
        const auto callback = state->change;
        if (callback) {
            try {
                callback(std::move(value));
            } catch (...) {
                const auto error = std::current_exception();
                if (const auto owner = state->owner.lock()) {
                    try {
                        owner->cancel_validation();
                    } catch (...) {
                    }
                }
                std::rethrow_exception(error);
            }
        }
    } else {
        if (state->value.get() == value) {
            return;
        }
        // Invalidate the transaction before observers or consumer callbacks can run.
        const auto owner = state->owner.lock();
        const auto generation = state->generation;
        const bool registered = state->registered;
        batch([&] {
            if (owner) {
                owner->value_changed(state);
            }
            if (state->generation == generation && state->registered == registered && state->owner.lock() == owner) {
                state->value.set(std::move(value));
            }
        });
    }
}

void configure_form_required(const std::shared_ptr<FormFieldState>& state, String message) {
    if (state->registered || message.empty()) {
        throw std::invalid_argument("Form required rule needs an unregistered field and a nonempty message");
    }
    state->required = true;
    state->required_message = std::move(message);
}

void configure_form_validator(const std::shared_ptr<FormFieldState>& state,
                              std::function<String(const FormValue&)> validator) {
    if (state->registered) {
        throw std::invalid_argument("Form rules must be configured before registration");
    }
    state->validator = std::move(validator);
}

std::function<void(FormValue)> form_change_handler(const std::shared_ptr<FormFieldState>& state) {
    const auto generation = FormRegistrationScope::generation(state);
    return [weak = std::weak_ptr{state}, generation](FormValue value) {
        const auto current = weak.lock();
        if (!current || !current->registered || current->generation != generation || form_field_disabled(current) ||
            form_field_read_only(current)) {
            return;
        }
        const auto owner = current->owner.lock();
        const auto eligible = current->eligible;
        if (!owner || !owner->available() || (eligible && !eligible())) {
            return;
        }
        const auto authorized = [&] {
            return current->registered && current->generation == generation && current->owner.lock() == owner &&
                   owner->alive;
        };
        try {
            auto meta = current->meta.get();
            meta.touched = true;
            current->meta.set(meta);
            if (!authorized()) {
                return;
            }
            const auto previous = read_form_field(current);
            if (!authorized()) {
                return;
            }
            write_form_field(current, std::move(value));
            if (authorized() && previous != read_form_field(current) && authorized()) {
                owner->notify_change(current);
            }
        } catch (...) {
            // Consumer observers/setters can already have changed other fields.
            // Retire this exact Form rather than leave a partially published UI.
            const auto error = std::current_exception();
            try {
                const auto action = owner->retire;
                if (owner->alive && action) {
                    action();
                }
            } catch (...) {
            }
            if (owner->alive) {
                try {
                    owner->dispose();
                } catch (...) {
                }
            }
            std::rethrow_exception(error);
        }
    };
}

String form_field_error(const std::shared_ptr<FormFieldState>& state) {
    return state->error.get();
}

bool form_field_disabled(const std::shared_ptr<FormFieldState>& state) {
    const bool local = state->disabled.get();
    const auto owner = state->owner.lock();
    const bool parent = owner && owner->disabled.get();
    return local || parent;
}

bool form_field_read_only(const std::shared_ptr<FormFieldState>& state) {
    const bool local = state->read_only.get();
    const auto owner = state->owner.lock();
    const bool parent = owner && owner->read_only.get();
    return local || parent;
}

std::uint64_t FormModel::add(String name, const std::shared_ptr<FormFieldState>& field,
                             std::function<bool()> eligibility, std::function<bool()> focus,
                             std::function<void()> reset) {
    const auto path = FormNamePath{name};
    return add(path, field, std::move(eligibility), std::move(focus), std::move(reset));
}

std::uint64_t FormModel::add(FormNamePath path, const std::shared_ptr<FormFieldState>& field,
                             std::function<bool()> eligibility, std::function<bool()> focus,
                             std::function<void()> reset) {
    if (!alive || !field || path.empty() || field->registered ||
        std::ranges::any_of(
            collections,
            [&](const auto& collection) { return collection.active() && collection.path().startsWith(path); }) ||
        std::any_of(fields_.begin(), fields_.end(),
                    [&](const auto& item) { return item.path.startsWith(path) || path.startsWith(item.path); })) {
        throw std::invalid_argument("Form paths must be nonempty, disjoint and uniquely registered");
    }
    const auto revision = revision_;
    FormData preflight = get_values(true);
    if (const auto* prior = preflight.at(path)) {
        if (const auto* value = std::get_if<FormValue>(&prior->value)) {
            if (value->index() != field->initial.index()) {
                throw std::invalid_argument("Stored Form field type mismatch");
            }
            if (!field->source) {
                field->value.set(*value);
            }
        }
    }
    preflight.set(path, FormData{read_form_field(field)});
    if (!alive || revision != revision_ || field->registered) {
        throw std::runtime_error("Form registration was superseded during value inspection");
    }
    const auto generation = ++field->generation;
    fields_.push_back({path.segments().size() == 1 && std::holds_alternative<String>(path.segments().front().value())
                           ? std::get<String>(path.segments().front().value())
                           : String{},
                       field, generation, eligibility, std::move(focus), std::move(reset), std::move(path)});
    std::erase_if(retained_, [&](const auto& item) { return item.path == fields_.back().path; });
    field->owner = shared_from_this();
    field->registered = true;
    field->eligible = std::move(eligibility);
    ++revision_;
    try {
        cancel_validation();
        if (!alive || !field->registered || field->generation != generation || field->owner.lock().get() != this) {
            throw std::runtime_error("Form registration was superseded during notification");
        }
        batch([&] {
            field->error.set(String{});
            field->meta.set({});
            field->status_override.set(std::nullopt);
            field->display_status.set(FormValidateStatus::None);
            topology.set(topology.get() + 1);
        });
        if (!alive || !field->registered || field->generation != generation || field->owner.lock().get() != this) {
            throw std::runtime_error("Form registration was superseded during notification");
        }
    } catch (...) {
        const auto error = std::current_exception();
        try {
            remove(field, generation);
        } catch (...) {
        }
        std::rethrow_exception(error);
    }
    return generation;
}

void FormModel::remove(const std::shared_ptr<FormFieldState>& field, std::uint64_t generation) {
    if (!field->registered || field->generation != generation || field->owner.lock().get() != this) {
        return;
    }
    // Retire identity before any cancellation or reactive observer can register this handle again.
    std::vector<Registration> removed;
    for (const auto& item : fields_) {
        if (item.field == field && item.generation == generation) {
            removed.push_back(item);
            if (field->preserve) {
                retained_.push_back(item);
            }
        }
    }
    std::erase_if(fields_, [&](const auto& item) { return item.field == field && item.generation == generation; });
    field->registered = false;
    field->owner.reset();
    field->eligible = {};
    field->due.reset();
    field->timeout.reset();
    field->validated = false;
    auto cancel = field->session->detach_cancel();
    ++revision_;
    std::exception_ptr error;
    try {
        cancel_validation();
    } catch (...) {
        error = std::current_exception();
    }
    if (cancel) {
        try {
            cancel();
        } catch (...) {
        }
    }
    try {
        batch([&] {
            if (!field->registered && field->generation == generation) {
                field->meta.set({});
                field->status_override.set(std::nullopt);
                field->display_status.set(FormValidateStatus::None);
                field->error.set(String{});
                if (!field->preserve) {
                    if (!field->source) {
                        field->value.set(field->initial);
                    }
                    auto stored = stored_values.get();
                    for (const auto& item : removed) {
                        erase_data(stored, item.path);
                    }
                    stored_values.set(std::move(stored));
                }
            }
            topology.set(topology.get() + 1);
        });
    } catch (...) {
        if (!error) {
            error = std::current_exception();
        }
    }
    if (error) {
        std::rethrow_exception(error);
    }
}

void FormModel::dispose() {
    alive = false;
    try {
        cancel_validation(false);
    } catch (...) {
    }
    wake = {};
    retire = {};
    const auto fields = fields_;
    for (const auto& item : fields) {
        try {
            remove(item.field, item.generation);
        } catch (...) {
        }
    }
    if (clear_on_destroy) {
        for (const auto& item : retained_) {
            if (!item.field->registered && !item.field->source) {
                try {
                    item.field->value.set(item.field->initial);
                } catch (...) {
                }
            }
        }
        retained_.clear();
        try {
            stored_values.set({});
        } catch (...) {
        }
    }
    eligible = {};
    finish = {};
    failed = {};
    changed = {};
    fields_changed = {};
    scroll = {};
    control = {};
    clock = {};
}

void FormModel::value_changed(const std::shared_ptr<FormFieldState>& field) {
    const auto generation = field->generation;
    ++revision_;
    cancel_validation();
    if (!alive || !field->registered || field->owner.lock().get() != this || field->generation != generation) {
        return;
    }
    field->error.set(String{});
    auto meta = field->meta.get();
    meta.errors.clear();
    meta.warnings.clear();
    field->meta.set(meta);
    schedule(field);
    const auto changed = std::ranges::find(fields_, field, &Registration::field);
    if (changed != fields_.end()) {
        const auto path = changed->path;
        for (const auto& item : fields_) {
            if (std::ranges::any_of(item.field->dependencies,
                                    [&](const auto& dependency) { return path.startsWith(dependency); })) {
                item.field->due = now_ + animation::AnimationDuration::milliseconds(item.field->debounce_ms);
                if (wake) {
                    wake();
                }
            }
        }
    }
}

bool FormModel::available() const {
    const auto check = eligible;
    return alive && !disabled.get() && (!check || check());
}

std::vector<FormFieldValue> FormModel::values() const {
    std::vector<FormFieldValue> result;
    const auto fields = fields_;
    result.reserve(fields.size());
    for (const auto& item : fields) {
        result.push_back({item.name, read_form_field(item.field), item.path});
    }
    return result;
}

FormResult FormModel::validate_transaction() {
    const auto revision = revision_;
    const auto fields = fields_;
    FormResult result{values(), {}, false};
    result.structure = get_values();
    const auto validation_values = get_values(true);
    if (!alive || revision != revision_ || result.values.size() != fields.size()) {
        result.out_of_date = true;
        return result;
    }
    for (std::size_t i = 0; i < fields.size(); ++i) {
        const auto& field = fields[i].field;
        const auto& value = result.values[i].value;
        String error;
        if (field->required && empty(value)) {
            error = field->required_message == String{u8"This field is required"}
                        ? field->presentation.get().locale.required
                        : field->required_message;
        } else if (const auto validator = field->validator) {
            error = validator(value);
        }
        const auto issues = !error.empty() && field->validate_first ? std::vector<FormIssue>{}
                                                                    : validate_rules(field, value, validation_values);
        for (const auto& issue : issues) {
            (issue.warning ? result.warnings : result.errors)
                .push_back({fields[i].name, issue.message, fields[i].path});
        }
        if (!alive || revision != revision_ || values() != result.values || !available()) {
            result.out_of_date = true;
            return result;
        }
        if (!error.empty()) {
            result.errors.push_back({fields[i].name, std::move(error), fields[i].path});
        }
    }
    const auto collections_snapshot = collections;
    for (const auto& collection : collections_snapshot) {
        if (!collection.active() || !collection.validation) {
            continue;
        }
        for (const auto& issue : validate_rules(collection.validation, FormValue{String{}}, validation_values)) {
            (issue.warning ? result.warnings : result.errors).push_back({{}, issue.message, collection.path()});
        }
        if (!alive || revision != revision_ || values() != result.values || !available()) {
            result.out_of_date = true;
            return result;
        }
    }
    // Publish only after all user validators have completed against the same snapshot.
    batch([&] {
        for (const auto& item : fields) {
            const auto error = std::find_if(result.errors.begin(), result.errors.end(),
                                            [&](const auto& value) { return value.path == item.path; });
            item.field->error.set(error == result.errors.end() ? String{} : error->message);
            auto meta = item.field->meta.get();
            meta.validating = false;
            meta.out_of_date = false;
            meta.errors.clear();
            meta.warnings.clear();
            for (const auto& error : result.errors) {
                if (error.path == item.path) {
                    meta.errors.push_back(error.message);
                }
            }
            for (const auto& warning : result.warnings) {
                if (warning.path == item.path) {
                    meta.warnings.push_back(warning.message);
                }
            }
            item.field->validated = true;
            item.field->meta.set(meta);
        }
    });
    for (const auto& collection : collections_snapshot) {
        if (!collection.active() || !collection.validation) {
            continue;
        }
        FormFieldMeta meta;
        for (const auto& error : result.errors) {
            if (error.path == collection.path()) {
                meta.errors.push_back(error.message);
            }
        }
        for (const auto& warning : result.warnings) {
            if (warning.path == collection.path()) {
                meta.warnings.push_back(warning.message);
            }
        }
        collection.validation->meta.set(meta);
    }
    result.out_of_date = !alive || revision != revision_ || values() != result.values || !available();
    if (result.out_of_date) {
        discard_errors(fields);
    }
    return result;
}

std::optional<FormResult> FormModel::validate() {
    const auto keep = shared_from_this();
    if (busy || pending_ || !available() ||
        (std::ranges::any_of(fields_, [](const auto& item) { return bool(item.field->async); }) ||
         std::ranges::any_of(collections,
                             [](const auto& c) { return c.active() && c.validation && bool(c.validation->async); }))) {
        return {};
    }
    Busy guard{*this};
    return validate_transaction();
}

bool FormModel::submit() {
    if ((std::ranges::any_of(fields_, [](const auto& item) { return bool(item.field->async); }) ||
         std::ranges::any_of(collections,
                             [](const auto& c) { return c.active() && c.validation && bool(c.validation->async); }))) {
        return validate_async({}, {}, true);
    }
    const auto keep = shared_from_this();
    if (busy || !available()) {
        return false;
    }
    Busy guard{*this};
    auto result = validate_transaction();
    if (!alive || !available()) {
        return false;
    }
    if (!result.valid() && !result.out_of_date && focus_first_invalid) {
        const auto revision = revision_;
        const auto fields = fields_;
        for (const auto& item : fields) {
            if (std::none_of(result.errors.begin(), result.errors.end(),
                             [&](const auto& e) { return e.path == item.path; }) ||
                form_field_disabled(item.field) || form_field_read_only(item.field) || !item.focus ||
                (item.eligible && !item.eligible())) {
                continue;
            }
            const bool focused = scroll_first && scroll ? scroll(item.path, {}) : item.focus();
            if (!alive || !available()) {
                return false;
            }
            if (revision != revision_ || values() != result.values) {
                result.out_of_date = true;
                discard_errors(fields);
                break;
            }
            if (focused) {
                break;
            }
        }
    }
    if (!alive || !available()) {
        return false;
    }
    const auto callback = result.valid() ? finish : failed;
    if (callback) {
        callback(std::move(result));
    }
    return true;
}

bool FormModel::reset() {
    const auto keep = shared_from_this();
    if (!alive || busy) {
        return false;
    }
    cancel_validation();
    ++revision_;
    if (!alive) {
        return false;
    }
    Busy guard{*this};
    stored_values.set(initial_values);
    const auto hooks = reset_hooks;
    for (const auto& hook : hooks) {
        if (alive && hook) {
            hook();
        }
    }
    const auto fields = fields_;
    for (const auto& item : fields) {
        if (!alive) {
            return false;
        }
        if (!item.field->registered || item.field->generation != item.generation) {
            continue;
        }
        batch([&] {
            item.field->error.set(String{});
            item.field->meta.set({});
            item.field->validated = false;
            item.field->due.reset();
        });
        if (!alive) {
            return false;
        }
        if (!item.field->registered || item.field->generation != item.generation) {
            continue;
        }
        write_form_field(item.field, item.field->initial);
        if (alive && item.field->registered && item.field->generation == item.generation && item.reset) {
            item.reset();
        }
    }
    return alive;
}
} // namespace ryn::detail

namespace ryn {
FormRef::FormRef() : state_(std::make_shared<detail::FormRefState>()) {}

bool FormRef::bound() const {
    if (!state_) {
        return false;
    }
    const auto model = detail::resolve_form_model(state_);
    return model && model->alive;
}

std::optional<FormResult> FormRef::validate() const {
    if (!state_) {
        return {};
    }
    const auto model = detail::resolve_form_model(state_);
    return model ? model->validate() : std::nullopt;
}

bool FormRef::submit() const {
    if (!state_) {
        return false;
    }
    const auto model = detail::resolve_form_model(state_);
    return model && model->submit();
}

bool FormRef::reset() const {
    if (!state_) {
        return false;
    }
    const auto model = detail::resolve_form_model(state_);
    return model && model->reset();
}
} // namespace ryn

namespace ryn::detail {
std::optional<FormValue> FormModel::get(const FormNamePath& path) const {
    static_cast<void>(topology.get());
    const auto it = std::ranges::find(fields_, path, &Registration::path);
    const auto field = it == fields_.end() ? nullptr : it->field;
    if (field) {
        return read_form_field(field);
    }
    const auto stored = stored_values.get();
    const auto* node = stored.at(path);
    const auto* value = node ? std::get_if<FormValue>(&node->value) : nullptr;
    return value ? std::optional{*value} : std::nullopt;
}

FormData FormModel::get_values(bool preserve) const {
    static_cast<void>(topology.get());
    FormData result = preserve ? stored_values.get() : FormData{};
    const auto shapes = collections;
    const auto stored = shapes.empty() ? FormData{} : stored_values.get();
    const auto retained = retained_;
    const auto fields = fields_;
    for (const auto& collection : shapes) {
        if (collection.active()) {
            auto rows = FormData::Array(collection.size());
            const auto* node = result.at(collection.path());
            if (!node) {
                node = stored.at(collection.path());
            }
            if (node) {
                if (const auto* array = std::get_if<FormData::Array>(&node->value)) {
                    rows = *array;
                    rows.resize(collection.size());
                }
            }
            result.set(collection.path(), FormData{std::move(rows)});
        }
    }
    if (preserve) {
        for (const auto& item : retained) {
            result.set(item.path, FormData{read_form_field(item.field)});
        }
    }
    for (const auto& item : fields) {
        result.set(item.path, FormData{read_form_field(item.field)});
    }
    return result;
}

bool FormModel::set(const FormNamePath& path, FormValue value) {
    if (!alive) {
        return false;
    }
    const auto it = std::ranges::find(fields_, path, &Registration::path);
    if (it == fields_.end()) {
        if (path.empty()) {
            throw std::invalid_argument("Form field path cannot be empty");
        }
        FormData update;
        update.set(path, FormData{std::move(value)});
        return set_values(update);
    }
    const auto item = *it;
    if (read_form_field(item.field) == value) {
        ++revision_;
        cancel_validation();
        if (alive && item.field->registered && item.field->generation == item.generation) {
            batch([&] {
                item.field->error.set({});
                auto meta = item.field->meta.get();
                meta.errors.clear();
                meta.warnings.clear();
                item.field->meta.set(meta);
            });
        }
    } else if (alive && item.field->registered && item.field->generation == item.generation) {
        write_form_field(item.field, std::move(value));
    }
    return alive;
}

bool FormModel::set_values(const FormData& data) {
    if (!alive) {
        return false;
    }
    data.validate();
    const auto inspected_revision = revision_;
    const auto inspected_fields = fields_;
    auto stored = stored_values.get();
    merge_data(stored, data);
    auto preflight = stored;
    for (const auto& item : inspected_fields) {
        const auto* node = data.at(item.path);
        preflight.set(item.path, node ? *node : FormData{read_form_field(item.field)});
    }
    for (const auto& item : inspected_fields) {
        if (const auto* node = data.at(item.path)) {
            const auto* value = std::get_if<FormValue>(&node->value);
            if (!value || value->index() != item.field->initial.index()) {
                throw std::invalid_argument("Form setFieldsValue type mismatch");
            }
        }
    }
    if (!alive || revision_ != inspected_revision) {
        throw std::runtime_error("Form update was superseded during value inspection");
    }
    const auto keep = shared_from_this();
    try {
        const auto shapes = collections;
        for (const auto& shape : shapes) {
            if (!shape.active()) {
                continue;
            }
            if (const auto* node = data.at(shape.path())) {
                const auto* array = std::get_if<FormData::Array>(&node->value);
                if (!array) {
                    throw std::invalid_argument("FormList updates require an array");
                }
                if (!shape.update(*array)) {
                    const auto action = retire;
                    if (alive && action) {
                        action();
                    } else if (alive) {
                        dispose();
                    }
                    return false;
                }
            }
        }
        std::vector<std::pair<Registration, FormValue>> updates;
        for (const auto& item : fields_) {
            if (const auto* node = data.at(item.path)) {
                const auto* leaf = std::get_if<FormValue>(&node->value);
                if (!leaf || leaf->index() != item.field->initial.index()) {
                    throw std::invalid_argument("Form setFieldsValue type mismatch");
                }
                updates.emplace_back(item, *leaf);
            }
        }
        batch([&] {
            ++revision_;
            cancel_validation();
            if (!alive) {
                return;
            }
            stored_values.set(std::move(stored));
            for (const auto& [item, value] : updates) {
                if (!alive) {
                    return;
                }
                if (!item.field->registered || item.field->generation != item.generation ||
                    item.field->owner.lock().get() != this) {
                    throw std::runtime_error("Form update lost its registration authority");
                }
                write_form_field(item.field, value);
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

bool FormModel::reset_fields(const std::vector<FormNamePath>& paths) {
    if (!alive || busy) {
        return false;
    }
    Busy guard{*this};
    cancel_validation();
    ++revision_;
    auto stored = stored_values.get();
    for (const auto& path : paths) {
        if (const auto* initial = initial_values.at(path)) {
            stored.set(path, *initial);
        } else {
            erase_data(stored, path);
        }
    }
    stored_values.set(std::move(stored));
    const auto shapes = collections;
    for (const auto& shape : shapes) {
        if (shape.active() && shape.reset &&
            std::ranges::any_of(paths, [&](const auto& path) { return shape.path().startsWith(path); })) {
            shape.reset();
        }
    }
    const auto fields = fields_;
    for (const auto& item : fields) {
        if (std::ranges::none_of(paths, [&](const auto& path) { return item.path.startsWith(path); })) {
            continue;
        }
        if (!alive) {
            return false;
        }
        if (item.field->registered && item.field->generation == item.generation) {
            batch([&] {
                item.field->error.set({});
                item.field->meta.set({});
                item.field->validated = false;
                item.field->due.reset();
            });
            if (!alive || !item.field->registered || item.field->generation != item.generation) {
                continue;
            }
            write_form_field(item.field, item.field->initial);
            if (alive && item.field->registered && item.field->generation == item.generation && item.reset) {
                item.reset();
            }
        }
    }
    return alive;
}

void FormModel::relocate(const std::vector<std::pair<FormNamePath, FormNamePath>>& moves) {
    const auto revision = revision_;
    auto next = fields_;
    for (auto& item : next) {
        for (const auto& [from, to] : moves) {
            if (item.path.startsWith(from)) {
                FormNamePath suffix;
                for (std::size_t i = from.segments().size(); i < item.path.segments().size(); ++i) {
                    suffix = suffix.append(FormNamePath{item.path.segments()[i]});
                }
                item.path = to.append(suffix);
                break;
            }
        }
    }
    FormData preflight;
    for (std::size_t i = 0; i < next.size(); ++i) {
        for (std::size_t j = 0; j < i; ++j) {
            if (next[i].path.startsWith(next[j].path) || next[j].path.startsWith(next[i].path)) {
                throw std::invalid_argument("Form list move conflicts with a registered path");
            }
        }
        preflight.set(next[i].path, FormData{read_form_field(next[i].field)});
    }
    if (!alive || revision_ != revision) {
        throw std::runtime_error("Form relocation was superseded during value inspection");
    }
    const auto previous_store = stored_values.get();
    auto next_store = previous_store;
    for (const auto& [from, to] : moves) {
        erase_data(next_store, from);
    }
    for (const auto& [from, to] : moves) {
        if (const auto* node = previous_store.at(from)) {
            next_store.set(to, *node);
        }
    }
    batch([&] {
        fields_ = std::move(next);
        ++revision_;
        stored_values.set(std::move(next_store));
        topology.set(topology.get() + 1);
        cancel_validation();
    });
}
} // namespace ryn::detail

namespace ryn {
std::optional<FormValue> FormRef::getFieldValue(const FormNamePath& path) const {
    const auto model = detail::resolve_form_model(state_);
    return model && model->alive ? model->get(path) : std::nullopt;
}

FormData FormRef::getFieldsValue(bool preserve) const {
    const auto model = detail::resolve_form_model(state_);
    return model && model->alive ? model->get_values(preserve) : FormData{};
}

bool FormRef::setFieldValue(const FormNamePath& path, FormValue value) const {
    const auto model = detail::resolve_form_model(state_);
    return model && model->set(path, std::move(value));
}

bool FormRef::setFieldsValue(const FormData& data) const {
    const auto model = detail::resolve_form_model(state_);
    return model && model->set_values(data);
}

Binding<std::optional<FormValue>> FormRef::watch(FormNamePath path) const {
    return bind([ref = *this, path = std::move(path)] { return ref.getFieldValue(path); });
}

bool FormRef::resetFields(const std::vector<FormNamePath>& paths) const {
    const auto model = detail::resolve_form_model(state_);
    return model && model->reset_fields(paths);
}
} // namespace ryn

namespace ryn::detail {
void FormModel::forget(const FormNamePath& prefix) {
    std::erase_if(retained_, [&](const auto& item) { return item.path.startsWith(prefix); });
    auto stored = stored_values.get();
    erase_data(stored, prefix);
    stored_values.set(std::move(stored));
}
} // namespace ryn::detail

namespace ryn::detail {
void FormModel::notify_change(const std::shared_ptr<FormFieldState>& field) {
    const auto keep = shared_from_this();
    if (!alive) {
        return;
    }
    const auto it = std::ranges::find(fields_, field, &Registration::field);
    if (it == fields_.end()) {
        return;
    }
    const auto item = *it;
    if (const auto callback = changed) {
        callback({item.name, read_form_field(field), item.path}, get_values());
    }
    if (alive && field->registered && field->generation == item.generation && field->owner.lock().get() == this) {
        if (const auto callback = fields_changed) {
            callback({{item.path, read_form_field(field), field->meta.get()}}, get_fields());
        }
    }
}
} // namespace ryn::detail

namespace ryn::detail {
std::optional<FormFieldMeta> FormModel::get_meta(const FormNamePath& path) const {
    static_cast<void>(topology.get());
    const auto it = std::ranges::find(fields_, path, &Registration::path);
    return it == fields_.end() ? std::nullopt : std::optional{it->field->meta.get()};
}
} // namespace ryn::detail
