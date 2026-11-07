#pragma once
#include <ryn/form_values.hpp>
#include <ryn/async_options.hpp>

namespace ryn {
enum class FormValidateTrigger { Manual, Change, Blur, ChangeAndBlur };
enum class FormValidateStatus { None, Success, Warning, Error, Validating };
enum class FormRuleType { Any, String, Number, Boolean, Array };

// Provenance of the first current error. Literal includes explicit setFields
// metadata even when its text equals a validator or builtin message.
enum class FormIssueSource { Literal, Required, Invalid, Validator, RuleValidator, AsyncValidator };

struct FormFieldMeta final {
    bool touched{};
    bool validating{};
    bool out_of_date{};
    std::vector<String> errors;
    std::vector<String> warnings;
    friend bool operator==(const FormFieldMeta&, const FormFieldMeta&) = default;
};

struct FormItemStatus final {
    FormValidateStatus status{FormValidateStatus::None};
    std::vector<String> errors;
    std::vector<String> warnings;
    friend bool operator==(const FormItemStatus&, const FormItemStatus&) = default;
};

struct FormIssue final {
    String message;
    bool warning{};
    friend bool operator==(const FormIssue&, const FormIssue&) = default;
};

struct FormValidationContext final {
    FormData values;
    std::uint64_t generation{};
    std::shared_ptr<const std::atomic_bool> cancelled;

    [[nodiscard]] std::optional<FormValue> get(const FormNamePath& path) const {
        const auto* data = values.at(path);
        const auto* leaf = data ? std::get_if<FormValue>(&data->value) : nullptr;
        return leaf ? std::optional{*leaf} : std::nullopt;
    }
};

struct FormRule final {
    FormRuleType type{FormRuleType::Any};
    bool required{};
    bool whitespace{};
    bool warning_only{};
    std::optional<std::size_t> length;
    std::optional<std::size_t> min_length;
    std::optional<std::size_t> max_length;
    std::optional<Decimal> minimum;
    std::optional<Decimal> maximum;
    std::vector<FormValue> enumeration;
    // Unset uses the current builtin locale. Every explicit String is literal.
    std::optional<String> message;
    std::function<bool(String)> pattern;
    std::function<FormValue(FormValue)> transform;
    std::function<std::vector<FormIssue>(const FormValue&, const FormValidationContext&)> validator;
};

using FormValidationComplete = std::function<void(std::vector<FormIssue>)>;
template <class T>
using FormAsyncValidator = std::function<AsyncOptionsCancel(T, FormValidationContext, FormValidationComplete)>;

struct FormFieldData final {
    FormNamePath path;
    std::optional<FormValue> value;
    FormFieldMeta meta;
};

struct FormValidateOptions final {
    std::vector<FormNamePath> names;
    bool recursive{};
    bool dirty{};
    bool validate_only{};
};
} // namespace ryn
