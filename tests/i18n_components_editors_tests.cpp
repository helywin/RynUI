#include "component/date_picker_component.hpp"
#include "component/time_picker_component.hpp"
#include "support/i18n_test.hpp"
#include "support/input_fixture.hpp"

void run_i18n_picker_range_cases();

namespace {
using namespace ryn;
using namespace ryn::input;
using namespace ryn_test::i18n;
using Fixture = ryn_test::input_component::Fixture;

void sync(Fixture& fixture) {
    require(fixture.services.layout_and_synchronize({900, 900}, {0, 0, 900, 900}), "editor i18n layout");
}

void key(Fixture& fixture, Key value, KeyModifier modifiers = KeyModifier::none) {
    fixture.services.focus().dispatch({value, KeyAction::down, modifiers});
    fixture.services.focus().dispatch({value, KeyAction::up, modifiers});
    sync(fixture);
}

void type(Fixture& fixture, const InputRef& reference, String text) {
    require(reference.focus(), "picker editor focus");
    key(fixture, Key::a, KeyModifier::control);
    const auto stamp = fixture.inputs.sessions().active();
    require(bool(fixture.inputs.dispatch(TextCommitted{std::move(text), stamp})), "real picker text event");
    sync(fixture);
}

const detail::MountedInputComponent& input(Fixture& fixture, runtime::ComponentId id) {
    for (const auto& candidate : fixture.inputs.mounted_inputs()) {
        if (candidate.component == id) {
            return candidate;
        }
    }
    throw std::runtime_error("missing real picker Input");
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

void date_draft_keeps_month_parser() {
    std::cerr << "case: date_draft_keeps_month_parser" << std::endl;
    Fixture fixture;
    Signal<LocaleId> language{locales::en_US};
    DatePickerRef reference;
    DatePickerValue requested;
    int changes = 0;
    fixture.services.mount(Content{[&] {
        ConfigProvider(ConfigProviderProps{}.locale(language), ConfigProviderContent{[&] {
                           DatePicker(DatePickerProps{}
                                          .ref(reference)
                                          .defaultOpen()
                                          .defaultValue(CivilDate{2026, 11, 1})
                                          .format(u8"DD MMMM YYYY")
                                          .onChange([&](auto value) {
                                              requested = value;
                                              ++changes;
                                          }));
                       }});
    }});
    sync(fixture);
    const auto id = fixture.services.date_picker().mounted().front();
    const auto original = fixture.services.date_picker().snapshot(id);
    type(fixture, reference, u8"07 November 2026");
    auto& editor = fixture.inputs.editors().require(input(fixture, original.input).editor);
    require(reference.select(16, 3), "reverse month selection");
    const auto selection = editor.selection();
    const auto history = editor.history();
    language.set(locales::zh_CN);
    sync(fixture);
    require(editor.value() == "07 November 2026" && editor.selection() == selection &&
                editor.history().undo_count == history.undo_count && fixture.inputs.focused(original.input) &&
                changes == 0 && fixture.services.date_picker().snapshot(id).input == original.input &&
                has_text(fixture, original.popup, u8"2026年11月"),
            "date translation changed dirty draft, reverse selection, history, focus, callback or panel labels");
    key(fixture, Key::enter);
    require(requested == CivilDate{2026, 11, 7} && changes == 1 &&
                fixture.services.date_picker().snapshot(id).draft == String{u8"07 十一月 2026"},
            "date commit parsed with new Locale or failed to adopt latest clean format");
}

void time_draft_keeps_meridiem_parser() {
    std::cerr << "case: time_draft_keeps_meridiem_parser" << std::endl;
    Fixture fixture;
    Signal<LocaleId> language{locales::en_US};
    TimePickerRef reference;
    TimePickerValue requested;
    int changes = 0;
    fixture.services.mount(Content{[&] {
        ConfigProvider(ConfigProviderProps{}.locale(language), ConfigProviderContent{[&] {
                           TimePicker(TimePickerProps{}
                                          .ref(reference)
                                          .defaultOpen()
                                          .use12Hours()
                                          .format(u8"h:mm A")
                                          .defaultValue(TimeOfDay{12})
                                          .onChange([&](auto value) {
                                              requested = value;
                                              ++changes;
                                          }));
                       }});
    }});
    sync(fixture);
    const auto id = fixture.services.time_picker().mounted().front();
    const auto before = fixture.services.time_picker().snapshot(id);
    type(fixture, reference, u8"1:45 PM");
    language.set(locales::zh_CN);
    sync(fixture);
    const auto after = fixture.services.time_picker().snapshot(id);
    require(after.draft == String{u8"1:45 PM"} && after.confirmable && after.rows == before.rows && changes == 0 &&
                has_text(fixture, after.popup, u8"取消") && has_text(fixture, after.popup, u8"确定") &&
                has_text(fixture, after.popup, u8"下午"),
            "time parser freeze or immediate popup translation failed");
    key(fixture, Key::enter);
    require(requested == TimeOfDay{13, 45} && changes == 1 &&
                fixture.services.time_picker().snapshot(id).draft == String{u8"1:45 下午"},
            "old meridiem draft was rejected or clean time did not use current format language");
}

void selected_clean_and_composition_are_preserved() {
    std::cerr << "case: selected_clean_and_composition_are_preserved" << std::endl;
    Fixture fixture;
    Signal<LocaleId> language{locales::en_US};
    DatePickerRef reference;
    InputRef next;
    int changes = 0;
    fixture.services.mount(Content{[&] {
        ConfigProvider(ConfigProviderProps{}.locale(language), ConfigProviderContent{[&] {
                           DatePicker(DatePickerProps{}
                                          .ref(reference)
                                          .defaultValue(CivilDate{2026, 11, 7})
                                          .format(u8"DD MMMM YYYY")
                                          .onChange([&](auto) { ++changes; }));
                           Input(InputProps{}.ref(next));
                       }});
    }});
    sync(fixture);
    const auto id = fixture.services.date_picker().mounted().front();
    const auto selector = fixture.services.date_picker().snapshot(id).input;
    const auto editor_id = input(fixture, selector).editor;
    auto& editor = fixture.inputs.editors().require(editor_id);
    require(reference.focus() && reference.select(16, 3), "clean selection focus");
    const auto selection = editor.selection();
    const auto history = editor.history();
    language.set(locales::zh_CN);
    sync(fixture);
    require(editor.value() == "07 November 2026" && editor.selection() == selection && changes == 0,
            "clean active selection was rewritten on language update");
    require(bool(editor.update_composition({String{u8"拼"}, {0, 1}, {}})), "preedit setup");
    const auto draft = std::string(editor.value());
    language.set(locales::en_US);
    language.set(locales::zh_CN);
    sync(fixture);
    require(editor.value() == draft && editor.selection() == selection && editor.composition().active &&
                editor.composition().text == String{u8"拼"}.bytes() &&
                editor.history().undo_count == history.undo_count && fixture.inputs.focused(selector) && changes == 0,
            "preedit, history or focus was lost during translation");
    editor.cancel_composition();
    require(next.focus(), "blur target");
    sync(fixture);
    require(fixture.services.date_picker().snapshot(id).draft == String{u8"07 十一月 2026"} && changes == 0,
            "clean blur failed to adopt latest format or emitted business change");
}

void independent_format_locale_and_literals() {
    std::cerr << "case: independent_format_locale_and_literals" << std::endl;
    Fixture fixture;
    Signal<LocaleId> language{locales::zh_CN};
    Signal<LocaleId> formatting{locales::en_US};
    Signal<TextDirection> direction{TextDirection::RightToLeft};
    int changes = 0;
    fixture.services.mount(Content{[&] {
        ConfigProvider(ConfigProviderProps{}.locale(language).formatLocale(formatting).direction(direction),
                       ConfigProviderContent{[&] {
                           DatePicker(DatePickerProps{}
                                          .defaultOpen()
                                          .defaultValue(CivilDate{2026, 11, 7})
                                          .format(u8"DD MMMM YYYY")
                                          .onChange([&](auto) { ++changes; }));
                           TimePicker(
                               TimePickerProps{}.defaultValue(TimeOfDay{13, 15}).use12Hours().format(u8"h:mm A"));
                           DatePicker(DatePickerProps{}
                                          .defaultValue(CivilDate{2026, 11, 7})
                                          .locale(PickerLocale{})
                                          .format(u8"DD MMMM YYYY"));
                       }});
    }});
    sync(fixture);
    const auto date_id = fixture.services.date_picker().mounted()[0];
    const auto explicit_id = fixture.services.date_picker().mounted()[1];
    const auto time_id = fixture.services.time_picker().mounted()[0];
    require(fixture.services.date_picker().snapshot(date_id).draft == String{u8"07 November 2026"} &&
                fixture.services.time_picker().snapshot(time_id).draft == String{u8"1:15 PM"},
            "format locale was replaced by UI language");
    direction.set(TextDirection::LeftToRight);
    formatting.set(locales::zh_CN);
    sync(fixture);
    require(fixture.services.date_picker().snapshot(date_id).draft == String{u8"07 十一月 2026"} &&
                fixture.services.time_picker().snapshot(time_id).draft == String{u8"1:15 下午"} &&
                fixture.services.date_picker().snapshot(explicit_id).draft == String{u8"07 November 2026"} &&
                changes == 0,
            "independent formatLocale, direction or explicit complete Locale priority failed");
    language.set(locales::en_US);
    sync(fixture);
    require(fixture.services.date_picker().snapshot(date_id).draft == String{u8"07 十一月 2026"},
            "message language overwrote explicit formatLocale");
}

void controlled_refusal_and_frozen_alternatives() {
    std::cerr << "case: controlled_refusal_and_frozen_alternatives" << std::endl;
    Fixture fixture;
    Signal<LocaleId> language{locales::en_US};
    Signal<DatePickerValue> authority{DatePickerValue{CivilDate{2026, 11, 1}}};
    Signal<std::vector<String>> alternatives{std::vector<String>{u8"D MMM YYYY"}};
    DatePickerRef reference;
    DatePickerValue requested;
    int changes = 0;
    fixture.services.mount(Content{[&] {
        ConfigProvider(ConfigProviderProps{}.locale(language), ConfigProviderContent{[&] {
                           DatePicker(DatePickerProps{}
                                          .ref(reference)
                                          .value(authority)
                                          .defaultOpen()
                                          .format(u8"DD MMMM YYYY")
                                          .inputFormats(alternatives)
                                          .onChange([&](auto value) {
                                              requested = value;
                                              ++changes;
                                          }));
                       }});
    }});
    sync(fixture);
    const auto id = fixture.services.date_picker().mounted().front();
    type(fixture, reference, u8"7 Nov 2026");
    alternatives.set({u8"YYYY/MM/DD"});
    language.set(locales::zh_CN);
    sync(fixture);
    require(changes == 0 && fixture.services.date_picker().snapshot(id).value == authority.get(),
            "translation changed controlled authority");
    key(fixture, Key::enter);
    const auto after = fixture.services.date_picker().snapshot(id);
    require(requested == CivilDate{2026, 11, 7} && changes == 1 && after.value == authority.get() &&
                after.draft == String{u8"01 十一月 2026"},
            "frozen alternative parse or controlled refusal projection failed");
}

void week_text_keeps_original_value_rules_and_panel_uses_live_rules() {
    std::cerr << "case: week_text_keeps_original_value_rules_and_panel_uses_live_rules" << std::endl;
    Fixture fixture;
    Signal<LocaleId> language{locales::en_US};
    Signal<bool> open{true};
    DatePickerRef reference;
    DatePickerValue requested;
    int changes = 0;
    fixture.services.mount(Content{[&] {
        ConfigProvider(ConfigProviderProps{}.locale(language), ConfigProviderContent{[&] {
                           DatePicker(DatePickerProps{}
                                          .ref(reference)
                                          .picker(DatePickerMode::Week)
                                          .open(open)
                                          .defaultValue(CivilDate{2026, 10, 25})
                                          .onChange([&](auto value) {
                                              requested = value;
                                              ++changes;
                                          }));
                       }});
    }});
    sync(fixture);
    const auto id = fixture.services.date_picker().mounted().front();
    const auto english = LocaleCatalog::builtin().pack(locales::en_US).data().picker;
    const auto chinese = LocaleCatalog::builtin().pack(locales::zh_CN).data().picker;
    const auto original_week = format_date(CivilDate{2026, 11, 1}, "gggg-[W]ww", english);
    type(fixture, reference, original_week);
    language.set(locales::zh_CN);
    sync(fixture);
    require(changes == 0, "week language update emitted a request");
    key(fixture, Key::enter);
    auto snapshot = fixture.services.date_picker().snapshot(id);
    require(changes == 1 && requested == CivilDate{2026, 11, 1} && snapshot.value == requested &&
                snapshot.draft == format_date(*requested, "gggg-[W]ww", chinese),
            "frozen Sunday week text was normalized again with live Monday rules");
    require(snapshot.cells.size() == 42, "Week collapsed raw daily cells into normalized week keys");
    // November's Monday-first grid starts on October 26: raw day 13 is November 8.
    const auto bounds = fixture.nodes.require(fixture.services.components().root(snapshot.cells[13])).bounds;
    for (const auto action : {PointerAction::down, PointerAction::up}) {
        fixture.services.pointer().dispatch({PointerIdentity::mouse(), action, PointerButton::primary,
                                             bounds.x + bounds.width / 2, bounds.y + bounds.height / 2});
    }
    sync(fixture);
    require(changes == 2 && requested == CivilDate{2026, 11, 2}, "panel click did not use live Monday week rules");
}

void time_precision_and_hour_mode_are_frozen_for_text_commit() {
    std::cerr << "case: time_precision_and_hour_mode_are_frozen_for_text_commit" << std::endl;
    Fixture fixture;
    Signal<LocaleId> language{locales::en_US};
    Signal<TimePickerPrecision> precision{TimePickerPrecision::Millisecond};
    Signal<bool> twelve{true};
    TimePickerRef reference;
    TimePickerValue requested;
    fixture.services.mount(Content{[&] {
        ConfigProvider(ConfigProviderProps{}.locale(language), ConfigProviderContent{[&] {
                           TimePicker(TimePickerProps{}
                                          .ref(reference)
                                          .defaultOpen()
                                          .precision(precision)
                                          .use12Hours(twelve)
                                          .defaultValue(TimeOfDay{12})
                                          .onChange([&](auto value) { requested = value; }));
                       }});
    }});
    sync(fixture);
    const auto id = fixture.services.time_picker().mounted().front();
    type(fixture, reference, u8"1:45:16.125 pm");
    language.set(locales::zh_CN);
    twelve.set(false);
    precision.set(TimePickerPrecision::Minute);
    sync(fixture);
    require(fixture.services.time_picker().snapshot(id).draft == String{u8"1:45:16.125 pm"},
            "precision change rewrote dirty text");
    key(fixture, Key::enter);
    require(requested == TimeOfDay{13, 45, 16, 125} && fixture.services.time_picker().snapshot(id).value == requested &&
                fixture.services.time_picker().snapshot(id).draft == String{u8"13:45"},
            "text commit used live precision/hour mode or lost exact millisecond value");
}

void rejected_date_commit_keeps_frozen_draft_until_blur() {
    std::cerr << "case: rejected_date_commit_keeps_frozen_draft_until_blur" << std::endl;
    for (const auto draft : {String{u8"not a date"}, String{u8"07 November 2026"}}) {
        Fixture fixture;
        Signal<LocaleId> language{locales::en_US};
        DatePickerRef reference;
        InputRef outside;
        int changes = 0;
        fixture.services.mount(Content{[&] {
            ConfigProvider(ConfigProviderProps{}.locale(language), ConfigProviderContent{[&] {
                               DatePicker(DatePickerProps{}
                                              .ref(reference)
                                              .defaultOpen()
                                              .defaultValue(CivilDate{2026, 11, 1})
                                              .format(u8"DD MMMM YYYY")
                                              .disabledDate([](CivilDate date) { return date.day() == 7; })
                                              .onChange([&](auto) { ++changes; }));
                               Input(InputProps{}.ref(outside));
                           }});
        }});
        sync(fixture);
        const auto id = fixture.services.date_picker().mounted().front();
        type(fixture, reference, draft);
        language.set(locales::zh_CN);
        key(fixture, Key::enter);
        require(fixture.services.date_picker().snapshot(id).draft == draft && changes == 0 &&
                    fixture.services.date_picker().snapshot(id).value == CivilDate{2026, 11, 1},
                "parse/disabled rejection consumed a dirty edit or changed authority");
        language.set(locales::en_US);
        language.set(locales::zh_CN);
        require(fixture.services.date_picker().snapshot(id).draft == draft,
                "rejected edit lost its frozen format on another update");
        require(outside.focus(), "rejected edit blur");
        sync(fixture);
        require(fixture.services.date_picker().snapshot(id).draft == String{u8"01 十一月 2026"} && changes == 0,
                "blur failed to restore accepted value using latest format");
    }
}

void time_controlled_refusal_uses_latest_accepted_display() {
    std::cerr << "case: time_controlled_refusal_uses_latest_accepted_display" << std::endl;
    Fixture fixture;
    Signal<LocaleId> language{locales::en_US};
    Signal<TimePickerValue> authority{TimePickerValue{TimeOfDay{12}}};
    TimePickerRef reference;
    TimePickerValue requested;
    int changes = 0;
    fixture.services.mount(Content{[&] {
        ConfigProvider(ConfigProviderProps{}.locale(language), ConfigProviderContent{[&] {
                           TimePicker(TimePickerProps{}
                                          .ref(reference)
                                          .value(authority)
                                          .defaultOpen()
                                          .use12Hours()
                                          .format(u8"h:mm A")
                                          .onChange([&](auto value) {
                                              requested = value;
                                              ++changes;
                                          }));
                       }});
    }});
    sync(fixture);
    const auto id = fixture.services.time_picker().mounted().front();
    type(fixture, reference, u8"1:45 PM");
    language.set(locales::zh_CN);
    require(changes == 0 && fixture.services.time_picker().snapshot(id).value == authority.get(),
            "translation changed controlled time");
    key(fixture, Key::enter);
    const auto snapshot = fixture.services.time_picker().snapshot(id);
    require(changes == 1 && requested == TimeOfDay{13, 45} && snapshot.value == authority.get() &&
                snapshot.draft == String{u8"12:00 下午"},
            "controlled time rejection was treated as acceptance or kept an obsolete display format");
}

void range_endpoints_freeze_independently() {
    std::cerr << "case: range_endpoints_freeze_independently" << std::endl;
    for (const auto endpoint : {PickerEndpoint::Start, PickerEndpoint::End}) {
        Fixture fixture;
        Signal<LocaleId> language{locales::en_US};
        Signal<DatePickerRangeValue> authority{DatePickerRangeValue{CivilDate{2026, 11, 1}, CivilDate{2026, 11, 12}}};
        Signal<bool> open{true};
        DateRangePickerRef reference;
        DatePickerValue requested;
        int calendars = 0;
        int changes = 0;
        fixture.services.mount(Content{[&] {
            ConfigProvider(ConfigProviderProps{}.locale(language), ConfigProviderContent{[&] {
                               DateRangePicker(DateRangePickerProps{}
                                                   .ref(reference)
                                                   .value(authority)
                                                   .open(open)
                                                   .activeEndpoint(endpoint)
                                                   .picker(DatePickerProps{}.format(u8"DD MMMM YYYY"))
                                                   .onChange([&](auto) { ++changes; })
                                                   .onCalendarChange([&](auto range, auto active) {
                                                       ++calendars;
                                                       requested =
                                                           active == PickerEndpoint::Start ? range.start : range.end;
                                                   }));
                           }});
        }});
        sync(fixture);
        const auto index = static_cast<std::size_t>(endpoint);
        const auto id = fixture.services.date_picker().mounted()[index];
        const auto other = fixture.services.date_picker().mounted()[1 - index];
        type(fixture, index == 0 ? reference.start : reference.end, u8"07 November 2026");
        language.set(locales::zh_CN);
        sync(fixture);
        require(fixture.services.date_picker().snapshot(id).draft == String{u8"07 November 2026"} &&
                    fixture.services.date_picker().snapshot(other).draft ==
                        (index == 0 ? String{u8"12 十一月 2026"} : String{u8"01 十一月 2026"}) &&
                    calendars == 0 && changes == 0 && open.get(),
                "range locale update changed endpoint/pending/authority or shared editor freeze");
        key(fixture, Key::enter);
        require(calendars == 1 && requested == CivilDate{2026, 11, 7},
                "range endpoint parsed its old draft with the new language");
    }
    for (const auto endpoint : {PickerEndpoint::Start, PickerEndpoint::End}) {
        Fixture fixture;
        Signal<LocaleId> language{locales::en_US};
        Signal<TimePickerRangeValue> authority{TimePickerRangeValue{TimeOfDay{8}, TimeOfDay{18}}};
        Signal<bool> open{true};
        TimeRangePickerRef reference;
        TimePickerValue requested;
        int calendars = 0;
        fixture.services.mount(Content{[&] {
            ConfigProvider(ConfigProviderProps{}.locale(language), ConfigProviderContent{[&] {
                               TimeRangePicker(TimeRangePickerProps{}
                                                   .ref(reference)
                                                   .value(authority)
                                                   .open(open)
                                                   .activeEndpoint(endpoint)
                                                   .picker(TimePickerProps{}.use12Hours().format(u8"h:mm A"))
                                                   .onCalendarChange([&](auto range, auto active) {
                                                       ++calendars;
                                                       requested =
                                                           active == PickerEndpoint::Start ? range.start : range.end;
                                                   }));
                           }});
        }});
        sync(fixture);
        const auto index = static_cast<std::size_t>(endpoint);
        const auto id = fixture.services.time_picker().mounted()[index];
        type(fixture, index == 0 ? reference.start : reference.end, u8"1:45 PM");
        language.set(locales::zh_CN);
        sync(fixture);
        require(fixture.services.time_picker().snapshot(id).draft == String{u8"1:45 PM"} && calendars == 0 &&
                    open.get(),
                "time range locale update consumed the active endpoint draft");
        key(fixture, Key::enter);
        require(calendars == 1 && requested == TimeOfDay{13, 45}, "time range endpoint lost old meridiem semantics");
    }
}

void datetime_composes_one_frozen_outer_context_and_keeps_inner_override() {
    std::cerr << "case: datetime_composes_one_frozen_outer_context_and_keeps_inner_override" << std::endl;
    for (const auto endpoint : {PickerEndpoint::Start, PickerEndpoint::End}) {
        Fixture fixture;
        Signal<LocaleId> language{locales::en_US};
        Signal<DateTimePickerRangeValue> authority{
            DateTimePickerRangeValue{CivilDateTime{CivilDate{2026, 11, 1}, TimeOfDay{8}},
                                     CivilDateTime{CivilDate{2026, 11, 12}, TimeOfDay{18}}}};
        Signal<bool> open{true};
        DateRangePickerRef reference;
        DatePickerDateTimeValue requested;
        int calendars = 0;
        fixture.services.mount(Content{[&] {
            ConfigProvider(ConfigProviderProps{}.locale(language), ConfigProviderContent{[&] {
                               DateRangePicker(DateTimeRangePickerProps{}
                                                   .ref(reference)
                                                   .value(authority)
                                                   .open(open)
                                                   .activeEndpoint(endpoint)
                                                   .picker(DatePickerProps{}
                                                               .showTime(TimePickerProps{}.use12Hours().precision(
                                                                   TimePickerPrecision::Millisecond))
                                                               .format(u8"DD MMMM YYYY h:mm:ss.SSS A"))
                                                   .onCalendarChange([&](auto range, auto active) {
                                                       ++calendars;
                                                       requested =
                                                           active == PickerEndpoint::Start ? range.start : range.end;
                                                   }));
                           }});
        }});
        sync(fixture);
        const auto id = fixture.services.date_picker().mounted()[static_cast<std::size_t>(endpoint)];
        type(fixture, endpoint == PickerEndpoint::Start ? reference.start : reference.end,
             u8"07 November 2026 1:45:16.125 PM");
        language.set(locales::zh_CN);
        sync(fixture);
        require(calendars == 0 && open.get() &&
                    fixture.services.date_picker().snapshot(id).draft == String{u8"07 November 2026 1:45:16.125 PM"},
                "DateTimeRange locale update consumed or reformatted an active endpoint");
        key(fixture, Key::enter);
        require(calendars == 1 && requested == CivilDateTime{CivilDate{2026, 11, 7}, TimeOfDay{13, 45, 16, 125}},
                "DateTimeRange endpoint lost its frozen combined parse or exact milliseconds");
    }
    for (const bool outer_override : {false, true}) {
        for (const bool inner_override : {false, true}) {
            Fixture fixture;
            Signal<LocaleId> language{locales::en_US};
            DatePickerRef reference;
            DatePickerDateTimeValue requested;
            int changes = 0;
            auto inner =
                TimePickerProps{}.use12Hours().precision(TimePickerPrecision::Millisecond).format(u8"h:mm:ss.SSS A");
            if (inner_override) {
                inner.locale(LocaleCatalog::builtin().pack(locales::zh_CN).data().picker);
            }
            auto props = DatePickerProps{}
                             .ref(reference)
                             .defaultOpen()
                             .showTime(inner)
                             .defaultDateTimeValue(CivilDateTime{CivilDate{2026, 11, 1}, TimeOfDay{12}})
                             .format(u8"DD MMMM YYYY h:mm:ss.SSS A")
                             .onDateTimeChange([&](auto value) {
                                 requested = value;
                                 ++changes;
                             });
            if (outer_override) {
                props.locale(PickerLocale{});
            }
            fixture.services.mount(Content{[&] {
                ConfigProvider(ConfigProviderProps{}.locale(language),
                               ConfigProviderContent{[&] { DatePicker(props); }});
            }});
            sync(fixture);
            const auto id = fixture.services.date_picker().mounted().front();
            const auto time = fixture.services.time_picker().mounted().front();
            type(fixture, reference, u8"07 November 2026 1:45:16.125 PM");
            language.set(locales::zh_CN);
            sync(fixture);
            require(fixture.services.date_picker().snapshot(id).draft == String{u8"07 November 2026 1:45:16.125 PM"} &&
                        changes == 0,
                    "DateTime rebuilt half of a dirty outer format");
            key(fixture, Key::enter);
            const CivilDateTime exact{CivilDate{2026, 11, 7}, TimeOfDay{13, 45, 16, 125}};
            require(changes == 1 && requested == exact &&
                        fixture.services.date_picker().snapshot(id).date_time_value == exact &&
                        fixture.services.date_picker().snapshot(id).draft ==
                            (outer_override ? String{u8"07 November 2026 1:45:16.125 PM"}
                                            : String{u8"07 十一月 2026 1:45:16.125 下午"}),
                    "DateTime outer parse lost exact value or used inner local Locale as outer authority");
            const bool inner_chinese = inner_override || !outer_override;
            require(fixture.services.time_picker().snapshot(time).draft ==
                        (inner_chinese ? String{u8"1:45:16.125 下午"} : String{u8"1:45:16.125 PM"}),
                    "explicit inner Locale or inherited explicit outer Locale priority was lost");
        }
    }
}

void commit_callbacks_can_begin_a_new_edit_without_old_tail_overwrite() {
    std::cerr << "case: commit_callbacks_can_begin_a_new_edit_without_old_tail_overwrite" << std::endl;
    {
        Fixture fixture;
        Signal<LocaleId> language{locales::en_US};
        DatePickerRef reference;
        int changes = 0;
        fixture.services.mount(Content{[&] {
            ConfigProvider(ConfigProviderProps{}.locale(language), ConfigProviderContent{[&] {
                               DatePicker(
                                   DatePickerProps{}
                                       .ref(reference)
                                       .defaultOpen()
                                       .defaultValue(CivilDate{2026, 11, 1})
                                       .format(u8"DD MMMM YYYY")
                                       .onChange([&](auto) {
                                           if (++changes == 1) {
                                               language.set(locales::zh_CN);
                                               require(reference.focus(), "reentrant date focus");
                                               const auto stamp = fixture.inputs.sessions().active();
                                               require(bool(fixture.inputs.editors().require(stamp.owner).select_all()),
                                                       "reentrant date select");
                                               require(bool(fixture.inputs.dispatch(
                                                           TextCommitted{String{u8"08 十一月 2026"}, stamp})),
                                                       "reentrant date edit");
                                           }
                                       }));
                           }});
        }});
        sync(fixture);
        const auto id = fixture.services.date_picker().mounted().front();
        type(fixture, reference, u8"07 November 2026");
        key(fixture, Key::enter);
        require(changes == 1 && fixture.services.date_picker().snapshot(id).draft == String{u8"08 十一月 2026"} &&
                    fixture.services.date_picker().snapshot(id).value == CivilDate{2026, 11, 7},
                "old date commit tail overwrote callback's new draft");
        key(fixture, Key::enter);
        require(changes == 2 && fixture.services.date_picker().snapshot(id).value == CivilDate{2026, 11, 8},
                "new date edit did not keep its own context");
    }
    {
        Fixture fixture;
        Signal<LocaleId> language{locales::en_US};
        TimePickerRef reference;
        int changes = 0;
        fixture.services.mount(Content{[&] {
            ConfigProvider(
                ConfigProviderProps{}.locale(language), ConfigProviderContent{[&] {
                    TimePicker(
                        TimePickerProps{}
                            .ref(reference)
                            .defaultOpen()
                            .use12Hours()
                            .format(u8"h:mm A")
                            .onChange([&](auto) {
                                if (++changes == 1) {
                                    language.set(locales::zh_CN);
                                    require(reference.focus(), "reentrant time focus");
                                    const auto stamp = fixture.inputs.sessions().active();
                                    require(bool(fixture.inputs.editors().require(stamp.owner).select_all()),
                                            "reentrant time select");
                                    require(bool(fixture.inputs.dispatch(TextCommitted{String{u8"2:20 下午"}, stamp})),
                                            "reentrant time edit");
                                }
                            }));
                }});
        }});
        sync(fixture);
        const auto id = fixture.services.time_picker().mounted().front();
        type(fixture, reference, u8"1:45 PM");
        key(fixture, Key::enter);
        require(changes == 1 && fixture.services.time_picker().snapshot(id).draft == String{u8"2:20 下午"} &&
                    fixture.services.time_picker().snapshot(id).value == TimeOfDay{13, 45},
                "old time commit tail overwrote callback's new draft");
        key(fixture, Key::enter);
        require(changes == 2 && fixture.services.time_picker().snapshot(id).value == TimeOfDay{14, 20},
                "new time edit lost its parser context");
    }
}

void equivalent_unused_config_is_idle_and_late_update_is_safe() {
    std::cerr << "case: equivalent_unused_config_is_idle_and_late_update_is_safe" << std::endl;
    Fixture fixture;
    Signal<int> revision{0};
    Signal<LocaleId> language{LocaleId("en-GB")};
    DatePickerRef reference;
    TimePickerRef time;
    fixture.services.mount(Content{[&] {
        ConfigProvider(ConfigProviderProps{}.locale(language).catalog(bind([&] {
            const auto step = revision.get();
            auto data = std::make_unique<LocaleData>(LocaleCatalog::builtin().pack(locales::en_US).data());
            if (step > 0) {
                data->common.retry = u8"Unused retry";
            }
            return LocaleCatalog({{LocaleId("en-GB"), LocalePack(std::move(*data)), locales::en_US}});
        })),
                       ConfigProviderContent{[&] {
                           DatePicker(DatePickerProps{}.ref(reference).defaultValue(CivilDate{2026, 11, 7}));
                           TimePicker(TimePickerProps{}.ref(time));
                       }});
    }});
    sync(fixture);
    static_cast<void>(fixture.frames.consume_request());
    const auto requests = fixture.frames.counters().requests;
    const auto deadline = fixture.services.next_frame_deadline();
    for (int i = 1; i <= 100; ++i) {
        revision.set(i);
    }
    require(!fixture.frames.pending() && fixture.frames.counters().requests == requests &&
                fixture.services.next_frame_deadline() == deadline,
            "unused/equal config emitted a frame or deadline");
    const auto date_id = fixture.services.date_picker().mounted().front();
    const auto time_id = fixture.services.time_picker().mounted().front();
    fixture.services.destroy(date_id);
    fixture.services.destroy(time_id);
    sync(fixture);
    static_cast<void>(fixture.frames.consume_request());
    const auto retired_requests = fixture.frames.counters().requests;
    language.set(locales::zh_CN);
    revision.set(101);
    require(!reference.focus() && !time.focus() && fixture.frames.counters().requests == retired_requests &&
                !fixture.frames.pending() && !fixture.services.next_frame_deadline(),
            "retired editor retained callbacks, subscriptions or deadlines");
}

} // namespace

int main() {
    suppress_dialogs();
    try {
        date_draft_keeps_month_parser();
        time_draft_keeps_meridiem_parser();
        selected_clean_and_composition_are_preserved();
        independent_format_locale_and_literals();
        controlled_refusal_and_frozen_alternatives();
        week_text_keeps_original_value_rules_and_panel_uses_live_rules();
        time_precision_and_hour_mode_are_frozen_for_text_commit();
        rejected_date_commit_keeps_frozen_draft_until_blur();
        time_controlled_refusal_uses_latest_accepted_display();
        range_endpoints_freeze_independently();
        datetime_composes_one_frozen_outer_context_and_keeps_inner_override();
        commit_callbacks_can_begin_a_new_edit_without_old_tail_overwrite();
        equivalent_unused_config_is_idle_and_late_update_is_safe();
        run_i18n_picker_range_cases();
        std::cout << "i18n picker editors: 13 original groups and range candidate cases passed\n";
        return 0;
    } catch (const std::exception& error) {
        std::cerr << error.what() << '\n';
        return 1;
    }
}
