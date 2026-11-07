#include "component/auto_complete_component.hpp"
#include "component/avatar_component.hpp"
#include "component/card_component.hpp"
#include "component/date_picker_component.hpp"
#include "component/form_component.hpp"
#include "component/feedback_component.hpp"
#include "component/select_component.hpp"
#include "component/segmented_component.hpp"
#include "component/selection_component.hpp"
#include "component/skeleton_component.hpp"
#include "component/table_component.hpp"
#include "component/time_picker_component.hpp"
#include "component/tree_select_component.hpp"
#include "runtime/prop_connection.hpp"
#include "support/config_provider_consumers.hpp"

#include <array>
#include <utility>
#include <vector>

namespace {
using namespace ryn;
using namespace ryn::input;
using namespace ryn_test::i18n_components;
using ryn_test::config_consumers::ConsumerFixture;

detail::MountedInputComponent editor_in(ConsumerFixture& fixture, ComponentId owner) {
    for (const auto& input : fixture.inputs.mounted_inputs()) {
        if (descendant(fixture, input.component, owner)) {
            return input;
        }
    }
    throw std::runtime_error("actual compound editor is missing");
}

void sync_large(ConsumerFixture& fixture) {
    require(fixture.services.layout_and_synchronize({1000, 1400}, {0, 0, 1000, 1400}), "consumer layout failed");
}
} // namespace

