#pragma once

#include <ryn/input_number.hpp>

#include <cstdint>
#include <string_view>

namespace ryn::detail {

struct InputNumberOptions final {
    double min{-9007199254740991.0};
    double max{9007199254740991.0};
    double step{1};
    std::optional<unsigned> precision;
    friend bool operator==(const InputNumberOptions&, const InputNumberOptions&) = default;
};

// A fixed decimal with separate integer/fraction limbs avoids scaling a large
// integer into int64 overflow. No binary floating point arithmetic is used.
struct InputNumberDecimal final {
    std::uint64_t whole{};
    std::uint64_t fraction{};
    bool negative{};
    friend bool operator==(const InputNumberDecimal&, const InputNumberDecimal&) = default;
};

class InputNumberModel final {
public:
    InputNumberModel(InputNumberOptions, InputNumberValue, bool controlled);
    void configure(InputNumberOptions);
    void reconcile(InputNumberValue);
    void accept(InputNumberValue);
    [[nodiscard]] InputNumberValue value() const noexcept;
    [[nodiscard]] const InputNumberOptions& options() const noexcept;
    // Outer nullopt means incomplete/invalid/out-of-range draft. Inner nullopt is empty.
    [[nodiscard]] std::optional<InputNumberValue> parse(std::string_view, bool finish = false) const;
    [[nodiscard]] InputNumberValue stepped(InputNumberStepDirection) const;
    [[nodiscard]] static String format(InputNumberValue);
    static void validate_value(InputNumberValue);

private:
    InputNumberOptions options_;
    InputNumberDecimal min_;
    InputNumberDecimal max_;
    InputNumberDecimal step_;
    InputNumberValue value_;
    bool controlled_{};
};

} // namespace ryn::detail
