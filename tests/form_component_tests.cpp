#include "component/form_component.hpp"
#include "component/feedback_component.hpp"
#include "component/selection_component.hpp"
#include "component/select_component.hpp"
#include "support/input_fixture.hpp"

#include <iostream>
#include <cmath>
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

void sync(Fixture& fixture, float width = 400, runtime::Rect clip = {0, 0, 400, 900}) {
    check(fixture.services.layout_and_synchronize({width, 900}, clip), "Form layout synchronization failed");
}

runtime::Rect bounds(Fixture& fixture, runtime::ComponentId id) {
    return fixture.nodes.require(fixture.services.components().root(id)).bounds;
}

void key(Fixture& fixture, Key value) {
    fixture.services.focus().dispatch({value, KeyAction::down});
    fixture.services.focus().dispatch({value, KeyAction::up});
}

void click(Fixture& fixture, runtime::ComponentId id) {
    const auto rect = bounds(fixture, id);
    for (auto action : {PointerAction::down, PointerAction::up}) {
        fixture.services.pointer().dispatch({PointerIdentity::mouse(), action, PointerButton::primary,
                                             rect.x + rect.width / 2, rect.y + rect.height / 2});
    }
    sync(fixture);
}

void type(Fixture& fixture, const InputRef& ref, String value) {
    check(ref.focus({InputFocusCursor::All}), "Form input focus failed");
    check(bool(fixture.inputs.dispatch(TextCommitted{std::move(value), fixture.inputs.sessions().active()})),
          "Form input commit failed");
}

void real_control_flow() {
    Fixture f;
    detail::SelectionComponentHost selection{f.services};
    FormRef form;
    FormField<String> name;
    FormField<SelectSelection> role;
    FormField<bool> agree;
    FormField<InputNumberValue> count;
    name.required();
    role.required();
    agree.required();
    count.required().validator(
        [](InputNumberValue value) { return value && *value < 1 ? String{u8"Count must be positive"} : String{}; });
    InputRef name_ref;
    SelectRef role_ref;
    CheckboxRef agree_ref;
    InputRef count_ref;
    Signal<bool> disabled{false};
    Signal<bool> read_only{false};
    int mounts{};
    int finishes{};
    int failures{};
    FormResult submitted;
    f.services.mount(Content{[&] {
        Form(FormProps{}
                 .ref(form)
                 .disabled(disabled)
                 .readOnly(read_only)
                 .onFinish([&](FormResult result) {
                     ++finishes;
                     submitted = std::move(result);
                 })
                 .onFinishFailed([&](FormResult result) {
                     ++failures;
                     submitted = std::move(result);
                 }),
             FormContent{[&] {
                 FormItem(FormItemProps{}
                              .field(String{u8"name"}, name)
                              .label(String{u8"Name"})
                              .help(String{u8"Use a display name"})
                              .control(name_ref),
                          FormItemContent{[&] {
                              ++mounts;
                              Input(InputProps{}
                                        .value(name.value())
                                        .onChange(name.onChange())
                                        .status(name.status())
                                        .disabled(name.disabled())
                                        .readOnly(name.readOnly())
                                        .ref(name_ref));
                          }});
                 FormItem(FormItemProps{}.field(String{u8"role"}, role).label(String{u8"Role"}).control(role_ref),
                          FormItemContent{[&] {
                              ++mounts;
                              Select(SelectProps{}
                                         .options({{SelectValue{u8"operator"}, String{u8"Operator"}}})
                                         .value(role.value())
                                         .onChange(role.onChange())
                                         .status(role.status())
                                         .disabled(role.uneditable())
                                         .ref(role_ref));
                          }});
                 FormItem(FormItemProps{}.field(String{u8"agree"}, agree).label(String{u8"Consent"}).control(agree_ref),
                          FormItemContent{[&] {
                              ++mounts;
                              Checkbox(CheckboxProps{}
                                           .checked(agree.value())
                                           .onChange(agree.onChange())
                                           .disabled(agree.uneditable())
                                           .ref(agree_ref),
                                       [] { Text(u8"Accept"); });
                          }});
                 FormItem(FormItemProps{}.field(String{u8"count"}, count).label(String{u8"Count"}).control(count_ref),
                          FormItemContent{[&] {
                              ++mounts;
                              InputNumber(InputNumberProps{}
                                              .value(count.value())
                                              .onChange(count.onChange())
                                              .status(count.status())
                                              .disabled(count.disabled())
                                              .readOnly(count.readOnly())
                                              .ref(count_ref));
                          }});
                 Button(ButtonProps{}.onClick([=] { static_cast<void>(form.submit()); }), [] { Text(u8"Submit"); });
                 Button(ButtonProps{}.onClick([=] { static_cast<void>(form.reset()); }), [] { Text(u8"Reset"); });
             }});
    }});
    sync(f);
    const auto form_id = f.services.form().mounted().front();
    const auto name_input = f.inputs.mounted_inputs().front();
    const auto submit = f.buttons.mounted_buttons()[0].component;
    const auto reset = f.buttons.mounted_buttons()[1].component;
    const auto components = f.services.components().component_count();
    click(f, submit);
    check(failures == 1 && !finishes && submitted.errors.size() == 4 &&
              f.services.focus().state().focused == name_input.interaction,
          "Form failure/focus missing");
    check(f.inputs.status(name_input.component) == InputStatus::Error, "Form status did not reach actual Input");
    type(f, name_ref, String{u8"RynUI"});
    check(name.get() == String{u8"RynUI"} && name.error().empty(), "native Input binding did not update field");
    check(role_ref.focus(), "Select focus failed");
    key(f, Key::down);
    key(f, Key::enter);
    check(role.get() == SelectSelection{SelectValue{u8"operator"}}, "native Select binding failed");
    check(agree_ref.focus(), "Checkbox focus failed");
    key(f, Key::space);
    check(agree.get(), "native Checkbox binding failed");
    type(f, count_ref, String{u8"0"});
    check(form.submit() && failures == 2 && count.error() == String{u8"Count must be positive"},
          "custom numeric validation missing");
    type(f, count_ref, String{u8"3"});
    click(f, submit);
    check(finishes == 1 && submitted.valid() && submitted.values.size() == 4 && count.get() == 3,
          "native form success flow failed");
    disabled.set(true);
    sync(f);
    check(!form.submit() && !name_ref.focus() && !agree_ref.focus(), "Form disabled did not reach native controls");
    disabled.set(false);
    read_only.set(true);
    sync(f);
    check(!agree_ref.focus() && form.submit() && finishes == 2,
          "readOnly should block selection edits and allow submit");
    read_only.set(false);
    click(f, reset);
    check(name.get().empty() && !role.get() && !agree.get() && !count.get() && name.error().empty(),
          "native reset did not restore initial values");
    check(mounts == 4 && f.services.components().component_count() == components &&
              f.inputs.mounted_inputs().front().component == name_input.component,
          "value/validation/reset remounted unrelated control slots");
    check(f.services.destroy(form_id) && !form.bound() && !form.submit() && !form.reset() && !form.validate() &&
              !name_ref.bound() && !role_ref.bound() && !agree_ref.bound() && !count_ref.bound(),
          "Form teardown left held refs active");
}