void config_provider_form_sources() {
    ConsumerFixture fixture;
    Signal<bool> inherited_disabled{true};
    Signal<bool> form_gate{false};
    Signal<ControlSize> size{ControlSize::Large};
    Signal<LocaleId> language{locales::en_US};
    FormField<String> inherited{u8"inherited"};
    FormField<String> explicit_field{u8"abcdef"};
    FormField<String> compact_field{u8"compact"};
    FormRef form;
    InputRef enabled;
    int validators{};
    int mounts{};
    explicit_field.validator([&](String) {
        ++validators;
        return String{};
    });
    fixture.services.mount(Content{[&] {
        ConfigProvider(
            ConfigProviderProps{}
                .disabled(inherited_disabled)
                .size(size)
                .locale(language)
                .direction(TextDirection::RightToLeft),
            ConfigProviderContent{[&] {
                Form(FormProps{}.ref(form).size(ControlSize::Small).disabled(form_gate), FormContent{[&] {
                         ++mounts;
                         ConfigProvider(
                             ConfigProviderProps{}.disabled(false), ConfigProviderContent{[&] {
                                 FormItem(FormItemProps{}.field(u8"inherited", inherited),
                                          FormItemContent{[] { Input(InputProps{}); }});
                                 FormItem(FormItemProps{}.field(u8"explicit", explicit_field), FormItemContent{[&] {
                                              Input(InputProps{}
                                                        .size(ControlSize::Middle)
                                                        .direction(TextDirection::Auto)
                                                        .disabled(false)
                                                        .ref(enabled));
                                          }});
                                 FormItem(FormItemProps{}.field(u8"compact", compact_field), FormItemContent{[] {
                                              SpaceCompact(SpaceCompactProps{}.size(ControlSize::Large),
                                                           SpaceCompactContent{[] { Input(InputProps{}); }});
                                          }});
                             }});
                     }});
            }});
    }});
    sync_large(fixture);
    const auto inputs = std::vector<detail::MountedInputComponent>{fixture.inputs.mounted_inputs().begin(),
                                                                   fixture.inputs.mounted_inputs().end()};
    require(inputs.size() == 3 && fixture.inputs.size(inputs[0].component) == ControlSize::Small &&
                fixture.inputs.size(inputs[1].component) == ControlSize::Middle &&
                fixture.inputs.size(inputs[2].component) == ControlSize::Large && enabled.focus(),
            "Form/Compact/explicit size priority or nested false failed");
    require(std::as_const(fixture.scene).text_state(fixture.inputs.text_scene(inputs[1].component)).direction() ==
                TextDirection::Auto,
            "Form overwrote explicit Auto");
    require(form.validate().has_value() && validators == 1, "consumer Form validator setup failed");
    require(enabled.select(5, 1), "Form reverse selection failed");
    const auto selection = fixture.inputs.editors().require(inputs[1].editor).selection();
    language.set(locales::zh_CN);
    size.set(ControlSize::Middle);
    inherited_disabled.set(false);
    inherited_disabled.set(true);
    sync_large(fixture);
    require(validators == 1 && mounts == 1 && fixture.inputs.mounted_inputs()[1].editor == inputs[1].editor &&
                fixture.inputs.editors().require(inputs[1].editor).selection() == selection && enabled.focus(),
            "Provider presentation reran validator/slot/editor or lifted nested false incorrectly");
    form_gate.set(true);
    require(!enabled.focus(), "explicit Form true was released by member false/nested Provider false");
    form_gate.set(false);
    require(enabled.focus() && fixture.inputs.editors().require(inputs[1].editor).value() == "abcdef",
            "Form gate recovery reset the business value");

    ConsumerFixture group_fixture;
    Signal<bool> group_gate{false};
    group_fixture.services.mount(Content{[&] {
        ConfigProvider(
            ConfigProviderProps{}.disabled(true), ConfigProviderContent{[&] {
                CheckboxGroup(CheckboxGroupProps{}.disabled(group_gate), CheckboxGroupContent{[] {
                                  ConfigProvider(ConfigProviderProps{}.disabled(false), ConfigProviderContent{[] {
                                                     Checkbox(CheckboxProps{}.value(u8"one"));
                                                 }});
                              }});
                RadioGroup(RadioGroupProps{}, RadioGroupContent{[] {
                               ConfigProvider(ConfigProviderProps{}.disabled(false),
                                              ConfigProviderContent{[] { Radio(RadioProps{}.value(u8"one")); }});
                           }});
            }});
    }});
    synchronize(group_fixture);
    const auto members = group_fixture.selections.mounted();
    require(!group_fixture.selections.snapshot(members[0].component).disabled &&
                !group_fixture.selections.snapshot(members[1].component).disabled,
            "unset Group turned inherited Provider true into local OR authority");
    group_gate.set(true);
    require(group_fixture.selections.snapshot(members[0].component).disabled,
            "explicit Group true did not retain native OR authority");

    ConsumerFixture fallback;
    FormField<String> scoped_field;
    Signal<ControlSize> inner_size{ControlSize::Small};
    Signal<TextDirection> inner_direction{TextDirection::Auto};
    fallback.services.mount(Content{[&] {
        ConfigProvider(ConfigProviderProps{}.size(ControlSize::Large).direction(TextDirection::RightToLeft),
                       ConfigProviderContent{[&] {
                           Form(FormProps{}, FormContent{[&] {
                                    FormItem(FormItemProps{}.field(u8"scope", scoped_field), FormItemContent{[&] {
                                                 ConfigProvider(
                                                     ConfigProviderProps{}.size(inner_size).direction(inner_direction),
                                                     ConfigProviderContent{[] { Input(InputProps{}); }});
                                             }});
                                }});
                       }});
    }});
    synchronize(fallback);
    const auto scoped = fallback.inputs.mounted_inputs().front();
    require(fallback.inputs.size(scoped.component) == ControlSize::Small &&
                std::as_const(fallback.scene).text_state(fallback.inputs.text_scene(scoped.component)).direction() ==
                    TextDirection::Auto,
            "unset Form promoted inherited outer defaults above a nearer Provider");
    inner_size.set(ControlSize::Middle);
    inner_direction.set(TextDirection::LeftToRight);
    require(fallback.inputs.size(scoped.component) == ControlSize::Middle &&
                std::as_const(fallback.scene).text_state(fallback.inputs.text_scene(scoped.component)).direction() ==
                    TextDirection::LeftToRight,
            "Form consumer did not stay connected to its own nearest scope");

    ConsumerFixture legacy;
    FormField<String> old_field;
    legacy.services.mount(Content{[&] {
        Form(FormProps{}, FormContent{[&] {
                 FormItem(FormItemProps{}.field(u8"old", old_field), FormItemContent{[] { Input(InputProps{}); }});
             }});
    }});
    synchronize(legacy);
    const auto old_input = legacy.inputs.mounted_inputs().front();
    require(std::as_const(legacy.scene).text_state(legacy.inputs.text_scene(old_input.component)).direction() ==
                TextDirection::Auto,
            "no-Provider Form changed its old Auto editor fallback");
}

