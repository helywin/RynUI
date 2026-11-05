#pragma once

#include <ryn/select.hpp>
#include <ryn/decimal.hpp>
#include <concepts>
#include <stdexcept>
#include <variant>
#include <vector>

namespace ryn {

using FormValue =
    std::variant<String, std::optional<double>, bool, SelectSelection, SelectValues, std::optional<Decimal>>;

class FormNameSegment final {
public:
    FormNameSegment(String value) : value_(std::move(value)) {}

    template <std::size_t N> FormNameSegment(const char8_t (&value)[N]) : value_(String{value}) {}

    template <std::integral T> FormNameSegment(T value) : value_(std::size_t(value)) {
        if constexpr (std::signed_integral<T>) {
            if (value < 0) {
                throw std::invalid_argument("Form path indices cannot be negative");
            }
        }
    }

    [[nodiscard]] const std::variant<String, std::size_t>& value() const {
        return value_;
    }

    friend bool operator==(const FormNameSegment&, const FormNameSegment&) = default;

private:
    std::variant<String, std::size_t> value_;
};

class FormNamePath final {
public:
    FormNamePath() = default;

    FormNamePath(std::initializer_list<FormNameSegment> value) : segments_(value) {}

    explicit FormNamePath(String name) : segments_{std::move(name)} {}

    [[nodiscard]] const std::vector<FormNameSegment>& segments() const {
        return segments_;
    }

    [[nodiscard]] bool empty() const noexcept {
        return segments_.empty();
    }

    [[nodiscard]] FormNamePath append(const FormNamePath& suffix) const {
        auto result = *this;
        result.segments_.insert(result.segments_.end(), suffix.segments_.begin(), suffix.segments_.end());
        return result;
    }

    [[nodiscard]] bool startsWith(const FormNamePath& prefix) const;
    friend bool operator==(const FormNamePath&, const FormNamePath&) = default;

private:
    std::vector<FormNameSegment> segments_;
};

// Owned structured snapshots. Object keys are literal strings; only explicit index segments create arrays.
struct FormData final {
    using Object = std::vector<std::pair<String, FormData>>;
    using Array = std::vector<FormData>;
    std::variant<std::monostate, FormValue, Object, Array> value;
    FormData() = default;

    FormData(FormValue leaf) : value(std::move(leaf)) {}

    FormData(Object object) : value(std::move(object)) {}

    FormData(Array array) : value(std::move(array)) {}

    [[nodiscard]] const FormData* at(const FormNamePath&) const;
    void set(const FormNamePath&, FormData);
    void validate() const;
    friend bool operator==(const FormData&, const FormData&) = default;
};

struct FormFieldValue final {
    String name;
    FormValue value;
    FormNamePath path;
    friend bool operator==(const FormFieldValue&, const FormFieldValue&) = default;
};

struct FormError final {
    String name;
    String message;
    FormNamePath path;
    friend bool operator==(const FormError&, const FormError&) = default;
};

struct FormResult final {
    std::vector<FormFieldValue> values;
    std::vector<FormError> errors;
    bool out_of_date{};
    std::vector<FormError> warnings;
    FormData structure;

    [[nodiscard]] bool valid() const noexcept {
        return !out_of_date && errors.empty();
    }

    [[nodiscard]] FormData structuredValues() const;
};

} // namespace ryn