void draft_preedit_and_equal_value_reset() {
    Fixture f;
    FormRef form;
    FormField<InputNumberValue> number;
    number.required();
    InputRef ref;
    Signal<String> help{String{u8"A number"}};
    int mounts{};
    f.services.mount(Content{[&] {
        Form(FormProps{}.ref(form), FormContent{[&] {
                 FormItem(FormItemProps{}.field(String{u8"number"}, number).help(help).control(ref),
                          FormItemContent{[&] {
                              ++mounts;
                              InputNumber(InputNumberProps{}
                                              .value(number.value())
                                              .onChange(number.onChange())
                                              .status(number.status())
                                              .ref(ref));
                          }});
             }});
    }});
    sync(f);
    const auto input = f.inputs.mounted_inputs().front();
    type(f, ref, String{u8"-"});
    check(!number.get() && f.inputs.editors().require(input.editor).value() == "-", "numeric draft fixture failed");
    check(form.validate()->errors.size() == 1, "draft validation missing required error");
    help.set(String{u8"Updated help"});
    sync(f);
    check(mounts == 1 && f.inputs.editors().require(input.editor).value() == "-", "validation/render discarded draft");
    const auto stamp = f.inputs.sessions().active();
    check(bool(f.inputs.dispatch(CompositionChanged{String{u8"候选"}, {0, 0}, stamp})), "preedit fixture failed");
    check(form.validate().has_value(), "preedit validation failed");
    sync(f);
    check(f.inputs.editors().require(input.editor).composition().active && number.get() == std::nullopt,
          "validation discarded or published preedit");
    check(form.reset(), "equal value reset rejected");
    sync(f);
    check(!f.inputs.editors().require(input.editor).composition().active &&
              f.inputs.editors().require(input.editor).value().empty() && number.error().empty() && mounts == 1 &&
              f.inputs.mounted_inputs().front().editor == input.editor,
          "same numeric value reset retained draft/preedit or remounted");
}

