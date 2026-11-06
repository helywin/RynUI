#include "component/calendar_component.hpp"
#include "component/date_picker_component.hpp"
#include "component/time_picker_component.hpp"
#include "support/i18n_test.hpp"
#include "support/input_fixture.hpp"

#include <array>

void i18n_pickers_retained_cases();

namespace {
using namespace ryn;
using namespace ryn_test::i18n;
using Fixture = ryn_test::input_component::Fixture;

void sync(Fixture& fixture) {
    require(fixture.services.layout_and_synchronize({1000, 1400}, {0, 0, 1000, 1400}), "picker i18n layout");
}

bool descendant(Fixture& fixture, runtime::ComponentId child, runtime::ComponentId owner) {
    for (auto current = std::optional{child}; current; current = fixture.services.components().parent(*current)) {
        if (*current == owner) {
            return true;
        }
    }
    return false;
}

bool has_text(Fixture& fixture, runtime::ComponentId owner, String expected) {
    for (const auto& text : fixture.services.text().mounted_texts()) {
        if (descendant(fixture, text.component, owner) &&
            fixture.scene.text_state(text.scene).content() == expected.view()) {
            return true;
        }
    }
    return false;
}

String placeholder(Fixture& fixture, runtime::ComponentId input) {
    return String::from_utf8(
               std::as_const(fixture.scene).text_state(fixture.inputs.text_layers(input).placeholder).content().bytes())
        .value();
}

void nested_windows_and_defaults() {
    std::cerr << "case: nested_windows_and_defaults" << std::endl;
    Fixture fixture;
    Signal<LocaleId> language{locales::zh_CN};
    int mounts = 0;
    int changes = 0;
    fixture.services.mount(Content{[&] {
        ConfigProvider(
            ConfigProviderProps{}.locale(language), ConfigProviderContent{[&] {
                ++mounts;
                DatePicker(
                    DatePickerProps{}.defaultPanelDate({2026, 11, 1}).defaultOpen().onChange([&](auto) { ++changes; }));
                ConfigProvider(ConfigProviderProps{}.locale(locales::en_US), ConfigProviderContent{[&] {
                                   TimePicker(
                                       TimePickerProps{}.defaultOpen().showNow().clock([] { return TimeOfDay{13}; }));
                               }});
            }});
    }});
    sync(fixture);
    const auto date_id = fixture.services.date_picker().mounted().front();
    const auto time_id = fixture.services.time_picker().mounted().front();
    const auto date = fixture.services.date_picker().snapshot(date_id);
    const auto time = fixture.services.time_picker().snapshot(time_id);
    require(placeholder(fixture, date.input) == String{u8"请选择日期"} &&
                placeholder(fixture, time.selector) == String{u8"Select a time"} &&
                has_text(fixture, time.popup, u8"Now") && has_text(fixture, date.popup, u8"2026年11月"),
            "nested language or actual default placeholder/panel text failed");
    Fixture second;
    second.services.mount(Content{[] {
        TimePicker(TimePickerProps{});
        DatePicker(DatePickerProps{}.showTime());
        DatePicker(DatePickerProps{}.showTime().placeholder(u8"Explicit date and time"));
    }});
    sync(second);
    const auto second_id = second.services.time_picker().mounted().front();
    const auto second_input = second.services.time_picker().snapshot(second_id).selector;
    language.set(locales::en_US);
    sync(fixture);
    require(placeholder(fixture, date.input) == String{u8"Select date"} &&
                has_text(fixture, date.popup, u8"November 2026") && mounts == 1 && changes == 0 &&
                fixture.services.date_picker().snapshot(date_id).input == date.input &&
                fixture.services.date_picker().snapshot(date_id).popup == date.popup &&
                fixture.services.time_picker().snapshot(time_id).rows == time.rows,
            "translation changed business state, remounted a picker, or missed panel text");
    language.set(locales::zh_CN);
    sync(fixture);
    require(placeholder(second, second_input) == String{u8"Select a time"},
            "one window changed another window default");
    require(
        placeholder(second, second.services.date_picker().snapshot(second.services.date_picker().mounted()[0]).input) ==
                String{u8"Select date"} &&
            placeholder(second,
                        second.services.date_picker().snapshot(second.services.date_picker().mounted()[1]).input) ==
                String{u8"Explicit date and time"},
        "no-Provider showTime placeholder compatibility or explicit placeholder priority changed");
}

void complete_overrides_and_mode_placeholders() {
    std::cerr << "case: complete_overrides_and_mode_placeholders" << std::endl;
    Fixture fixture;
    Signal<LocaleId> language{locales::zh_CN};
    Signal<DatePickerMode> mode{DatePickerMode::Year};
    auto empty = PickerLocale{};
    empty.confirm = {};
    empty.cancel = {};
    empty.time_placeholder = {};
    fixture.services.mount(Content{[&] {
        ConfigProvider(ConfigProviderProps{}.locale(language), ConfigProviderContent{[&] {
                           DatePicker(DatePickerProps{}.defaultPanelDate({2026, 11, 1}).picker(mode));
                           DatePicker(DatePickerProps{}.locale(PickerLocale{}).placeholder(u8"Select date"));
                           TimePicker(TimePickerProps{}.locale(empty).defaultOpen());
                           TimePicker(TimePickerProps{}.placeholder(u8""));
                       }});
    }});
    sync(fixture);
    const auto inherited_id = fixture.services.date_picker().mounted()[0];
    const auto explicit_id = fixture.services.date_picker().mounted()[1];
    const auto inherited = fixture.services.date_picker().snapshot(inherited_id).input;
    const auto explicit_input = fixture.services.date_picker().snapshot(explicit_id).input;
    const auto time_id = fixture.services.time_picker().mounted()[0];
    const auto time = fixture.services.time_picker().snapshot(time_id);
    require(placeholder(fixture, inherited) == String{u8"请选择年份"} &&
                placeholder(fixture, explicit_input) == String{u8"Select date"} &&
                placeholder(fixture, time.selector).empty() && !has_text(fixture, time.popup, u8"确定") &&
                !has_text(fixture, time.popup, u8"OK"),
            "explicit complete English/empty Locale or literal did not outrank defaults");
    const std::array modes{DatePickerMode::Quarter, DatePickerMode::Month, DatePickerMode::Week};
    const std::array<String, 3> expected{u8"请选择季度", u8"请选择月份", u8"请选择周"};
    for (std::size_t i = 0; i < modes.size(); ++i) {
        mode.set(modes[i]);
        sync(fixture);
        require(placeholder(fixture, inherited) == expected[i], "mode placeholder did not update retained Input");
    }
    language.set(locales::en_US);
    sync(fixture);
    require(placeholder(fixture, inherited) == String{u8"Select week"} &&
                placeholder(fixture, explicit_input) == String{u8"Select date"} &&
                placeholder(fixture, time.selector).empty(),
            "language update overwrote explicit English/empty content");
}

void compound_ranges_and_rich_footer() {
    std::cerr << "case: compound_ranges_and_rich_footer" << std::endl;
    Fixture fixture;
    Signal<LocaleId> language{locales::zh_CN};
    Signal<DatePickerRangeValue> controlled{DatePickerRangeValue{}};
    Signal<bool> open{true};
    int changes = 0;
    int footer_mounts = 0;
    fixture.services.mount(Content{[&] {
        ConfigProvider(ConfigProviderProps{}.locale(language), ConfigProviderContent{[&] {
                           DateRangePicker(DateRangePickerProps{}
                                               .value(controlled)
                                               .open(open)
                                               .picker(DatePickerProps{}.picker(DatePickerMode::Month))
                                               .onChange([&](auto) { ++changes; }),
                                           DatePickerFooter{[&] {
                                               ++footer_mounts;
                                               Input(InputProps{}.defaultValue(u8"业务草稿"));
                                           }});
                           DateRangePicker(DateTimeRangePickerProps{}.defaultOpen());
                           TimeRangePicker(TimeRangePickerProps{}.defaultOpen());
                           TimeRangePicker(TimeRangePickerProps{}.picker(TimePickerProps{}.locale(PickerLocale{})));
                       }});
    }});
    sync(fixture);
    const auto dates = fixture.services.date_picker().mounted();
    const auto times = fixture.services.time_picker().mounted();
    const auto first = fixture.services.date_picker().snapshot(dates[0]);
    require(placeholder(fixture, first.input) == String{u8"开始月份"} &&
                placeholder(fixture, fixture.services.date_picker().snapshot(dates[1]).input) == String{u8"结束月份"} &&
                has_text(fixture, first.footer, u8"清除范围") && has_text(fixture, first.footer, u8"确认范围") &&
                has_text(fixture, first.footer, u8"取消范围"),
            "date range inherited defaults were sealed by English child Locale");
    // Input placeholder and selected glyph layers share the displayed text state.
    // Verify visible placeholders while empty, then retain a nonempty controlled range.
    controlled.set({CivilDate{2026, 11, 1}, CivilDate{2026, 11, 5}});
    sync(fixture);
    // DateTimePicker mounts its internal time selector first; the standalone range is last.
    const auto standalone_start = times[times.size() - 4];
    const auto explicit_start = times[times.size() - 2];
    require(placeholder(fixture, fixture.services.time_picker().snapshot(standalone_start).selector) ==
                    String{u8"开始时间"} &&
                placeholder(fixture, fixture.services.time_picker().snapshot(explicit_start).selector) ==
                    String{u8"Start time"},
            "time range inheritance or explicit nested Locale failed");
    const int footer_count = footer_mounts;
    const auto input_count = fixture.inputs.mounted_inputs().size();
    language.set(locales::en_US);
    sync(fixture);
    require(has_text(fixture, first.footer, u8"Clear range") && has_text(fixture, first.footer, u8"Confirm range") &&
                has_text(fixture, first.footer, u8"Cancel range") && footer_mounts == footer_count &&
                fixture.inputs.mounted_inputs().size() == input_count && changes == 0 && open.get() &&
                fixture.services.date_picker().snapshot(dates[0]).value == controlled.get().start,
            "range translation lost footer/Input identity, changed controlled values, or emitted callbacks");
}

void calendar_retains_date_keys_and_header() {
    std::cerr << "case: calendar_retains_date_keys_and_header" << std::endl;
    Fixture fixture;
    Signal<LocaleId> language{locales::en_US};
    int header_mounts = 0;
    CalendarRef reference;
    int changes = 0;
    fixture.services.mount(Content{[&] {
        ConfigProvider(ConfigProviderProps{}.locale(language), ConfigProviderContent{[&] {
                           Calendar(CalendarProps{}.defaultValue({2026, 11, 7}).onChange([&](auto) { ++changes; }));
                           Calendar(CalendarProps{}
                                        .defaultValue({2026, 11, 7})
                                        .ref(reference)
                                        .headerRender(CalendarHeaderRenderer{[&](CalendarHeaderContext context) {
                                            ++header_mounts;
                                            return CalendarHeader{
                                                [context] { Input(InputProps{}.defaultValue(u8"header draft")); }};
                                        }}));
                       }});
    }});
    sync(fixture);
    const auto id = fixture.services.calendar().mounted()[0];
    const auto before = fixture.services.calendar().snapshot(id);
    require(has_text(fixture, before.header, u8"Month") && has_text(fixture, before.header, u8"Year"),
            "English Calendar header labels missing");
    language.set(locales::zh_CN);
    sync(fixture);
    const auto after = fixture.services.calendar().snapshot(id);
    require(has_text(fixture, after.header, u8"月") && has_text(fixture, after.header, u8"年") &&
                before.header == after.header && header_mounts == 1 && before.value == after.value && changes == 0,
            "Calendar labels did not translate or header/business identity changed");
    for (std::size_t i = 0; i < before.cells.size(); ++i) {
        for (std::size_t j = 0; j < after.cells.size(); ++j) {
            if (before.cells[i].date == after.cells[j].date) {
                require(before.components[i] == after.components[j],
                        "Calendar reused index instead of retained date key");
            }
        }
    }
    fixture.services.dispose();
    language.set(locales::en_US);
    require(!reference.focus(), "Calendar late ref survived disposal");
}

} // namespace

int main() {
    suppress_dialogs();
    try {
        nested_windows_and_defaults();
        complete_overrides_and_mode_placeholders();
        compound_ranges_and_rich_footer();
        calendar_retains_date_keys_and_header();
        i18n_pickers_retained_cases();
        std::cout << "i18n picker components: 12 contract groups passed\n";
        return 0;
    } catch (const std::exception& error) {
        std::cerr << error.what() << '\n';
        return 1;
    }
}
