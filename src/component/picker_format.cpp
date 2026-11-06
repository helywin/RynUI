#include "component/picker_format.hpp"
#include <ryn/time_picker.hpp>
#include <algorithm>
#include <set>

namespace ryn {
void validate_time_format(std::string_view format) {
    const auto parts = detail::tokenize_picker_format(format, "HhmsSaA");
    std::set<char> fields;
    bool twelve{};
    bool meridiem{};
    for (const auto& p : parts) {
        if (!p.token) {
            continue;
        }
        const auto& t = p.text;
        if (t != "H" && t != "HH" && t != "h" && t != "hh" && t != "m" && t != "mm" && t != "s" && t != "ss" &&
            t != "SSS" && t != "a" && t != "A") {
            throw std::invalid_argument("Unsupported time format token");
        }
        const char field = t[0] == 'h' ? 'H' : t[0] == 'a' ? 'A' : t[0];
        if (!fields.insert(field).second) {
            throw std::invalid_argument("Duplicate time format field");
        }
        twelve |= t[0] == 'h';
        meridiem |= field == 'A';
    }
    if (!fields.contains('H') || twelve != meridiem) {
        throw std::invalid_argument("Time format requires an hour; 12-hour format requires meridiem");
    }
}

String format_time_of_day(TimeOfDay value, std::string_view format, const PickerLocale& locale) {
    validate_time_format(format);
    locale.validate();
    std::string text;
    for (const auto& p : detail::tokenize_picker_format(format, "HhmsSaA")) {
        if (!p.token) {
            text += p.text;
            continue;
        }
        const char token = p.text[0];
        if (token == 'a' || token == 'A') {
            auto meridiem = std::string{(value.hour() < 12 ? locale.am : locale.pm).bytes()};
            if (token == 'a') {
                for (auto& c : meridiem) {
                    c = static_cast<char>(std::tolower(static_cast<unsigned char>(c)));
                }
            }
            text += meridiem;
        } else {
            const int number = token == 'H'   ? value.hour()
                               : token == 'h' ? (value.hour() + 11) % 12 + 1
                               : token == 'm' ? value.minute()
                               : token == 's' ? value.second()
                                              : value.millisecond();
            text += detail::picker_number(number, static_cast<int>(p.text.size()));
        }
    }
    return String::from_utf8(text).value();
}

TimePickerValue parse_time_of_day(std::string_view text, std::string_view format, const PickerLocale& locale) {
    validate_time_format(format);
    locale.validate();
    int hour{};
    int minute{};
    int second{};
    int millisecond{};
    bool twelve{};
    bool pm{};
    std::size_t offset{};
    for (const auto& p : detail::tokenize_picker_format(format, "HhmsSaA")) {
        if (!p.token) {
            if (!detail::picker_read_literal(text, offset, p.text)) {
                return {};
            }
            continue;
        }
        const char token = p.text[0];
        if (token == 'a' || token == 'A') {
            auto am = std::string{locale.am.bytes()};
            auto afternoon = std::string{locale.pm.bytes()};
            if (token == 'a') {
                for (auto* label : {&am, &afternoon}) {
                    for (auto& c : *label) {
                        c = static_cast<char>(std::tolower(static_cast<unsigned char>(c)));
                    }
                }
            }
            if (detail::picker_read_literal(text, offset, afternoon)) {
                pm = true;
            } else if (!detail::picker_read_literal(text, offset, am)) {
                return {};
            }
        } else {
            const int width = static_cast<int>(p.text.size());
            const auto n = detail::picker_read_number(text, offset, width == 1 ? 1 : width, width == 1 ? 2 : width);
            if (!n) {
                return {};
            }
            if (token == 'H' || token == 'h') {
                hour = *n;
                twelve = token == 'h';
            } else if (token == 'm') {
                minute = *n;
            } else if (token == 's') {
                second = *n;
            } else {
                millisecond = *n;
            }
        }
    }
    if (offset != text.size() || hour > (twelve ? 12 : 23) || (twelve && hour == 0) || minute > 59 || second > 59) {
        return {};
    }
    return TimeOfDay{twelve ? hour % 12 + (pm ? 12 : 0) : hour, minute, second, millisecond};
}
} // namespace ryn

#include <ryn/date_picker.hpp>
#include <chrono>