void multiline_and_input_boundaries() {
    Fixture f;
    FormRef background;
    FormRef modal_form;
    FormField<String> text;
    FormField<String> modal_text;
    text.required();
    modal_text.required();
    InputRef ref;
    InputRef modal_ref;
    Signal<bool> spinning{false};
    Signal<bool> open{false};
    int submissions{};
    f.services.mount(Content{[&] {
        Spin(SpinProps{}.spinning(spinning), SpinContent{[&] {
                 Form(FormProps{}.ref(background).onFinish([&](FormResult) { ++submissions; }), FormContent{[&] {
                          FormItem(FormItemProps{}.field(String{u8"notes"}, text).control(ref), FormItemContent{[&] {
                                       TextArea(TextAreaProps{}.value(text.value()).onChange(text.onChange()).ref(ref));
                                   }});
                      }});
             }});
        Modal(ModalProps{}.open(open).footer(ModalFooterMode::None), ModalContent{[&] {
                  Form(FormProps{}.ref(modal_form), FormContent{[&] {
                           FormItem(FormItemProps{}.field(String{u8"modal"}, modal_text).control(modal_ref),
                                    FormItemContent{[&] {
                                        Input(InputProps{}
                                                  .value(modal_text.value())
                                                  .onChange(modal_text.onChange())
                                                  .ref(modal_ref));
                                    }});
                       }});
              }});
    }});
    sync(f);
    check(!modal_form.submit() && !modal_ref.focus(), "closed Modal form escaped branch boundary");
    type(f, ref, String{u8"line"});
    key(f, Key::enter);
    check(text.get() == String{u8"line\n"} && submissions == 0, "Form swallowed TextArea Enter");
    check(bool(f.inputs.dispatch(CompositionChanged{String{u8"候选"}, {0, 0}, f.inputs.sessions().active()})),
          "IME fixture failed");
    key(f, Key::enter);
    check(submissions == 0, "Form implicitly submitted an IME Enter");
    spinning.set(true);
    sync(f);
    check(!background.submit() && !background.validate() && !ref.focus() && !f.inputs.sessions().active().valid(),
          "Spin blocked Form accepted command or retained input session");
    spinning.set(false);
    open.set(true);
    sync(f);
    check(!background.submit() && !background.validate() && !ref.focus() && modal_form.submit(),
          "Form crossed Modal input boundary");
    open.set(false);
    sync(f);
    const auto form_id = f.services.form().mounted().front();
    f.services.components().set_branch_active(form_id, false);
    f.services.mark_scene_structure_dirty();
    sync(f);
    check(!background.submit() && !background.validate() && !ref.focus(), "hidden Form accepted commands/focus");
    f.services.components().set_branch_active(form_id, true);
    f.services.mark_scene_structure_dirty();
    sync(f);
    check(background.submit() && submissions == 1, "restored Form did not submit");
}

void removal_focus_and_callback_destruction() {
    Fixture f;
    FormRef form;
    FormField<String> first;
    FormField<String> second;
    first.required();
    second.required();
    InputRef one;
    InputRef two;
    std::function<void(String)> old_change;
    int finishes{};
    bool destroy_in_validator{};
    runtime::ComponentId form_id;
    second.validator([&](String) {
        if (destroy_in_validator) {
            check(f.services.destroy(form_id), "validator could not destroy form");
        }
        return String{};
    });
    f.services.mount(Content{[&] {
        Form(FormProps{}.ref(form).onFinish([&](FormResult) { ++finishes; }), FormContent{[&] {
                 FormItem(FormItemProps{}.field(String{u8"first"}, first).control(one), FormItemContent{[&] {
                              old_change = first.onChange();
                              Input(InputProps{}.value(first.value()).onChange(first.onChange()).ref(one));
                          }});
                 FormItem(FormItemProps{}.field(String{u8"second"}, second).control(two), FormItemContent{[&] {
                              Input(InputProps{}.value(second.value()).onChange(second.onChange()).ref(two));
                          }});
             }});
    }});
    sync(f);
    form_id = f.services.form().mounted().front();
    const auto first_item = f.services.form().items().front();
    f.services.components().set_branch_active(first_item, false);
    f.services.mark_scene_structure_dirty();
    sync(f);
    check(form.submit() && f.services.focus().state().focused == f.inputs.mounted_inputs().back().interaction,
          "hidden invalid field stole focus");
    check(f.services.destroy(first_item) && first.error().empty() && form.validate()->values.size() == 1,
          "removed field leaked result/error");
    old_change(String{u8"late"});
    check(first.get().empty(), "removed control callback mutated retained field");
    first.set(String{u8"retained"});
    f.services.append_slot(form_id, Content{[&] {
                               FormItem(
                                   FormItemProps{}.field(String{u8"first"}, first).control(one), FormItemContent{[&] {
                                       Input(InputProps{}.value(first.value()).onChange(first.onChange()).ref(one));
                                   }});
                           }});
    sync(f);
    old_change(String{u8"late"});
    check(first.get() == String{u8"retained"} && first.error().empty() && form.validate()->values.size() == 2,
          "remount did not retain value/clear old registration");
    second.set(String{u8"valid"});
    destroy_in_validator = true;
    check(!form.submit() && !form.bound() && finishes == 0 && !two.bound() && first.error().empty(),
          "validator destruction emitted callback or left held refs active");
}