void config_provider_compound_consumers() {
    ConsumerFixture fixture;
    Signal<ControlSize> size{ControlSize::Small};
    Signal<bool> disabled{false};
    Signal<TextDirection> direction{TextDirection::RightToLeft};
    InputRef select_ref;
    InputRef date_ref;
    InputRef time_ref;
    fixture.services.mount(Content{[&] {
        ConfigProvider(ConfigProviderProps{}.size(size).disabled(disabled).direction(direction),
                       ConfigProviderContent{[&] {
                           Select(SelectProps{}.ref(select_ref));
                           AutoComplete(AutoCompleteProps{});
                           TreeSelect(TreeSelectProps{});
                           DatePicker(DatePickerProps{}.ref(date_ref));
                           TimePicker(TimePickerProps{}.ref(time_ref));
                           DateRangePicker(DateRangePickerProps{});
                           TimeRangePicker(TimeRangePickerProps{});
                           Search(SearchProps{});
                           OTP(OTPProps{}.length(2));
                           InputNumber(InputNumberProps{}.defaultValue(Decimal{"9007199254740993.125"}));
                       }});
    }});
    sync_large(fixture);
    const std::array owners{
        fixture.services.select().mounted().front(),      fixture.services.auto_complete().mounted().front(),
        fixture.services.tree_select().mounted().front(), fixture.services.date_picker().mounted().front(),
        fixture.services.time_picker().mounted().front(),
    };
    std::vector<detail::MountedInputComponent> editors;
    for (const auto owner : owners) {
        const auto input = editor_in(fixture, owner);
        editors.push_back(input);
        require(fixture.inputs.size(input.component) == ControlSize::Small &&
                    std::as_const(fixture.scene).text_state(fixture.inputs.text_scene(input.component)).direction() ==
                        TextDirection::RightToLeft,
                "compound selector sealed default size or direction");
    }
    size.set(ControlSize::Large);
    direction.set(TextDirection::LeftToRight);
    sync_large(fixture);
    for (const auto& input : editors) {
        require(fixture.inputs.size(input.component) == ControlSize::Large &&
                    std::as_const(fixture.scene).text_state(fixture.inputs.text_scene(input.component)).direction() ==
                        TextDirection::LeftToRight &&
                    (fixture.inputs.editors().find(input.editor) != nullptr),
                "dynamic compound defaults replaced editor or failed to reach it");
    }
    disabled.set(true);
    require(!select_ref.focus() && !date_ref.focus() && !time_ref.focus(),
            "compound Provider disable retained focus eligibility");
    disabled.set(false);
    require(select_ref.focus() && date_ref.focus() && time_ref.focus(), "compound disable recovery lost refs");
}

