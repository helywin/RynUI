#include "component/date_picker_component.hpp"
#include "component/calendar_component.hpp"
#include "runtime/prop_connection.hpp"
#include "support/i18n_test.hpp"
#include "support/input_fixture.hpp"

#include <map>
#include <set>

namespace {
using namespace ryn;
using namespace ryn::input;
using namespace ryn_test::i18n;
using CellKey = std::pair<DatePickerMode, CivilDate>;

struct CellRecord final {
    InputRef reference;
    detail::MountedInputComponent input;
    runtime::ComponentId cell;
    Prop<DatePickerCellInfo> info{DatePickerCellInfo{}};
    I18n i18n;
    Color primary;
    int edits{};
};

struct RichFixture final {
    // The fixture retires all callbacks before their captured records disappear.
    std::map<CellKey, CellRecord> records;
    int factories{};
    int footer_mounts{};
    int changes{};
    DatePickerValue requested;
    InputRef footer;
    InputRef sibling;
    detail::MountedInputComponent footer_input;
    detail::MountedInputComponent sibling_input;
    std::function<void(CellKey, bool)> hook;
    ryn_test::input_component::Fixture fixture;

    DatePickerCellRenderer renderer() {
        return [this](Prop<DatePickerCellInfo> info) {
            ++factories;
            const auto value = detail::read_prop(info);
            if (!value.date) {
                return DatePickerCell{[] { Text(u8""); }};
            }
            const CellKey key{value.type, *value.date};
            auto& record = records[key];
            record.reference = InputRef{};
            record.info = info;
            if (hook) {
                hook(key, false);
            }
            return DatePickerCell{[this, key] {
                auto& cell = records.at(key);
                Input(InputProps{}
                          .ref(cell.reference)
                          .defaultValue(u8"cell")
                          .layout(LayoutStyle{}.width(dp(28)))
                          .onChange([this, key](String) { ++records.at(key).edits; }));
                cell.input = fixture.inputs.mounted_inputs().back();
                cell.cell = *fixture.services.components().parent(cell.input.component);
                cell.i18n = useI18n();
                cell.primary =
                    runtime::require_component_build_context().theme_scope()->snapshot().seed().color_primary;
                if (hook) {
                    hook(key, true);
                }
            }};
        };
    }

    DatePickerFooter footer_slot() {
        return DatePickerFooter{[this] {
            ++footer_mounts;
            Input(InputProps{}.ref(footer).defaultValue(u8"footer"));
            footer_input = fixture.inputs.mounted_inputs().back();
        }};
    }

    void mount_sibling() {
        Input(InputProps{}.ref(sibling));
        sibling_input = fixture.inputs.mounted_inputs().back();
    }

    void sync() {
        require(fixture.services.layout_and_synchronize({900, 900}, {0, 0, 900, 900}), "retained picker layout");
    }

    void settle(runtime::ComponentId id) {
        for (int pass = 0; pass < 4; ++pass) {
            sync();
            if (!fixture.services.components().contains(id) ||
                !fixture.services.date_picker().snapshot(id).grid_pending) {
                return;
            }
        }
        throw std::runtime_error("keyed picker did not settle its latest revision");
    }