void feedback_flow_and_actual_glyph_clip() {
    Fixture f;
    FormRef form;
    FormField<String> field;
    field.required(String{u8"Required feedback"});
    InputRef ref;
    Signal<String> label{String{}};
    Signal<String> help{String{}};
    f.services.mount(Content{[&] {
        Form(FormProps{}.ref(form), FormContent{[&] {
                 FormItem(FormItemProps{}.field(String{u8"name"}, field).label(label).help(help).control(ref),
                          FormItemContent{
                              [&] { Input(InputProps{}.value(field.value()).onChange(field.onChange()).ref(ref)); }});
                 Button(ButtonProps{}, [] { Text(u8"Following"); });
             }});
    }});
    sync(f);
    const auto form_id = f.services.form().mounted().front();
    const auto item = f.services.form().items().front();
    const auto parts = f.services.form().snapshot(item);
    const auto sibling = f.buttons.mounted_buttons().front().component;
    const auto initial = bounds(f, sibling);
    check(f.nodes.require(f.services.components().root(parts.label)).out_of_flow &&
              f.nodes.require(f.services.components().root(parts.feedback)).out_of_flow &&
              f.nodes.require(f.services.components().root(parts.feedback)).layout_size == runtime::Size{},
          "empty label/feedback consumed actual layout flow");
    label.set(String{u8"Long label that wraps within a narrow native form"});
    check(form.validate()->errors.size() == 1, "feedback validation failed");
    sync(f);
    check(bounds(f, sibling).y > initial.y && bounds(f, parts.control).y > bounds(f, parts.label).y,
          "visible label/error did not participate in real layout");
    auto& root = f.nodes.require(f.services.components().root(form_id));
    root.clip_content = true;
    bool glyph_seen{};
    sync(f, 110, {0, 0, 110, 70});
    for (const auto& command : f.services.scene_composer().ordered_scene().commands()) {
        if (command.kind != graphics::SceneDrawKind::glyph) {
            continue;
        }
        for (std::uint32_t i = command.first_instance; i < command.first_instance + command.instance_count; ++i) {
            const auto& glyph = f.scene.glyph_scene().instances().at(i);
            if (glyph.translation_opacity[2] <= 0 || glyph.color[3] <= 0) {
                continue;
            }
            glyph_seen = true;
            const runtime::Rect rect{glyph.position_size[0] + glyph.translation_opacity[0],
                                     glyph.position_size[1] + glyph.translation_opacity[1], glyph.position_size[2],
                                     glyph.position_size[3]};
            const runtime::Rect clip{glyph.clip_bounds[0], glyph.clip_bounds[1],
                                     glyph.clip_bounds[2] - glyph.clip_bounds[0],
                                     glyph.clip_bounds[3] - glyph.clip_bounds[1]};
            const auto visible = graphics::intersect_effect_bounds(rect, clip);
            check(visible.width <= 0 || visible.height <= 0 ||
                      (visible.x >= 0 && visible.y >= 0 && visible.x + visible.width <= 110.001F &&
                       visible.y + visible.height <= 70.001F),
                  "Form glyph escaped actual ancestor/document clip");
        }
    }
    check(glyph_seen, "Form clip test did not inspect actual glyphs");
    check(form.reset(), "feedback reset failed");
    label.set(String{});
    sync(f);
    check(bounds(f, sibling) == initial && f.nodes.require(f.services.components().root(parts.feedback)).out_of_flow,
          "hidden feedback left stale height/gap");
}

