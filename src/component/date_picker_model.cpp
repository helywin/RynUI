#include "component/date_picker_model.hpp"
#include <algorithm>
#include <chrono>
#include <stdexcept>

namespace ryn {
bool CivilDate::valid(int year, unsigned month, unsigned day) noexcept {
    return year >= 1 && year <= 9999 && month >= 1 && month <= 12 && day >= 1 && day <= 31 &&
           std::chrono::year_month_day{std::chrono::year{year}, std::chrono::month{month}, std::chrono::day{day}}.ok();
}

CivilDate::CivilDate(int year, unsigned month, unsigned day) : year_(year), month_(month), day_(day) {
    if (!valid(year, month, day)) {
        throw std::invalid_argument("CivilDate requires a valid Gregorian date in years 1..9999");
    }
}

DatePickerValue parse_iso_date(std::string_view text) {
    if (text.size() != 10 || text[4] != '-' || text[7] != '-') {
        return {};
    }
    int year = 0;
    unsigned month = 0;
    unsigned day = 0;
    for (std::size_t i = 0; i < text.size(); ++i) {
        if (i == 4 || i == 7) {
            continue;
        }
        if (text[i] < '0' || text[i] > '9') {
            return {};
        }
        if (i < 4) {
            year = year * 10 + text[i] - '0';
        } else if (i < 7) {
            month = month * 10 + text[i] - '0';
        } else {
            day = day * 10 + text[i] - '0';
        }
    }
    if (!CivilDate::valid(year, month, day)) {
        return {};
    }
    return CivilDate{year, month, day};
}

String format_iso_date(CivilDate value) {
    std::string result = "0000-00-00";
    int year = value.year();
    for (int i = 3; i >= 0; --i) {
        result[static_cast<std::size_t>(i)] = static_cast<char>('0' + year % 10);
        year /= 10;
    }
    result[5] = static_cast<char>('0' + value.month() / 10);
    result[6] = static_cast<char>('0' + value.month() % 10);
    result[8] = static_cast<char>('0' + value.day() / 10);
    result[9] = static_cast<char>('0' + value.day() % 10);
    return String::from_utf8(result).value();
}
} // namespace ryn

