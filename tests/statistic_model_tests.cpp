#include "component/statistic_model.hpp"
#include <array>
#include <iostream>
#include <limits>
#include <stdexcept>

namespace {
using namespace ryn;
using namespace ryn::detail;

void check(bool ok, const char* message) {
    if (!ok) {
        throw std::runtime_error(message);
    }
}

void expect(StatisticValue value, std::string_view expected, std::optional<int> precision = {}) {
    StatisticModel model;
    model.value = std::move(value);
    model.precision = precision;
    check(format_statistic(model).bytes() == expected, "Statistic formatting mismatch");
}
} // namespace

int main() {
    try {
        expect(std::numeric_limits<std::uint64_t>::max(), "18,446,744,073,709,551,615");
        expect(std::numeric_limits<std::int64_t>::min(), "-9,223,372,036,854,775,808");
        expect(1.234567890123, "1.234567890123");
        expect(-112893.12345, "-112,893.12", 2);
        expect(u8"-1234.123456789", "-1,234.12345678", 8);
        expect(u8".5", "0.500", 3);
        expect(u8"", "0");
        expect(u8"-", "-");
        expect(u8"1e3", "1e3", 2);
        expect(u8"12.", "12.");
        expect(u8"+12", "+12");
        expect(u8"0000.123", "0,000", -7);
        expect(-0.0, "-0");
        expect(0.0, "0");
        expect(-0.0, "-0.00", 2);
        expect(0.0, "0.00", 2);
        StatisticModel model;
        model.value = u8"123456.789";
        model.group_separator = u8"__";
        model.decimal_separator = u8",";
        check(format_statistic(model).bytes() == "123__456,789", "custom separators missing");
        model.formatter = [](const StatisticValue& value) {
            return std::get<String>(value.storage());
        };
        check(format_statistic(model).bytes() == "123456.789", "formatter did not receive raw value");
        model.formatter = {};
        for (double value : {std::numeric_limits<double>::quiet_NaN(), std::numeric_limits<double>::infinity(),
                             -std::numeric_limits<double>::infinity()}) {
            model.value = value;
            bool rejected = false;
            try {
                static_cast<void>(format_statistic(model));
            } catch (const std::invalid_argument&) {
                rejected = true;
            }
            check(rejected, "nonfinite numeric accepted");
        }
        model.value = 42;
        model.precision = 101;
        bool rejected = false;
        try {
            static_cast<void>(format_statistic(model));
        } catch (const std::invalid_argument&) {
            rejected = true;
        }
        check(rejected, "unbounded precision accepted");
        model.precision = 2;
        model.group_separator = u8",";
        model.decimal_separator = u8".";
        check(format_statistic(model).bytes() == "42.00", "format recovery failed");
        const std::array values{StatisticValue{std::int64_t{7}}, StatisticValue{std::uint64_t{7}}, StatisticValue{7.0},
                                StatisticValue{u8"7"}};
        for (std::size_t i = 0; i < values.size(); ++i) {
            for (std::size_t j = 0; j < values.size(); ++j) {
                check((values[i] == values[j]) == (i == j), "Statistic variant equality changed");
            }
        }
        const auto nan = StatisticValue{std::numeric_limits<double>::quiet_NaN()};
        check(nan != nan && StatisticValue{1.5} != StatisticValue{2.5} &&
                  StatisticValue{u8"7"} != StatisticValue{u8"8"},
              "Statistic nonzero/String/NaN equality changed");
        check(StatisticValue{0.0} == StatisticValue{0.0} && StatisticValue{-0.0} == StatisticValue{-0.0},
              "same-sign zero equality changed");
        check(StatisticValue{0.0} != StatisticValue{-0.0} && StatisticValue{-0.0} != StatisticValue{0.0},
              "opposite-sign double zeros compare equal");
        std::cout << "Statistic model passed\n";
        return 0;
    } catch (const std::exception& e) {
        std::cerr << e.what() << '\n';
        return 1;
    }
}
