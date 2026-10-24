#include "component/time_picker_model.hpp"
#include <iostream>

namespace {
using namespace ryn;
using namespace ryn::detail;

void check(bool value, const char* message) {
    if (!value) {
        throw std::runtime_error(message);
    }
}

void contracts() {
    check(parse_time_of_day("00:00:00") == TimeOfDay{} && parse_time_of_day("23:59:59") == TimeOfDay{23, 59, 59},
          "time endpoints");
    for (auto text :
         {"", "1:02:03", "24:00:00", "00:60:00", "00:00:60", " 01:02:03", "01:02:03x", "01:02", "aa:bb:cc"}) {
        check(!parse_time_of_day(text), "strict seconds parse");
    }
    check(parse_time_of_day("09:30", TimePickerPrecision::Minute) == TimeOfDay{9, 30}, "minute parse");
    check(!parse_time_of_day("09:30:00", TimePickerPrecision::Minute), "minute accepts seconds");
    bool invalid = false;
    try {
        static_cast<void>(TimeOfDay{0, -1});
    } catch (const std::invalid_argument&) {
        invalid = true;
    }
    check(invalid, "invalid TimeOfDay construction");
    TimePickerModel model{{}, TimePickerPrecision::Second, {}, false};
    check(model.panel() == TimeOfDay{} && model.draft().empty(), "default panel uses clock");
    model.edit(String{u8"23:59:59"});
    check(model.text_value() && *model.text_value() == TimePickerValue{TimeOfDay{23, 59, 59}}, "valid draft");
    check(model.request(*model.text_value()).has_value() && model.value() == TimeOfDay{23, 59, 59}, "text request");
    model.edit(String{u8"25:"});
    check(!model.text_value() && model.value() == TimeOfDay{23, 59, 59}, "invalid draft published");
    model.cancel();
    check(model.draft() == String{u8"23:59:59"}, "blur/cancel restoration");
    TimePickerRules rules;
    rules.minute_step = 15;
    rules.min = TimeOfDay{8};
    rules.max = TimeOfDay{18};
    model.configure(rules, TimePickerPrecision::Minute);
    check(model.select(TimeColumn::Hour, 9) && model.select(TimeColumn::Minute, 30), "stepped columns");
    check(model.panel() == TimeOfDay{9, 30} && model.value() == TimeOfDay{23, 59, 59}, "pending is accepted value");
    check(!model.select(TimeColumn::Minute, 31) && !model.request(TimeOfDay{9, 31}), "step not enforced");
    check(model.request(model.panel()).has_value() && model.value() == TimeOfDay{9, 30}, "minute commit");
    rules.disabled = [](TimeOfDay) {
        return true;
    };
    model.configure(rules, TimePickerPrecision::Second);
    check(!model.confirmable() && !model.navigate(TimeColumn::Hour, 1) &&
              !model.navigate(TimeColumn::Minute, -1, true) && !model.navigate(TimeColumn::Second, 1),
          "all disabled navigation");
    check(!model.request(TimeOfDay{12}), "disabled request");
    check(model.request({}).has_value() && !model.value(), "clear outer/inner optional");
    rules.hour_step = 0;
    invalid = false;
    try {
        model.configure(rules, TimePickerPrecision::Second);
    } catch (const std::invalid_argument&) {
        invalid = true;
    }
    check(invalid && model.rules().hour_step == 1, "invalid rules partially published");
    TimePickerModel controlled{{}, TimePickerPrecision::Minute, TimeOfDay{8, 7, 59}, true};
    check(controlled.value() == TimeOfDay{8, 7, 59} && controlled.draft() == String{u8"08:07"},
          "controlled seconds rewritten");
    controlled.select(TimeColumn::Hour, 9);
    controlled.cancel();
    check(controlled.panel() == TimeOfDay{8, 7} && controlled.value() == TimeOfDay{8, 7, 59},
          "cancel mutated authority");
    check(controlled.request(TimeOfDay{9, 15, 45}) == std::optional<TimePickerValue>{TimeOfDay{9, 15}} &&
              controlled.value() == TimeOfDay{8, 7, 59},
          "controlled rejection");
    controlled.reconcile(TimeOfDay{10, 2, 3});
    check(controlled.value() == TimeOfDay{10, 2, 3} && controlled.draft() == String{u8"10:02"}, "controlled reconcile");
}
} // namespace

int main() {
    try {
        contracts();
        std::cout << "TimePicker model contracts passed\n";
        return 0;
    } catch (const std::exception& e) {
        std::cerr << e.what() << '\n';
        return 1;
    }
}