namespace ryn::detail {
namespace {
std::chrono::sys_days days(CivilDate date) {
    return std::chrono::year{date.year()} / date.month() / date.day();
}

CivilDate civil(std::chrono::sys_days date) {
    const std::chrono::year_month_day value{date};
    return {int(value.year()), unsigned(value.month()), unsigned(value.day())};
}

CivilDate clamped(CivilDate date, const DatePickerRules& rules) {
    return std::clamp(date, rules.min.value_or(CivilDate{1, 1, 1}), rules.max.value_or(CivilDate{9999, 12, 31}));
}
} // namespace

DatePickerModel::DatePickerModel(DatePickerValue value, bool controlled, CivilDate panel, DatePickerRules rules)
    : value_(value), controlled_(controlled), active_(value.value_or(panel)) {
    mode_ = rules.type;
    configure(std::move(rules));
}

void DatePickerModel::configure(DatePickerRules rules) {
    if (rules.min && rules.max && *rules.min > *rules.max) {
        throw std::invalid_argument("DatePicker minDate exceeds maxDate");
    }
    rules_ = std::move(rules);
    active_ = clamped(active_, rules_);
}

void DatePickerModel::reconcile(DatePickerValue value) {
    if (value_ != value) {
        value_ = value;
        if (value) {
            active_ = clamped(*value, rules_);
        }
    }
}

bool DatePickerModel::eligible(CivilDate date) const {
    return eligible(date, {mode_, locale_.first_day});
}

bool DatePickerModel::eligible(CivilDate date, DateValueSemantics semantics) const {
    if (!owner_active()) {
        return false;
    }
    auto first = date;
    auto last = date;
    const auto mode = semantics.mode;
    if (mode == DatePickerMode::Month || mode == DatePickerMode::Quarter) {
        const auto month = mode == DatePickerMode::Month ? date.month() : (date.month() - 1) / 3 * 3 + 1;
        first = CivilDate{date.year(), month, 1};
        const auto final_month = mode == DatePickerMode::Month ? month : month + 2;
        unsigned day = 31;
        while (!CivilDate::valid(date.year(), final_month, day)) {
            --day;
        }
        last = CivilDate{date.year(), final_month, day};
    } else if (mode == DatePickerMode::Year || mode == DatePickerMode::Decade) {
        const int year = mode == DatePickerMode::Year ? date.year() : std::max(1, date.year() / 10 * 10);
        first = CivilDate{year, 1, 1};
        last = CivilDate{std::min(9999, mode == DatePickerMode::Year ? year : year + 9), 12, 31};
    } else if (mode == DatePickerMode::Week) {
        first = civil(std::max(
            days(CivilDate{1, 1, 1}),
            days(date) -
                std::chrono::days{(std::chrono::weekday{days(date)}.c_encoding() + 7 - semantics.first_day) % 7}));
        last = civil(std::min(days(first) + std::chrono::days{6}, days(CivilDate{9999, 12, 31})));
    }
    return (!rules_.min || last >= *rules_.min) && (!rules_.max || first <= *rules_.max) &&
           (!rules_.disabled || !rules_.disabled(date)) && owner_active() &&
           (!rules_.info || !rules_.info(date, {mode, rules_.from})) && owner_active();
}

std::optional<DatePickerValue> DatePickerModel::request(DatePickerValue value) {
    if (value && !eligible(*value)) {
        return {};
    }
    if (value) {
        active_ = *value;
    }
    if (!controlled_) {
        value_ = value;
    }
    return std::optional<DatePickerValue>{value};
}

bool DatePickerModel::navigate(int months) {
    const int index = std::clamp((active_.year() - 1) * 12 + int(active_.month()) - 1 + months, 0, 9999 * 12 - 1);
    const int year = index / 12 + 1;
    const unsigned month = unsigned(index % 12 + 1);
    unsigned day = active_.day();
    while (!CivilDate::valid(year, month, day)) {
        --day;
    }
    const auto next = clamped(CivilDate{year, month, day}, rules_);
    if ((year != active_.year() || month != active_.month()) && next.year() == active_.year() &&
        next.month() == active_.month()) {
        return false;
    }
    const bool changed = next != active_;
    active_ = next;
    return changed;
}

bool DatePickerModel::move(int count) {
    const auto next = clamped(civil(std::clamp(days(active_) + std::chrono::days{count}, days(CivilDate{1, 1, 1}),
                                               days(CivilDate{9999, 12, 31}))),
                              rules_);
    const bool changed = next != active_;
    active_ = next;
    return changed;
}

bool DatePickerModel::boundary(bool last) {
    if (mode_ != DatePickerMode::Date && mode_ != DatePickerMode::Week) {
        CivilDate next = active_;
        if (mode_ == DatePickerMode::Month || mode_ == DatePickerMode::Quarter) {
            next = CivilDate{active_.year(), last ? (mode_ == DatePickerMode::Month ? 12U : 10U) : 1U, 1};
        } else {
            const int span = mode_ == DatePickerMode::Year ? 10 : 100;
            const int year = std::clamp(
                active_.year() / span * span + (last ? span - (mode_ == DatePickerMode::Year ? 1 : 10) : 0), 1, 9999);
            next = CivilDate{year, 1, 1};
        }
        next = clamped(next, rules_);
        const bool changed = next != active_;
        active_ = next;
        return changed;
    }

    unsigned day = last ? 31 : 1;
    while (!CivilDate::valid(active_.year(), active_.month(), day)) {
        --day;
    }
    const auto next = clamped(CivilDate{active_.year(), active_.month(), day}, rules_);
    const bool changed = next != active_;
    active_ = next;
    return changed;
}

std::array<DatePickerValue, 42> DatePickerModel::grid() const {
    std::array<DatePickerValue, 42> result;
    const auto first = days(panel());
    const auto start =
        first - std::chrono::days{(std::chrono::weekday{first}.c_encoding() + 7 - locale_.first_day) % 7};
    for (int i = 0; i < 42; ++i) {
        const auto day = start + std::chrono::days{i};
        if (day >= days(CivilDate{1, 1, 1}) && day <= days(CivilDate{9999, 12, 31})) {
            result[std::size_t(i)] = civil(day);
        }
    }
    return result;
}

void DatePickerModel::view(DatePickerMode mode, PickerLocale locale) {
    if (mode < DatePickerMode::Date || mode > DatePickerMode::Decade) {
        throw std::invalid_argument("Invalid date picker mode");
    }
    locale.validate();
    mode_ = mode;
    locale_ = std::move(locale);
}

std::size_t DatePickerModel::cell_count() const {
    return mode_ == DatePickerMode::Date || mode_ == DatePickerMode::Week ? 42
           : mode_ == DatePickerMode::Quarter                             ? 4
                                                                          : 12;
}

DatePickerValue DatePickerModel::cell(std::size_t index) const {
    if (index >= cell_count()) {
        return {};
    }
    if (mode_ == DatePickerMode::Date || mode_ == DatePickerMode::Week) {
        return grid()[index];
    }
    if (mode_ == DatePickerMode::Month || mode_ == DatePickerMode::Quarter) {
        return CivilDate{active_.year(),
                         static_cast<unsigned>(mode_ == DatePickerMode::Month ? index + 1 : index * 3 + 1), 1};
    }
    const int year = mode_ == DatePickerMode::Year ? active_.year() / 10 * 10 - 1 + static_cast<int>(index)
                                                   : active_.year() / 100 * 100 - 10 + static_cast<int>(index) * 10;
    if (mode_ == DatePickerMode::Decade && year == 0) {
        return CivilDate{1, 1, 1};
    }
    return year >= 1 && year <= 9999 ? DatePickerValue{CivilDate{year, 1, 1}} : DatePickerValue{};
}

CivilDate DatePickerModel::normalized(CivilDate date) const {
    if (mode_ == DatePickerMode::Month) {
        return {date.year(), date.month(), 1};
    }
    if (mode_ == DatePickerMode::Quarter) {
        return {date.year(), (date.month() - 1) / 3 * 3 + 1, 1};
    }
    if (mode_ == DatePickerMode::Year) {
        return {date.year(), 1, 1};
    }
    if (mode_ == DatePickerMode::Decade) {
        return {std::max(1, date.year() / 10 * 10), 1, 1};
    }
    if (mode_ == DatePickerMode::Week) {
        const auto start =
            std::max(days(CivilDate{1, 1, 1}),
                     days(date) - std::chrono::days{
                                      (std::chrono::weekday{days(date)}.c_encoding() + 7 - locale_.first_day) % 7});
        return civil(start);
    }
    return date;
}

CivilDate DatePickerModel::utc_date() {
    return civil(std::chrono::floor<std::chrono::days>(std::chrono::system_clock::now()));
}
} // namespace ryn::detail
