#include "component/date_picker_component.hpp"
#include "component/time_picker_component.hpp"
#include "support/i18n_test.hpp"
#include "support/input_fixture.hpp"

namespace {
using namespace ryn;
using namespace ryn::input;
using namespace ryn_test::i18n;
using Fixture = ryn_test::input_component::Fixture;

void sync(Fixture& fixture) {
    require(fixture.services.layout_and_synchronize({1000, 1000}, {0, 0, 1000, 1000}), "range candidate layout");
}

void key(Fixture& fixture, Key value, KeyModifier modifiers = KeyModifier::none) {
    fixture.services.focus().dispatch({value, KeyAction::down, modifiers});
    fixture.services.focus().dispatch({value, KeyAction::up, modifiers});
    sync(fixture);
}

const detail::MountedInputComponent& input(Fixture& fixture, runtime::ComponentId id) {
    for (const auto& candidate : fixture.inputs.mounted_inputs()) {
        if (candidate.component == id) {
            return candidate;
        }
    }
    throw std::runtime_error("range candidate editor missing");
}

void type(Fixture& fixture, runtime::ComponentId id, String text) {
    static_cast<void>(fixture.services.focus().request_focus(input(fixture, id).interaction, FocusModality::keyboard));
    require(fixture.inputs.focused(id), "range candidate editor focus");
    key(fixture, Key::a, KeyModifier::control);
    require(fixture.inputs.focused(id) && fixture.inputs.sessions().active().owner == input(fixture, id).editor,
            "range candidate editor lost focus before text dispatch");
    require(bool(fixture.inputs.dispatch(TextCommitted{std::move(text), fixture.inputs.sessions().active()})),
            "range candidate real text event");
    sync(fixture);
}

bool below(Fixture& fixture, runtime::ComponentId child, runtime::ComponentId owner) {
    for (auto current = std::optional{child}; current; current = fixture.services.components().parent(*current)) {
        if (*current == owner) {
            return true;
        }
    }
    return false;
}

void button(Fixture& fixture, const String& label) {
    for (const auto& text : fixture.services.text().mounted_texts()) {
        if (fixture.scene.text_state(text.scene).content() != label.view()) {
            continue;
        }
        for (const auto& action : fixture.buttons.mounted_buttons()) {
            const auto* hit = fixture.services.interactions().find(action.interaction);
            if (below(fixture, text.component, action.component) && hit && hit->eligible &&
                fixture.services.interactions().component_active(action.component)) {
                static_cast<void>(fixture.services.focus().request_focus(action.interaction, FocusModality::keyboard));
                require(fixture.services.focus().state().focused == action.interaction, "range candidate action focus");
                key(fixture, Key::enter);
                return;
            }
        }
    }
    throw std::runtime_error("range candidate action missing");
}

void week_range_keeps_candidate_span_and_latest_business_rules() {
    std::cerr << "case: week_range_keeps_candidate_span_and_latest_business_rules" << std::endl;
    const auto english = LocaleCatalog::builtin().pack(locales::en_US).data().picker;
    const auto chinese = LocaleCatalog::builtin().pack(locales::zh_CN).data().picker;
    for (int rejection = 0; rejection != 3; ++rejection) {
        std::cerr << "  rejection=" << rejection << std::endl;
        Fixture fixture;
        Signal<LocaleId> language{locales::en_US};
        Signal<DatePickerValue> minimum{CivilDate{2026, 11, 2}};
        DatePickerRangeValue requested;
        DatePickerRangeValue calendar;
        int changes = 0;
        int calendars = 0;
        bool reject = false;
        bool saw_week_from = false;
        fixture.services.mount(Content{[&] {
            ConfigProvider(
                ConfigProviderProps{}.locale(language), ConfigProviderContent{[&] {
                    DateRangePicker(
                        DateRangePickerProps{}
                            .defaultOpen()
                            .defaultValue({CivilDate{2026, 10, 25}, CivilDate{2026, 11, 16}})
                            .picker(DatePickerProps{}
                                        .picker(DatePickerMode::Week)
                                        .minDate(minimum)
                                        .disabledDateInfo([&](CivilDate value, const DatePickerDisabledInfo& info) {
                                            if (value == CivilDate{2026, 11, 1} && info.type == DatePickerMode::Week &&
                                                info.from == value) {
                                                saw_week_from = true;
                                            }
                                            return false;
                                        }))
                            .disabledDate([&](CivilDate value, PickerEndpoint, const DatePickerRangeValue&) {
                                return reject && value == CivilDate{2026, 11, 1};
                            })
                            .onCalendarChange([&](auto value, auto) {
                                calendar = value;
                                ++calendars;
                            })
                            .onChange([&](auto value) {
                                requested = value;
                                ++changes;
                            }));
                }});
        }});
        sync(fixture);
        const auto start = fixture.services.date_picker().mounted()[0];
        const auto end = fixture.services.date_picker().mounted()[1];
        const auto draft = format_date(CivilDate{2026, 11, 1}, "gggg-[W]ww", english);
        type(fixture, fixture.services.date_picker().snapshot(start).input, draft);
        language.set(locales::zh_CN);
        if (rejection == 1) {
            minimum.set(CivilDate{2026, 11, 8});
        }
        reject = rejection == 2;
        key(fixture, Key::enter);
        if (rejection) {
            require(calendars == 0 && changes == 0 && fixture.services.date_picker().snapshot(start).draft == draft,
                    "Week range skipped latest min/custom or consumed a rejected draft");
            minimum.set(CivilDate{2026, 11, 2});
            reject = false;
            key(fixture, Key::enter);
        }
        require(calendars == 1 && calendar.start == CivilDate{2026, 11, 1} && saw_week_from,
                "Week range lost original Sunday span or effective disabled info/from");
        type(fixture, fixture.services.date_picker().snapshot(end).input,
             format_date(CivilDate{2026, 11, 16}, "gggg-[W]ww", chinese));
        key(fixture, Key::enter);
        require(changes == 1 && requested.start == CivilDate{2026, 11, 1} && requested.end == CivilDate{2026, 11, 16},
                "final range admission reinterpreted the staged Sunday endpoint");
    }
}

void time_range_keeps_precision_and_latest_rules() {
    std::cerr << "case: time_range_keeps_precision_and_latest_rules" << std::endl;
    for (int rejection = 0; rejection != 4; ++rejection) {
        std::cerr << "  rejection=" << rejection << std::endl;
        Fixture fixture;
        Signal<TimePickerPrecision> precision{TimePickerPrecision::Millisecond};
        Signal<bool> twelve{false};
        Signal<TimePickerRules> rules{TimePickerRules{}};
        TimePickerRangeValue requested;
        TimePickerRangeValue calendar;
        int changes = 0;
        int calendars = 0;
        fixture.services.mount(Content{[&] {
            TimeRangePicker(TimeRangePickerProps{}
                                .defaultOpen()
                                .defaultValue({TimeOfDay{8}, TimeOfDay{18}})
                                .picker(TimePickerProps{}.precision(precision).use12Hours(twelve).rules(rules))
                                .onCalendarChange([&](auto value, auto) {
                                    calendar = value;
                                    ++calendars;
                                })
                                .onChange([&](auto value) {
                                    requested = value;
                                    ++changes;
                                }));
        }});
        sync(fixture);
        const auto start = fixture.services.time_picker().mounted()[0];
        const auto end = fixture.services.time_picker().mounted()[1];
        type(fixture, fixture.services.time_picker().snapshot(start).selector, u8"13:45:16.125");
        precision.set(TimePickerPrecision::Minute);
        twelve.set(true);
        auto latest = rules.get();
        if (rejection == 1) {
            latest.max = TimeOfDay{13, 45, 16, 124};
        } else if (rejection == 2) {
            latest.millisecond_step = 250;
        } else if (rejection == 3) {
            latest.disabled_milliseconds = [](int, int, int, int ms) {
                return ms == 125;
            };
        }
        rules.set(latest);
        key(fixture, Key::enter);
        if (rejection) {
            require(calendars == 0 && changes == 0 &&
                        fixture.services.time_picker().snapshot(start).draft == String{u8"13:45:16.125"},
                    "TimeRange skipped latest max/step/disabled-ms or consumed rejected draft");
            rules.set(TimePickerRules{});
            key(fixture, Key::enter);
        }
        require(calendars == 1 && calendar.start == TimeOfDay{13, 45, 16, 125},
                "TimeRange parent used live Minute precision for an MS candidate");
        type(fixture, fixture.services.time_picker().snapshot(end).selector, u8"5:00 pm");
        key(fixture, Key::enter);
        require(changes == 1 && requested.start == TimeOfDay{13, 45, 16, 125} && requested.end == TimeOfDay{17},
                "TimeRange final commit lost one endpoint's independent precision");
    }
}

void datetime_range_outer_and_inner_candidates() {
    std::cerr << "case: datetime_range_outer_and_inner_candidates" << std::endl;
    const auto english = LocaleCatalog::builtin().pack(locales::en_US).data().picker;
    const auto chinese = LocaleCatalog::builtin().pack(locales::zh_CN).data().picker;
    for (const bool inner : {false, true}) {
        for (const auto endpoint : {PickerEndpoint::Start, PickerEndpoint::End}) {
            for (const bool confirm : {false, true}) {
                std::cerr << "  inner=" << inner << " endpoint=" << static_cast<int>(endpoint) << " confirm=" << confirm
                          << std::endl;
                Fixture fixture;
                Signal<LocaleId> language{locales::en_US};
                Signal<TimePickerPrecision> precision{TimePickerPrecision::Millisecond};
                Signal<PickerLocale> outer_locale{english};
                Signal<PickerLocale> inner_locale{confirm ? chinese : english};
                DateTimePickerRangeValue requested;
                DateTimePickerRangeValue calendar;
                int changes = 0;
                int calendars = 0;
                int inner_changes = 0;
                fixture.services.mount(Content{[&] {
                    ConfigProvider(ConfigProviderProps{}.locale(language), ConfigProviderContent{[&] {
                                       auto picker = DatePickerProps{}
                                                         .needConfirm(confirm)
                                                         .showTime(TimePickerProps{}
                                                                       .locale(inner_locale)
                                                                       .precision(precision)
                                                                       .use12Hours()
                                                                       .onChange([&](auto) { ++inner_changes; }))
                                                         .format(u8"DD MMMM YYYY h:mm:ss.SSS A");
                                       if (confirm) {
                                           picker.locale(outer_locale);
                                       }
                                       DateRangePicker(
                                           DateTimeRangePickerProps{}
                                               .defaultOpen()
                                               .activeEndpoint(endpoint)
                                               .defaultValue({CivilDateTime{CivilDate{2026, 11, 1}, TimeOfDay{8}},
                                                              CivilDateTime{CivilDate{2026, 11, 12}, TimeOfDay{18}}})
                                               .picker(std::move(picker))
                                               .onCalendarChange([&](auto value, auto) {
                                                   calendar = value;
                                                   ++calendars;
                                               })
                                               .onChange([&](auto value) {
                                                   requested = value;
                                                   ++changes;
                                               }));
                                   }});
                }});
                sync(fixture);
                const auto index = static_cast<std::size_t>(endpoint);
                const auto date_id = fixture.services.date_picker().mounted()[index];
                const auto time_id = fixture.services.time_picker().mounted()[index];
                type(fixture,
                     inner ? fixture.services.time_picker().snapshot(time_id).selector
                           : fixture.services.date_picker().snapshot(date_id).input,
                     inner ? (confirm ? String{u8"1:45:16.125 下午"} : String{u8"1:45:16.125 pm"})
                           : String{u8"07 November 2026 1:45:16.125 PM"});
                precision.set(TimePickerPrecision::Minute);
                language.set(locales::zh_CN);
                outer_locale.set(chinese);
                inner_locale.set(english);
                key(fixture, Key::enter);
                if (inner && confirm) {
                    require(calendars == 0 && inner_changes == 1, "inner time did not stage independently");
                    button(fixture, chinese.confirm);
                }
                require(calendars == 1 && (!inner || inner_changes == 1),
                        "DateTimeRange lost private inner request or user Time onChange");
                const auto selected = endpoint == PickerEndpoint::Start ? calendar.start : calendar.end;
                require(selected && selected->time == TimeOfDay{13, 45, 16, 125} &&
                            selected->date ==
                                (inner ? CivilDate{2026, 11, index == 0 ? 1U : 12U} : CivilDate{2026, 11, 7}),
                        "DateTimeRange reparsed independent outer/inner Locale or truncated frozen MS");
                if (endpoint == PickerEndpoint::Start) {
                    button(fixture, chinese.confirm_range);
                }
                const auto accepted = endpoint == PickerEndpoint::Start ? requested.start : requested.end;
                require(changes == 1 && accepted == selected,
                        "DateTimeRange final admission lost staged time precision");
                require(fixture.services.date_picker().snapshot(date_id).draft ==
                                format_date_time(*selected, "DD MMMM YYYY h:mm:ss.SSS A", chinese) &&
                            fixture.services.time_picker().snapshot(time_id).draft ==
                                format_time_of_day(selected->time, "h:mm a", english),
                        "outer/inner clean display did not use its own latest Locale and precision");
            }
        }
    }
}

void order_and_controlled_authority_keep_endpoint_provenance() {
    std::cerr << "case: order_and_controlled_authority_keep_endpoint_provenance" << std::endl;
    for (int echo = 0; echo != 3; ++echo) {
        Fixture fixture;
        Signal<TimePickerPrecision> precision{TimePickerPrecision::Millisecond};
        Signal<TimePickerRangeValue> authority{TimePickerRangeValue{TimeOfDay{8}, TimeOfDay{20}}};
        Signal<bool> open{true};
        TimePickerRangeValue requested;
        int changes = 0;
        bool sorted_from = false;
        fixture.services.mount(Content{[&] {
            TimeRangePicker(
                TimeRangePickerProps{}
                    .value(authority)
                    .open(open)
                    .picker(TimePickerProps{}.precision(precision))
                    .disabledTime([&](TimeOfDay value, PickerEndpoint endpoint, const TimePickerRangeValue& range) {
                        if (endpoint == PickerEndpoint::End && value == TimeOfDay{18, 45, 16, 125} &&
                            range.start == TimeOfDay{8}) {
                            sorted_from = true;
                        }
                        return false;
                    })
                    .onChange([&](auto value) {
                        requested = value;
                        ++changes;
                        if (echo == 1) {
                            authority.set(value);
                        } else if (echo == 2) {
                            authority.set({TimeOfDay{9}, TimeOfDay{21}});
                        }
                    }));
        }});
        sync(fixture);
        const auto start = fixture.services.time_picker().mounted()[0];
        const auto end = fixture.services.time_picker().mounted()[1];
        type(fixture, fixture.services.time_picker().snapshot(start).selector, u8"18:45:16.125");
        precision.set(TimePickerPrecision::Minute);
        key(fixture, Key::enter);
        type(fixture, fixture.services.time_picker().snapshot(end).selector, u8"08:00");
        key(fixture, Key::enter);
        require(changes == 1 && requested.start == TimeOfDay{8} && requested.end == TimeOfDay{18, 45, 16, 125} &&
                    sorted_from,
                "order detached value/precision or failed to validate sorted endpoint/range context");
        require(fixture.services.time_picker().snapshot(start).value == authority.get().start &&
                    fixture.services.time_picker().snapshot(end).value == authority.get().end,
                "controlled refusal/echo/replacement forged the accepted range");
        if (echo == 1) {
            type(fixture, fixture.services.time_picker().snapshot(start).selector, u8"06:00");
            key(fixture, Key::enter);
            button(fixture, PickerLocale{}.confirm_range);
            require(changes == 2 && requested.start == TimeOfDay{6} && requested.end == TimeOfDay{18, 45, 16, 125},
                    "accepted metadata did not survive a later edit of only the other endpoint");
        }
    }
    Fixture fixture;
    PickerRangePolicy policy;
    policy.disabled[0] = true;
    int changes = 0;
    fixture.services.mount(Content{[&] {
        TimeRangePicker(TimeRangePickerProps{}
                            .defaultOpen()
                            .policy(policy)
                            .defaultValue({TimeOfDay{18}, TimeOfDay{8}})
                            .picker(TimePickerProps{}.precision(TimePickerPrecision::Minute))
                            .onChange([&](auto) { ++changes; }));
    }});
    sync(fixture);
    const auto end = fixture.services.time_picker().mounted()[1];
    type(fixture, fixture.services.time_picker().snapshot(end).selector, u8"07:00");
    key(fixture, Key::enter);
    require(changes == 0, "order swapped a disabled endpoint");
}

void range_callbacks_cannot_overwrite_new_authority_or_editors() {
    std::cerr << "case: range_callbacks_cannot_overwrite_new_authority_or_editors" << std::endl;
    for (const int reentry : {0, 2, 1}) {
        std::cerr << "  reentry=" << reentry << std::endl;
        Fixture fixture;
        Signal<LocaleId> language{locales::en_US};
        Signal<DatePickerRangeValue> authority{DatePickerRangeValue{CivilDate{2026, 11, 1}, CivilDate{2026, 11, 12}}};
        Signal<bool> open{true};
        DateRangePickerRef reference;
        int changes = 0;
        int calendars = 0;
        int closes = 0;
        bool armed = false;
        const auto replace_draft = [&] {
            language.set(locales::zh_CN);
            const auto start = fixture.services.date_picker().mounted().front();
            const auto selector = fixture.services.date_picker().snapshot(start).input;
            require(reference.start.focus(), "reentry range focus");
            const auto& editor = fixture.inputs.editors().require(input(fixture, selector).editor);
            require(bool(fixture.inputs.replace_range(selector, {0, editor.value().size()},
                                                      String{u8"09 十一月 2026"}.view())),
                    "reentry range text");
        };
        fixture.services.mount(Content{[&] {
            ConfigProvider(ConfigProviderProps{}.locale(language), ConfigProviderContent{[&] {
                               DateRangePicker(
                                   DateRangePickerProps{}
                                       .ref(reference)
                                       .value(authority)
                                       .open(open)
                                       .onOpenChange([&](bool next) { closes += !next; })
                                       .picker(DatePickerProps{}.format(u8"DD MMMM YYYY"))
                                       .disabledDate([&](CivilDate value, PickerEndpoint, const DatePickerRangeValue&) {
                                           if (armed && reentry == 0 && value == CivilDate{2026, 11, 7}) {
                                               armed = false;
                                               authority.set({CivilDate{2026, 11, 3}, CivilDate{2026, 11, 14}});
                                           }
                                           return false;
                                       })
                                       .onCalendarChange([&](auto, auto) {
                                           ++calendars;
                                           if (armed && reentry == 1) {
                                               armed = false;
                                               replace_draft();
                                           }
                                       })
                                       .onChange([&](auto) {
                                           ++changes;
                                           if (armed && reentry == 2) {
                                               armed = false;
                                               replace_draft();
                                           }
                                       }));
                           }});
        }});
        sync(fixture);
        const auto start = fixture.services.date_picker().mounted()[0];
        const auto end = fixture.services.date_picker().mounted()[1];
        type(fixture, fixture.services.date_picker().snapshot(start).input, u8"07 November 2026");
        armed = true;
        key(fixture, Key::enter);
        if (reentry == 0) {
            require(calendars == 0 && changes == 0 &&
                        fixture.services.date_picker().snapshot(start).value == authority.get().start &&
                        fixture.services.date_picker().snapshot(end).value == authority.get().end,
                    "predicate reentry allowed old candidate to overwrite new authority");
        } else {
            if (reentry == 2) {
                type(fixture, fixture.services.date_picker().snapshot(end).input, u8"13 November 2026");
                key(fixture, Key::enter);
            }
            std::cerr << "  callback result: calendars=" << calendars << " changes=" << changes << " closes=" << closes
                      << " draft=" << fixture.services.date_picker().snapshot(start).draft.bytes()
                      << " focused=" << fixture.inputs.focused(fixture.services.date_picker().snapshot(start).input)
                      << " open=" << open.get() << std::endl;
            require(fixture.services.date_picker().snapshot(start).draft == String{u8"09 十一月 2026"} && open.get() &&
                        fixture.inputs.focused(fixture.services.date_picker().snapshot(start).input),
                    "old range operation overwrote callback's new draft/focus or continued closing");
            require(calendars == (reentry == 2 ? 2 : 1) && changes == (reentry == 2 ? 1 : 0) && closes == 0,
                    "superseded range request retried its old candidate or continued close callbacks");
            if (reentry == 1) {
                const auto before = calendars;
                key(fixture, Key::enter);
                std::cerr << "  callback new Locale submit: calendars=" << calendars << " expected=" << before + 1
                          << std::endl;
                require(calendars == before + 1 &&
                            fixture.services.date_picker().snapshot(start).value == CivilDate{2026, 11, 9},
                        "callback's new range draft retained the submitted editor's old Locale parser");
            }
        }
    }
    {
        std::cerr << "  callback time range new Locale" << std::endl;
        Fixture fixture;
        Signal<LocaleId> language{locales::en_US};
        TimeRangePickerRef reference;
        int calendars = 0;
        bool armed = true;
        fixture.services.mount(Content{[&] {
            ConfigProvider(
                ConfigProviderProps{}.locale(language), ConfigProviderContent{[&] {
                    TimeRangePicker(
                        TimeRangePickerProps{}
                            .ref(reference)
                            .defaultOpen()
                            .defaultValue({TimeOfDay{8}, TimeOfDay{18}})
                            .picker(TimePickerProps{}.use12Hours().format(u8"h:mm A"))
                            .onCalendarChange([&](auto, auto) {
                                ++calendars;
                                if (armed) {
                                    armed = false;
                                    language.set(locales::zh_CN);
                                    require(reference.start.focus(), "callback time range focus");
                                    const auto start = fixture.services.time_picker().mounted().front();
                                    const auto selector = fixture.services.time_picker().snapshot(start).selector;
                                    const auto& editor =
                                        fixture.inputs.editors().require(input(fixture, selector).editor);
                                    require(bool(fixture.inputs.replace_range(selector, {0, editor.value().size()},
                                                                              String{u8"2:20 下午"}.view())),
                                            "callback time range new draft");
                                }
                            }));
                }});
        }});
        sync(fixture);
        const auto start = fixture.services.time_picker().mounted().front();
        const auto selector = fixture.services.time_picker().snapshot(start).selector;
        type(fixture, selector, u8"1:45 PM");
        key(fixture, Key::enter);
        require(calendars == 1 && fixture.services.time_picker().snapshot(start).draft == String{u8"2:20 下午"} &&
                    fixture.inputs.focused(selector),
                "old time range request overwrote callback's new draft or focus");
        key(fixture, Key::enter);
        std::cerr << "  callback new Time Locale submit: calendars=" << calendars << " expected=2" << std::endl;
        require(calendars == 2 && fixture.services.time_picker().snapshot(start).value == TimeOfDay{14, 20},
                "callback's new time range draft retained the submitted editor's old Locale parser");
    }
}

void reset_cancel_clear_and_external_close_end_old_candidates() {
    std::cerr << "case: reset_cancel_clear_and_external_close_end_old_candidates" << std::endl;
    for (int action = 0; action != 5; ++action) {
        Fixture fixture;
        Signal<TimePickerPrecision> precision{TimePickerPrecision::Millisecond};
        Signal<bool> open{true};
        Signal<bool> disabled{false};
        TimeRangePickerRef reference;
        TimePickerRangeValue requested;
        int changes = 0;
        fixture.services.mount(Content{[&] {
            TimeRangePicker(TimeRangePickerProps{}
                                .ref(reference)
                                .open(open)
                                .disabled(disabled)
                                .defaultValue({TimeOfDay{8}, TimeOfDay{18}})
                                .picker(TimePickerProps{}.precision(precision))
                                .onChange([&](auto value) {
                                    requested = value;
                                    ++changes;
                                }));
        }});
        sync(fixture);
        const auto start = fixture.services.time_picker().mounted()[0];
        const auto end = fixture.services.time_picker().mounted()[1];
        type(fixture, fixture.services.time_picker().snapshot(start).selector, u8"13:45:16.125");
        precision.set(TimePickerPrecision::Minute);
        key(fixture, Key::enter);
        type(fixture, fixture.services.time_picker().snapshot(end).selector, u8"17:30");
        if (action == 0) {
            const auto reset = detail::capture_form_input(reference.start).second;
            require(bool(reset), "range explicit reset hook");
            reset();
        } else if (action == 1) {
            button(fixture, PickerLocale{}.cancel_range);
        } else if (action == 2) {
            button(fixture, PickerLocale{}.clear_range);
        } else if (action == 3) {
            open.set(false);
        } else {
            disabled.set(true);
        }
        sync(fixture);
        if (action == 2) {
            require(changes == 1 && !requested.start && !requested.end, "clear retained stale pending endpoints");
        } else {
            require(changes == 0 && fixture.services.time_picker().snapshot(start).value == TimeOfDay{8} &&
                        fixture.services.time_picker().snapshot(end).value == TimeOfDay{18} &&
                        fixture.services.time_picker().snapshot(end).draft == String{u8"18:00"},
                    "reset/cancel/close/disable retained frozen pending value or draft");
        }
    }
}

void range_format_locale_and_live_mode_do_not_replace_candidate_mode() {
    std::cerr << "case: range_format_locale_and_live_mode_do_not_replace_candidate_mode" << std::endl;
    Fixture fixture;
    Signal<LocaleId> formatting{locales::en_US};
    Signal<DatePickerMode> mode{DatePickerMode::Week};
    const auto english = LocaleCatalog::builtin().pack(locales::en_US).data().picker;
    const auto chinese = LocaleCatalog::builtin().pack(locales::zh_CN).data().picker;
    DatePickerRangeValue requested;
    int changes = 0;
    bool saw_week = false;
    fixture.services.mount(Content{[&] {
        ConfigProvider(
            ConfigProviderProps{}.locale(locales::zh_CN).formatLocale(formatting), ConfigProviderContent{[&] {
                DateRangePicker(
                    DateRangePickerProps{}
                        .defaultOpen()
                        .defaultValue({CivilDate{2026, 10, 25}, CivilDate{2026, 11, 16}})
                        .picker(DatePickerProps{}
                                    .picker(mode)
                                    .minDate(CivilDate{2026, 11, 2})
                                    .disabledDateInfo([&](CivilDate value, const DatePickerDisabledInfo& info) {
                                        if (value == CivilDate{2026, 11, 1} && info.type == DatePickerMode::Week &&
                                            info.from == value) {
                                            saw_week = true;
                                        }
                                        return false;
                                    }))
                        .onChange([&](auto value) {
                            requested = value;
                            ++changes;
                        }));
            }});
    }});
    sync(fixture);
    const auto start = fixture.services.date_picker().mounted()[0];
    const auto end = fixture.services.date_picker().mounted()[1];
    type(fixture, fixture.services.date_picker().snapshot(start).input,
         format_date(CivilDate{2026, 11, 1}, "gggg-[W]ww", english));
    formatting.set(locales::zh_CN);
    mode.set(DatePickerMode::Month);
    key(fixture, Key::enter);
    require(saw_week && fixture.services.date_picker().snapshot(start).value == CivilDate{2026, 11, 1} &&
                fixture.services.date_picker().snapshot(end).mode == DatePickerMode::Month,
            "range guessed text mode/first-day from live panel instead of formatLocale candidate");
    type(fixture, fixture.services.date_picker().snapshot(end).input,
         format_date(CivilDate{2026, 12, 1}, "YYYY-MM", chinese));
    key(fixture, Key::enter);
    require(changes == 1 && requested.start == CivilDate{2026, 11, 1} && requested.end == CivilDate{2026, 12, 1},
            "range final admission replaced an older Week endpoint with live Month semantics");
}

void datetime_authority_replacement_releases_outer_parser_and_inner_pending() {
    std::cerr << "case: datetime_authority_replacement_releases_outer_parser_and_inner_pending" << std::endl;
    for (int authority_mode = 0; authority_mode != 3; ++authority_mode) {
        const bool controlled = authority_mode != 0;
        Fixture fixture;
        Signal<LocaleId> language{locales::en_US};
        Signal<DatePickerDateTimeValue> authority{CivilDateTime{CivilDate{2026, 11, 1}, TimeOfDay{8}}};
        DatePickerDateTimeValue requested;
        int changes = 0;
        int time_changes = 0;
        fixture.services.mount(Content{[&] {
            ConfigProvider(ConfigProviderProps{}.locale(language), ConfigProviderContent{[&] {
                               auto props =
                                   DatePickerProps{}
                                       .defaultOpen()
                                       .needConfirm(false)
                                       .showTime(TimePickerProps{}
                                                     .use12Hours()
                                                     .precision(TimePickerPrecision::Millisecond)
                                                     .onChange([&](auto) { ++time_changes; }))
                                       .format(u8"DD MMMM YYYY h:mm:ss.SSS A")
                                       .onDateTimeChange([&](auto value) {
                                           requested = value;
                                           ++changes;
                                           if (authority_mode == 1) {
                                               authority.set(value);
                                           } else if (authority_mode == 2) {
                                               authority.set(CivilDateTime{CivilDate{2026, 11, 3}, TimeOfDay{9}});
                                           }
                                       });
                               if (controlled) {
                                   props.dateTimeValue(authority);
                               } else {
                                   props.defaultDateTimeValue(authority.get());
                               }
                               DatePicker(std::move(props));
                           }});
        }});
        sync(fixture);
        const auto date = fixture.services.date_picker().mounted().front();
        const auto time = fixture.services.time_picker().mounted().front();
        type(fixture, fixture.services.time_picker().snapshot(time).selector, u8"1:45:16.125 pm");
        key(fixture, Key::enter);
        if (authority_mode == 2) {
            require(changes == 1 && time_changes == 0 &&
                        fixture.services.date_picker().snapshot(date).date_time_value == authority.get() &&
                        fixture.services.time_picker().snapshot(time).value == TimeOfDay{9} &&
                        fixture.services.time_picker().snapshot(time).draft == String{u8"9:00:00.000 am"},
                    "different DateTime authority inside request retained or consumed the old inner draft");
            continue;
        }
        require(changes == 1 && time_changes == 1 && requested && requested->time == TimeOfDay{13, 45, 16, 125} &&
                    fixture.services.time_picker().snapshot(time).value == TimeOfDay{13, 45, 16, 125},
                "immediate inner request did not publish pending authority or preserve callback on controlled echo");
        if (controlled) {
            type(fixture, fixture.services.date_picker().snapshot(date).input, u8"07 November 2026 2:20:17.250 PM");
            authority.set(CivilDateTime{CivilDate{2026, 11, 1}, TimeOfDay{9}});
            language.set(locales::zh_CN);
            key(fixture, Key::enter);
            require(changes == 1 && fixture.services.date_picker().snapshot(date).date_time_value == authority.get(),
                    "external DateTime authority retained old dirty parser or old candidate");
            type(fixture, fixture.services.date_picker().snapshot(date).input, u8"08 十一月 2026 2:20:17.250 下午");
            key(fixture, Key::enter);
            require(changes == 2 && requested == CivilDateTime{CivilDate{2026, 11, 8}, TimeOfDay{14, 20, 17, 250}},
                    "new DateTime edit after authority replacement used the obsolete parser");
        }
    }
}

void range_preset_factory_cannot_resume_after_reset_authority_or_destruction() {
    std::cerr << "case: range_preset_factory_cannot_resume_after_reset_authority_or_destruction" << std::endl;
    for (int reentry = 0; reentry != 3; ++reentry) {
        Fixture fixture;
        Signal<TimePickerRangeValue> authority{TimePickerRangeValue{TimeOfDay{8}, TimeOfDay{18}}};
        TimeRangePickerRef reference;
        runtime::ComponentId owner;
        int changes = 0;
        int calendars = 0;
        fixture.services.mount(Content{[&] {
            TimeRangePicker(TimeRangePickerProps{}
                                .ref(reference)
                                .value(authority)
                                .defaultOpen()
                                .presets({{u8"replace", u8"Replace range",
                                           [&] {
                                               if (reentry == 0) {
                                                   const auto reset =
                                                       detail::capture_form_input(reference.start).second;
                                                   require(bool(reset), "preset range reset hook");
                                                   reset();
                                               } else if (reentry == 1) {
                                                   authority.set({TimeOfDay{9}, TimeOfDay{19}});
                                               } else {
                                                   fixture.services.destroy(owner);
                                               }
                                               return TimePickerRangeValue{TimeOfDay{10}, TimeOfDay{20}};
                                           }}})
                                .onCalendarChange([&](auto, auto) { ++calendars; })
                                .onChange([&](auto) { ++changes; }));
        }});
        sync(fixture);
        const auto start = fixture.services.time_picker().mounted().front();
        owner = *fixture.services.components().parent(start);
        button(fixture, u8"Replace range");
        require(changes == 0 && calendars == 0, "preset factory supersession still emitted old callbacks");
        if (reentry == 2) {
            require(!fixture.services.components().contains(start), "preset owner destruction did not retire children");
        } else {
            require(fixture.services.time_picker().snapshot(start).value == authority.get().start,
                    "preset return overwrote reset/new authority");
        }
    }
}

void range_clear_preserves_a_disabled_endpoint() {
    std::cerr << "case: range_clear_preserves_a_disabled_endpoint" << std::endl;
    Fixture fixture;
    PickerRangePolicy policy;
    policy.disabled[0] = true;
    policy.allow_empty[1] = true;
    TimePickerRangeValue requested;
    int changes = 0;
    fixture.services.mount(Content{[&] {
        TimeRangePicker(TimeRangePickerProps{}
                            .defaultOpen()
                            .policy(policy)
                            .defaultValue({TimeOfDay{8, 45, 16, 125}, TimeOfDay{18}})
                            .picker(TimePickerProps{}.precision(TimePickerPrecision::Millisecond))
                            .onChange([&](auto value) {
                                requested = value;
                                ++changes;
                            }));
    }});
    sync(fixture);
    button(fixture, PickerLocale{}.clear_range);
    require(changes == 1 && requested.start == TimeOfDay{8, 45, 16, 125} && !requested.end,
            "clear erased or truncated the disabled endpoint");
}

void datetime_order_and_footer_commit_publish_metadata_before_child_values() {
    std::cerr << "case: datetime_order_and_footer_commit_publish_metadata_before_child_values" << std::endl;
    Fixture fixture;
    Signal<TimePickerPrecision> precision{TimePickerPrecision::Millisecond};
    Signal<bool> open{true};
    DateTimePickerRangeValue requested;
    int changes = 0;
    fixture.services.mount(Content{[&] {
        DateRangePicker(DateTimeRangePickerProps{}
                            .open(open)
                            .defaultValue({CivilDateTime{CivilDate{2026, 11, 1}, TimeOfDay{8}},
                                           CivilDateTime{CivilDate{2026, 11, 1}, TimeOfDay{20}}})
                            .picker(DatePickerProps{}
                                        .needConfirm(false)
                                        .showTime(TimePickerProps{}.precision(precision))
                                        .format(u8"YYYY-MM-DD HH:mm:ss.SSS"))
                            .onChange([&](auto value) {
                                requested = value;
                                ++changes;
                            }));
    }});
    sync(fixture);
    const auto start = fixture.services.date_picker().mounted()[0];
    const auto end = fixture.services.date_picker().mounted()[1];
    type(fixture, fixture.services.date_picker().snapshot(start).input, u8"2026-11-01 18:45:16.125");
    precision.set(TimePickerPrecision::Minute);
    key(fixture, Key::enter);
    type(fixture, fixture.services.date_picker().snapshot(end).input, u8"2026-11-01 08:00:00.000");
    key(fixture, Key::enter);
    require(changes == 1 && requested.start && requested.end && requested.start->time == TimeOfDay{8} &&
                requested.end->time == TimeOfDay{18, 45, 16, 125} &&
                fixture.services.date_picker().snapshot(start).date_time_value == requested.start &&
                fixture.services.date_picker().snapshot(end).date_time_value == requested.end,
            "DateTime order treated its own child projection as external authority or detached precision");
    type(fixture, fixture.services.date_picker().snapshot(start).input, u8"2026-11-01 06:00:00.000");
    key(fixture, Key::enter);
    button(fixture, PickerLocale{}.confirm_range);
    require(changes == 2 && requested.start && requested.end && requested.start->time == TimeOfDay{6} &&
                requested.end->time == TimeOfDay{18, 45, 16, 125},
            "footer commit or later independent DateTime edit lost accepted endpoint precision");
}

void equal_range_requests_consume_the_editor_without_value_notifications() {
    std::cerr << "case: equal_range_requests_consume_the_editor_without_value_notifications" << std::endl;
    Fixture fixture;
    Signal<TimePickerPrecision> precision{TimePickerPrecision::Millisecond};
    int calendars = 0;
    fixture.services.mount(Content{[&] {
        TimeRangePicker(TimeRangePickerProps{}
                            .defaultOpen()
                            .defaultValue({TimeOfDay{8}, TimeOfDay{18}})
                            .picker(TimePickerProps{}.precision(precision).use12Hours().format(u8"h:mm:ss.SSS A"))
                            .onCalendarChange([&](auto, auto) { ++calendars; }));
    }});
    sync(fixture);
    const auto start = fixture.services.time_picker().mounted().front();
    type(fixture, fixture.services.time_picker().snapshot(start).selector, u8"08:00:00.000 AM");
    precision.set(TimePickerPrecision::Minute);
    key(fixture, Key::enter);
    std::cerr << "  equal endpoint: calendars=" << calendars
              << " draft=" << fixture.services.time_picker().snapshot(start).draft.bytes() << std::endl;
    require(calendars == 1 && fixture.services.time_picker().snapshot(start).draft == String{u8"8:00:00.000 AM"},
            "equal typed endpoint request waited for a suppressed value notification to finish editing");
    type(fixture, fixture.services.time_picker().snapshot(start).selector, u8"9:00:16.125 AM");
    std::cerr << "  new Minute draft: confirmable=" << fixture.services.time_picker().snapshot(start).confirmable
              << " draft=" << fixture.services.time_picker().snapshot(start).draft.bytes() << std::endl;
    key(fixture, Key::enter);
    std::cerr << "  new Minute result: calendars=" << calendars
              << " draft=" << fixture.services.time_picker().snapshot(start).draft.bytes() << std::endl;
    require(calendars == 1 && fixture.services.time_picker().snapshot(start).draft == String{u8"9:00:16.125 AM"},
            "a new Minute draft inherited the previous equal request's frozen MS parser");
}

void calendar_callback_new_composition_supersedes_the_old_request() {
    std::cerr << "case: calendar_callback_new_composition_supersedes_the_old_request" << std::endl;
    Fixture fixture;
    DateRangePickerRef reference;
    int calendars = 0;
    fixture.services.mount(Content{[&] {
        DateRangePicker(DateRangePickerProps{}
                            .ref(reference)
                            .defaultOpen()
                            .defaultValue({CivilDate{2026, 11, 1}, CivilDate{2026, 11, 12}})
                            .onCalendarChange([&](auto, auto) {
                                ++calendars;
                                const auto stamp = fixture.inputs.sessions().active();
                                require(bool(fixture.inputs.editors()
                                                 .require(stamp.owner)
                                                 .update_composition({String{u8"新"}, {0, 1}, {}})),
                                        "calendar callback new composition");
                            }));
    }});
    sync(fixture);
    const auto start = fixture.services.date_picker().mounted().front();
    const auto selector = fixture.services.date_picker().snapshot(start).input;
    type(fixture, selector, u8"2026-11-07");
    key(fixture, Key::enter);
    const auto& editor = fixture.inputs.editors().require(input(fixture, selector).editor);
    require(calendars == 1 && editor.composition().active && editor.composition().text == String{u8"新"}.bytes() &&
                fixture.inputs.focused(selector) &&
                fixture.services.date_picker().snapshot(start).draft == String{u8"2026-11-07"},
            "old range request cleared callback preedit or continued automatic endpoint activation");
}

void change_callback_other_endpoint_composition_blocks_the_old_close() {
    std::cerr << "case: change_callback_other_endpoint_composition_blocks_the_old_close" << std::endl;
    Fixture fixture;
    Signal<DatePickerRangeValue> authority{DatePickerRangeValue{CivilDate{2026, 11, 1}, CivilDate{2026, 11, 12}}};
    Signal<bool> open{true};
    DateRangePickerRef reference;
    int changes = 0;
    int closes = 0;
    fixture.services.mount(Content{[&] {
        DateRangePicker(DateRangePickerProps{}
                            .ref(reference)
                            .value(authority)
                            .open(open)
                            .onOpenChange([&](bool next) { closes += !next; })
                            .onChange([&](auto) {
                                ++changes;
                                const auto start = fixture.services.date_picker().mounted().front();
                                const auto selector = fixture.services.date_picker().snapshot(start).input;
                                require(reference.start.focus(), "other endpoint preedit focus");
                                require(bool(fixture.inputs.editors()
                                                 .require(input(fixture, selector).editor)
                                                 .update_composition({String{u8"新"}, {0, 1}, {}})),
                                        "other endpoint new preedit");
                            }));
    }});
    sync(fixture);
    const auto start = fixture.services.date_picker().mounted()[0];
    const auto end = fixture.services.date_picker().mounted()[1];
    const auto selector = fixture.services.date_picker().snapshot(start).input;
    type(fixture, selector, u8"2026-11-07");
    key(fixture, Key::enter);
    type(fixture, fixture.services.date_picker().snapshot(end).input, u8"2026-11-13");
    key(fixture, Key::enter);
    require(fixture.inputs.focused(selector), "other endpoint deferred preedit focus");
    require(
        bool(fixture.inputs.dispatch(CompositionChanged{String{u8"新"}, {0, 1}, fixture.inputs.sessions().active()})),
        "other endpoint focused native preedit");
    const auto& editor = fixture.inputs.editors().require(input(fixture, selector).editor);
    require(changes == 1 && closes == 0 && open.get() && editor.composition().active &&
                editor.composition().text == String{u8"新"}.bytes() && fixture.inputs.focused(selector),
            "old range close ignored a new composition in the other endpoint");
}

void equal_controlled_request_without_echo_keeps_original_accepted_semantics() {
    std::cerr << "case: equal_controlled_request_without_echo_keeps_original_accepted_semantics" << std::endl;
    Fixture fixture;
    Signal<TimePickerPrecision> precision{TimePickerPrecision::Millisecond};
    Signal<TimePickerRangeValue> authority{TimePickerRangeValue{TimeOfDay{13, 45, 16, 125}, TimeOfDay{18}}};
    Signal<bool> open{true};
    int changes = 0;
    int calendars = 0;
    fixture.services.mount(Content{[&] {
        TimeRangePicker(TimeRangePickerProps{}
                            .value(authority)
                            .open(open)
                            .picker(TimePickerProps{}.precision(precision).use12Hours().format(u8"h:mm:ss.SSS A"))
                            .onCalendarChange([&](auto, auto) { ++calendars; })
                            .onChange([&](auto) { ++changes; }));
    }});
    sync(fixture);
    const auto start = fixture.services.time_picker().mounted()[0];
    const auto end = fixture.services.time_picker().mounted()[1];
    type(fixture, fixture.services.time_picker().snapshot(start).selector, u8"01:45:16.125 PM");
    precision.set(TimePickerPrecision::Minute);
    key(fixture, Key::enter);
    type(fixture, fixture.services.time_picker().snapshot(end).selector, u8"6:00:00.000 PM");
    key(fixture, Key::enter);
    require(calendars == 2 && changes == 0, "equal controlled request emitted a fake authority change");
    type(fixture, fixture.services.time_picker().snapshot(end).selector, u8"7:00:00.000 PM");
    key(fixture, Key::enter);
    require(calendars == 3 && changes == 0 && authority.get().end == TimeOfDay{18},
            "equal no-echo request attached new text provenance to the old controlled authority");
}
} // namespace

void run_i18n_picker_range_cases() {
    std::cerr << "case: run_i18n_picker_range_cases" << std::endl;
    week_range_keeps_candidate_span_and_latest_business_rules();
    time_range_keeps_precision_and_latest_rules();
    datetime_range_outer_and_inner_candidates();
    order_and_controlled_authority_keep_endpoint_provenance();
    range_callbacks_cannot_overwrite_new_authority_or_editors();
    reset_cancel_clear_and_external_close_end_old_candidates();
    range_format_locale_and_live_mode_do_not_replace_candidate_mode();
    datetime_authority_replacement_releases_outer_parser_and_inner_pending();
    range_preset_factory_cannot_resume_after_reset_authority_or_destruction();
    range_clear_preserves_a_disabled_endpoint();
    datetime_order_and_footer_commit_publish_metadata_before_child_values();
    equal_range_requests_consume_the_editor_without_value_notifications();
    calendar_callback_new_composition_supersedes_the_old_request();
    change_callback_other_endpoint_composition_blocks_the_old_close();
    equal_controlled_request_without_echo_keeps_original_accepted_semantics();
}
