#include "component/date_picker_component.hpp"
#include "component/date_picker_model.hpp"
#include "component/time_picker_component.hpp"
#include "component/time_picker_model.hpp"
#include "component/picker_range.hpp"
#include "support/input_fixture.hpp"
#include "runtime/prop_connection.hpp"
#include <chrono>
#include <iostream>

namespace {
using namespace ryn;
using namespace ryn::detail;
using namespace ryn::input;
using Fixture = ryn_test::input_component::Fixture;

void check(bool value, const char* message) {
    if (!value) {
        throw std::runtime_error(message);
    }
}

template <typename F> void rejects(F fn) {
    bool thrown{};
    try {
        fn();
    } catch (const std::invalid_argument&) {
        thrown = true;
    }
    check(thrown, "invalid configuration accepted");
}

void sync(Fixture& f) {
    check(f.services.layout_and_synchronize({700, 700}, {0, 0, 700, 700}), "layout");
}

void key(Fixture& f, Key value) {
    f.services.focus().dispatch({value, KeyAction::down});
    f.services.focus().dispatch({value, KeyAction::up});
    sync(f);
}

void click(Fixture& f, runtime::ComponentId id) {
    const auto rect = f.nodes.require(f.services.components().root(id)).bounds;
    for (auto action : {PointerAction::down, PointerAction::up}) {
        f.services.pointer().dispatch({PointerIdentity::mouse(), action, PointerButton::primary,
                                       rect.x + rect.width / 2, rect.y + rect.height / 2});
    }
    sync(f);
}

void format_contracts() {
    std::cerr << "case: format_contracts" << std::endl;
    for (int hour = 0; hour < 24; ++hour) {
        for (int ms : {0, 1, 499, 999}) {
            const TimeOfDay time{hour, 59, 58, ms};
            for (auto format : {"HH:mm:ss.SSS", "h:mm:ss.SSS a", "hh:mm:ss.SSS A", "HH[時]mm[分]ss.SSS"}) {
                const auto text = format_time_of_day(time, format);
                check(parse_time_of_day(text.bytes(), format) == time, "precise 12/24 hour format round trip");
            }
        }
    }
    check(parse_time_of_day("12:00 am", "h:mm a") == TimeOfDay{}, "midnight conversion");
    check(parse_time_of_day("12:00 pm", "h:mm a") == TimeOfDay{12}, "noon conversion");
    for (auto text : {"00:00 am", "13:00 pm", "12:60 pm", "12:00", "12:00 AMx"}) {
        check(!parse_time_of_day(text, "h:mm a"), "invalid 12 hour draft");
    }
    for (auto pattern : {"", "hh:mm", "HH:mm a", "HH:HH", "HH:mm:SS", "HH:mm["}) {
        rejects([&] { validate_time_format(pattern); });
    }
    PickerLocale iso;
    iso.first_day = 1;
    iso.first_week_contains = 4;
    check(calendar_week(CivilDate{2021, 1, 1}, iso) == CalendarWeek{2020, 53}, "ISO previous week-year");
    check(calendar_week(CivilDate{2018, 12, 31}, iso) == CalendarWeek{2019, 1}, "ISO next week-year");
    for (int year : {1, 4, 100, 400, 1900, 2000, 2020, 2021, 2026, 9999}) {
        for (unsigned month = 1; month <= 12; ++month) {
            for (unsigned day = 1; day <= 31; ++day) {
                if (!CivilDate::valid(year, month, day)) {
                    continue;
                }
                const CivilDate date{year, month, day};
                for (auto pattern : {"YYYY-MM-DD", "D/M/YYYY", "DD MMMM YYYY", "YYYY[年]MM[月]DD[日]"}) {
                    const auto text = format_date(date, pattern, iso);
                    check(parse_date(text.bytes(), pattern, iso) == date, "Gregorian format round trip");
                }
                const auto week = format_date(date, "gggg-[W]ww", iso);
                const auto parsed = parse_date(week.bytes(), "gggg-[W]ww", iso);
                check(parsed && calendar_week(*parsed, iso) == calendar_week(date, iso), "week format round trip");
            }
        }
    }
    check(!parse_date("1900-02-29", "YYYY-MM-DD"), "century leap rule");
    check(parse_date("2000-02-29", "YYYY-MM-DD") == CivilDate{2000, 2, 29}, "400 year leap rule");
    for (int offset : {-9998, -6, 543, 9998}) {
        PickerLocale locale;
        locale.year_offset = offset;
        for (auto date : {CivilDate{1, 1, 1}, CivilDate{9999, 12, 31}}) {
            const auto text = format_date(date, "YYYY-MM-DD", locale);
            check(parse_date(text.bytes(), "YYYY-MM-DD", locale) == date, "era offset round trip");
        }
    }
    check(!parse_date("2026-W00", "gggg-[W]ww", iso), "week zero accepted");
    PickerLocale localized;
    localized.months[5] = String{u8"juin"};
    localized.months[6] = String{u8"juillet"};
    localized.short_months[5] = String{u8"juin"};
    localized.short_months[6] = String{u8"juil"};
    for (auto date : {CivilDate{2026, 6, 5}, CivilDate{2026, 7, 5}}) {
        const auto text = format_date(date, "DD MMM YYYY", localized);
        check(parse_date(text.bytes(), "DD MMM YYYY", localized) == date, "explicit localized month abbreviation");
    }
    localized.short_months[6] = localized.short_months[5];
    rejects([&] { localized.validate(); });
    rejects([] { validate_date_format("YYYY-MM-MM"); });
    rejects([] {
        PickerLocale locale;
        locale.first_day = 7;
        locale.validate();
    });
}

void time_rules() {
    std::cerr << "case: time_rules" << std::endl;
    TimePickerRules rules;
    rules.millisecond_step = 125;
    rules.minute_step = 15;
    rules.disabled_milliseconds = [](int h, int m, int s, int ms) {
        return h == 12 && m == 0 && s == 0 && ms == 500;
    };
    TimePickerModel model{rules, TimePickerPrecision::Millisecond, TimeOfDay{12}, false};
    model.configure_display(true, String{u8"h:mm:ss.SSS A"}, {});
    check(model.row_count(TimeColumn::Hour) == 12 && model.row_value(TimeColumn::Meridiem) == 1,
          "12 hour interactive columns");
    check(model.select(TimeColumn::Millisecond, 125) && model.panel().millisecond() == 125, "millisecond selection");
    check(!model.select(TimeColumn::Millisecond, 500) && !model.select(TimeColumn::Millisecond, 999),
          "disabled/step millisecond");
    check(model.select(TimeColumn::Meridiem, 0) && model.panel().hour() == 0, "midnight meridiem");
    check(model.select(TimeColumn::Hour, 11) && model.panel().hour() == 11, "11 AM");
    check(model.select(TimeColumn::Meridiem, 1) && model.panel().hour() == 23, "11 PM");
    check(model.select(TimeColumn::Hour, 0) && model.panel().hour() == 12, "noon row");
    for (int step : {1, 2, 7, 125, 1000}) {
        rules.millisecond_step = step;
        model.configure(rules, TimePickerPrecision::Millisecond);
        for (int value = 0; value < 1000; ++value) {
            const TimeOfDay t{0, 0, 0, value};
            check(model.allowed(t) == (value % step == 0), "all millisecond step boundaries");
        }
    }
    for (int step : {0, -1, 1001}) {
        rules.millisecond_step = step;
        rejects([&] { model.configure(rules, TimePickerPrecision::Millisecond); });
    }
    model.edit(String{u8"12:30:"});
    check(!model.text_value(), "partial time draft");
    model.cancel();
    check(model.value() == TimeOfDay{12}, "invalid draft changed accepted value");
}

void meridiem_steps() {
    std::cerr << "case: meridiem_steps" << std::endl;
    for (int step = 1; step <= 24; ++step) {
        TimePickerRules rules;
        rules.hour_step = step;
        TimePickerModel model{rules, TimePickerPrecision::Minute, {}, false};
        model.configure_display(true, String{}, {});
        const auto pm = model.candidate(TimeColumn::Meridiem, 1);
        bool possible{};
        for (int h = 12; h < 24; ++h) {
            possible |= h % step == 0;
        }
        check(bool(pm) == possible, "meridiem availability lost stepped PM hours");
        if (pm) {
            check(pm->hour() >= 12 && pm->hour() % step == 0, "meridiem step completion");
        }
    }
}

void ranges() {
    std::cerr << "case: ranges" << std::endl;
    for (bool controlled : {false, true}) {
        PickerRangeState<TimeOfDay> state;
        state.controlled = controlled;
        const TimePickerRangeValue original{TimeOfDay{8}, TimeOfDay{9}};
        state.accepted.set(original);
        state.pending.set(original);
        state.open.set(true);
        std::vector<int> events;
        state.on_calendar = [&](auto, auto) {
            events.push_back(1);
        };
        state.on_endpoint = [&](auto) {
            events.push_back(2);
        };
        state.on_change = [&](auto v) {
            check(v == TimePickerRangeValue{TimeOfDay{10}, TimeOfDay{11}}, "range order normalization");
            events.push_back(3);
        };
        state.on_open = [&](bool open) {
            check(!open, "range close");
            events.push_back(4);
        };
        state.choose(PickerEndpoint::Start, TimeOfDay{11});
        check(state.accepted.get() == original, "start prematurely accepted");
        state.choose(PickerEndpoint::End, TimeOfDay{10});
        check(events == std::vector<int>{1, 2, 1, 3, 4}, "range exact event ordering");
        check(state.accepted.get() == (controlled ? original : TimePickerRangeValue{TimeOfDay{10}, TimeOfDay{11}}),
              "range authority refusal");
    }
    PickerRangeState<TimeOfDay> partial;
    partial.policy.allow_empty = {true, false};
    partial.choose(PickerEndpoint::End, TimeOfDay{12});
    check(partial.accepted.get().end == TimeOfDay{12} && !partial.accepted.get().start, "allow empty start");
    partial.policy.allow_empty = {false, false};
    partial.pending.set({{}, TimeOfDay{13}});
    check(partial.commit() == PickerRequestDisposition::Rejected && partial.accepted.get().end == TimeOfDay{12},
          "invalid partial range atomicity");
    partial.disabled = [](TimeOfDay time, PickerEndpoint endpoint, const TimePickerRangeValue& range,
                          const PickerCandidateSemantics&) {
        return endpoint == PickerEndpoint::End && range.start && time < *range.start;
    };
    partial.pending.set({TimeOfDay{15}, TimeOfDay{13}});
    partial.policy.order = false;
    check(partial.commit() == PickerRequestDisposition::Rejected, "disabled time dependency");
}

void date_modes() {
    std::cerr << "case: date_modes" << std::endl;
    DatePickerModel model{{}, false, CivilDate{2026, 11, 5}};
    PickerLocale iso;
    iso.first_day = 1;
    iso.first_week_contains = 4;
    for (auto mode : {DatePickerMode::Date, DatePickerMode::Week, DatePickerMode::Month, DatePickerMode::Quarter,
                      DatePickerMode::Year, DatePickerMode::Decade}) {
        model.view(mode, iso);
        check(model.cell_count() == (mode == DatePickerMode::Date || mode == DatePickerMode::Week ? 42
                                     : mode == DatePickerMode::Quarter                            ? 4
                                                                                                  : 12),
              "interactive mode cells");
        check(!model.cell(model.cell_count()), "mode cell boundary");
    }
    DatePickerRules bounds;
    bounds.min = CivilDate{2026, 11, 5};
    bounds.max = CivilDate{2026, 11, 25};
    DatePickerModel limited{{}, false, CivilDate{2026, 11, 5}, bounds};
    limited.view(DatePickerMode::Month, iso);
    check(limited.eligible(CivilDate{2026, 11, 1}) && !limited.eligible(CivilDate{2026, 10, 1}),
          "month unit range overlap");
    limited.view(DatePickerMode::Quarter, iso);
    check(limited.eligible(CivilDate{2026, 10, 1}) && !limited.eligible(CivilDate{2026, 7, 1}),
          "quarter unit range overlap");
    limited.view(DatePickerMode::Year, iso);
    check(limited.eligible(CivilDate{2026, 1, 1}) && !limited.eligible(CivilDate{2025, 1, 1}),
          "year unit range overlap");
    model.view(DatePickerMode::Month, iso);
    check(model.cell(0) == CivilDate{2026, 1, 1} && model.cell(11) == CivilDate{2026, 12, 1}, "month panel");
    model.view(DatePickerMode::Quarter, iso);
    check(model.cell(3) == CivilDate{2026, 10, 1}, "quarter panel");
    model.panel_date(CivilDate{1, 1, 1});
    model.view(DatePickerMode::Decade, iso);
    check(model.cell(1) == CivilDate{1, 1, 1} && model.normalized(CivilDate{1, 1, 1}) == CivilDate{1, 1, 1},
          "first Gregorian decade boundary");
    model.view(DatePickerMode::Week, iso);
    check(model.normalized(CivilDate{2026, 11, 5}) == CivilDate{2026, 11, 2}, "locale week selection");
}

void native_time() {
    std::cerr << "case: native_time" << std::endl;
    Fixture f;
    TimePickerRef ref;
    Signal<TimePickerPrecision> precision{TimePickerPrecision::Minute};
    Signal<bool> twelve{true};
    int cells{};
    int footer{};
    std::vector<int> events;
    f.services.mount(Content{[&] {
        TimePicker(TimePickerProps{}
                       .ref(ref)
                       .defaultOpen()
                       .precision(precision)
                       .use12Hours(twelve)
                       .clock([] { return TimeOfDay{12, 30}; })
                       .showNow()
                       .onChange([&](auto) { events.push_back(1); })
                       .onOk([&](auto) { events.push_back(2); })
                       .onOpenChange([&](bool open) {
                           if (!open) {
                               events.push_back(3);
                           }
                       })
                       .cellRender([&](Prop<TimePickerCellInfo> info) {
                           ++cells;
                           return TimePickerCell{[info] {
                               Text(TextProps{}.content(bind([info] {
                                   return String::from_utf8(std::to_string(read_prop(info).value)).value();
                               })));
                           }};
                       }),
                   TimePickerFooter{[&] {
                       ++footer;
                       Input(InputProps{});
                   }});
    }});
    sync(f);
    const auto id = f.services.time_picker().mounted().front();
    const auto first = f.services.time_picker().snapshot(id);
    check(first.rows[4].size() == 2 && first.column_clips[4].width > 0, "actual AM/PM column");
    precision.set(TimePickerPrecision::Millisecond);
    sync(f);
    const auto milli = f.services.time_picker().snapshot(id);
    check(milli.rows[3].size() == 1000 && milli.column_clips[3].width > 0 && footer == 1,
          "actual precise column retained footer");
    const int mounted = cells;
    twelve.set(false);
    sync(f);
    check(cells == mounted && f.services.time_picker().snapshot(id).rows[0] == first.rows[0],
          "ordinary prop remounted rows");
    check(ref.focus(), "native ref");
    key(f, Key::enter);
    check(events == std::vector<int>{1, 2, 3}, "single exact event ordering");
}

void native_date() {
    std::cerr << "case: native_date" << std::endl;
    Fixture f;
    DatePickerRef ref;
    Signal<DatePickerMode> mode{DatePickerMode::Month};
    int cell_mounts{};
    int footer{};
    f.services.mount(Content{[&] {
        DatePicker(DatePickerProps{}
                       .ref(ref)
                       .picker(mode)
                       .defaultPanelDate(CivilDate{2026, 11, 1})
                       .defaultOpen()
                       .cellRender([&](Prop<DatePickerCellInfo> info) {
                           ++cell_mounts;
                           return DatePickerCell{[info] {
                               Text(TextProps{}.content(bind([info] {
                                   const auto value = read_prop(info);
                                   return value.date ? format_iso_date(*value.date) : String{};
                               })));
                           }};
                       }),
                   DatePickerFooter{[&] {
                       ++footer;
                       Input(InputProps{});
                   }});
    }});
    sync(f);
    const auto id = f.services.date_picker().mounted().front();
    const auto month = f.services.date_picker().snapshot(id);
    check(month.cells.size() == 12, "actual month panel");
    click(f, month.navigation[0]);
    const auto previous_year = f.services.date_picker().snapshot(id);
    check(previous_year.active.year() == 2025 && cell_mounts == 24, "month header did not reconcile one year's keys");
    for (auto old : month.cells) {
        check(!f.services.components().contains(old), "month key leaving grid retained a stale subtree");
    }
    mode.set(DatePickerMode::Quarter);
    sync(f);
    const auto quarter = f.services.date_picker().snapshot(id);
    check(quarter.cells.size() == 4 && cell_mounts == 28, "actual keyed quarter panel");
    for (auto old : previous_year.cells) {
        check(!f.services.components().contains(old), "mode change reused a different mode's cell identity");
    }
    mode.set(DatePickerMode::Date);
    sync(f);
    const auto date = f.services.date_picker().snapshot(id);
    check(date.cells.size() == 42 && date.cells.front() != month.cells.front() && footer == 1 && cell_mounts == 70,
          "date mode failed to mount only its own raw-date keys or remounted footer");
    for (auto old : quarter.cells) {
        check(!f.services.components().contains(old), "quarter key did not retire on date panel change");
    }
    mode.set(DatePickerMode::Date);
    sync(f);
    check(f.services.date_picker().snapshot(id).cells == date.cells && cell_mounts == 70,
          "equal mode update remounted retained date keys");
}

bool below(Fixture& f, runtime::ComponentId child, runtime::ComponentId owner) {
    for (auto current = std::optional{child}; current; current = f.services.components().parent(*current)) {
        if (*current == owner) {
            return true;
        }
    }
    return false;
}

void button(Fixture& f, std::string_view label) {
    for (const auto& text : f.services.text().mounted_texts()) {
        if (f.scene.text_state(text.scene).content().bytes() != label) {
            continue;
        }
        for (const auto& b : f.buttons.mounted_buttons()) {
            if (below(f, text.component, b.component)) {
                click(f, b.component);
                return;
            }
        }
    }
    throw std::runtime_error("action button missing");
}

void native_ranges() {
    std::cerr << "case: native_ranges" << std::endl;
    {
        Fixture f;
        rejects([&] {
            f.services.mount(Content{
                [] { TimeRangePicker(TimeRangePickerProps{}.activeEndpoint(static_cast<PickerEndpoint>(9))); }});
        });
        check(f.services.time_picker().mounted().empty(), "invalid endpoint left mounted selectors");
    }
    {
        Fixture f;
        PickerRangePolicy policy;
        policy.disabled[0] = true;
        f.services.mount(Content{[&] { TimeRangePicker(TimeRangePickerProps{}.defaultOpen().policy(policy)); }});
        sync(f);
        check(!f.services.time_picker().snapshot(f.services.time_picker().mounted()[0]).visible &&
                  f.services.time_picker().snapshot(f.services.time_picker().mounted()[1]).visible,
              "range begins at enabled endpoint");
    }
    {
        Fixture f;
        TimeRangePickerRef ref;
        Signal<TimePickerRangeValue> value{TimePickerRangeValue{TimeOfDay{9}, TimeOfDay{18}}};
        std::vector<int> events;
        TimePickerRangeValue requested;
        f.services.mount(Content{[&] {
            TimeRangePicker(TimeRangePickerProps{}
                                .value(value)
                                .ref(ref)
                                .defaultOpen()
                                .picker(TimePickerProps{}.precision(TimePickerPrecision::Minute))
                                .onCalendarChange([&](auto, auto) { events.push_back(1); })
                                .onChange([&](auto v) {
                                    requested = v;
                                    events.push_back(2);
                                })
                                .onOpenChange([&](bool open) {
                                    if (!open) {
                                        events.push_back(3);
                                    }
                                }));
        }});
        sync(f);
        check(f.services.time_picker().mounted().size() == 2, "native time range endpoints");
        const auto first = f.services.time_picker().mounted()[0];
        const auto second = f.services.time_picker().mounted()[1];
        check(f.services.time_picker().snapshot(first).visible && !f.services.time_picker().snapshot(second).visible,
              "start activation");
        check(ref.start.focus(), "start range focus");
        key(f, Key::down);
        key(f, Key::enter);
        check(events == std::vector<int>{1} && value.get().start == TimeOfDay{9} &&
                  f.services.time_picker().snapshot(second).visible,
              "pending start / end activation");
        const auto focused = f.services.focus().state().focused;
        check(focused && f.services.interactions().require(*focused).component ==
                             f.services.time_picker().snapshot(second).selector,
              "time range transfers focus to the activated end");
        key(f, Key::up);
        key(f, Key::enter);
        check(events == std::vector<int>{1, 1, 2, 3} &&
                  requested == TimePickerRangeValue{TimeOfDay{10}, TimeOfDay{17}} &&
                  value.get() == TimePickerRangeValue{TimeOfDay{9}, TimeOfDay{18}},
              "native range request order/refusal");
        check(f.services.time_picker().snapshot(first).value == TimeOfDay{9}, "refused range displayed draft");
    }
    {
        Fixture f;
        DateRangePickerRef ref;
        int changes{};
        f.services.mount(Content{[&] {
            DateRangePicker(DateRangePickerProps{}
                                .ref(ref)
                                .defaultOpen()
                                .picker(DatePickerProps{}.defaultPanelDate(CivilDate{2026, 11, 5}))
                                .onChange([&](auto) { ++changes; }));
        }});
        sync(f);
        check(f.services.date_picker().mounted().size() == 2, "native date range endpoints");
        check(ref.start.focus(), "date range start focus");
        key(f, Key::enter);
        check(changes == 0, "date range committed incomplete start");
        const auto focused = f.services.focus().state().focused;
        check(focused && f.services.interactions().require(*focused).component ==
                             f.services.date_picker().snapshot(f.services.date_picker().mounted()[1]).input,
              "date range transfers focus to the activated end");
        key(f, Key::right);
        key(f, Key::enter);
        check(changes == 1, "date range never committed");
    }
}

void range_same_value_reset() {
    std::cerr << "case: range_same_value_reset" << std::endl;
    {
        Fixture f;
        TimeRangePickerRef ref;
        int changes{};
        const TimePickerRangeValue original{TimeOfDay{9}, TimeOfDay{18}};
        f.services.mount(Content{[&] {
            TimeRangePicker(TimeRangePickerProps{}
                                .value(original)
                                .ref(ref)
                                .defaultOpen()
                                .picker(TimePickerProps{}.precision(TimePickerPrecision::Minute))
                                .onChange([&](auto) { ++changes; }));
        }});
        sync(f);
        const auto a = f.services.time_picker().mounted()[0];
        const auto b = f.services.time_picker().mounted()[1];
        check(ref.start.focus(), "range reset start focus");
        key(f, Key::down);
        key(f, Key::enter);
        check(ref.end.focus({InputFocusCursor::All}), "range reset end focus");
        check(bool(f.inputs.dispatch(TextCommitted{String{u8"17:"}, f.inputs.sessions().active()})),
              "range reset partial text");
        sync(f);
        const auto reset = capture_form_input(ref.start).second;
        check(bool(reset), "range Form reset callback");
        reset();
        sync(f);
        check(changes == 0 && f.services.time_picker().snapshot(a).pending == TimeOfDay{9} &&
                  f.services.time_picker().snapshot(b).draft == String{u8"18:00"},
              "same-value time range reset retained pending draft");
    }
    {
        Fixture f;
        DateRangePickerRef ref;
        int changes{};
        const DatePickerRangeValue original{CivilDate{2026, 11, 5}, CivilDate{2026, 11, 12}};
        f.services.mount(Content{[&] {
            DateRangePicker(
                DateRangePickerProps{}.value(original).ref(ref).defaultOpen().onChange([&](auto) { ++changes; }));
        }});
        sync(f);
        const auto a = f.services.date_picker().mounted()[0];
        const auto b = f.services.date_picker().mounted()[1];
        check(ref.start.focus(), "date range reset start focus");
        key(f, Key::right);
        key(f, Key::enter);
        check(ref.end.focus({InputFocusCursor::All}), "date range reset end focus");
        check(bool(f.inputs.dispatch(TextCommitted{String{u8"2026-11-"}, f.inputs.sessions().active()})),
              "date range reset draft");
        sync(f);
        const auto reset = capture_form_input(ref.start).second;
        check(bool(reset), "range Form reset callback");
        reset();
        sync(f);
        check(changes == 0 && f.services.date_picker().snapshot(a).value == original.start &&
                  f.services.date_picker().snapshot(b).draft == String{u8"2026-11-12"},
              "same-value date range reset retained pending draft");
    }
}

void range_controlled_refusal() {
    std::cerr << "case: range_controlled_refusal" << std::endl;
    {
        Fixture f;
        TimeRangePickerRef ref;
        Signal<PickerEndpoint> endpoint{PickerEndpoint::Start};
        int calendars{};
        int changes{};
        int closes{};
        int advances{};
        const TimePickerRangeValue value{TimeOfDay{9}, TimeOfDay{18}};
        f.services.mount(Content{[&] {
            TimeRangePicker(TimeRangePickerProps{}
                                .value(value)
                                .open(true)
                                .activeEndpoint(endpoint)
                                .ref(ref)
                                .picker(TimePickerProps{}.precision(TimePickerPrecision::Minute))
                                .onCalendarChange([&](auto, auto) { ++calendars; })
                                .onChange([&](auto) { ++changes; })
                                .onOpenChange([&](bool open) { closes += !open; })
                                .onActiveEndpointChange([&](auto) { ++advances; }));
        }});
        sync(f);
        const auto a = f.services.time_picker().mounted()[0];
        const auto b = f.services.time_picker().mounted()[1];
        check(ref.start.focus(), "controlled start focus");
        key(f, Key::enter);
        check(calendars == 1 && advances == 1 && changes == 0 && closes == 0 &&
                  f.services.time_picker().snapshot(a).visible,
              "controlled endpoint refusal duplicated or closed start");
        endpoint.set(PickerEndpoint::End);
        check(f.services.focus().state().focused &&
                  f.services.interactions().require(*f.services.focus().state().focused).component ==
                      f.services.time_picker().snapshot(b).selector,
              "controlled end authority transfers focus");
        key(f, Key::up);
        key(f, Key::enter);
        std::cerr << "controlled time refusal: calendar=" << calendars << " change=" << changes << " close=" << closes
                  << " visible=" << f.services.time_picker().snapshot(b).visible
                  << " pending-hour=" << f.services.time_picker().snapshot(b).pending.hour() << std::endl;
        check(calendars == 2 && changes == 1 && closes == 1 && f.services.time_picker().snapshot(b).visible &&
                  f.services.time_picker().snapshot(b).pending == TimeOfDay{18},
              "controlled range close/value refusal event order");
    }
    {
        Fixture f;
        DateRangePickerRef ref;
        Signal<PickerEndpoint> endpoint{PickerEndpoint::Start};
        int calendars{};
        int changes{};
        int closes{};
        const DatePickerRangeValue value{CivilDate{2026, 11, 5}, CivilDate{2026, 11, 12}};
        f.services.mount(Content{[&] {
            DateRangePicker(DateRangePickerProps{}
                                .value(value)
                                .open(true)
                                .activeEndpoint(endpoint)
                                .ref(ref)
                                .onActiveEndpointChange([&](auto v) { endpoint.set(v); })
                                .onCalendarChange([&](auto, auto) { ++calendars; })
                                .onChange([&](auto) { ++changes; })
                                .onOpenChange([&](bool open) { closes += !open; }));
        }});
        sync(f);
        check(ref.start.focus(), "controlled date start focus");
        key(f, Key::enter);
        check(f.services.focus().state().focused &&
                  f.services.interactions().require(*f.services.focus().state().focused).component ==
                      f.services.date_picker().snapshot(f.services.date_picker().mounted()[1]).input,
              "controlled date endpoint acceptance transfers focus");
        key(f, Key::right);
        key(f, Key::enter);
        const auto b = f.services.date_picker().mounted()[1];
        check(calendars == 2 && changes == 1 && closes == 1 && f.services.date_picker().snapshot(b).visible &&
                  f.services.date_picker().snapshot(b).value == value.end,
              "controlled date range refusal emitted duplicate requests");
    }
}

void multiple_authority() {
    std::cerr << "case: multiple_authority" << std::endl;
    for (bool accept : {false, true}) {
        Fixture f;
        Signal<DatePickerMultipleValue> value{DatePickerMultipleValue{CivilDate{2026, 11, 5}}};
        int changes{};
        f.services.mount(Content{[&] {
            DatePicker(DatePickerProps{}
                           .multiple()
                           .multipleValue(value)
                           .defaultOpen()
                           .defaultPanelDate(CivilDate{2026, 11, 1})
                           .onMultipleChange([&](auto v) {
                               ++changes;
                               if (accept) {
                                   value.set(v);
                               }
                           }));
        }});
        sync(f);
        const auto id = f.services.date_picker().mounted()[0];
        const auto initial = f.services.date_picker().snapshot(id);
        click(f, initial.cells[15]);
        const auto pending = f.services.date_picker().snapshot(id);
        check(changes == 0 && pending.multiple_value.size() == 1 && pending.pending_multiple.size() == 2,
              "multiple pending/accepted separation");
        button(f, "OK");
        const auto closed = f.services.date_picker().snapshot(id);
        check(changes == 1 && closed.multiple_value.size() == (accept ? 2U : 1U) &&
                  closed.pending_multiple == closed.multiple_value,
              "multiple authority/refusal rollback");
    }
}

void date_confirmation_modes() {
    std::cerr << "case: date_confirmation_modes" << std::endl;
    {
        Fixture f;
        DatePickerRef ref;
        int requests{};
        f.services.mount(Content{[&] {
            DatePicker(
                DatePickerProps{}.ref(ref).needConfirm().defaultPanelDate(CivilDate{2026, 11, 5}).onChange([&](auto) {
                    ++requests;
                }));
        }});
        sync(f);
        check(ref.focus({InputFocusCursor::All}), "confirmation draft focus");
        check(bool(f.inputs.dispatch(TextCommitted{String{u8"2026-11-06"}, f.inputs.sessions().active()})),
              "confirmation draft input");
        key(f, Key::enter);
        check(requests == 1, "valid confirmed text could not commit");
    }
    {
        Fixture f;
        int requests{};
        f.services.mount(Content{[&] {
            DatePicker(DatePickerProps{}
                           .multiple()
                           .needConfirm(false)
                           .defaultOpen()
                           .defaultPanelDate(CivilDate{2026, 11, 5})
                           .onMultipleChange([&](auto) { ++requests; }));
        }});
        sync(f);
        const auto id = f.services.date_picker().mounted().front();
        const auto cells = f.services.date_picker().snapshot(id).cells;
        click(f, cells[15]);
        click(f, cells[16]);
        const auto state = f.services.date_picker().snapshot(id);
        check(requests == 2 && state.multiple_value.size() == 2 && state.visible,
              "immediate multiple selection closed or lost values");
    }
}

void datetime_drafts() {
    std::cerr << "case: datetime_drafts" << std::endl;
    Fixture f;
    DatePickerRef ref;
    int changes{};
    DatePickerDateTimeValue requested;
    const DatePickerDateTimeValue original{CivilDateTime{CivilDate{2026, 11, 5}, TimeOfDay{9}}};
    TimePickerRules rules;
    rules.millisecond_step = 125;
    f.services.mount(Content{[&] {
        DatePicker(DatePickerProps{}
                       .ref(ref)
                       .showTime(TimePickerProps{}.rules(rules).precision(TimePickerPrecision::Millisecond))
                       .dateTimeValue(original)
                       .format(u8"DD/MM/YYYY HH:mm:ss.SSS")
                       .onDateTimeChange([&](auto v) {
                           requested = v;
                           ++changes;
                       }));
    }});
    sync(f);
    const auto id = f.services.date_picker().mounted()[0];
    const auto edit = [&](String text) {
        check(ref.focus({InputFocusCursor::All}), "datetime focus");
        check(bool(f.inputs.dispatch(TextCommitted{text, f.inputs.sessions().active()})), "datetime text dispatch");
        sync(f);
        key(f, Key::enter);
    };
    edit(String{u8"06/11/2026 12:30:"});
    check(changes == 0, "partial datetime published");
    edit(String{u8"06/11/2026 12:30:00.126"});
    check(changes == 0, "off-step datetime bypassed time rules");
    edit(String{u8"06/11/2026 12:30:00.125"});
    check(changes == 1 && requested == CivilDateTime{CivilDate{2026, 11, 6}, TimeOfDay{12, 30, 0, 125}},
          "typed datetime request");
    check(f.services.date_picker().snapshot(id).date_time_value == original, "datetime controlled refusal");
    {
        Fixture range;
        DateRangePickerRef refs;
        int requests{};
        range.services.mount(Content{[&] {
            DateRangePicker(DateTimeRangePickerProps{}
                                .ref(refs)
                                .defaultOpen()
                                .defaultValue({CivilDateTime{CivilDate{2026, 11, 5}, TimeOfDay{9}},
                                               CivilDateTime{CivilDate{2026, 11, 5}, TimeOfDay{18}}})
                                .onChange([&](auto) { ++requests; }));
        }});
        sync(range);
        check(range.services.date_picker().mounted().size() == 2 && range.services.time_picker().mounted().size() == 2,
              "date-time range actual composition");
        check(refs.start.focus(), "datetime start focus");
        key(range, Key::enter);
        check(requests == 0, "datetime start atomicity");
        check(range.services.focus().state().focused &&
                  range.services.interactions().require(*range.services.focus().state().focused).component ==
                      range.services.date_picker().snapshot(range.services.date_picker().mounted()[1]).input,
              "date-time range transfers focus");
        key(range, Key::right);
        key(range, Key::enter);
        std::cerr << "datetime range requests=" << requests << std::endl;
        check(requests == 1, "datetime range commit");
    }
}

void immediate_date_time() {
    std::cerr << "case: immediate_date_time" << std::endl;
    Fixture f;
    TimePickerRef time;
    int changes{};
    DatePickerDateTimeValue value;
    f.services.mount(Content{[&] {
        DatePicker(DatePickerProps{}
                       .showTime(TimePickerProps{}.ref(time).precision(TimePickerPrecision::Minute))
                       .defaultDateTimeValue(CivilDateTime{CivilDate{2026, 11, 5}, TimeOfDay{9}})
                       .defaultOpen()
                       .needConfirm(false)
                       .onDateTimeChange([&](auto v) {
                           value = v;
                           ++changes;
                       }));
    }});
    sync(f);
    check(time.focus(), "immediate date-time time focus");
    key(f, Key::down);
    key(f, Key::down);
    check(changes == 1 && value == CivilDateTime{CivilDate{2026, 11, 5}, TimeOfDay{10}} &&
              f.services.date_picker().snapshot(f.services.date_picker().mounted().front()).visible,
          "immediate time selection was not published atomically");
}

void failure_cleanup() {
    std::cerr << "case: failure_cleanup" << std::endl;
    for (bool date : {false, true}) {
        Fixture f;
        InputRef ref;
        f.services.mount(Content{[&] {
            if (date) {
                DatePicker(DatePickerProps{}
                               .ref(ref)
                               .defaultPanelDate(CivilDate{2026, 11, 5})
                               .defaultOpen()
                               .onChange([](auto) { throw std::runtime_error("application callback"); }));
            } else {
                TimePicker(TimePickerProps{}.ref(ref).defaultOpen().onChange(
                    [](auto) { throw std::runtime_error("application callback"); }));
            }
        }});
        sync(f);
        check(ref.focus(), "failure ref");
        bool thrown{};
        try {
            key(f, Key::enter);
        } catch (const std::runtime_error&) {
            thrown = true;
        }
        check(thrown && f.services.components().component_count() == 0 && f.services.interactions().size() == 0 &&
                  f.services.surfaces().size() == 0 && !ref.bound(),
              "throw left component resources");
    }
    {
        Fixture f;
        TimePickerRef ref;
        f.services.mount(Content{[&] {
            TimePicker(TimePickerProps{}.ref(ref).defaultOpen().onChange([&](auto) { f.services.dispose(); }));
        }});
        sync(f);
        check(ref.focus(), "self-destroy focus");
        key(f, Key::enter);
        check(!ref.bound() && f.services.components().component_count() == 0, "callback self-destruction");
    }
    {
        Fixture f;
        bool threw{};
        try {
            f.services.mount(Content{[] {
                DatePicker(
                    DatePickerProps{}.cellRender([](auto) -> DatePickerCell { throw std::runtime_error("renderer"); }));
            }});
        } catch (const std::runtime_error&) {
            threw = true;
        }
        check(threw && f.services.components().component_count() == 0 && f.services.interactions().size() == 0,
              "renderer mount rollback");
    }
}

void clock_presets() {
    std::cerr << "case: clock_presets" << std::endl;
    {
        Fixture f;
        TimePickerRef ref;
        int calls{};
        int changes{};
        TimePickerValue value;
        f.services.mount(Content{[&] {
            TimePicker(
                TimePickerProps{}
                    .ref(ref)
                    .defaultOpen()
                    .precision(TimePickerPrecision::Millisecond)
                    .clock([&] {
                        ++calls;
                        return TimeOfDay{9, 30, 0, 125};
                    })
                    .showNow()
                    .presets({{String{u8"morning"}, String{u8"Morning"}, [] { return TimePickerValue{TimeOfDay{8}}; }}})
                    .onChange([&](auto v) {
                        value = v;
                        ++changes;
                    }));
        }});
        sync(f);
        check(calls == 0, "clock read eagerly");
        button(f, "Now");
        check(calls == 1 && changes == 1 && value == TimeOfDay{9, 30, 0, 125}, "Now explicit precise clock");
        check(ref.focus(), "preset focus");
        key(f, Key::down);
        button(f, "Morning");
        check(changes == 2 && value == TimeOfDay{8}, "preset action");
    }
    {
        Fixture f;
        int calls{};
        DatePickerValue value;
        f.services.mount(Content{[&] {
            DatePicker(DatePickerProps{}
                           .defaultPanelDate(CivilDate{2026, 1, 1})
                           .defaultOpen()
                           .clock([&] {
                               ++calls;
                               return CivilDateTime{CivilDate{2026, 11, 5}, TimeOfDay{12}};
                           })
                           .showNow()
                           .onChange([&](auto v) { value = v; }));
        }});
        sync(f);
        button(f, "Today");
        check(calls == 1 && value == CivilDate{2026, 11, 5}, "business Today clock");
    }
    {
        Fixture f;
        f.services.mount(Content{[] {
            TimePicker(TimePickerProps{}.defaultOpen().showNow().clock(
                []() -> TimeOfDay { throw std::runtime_error("clock"); }));
        }});
        sync(f);
        bool threw{};
        try {
            button(f, "Now");
        } catch (const std::runtime_error&) {
            threw = true;
        }
        check(threw && f.services.components().component_count() == 0 && f.services.interactions().size() == 0,
              "clock callback failure leaked owner");
    }
}

void typed_date_presets_and_panel_authority() {
    std::cerr << "case: typed_date_presets_and_panel_authority" << std::endl;
    {
        Fixture f;
        DatePickerMultipleValue selected;
        f.services.mount(Content{[&] {
            DatePicker(
                DatePickerProps{}
                    .multiple()
                    .defaultOpen()
                    .defaultPanelDate(CivilDate{2026, 11, 1})
                    .disabledDate([](CivilDate d) { return d.day() == 1; })
                    .multiplePresets(
                        {{String{u8"work"}, String{u8"Work days"},
                          [] { return DatePickerMultipleValue{CivilDate{2026, 11, 5}, CivilDate{2026, 11, 6}}; }}})
                    .onMultipleChange([&](auto v) { selected = v; }));
        }});
        sync(f);
        button(f, "Work days");
        check(selected.size() == 2, "typed multiple preset depended on disabled cursor date");
    }
    {
        Fixture f;
        DatePickerDateTimeValue selected;
        f.services.mount(Content{[&] {
            DatePicker(DatePickerProps{}
                           .showTime(TimePickerProps{}.precision(TimePickerPrecision::Millisecond))
                           .defaultOpen()
                           .defaultPanelDate(CivilDate{2026, 11, 1})
                           .dateTimePresets({{String{u8"noon"}, String{u8"Precise noon"},
                                              [] {
                                                  return DatePickerDateTimeValue{
                                                      CivilDateTime{CivilDate{2026, 11, 5}, TimeOfDay{12, 0, 0, 125}}};
                                              }}})
                           .onDateTimeChange([&](auto v) { selected = v; }));
        }});
        sync(f);
        button(f, "Precise noon");
        check(selected == CivilDateTime{CivilDate{2026, 11, 5}, TimeOfDay{12, 0, 0, 125}},
              "typed date-time preset lost precision");
    }
    {
        Fixture f;
        DatePickerRef ref;
        Signal<CivilDate> panel{CivilDate{2026, 11, 1}};
        Signal<DatePickerValue> value{CivilDate{2026, 11, 5}};
        DatePickerValue requested;
        f.services.mount(Content{[&] {
            DatePicker(DatePickerProps{}.ref(ref).value(value).panelDate(panel).defaultOpen().onPanelChange(
                [&](CivilDate v, auto) { requested = v; }));
        }});
        sync(f);
        const auto id = f.services.date_picker().mounted().front();
        value.set(CivilDate{2027, 1, 5});
        sync(f);
        const auto old = f.services.date_picker().snapshot(id).active;
        check(old.year() == 2026 && old.month() == 11, "value update stole controlled panel authority");
        panel.set(CivilDate{2027, 1, 1});
        check(ref.focus(), "controlled panel focus");
        key(f, Key::page_down);
        const auto active = f.services.date_picker().snapshot(id).active;
        check(requested == CivilDate{2027, 2, 1} && active.year() == 2027 && active.month() == 1,
              "controlled panel refused navigation was applied");
    }
}

void predicate_lifetime_and_prefix_cost() {
    std::cerr << "case: predicate_lifetime_and_prefix_cost" << std::endl;
    {
        int calls{};
        TimePickerRules rules;
        rules.min = TimeOfDay{8};
        rules.max = TimeOfDay{8};
        rules.disabled_hours = [&](int) {
            ++calls;
            return false;
        };
        TimePickerModel model{rules, TimePickerPrecision::Millisecond, {}, false};
        for (int hour = 0; hour < 24; ++hour) {
            check(bool(model.candidate(TimeColumn::Hour, hour)) == (hour == 8), "millisecond range prefix eligibility");
        }
        check(calls <= 64, "bounded hour prefixes enumerated out-of-range milliseconds");
    }
    {
        Fixture f;
        TimePickerRef ref;
        Signal<TimePickerRules> rules{TimePickerRules{}};
        int after{};
        f.services.mount(Content{[&] { TimePicker(TimePickerProps{}.ref(ref).rules(rules).defaultOpen()); }});
        sync(f);
        const auto id = f.services.time_picker().mounted().front();
        check(ref.focus(), "predicate lifetime focus");
        TimePickerRules changed;
        changed.disabled_hours = [&](int) {
            static_cast<void>(f.services.destroy(id));
            return false;
        };
        changed.disabled_minutes = [&](int, int) {
            ++after;
            return false;
        };
        rules.set(changed);
        sync(f);
        check(after == 0 && !ref.bound() && f.services.components().component_count() == 0 &&
                  f.services.interactions().size() == 0,
              "time predicate called a dependent rule after owner retirement");
    }
    {
        Fixture f;
        DatePickerRef ref;
        Signal<DisabledDate> rule{DisabledDate{}};
        int after{};
        f.services.mount(Content{[&] {
            DatePicker(DatePickerProps{}
                           .ref(ref)
                           .defaultPanelDate(CivilDate{2026, 11, 1})
                           .defaultOpen()
                           .disabledDate(rule)
                           .disabledDateInfo([&](CivilDate, const auto&) {
                               ++after;
                               return false;
                           }));
        }});
        sync(f);
        const auto id = f.services.date_picker().mounted().front();
        after = 0;
        rule.set(DisabledDate{[&](CivilDate) {
            static_cast<void>(f.services.destroy(id));
            return false;
        }});
        sync(f);
        check(after == 0 && !ref.bound() && f.services.components().component_count() == 0 &&
                  f.services.surfaces().size() == 0,
              "date predicate called info after owner retirement");
    }
    {
        Fixture f;
        Signal<TimePickerRules> rules{TimePickerRules{}};
        f.services.mount(Content{[&] { TimePicker(TimePickerProps{}.rules(rules).defaultOpen()); }});
        sync(f);
        TimePickerRules changed;
        changed.disabled = [](TimeOfDay) -> bool {
            throw std::runtime_error("predicate");
        };
        bool threw{};
        try {
            rules.set(changed);
        } catch (const std::runtime_error&) {
            threw = true;
        }
        check(threw && f.services.components().component_count() == 0 && f.services.interactions().size() == 0,
              "predicate exception left resources");
    }
}

void rich_slot_identity() {
    std::cerr << "case: rich_slot_identity" << std::endl;
    {
        Fixture f;
        InputRef cell;
        Signal<DatePickerValue> value{CivilDate{2026, 11, 5}};
        Signal<DatePickerMode> mode{DatePickerMode::Date};
        Signal<ThemeConfig> theme{ThemeConfig{}};
        int renders{};
        f.services.mount(Content{[&] {
            Theme(ThemeProps{}.config(theme), ThemeContent{[&] {
                      DatePicker(DatePickerProps{}.value(value).picker(mode).defaultOpen().cellRender(
                          [&](Prop<DatePickerCellInfo> info) {
                              const bool first = renders++ == 0;
                              return DatePickerCell{[first, info, cell] {
                                  if (first) {
                                      Input(InputProps{}.ref(cell).defaultValue(String{u8"cell"}));
                                  } else {
                                      Text(TextProps{}.content(bind([info] {
                                          const auto state = read_prop(info);
                                          return state.date ? format_iso_date(*state.date) : String{};
                                      })));
                                  }
                              }};
                          }));
                  }});
        }});
        sync(f);
        check(cell.focus({InputFocusCursor::All}), "rich date cell focus");
        const auto editor = f.inputs.sessions().active().owner;
        check(bool(f.inputs.dispatch(TextCommitted{String{u8"retained date draft"}, f.inputs.sessions().active()})),
              "rich date cell edit");
        const auto id = f.services.date_picker().mounted().front();
        const auto original_cells = f.services.date_picker().snapshot(id).cells;
        value.set(CivilDate{2026, 11, 6});
        ThemeConfig config;
        config.date_picker.tokens.selected = Color::rgba8(30, 100, 150);
        theme.set(config);
        sync(f);
        check(renders == 42 && cell.bound() && f.services.date_picker().snapshot(id).cells == original_cells &&
                  f.inputs.editors().require(editor).value() == "retained date draft",
              "rich date cell was rebuilt by value/Theme");
        mode.set(DatePickerMode::Month);
        sync(f);
        const auto months = f.services.date_picker().snapshot(id);
        check(renders == 54 && !cell.bound() && !f.inputs.editors().find(editor) && months.cells.size() == 12 &&
                  months.mode == DatePickerMode::Month,
              "mode change retained a Date-keyed editor or failed to mount Month keys");
        for (const auto old : original_cells) {
            check(!f.services.components().contains(old), "mode change retained a previous Date cell identity");
        }
    }
    {
        Fixture f;
        InputRef panel;
        Signal<TimePickerValue> value{TimeOfDay{9}};
        Signal<ThemeConfig> theme{ThemeConfig{}};
        int panels{};
        int footers{};
        f.services.mount(Content{[&] {
            Theme(ThemeProps{}.config(theme), ThemeContent{[&] {
                      TimePicker(TimePickerProps{}.value(value).defaultOpen().panel(TimePickerPanel{[&] {
                          ++panels;
                          Input(InputProps{}.ref(panel).defaultValue(String{u8"panel"}));
                      }}),
                                 TimePickerFooter{[&] {
                                     ++footers;
                                     Text(u8"Footer");
                                 }});
                  }});
        }});
        sync(f);
        check(panel.focus({InputFocusCursor::All}), "rich time panel focus");
        const auto editor = f.inputs.sessions().active().owner;
        check(bool(f.inputs.dispatch(TextCommitted{String{u8"retained time draft"}, f.inputs.sessions().active()})),
              "rich time panel edit");
        value.set(TimeOfDay{10});
        ThemeConfig config;
        config.time_picker.tokens.cell_height = dp(30);
        theme.set(config);
        sync(f);
        check(panels == 1 && footers == 1 && panel.bound() &&
                  f.inputs.editors().require(editor).value() == "retained time draft",
              "rich time panel remounted by value/Theme");
    }
}

void wheel_motion() {
    std::cerr << "case: wheel_motion" << std::endl;
    Fixture f;
    TimePickerRef ref;
    Signal<bool> scroll{false};
    f.services.mount(Content{[&] {
        TimePicker(TimePickerProps{}
                       .ref(ref)
                       .defaultOpen()
                       .changeOnScroll(scroll)
                       .defaultPanelValue(TimeOfDay{10})
                       .precision(TimePickerPrecision::Minute));
    }});
    sync(f);
    const auto id = f.services.time_picker().mounted()[0];
    const auto first = f.services.time_picker().snapshot(id);
    const auto clip = first.column_clips[0];
    check(!f.services.dispatch_scroll({0, -1, 690, 690}), "wheel consumed outside popup");
    check(f.services.dispatch_scroll({0, -1, clip.x + 4, clip.y + 4}), "column wheel missing");
    sync(f);
    check(f.services.time_picker().snapshot(id).pending == first.pending, "ordinary wheel selected value");
    scroll.set(true);
    sync(f);
    check(f.services.dispatch_scroll({0, -1, clip.x + 4, clip.y + 4}), "selection wheel missing");
    sync(f);
    check(f.services.time_picker().snapshot(id).pending.hour() == 11 && f.services.components().mount_runs() == 1,
          "selection wheel remounted rows or failed to select");
    f.services.set_motion_preference(animation::MotionPreference::normal);
    check(ref.focus(), "motion focus");
    key(f, Key::escape);
    key(f, Key::down);
    static_cast<void>(f.services.tick_animations(animation::AnimationTime::microseconds(2000000)));
    sync(f);
    check(ref.blur(), "motion blur");
    static_cast<void>(f.services.tick_animations(animation::AnimationTime::microseconds(4000000)));
    sync(f);
    static_cast<void>(f.frames.consume_request());
    sync(f);
    check(!f.frames.pending() && !f.services.next_frame_deadline(), "finite animation did not settle");
    f.services.set_window_active(false);
    sync(f);
    check(!f.services.time_picker().snapshot(id).visible, "inactive window popup");
}
} // namespace