void live_theme_and_clipped_focus() {
    Fixture f;
    Signal<ThemeConfig> theme{ThemeConfig{}};
    FormRef form;
    FormField<String> name;
    name.required();
    InputRef ref;
    int mounts{};
    f.services.mount(Content{[&] {
        Theme(ThemeProps{}.config(theme), ThemeContent{[&] {
                  Form(FormProps{}.ref(form), FormContent{[&] {
                           FormItem(FormItemProps{}.field(String{u8"name"}, name).label(String{u8"Label"}).control(ref),
                                    FormItemContent{[&] {
                                        ++mounts;
                                        Input(InputProps{}.value(name.value()).onChange(name.onChange()).ref(ref));
                                    }});
                       }});
              }});
    }});
    sync(f);
    const auto item = f.services.form().items().front();
    const auto parts = f.services.form().snapshot(item);
    const auto initial_label = bounds(f, parts.label);
    const auto initial_control = bounds(f, parts.control);
    auto config = ThemeConfig{};
    config.form.tokens.label = Color::rgba8(33, 77, 99);
    theme.set(config);
    sync(f);
    check(bounds(f, parts.label) == initial_label && bounds(f, parts.control) == initial_control && mounts == 1,
          "Form color update changed layout or reran controls");
    bool label_glyph{};
    for (const auto& text : f.services.text().mounted_texts()) {
        if (f.services.components().parent(text.component) != parts.label) {
            continue;
        }
        const auto range = f.scene.primitive(text.scene).instances;
        for (auto i = range.first; i < range.first + range.count; ++i) {
            const auto& glyph = f.scene.glyph_scene().instances().at(i);
            label_glyph = true;
            const auto color = *config.form.tokens.label;
            check(glyph.color == std::array{color.red(), color.green(), color.blue(), color.alpha()},
                  "Form theme did not update actual label glyph material");
        }
    }
    check(label_glyph, "Form Theme test has no actual label glyphs");
    config.form.tokens.line_height = dp(30);
    config.form.tokens.label_gap = dp(12);
    theme.set(config);
    sync(f);
    check(bounds(f, parts.label).height > initial_label.height && bounds(f, parts.control).y > initial_control.y &&
              mounts == 1,
          "Form metric update did not update actual layout");
    const auto control = bounds(f, parts.control);
    sync(f, 400, {0, 0, 400, control.y});
    check(form.submit() && !f.services.focus().state().focused, "clipped control focused from a visible label");
    sync(f);
    check(form.submit() && f.services.focus().state().focused == f.inputs.mounted_inputs().front().interaction,
          "visible invalid control did not recover focus");
}

