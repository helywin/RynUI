#pragma once
#include <ryn/decimal.hpp>
#include <optional>
#include <string>

namespace ryn::detail {
struct ExactDecimal final {
    std::string digits{"0"};
    std::size_t scale{};
    bool negative{};
    static std::optional<ExactDecimal> parse(std::string_view, bool finish = true);
    [[nodiscard]] std::string text() const;
    [[nodiscard]] ExactDecimal rounded(unsigned precision, int direction = 0) const;
    [[nodiscard]] int compare(const ExactDecimal&) const;
    [[nodiscard]] ExactDecimal add(ExactDecimal) const;
    void normalize();
};
} // namespace ryn::detail
