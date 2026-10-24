#include "component/form_model.hpp"

#include <algorithm>
#include <cmath>
#include <stdexcept>

namespace ryn::detail {
namespace {
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
} // namespace

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
            callback(std::move(value));
        }
    } else {
        if (state->value.get() == value) {
            return;
        }
        // Invalidate the transaction before observers or consumer callbacks can run.
        batch([&] {
            if (const auto owner = state->owner.lock()) {
                owner->value_changed(state);
            }
            state->value.set(std::move(value));
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
    const auto generation = state->generation;
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
        write_form_field(current, std::move(value));
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
    if (!alive || !field || name.empty() || field->registered ||
        std::any_of(fields_.begin(), fields_.end(), [&](const auto& item) { return item.name == name; })) {
        throw std::invalid_argument("Form field/name must be nonempty and uniquely registered");
    }
    const auto generation = ++field->generation;
    fields_.push_back({std::move(name), field, generation, eligibility, std::move(focus), std::move(reset)});
    field->owner = shared_from_this();
    field->registered = true;
    field->eligible = std::move(eligibility);
    field->error.set(String{});
    ++revision_;
    return generation;
}

void FormModel::remove(const std::shared_ptr<FormFieldState>& field, std::uint64_t generation) {
    if (!field->registered || field->generation != generation) {
        return;
    }
    ++revision_;
    field->registered = false;
    field->owner.reset();
    field->eligible = {};
    std::erase_if(fields_, [&](const auto& item) { return item.field == field && item.generation == generation; });
    field->error.set(String{});
}

void FormModel::dispose() {
    alive = false;
    const auto fields = fields_;
    for (const auto& item : fields) {
        remove(item.field, item.generation);
    }
    eligible = {};
    finish = {};
    failed = {};
}

void FormModel::value_changed(const std::shared_ptr<FormFieldState>& field) {
    ++revision_;
    field->error.set(String{});
}

bool FormModel::available() const {
    const auto check = eligible;
    return alive && !disabled.get() && (!check || check());
}

std::vector<FormFieldValue> FormModel::values() const {
    std::vector<FormFieldValue> result;
    result.reserve(fields_.size());
    for (const auto& item : fields_) {
        result.push_back({item.name, read_form_field(item.field)});
    }
    return result;
}

FormResult FormModel::validate_transaction() {
    const auto revision = revision_;
    const auto fields = fields_;
    FormResult result{values(), {}, false};
    for (std::size_t i = 0; i < fields.size(); ++i) {
        const auto& field = fields[i].field;
        const auto& value = result.values[i].value;
        String error;
        if (field->required && empty(value)) {
            error = field->required_message;
        } else if (const auto validator = field->validator) {
            error = validator(value);
        }
        if (!alive || revision != revision_ || values() != result.values || !available()) {
            result.out_of_date = true;
            return result;
        }
        if (!error.empty()) {
            result.errors.push_back({fields[i].name, std::move(error)});
        }
    }
    // Publish only after all user validators have completed against the same snapshot.
    batch([&] {
        for (const auto& item : fields) {
            const auto error = std::find_if(result.errors.begin(), result.errors.end(),
                                            [&](const auto& value) { return value.name == item.name; });
            item.field->error.set(error == result.errors.end() ? String{} : error->message);
        }
    });
    result.out_of_date = !alive || revision != revision_ || values() != result.values || !available();
    return result;
}

std::optional<FormResult> FormModel::validate() {
    const auto keep = shared_from_this();
    if (busy || !available()) {
        return {};
    }
    Busy guard{*this};
    return validate_transaction();
}

bool FormModel::submit() {
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
                             [&](const auto& e) { return e.name == item.name; }) ||
                form_field_disabled(item.field) || form_field_read_only(item.field) || !item.focus ||
                (item.eligible && !item.eligible())) {
                continue;
            }
            const bool focused = item.focus();
            if (!alive || !available()) {
                return false;
            }
            if (revision != revision_ || values() != result.values) {
                result.out_of_date = true;
                break;
            }
            if (focused) {
                break;
            }
        }
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
    Busy guard{*this};
    const auto fields = fields_;
    for (const auto& item : fields) {
        if (!alive) {
            return false;
        }
        if (!item.field->registered || item.field->generation != item.generation) {
            continue;
        }
        item.field->error.set(String{});
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
    const auto model = state_->model.lock();
    return model && model->alive;
}

std::optional<FormResult> FormRef::validate() const {
    if (!state_) {
        return {};
    }
    const auto model = state_->model.lock();
    return model ? model->validate() : std::nullopt;
}

bool FormRef::submit() const {
    if (!state_) {
        return false;
    }
    const auto model = state_->model.lock();
    return model && model->submit();
}

bool FormRef::reset() const {
    if (!state_) {
        return false;
    }
    const auto model = state_->model.lock();
    return model && model->reset();
}
} // namespace ryn