void callbacks_and_control_generation() {
    {
        Fixture f;
        FormRef form;
        FormField<String> field{String{u8"initial"}};
        field.required();
        InputRef ref;
        f.services.mount(Content{[&] {
            Form(FormProps{}.ref(form), FormContent{[&] {
                     FormItem(FormItemProps{}.field(String{u8"name"}, field).control(ref), FormItemContent{[&] {
                                  Input(InputProps{}.value(field.value()).onChange(field.onChange()).ref(ref));
                              }});
                 }});
        }});
        sync(f);
        const auto id = f.services.form().mounted().front();
        field.set(String{});
        check(form.validate()->errors.size() == 1, "reset destruction fixture failed");
        Scope observers;
        bool destroy_on_clear{};
        auto observer = effect(observers, [&] {
            const auto error = field.error();
            if (destroy_on_clear && error.empty() && form.bound()) {
                check(f.services.destroy(id), "error observer could not destroy form");
            }
        });
        destroy_on_clear = true;
        check(!form.reset() && !form.bound() && field.get().empty() && !ref.bound(),
              "reset continued writing after an observer destroyed Form");
    }
    {
        Fixture f;
        FormRef form;
        FormField<InputNumberValue> field;
        field.required();
        InputRef ref;
        f.services.mount(Content{[&] {
            Form(FormProps{}.ref(form), FormContent{[&] {
                     FormItem(FormItemProps{}.field(String{u8"number"}, field).control(ref), FormItemContent{[&] {
                                  InputNumber(
                                      InputNumberProps{}.value(field.value()).onChange(field.onChange()).ref(ref));
                              }});
                 }});
        }});
        sync(f);
        const auto original = f.inputs.mounted_inputs().front();
        check(f.services.destroy(original.component), "control removal failed");
        f.services.append_slot(f.services.form().mounted().front(),
                               Content{[&] { InputNumber(InputNumberProps{}.ref(ref)); }});
        sync(f);
        const auto outside = f.inputs.mounted_inputs().back();
        type(f, ref, String{u8"-"});
        check(form.reset() && f.inputs.editors().require(outside.editor).value() == "-",
              "old FormItem reset a rebound external InputRef");
        check(ref.blur(), "rebound ref blur failed");
        check(form.submit() && !f.services.focus().state().focused, "old FormItem focused a rebound external InputRef");
    }
    {
        Fixture f;
        FormRef form;
        FormField<String> field;
        field.required();
        InputRef ref;
        bool destroy_on_focus{};
        int failed{};
        runtime::ComponentId id;
        f.services.mount(Content{[&] {
            Form(FormProps{}.ref(form).onFinishFailed([&](FormResult) { ++failed; }), FormContent{[&] {
                     FormItem(
                         FormItemProps{}.field(String{u8"name"}, field).control(ref), FormItemContent{[&] {
                             Input(InputProps{}.value(field.value()).onChange(field.onChange()).ref(ref).onFocus([&] {
                                 if (destroy_on_focus) {
                                     check(f.services.destroy(id), "focus callback could not destroy Form");
                                 }
                             }));
                         }});
                 }});
        }});
        sync(f);
        id = f.services.form().mounted().front();
        destroy_on_focus = true;
        check(!form.submit() && !form.bound() && !ref.bound() && failed == 0,
              "focus destruction emitted stale failure");
    }
    {
        Fixture f;
        FormRef form;
        FormField<String> field{String{u8"valid"}};
        int finished{};
        runtime::ComponentId id;
        f.services.mount(Content{[&] {
            Form(FormProps{}.ref(form).onFinish([&](FormResult) {
                ++finished;
                check(!form.submit() && !form.reset() && !form.validate(), "finish accepted recursive command");
                check(f.services.destroy(id), "finish could not destroy form");
            }),
                 FormContent{[&] {
                     FormItem(FormItemProps{}.field(String{u8"name"}, field), FormItemContent{[&] {
                                  Input(InputProps{}.value(field.value()).onChange(field.onChange()));
                              }});
                     Button(ButtonProps{}.onClick([=] { static_cast<void>(form.submit()); }), [] { Text(u8"Submit"); });
                 }});
        }});
        sync(f);
        id = f.services.form().mounted().front();
        click(f, f.buttons.mounted_buttons().front().component);
        check(finished == 1 && !form.bound() && f.services.interactions().size() == 0,
              "native submit callback destruction leaked interactions");
    }
}