int main(int argc, char** argv) {
    try {
        const std::string_view test = argc > 1 ? argv[1] : "all";
        if (test == "format" || test == "all") {
            format_contracts();
        }
        if (test == "time_rules" || test == "all") {
            time_rules();
            meridiem_steps();
        }
        if (test == "ranges" || test == "all") {
            ranges();
        }
        if (test == "date_modes" || test == "all") {
            date_modes();
        }
        if (test == "native_time" || test == "all") {
            native_time();
        }
        if (test == "native_date" || test == "all") {
            native_date();
            rich_slot_identity();
        }
        if (test == "native_ranges" || test == "all") {
            native_ranges();
            range_same_value_reset();
            range_controlled_refusal();
        }
        if (test == "multiple_authority" || test == "all") {
            multiple_authority();
        }
        if (test == "datetime_drafts" || test == "all") {
            datetime_drafts();
            date_confirmation_modes();
            immediate_date_time();
        }
        if (test == "failure_cleanup" || test == "all") {
            failure_cleanup();
            clock_presets();
            typed_date_presets_and_panel_authority();
            predicate_lifetime_and_prefix_cost();
        }
        if (test == "wheel_motion" || test == "all") {
            wheel_motion();
        }
        std::cout << "picker completion " << test << " passed\n";
    } catch (const std::exception& e) {
        std::cerr << e.what() << '\n';
        return 1;
    }
}