void config_provider_popup_and_service() {
    ConsumerFixture fixture;
    AppRef captured;
    Signal<ControlSize> size{ControlSize::Small};
    Signal<TextDirection> direction{TextDirection::RightToLeft};
    Signal<bool> disabled{false};
    Signal<std::vector<SelectOption>> options{std::vector<SelectOption>{{SelectValue{u8"a"}, u8"a"}}};
    std::vector<detail::MountedInputComponent> rows;
    int factories{};
    fixture.services.mount(Content{[&] {
        App(AppProps{}, AppContent{[&] {
                ConfigProvider(ConfigProviderProps{}.size(size).direction(direction).disabled(disabled),
                               ConfigProviderContent{[&] {
                                   captured = useApp();
                                   Select(SelectProps{}.options(options).defaultOpen(true).optionRender(
                                       SelectOptionRenderer{[&](SelectRenderInfo) {
                                           ++factories;
                                           Input(InputProps{}.defaultValue(u8"row draft"));
                                           rows.push_back(fixture.inputs.mounted_inputs().back());
                                       }}));
                                   DatePicker(DatePickerProps{}.defaultOpen(true));
                               }});
            }});
    }});
    sync_large(fixture);
    const auto select = fixture.services.select().mounted().front();
    const auto picker = fixture.services.date_picker().mounted().front();
    require(!rows.empty(), "Select did not mount a real rich row");
    const auto first = rows.front();
    const auto count = factories;
    size.set(ControlSize::Large);
    direction.set(TextDirection::LeftToRight);
    sync_large(fixture);
    require(factories == count && (fixture.inputs.editors().find(first.editor) != nullptr) &&
                fixture.inputs.size(first.component) == ControlSize::Large,
            "open popup reran rich factory or failed to update retained consumer");
    options.set(std::vector<SelectOption>{{SelectValue{u8"a"}, u8"a"}, {SelectValue{u8"b"}, u8"b"}});
    sync_large(fixture);
    require(factories == count + 1 && (fixture.inputs.editors().find(first.editor) != nullptr) &&
                fixture.inputs.size(rows.back().component) == ControlSize::Large,
            "same-key reconciliation lost identity or late row used a different mount scope");
    ComponentId late;
    const auto footer = fixture.services.date_picker().snapshot(picker).footer;
    fixture.services.append_slot(footer, Content{[&] {
                                     Input(InputProps{});
                                     late = fixture.inputs.mounted_inputs().back().component;
                                 }});
    require(fixture.inputs.size(late) == ControlSize::Large &&
                std::as_const(fixture.scene).text_state(fixture.inputs.text_scene(late)).direction() ==
                    TextDirection::LeftToRight,
            "late Picker subtree missed its captured live config");
    disabled.set(true);
    InputRef service_ref;
    const auto handle = captured.modal().confirm(ModalProps{}.title(u8"Service"),
                                                 ModalContent{[&] { Input(InputProps{}.ref(service_ref)); }});
    sync_large(fixture);
    const auto modal = fixture.services.modal().mounted().back();
    const auto snapshot = fixture.services.modal().snapshot(modal);
    const auto service_input = fixture.inputs.mounted_inputs().back();
    require(fixture.inputs.size(service_input.component) == ControlSize::Large && !service_ref.focus(),
            "late Modal body did not read its caller's current size/disabled");
    require(!fixture.buttons.snapshot(snapshot.ok).disabled && !fixture.buttons.snapshot(snapshot.cancel).disabled,
            "Provider disabled captured intrinsic Modal footer actions");
    disabled.set(false);
    require(service_ref.focus() && fixture.services.components().contains(select),
            "service live config or prior retained selector was lost");
    require(handle.destroy(), "service cleanup failed");
}