void controlled_authority_and_checkbox_generation() {
    {
        Fixture f;
        FormRef form;
        Signal<String> authority{String{u8"authority"}};
        FormField<String> field{String{u8"initial"}, authority, [](String) {}};
        InputRef ref;
        f.services.mount(Content{[&] {
            Form(FormProps{}.ref(form), FormContent{[&] {
                     FormItem(FormItemProps{}.field(String{u8"name"}, field).control(ref), FormItemContent{[&] {
                                  Input(InputProps{}.value(field.value()).onChange(field.onChange()).ref(ref));
                              }});
                 }});
        }});
        sync(f);
        const auto input = f.inputs.mounted_inputs().front();
        type(f, ref, String{u8"draft"});
        check(form.validate()->valid() && field.get() == String{u8"authority"} &&
                  f.inputs.editors().require(input.editor).value() == "draft",
              "validation discarded controlled text draft");
        check(form.reset() && field.get() == String{u8"authority"} &&
                  f.inputs.editors().require(input.editor).value() == "authority",
              "text reset forged rejected authority or retained old draft");
    }
    {
        Fixture f;
        FormRef form;
        Signal<InputNumberValue> authority{4.0};
        bool accept{};
        std::vector<InputNumberValue> proposals;
        FormField<InputNumberValue> field{2.0, authority, [&](InputNumberValue value) {
                                              proposals.push_back(value);
                                              if (accept) {
                                                  authority.set(value);
                                              }
                                          }};
        field.required();
        InputRef ref;
        f.services.mount(Content{[&] {
            Form(FormProps{}.ref(form), FormContent{[&] {
                     FormItem(FormItemProps{}.field(String{u8"count"}, field).control(ref), FormItemContent{[&] {
                                  InputNumber(InputNumberProps{}
                                                  .value(field.value())
                                                  .onChange(field.onChange())
                                                  .status(field.status())
                                                  .ref(ref));
                              }});
                 }});
        }});
        sync(f);
        const auto input = f.inputs.mounted_inputs().front();
        type(f, ref, String{u8"5"});
        check(field.get() == 4 && proposals == std::vector<InputNumberValue>{5.0},
              "Form forged rejected controlled value");
        check(form.reset() && proposals.back() == 2 && field.get() == 4 &&
                  f.inputs.editors().require(input.editor).value() == "4",
              "controlled reset ignored external authority");
        accept = true;
        check(form.reset() && field.get() == 2 && f.inputs.editors().require(input.editor).value() == "2",
              "accepted controlled reset did not reach editor");
        authority.set(std::nullopt);
        check(form.validate()->errors.size() == 1, "controlled empty required validation missing");
        const auto callbacks = proposals.size();
        authority.set(7);
        sync(f);
        check(field.error().empty() && f.inputs.number_value(input.component) == 7 && proposals.size() == callbacks,
              "external authority change retained stale error or emitted a user change");
    }
    {
        Fixture f;
        detail::SelectionComponentHost selection{f.services};
        FormRef form;
        FormField<bool> field;
        field.required();
        CheckboxRef ref;
        f.services.mount(Content{[&] {
            Form(FormProps{}.ref(form), FormContent{[&] {
                     FormItem(FormItemProps{}.field(String{u8"agree"}, field).control(ref), FormItemContent{[&] {
                                  Checkbox(CheckboxProps{}.checked(field.value()).onChange(field.onChange()).ref(ref));
                              }});
                 }});
        }});
        sync(f);
        check(f.services.destroy(selection.mounted().front().component), "checkbox removal failed");
        f.services.append_slot(f.services.form().mounted().front(),
                               Content{[&] { Checkbox(CheckboxProps{}.ref(ref)); }});
        sync(f);
        check(form.submit() && !f.services.focus().state().focused, "old FormItem focused a rebound CheckboxRef");
    }
}

