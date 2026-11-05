#pragma once

#include <ryn/form.hpp>
#include "component/option_list_support.hpp"

namespace ryn::detail {

class FormModel;

struct FormFieldState final {
    explicit FormFieldState(FormValue value) : initial(value), value(std::move(value)) {}

    FormValue initial;
    Signal<FormValue> value;
    std::optional<Binding<FormValue>> source;
    std::function<void(FormValue)> change;
    bool required{};
    String required_message;
    std::function<String(const FormValue&)> validator;
    Signal<String> error{String{}};
    Signal<bool> disabled{false};
    Signal<bool> read_only{false};
    std::weak_ptr<FormModel> owner;
    std::uint64_t generation{};
    bool registered{};
    bool preserve{true};
    bool validated{};
    std::vector<FormRule> rules;
    bool validate_first{};
    std::function<AsyncOptionsSource<FormIssue>(FormValue, FormValidationContext)> async;
    std::shared_ptr<AsyncOptionsSession<FormIssue>> session{std::make_shared<AsyncOptionsSession<FormIssue>>()};
    Signal<FormFieldMeta> meta{FormFieldMeta{}};
    Signal<FormValidateStatus> display_status{FormValidateStatus::None};
    Signal<FormPresentation> presentation{FormPresentation{}};
    Signal<std::optional<FormValidateStatus>> status_override{std::nullopt};
    FormValidateTrigger trigger{FormValidateTrigger::Manual};
    std::uint32_t debounce_ms{};
    std::vector<FormNamePath> dependencies;
    std::optional<animation::AnimationTime> due;
    std::optional<animation::AnimationTime> timeout;
    std::function<bool()> eligible;
};

// A callback created while mounting an old item must keep that item's registration ticket.
class FormRegistrationScope final {
public:
    FormRegistrationScope(const std::shared_ptr<FormFieldState>&, std::uint64_t);
    ~FormRegistrationScope();
    FormRegistrationScope(const FormRegistrationScope&) = delete;
    FormRegistrationScope& operator=(const FormRegistrationScope&) = delete;
    [[nodiscard]] static std::uint64_t generation(const std::shared_ptr<FormFieldState>&);

private:
    const FormFieldState* field_;
    std::uint64_t generation_;
    FormRegistrationScope* previous_;
    static thread_local FormRegistrationScope* current_;
};

struct FormRefState final {
    std::weak_ptr<FormModel> model;
    Signal<std::uint64_t> revision{0};
};

class FormModel final : public std::enable_shared_from_this<FormModel> {
public:
    struct Registration final {
        String name;
        std::shared_ptr<FormFieldState> field;
        std::uint64_t generation{};
        std::function<bool()> eligible;
        std::function<bool()> focus;
        std::function<void()> reset;
        FormNamePath path;
    };

    struct Collection final {
        std::function<FormNamePath()> path;
        std::function<std::size_t()> size;
        std::function<bool(FormData::Array)> update;
        std::function<bool()> active;
        std::function<void()> reset;
        std::shared_ptr<FormFieldState> validation;
    };

    std::vector<Collection> collections;
    bool alive{true};
    bool busy{};
    bool focus_first_invalid{true};
    bool preserve{true};
    bool clear_on_destroy{};
    FormData initial_values;
    Signal<FormData> stored_values{FormData{}};
    std::vector<std::function<void()>> reset_hooks;
    Signal<bool> disabled{false};
    Signal<bool> read_only{false};
    std::function<bool()> eligible;
    std::function<void(FormResult)> finish;
    std::function<void(FormResult)> failed;
    std::function<void(FormFieldValue, FormData)> changed;
    std::function<void(std::vector<FormFieldData>, std::vector<FormFieldData>)> fields_changed;
    std::function<bool(FormNamePath, FormScrollOptions)> scroll;
    std::function<std::optional<FormControlTarget>(FormNamePath)> control;
    bool scroll_first{};
    void notify_change(const std::shared_ptr<FormFieldState>&);

    std::uint64_t add(String name, const std::shared_ptr<FormFieldState>& field, std::function<bool()> eligible = {},
                      std::function<bool()> focus = {}, std::function<void()> reset = {});
    std::uint64_t add(FormNamePath path, const std::shared_ptr<FormFieldState>& field,
                      std::function<bool()> eligible = {}, std::function<bool()> focus = {},
                      std::function<void()> reset = {});
    [[nodiscard]] std::optional<FormValue> get(const FormNamePath&) const;
    [[nodiscard]] std::optional<FormFieldMeta> get_meta(const FormNamePath&) const;
    [[nodiscard]] FormData get_values(bool preserve = false) const;
    bool set(const FormNamePath&, FormValue);
    bool set_values(const FormData&);
    bool reset_fields(const std::vector<FormNamePath>&);
    void forget(const FormNamePath&);
    void relocate(const std::vector<std::pair<FormNamePath, FormNamePath>>&);
    Signal<std::uint64_t> topology{0};
    void remove(const std::shared_ptr<FormFieldState>& field, std::uint64_t generation);
    void dispose();
    void value_changed(const std::shared_ptr<FormFieldState>& field);
    [[nodiscard]] bool available() const;
    [[nodiscard]] std::optional<FormResult> validate();
    bool submit();
    bool reset();
    bool validate_async(std::function<void(FormResult)>, FormValidateOptions = {}, bool submit = false);
    bool cancel_validation(bool notify = true);
    void boundary_changed();
    void schedule(const std::shared_ptr<FormFieldState>&, bool blur = false);
    std::size_t tick(animation::AnimationTime);
    [[nodiscard]] std::optional<animation::AnimationTime> deadline() const;
    std::function<void()> wake;
    std::function<void()> retire;
    std::function<animation::AnimationTime()> clock;
    [[nodiscard]] std::vector<FormFieldData> get_fields() const;
    bool set_fields(const std::vector<FormFieldData>&);

    [[nodiscard]] std::uint64_t revision() const {
        return revision_;
    }

    [[nodiscard]] const std::vector<Registration>& fields() const noexcept {
        return fields_;
    }

private:
    FormResult validate_transaction();
    struct ValidationRun;
    std::shared_ptr<ValidationRun> pending_;
    animation::AnimationTime now_;
    std::uint64_t validation_generation_{};
    void complete_async(const std::shared_ptr<ValidationRun>&);
    std::vector<FormIssue> validate_rules(const std::shared_ptr<FormFieldState>&, const FormValue&, const FormData&);
    std::vector<FormFieldValue> values() const;
    std::vector<Registration> fields_;
    std::uint64_t revision_{};
    std::vector<Registration> retained_;
};

} // namespace ryn::detail

namespace ryn::detail {
inline std::shared_ptr<FormModel> resolve_form_model(const std::shared_ptr<FormRefState>& state) {
    if (!state) {
        return {};
    }
    static_cast<void>(state->revision.get());
    return state->model.lock();
}
} // namespace ryn::detail
