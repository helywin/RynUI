#pragma once
#include <ryn/string.hpp>
#include <string_view>

namespace ryn {
// Exact base-ten value. Exponents are expanded without binary floating point.
// Input is bounded to 16384 decimal digits; excess is rejected, never truncated.
class Decimal final {
public:
    Decimal();
    explicit Decimal(std::string_view);

    explicit Decimal(String value) : Decimal(value.bytes()) {}

    [[nodiscard]] const String& text() const noexcept {
        return text_;
    }

    friend bool operator==(const Decimal&, const Decimal&) = default;

private:
    String text_;
};
} // namespace ryn