void form_font_publication_and_dpi() {
    for (bool algorithm : {false, true}) {
        for (float initial_scale : {1.0F, 2.0F}) {
            Fixture f;
            float scale = initial_scale;
            std::vector<std::uint32_t> requests;
            const auto resolver = [&](SystemFontFamily, std::uint32_t, bool, std::uint32_t logical_pixels) {
                requests.push_back(logical_pixels);
                f.font_scale = scale;
                return f.resolve(static_cast<std::uint32_t>(std::lround(logical_pixels * scale)));
            };
            static_cast<void>(f.services.text().set_font_resolver(resolver));
            f.inputs.set_display_scale(scale);
            ThemeConfig config;
            config.form.algorithm = algorithm;
            config.form.seed.font_size = dp(18);
            config.form.tokens.font_size = dp(14);
            config.form.tokens.line_height = dp(22);
            Signal<ThemeConfig> theme{config};
            FormRef form;
            FormField<String> field;
            field.required();
            InputRef ref;
            int mounts{};
            f.services.mount(Content{[&] {
                Theme(ThemeProps{}.config(theme), ThemeContent{[&] {
                          Form(FormProps{}.ref(form), FormContent{[&] {
                                   FormItem(
                                       FormItemProps{}
                                           .field(String{u8"name"}, field)
                                           .label(String{u8"Label"})
                                           .help(String{u8"Help"})
                                           .control(ref),
                                       FormItemContent{[&] {
                                           ++mounts;
                                           Input(InputProps{}.value(field.value()).onChange(field.onChange()).ref(ref));
                                       }});
                               }});
                      }});
            }});
            sync(f);
            const auto id = f.services.form().mounted().front();
            const auto scope = f.services.components().theme_scope(id);
            const auto parts = f.services.form().snapshot(f.services.form().items().front());
            const auto texts = f.services.text().mounted_texts();
            const auto label = *std::find_if(texts.begin(), texts.end(), [&](const auto& text) {
                return f.services.components().parent(text.component) == parts.label;
            });
            const auto feedback = *std::find_if(texts.begin(), texts.end(), [&](const auto& text) {
                return f.services.components().parent(text.component) == parts.feedback;
            });
            const auto input = f.inputs.mounted_inputs().front();
            const auto count = f.services.components().component_count();
            check(ref.focus(), "Form font fixture input focus failed");
            const auto session = f.inputs.sessions().active();
            for (float size : {.49F, .1F, std::numeric_limits<float>::denorm_min(), std::nextafter(.5F, 0.0F),
                               std::numeric_limits<float>::infinity(), std::numeric_limits<float>::quiet_NaN()}) {
                const auto snapshot = scope->snapshot().diagnostic_json();
                const auto generation = scope->generation();
                const auto label_font = f.services.text().resolved_typography(label.component);
                const auto feedback_font = f.services.text().resolved_typography(feedback.component);
                const auto label_revision = f.scene.revisions(label.scene);
                const auto feedback_revision = f.scene.revisions(feedback.scene);
                const auto geometry = bounds(f, parts.label);
                const auto request_count = requests.size();
                auto invalid = config;
                invalid.form.tokens.font_size = dp(size);
                bool rejected{};
                try {
                    theme.set(invalid);
                } catch (const std::exception&) {
                    rejected = true;
                }
                check(rejected && scope->snapshot().diagnostic_json() == snapshot &&
                          scope->generation() == generation &&
                          f.services.text().resolved_typography(label.component) == label_font &&
                          f.services.text().resolved_typography(feedback.component) == feedback_font &&
                          f.scene.revisions(label.scene) == label_revision &&
                          f.scene.revisions(feedback.scene) == feedback_revision && requests.size() == request_count &&
                          bounds(f, parts.label) == geometry && f.inputs.sessions().active() == session,
                      "invalid Form font partially published Theme or mutated actual Text/input state");
            }
            config.form.tokens.font_size = dp(.5F);
            theme.set(config);
            sync(f);
            check(f.services.text().resolved_typography(label.component).font_size == .5F &&
                      f.scene.primitive(label.scene).instances.count > 0 &&
                      !f.scene.text_state(label.scene).shaped().glyphs.empty(),
                  "threshold Form font did not produce actual text glyphs");
            scale = initial_scale == 1 ? 2.0F : 1.0F;
            f.chains.clear();
            check(f.services.text().set_font_resolver(resolver), "Form threshold font DPI did not refresh");
            f.inputs.set_display_scale(scale);
            sync(f);
            const auto font = f.scene.text_state(label.scene).shaped().glyphs.front().font;
            check(f.fonts->metrics(font).metrics.display_scale == scale &&
                      std::none_of(requests.begin(), requests.end(), [](auto pixels) { return pixels == 0; }),
                  "Form threshold rounded after DPI or requested a zero-size font");
            config.form.tokens.font_size = dp(18);
            theme.set(config);
            sync(f);
            check(scope->snapshot().form().metrics.font_size == 18 &&
                      f.services.text().resolved_typography(label.component).font_size == 18 && mounts == 1 &&
                      f.services.components().component_count() == count &&
                      f.inputs.mounted_inputs().front().editor == input.editor,
                  "Form font failed recovery or remounted native controls");
        }
        Fixture initial;
        FormRef form;
        FormField<String> field;
        ThemeConfig invalid;
        invalid.form.algorithm = algorithm;
        invalid.form.tokens.font_size = dp(.49F);
        bool rejected{};
        try {
            initial.services.mount(Content{[&] {
                Theme(ThemeProps{}.config(invalid), ThemeContent{[&] {
                          Form(FormProps{}.ref(form), FormContent{[&] {
                                   FormItem(FormItemProps{}.field(String{u8"name"}, field).label(String{u8"Label"}),
                                            FormItemContent{[] { Input(InputProps{}); }});
                               }});
                      }});
            }});
        } catch (const std::invalid_argument&) {
            rejected = true;
        }
        check(rejected && !form.bound() && initial.inputs.mounted_inputs().empty() &&
                  initial.services.text().mounted_texts().empty(),
              "initial Form font failure left partial Text/control state");
    }
}
} // namespace

int main() {
    try {
        real_control_flow();
        draft_preedit_and_equal_value_reset();
        multiline_and_input_boundaries();
        removal_focus_and_callback_destruction();
        feedback_flow_and_actual_glyph_clip();
        live_theme_and_clipped_focus();
        callbacks_and_control_generation();
        controlled_authority_and_checkbox_generation();
        form_font_publication_and_dpi();
        std::cout << "Form retained component flows passed\n";
        return 0;
    } catch (const std::exception& error) {
        std::cerr << error.what() << '\n';
        return 1;
    }
}
