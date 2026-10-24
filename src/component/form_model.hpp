#pragma once

#include <ryn/form.hpp>

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
    std::function<bool()> eligible;
};

struct FormRefState final {
    std::weak_ptr<FormModel> model;
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
    };

    bool alive{true};
    bool busy{};
    bool focus_first_invalid{true};
    Signal<bool> disabled{false};
    Signal<bool> read_only{false};
    std::function<bool()> eligible;
    std::function<void(FormResult)> finish;
    std::function<void(FormResult)> failed;

    std::uint64_t add(String name, const std::shared_ptr<FormFieldState>& field, std::function<bool()> eligible = {},
                      std::function<bool()> focus = {}, std::function<void()> reset = {});
    void remove(const std::shared_ptr<FormFieldState>& field, std::uint64_t generation);
    void dispose();
    void value_changed(const std::shared_ptr<FormFieldState>& field);
    [[nodiscard]] bool available() const;
    [[nodiscard]] std::optional<FormResult> validate();
    bool submit();
    bool reset();

    [[nodiscard]] const std::vector<Registration>& fields() const noexcept {
        return fields_;
    }

private:
    FormResult validate_transaction();
    std::vector<FormFieldValue> values() const;
    std::vector<Registration> fields_;
    std::uint64_t revision_{};
};

} // namespace ryn::detail
