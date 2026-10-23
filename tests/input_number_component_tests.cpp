#include "support/input_fixture.hpp"
#include "theme/input_tokens.hpp"

#include <iostream>
#include <limits>

namespace {
using namespace ryn;
using namespace ryn::input;
using Fixture = ryn_test::input_component::Fixture;

void check(bool value, const char* message) {
    if (!value) {
        throw std::runtime_error(message);
    }
}

void key(Fixture& f, Key value, KeyModifier modifiers = KeyModifier::none) {
    f.services.focus().dispatch({value, KeyAction::down, modifiers});
    f.services.focus().dispatch({value, KeyAction::up, modifiers});
}

void type(Fixture& f, const InputRef& ref, String value) {
    check(ref.focus({InputFocusCursor::All}), "InputNumber focus/select failed");
    if (value.empty()) {
        key(f, Key::backspace);
    } else {
        check(bool(f.inputs.dispatch(TextCommitted{value, f.inputs.sessions().active()})),
              "numeric text commit failed");
    }
}

InteractionId action(Fixture& f, runtime::ComponentId input, std::size_t index) {
    const auto component = f.inputs.number_actions(input)[index];
    for (const auto id : f.services.interactions().declaration_order()) {
        if (f.services.interactions().require(id).component == component) {
            return id;
        }
    }
    throw std::runtime_error("numeric action missing");
}

void pointer(Fixture& f, InteractionId id, PointerAction action) {
    const auto rect = f.nodes.require(f.services.interactions().require(id).node).bounds;
    f.services.pointer().dispatch({PointerIdentity::mouse(), action, PointerButton::primary, rect.x + rect.width / 2,
                                   rect.y + rect.height / 2, 1});
}

void click(Fixture& f, InteractionId id) {
    pointer(f, id, PointerAction::down);
    pointer(f, id, PointerAction::up);
}

void composition_selection_cancellation() {
    enum class Path { ReferenceSelection, FocusCursor, Pointer };
    for (const auto path : {Path::ReferenceSelection, Path::FocusCursor, Path::Pointer}) {
        Fixture f;
        InputRef ref;
        int changes{};
        f.inputs.mount(Content{[&] {
            InputNumber(InputNumberProps{}.defaultValue(.2).min(0).max(1).step(.1).ref(ref).onChange(
                [&](InputNumberValue) { ++changes; }));
        }});
        f.synchronize();
        const auto mounted = f.inputs.mounted_inputs().front();
        const auto up = action(f, mounted.component, 0);
        const auto down = action(f, mounted.component, 1);
        check(ref.focus({InputFocusCursor::All}), "cancel fixture focus failed");
        const auto stamp = f.inputs.sessions().active();
        check(bool(f.inputs.dispatch(CompositionChanged{String{u8"0.9"}, {3, 0}, stamp})), "cancel preedit failed");
        f.synchronize();
        check(!f.services.interactions().require(up).eligible && !f.services.interactions().require(down).eligible &&
                  changes == 0,
              "preedit did not suspend numeric actions");
        if (path == Path::ReferenceSelection) {
            check(ref.select(0, 0), "ref selection did not cancel preedit");
        } else if (path == Path::FocusCursor) {
            check(ref.focus({InputFocusCursor::Start}), "focus cursor did not cancel preedit");
        } else {
            const auto viewport = f.inputs.layout_snapshot(mounted.component).viewport;
            for (const auto action : {PointerAction::down, PointerAction::up}) {
                f.services.pointer().dispatch({PointerIdentity::mouse(), action, PointerButton::primary, viewport.x + 1,
                                               viewport.y + viewport.height / 2, 1});
            }
        }
        f.synchronize();
        check(!f.inputs.editors().require(mounted.editor).composition().active && changes == 0 &&
                  f.inputs.number_value(mounted.component) == .2 &&
                  f.inputs.editors().require(mounted.editor).value() == "0.2",
              "cancel published preedit or changed value");
        const char* message = path == Path::ReferenceSelection ? "ref.select cancellation left actions disabled"
                              : path == Path::FocusCursor      ? "focus cursor cancellation left actions disabled"
                                                               : "pointer cancellation left actions disabled";
        check(f.services.interactions().require(up).eligible && f.services.interactions().require(down).eligible,
              message);
        click(f, up);
        check(changes == 1 && f.inputs.number_value(mounted.component) == .3, "button step after cancel failed");
        key(f, Key::down);
        check(changes == 2 && f.inputs.number_value(mounted.component) == .2, "keyboard step after cancel failed");
    }
}

void precision_step_direction() {
    {
        Fixture initial;
        InputRef ref;
        InputNumberValue changed;
        initial.inputs.mount(Content{[&] {
            InputNumber(InputNumberProps{}.defaultValue(.26).step(.01).precision(1).ref(ref).onChange(
                [&](InputNumberValue next) { changed = next; }));
        }});
        initial.synchronize();
        const auto number = initial.inputs.mounted_inputs().front();
        check(ref.focus(), "default precision fixture focus failed");
        key(initial, Key::down);
        check(changed == .2 && initial.inputs.number_value(number.component) == .2,
              "defaultValue 0.26 Down step increased after rounding");
    }
    Fixture f;
    InputRef ref;
    Signal<InputNumberValue> value{.26};
    Signal<std::optional<unsigned>> precision{2};
    Signal<double> minimum{-1};
    Signal<double> maximum{1};
    int changes{};
    std::vector<InputNumberStepDirection> directions;
    f.inputs.mount(Content{[&] {
        InputNumber(InputNumberProps{}
                        .value(value)
                        .min(minimum)
                        .max(maximum)
                        .step(.01)
                        .precision(precision)
                        .ref(ref)
                        .onChange([&](InputNumberValue next) {
                            ++changes;
                            value.set(next);
                        })
                        .onStep([&](double, InputNumberStepInfo info) { directions.push_back(info.direction); }));
    }});
    f.synchronize();
    const auto mounted = f.inputs.mounted_inputs().front();
    check(ref.focus(), "precision fixture focus failed");
    precision.set(1);
    check(changes == 0 && value.get() == .26, "precision update rewrote controlled authority");
    key(f, Key::down);
    check(value.get() == .2 && changes == 1 && directions.back() == InputNumberStepDirection::Down,
          "native Down step emitted a precision-induced increase");
    value.set(-.26);
    f.synchronize();
    click(f, action(f, mounted.component, 0));
    check(value.get() == -.2 && changes == 2 && directions.back() == InputNumberStepDirection::Up,
          "native Up step emitted a precision-induced decrease");
    value.set(.26);
    minimum.set(.27);
    check(changes == 2 && value.get() == .26, "range update rewrote authority");
    key(f, Key::down);
    check(changes == 2 && value.get() == .26, "minimum quantization reversed Down");
    key(f, Key::up);
    check(changes == 3 && value.get() == .3, "minimum quantization blocked the allowed Up step");
    minimum.set(-1);
    maximum.set(.73);
    value.set(.74);
    key(f, Key::up);
    check(changes == 3 && value.get() == .74, "maximum quantization reversed Up");
    key(f, Key::down);
    check(changes == 4 && value.get() == .7 && directions.back() == InputNumberStepDirection::Down,
          "maximum quantization blocked the allowed Down step");
}

void composition_cancellation_gates() {
    Fixture f;
    InputRef ref;
    Signal<bool> disabled{false};
    Signal<bool> read_only{false};
    int changes{};
    f.inputs.mount(Content{[&] {
        InputNumber(InputNumberProps{}
                        .defaultValue(.2)
                        .min(0)
                        .max(1)
                        .step(.1)
                        .ref(ref)
                        .disabled(disabled)
                        .readOnly(read_only)
                        .onChange([&](InputNumberValue) { ++changes; }));
        Input(InputProps{}.defaultValue(u8"boundary"));
    }});
    f.synchronize();
    const auto mounted = f.inputs.mounted_inputs().front();
    const auto other = f.inputs.mounted_inputs().back();
    const auto up = action(f, mounted.component, 0);
    const auto down = action(f, mounted.component, 1);
    for (int gate = 0; gate < 4; ++gate) {
        check(ref.focus({InputFocusCursor::All}), "gate fixture focus failed");
        const auto count = changes;
        check(bool(f.inputs.dispatch(CompositionChanged{String{u8"0.9"}, {3, 0}, f.inputs.sessions().active()})),
              "gated preedit failed");
        if (gate == 0) {
            disabled.set(true);
        } else if (gate == 1) {
            read_only.set(true);
            check(ref.select(0, 0), "readonly selection should still be available");
        } else if (gate == 2) {
            f.services.components().set_branch_active(mounted.component, false);
            f.services.mark_scene_structure_dirty();
            f.services.focus().synchronize();
        } else {
            f.services.set_input_boundary(other.component);
        }
        f.synchronize();
        if (gate < 2) {
            check(!f.services.interactions().require(up).eligible && !f.services.interactions().require(down).eligible,
                  "composition cancellation lifted disabled/readonly eligibility");
        } else {
            check(!f.services.interactions().component_active(mounted.component) && !ref.select(0, 0) && !ref.focus(),
                  "composition cancellation lifted a logical interaction gate");
        }
        click(f, up);
        key(f, Key::up);
        check(!f.inputs.editors().require(mounted.editor).composition().active && changes == count &&
                  f.inputs.number_value(mounted.component) == .2,
              "gate allowed preedit publication or stepping");
        disabled.set(false);
        read_only.set(false);
        f.services.components().set_branch_active(mounted.component, true);
        f.services.mark_scene_structure_dirty();
        f.services.set_input_boundary({});
        f.synchronize();
        check(ref.focus(), "restored numeric gate did not focus");
        click(f, up);
        key(f, Key::down);
        check(changes == count + 2 && f.inputs.number_value(mounted.component) == .2,
              "restored gate did not resume button/keyboard stepping");
    }
}

void spin_preserves_numeric_input_gates() {
    Fixture f;
    InputRef ref;
    Signal<bool> loading{false};
    Signal<bool> disabled{false};
    Signal<bool> read_only{false};
    int changes{};
    int steps{};
    int mounts{};
    f.services.mount(Content{[&] {
        Spin(SpinProps{}.spinning(loading), SpinContent{[&] {
                 ++mounts;
                 InputNumber(InputNumberProps{}
                                 .defaultValue(.2)
                                 .min(0)
                                 .max(1)
                                 .step(.1)
                                 .ref(ref)
                                 .disabled(disabled)
                                 .readOnly(read_only)
                                 .onChange([&](InputNumberValue) { ++changes; })
                                 .onStep([&](double, InputNumberStepInfo) { ++steps; }));
             }});
    }});
    f.synchronize();
    const auto mounted = f.inputs.mounted_inputs().front();
    const auto actions = f.inputs.number_actions(mounted.component);
    const auto text = f.inputs.text_scene(mounted.component);
    const auto up = action(f, mounted.component, 0);
    const auto down = action(f, mounted.component, 1);
    for (int gate = 0; gate < 3; ++gate) {
        check(ref.focus({InputFocusCursor::All}), "Spin numeric fixture focus failed");
        const auto stale = f.inputs.sessions().active();
        const auto count = changes;
        check(stale.valid() && bool(f.inputs.dispatch(CompositionChanged{String{u8"0.9"}, {3, 0}, stale})),
              "Spin numeric fixture preedit failed");
        loading.set(true);
        disabled.set(gate == 1);
        read_only.set(gate == 2);
        f.synchronize();
        check(!f.services.focus().state().focused && !f.inputs.sessions().active().valid() &&
                  !f.inputs.editors().require(mounted.editor).composition().active &&
                  !f.services.interactions().component_active(mounted.component) &&
                  !f.services.interactions().component_active(actions[0]) &&
                  !f.services.interactions().component_active(actions[1]) && !ref.focus() && !ref.select(0, 0),
              "loading Spin retained numeric focus/session/ref/action access");
        click(f, up);
        key(f, Key::up);
        check(!f.inputs.dispatch(TextCommitted{String{u8"0.9"}, stale}) &&
                  !f.inputs.dispatch(CompositionChanged{String{u8"0.9"}, {3, 0}, stale}) && changes == count &&
                  steps == count && f.inputs.number_value(mounted.component) == .2 &&
                  f.inputs.editors().require(mounted.editor).value() == "0.2",
              "loading Spin allowed numeric action/keyboard/late IME mutation");

        loading.set(false);
        f.synchronize();
        check(f.services.interactions().component_active(mounted.component) &&
                  f.services.interactions().require(up).eligible == (gate == 0) &&
                  f.services.interactions().require(down).eligible == (gate == 0),
              "Spin recovery overwrote numeric disabled/readOnly eligibility");
        check(ref.focus() == (gate != 1) && ref.select(0, 0) == (gate != 1),
              "Spin recovery did not preserve numeric ref gates");
        if (gate != 0) {
            click(f, up);
            key(f, Key::up);
            check(!f.inputs.dispatch(TextCommitted{String{u8"0.9"}, f.inputs.sessions().active()}) &&
                      changes == count && steps == count && f.inputs.number_value(mounted.component) == .2,
                  "Spin recovery bypassed numeric disabled/readOnly");
        }

        disabled.set(false);
        read_only.set(false);
        f.synchronize();
        check(ref.focus({InputFocusCursor::All}), "Spin numeric ref did not recover");
        const auto fresh = f.inputs.sessions().active();
        check(fresh.valid() && fresh != stale && !f.inputs.dispatch(TextCommitted{String{u8"0.9"}, stale}) &&
                  bool(f.inputs.dispatch(CompositionChanged{String{u8"0.9"}, {3, 0}, fresh})),
              "Spin numeric recovery reused a stale session or rejected fresh IME");
        f.synchronize();
        click(f, up);
        key(f, Key::up);
        check(!f.services.interactions().require(up).eligible && !f.services.interactions().require(down).eligible &&
                  changes == count && steps == count && f.inputs.number_value(mounted.component) == .2,
              "Spin recovery bypassed numeric preedit gate");
        check(ref.select(0, 0), "Spin numeric preedit cancellation failed");
        f.synchronize();
        check(f.services.interactions().require(up).eligible && f.services.interactions().require(down).eligible,
              "Spin numeric preedit cancellation did not restore controls");
        click(f, up);
        key(f, Key::down);
        check(changes == count + 2 && steps == count + 2 && f.inputs.number_value(mounted.component) == .2 &&
                  f.inputs.editors().require(mounted.editor).value() == "0.2" &&
                  f.inputs.mounted_inputs().front().component == mounted.component &&
                  f.inputs.mounted_inputs().front().editor == mounted.editor && f.inputs.editors().size() == 1 &&
                  f.inputs.number_actions(mounted.component) == actions &&
                  f.inputs.text_scene(mounted.component) == text && mounts == 1,
              "Spin numeric recovery lost button/keyboard behavior, value or retained identity");
    }
}

void controlled_draft_and_writeback() {
    Fixture f;
    Signal<InputNumberValue> value{1.0};
    InputRef ref;
    bool accept{};
    std::vector<InputNumberValue> changes;
    int submits{};
    f.inputs.mount(Content{[&] {
        InputNumber(InputNumberProps{}
                        .value(value)
                        .ref(ref)
                        .onChange([&](InputNumberValue candidate) {
                            changes.push_back(candidate);
                            if (accept) {
                                value.set(candidate);
                            }
                        })
                        .onSubmit([&](InputNumberValue current) {
                            check(current == value.get(), "submit leaked refused value");
                            ++submits;
                        }));
    }});
    f.synchronize();
    const auto mounted = f.inputs.mounted_inputs().front();
    auto& editor = f.inputs.editors().require(mounted.editor);
    type(f, ref, u8"-");
    check(changes.empty() && editor.value() == "-", "sign draft published or disappeared");
    type(f, ref, u8"1.");
    check(changes.empty() && editor.value() == "1.", "tail decimal draft published or disappeared");
    type(f, ref, u8"2.50");
    check(changes.size() == 1 && changes.back() == 2.5 && f.inputs.number_value(mounted.component) == 1 &&
              editor.value() == "2.50",
          "controlled refusal changed authority/draft");
    key(f, Key::enter);
    check(changes.size() == 1 && editor.value() == "1" && submits == 1, "submit repeated refused candidate");
    key(f, Key::up);
    check(changes.back() == 2 && f.inputs.number_value(mounted.component) == 1 && editor.value() == "1",
          "controlled step refusal changed authority/display");
    accept = true;
    type(f, ref, u8"2.50");
    check(value.get() == 2.5 && editor.value() == "2.50", "writeback destroyed typing form");
    check(ref.blur() && editor.value() == "2.5", "blur did not normalize accepted numeric draft");
    type(f, ref, {});
    check(!value.get() && editor.value().empty(), "empty input did not publish nullopt");
    value.set(4.25);
    check(editor.value() == "4.25", "external value did not reconcile");
    type(f, ref, u8"bad");
    const auto count = changes.size();
    key(f, Key::enter);
    check(editor.value() == "4.25" && changes.size() == count, "invalid draft did not restore authority");
}

void native_steps_preedit_and_lifecycle() {
    Fixture f;
    InputRef ref;
    Signal<bool> disabled{false};
    Signal<bool> read_only{false};
    Signal<bool> controls{true};
    Signal<bool> keyboard{true};
    std::vector<InputNumberStepInfo> steps;
    std::vector<InputNumberValue> changes;
    int runs{};
    int prefix_runs{};
    int suffix_runs{};
    f.inputs.mount(Content{[&] {
        ++runs;
        InputNumber(InputNumberProps{}
                        .defaultValue(.1)
                        .min(0)
                        .max(.3)
                        .step(.1)
                        .ref(ref)
                        .disabled(disabled)
                        .readOnly(read_only)
                        .controls(controls)
                        .keyboard(keyboard)
                        .onChange([&](InputNumberValue value) { changes.push_back(value); })
                        .onStep([&](double, InputNumberStepInfo info) { steps.push_back(info); }),
                    InputPrefix{[&] { ++prefix_runs; }}, InputSuffix{[&] { ++suffix_runs; }});
    }});
    f.synchronize();
    const auto mounted = f.inputs.mounted_inputs().front();
    const auto text = f.inputs.text_scene(mounted.component);
    const auto actions = f.inputs.number_actions(mounted.component);
    const auto up = action(f, mounted.component, 0);
    const auto down = action(f, mounted.component, 1);
    check(f.inputs.editors().size() == 1 && ref.focus(), "number created a second editor");
    key(f, Key::up);
    check(f.inputs.number_value(mounted.component) == .2 && steps.size() == 1 &&
              steps.back().source == InputNumberStepSource::Keyboard,
          "native keyboard step failed");
    f.synchronize();
    click(f, up);
    check(f.inputs.number_value(mounted.component) == .3 && steps.size() == 2 &&
              steps.back().source == InputNumberStepSource::Button &&
              f.services.focus().state().focused == mounted.interaction,
          "native click did not step/keep editor focus");
    f.synchronize();
    check(!f.services.interactions().require(up).eligible, "upper bound action eligible");
    key(f, Key::up);
    check(steps.size() == 2, "bound step published");
    click(f, down);
    check(f.inputs.number_value(mounted.component) == .2, "native decrement drifted");
    const auto stamp = f.inputs.sessions().active();
    const auto count = changes.size();
    check(bool(f.inputs.dispatch(CompositionChanged{String{u8"123"}, {3, 0}, stamp})), "number preedit failed");
    key(f, Key::up);
    f.synchronize();
    click(f, up);
    check(changes.size() == count && f.inputs.editors().require(mounted.editor).composition().active &&
              f.inputs.sessions().active() == stamp,
          "preedit parsed or stepped");
    check(bool(f.inputs.dispatch(TextCommitted{String{u8"x"}, stamp})), "preedit commit failed");
    check(changes.size() == count, "invalid IME commit published a number");
    key(f, Key::enter);
    f.synchronize();
    check(f.services.interactions().require(up).eligible, "IME finish did not restore action eligibility");
    check(ref.focus({InputFocusCursor::All}), "IME replacement focus failed");
    const auto numeric_stamp = f.inputs.sessions().active();
    check(bool(f.inputs.dispatch(CompositionChanged{String{u8"0.1"}, {3, 0}, numeric_stamp})) &&
              bool(f.inputs.dispatch(TextCommitted{String{u8"0.1"}, numeric_stamp})) &&
              f.inputs.number_value(mounted.component) == .1,
          "numeric IME commit did not publish");
    check(ref.focus({InputFocusCursor::All}), "same-value IME focus failed");
    check(bool(f.inputs.dispatch(CompositionChanged{String{u8"0.1"}, {3, 0}, numeric_stamp})) &&
              bool(f.inputs.dispatch(TextCommitted{String{u8"0.1"}, numeric_stamp})) &&
              f.services.interactions().require(up).eligible,
          "same-value IME commit left controls disabled");
    check(bool(f.inputs.dispatch(CompositionChanged{String{u8"x"}, {1, 0}, numeric_stamp})), "cancel preedit failed");
    key(f, Key::escape);
    check(!f.inputs.editors().require(mounted.editor).composition().active &&
              f.services.interactions().require(up).eligible,
          "IME cancellation left controls disabled");
    keyboard.set(false);
    key(f, Key::up);
    check(f.inputs.number_value(mounted.component) == .1, "keyboard=false stepped");
    keyboard.set(true);
    const auto scroll_before = f.inputs.number_value(mounted.component);
    static_cast<void>(f.inputs.dispatch(ScrollInputEvent{10, 10, 0, 1}));
    check(f.inputs.number_value(mounted.component) == scroll_before, "wheel changed numeric value");
    f.synchronize();
    pointer(f, up, PointerAction::down);
    disabled.set(true);
    pointer(f, up, PointerAction::up);
    check(!f.inputs.sessions().active().valid() && !ref.focus() && f.inputs.number_value(mounted.component) == .1,
          "disabled did not cancel press/session");
    disabled.set(false);
    read_only.set(true);
    check(ref.focus(), "readonly focus rejected");
    key(f, Key::up);
    check(f.inputs.number_value(mounted.component) == .1 && !f.services.interactions().require(up).eligible,
          "readonly stepped");
    read_only.set(false);
    controls.set(false);
    f.synchronize();
    check(!f.services.interactions().require(up).eligible, "hidden controls stayed eligible");
    key(f, Key::up);
    check(f.inputs.number_value(mounted.component) == .2, "hidden controls disabled keyboard");
    controls.set(true);
    f.synchronize();
    check(f.inputs.text_scene(mounted.component) == text && f.inputs.number_actions(mounted.component) == actions &&
              f.inputs.mounted_inputs().front().editor == mounted.editor && runs == 1 && prefix_runs == 1 &&
              suffix_runs == 1,
          "numeric updates remounted retained state/slots");
    const auto late = f.inputs.sessions().active();
    check(f.services.destroy(mounted.component), "numeric teardown did not destroy component");
    check(!ref.bound() && f.inputs.editors().size() == 0, "numeric teardown leaked ref/editor");
    check(!f.services.interactions().find(up) && !f.services.interactions().find(down),
          "numeric teardown leaked actions");
    check(!f.inputs.sessions().active().valid(), "numeric teardown leaked session");
    check(!f.inputs.dispatch(TextCommitted{String{u8"9"}, late}), "numeric teardown accepted stale commit");
}

void range_atomicity_and_sizes() {
    Fixture f;
    InputRef ref;
    Signal<double> maximum{1};
    Signal<double> step{.1};
    Signal<std::optional<unsigned>> precision{1};
    Signal<InputNumberValue> value{.2};
    int changes{};
    f.inputs.mount(Content{[&] {
        InputNumber(
            InputNumberProps{}.value(value).min(0).max(maximum).step(step).precision(precision).ref(ref).onChange(
                [&](InputNumberValue next) {
                    ++changes;
                    value.set(next);
                }));
    }});
    f.synchronize();
    const auto mounted = f.inputs.mounted_inputs().front();
    type(f, ref, u8"2.6");
    check(changes == 0 && f.inputs.editors().require(mounted.editor).value() == "2.6", "out-of-range draft published");
    key(f, Key::enter);
    check(value.get() == 1 && changes == 1, "range finish did not clamp");
    maximum.set(.5);
    check(value.get() == 1 && changes == 1, "range change modified controlled value");
    key(f, Key::down);
    check(value.get() == .5, "out-of-range value could not move towards range");
    type(f, ref, u8"-");
    for (int i = 0; i < 4; ++i) {
        type(f, ref, u8"-");
        bool rejected{};
        try {
            if (i == 0) {
                maximum.set(-1);
            }
            if (i == 1) {
                step.set(0);
            }
            if (i == 2) {
                value.set(std::numeric_limits<double>::quiet_NaN());
            }
            if (i == 3) {
                precision.set(13);
            }
        } catch (const std::invalid_argument&) {
            rejected = true;
        }
        check(rejected && f.inputs.number_value(mounted.component) == .5 &&
                  f.inputs.editors().require(mounted.editor).value() == "-",
              "invalid update partially applied");
        if (i == 0) {
            maximum.set(.5);
        }
        if (i == 1) {
            step.set(.1);
        }
        if (i == 2) {
            value.set(.5);
        }
        if (i == 3) {
            precision.set(1);
        }
    }
    for (const auto size : {ControlSize::Small, ControlSize::Middle, ControlSize::Large}) {
        for (const auto variant :
             {InputVariant::Outlined, InputVariant::Filled, InputVariant::Borderless, InputVariant::Underlined}) {
            Fixture sized;
            sized.inputs.mount(Content{[&] { InputNumber(InputNumberProps{}.size(size).variant(variant)); }});
            sized.synchronize();
            const auto item = sized.inputs.mounted_inputs().front();
            const auto rect = sized.nodes.require(item.node).bounds;
            check(rect.width == 90 &&
                      rect.height == detail::derive_input_tokens(resolve_theme()).size(size).control_height &&
                      sized.inputs.variant(item.component) == variant,
                  "numeric size/variant metrics drifted");
        }
    }
    Fixture conflict;
    bool rejected{};
    try {
        composition_selection_cancellation();
        precision_step_direction();
        composition_cancellation_gates();
        conflict.inputs.mount(Content{[] { InputNumber(InputNumberProps{}.value(1).defaultValue(2)); }});
    } catch (const std::invalid_argument&) {
        rejected = true;
    }
    check(rejected && conflict.inputs.editors().size() == 0, "conflicting mode acquired editor resources");
}

void synchronous_disposal() {
    for (const bool step : {false, true}) {
        Fixture f;
        InputRef ref;
        runtime::ComponentId self;
        int after{};
        f.inputs.mount(Content{[&] {
            InputNumber(InputNumberProps{}
                            .defaultValue(1)
                            .ref(ref)
                            .onChange([&](InputNumberValue) {
                                check(f.services.destroy(self), "numeric callback disposal failed");
                            })
                            .onStep([&](double, InputNumberStepInfo) { ++after; }));
        }});
        self = f.inputs.mounted_inputs().front().component;
        f.synchronize();
        check(ref.focus(), "disposal focus failed");
        if (step) {
            key(f, Key::up);
        } else {
            type(f, ref, u8"2");
        }
        check(!ref.bound() && f.inputs.mounted_inputs().empty() && !f.inputs.sessions().active().valid() && after == 0,
              "disposed number used late state/callback");
    }
}

void focus_redirect_and_blur_disposal() {
    Fixture f;
    InputRef first;
    InputRef second;
    int changes{};
    f.inputs.mount(Content{[&] {
        InputNumber(InputNumberProps{}
                        .defaultValue(1)
                        .ref(first)
                        .onFocus([&] { static_cast<void>(second.focus()); })
                        .onChange([&](InputNumberValue) { ++changes; }));
        Input(InputProps{}.ref(second));
    }});
    const auto number = f.inputs.mounted_inputs().front();
    f.synchronize();
    click(f, action(f, number.component, 0));
    check(changes == 0 && f.inputs.number_value(number.component) == 1 &&
              f.services.focus().state().focused == f.inputs.mounted_inputs().back().interaction,
          "redirected focus still stepped old number");
    Fixture blur;
    InputRef ref;
    runtime::ComponentId self;
    blur.inputs.mount(Content{[&] {
        InputNumber(InputNumberProps{}.defaultValue(1).max(2).ref(ref).onChange(
            [&](InputNumberValue) { check(blur.services.destroy(self), "blur normalization disposal failed"); }));
    }});
    self = blur.inputs.mounted_inputs().front().component;
    blur.synchronize();
    type(blur, ref, u8"3");
    static_cast<void>(ref.blur());
    check(!ref.bound() && blur.inputs.editors().size() == 0 && !blur.inputs.sessions().active().valid(),
          "blur normalization used disposed numeric state");
}
} // namespace

int main() {
    try {
        controlled_draft_and_writeback();
        native_steps_preedit_and_lifecycle();
        spin_preserves_numeric_input_gates();
        range_atomicity_and_sizes();
        synchronous_disposal();
        focus_redirect_and_blur_disposal();
    } catch (const std::exception& error) {
        std::cerr << error.what() << '\n';
        return 1;
    }
}
