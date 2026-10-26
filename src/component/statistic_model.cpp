#include "component/statistic_model.hpp"
#include <algorithm>
#include <array>
#include <charconv>
#include <cmath>
#include <stdexcept>

namespace ryn::detail {
String format_statistic(const StatisticModel& model) {
    if (model.precision && *model.precision > 100) {
        throw std::invalid_argument("Statistic precision must not exceed 100");
    }
    const auto raw = std::visit(
        [](const auto& value) -> String {
            using T = std::decay_t<decltype(value)>;
            if constexpr (std::same_as<T, String>) {
                return value;
            } else {
                if constexpr (std::same_as<T, double>) {
                    if (!std::isfinite(value)) {
                        throw std::invalid_argument("Statistic numeric value must be finite");
                    }
                }
                std::array<char, 128> buffer{};
                const auto result = std::to_chars(buffer.data(), buffer.data() + buffer.size(), value);
                if (result.ec != std::errc{}) {
                    throw std::invalid_argument("Statistic value cannot be formatted");
                }
                return String::from_utf8(std::string_view{buffer.data(), result.ptr}).value();
            }
        },
        model.value.storage());
    if (model.formatter) {
        return model.formatter(model.value);
    }
    const auto text = raw.bytes();
    const bool negative = text.starts_with('-');
    const auto start = negative ? std::size_t{1} : 0;
    auto end = start;
    while (end < text.size() && text[end] >= '0' && text[end] <= '9') {
        ++end;
    }
    auto decimal_start = end;
    if (decimal_start < text.size() && text[decimal_start] == '.') {
        ++decimal_start;
        if (decimal_start == text.size()) {
            return raw;
        }
        for (auto i = decimal_start; i < text.size(); ++i) {
            if (text[i] < '0' || text[i] > '9') {
                return raw;
            }
        }
    } else if (end != text.size() || (negative && end == start)) {
        return raw;
    }
    const auto integer = end == start ? std::string_view{"0"} : text.substr(start, end - start);
    std::string decimal{text.substr(decimal_start)};
    if (model.precision) {
        decimal.resize(static_cast<std::size_t>(std::max(0, *model.precision)), '0');
    }
    std::string formatted = negative ? "-" : "";
    for (std::size_t i = 0; i < integer.size(); ++i) {
        if (i > 0 && (integer.size() - i) % 3 == 0) {
            formatted += model.group_separator.bytes();
        }
        formatted += integer[i];
    }
    if (!decimal.empty()) {
        formatted += model.decimal_separator.bytes();
        formatted += decimal;
    }
    return String::from_utf8(formatted).value();
}
} // namespace ryn::detail