void config_provider_collections_and_idle() {
    ConsumerFixture fixture;
    Signal<ControlSize> size{ControlSize::Large};
    Signal<bool> segmented_disabled{true};
    Signal<bool> local_segmented_disabled{true};
    Signal<int> revision{0};
    Signal<LocaleId> language{locales::en_US};

    struct Row final {
        String key;
        bool operator==(const Row&) const = default;
    };

    int factories{};
    fixture.services.mount(Content{[&] {
        ConfigProvider(ConfigProviderProps{}.locale(language).size(bind([&] {
            static_cast<void>(revision.get());
            return size.get();
        })),
                       ConfigProviderContent{[&] {
                           Table(TableProps<Row>{}
                                     .rowKey([](const Row& row) { return row.key; })
                                     .dataSource(std::vector<Row>{{u8"one"}})
                                     .columns({TableColumn<Row>{u8"value", u8"Value", [&](TableCell<Row>) {
                                                                    ++factories;
                                                                    Input(InputProps{}.defaultValue(u8"dirty"));
                                                                }}}));
                           Avatar(AvatarProps{}.text(u8"A"));
                           Avatar(AvatarProps{}.text(u8"B").size(AvatarSize::Medium));
                           SkeletonButton(SkeletonButtonProps{});
                           Card(CardProps{}.title(u8"Card"), CardContent{[] { Text(u8"Body"); }});
                           ConfigProvider(ConfigProviderProps{}.disabled(segmented_disabled),
                                          ConfigProviderContent{[&] {
                                              Segmented(SegmentedProps{}, {{u8"a", u8"A"}, {u8"b", u8"B"}});
                                              Segmented(SegmentedProps{}.disabled(local_segmented_disabled),
                                                        {{u8"a", u8"A"}, {u8"b", u8"B"}});
                                          }});
                       }});
    }});
    sync_large(fixture);
    const auto segmented = fixture.services.segmented().mounted();
    require(segmented.size() == 2, "Segmented consumers are missing");
    const auto inherited_segment = fixture.services.segmented().snapshot(segmented[0]).items;
    const auto explicit_segment = fixture.services.segmented().snapshot(segmented[1]).items;
    require(fixture.services.interactions().component_active(inherited_segment.front()) &&
                !fixture.services.interactions().component_active(explicit_segment.front()),
            "Provider disabled replaced Segmented's local-only disabled contract");
    segmented_disabled.set(false);
    sync_large(fixture);
    require(fixture.services.interactions().component_active(inherited_segment.front()) &&
                fixture.services.segmented().snapshot(segmented[0]).items == inherited_segment,
            "Segmented did not restore its retained items after enabling");
    segmented_disabled.set(true);
    local_segmented_disabled.set(false);
    sync_large(fixture);
    require(fixture.services.interactions().component_active(inherited_segment.front()) &&
                fixture.services.interactions().component_active(explicit_segment.front()) &&
                fixture.services.segmented().snapshot(segmented[1]).items == explicit_segment,
            "Segmented local reenable failed under a disabled Provider or rebuilt items");
    const auto table = fixture.services.table().mounted().front();
    const auto before = fixture.services.table().snapshot(table);
    const auto input = editor_in(fixture, table);
    const auto cell = before.rows.front().cells.front();
    const auto avatars = fixture.services.avatar().mounted();
    const auto initial_avatar = fixture.services.avatar().snapshot(avatars[0]).viewport.width;
    size.set(ControlSize::Small);
    sync_large(fixture);
    require(fixture.services.table().snapshot(table).rows.front().cells.front() == cell && factories == 1 &&
                fixture.inputs.mounted_inputs().front().editor == input.editor &&
                fixture.inputs.editors().require(input.editor).value() == "dirty" &&
                fixture.services.avatar().snapshot(avatars[0]).viewport.width < initial_avatar &&
                fixture.services.avatar().snapshot(avatars[1]).viewport.width >
                    fixture.services.avatar().snapshot(avatars[0]).viewport.width,
            "Table/Avatar dynamic defaults rebuilt keyed cells or overwrote explicit medium");
    const auto work = fixture.services.table().work(table);
    const auto measures = fixture.nodes.require(fixture.services.components().root(table)).measure_count;
    static_cast<void>(fixture.frames.consume_request());
    const auto requests = fixture.frames.counters().requests;
    for (int i = 1; i <= 100; ++i) {
        revision.set(i);
    }
    require(!fixture.frames.pending() && fixture.frames.counters().requests == requests &&
                fixture.nodes.require(fixture.services.components().root(table)).measure_count == measures &&
                fixture.services.table().work(table).sort_passes == work.sort_passes &&
                fixture.services.table().work(table).copied_records == work.copied_records && factories == 1,
            "equal Provider values added layout/projection/factory/frame work");
    language.set(locales::zh_CN);
    require(factories == 1 && fixture.services.table().work(table).sort_passes == work.sort_passes &&
                fixture.services.table().work(table).copied_records == work.copied_records,
            "locale-only update entered data projection/factories");
}

void config_provider_getter_retirement() {
    for (const bool button : {false, true}) {
        ConsumerFixture fixture;
        Signal<int> revision{0};
        bool retire{};
        int getters{};
        ComponentId owner;
        owner = mount_owned(fixture, Content{[&] {
                                ConfigProvider(ConfigProviderProps{}, ConfigProviderContent{[&] {
                                                   if (button) {
                                                       Button(ButtonProps{}.disabled(bind([&] {
                                                           static_cast<void>(revision.get());
                                                           ++getters;
                                                           if (retire) {
                                                               fixture.services.destroy(owner);
                                                           }
                                                           return false;
                                                       })),
                                                              [] { Text(u8"Retiring button"); });
                                                   } else {
                                                       Input(InputProps{}.size(bind([&] {
                                                           static_cast<void>(revision.get());
                                                           ++getters;
                                                           if (retire) {
                                                               fixture.services.destroy(owner);
                                                           }
                                                           return ControlSize::Middle;
                                                       })));
                                                   }
                                               }});
                            }});
        synchronize(fixture);
        retire = true;
        revision.set(1);
        const auto calls = getters;
        revision.set(2);
        require(!fixture.services.components().contains(owner) && fixture.inputs.mounted_inputs().empty() &&
                    getters == calls,
                "retired explicit getter retained consumer subscriptions");
    }
}

