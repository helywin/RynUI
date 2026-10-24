#include "component/date_picker_model.hpp"
#include <iostream>
#include <stdexcept>
using namespace ryn;
using namespace ryn::detail;

void check(bool value, const char* message) {
    if (!value) {
        throw std::runtime_error(message);
    }
}

int main() {
    try {
        for (const auto date :
             {CivilDate{1, 1, 1}, CivilDate{9999, 12, 31}, CivilDate{2000, 2, 29}, CivilDate{2024, 2, 29}}) {
            check(parse_iso_date(format_iso_date(date).bytes()) == DatePickerValue{date}, "ISO round trip");
        }
        for (auto text : {"1900-02-29", "2100-02-29", "0000-01-01", "10000-01-01", "2024-2-29", "2024-02-30",
                          "2024-13-01", "2024-01-00", " 2024-01-01", "2024-01-01Z", "202a-01-01", ""}) {
            check(!parse_iso_date(text), "invalid ISO accepted");
        }
        check(!CivilDate::valid(2024, 257, 1) && !CivilDate::valid(2024, 1, 257), "oversized month/day wrapped");
        bool threw = false;
        try {
            CivilDate bad{1900, 2, 29};
        } catch (const std::invalid_argument&) {
            threw = true;
        }
        check(threw, "invalid construction");
        DatePickerModel m{{}, false, {2024, 1, 31}};
        m.navigate(1);
        check(m.active() == CivilDate{2024, 2, 29}, "leap month clamp");
        m.navigate(12);
        check(m.active() == CivilDate{2025, 2, 28}, "year navigation");
        m.boundary(false);
        check(m.active() == CivilDate{2025, 2, 1}, "Home");
        m.boundary(true);
        m.move(1);
        check(m.active() == CivilDate{2025, 3, 1}, "day navigation");
        DatePickerModel first{{}, false, {1, 1, 1}};
        check(!first.move(-1) && !first.navigate(-12) && !first.grid()[0], "year 1 boundary");
        DatePickerModel last{{}, false, {9999, 12, 31}};
        check(!last.move(1) && !last.navigate(12) && !last.grid().back(), "year 9999 boundary");
        const auto grid = DatePickerModel{{}, false, {2024, 2, 1}}.grid();
        check(grid.size() == 42 && grid.front() == DatePickerValue{CivilDate{2024, 1, 28}} &&
                  grid.back() == DatePickerValue{CivilDate{2024, 3, 9}},
              "42 cell Sunday grid");
        DatePickerModel controlled{{CivilDate{2024, 2, 29}}, true, {2024, 1, 1}};
        check(controlled.request(CivilDate{2024, 3, 1}) == std::optional<DatePickerValue>{CivilDate{2024, 3, 1}} &&
                  controlled.value() == DatePickerValue{CivilDate{2024, 2, 29}},
              "controlled rejection");
        controlled.reconcile(CivilDate{2024, 3, 1});
        check(controlled.active() == CivilDate{2024, 3, 1}, "controlled update");
        int calls = 0;
        m.configure({CivilDate{2024, 2, 10}, CivilDate{2024, 2, 20}, [&](CivilDate d) {
                         ++calls;
                         return d.day() == 15;
                     }});
        check(!m.request(CivilDate{2024, 2, 9}) && !m.request(CivilDate{2024, 2, 15}), "disabled submission");
        check(m.request(CivilDate{2024, 2, 16}) && m.value() == DatePickerValue{CivilDate{2024, 2, 16}},
              "uncontrolled selection");
        m.navigate(1000);
        check(m.active() == CivilDate{2024, 2, 20}, "maximum range navigation");
        m.configure({{}, {}, [&](CivilDate) {
                         ++calls;
                         return true;
                     }});
        calls = 0;
        m.move(7);
        m.navigate(-12);
        m.boundary(true);
        check(calls == 0 && !m.request(m.active()) && calls == 1, "disabled rule performed search");
        check(m.request({}) && !m.value(), "clear");
        std::cout << "DatePicker model passed\n";
    } catch (const std::exception& e) {
        std::cerr << e.what() << '\n';
        return 1;
    }
}
