#include "component/color_picker_component.hpp"
#include "component/statistic_component.hpp"
#include "support/i18n_components.hpp"

#include <limits>
#include <utility>

namespace {
using namespace ryn;
using namespace ryn::input;
using namespace ryn_test::i18n_components;

const LocaleId alternate{"en-XA"};
const LocaleId wide{"en-XB"};

LocaleCatalog catalog(int unused = 0) {
    auto data = std::make_unique<LocaleData>(LocaleCatalog::builtin().pack(locales::en_US).data());
    data->formats.source = alternate;
    data->statistic.decimal_separator = u8",";
    data->statistic.group_separator = u8"_";
    if (unused) {
        data->common.retry = u8"Unused retry";
    }
    auto other = std::make_unique<LocaleData>(*data);
    other->formats.source = wide;
    other->statistic.digits = {u8"０", u8"１", u8"２", u8"３", u8"４", u8"５", u8"６", u8"７", u8"８", u8"９"};
    return LocaleCatalog({{alternate, LocalePack(std::move(*data)), locales::en_US},
                          {wide, LocalePack(std::move(*other)), locales::en_US}});
}

void key(Fixture& fixture, Key value, KeyModifier modifiers = KeyModifier::none) {
    fixture.services.focus().dispatch({value, KeyAction::down, modifiers});
    fixture.services.focus().dispatch({value, KeyAction::up, modifiers});
}

void type(Fixture& fixture, const InputRef& reference, String value) {
    require(reference.focus({InputFocusCursor::All}), "number focus/select");
    require(bool(fixture.inputs.dispatch(TextCommitted{std::move(value), fixture.inputs.sessions().active()})),
            "real numeric text event");
}

String number_text(Fixture& fixture, std::size_t index) {
    const auto editor = fixture.inputs.mounted_inputs()[index].editor;
    return String::from_utf8(fixture.inputs.editors().require(editor).value()).value();
}

bool has_text(Fixture& fixture, ComponentId owner, String value) {
    for (const auto& text : fixture.services.text().mounted_texts()) {
        if (descendant(fixture, text.component, owner) &&
            fixture.scene.text_state(text.scene).content() == value.view()) {
            return true;
        }
    }
    return false;
}

void pure_exact_numbers() {
    const auto& builtin = LocaleCatalog::builtin();
    for (const auto locale : {locales::en_US, locales::zh_CN}) {
        require(formatNumber(builtin, locale, std::numeric_limits<std::uint64_t>::max()) ==
                    String{u8"18,446,744,073,709,551,615"},
                "public uint64 lost precision");
        require(formatNumber(builtin, locale, std::numeric_limits<std::int64_t>::min()) ==
                    String{u8"-9,223,372,036,854,775,808"},
                "public signed integer lost precision");
        require(formatNumber(builtin, locale, Decimal{"9007199254740993.123456789012345678901"}) ==
                    String{u8"9,007,199,254,740,993.123456789012345678901"},
                "public Decimal passed through double");
        require(formatNumber(builtin, locale, -0.0, {.precision = 2}) == String{u8"-0.00"},
                "public negative zero lost its sign");
        require(formatNumber(builtin, locale, Decimal{"-1.239"}, {.precision = 2}) == String{u8"-1.23"},
                "display precision changed Statistic truncation to business rounding");
        require(formatNumber(builtin, locale, Decimal{"1e20"}, {.group_separator = String{}}) ==
                    String{u8"100000000000000000000"},
                "Decimal exponent or explicit empty group separator changed");
    }
    require(formatNumber(catalog(), alternate, Decimal{"12345.678"},
                         {.precision = 2, .decimal_separator = String{u8"."}, .group_separator = String{u8""}}) ==
                String{u8"12345.67"},
            "explicit separator source was guessed from an English default");
}

void actual_provider_numbers_and_sources() {
    Fixture fixture;
    Signal<LocaleId> language{locales::zh_CN};
    Signal<LocaleId> formatting{alternate};
    int changes{};
    I18n captured;
    fixture.services.mount(Content{[&] {
        ConfigProvider(
            ConfigProviderProps{}.locale(language).formatLocale(formatting).catalog(catalog()),
            ConfigProviderContent{[&] {
                captured = useI18n();
                Statistic(StatisticProps{}.value(std::numeric_limits<std::uint64_t>::max()));
                Statistic(StatisticProps{}.value(Decimal{"9007199254740993.1234567890123456789"}).precision(4));
                Statistic(StatisticProps{}.value(-0.0).precision(2));
                Statistic(StatisticProps{}.value(1234.5).decimalSeparator(u8".").groupSeparator(u8""));
                Statistic(StatisticProps{}.value(1234.5).locale(StatisticLocale{}));
                InputNumber(InputNumberProps{}.defaultValue(Decimal{"9007199254740993.125"}).onStringChange([&](auto) {
                    ++changes;
                }));
                InputNumber(InputNumberProps{}.defaultValue(-0.0));
                InputNumber(InputNumberProps{}.defaultValue(12.5).decimalSeparator(String{u8"."}));
                InputNumber(InputNumberProps{}.defaultValue(12.5).formatter(
                    [](String, InputNumberFormatInfo) { return String{u8"literal formatter"}; }));
            }});
    }});
    synchronize(fixture);
    const auto ids = fixture.services.statistic().mounted();
    const auto value_region = fixture.services.statistic().snapshot(ids[0]).value;
    require(fixture.services.statistic().snapshot(ids[0]).formatted_value == String{u8"18_446_744_073_709_551_615"} &&
                fixture.services.statistic().snapshot(ids[1]).formatted_value ==
                    String{u8"9_007_199_254_740_993,1234"} &&
                fixture.services.statistic().snapshot(ids[2]).formatted_value == String{u8"-0,00"} &&
                fixture.services.statistic().snapshot(ids[3]).formatted_value == String{u8"1234.5"} &&
                fixture.services.statistic().snapshot(ids[4]).formatted_value == String{u8"1,234.5"},
            "actual Statistic failed precise formats or explicit Locale/separator priority");
    require(number_text(fixture, 0) == String{u8"9007199254740993,125"} && number_text(fixture, 1) == String{u8"-0"} &&
                number_text(fixture, 2) == String{u8"12.5"} &&
                number_text(fixture, 3) == String{u8"literal formatter"} &&
                captured.formatNumber(Decimal{"1234.5"}) == String{u8"1_234,5"},
            "InputNumber or captured public helper ignored actual formatLocale");
    language.set(locales::en_US);
    synchronize(fixture);
    require(number_text(fixture, 0) == String{u8"9007199254740993,125"} && changes == 0,
            "translation language overwrote independent number formatLocale");
    formatting.set(locales::en_US);
    synchronize(fixture);
    require(number_text(fixture, 0) == String{u8"9007199254740993.125"} && changes == 0 &&
                fixture.services.statistic().snapshot(ids[0]).value == value_region &&
                fixture.services.statistic().snapshot(ids[0]).formatted_value == String{u8"18,446,744,073,709,551,615"},
            "clean live formatting changed identity/value or emitted a callback");
}

void dirty_composition_and_session_switch() {
    Fixture fixture;
    Signal<LocaleId> formatting{alternate};
    InputRef reference;
    InputRef outside;
    int changes{};
    InputNumberStringValue requested;
    fixture.services.mount(Content{[&] {
        ConfigProvider(
            ConfigProviderProps{}.catalog(catalog()).formatLocale(formatting), ConfigProviderContent{[&] {
                InputNumber(
                    InputNumberProps{}.ref(reference).defaultValue(Decimal{"7.5"}).onStringChange([&](auto value) {
                        ++changes;
                        requested = value;
                    }));
                Input(InputProps{}.ref(outside));
            }});
    }});
    synchronize(fixture);
    const auto mounted = fixture.inputs.mounted_inputs()[0];
    type(fixture, reference, u8"9007199254740993,");
    auto& editor = fixture.inputs.editors().require(mounted.editor);
    require(reference.select(16, 2), "reverse numeric selection");
    const auto selection = editor.selection();
    const auto history = editor.history();
    const auto session = fixture.inputs.sessions().active();
    require(bool(fixture.inputs.dispatch(CompositionChanged{String{u8"拼"}, {0, 1}, session})), "real numeric preedit");
    formatting.set(locales::en_US);
    synchronize(fixture);
    require(editor.value() == "9007199254740993," && editor.selection() == selection && editor.composition().active &&
                editor.composition().text == String{u8"拼"}.bytes() &&
                editor.history().undo_count == history.undo_count && changes == 0 &&
                fixture.inputs.sessions().active() == session &&
                fixture.inputs.mounted_inputs()[0].editor == mounted.editor,
            "format update changed draft/reverse selection/composition/history/session/identity");
    editor.cancel_composition();
    type(fixture, reference, u8"9007199254740993,125");
    require(changes == 1 && requested == Decimal{"9007199254740993.125"}, "frozen decimal parser lost exact draft");
    key(fixture, Key::enter);
    require(changes == 1 && number_text(fixture, 0) == String{u8"9007199254740993.125"},
            "commit duplicated change or did not use latest clean format");
    type(fixture, reference, u8"8.");
    formatting.set(alternate);
    require(outside.focus(), "new numeric session blur target");
    synchronize(fixture);
    require(changes == 2 && requested == Decimal{"8"} && number_text(fixture, 0) == String{u8"8"} &&
                fixture.inputs.sessions().active().owner != mounted.editor,
            "blur committed with the wrong session format or retained the old input session");
}

void selected_clean_and_history_survive() {
    Fixture fixture;
    Signal<LocaleId> formatting{locales::en_US};
    InputRef reference;
    InputRef outside;
    int changes{};
    fixture.services.mount(Content{[&] {
        ConfigProvider(ConfigProviderProps{}.catalog(catalog()).formatLocale(formatting), ConfigProviderContent{[&] {
                           InputNumber(
                               InputNumberProps{}.ref(reference).defaultValue(12.5).onChange([&](auto) { ++changes; }));
                           Input(InputProps{}.ref(outside));
                       }});
    }});
    synchronize(fixture);
    type(fixture, reference, u8"13.5");
    key(fixture, Key::enter);
    const auto mounted = fixture.inputs.mounted_inputs()[0];
    auto& editor = fixture.inputs.editors().require(mounted.editor);
    const auto history = editor.history();
    const auto caret = editor.boundaries().byte_to_scalar(editor.selection().caret);
    require(history.undo_count > 0, "numeric fixture did not retain real undo history");
    formatting.set(wide);
    synchronize(fixture);
    require(number_text(fixture, 0) == String{u8"１３,５"} && editor.history().undo_count == history.undo_count &&
                editor.boundaries().byte_to_scalar(editor.selection().caret) == caret && changes == 1,
            "clean presentation update cleared history/caret or emitted a business callback");
    key(fixture, Key::z, KeyModifier::control);
    require(number_text(fixture, 0) == String{u8"12.5"} && changes == 2, "real numeric undo was lost after reformat");
    key(fixture, Key::y, KeyModifier::control);
    require(number_text(fixture, 0) == String{u8"13.5"} && changes == 3, "real numeric redo was lost after reformat");
    key(fixture, Key::enter);
    require(reference.select(10, 3), "non-ASCII reverse numeric selection");
    const auto selection = editor.selection();
    formatting.set(locales::en_US);
    require(editor.selection() == selection && number_text(fixture, 0) == String{u8"１３,５"},
            "selected clean numeric display did not freeze");
    require(outside.focus(), "selected numeric blur target");
    synchronize(fixture);
    require(number_text(fixture, 0) == String{u8"13.5"} && changes == 3,
            "selected clean blur failed to adopt latest format without onChange");
}

void explicit_callbacks_and_separator_session() {
    Fixture fixture;
    Signal<String> separator{String{u8"."}};
    InputRef reference;
    InputRef custom;
    String parser_input;
    String formatter_input;
    int changes{};
    fixture.services.mount(Content{[&] {
        ConfigProvider(
            ConfigProviderProps{}.catalog(catalog()).formatLocale(alternate), ConfigProviderContent{[&] {
                InputNumber(InputNumberProps{}.ref(reference).defaultValue(12.5).decimalSeparator(separator).onChange(
                    [&](auto) { ++changes; }));
                InputNumber(InputNumberProps{}
                                .ref(custom)
                                .defaultValue(7.5)
                                .formatter([&](String raw, InputNumberFormatInfo info) {
                                    formatter_input = raw;
                                    return info.user_typing ? info.input : String{u8"literal"};
                                })
                                .parser([&](String text) {
                                    parser_input = text;
                                    return String{u8"9.25"};
                                }));
            }});
    }});
    synchronize(fixture);
    require(number_text(fixture, 0) == String{u8"12.5"} && formatter_input == String{u8"7.5"},
            "explicit decimal source or canonical formatter input was replaced by inherited locale");
    type(fixture, reference, u8"1.");
    const auto mounted = fixture.inputs.mounted_inputs()[0];
    auto& editor = fixture.inputs.editors().require(mounted.editor);
    const auto history = editor.history();
    separator.set(String{u8","});
    require(number_text(fixture, 0) == String{u8"1."} && editor.history().undo_count == history.undo_count &&
                changes == 0,
            "explicit separator update consumed a dirty session");
    type(fixture, reference, u8"1.25");
    key(fixture, Key::enter);
    require(changes == 1 && number_text(fixture, 0) == String{u8"1,25"},
            "explicit separator snapshot did not survive until commit");
    type(fixture, custom, u8"$9.25");
    require(parser_input == String{u8"$9.25"} && formatter_input == String{u8"9.25"},
            "custom parser/formatter lost precedence or received translated business input");
}

void paused_timer_projects_live_digits() {
    Fixture fixture;
    Signal<LocaleId> formatting{locales::en_US};
    int callbacks{};
    fixture.services.mount(Content{[&] {
        ConfigProvider(ConfigProviderProps{}.catalog(catalog()).formatLocale(formatting), ConfigProviderContent{[&] {
                           Statistic(StatisticProps{}
                                         .timer(StatisticTimer{StatisticTimerType::Countdown,
                                                               Duration::milliseconds(65000), u8"mm:ss"})
                                         .onChange([&](auto) { ++callbacks; }));
                       }});
    }});
    synchronize(fixture);
    const auto id = fixture.services.statistic().mounted()[0];
    formatting.set(wide);
    synchronize(fixture);
    require(fixture.services.statistic().snapshot(id).formatted_value == String{u8"０１:０５"} && callbacks == 0 &&
                !fixture.services.next_frame_deadline(),
            "paused Timer did not project digits without time/callbacks");
}

void color_labels_and_numeric_children_are_retained() {
    Fixture fixture;
    Signal<LocaleId> language{locales::en_US};
    Signal<LocaleId> formatting{locales::en_US};
    int changes{};
    fixture.services.mount(Content{[&] {
        ConfigProvider(ConfigProviderProps{}.locale(language).formatLocale(formatting).catalog(catalog()),
                       ConfigProviderContent{[&] {
                           ColorPicker(ColorPickerProps{}
                                           .defaultValue(PickerColor{ColorRgb{12.5, 34.5, 56.5, .5}})
                                           .defaultOpen(true)
                                           .allowClear(true)
                                           .modes(std::vector{ColorPickerMode::Single, ColorPickerMode::Gradient})
                                           .onChange([&](auto, auto) { ++changes; }));
                           ColorPicker(ColorPickerProps{}.locale(ColorPickerLocale{}));
                       }});
    }});
    synchronize(fixture);
    const auto id = fixture.services.color_picker().mounted()[0];
    const auto literal = fixture.services.color_picker().mounted()[1];
    const auto before = fixture.services.color_picker().snapshot(id);
    const auto inputs = fixture.inputs.mounted_inputs();
    const std::vector children(inputs.begin(), inputs.end());
    require(has_text(fixture, id, u8"Color") && has_text(fixture, id, u8"Clear") && has_text(fixture, id, u8"Single"),
            "actual ColorPicker English labels missing");
    language.set(locales::zh_CN);
    formatting.set(alternate);
    synchronize(fixture);
    const auto after = fixture.services.color_picker().snapshot(id);
    require(has_text(fixture, id, u8"颜色") && has_text(fixture, id, u8"清除") && has_text(fixture, id, u8"单色") &&
                has_text(fixture, id, u8"添加色标") && has_text(fixture, literal, u8"Color") && changes == 0 &&
                after.value == before.value && after.open && after.panel == before.panel &&
                after.fields == before.fields && fixture.inputs.mounted_inputs().size() == children.size(),
            "ColorPicker labels/source/open/value/retained panel changed incorrectly");
    bool localized_channel{};
    for (std::size_t i = 0; i < children.size(); ++i) {
        require(fixture.inputs.mounted_inputs()[i].component == children[i].component &&
                    fixture.inputs.mounted_inputs()[i].editor == children[i].editor,
                "ColorPicker remounted numeric child");
        localized_channel |= descendant(fixture, children[i].component, id) &&
                             number_text(fixture, i).bytes().find(',') != std::string_view::npos;
    }
    require(localized_channel, "ColorPicker real numeric channels bypassed number helper");
}

void callbacks_reenter_or_destroy() {
    for (const bool commit_on_blur : {false, true}) {
        Fixture fixture;
        InputRef reference;
        ComponentId owner;
        int reads{};
        int blur_notifications{};
        owner = mount_owned(fixture, Content{[&] {
                                InputNumber(InputNumberProps{}
                                                .ref(reference)
                                                .defaultValue(1)
                                                .changeOnBlur(bind([&] {
                                                    ++reads;
                                                    fixture.services.destroy(owner);
                                                    return commit_on_blur;
                                                }))
                                                .onBlur([&] { ++blur_notifications; }));
                            }});
        synchronize(fixture);
        require(reference.focus(), "changeOnBlur retirement focus");
        static_cast<void>(reference.blur());
        require(reads == 1 && !reference.bound() && !fixture.services.components().contains(owner) &&
                    blur_notifications == 0,
                "changeOnBlur getter continued the old blur after owner retirement");
    }
    for (const bool destroy : {false, true}) {
        Fixture fixture;
        Signal<LocaleId> formatting{locales::en_US};
        Signal<InputNumberStringValue> authority{InputNumberStringValue{Decimal{"1.5"}}};
        InputRef reference;
        ComponentId id;
        bool armed{};
        int changes{};
        fixture.services.mount(Content{[&] {
            ConfigProvider(ConfigProviderProps{}.catalog(catalog()).formatLocale(formatting),
                           ConfigProviderContent{[&] {
                               InputNumber(InputNumberProps{}
                                               .ref(reference)
                                               .value(authority)
                                               .parser([&](String text) {
                                                   if (std::exchange(armed, false)) {
                                                       if (destroy) {
                                                           fixture.services.destroy(id);
                                                       } else {
                                                           authority.set(InputNumberStringValue{Decimal{"77"}});
                                                           formatting.set(alternate);
                                                       }
                                                   }
                                                   return text;
                                               })
                                               .onStringChange([&](auto) { ++changes; }));
                           }});
        }});
        synchronize(fixture);
        id = fixture.inputs.mounted_inputs()[0].component;
        armed = true;
        type(fixture, reference, u8"55");
        synchronize(fixture);
        require(changes == 0 && (destroy ? !reference.bound() : number_text(fixture, 0) == String{u8"77"}),
                "parser callback published stale parse after reentry/disposal");
    }
    for (const bool destroy : {false, true}) {
        Fixture fixture;
        Signal<InputNumberValue> authority{InputNumberValue{1}};
        InputRef reference;
        ComponentId id;
        bool armed{};
        fixture.services.mount(Content{[&] {
            ConfigProvider(ConfigProviderProps{}, ConfigProviderContent{[&] {
                               InputNumber(InputNumberProps{}.ref(reference).value(authority).formatter(
                                   [&](String text, InputNumberFormatInfo) {
                                       if (std::exchange(armed, false)) {
                                           if (destroy) {
                                               fixture.services.destroy(id);
                                           } else {
                                               authority.set(InputNumberValue{9});
                                           }
                                           return String{u8"stale"};
                                       }
                                       return text;
                                   }));
                           }});
        }});
        synchronize(fixture);
        id = fixture.inputs.mounted_inputs()[0].component;
        armed = true;
        authority.set(InputNumberValue{2});
        synchronize(fixture);
        require(destroy ? !reference.bound() : number_text(fixture, 0) == String{u8"9"},
                "formatter callback published stale display after reentry/disposal");
    }
    Fixture fixture;
    Signal<LocaleId> formatting{locales::en_US};
    bool armed{};
    ComponentId id;
    fixture.services.mount(Content{[&] {
        ConfigProvider(ConfigProviderProps{}.catalog(catalog()).formatLocale(formatting), ConfigProviderContent{[&] {
                           Statistic(StatisticProps{}.value(1234).formatter([&](const StatisticValue&) {
                               if (armed) {
                                   fixture.services.destroy(id);
                               }
                               return String{u8"literal Statistic"};
                           }));
                       }});
    }});
    synchronize(fixture);
    id = fixture.services.statistic().mounted()[0];
    armed = true;
    formatting.set(alternate);
    synchronize(fixture);
    require(!fixture.services.components().contains(id), "Statistic formatter retained retired owner");
}

void equivalent_projection_and_late_updates_are_idle() {
    Fixture fixture;
    Signal<int> revision{0};
    Signal<LocaleId> formatting{alternate};
    InputRef reference;
    I18n captured;
    const auto owner = mount_owned(
        fixture, Content{[&] {
            ConfigProvider(
                ConfigProviderProps{}.catalog(bind([&] { return catalog(revision.get()); })).formatLocale(formatting),
                ConfigProviderContent{[&] {
                    captured = useI18n();
                    Statistic(StatisticProps{}.value(1234.5));
                    InputNumber(InputNumberProps{}.ref(reference).defaultValue(12.5));
                    ColorPicker(ColorPickerProps{});
                }});
        }});
    synchronize(fixture);
    static_cast<void>(fixture.frames.consume_request());
    const auto requests = fixture.frames.counters().requests;
    const auto deadline = fixture.services.next_frame_deadline();
    for (int i = 1; i <= 20; ++i) {
        revision.set(i);
    }
    require(!fixture.frames.pending() && fixture.frames.counters().requests == requests &&
                fixture.services.next_frame_deadline() == deadline,
            "equivalent/unused format projection requested an extra frame");
    fixture.services.destroy(owner);
    synchronize(fixture);
    static_cast<void>(fixture.frames.consume_request());
    const auto retired_requests = fixture.frames.counters().requests;
    formatting.set(locales::zh_CN);
    revision.set(21);
    require(!reference.bound() && !fixture.frames.pending() && fixture.frames.counters().requests == retired_requests &&
                !fixture.services.next_frame_deadline(),
            "retired formatting subscription/session survived");
    rejects([&] { static_cast<void>(captured.formatNumber(std::uint64_t{1})); }, I18nErrorCode::StaleContext);
}

} // namespace

int main() {
    suppress_dialogs();
    try {
        pure_exact_numbers();
        actual_provider_numbers_and_sources();
        dirty_composition_and_session_switch();
        selected_clean_and_history_survive();
        explicit_callbacks_and_separator_session();
        paused_timer_projects_live_digits();
        color_labels_and_numeric_children_are_retained();
        callbacks_reenter_or_destroy();
        equivalent_projection_and_late_updates_are_idle();
        std::cout << "i18n component formatting: 9 groups passed\n";
        return 0;
    } catch (const std::exception& error) {
        std::cerr << error.what() << '\n';
        return 1;
    }
}