void config_provider_boundary_and_button_rtl() {
    ConsumerFixture fixture;
    Signal<TextDirection> direction{TextDirection::RightToLeft};
    Signal<bool> modal_open{false};
    Signal<bool> spinning{false};
    Signal<bool> disabled{false};
    int outside_clicks{};
    int inside_clicks{};
    ComponentId outside;
    ComponentId inside;
    detail::MountedTextComponent label;
    fixture.services.mount(Content{[&] {
        ConfigProvider(ConfigProviderProps{}.direction(direction).disabled(disabled), ConfigProviderContent{[&] {
                           Button(ButtonProps{}.disabled(false).onClick([&] { ++outside_clicks; }), ButtonContent{[&] {
                                      Text(u8"Outside");
                                      label = fixture.services.text().mounted_texts().back();
                                  }},
                                  ButtonIcon{[] { Icon(IconProps{}.name(IconName::PlusOutlined)); }});
                           outside = fixture.buttons.mounted_buttons().back().component;
                           Modal(ModalProps{}.open(modal_open), ModalContent{[&] {
                                     Button(ButtonProps{}.disabled(false).onClick([&] { ++inside_clicks; }),
                                            [] { Text(u8"Inside"); });
                                     inside = fixture.buttons.mounted_buttons().back().component;
                                 }});
                           Spin(SpinProps{}.fullscreen(true).spinning(spinning));
                       }});
    }});
    synchronize(fixture);
    const auto icon = fixture.buttons.snapshot(outside).icon;
    const auto icon_x = [&] {
        return fixture.nodes.require(fixture.services.components().root(icon)).bounds.x;
    };
    const auto label_x = [&] {
        return fixture.nodes.require(fixture.services.components().root(label.component)).bounds.x;
    };
    require(icon.valid() && icon_x() > label_x(), "logical Button start icon did not mirror in Provider RTL");
    direction.set(TextDirection::LeftToRight);
    synchronize(fixture);
    require(icon_x() < label_x() && fixture.buttons.snapshot(outside).icon == icon,
            "Button RTL update lost logical placement or retained icon identity");
    const auto click = [&](ComponentId id) {
        const auto& node = fixture.nodes.require(fixture.services.components().root(id));
        for (const auto action : {PointerAction::down, PointerAction::up}) {
            fixture.services.pointer().dispatch({PointerIdentity::mouse(), action, PointerButton::primary,
                                                 node.bounds.x + node.translation.x + node.bounds.width / 2,
                                                 node.bounds.y + node.translation.y + node.bounds.height / 2});
        }
    };
    modal_open.set(true);
    disabled.set(true);
    synchronize(fixture);
    click(outside);
    require(outside_clicks == 0, "explicit false released the active Modal input boundary");
    spinning.set(true);
    synchronize(fixture);
    click(inside);
    require(inside_clicks == 0, "explicit false released fullscreen Spin authority");
    spinning.set(false);
    synchronize(fixture);
    click(inside);
    require(inside_clicks == 1, "Modal input authority did not recover after Spin");
}