namespace ryn {
namespace {
std::chrono::sys_days civil_days(CivilDate value) {
    return std::chrono::year{value.year()} / value.month() / value.day();
}

std::chrono::sys_days week_start(int year, const PickerLocale& locale) {
    const std::chrono::sys_days reference = std::chrono::year{year} / 1 / locale.first_week_contains;
    return reference - std::chrono::days{(std::chrono::weekday{reference}.c_encoding() + 7 - locale.first_day) % 7};
}
} // namespace

CalendarWeek calendar_week(CivilDate value, const PickerLocale& locale) {
    locale.validate();
    const auto date = civil_days(value);
    int year = value.year();
    if (date < week_start(year, locale)) {
        --year;
    } else if (date >= week_start(year + 1, locale)) {
        ++year;
    }
    return {year, static_cast<unsigned>((date - week_start(year, locale)).count() / 7 + 1)};
}

void validate_date_format(std::string_view format) {
    std::set<char> fields;
    for (const auto& p : detail::tokenize_picker_format(format, "YMDQgw")) {
        if (!p.token) {
            continue;
        }
        const auto& t = p.text;
        if (t != "YYYY" && t != "M" && t != "MM" && t != "MMM" && t != "MMMM" && t != "D" && t != "DD" && t != "Q" &&
            t != "gggg" && t != "w" && t != "ww") {
            throw std::invalid_argument("Unsupported date format token");
        }
        if (!fields.insert(t[0]).second) {
            throw std::invalid_argument("Duplicate date format field");
        }
    }
    if ((fields.contains('g') != fields.contains('w')) || (fields.contains('Q') && fields.contains('M')) ||
        (fields.contains('w') &&
         (fields.contains('M') || fields.contains('D') || fields.contains('Y') || fields.contains('Q')))) {
        throw std::invalid_argument("Invalid date format field combination");
    }
}

String format_date(CivilDate date, std::string_view format, const PickerLocale& locale) {
    validate_date_format(format);
    locale.validate();
    std::string result;
    for (const auto& p : detail::tokenize_picker_format(format, "YMDQgw")) {
        if (!p.token) {
            result += p.text;
            continue;
        }
        const auto t = p.text[0];
        const int n = t == 'Y'   ? date.year() + locale.year_offset
                      : t == 'M' ? static_cast<int>(date.month())
                      : t == 'D' ? static_cast<int>(date.day())
                      : t == 'Q' ? static_cast<int>((date.month() - 1) / 3 + 1)
                      : t == 'g' ? calendar_week(date, locale).year + locale.year_offset
                                 : static_cast<int>(calendar_week(date, locale).week);
        if (t == 'M' && p.text.size() >= 3) {
            result +=
                (p.text.size() == 3 ? locale.short_months[date.month() - 1] : locale.months[date.month() - 1]).bytes();
        } else {
            result += detail::picker_number(n, static_cast<int>(p.text.size()));
        }
    }
    return String::from_utf8(result).value();
}

DatePickerValue parse_date(std::string_view text, std::string_view format, const PickerLocale& locale) {
    validate_date_format(format);
    locale.validate();
    int year{};
    int month{1};
    int day{1};
    int week{};
    bool has_week{};
    std::size_t offset{};
    for (const auto& p : detail::tokenize_picker_format(format, "YMDQgw")) {
        if (!p.token) {
            if (!detail::picker_read_literal(text, offset, p.text)) {
                return {};
            }
            continue;
        }
        const char t = p.text[0];
        if (t == 'M' && p.text.size() >= 3) {
            std::optional<unsigned> matched;
            std::size_t length{};
            for (unsigned i = 0; i < 12; ++i) {
                const auto name = (p.text.size() == 3 ? locale.short_months[i] : locale.months[i]).bytes();
                if (text.substr(offset, name.size()) == name && name.size() > length) {
                    matched = i + 1;
                    length = name.size();
                }
            }
            if (!matched) {
                return {};
            }
            month = static_cast<int>(*matched);
            offset += length;
            continue;
        }
        const int width = static_cast<int>(p.text.size());
        bool negative{};
        if ((t == 'Y' || t == 'g') && offset < text.size() && text[offset] == '-') {
            negative = true;
            ++offset;
        }
        const auto n = detail::picker_read_number(text, offset, width == 1 ? 1 : width,
                                                  t == 'Y' || t == 'g' ? 5
                                                  : width == 1         ? 2
                                                                       : width);
        if (!n) {
            return {};
        }
        if (t == 'Y' || t == 'g') {
            year = (negative ? -*n : *n) - locale.year_offset;
        } else if (t == 'M') {
            month = *n;
        } else if (t == 'D') {
            day = *n;
        } else if (t == 'Q') {
            if (*n < 1 || *n > 4) {
                return {};
            }
            month = (*n - 1) * 3 + 1;
        } else {
            week = *n;
            has_week = true;
        }
    }
    if (offset != text.size()) {
        return {};
    }
    if (has_week) {
        if (year < 0 || year > 10000 || week < 1 || week > 53) {
            return {};
        }
        auto date = week_start(year, locale) + std::chrono::days{(week - 1) * 7};
        // At the CivilDate boundary the first in-domain day represents the week.
        date = std::max(date, civil_days(CivilDate{1, 1, 1}));
        if (date > civil_days(CivilDate{9999, 12, 31})) {
            return {};
        }
        const std::chrono::year_month_day ymd{date};
        CivilDate result{int(ymd.year()), unsigned(ymd.month()), unsigned(ymd.day())};
        if (calendar_week(result, locale) != CalendarWeek{year, static_cast<unsigned>(week)}) {
            return {};
        }
        return result;
    }
    if (!CivilDate::valid(year, static_cast<unsigned>(month), static_cast<unsigned>(day))) {
        return {};
    }
    return CivilDate{year, static_cast<unsigned>(month), static_cast<unsigned>(day)};
}
} // namespace ryn

namespace ryn {
void validate_date_time_format(std::string_view format) {
    const auto [date, time] = detail::split_date_time_format(format);
    validate_date_format(date);
    validate_time_format(time);
}

String format_date_time(CivilDateTime value, std::string_view format, const PickerLocale& locale) {
    const auto [date, time] = detail::split_date_time_format(format);
    return String::from_utf8(std::string{format_date(value.date, date, locale).bytes()} +
                             std::string{format_time_of_day(value.time, time, locale).bytes()})
        .value();
}

DatePickerDateTimeValue parse_date_time(std::string_view text, std::string_view format, const PickerLocale& locale) {
    const auto [date, time] = detail::split_date_time_format(format);
    validate_date_format(date);
    validate_time_format(time);
    if (text.size() > 256) {
        return {};
    }
    for (std::size_t split = 1; split < text.size(); ++split) {
        const auto d = parse_date(text.substr(0, split), date, locale);
        if (!d) {
            continue;
        }
        const auto t = parse_time_of_day(text.substr(split), time, locale);
        if (t) {
            return CivilDateTime{*d, *t};
        }
    }
    return {};
}
} // namespace ryn
