#pragma once

#include <optional>

namespace ryn {

class SelectionOptionDisabled final {
public:
    constexpr SelectionOptionDisabled() noexcept = default;

    constexpr SelectionOptionDisabled(bool value) noexcept : value_(value), explicit_(true) {}

    constexpr SelectionOptionDisabled& operator=(bool value) noexcept {
        value_ = value;
        explicit_ = true;
        return *this;
    }

    constexpr operator bool() const noexcept {
        return value_;
    }

    [[nodiscard]] constexpr std::optional<bool> explicitValue() const noexcept {
        return explicit_ ? std::optional{value_} : std::nullopt;
    }

    [[nodiscard]] constexpr bool valueOr(bool inherited) const noexcept {
        return explicit_ ? value_ : inherited;
    }

    friend constexpr bool operator==(const SelectionOptionDisabled&, const SelectionOptionDisabled&) = default;

    friend constexpr bool operator==(const SelectionOptionDisabled& left, bool right) noexcept {
        return static_cast<bool>(left) == right;
    }

private:
    bool value_{};
    bool explicit_{};
};

} // namespace ryn
