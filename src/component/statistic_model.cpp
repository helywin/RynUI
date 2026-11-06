#include "component/statistic_model.hpp"
#include <algorithm>
#include <array>
#include <charconv>
#include <cmath>
#include <stdexcept>
#include <limits>

namespace ryn::detail {
void validate_statistic_locale(const StatisticLocale& locale) {
    if (locale.primary_group == 0 || locale.secondary_group == 0) {
        throw std::invalid_argument("Statistic locale groups must be positive");
    }
    for (const auto& digit : locale.digits) {
        if (digit.empty()) {
            throw std::invalid_argument("Statistic locale digits must not be empty");
        }
    }
}

namespace {
constexpr std::array<char, 7> time_names{'Y', 'M', 'D', 'H', 'm', 's', 'S'};
constexpr std::array<std::int64_t, 7> time_units{31536000000LL, 2592000000LL, 86400000, 3600000, 60000, 1000, 1};

std::array<bool, 7> time_fields(std::string_view format) {
    std::array<bool, 7> result{};
    for (std::size_t i = 0; i < format.size(); ++i) {
        if (format[i] == '[') {
            const auto end = format.find(']', i + 1);
            if (end != std::string_view::npos) {
                i = end;
                continue;
            }
        }
        for (std::size_t unit = 0; unit < time_names.size(); ++unit) {
            result[unit] |= format[i] == time_names[unit];
        }
    }
    return result;
}

void append_digits(std::string& output, std::string_view text, const StatisticLocale& locale) {
    for (const auto c : text) {
        output += c >= '0' && c <= '9' ? locale.digits[std::size_t(c - '0')].bytes() : std::string_view{&c, 1};
    }
}
} // namespace

void validate_statistic_timer(const StatisticTimer& timer) {
    if ((timer.type != StatisticTimerType::Countdown && timer.type != StatisticTimerType::Countup) ||
        timer.value.count_milliseconds() > double(std::numeric_limits<std::int64_t>::max() / 1000)) {
        throw std::invalid_argument("Statistic timer is invalid");
    }
}

std::int64_t statistic_timer_quantum(const String& format) {
    const auto fields = time_fields(format.bytes());
    for (std::size_t unit = fields.size(); unit > 0; --unit) {
        if (fields[unit - 1]) {
            return time_units[unit - 1];
        }
    }
    return 0;
}

String format_statistic_timer(Duration duration, const String& format, const StatisticLocale& locale) {
    validate_statistic_locale(locale);
    const auto fields = time_fields(format.bytes());
    std::array<std::uint64_t, 7> values{};
    auto remaining = static_cast<std::uint64_t>(duration.count_milliseconds());
    for (std::size_t unit = 0; unit < fields.size(); ++unit) {
        if (fields[unit]) {
            values[unit] = remaining / std::uint64_t(time_units[unit]);
            remaining %= std::uint64_t(time_units[unit]);
        }
    }
    std::string output;
    const auto pattern = format.bytes();
    for (std::size_t i = 0; i < pattern.size();) {
        if (pattern[i] == '[') {
            const auto end = pattern.find(']', i + 1);
            if (end != std::string_view::npos) {
                output += pattern.substr(i + 1, end - i - 1);
                i = end + 1;
                continue;
            }
        }
        const auto unit = std::ranges::find(time_names, pattern[i]);
        if (unit == time_names.end()) {
            output += pattern[i++];
            continue;
        }
        auto end = i + 1;
        while (end < pattern.size() && pattern[end] == pattern[i]) {
            ++end;
        }
        auto text = std::to_string(values[std::size_t(unit - time_names.begin())]);
        text.insert(0, end - i > text.size() ? end - i - text.size() : 0, '0');
        append_digits(output, text, locale);
        i = end;
    }
    return String::from_utf8(output).value();
}

String format_statistic(const StatisticModel& model) {
    validate_statistic_locale(model.locale);
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
        const auto trailing = integer.size() - i;
        if (i > 0 && trailing >= model.locale.primary_group &&
            (trailing - model.locale.primary_group) % model.locale.secondary_group == 0) {
            formatted += (model.explicit_group ? model.group_separator : model.locale.group_separator).bytes();
        }
        formatted += model.locale.digits[std::size_t(integer[i] - '0')].bytes();
    }
    if (!decimal.empty()) {
        formatted += (model.explicit_decimal ? model.decimal_separator : model.locale.decimal_separator).bytes();
        append_digits(formatted, decimal, model.locale);
    }
    return String::from_utf8(formatted).value();
}
} // namespace ryn::detail