void config_provider_observer_retirement() {
    for (const bool checkbox : {false, true}) {
        ConsumerFixture fixture;
        Scope observer;
        Signal<bool> disabled{false};
        bool retire{};
        ComponentId owner;
        int callbacks{};
        const auto label = [&] {
            const auto& foreground = require_component_build_context().semantic_foreground();
            require(foreground.has_value(), "real consumer did not expose its semantic label foreground");
            detail::connect_prop(observer, *foreground, [&](runtime::SemanticForeground) {
                ++callbacks;
                if (retire) {
                    fixture.services.destroy(owner);
                }
            });
            Text(u8"Observed label");
        };
        owner = mount_owned(fixture, Content{[&] {
                                ConfigProvider(ConfigProviderProps{}.disabled(disabled), ConfigProviderContent{[&] {
                                                   if (checkbox) {
                                                       Checkbox(CheckboxProps{}, CheckboxLabel{label});
                                                   } else {
                                                       Button(ButtonProps{}, ButtonContent{label});
                                                   }
                                               }});
                            }});
        synchronize(fixture);
        retire = true;
        disabled.set(true);
        const auto count = callbacks;
        disabled.set(false);
        require(!fixture.services.components().contains(owner) && fixture.services.interactions().size() == 0 &&
                    callbacks == count,
                "consumer continued through semantic observer retirement or retained a live publisher");
    }
}

void config_provider_generated_option_sources() {
    ConsumerFixture fixture;
    Signal<bool> disabled{true};
    std::vector<CheckboxOption> checks{
        {String{u8"unset"}, u8"Unset"}, {String{u8"false"}, u8"False", false}, {String{u8"true"}, u8"True", true}};
    std::vector<RadioOption> radios{
        {String{u8"unset"}, u8"Unset"}, {String{u8"false"}, u8"False", false}, {String{u8"true"}, u8"True", true}};
    Signal<std::vector<CheckboxOption>> checkbox_options{checks};
    Signal<std::vector<RadioOption>> radio_options{radios};
    fixture.services.mount(Content{[&] {
        ConfigProvider(ConfigProviderProps{}.disabled(disabled), ConfigProviderContent{[&] {
                           CheckboxGroup(CheckboxGroupProps{}.options(checkbox_options));
                           RadioGroup(RadioGroupProps{}.options(radio_options));
                       }});
    }});
    synchronize(fixture);
    const auto members = std::vector<detail::MountedSelectionComponent>{fixture.selections.mounted().begin(),
                                                                        fixture.selections.mounted().end()};
    require(members.size() == 6, "generated options did not mount real selection controls");
    const auto expect = [&](bool inherited) {
        for (const auto offset : {0U, 3U}) {
            require(fixture.selections.snapshot(members[offset].component).disabled == inherited &&
                        !fixture.selections.snapshot(members[offset + 1].component).disabled &&
                        fixture.selections.snapshot(members[offset + 2].component).disabled,
                    "generated option confused unset/explicit false/explicit true");
        }
    };
    expect(true);
    const auto& enabled = fixture.nodes.require(members[1].node);
    for (const auto action : {PointerAction::down, PointerAction::up}) {
        fixture.services.pointer().dispatch({PointerIdentity::mouse(), action, PointerButton::primary,
                                             enabled.bounds.x + enabled.bounds.width / 2,
                                             enabled.bounds.y + enabled.bounds.height / 2});
    }
    require(fixture.selections.snapshot(members[1].component).checked,
            "explicit false generated option could not consume actual pointer input");
    disabled.set(false);
    expect(false);
    synchronize(fixture);
    static_cast<void>(fixture.frames.consume_request());
    const auto requests = fixture.frames.counters().requests;
    checks[0].disabled = false;
    radios[0].disabled = false;
    checkbox_options.set(checks);
    radio_options.set(radios);
    require(!fixture.frames.pending() && fixture.frames.counters().requests == requests,
            "equal effective option disabled sources scheduled layout or frames");
    disabled.set(true);
    require(!fixture.selections.snapshot(members[0].component).disabled &&
                !fixture.selections.snapshot(members[3].component).disabled,
            "equal bool value suppressed an unset to explicit-false source update");
    checks[0].disabled = SelectionOptionDisabled{};
    radios[0].disabled = SelectionOptionDisabled{};
    checkbox_options.set(checks);
    radio_options.set(radios);
    expect(true);
    require(fixture.selections.snapshot(members[1].component).checked &&
                fixture.selections.mounted().front().component == members.front().component,
            "option source changes reset selection or remounted a stable value");
}