    void key(Key key, KeyModifier modifiers = KeyModifier::none) {
        fixture.services.focus().dispatch({key, KeyAction::down, modifiers});
        fixture.services.focus().dispatch({key, KeyAction::up, modifiers});
        sync();
    }
};

std::map<CellKey, runtime::ComponentId> keys(const detail::DatePickerSnapshot& snapshot) {
    require(snapshot.cells.size() == snapshot.cell_info.size(), "published metadata count disagrees with cells");
    std::map<CellKey, runtime::ComponentId> result;
    std::set<std::pair<std::uint32_t, std::uint32_t>> identities;
    for (std::size_t i = 0; i < snapshot.cells.size(); ++i) {
        require(identities.emplace(snapshot.cells[i].index, snapshot.cells[i].generation).second,
                "two raw cells shared one component identity");
        require(snapshot.cell_info[i].type == snapshot.mode, "published mode disagrees with cell mode");
        if (snapshot.cell_info[i].date) {
            require(result.emplace(CellKey{snapshot.mode, *snapshot.cell_info[i].date}, snapshot.cells[i]).second,
                    "duplicate mode/raw CivilDate key");
        }
    }
    return result;
}

void keyed_editing_geometry_and_tab_order() {
    std::cerr << "case: keyed_editing_geometry_and_tab_order" << std::endl;
    RichFixture test;
    Signal<LocaleId> language{locales::en_US};
    test.fixture.services.mount(Content{[&] {
        ConfigProvider(ConfigProviderProps{}.locale(language), ConfigProviderContent{[&] {
                           DatePicker(DatePickerProps{}
                                          .defaultOpen()
                                          .defaultPanelDate({2026, 11, 1})
                                          .cellRender(test.renderer())
                                          .onChange([&](auto value) {
                                              ++test.changes;
                                              test.requested = value;
                                          }),
                                      test.footer_slot());
                       }});
        test.mount_sibling();
    }});
    test.sync();
    const auto id = test.fixture.services.date_picker().mounted().front();
    const auto before = test.fixture.services.date_picker().snapshot(id);
    const auto original = keys(before);
    auto& cell = test.records.at({DatePickerMode::Date, CivilDate{2026, 11, 7}});
    const auto component = cell.cell;
    const auto input = cell.input;
    const auto previous_bounds = test.fixture.nodes.require(test.fixture.services.components().root(component)).bounds;
    const auto leaving = test.records.at({DatePickerMode::Date, CivilDate{2026, 12, 12}}).reference;
    require(cell.reference.focus(), "rich date cell focus");
    auto& editor = test.fixture.inputs.editors().require(input.editor);
    require(bool(editor.select_all()), "rich editor selection");
    require(
        bool(test.fixture.inputs.dispatch(TextCommitted{String{u8"a中文z"}, test.fixture.inputs.sessions().active()})),
        "rich text event");
    require(cell.reference.select(7, 1), "rich reverse selection");
    const auto selection = editor.selection();
    const auto history = editor.history();
    require(bool(editor.update_composition({String{u8"拼"}, {0, 1}, {}})), "rich preedit");
    const auto stamp = test.fixture.inputs.sessions().active();
    const auto edits = cell.edits;
    language.set(locales::zh_CN);
    // The published list remains self-consistent before the safe auxiliary pass.
    const auto queued = test.fixture.services.date_picker().snapshot(id);
    require(queued.grid_pending && queued.cells == before.cells && queued.mode == before.mode,
            "pending grid exposed unpublished indices");
    test.settle(id);
    const auto after = test.fixture.services.date_picker().snapshot(id);
    const auto updated = keys(after);
    int retained = 0;
    for (const auto& [key, child] : original) {
        if (const auto found = updated.find(key); found != updated.end()) {
            require(found->second == child, "same raw date received another component identity");
            ++retained;
        } else {
            require(!test.fixture.services.components().contains(child), "leaving date subtree did not retire");
        }
    }
    require(retained == 36 && test.factories == 48 && test.footer_mounts == 1 && !leaving.focus(),
            "Sunday/Monday key difference was not 36/6/6");
    require(cell.cell == component && cell.input.editor == input.editor &&
                cell.input.interaction == input.interaction && cell.input.node == input.node &&
                editor.value() == String{u8"a中文z"}.bytes() && editor.selection() == selection &&
                editor.composition().active && editor.composition().text == String{u8"拼"}.bytes() &&
                editor.history().undo_count == history.undo_count && test.fixture.inputs.sessions().active() == stamp &&
                test.fixture.inputs.focused(input.component) && cell.edits == edits && test.changes == 0,
            "locale reordering lost editor/session/draft/preedit/history/focus or emitted changes");
    const auto bounds = test.fixture.nodes.require(test.fixture.services.components().root(component)).bounds;
    require(bounds != previous_bounds, "retained cell was not placed at its new grid position");
    const auto input_bounds = test.fixture.nodes.require(input.node).bounds;
    require(test.fixture.services.hit_test().hit_test({input_bounds.x + input_bounds.width / 2,
                                                       input_bounds.y + input_bounds.height / 2}) == input.interaction,
            "retained Input hit clip did not follow translated grid geometry");
    editor.cancel_composition();
    test.key(Key::tab);
    require(
        test.fixture.inputs.focused(test.records.at({DatePickerMode::Date, CivilDate{2026, 11, 8}}).input.component),
        "rich Tab followed old declaration order");
    test.key(Key::tab, KeyModifier::shift);
    require(test.fixture.inputs.focused(input.component), "rich Shift-Tab did not follow reordered raw dates");
    require(test.records.at({DatePickerMode::Date, CivilDate{2026, 12, 6}}).reference.focus(), "last rich date focus");
    test.key(Key::tab);
    require(test.fixture.inputs.focused(test.footer_input.component), "footer was skipped by reordered cells");
    test.key(Key::tab);
    require(test.fixture.inputs.focused(test.sibling_input.component),
            "external sibling order was changed by grid reorder");
    require(cell.reference.focus(), "retained date ref after Tab");
    test.fixture.services.pointer().dispatch(
        {PointerIdentity::mouse(), PointerAction::down, PointerButton::primary, bounds.x + .25F, bounds.y + .25F});
    test.fixture.services.pointer().dispatch(
        {PointerIdentity::mouse(), PointerAction::up, PointerButton::primary, bounds.x + .25F, bounds.y + .25F});
    test.sync();
    require(test.requested == CivilDate{2026, 11, 7} && test.changes == 1,
            "retained date hit selected another raw CivilDate");
}

void modes_boundaries_and_text_only_updates() {
    std::cerr << "case: modes_boundaries_and_text_only_updates" << std::endl;
    RichFixture test;
    Signal<DatePickerMode> mode{DatePickerMode::Date};
    Signal<CivilDate> panel{CivilDate{2026, 11, 1}};
    Signal<PickerLocale> locale{PickerLocale{}};
    test.fixture.services.mount(Content{[&] {
        DatePicker(
            DatePickerProps{}.defaultOpen().mode(mode).panelDate(panel).locale(locale).cellRender(test.renderer()),
            test.footer_slot());
    }});
    test.sync();
    const auto id = test.fixture.services.date_picker().mounted().front();
    auto before = test.fixture.services.date_picker().snapshot(id);
    auto changed = locale.get();
    changed.year_offset = 543;
    changed.first_week_contains = 4;
    changed.months[10] = u8"November custom";
    locale.set(changed);
    test.settle(id);
    require(test.fixture.services.date_picker().snapshot(id).cells == before.cells && test.factories == 42,
            "text/year/week-number changes remounted identical date keys");
    for (const auto next : {DatePickerMode::Week, DatePickerMode::Month, DatePickerMode::Quarter, DatePickerMode::Year,
                            DatePickerMode::Decade, DatePickerMode::Date}) {
        before = test.fixture.services.date_picker().snapshot(id);
        mode.set(next);
        const auto queued = test.fixture.services.date_picker().snapshot(id);
        require(queued.cells == before.cells && queued.mode == before.mode,
                "live mode indexed an older published grid");
        test.settle(id);
        const auto after = test.fixture.services.date_picker().snapshot(id);
        static_cast<void>(keys(after));
        const auto expected = next == DatePickerMode::Date || next == DatePickerMode::Week ? 42U
                              : next == DatePickerMode::Quarter                            ? 4U
                                                                                           : 12U;
        require(after.cells.size() == expected && after.mode == next, "panel mode count/key contract");
        for (auto old : before.cells) {
            require(!test.fixture.services.components().contains(old), "different panel mode reused old cell identity");
        }
    }
    for (const auto boundary : {CivilDate{1, 1, 1}, CivilDate{9999, 12, 31}}) {
        panel.set(boundary);
        for (const auto next : {DatePickerMode::Date, DatePickerMode::Week, DatePickerMode::Month,
                                DatePickerMode::Quarter, DatePickerMode::Year, DatePickerMode::Decade}) {
            mode.set(next);
            test.settle(id);
            const auto snapshot = test.fixture.services.date_picker().snapshot(id);
            static_cast<void>(keys(snapshot));
            for (const auto& info : snapshot.cell_info) {
                require(info.date || info.disabled, "out-of-domain structural cell was interactive");
            }
        }
    }
    require(test.footer_mounts == 1, "boundary/mode changes remounted rich footer");
}

void superseding_factory_and_slot_cancel_temporaries() {
    std::cerr << "case: superseding_factory_and_slot_cancel_temporaries" << std::endl;
    for (const bool from_slot : {false, true}) {
        RichFixture test;
        Signal<LocaleId> language{locales::en_US};
        bool armed = false;
        test.hook = [&](CellKey key, bool slot) {
            if (armed && slot == from_slot && key.second == CivilDate{2026, 10, 26}) {
                armed = false;
                language.set(locales::en_US);
            }
        };
        test.fixture.services.mount(Content{[&] {
            ConfigProvider(
                ConfigProviderProps{}.locale(language), ConfigProviderContent{[&] {
                    DatePicker(
                        DatePickerProps{}.defaultOpen().defaultPanelDate({2026, 11, 1}).cellRender(test.renderer()),
                        test.footer_slot());
                }});
            test.mount_sibling();
        }});
        test.sync();
        const auto id = test.fixture.services.date_picker().mounted().front();
        const auto original = test.fixture.services.date_picker().snapshot(id);
        const auto inputs = test.fixture.inputs.mounted_inputs().size();
        armed = true;
        language.set(locales::zh_CN);
        test.settle(id);
        require(test.fixture.services.date_picker().snapshot(id).cells == original.cells &&
                    test.fixture.inputs.mounted_inputs().size() == inputs && test.footer_mounts == 1 &&
                    test.sibling.focus(),
                "superseded renderer/slot leaked a temporary, remounted old keys or retired the owner");
        if (from_slot) {
            require(!test.records.at({DatePickerMode::Date, CivilDate{2026, 10, 26}}).reference.focus(),
                    "cancelled staged Input ref survived");
        }
    }
}

void metadata_and_predicate_reentry_use_latest_revision() {
    std::cerr << "case: metadata_and_predicate_reentry_use_latest_revision" << std::endl;
    for (const bool last : {false, true}) {
        RichFixture test;
        Signal<LocaleId> language{locales::en_US};
        Signal<DatePickerValue> value{DatePickerValue{CivilDate{2026, 11, 7}}};
        test.fixture.services.mount(Content{[&] {
            ConfigProvider(ConfigProviderProps{}.locale(language), ConfigProviderContent{[&] {
                               DatePicker(DatePickerProps{}
                                              .value(value)
                                              .panelDate(CivilDate{2026, 11, 1})
                                              .defaultOpen()
                                              .cellRender(test.renderer()));
                           }});
        }});
        test.sync();
        const auto id = test.fixture.services.date_picker().mounted().front();
        const auto target = last ? CivilDate{2026, 12, 12} : CivilDate{2026, 11, 1};
        Scope observers;
        bool armed = false;
        const auto info = test.records.at({DatePickerMode::Date, target}).info;
        static_cast<void>(connect_binding(observers, bind([info] { return detail::read_prop(info); }),
                                          [&](const DatePickerCellInfo& meta) {
                                              if (armed && meta.selected) {
                                                  armed = false;
                                                  language.set(locales::zh_CN);
                                              }
                                          }));
        armed = true;
        value.set(target);
        test.settle(id);
        const auto snapshot = test.fixture.services.date_picker().snapshot(id);
        require(snapshot.cells.size() == 42 && !snapshot.grid_pending && snapshot.value == value.get() &&
                    snapshot.cell_info.front().date == CivilDate{2026, 10, 26},
                "first/last metadata callback published an obsolete grid");
        static_cast<void>(keys(snapshot));
    }
    RichFixture test;
    Signal<PickerLocale> locale{PickerLocale{}};
    Signal<DatePickerMode> mode{DatePickerMode::Date};
    bool armed = false;
    test.fixture.services.mount(Content{[&] {
        DatePicker(DatePickerProps{}
                       .locale(locale)
                       .mode(mode)
                       .defaultOpen()
                       .defaultPanelDate({2026, 11, 1})
                       .cellRender(test.renderer())
                       .disabledDate([&](CivilDate) {
                           if (armed) {
                               armed = false;
                               mode.set(DatePickerMode::Quarter);
                           }
                           return false;
                       }));
    }});
    test.sync();
    const auto id = test.fixture.services.date_picker().mounted().front();
    armed = true;
    auto changed = locale.get();
    changed.months[0] = u8"January custom";
    locale.set(changed);
    test.settle(id);
    require(test.fixture.services.date_picker().snapshot(id).mode == DatePickerMode::Quarter &&
                test.fixture.services.date_picker().snapshot(id).cells.size() == 4,
            "predicate reentry retained obsolete mode/count or retired a healthy picker");
}

void captured_scope_survives_another_provider_mount() {
    std::cerr << "case: captured_scope_survives_another_provider_mount" << std::endl;
    for (const bool other_window : {false, true}) {
        RichFixture test;
        Signal<LocaleId> language{locales::en_US};
        auto theme = std::make_unique<ThemeConfig>();
        theme->seed.color_primary = Color::rgba8(190, 20, 40);
        const auto original = [&] {
            ConfigProvider(
                ConfigProviderProps{}.locale(language).theme(*theme), ConfigProviderContent{[&] {
                    DatePicker(
                        DatePickerProps{}.defaultOpen().defaultPanelDate({2026, 11, 1}).cellRender(test.renderer()));
                }});
        };
        const auto foreign = [&] {
            auto foreign_theme = std::make_unique<ThemeConfig>();
            foreign_theme->seed.color_primary = Color::rgba8(20, 190, 40);
            ConfigProvider(ConfigProviderProps{}.locale(locales::en_US).theme(*foreign_theme),
                           ConfigProviderContent{[&] {
                               language.set(locales::zh_CN);
                               Text(u8"other provider");
                           }});
        };
        if (other_window) {
            test.fixture.services.mount(Content{original});
            ryn_test::input_component::Fixture second;
            second.services.mount(Content{foreign});
            second.synchronize();
        } else {
            test.fixture.services.mount(Content{[&] {
                original();
                foreign();
            }});
        }
        const auto id = test.fixture.services.date_picker().mounted().front();
        test.settle(id);
        const auto& new_cell = test.records.at({DatePickerMode::Date, CivilDate{2026, 10, 26}});
        require(new_cell.i18n.locale().effective == locales::zh_CN && new_cell.primary == *theme->seed.color_primary,
                "deferred cell inherited the active foreign Provider/theme instead of its popup owner");
    }
}

void published_failure_and_owner_retirement_cleanup() {
    std::cerr << "case: published_failure_and_owner_retirement_cleanup" << std::endl;
    RichFixture test;
    Signal<LocaleId> language{locales::en_US};
    bool reject_seventh = false;
    test.fixture.services.mount(Content{[&] {
        ConfigProvider(ConfigProviderProps{}.locale(language), ConfigProviderContent{[&] {
                           DatePicker(DatePickerProps{}
                                          .defaultOpen()
                                          .defaultPanelDate({2026, 11, 1})
                                          .cellRender(test.renderer())
                                          .disabledDate([&](CivilDate date) {
                                              return reject_seventh && date == CivilDate{2026, 11, 7};
                                          }));
                       }});
        test.mount_sibling();
    }});
    test.sync();
    const auto id = test.fixture.services.date_picker().mounted().front();
    Scope observers;
    const auto info = test.records.at({DatePickerMode::Date, CivilDate{2026, 11, 7}}).info;
    static_cast<void>(connect_binding(observers, bind([info] { return detail::read_prop(info); }),
                                      [](const DatePickerCellInfo& meta) {
                                          if (meta.disabled) {
                                              throw std::runtime_error("published metadata failure");
                                          }
                                      }));
    reject_seventh = true;
    language.set(locales::zh_CN);
    bool threw = false;
    try {
        test.sync();
    } catch (const std::runtime_error&) {
        threw = true;
    }
    require(threw && !test.fixture.services.components().contains(id) &&
                test.fixture.inputs.mounted_inputs().size() == 1 && test.sibling.focus(),
            "published failure left child identities/editors or retired the unrelated sibling");
    for (const auto& [key, record] : test.records) {
        static_cast<void>(key);
        require(!record.reference.focus() && !record.i18n.active(),
                "retired published/created cell retained an active ref");
    }
    test.sync();
    static_cast<void>(test.fixture.frames.consume_request());
    language.set(locales::en_US);
    require(!test.fixture.frames.pending(), "late locale callback survived failed owner retirement");
}

void staged_exception_and_callback_retirement_are_safe() {
    std::cerr << "case: staged_exception_and_callback_retirement_are_safe" << std::endl;
    for (int action = 0; action < 4; ++action) {
        std::cerr << "  action=" << action << std::endl;
        RichFixture test;
        Signal<PickerLocale> locale{PickerLocale{}};
        Signal<DatePickerValue> value{DatePickerValue{CivilDate{2026, 11, 1}}};
        runtime::ComponentId owner;
        bool armed = false;
        test.hook = [&](CellKey key, bool slot) {
            if (armed && key.second == CivilDate{2026, 10, 26}) {
                if (action == 0 && slot) {
                    armed = false;
                    throw std::runtime_error("staged rich slot failure");
                }
                if (action == 1 && !slot) {
                    armed = false;
                    test.fixture.services.destroy(owner);
                }
            }
        };
        test.fixture.services.mount(Content{[&] {
            DatePicker(DatePickerProps{}
                           .locale(locale)
                           .value(value)
                           .panelDate(CivilDate{2026, 11, 1})
                           .defaultOpen()
                           .cellRender(test.renderer())
                           .disabledDate([&](CivilDate) {
                               if (armed && action == 3) {
                                   armed = false;
                                   test.fixture.services.destroy(owner);
                               }
                               return false;
                           }));
            test.mount_sibling();
        }});
        test.sync();
        owner = test.fixture.services.date_picker().mounted().front();
        Scope observers;
        if (action == 2) {
            const auto cell = test.records.at({DatePickerMode::Date, CivilDate{2026, 11, 7}}).cell;
            const auto info = test.records.at({DatePickerMode::Date, CivilDate{2026, 11, 7}}).info;
            static_cast<void>(connect_binding(observers, bind([info] { return detail::read_prop(info); }),
                                              [&, cell](const DatePickerCellInfo& meta) {
                                                  if (armed && meta.selected) {
                                                      armed = false;
                                                      test.fixture.services.destroy(cell);
                                                  }
                                              }));
        }
        armed = true;
        bool threw = false;
        try {
            if (action == 2) {
                value.set(CivilDate{2026, 11, 7});
            } else {
                auto changed = LocaleCatalog::builtin().pack(locales::zh_CN).data().picker;
                if (action == 3) {
                    changed = locale.get();
                    changed.months[0] = u8"January replacement";
                }
                locale.set(changed);
            }
            test.settle(owner);
        } catch (const std::runtime_error&) {
            threw = true;
        }
        require(threw == (action == 0) && !test.fixture.services.components().contains(owner) &&
                    test.fixture.inputs.mounted_inputs().size() == 1 && test.sibling.focus(),
                "staged failure or callback retirement leaked editors/IDs, swallowed a real exception or failed "
                "supersession");
        for (const auto& [key, record] : test.records) {
            static_cast<void>(key);
            require(!record.reference.focus() && !record.i18n.active(),
                    "callback-retired cell retained active input/i18n handles");
        }
    }
}

void calendar_last_metadata_supersession_preserves_published_keys() {
    std::cerr << "case: calendar_last_metadata_supersession_preserves_published_keys" << std::endl;
    ryn_test::input_component::Fixture fixture;
    Signal<LocaleId> language{locales::en_US};
    std::optional<Binding<CalendarCellInfo>> last;
    int factories = 0;
    int changes = 0;
    CalendarRef reference;
    InputRef editor;
    fixture.services.mount(Content{[&] {
        ConfigProvider(ConfigProviderProps{}.locale(language), ConfigProviderContent{[&] {
                           Calendar(CalendarProps{}
                                        .ref(reference)
                                        .defaultValue(CivilDate{2026, 11, 7})
                                        .onChange([&](auto) { ++changes; })
                                        .cellRender(CalendarCellRenderer{[&](Binding<CalendarCellInfo> meta) {
                                            ++factories;
                                            const auto date = meta.get().date;
                                            if (date == CivilDate{2026, 12, 6}) {
                                                last = meta;
                                            }
                                            return CalendarCell{[&, date] {
                                                if (date == CivilDate{2026, 11, 7}) {
                                                    Input(InputProps{}.ref(editor).defaultValue(u8"Calendar draft"));
                                                }
                                            }};
                                        }}));
                       }});
    }});
    require(fixture.services.layout_and_synchronize({900, 900}, {0, 0, 900, 900}), "Calendar initial layout");
    const auto id = fixture.services.calendar().mounted().front();
    const auto before = fixture.services.calendar().snapshot(id);
    const auto components = fixture.services.components().component_count();
    require(last.has_value() && editor.focus(), "Calendar last-row observer setup");
    const auto week = last->get().week;
    bool armed = true;
    Scope observer;
    static_cast<void>(connect_binding(observer, *last, [&](const CalendarCellInfo& info) {
        if (armed && info.week != week) {
            armed = false;
            language.set(locales::en_US);
        }
    }));
    language.set(locales::zh_CN);
    for (int pass = 0; pass < 3; ++pass) {
        require(fixture.services.layout_and_synchronize({900, 900}, {0, 0, 900, 900}), "Calendar supersession layout");
    }
    const auto after = fixture.services.calendar().snapshot(id);
    require(!armed && language.get() == locales::en_US && after.components == before.components &&
                fixture.services.components().component_count() == components && factories == 48 && changes == 0 &&
                editor.focus(),
            "Calendar last metadata callback published an obsolete row list, leaked temporaries, or remounted retained "
            "input");
    fixture.services.dispose();
    language.set(locales::zh_CN);
    require(!reference.focus() && !editor.focus(), "Calendar late ref or observer survived unmount");
}

} // namespace

void i18n_pickers_retained_cases() {
    std::cerr << "case: i18n_pickers_retained_cases" << std::endl;
    keyed_editing_geometry_and_tab_order();
    modes_boundaries_and_text_only_updates();
    superseding_factory_and_slot_cancel_temporaries();
    metadata_and_predicate_reentry_use_latest_revision();
    captured_scope_survives_another_provider_mount();
    published_failure_and_owner_retirement_cleanup();
    staged_exception_and_callback_retirement_are_safe();
    calendar_last_metadata_supersession_preserves_published_keys();
}
