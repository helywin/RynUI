#include "i18n/number_format.hpp"

#include <algorithm>
#include <array>
#include <charconv>
#include <cmath>

namespace ryn::detail {

void validate_number_locale(const StatisticLocale& locale) {
    if (locale.primary_group == 0 || locale.secondary_group == 0) {
        throw std::invalid_argument("Number locale groups must be positive");
    }
    for (const auto& digit : locale.digits) {
        if (digit.empty()) {
            throw std::invalid_argument("Number locale digits must not be empty");
        }
    }
}

String format_decimal_text(const String& raw, const StatisticLocale& locale, const NumberFormatOptions& options) {
    validate_number_locale(locale);
    if (options.precision && *options.precision > 100) {
        throw std::invalid_argument("Number display precision must not exceed 100");
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
    if (options.precision) {
        decimal.resize(static_cast<std::size_t>(std::max(0, *options.precision)), '0');
    }
    const auto& group_separator = options.group_separator ? *options.group_separator : locale.group_separator;
    const auto& decimal_separator = options.decimal_separator ? *options.decimal_separator : locale.decimal_separator;
    std::string formatted = negative ? "-" : "";
    for (std::size_t i = 0; i < integer.size(); ++i) {
        const auto trailing = integer.size() - i;
        if (options.grouping && i > 0 && trailing >= locale.primary_group &&
            (trailing - locale.primary_group) % locale.secondary_group == 0) {
            formatted += group_separator.bytes();
        }
        formatted += locale.digits[std::size_t(integer[i] - '0')].bytes();
    }
    if (!decimal.empty()) {
        formatted += decimal_separator.bytes();
        for (const auto c : decimal) {
            formatted += locale.digits[std::size_t(c - '0')].bytes();
        }
    }
    return String::from_utf8(formatted).value();
}

String parse_decimal_text(const String& text, const StatisticLocale& locale) {
    std::string result;
    auto remaining = text.bytes();
    while (!remaining.empty()) {
        std::size_t length{};
        char replacement{};
        if (!locale.decimal_separator.empty() && remaining.starts_with(locale.decimal_separator.bytes())) {
            length = locale.decimal_separator.bytes().size();
            replacement = '.';
        }
        for (std::size_t digit = 0; digit < locale.digits.size(); ++digit) {
            const auto candidate = locale.digits[digit].bytes();
            if (candidate.size() > length && remaining.starts_with(candidate)) {
                length = candidate.size();
                replacement = static_cast<char>('0' + digit);
            }
        }
        if (length) {
            result += replacement;
            remaining.remove_prefix(length);
        } else {
            result += remaining.front();
            remaining.remove_prefix(1);
        }
    }
    return String::from_utf8(result).value();
}

String number_to_text(const I18nNumber& value, bool fixed) {
    return std::visit(
        [fixed](const auto& number) -> String {
            using T = std::decay_t<decltype(number)>;
            if constexpr (std::same_as<T, Decimal>) {
                return number.text();
            } else {
                if constexpr (std::same_as<T, double>) {
                    if (!std::isfinite(number)) {
                        throw std::invalid_argument("Number must be finite");
                    }
                }
                std::array<char, 2048> buffer{};
                const auto converted = [&] {
                    if constexpr (std::same_as<T, double>) {
                        return fixed ? std::to_chars(buffer.data(), buffer.data() + buffer.size(), number,
                                                     std::chars_format::fixed)
                                     : std::to_chars(buffer.data(), buffer.data() + buffer.size(), number);
                    } else {
                        return std::to_chars(buffer.data(), buffer.data() + buffer.size(), number);
                    }
                }();
                if (converted.ec != std::errc{}) {
                    throw std::invalid_argument("Number cannot be formatted");
                }
                return String::from_utf8(std::string_view{buffer.data(), converted.ptr}).value();
            }
        },
        value);
}

} // namespace ryn::detail

namespace ryn {

String formatNumber(const LocaleCatalog& catalog, const LocaleId& requested, const I18nNumber& value,
                    const NumberFormatOptions& options) {
    const auto resolution = catalog.resolveFormat(requested);
    if (!resolution.pack.data().formats.decimal) {
        throw I18nException({I18nErrorCode::UnsupportedFormat, requested, {}});
    }
    const auto raw = detail::number_to_text(value, true);
    return detail::format_decimal_text(raw, resolution.pack.data().statistic, options);
}

} // namespace ryn
