#include "component/form_component.hpp"
#include "component/form_control_binding.hpp"
#include "component/modal_component.hpp"
#include "component/selection_component.hpp"
#include "support/timeline_fixture.hpp"
#include <array>
#include <iostream>
#include <utility>

namespace {
using namespace ryn;
using Fixture = ryn_test::input_component::Fixture;
int checks{};

void check(bool value, const char* message) {
    ++checks;
    if (!value) {
        throw std::runtime_error(message);
    }
}

struct Domain final {
    Fixture f;
    FormRef form;
    FormRef background;
    FormField<String> outer{String{u8"outer"}};
    FormField<String> nested{String{u8"nested"}};
    FormField<String> target{String{u8"target"}};
    FormField<String> other{String{u8"other"}};
    InputRef editor;
    InputRef private_editor;
    InputRef sibling;
    InputRef tail;
    InputRef nested_editor;
    InputRef other_editor;
    InputRef modal_editor;
    Signal<bool> modal_open{false};
    Signal<bool> hidden{false};
    Signal<bool> disabled{false};
    Signal<bool> readonly{false};
    runtime::ComponentId form_id;
    runtime::ComponentId controls;
    runtime::ComponentId item;
    runtime::ComponentId region;
    runtime::ComponentId editor_id;
    runtime::ComponentId sibling_id;
    runtime::ComponentId modal_id;
    input::InputConstraintLease allow;
    detail::ModalInputCompanion companion;
    std::function<void(String)> change;
    std::optional<FormControlBinding<String>> binding_ticket;
    std::function<void()> blur;
    std::function<void(String)> bare;
    int finishes{};
    int changes{};

    explicit Domain(bool with_nested = false, std::function<void(FormField<String>&)> configure = {}) {
        if (configure) {
            configure(target);
        }
        f.services.mount(Content{[&] {
            Form(FormProps{}
                     .ref(form)
                     .onFinish([&](FormResult) { ++finishes; })
                     .onValuesChange([&](FormFieldValue, FormData) { ++changes; }),
                 FormContent{[&] {
                     FormItem(
                         FormItemProps{}.field(u8"outer", outer), FormItemContent{[&] {
                             Flex(FlexProps{}.vertical(true), FlexContent{[&] {
                                      detail::FormPrivateEditorScope scope;
                                      Input(InputProps{}.defaultValue(u8"private").ref(private_editor));
                                      controls =
                                          *f.services.components().parent(f.inputs.mounted_inputs().back().component);
                                      if (with_nested) {
                                          FormItem(FormItemProps{}.field(u8"nested", nested),
                                                   FormItemContent{[&] { Input(InputProps{}.ref(nested_editor)); }});
                                      }
                                  }});
                             Input(InputProps{}.ref(tail));
                         }});
                     FormItem(
                         FormItemProps{}.field(u8"target", target).hidden(hidden).disabled(disabled).readOnly(readonly),
                         FormItemContent{[&] {
                             Flex(FlexProps{}.vertical(true), FlexContent{[&] {
                                      mount_editor();
                                      region = *f.services.components().parent(editor_id);
                                      Input(InputProps{}.defaultValue(u8"sibling").ref(sibling));
                                      sibling_id = f.inputs.mounted_inputs().back().component;
                                  }});
                         }});
                 }});
            Form(FormProps{}.ref(background), FormContent{[&] {
                     FormItem(FormItemProps{}.field(u8"other", other),
                              FormItemContent{[&] { Input(InputProps{}.ref(other_editor)); }});
                 }});
            Modal(ModalProps{}.open(modal_open).footer(ModalFooterMode::None), ModalContent{[&] {
                      Input(InputProps{}.defaultValue(u8"modal").ref(modal_editor));
                      modal_id = f.inputs.mounted_inputs().back().component;
                  }});
        }});
        sync();
        sync();
        form_id = f.services.form().mounted().front();
        for (auto id : f.services.form().items()) {
            if (timeline_test::descendant(f, editor_id, id)) {
                item = id;
            }
        }
        check(item.valid(), "target Item missing");
        check(editor.focus() && target.get() == String{u8"target"}, "baseline native binding missing");
        allow = f.services.create_input_constraint(f.services.interactions().input_root(form_id),
                                                   input::InputConstraintKind::RestrictToSubtrees);
    }

    void mount_editor() {
        FormControlBinding<String> binding;
        binding_ticket = binding;
        change = binding.onChange();
        blur = binding.onBlur();
        bare = target.onChange();
        Input(InputProps{}
                  .value(binding.value())
                  .onChange(change)
                  .onBlur(blur)
                  .disabled(binding.disabled())
                  .readOnly(binding.readOnly())
                  .ref(editor));
        editor_id = f.inputs.mounted_inputs().back().component;
        binding.control(editor);
    }

    void sync() {
        check(f.services.layout_and_synchronize({720, 900}, {0, 0, 720, 900}), "bound-control layout");
    }

    void restrict(bool modal = false, bool only_sibling = false) {
        if (modal) {
            modal_open.set(true);
            sync();
        }
        std::vector roots{f.services.interactions().input_root(controls),
                          f.services.interactions().input_root(only_sibling ? sibling_id : editor_id)};
        if (modal) {
            companion = f.services.modal().register_tour_controls(
                f.services.interactions().input_root(form_id),
                {f.services.interactions().input_root(controls), f.services.interactions().input_root(editor_id)});
            roots.push_back(f.services.interactions().input_root(modal_id));
            const std::array updates{
                input::InputConstraintUpdate{allow, input::InputConstraintSpec{std::move(roots), true}}};
            check(companion.update(true, updates), "bound-control companion publication");
        } else {
            auto batch = f.services.interactions().begin_input_constraint_update();
            batch.replace(allow, {std::move(roots), true});
            check(f.services.publish_input_constraints(batch), "bound-control publication");
        }
    }

    input::TextInputSessionStamp edit(InputRef ref, String value) {
        check(ref.focus(), "bound-control native focus");
        const auto stamp = f.inputs.sessions().active();
        static_cast<void>(f.inputs.editors().require(stamp.owner).select_all());
        check(bool(f.inputs.dispatch(input::TextCommitted{std::move(value), stamp})), "bound-control native edit");
        return stamp;
    }

    void enter() {
        f.services.focus().dispatch(input::KeyboardInputEvent{input::Key::enter, input::KeyAction::down});
        f.services.focus().dispatch(input::KeyboardInputEvent{input::Key::enter, input::KeyAction::up});
    }

    void clean() {
        f.services.dispose();
        const auto d = f.services.interactions().input_constraint_diagnostics();
        check(f.services.components().component_count() == 0 && f.nodes.size() == 0 &&
                  f.services.interactions().size() == 0 && !f.inputs.sessions().active().valid() && d.leases == 0 &&
                  d.companions == 0 && timeline_test::blocked_roots(f.services.interactions()).empty(),
              "bound-control retirement leaked resources");
    }
};

void admitted(bool modal, bool nested) {
    Domain d{nested};
    d.restrict(modal);
    check(!d.f.services.interactions().component_active(d.form_id) &&
              !d.f.services.interactions().component_active(d.item) &&
              d.f.services.interactions().component_active(d.editor_id),
          "ancestor gained receiver authority");
    d.edit(d.private_editor, u8"private changed");
    d.enter();
    check(d.finishes == 0 && d.outer.get() == String{u8"outer"} && !d.outer.meta().touched,
          "private editor borrowed outer binding/Enter");
    if (nested) {
        d.edit(d.nested_editor, u8"nested changed");
        d.enter();
        check(d.nested.get() == String{u8"nested changed"} && d.finishes == 1,
              "explicit nested Item contract regressed");
    }
    d.bare(u8"bare steal");
    check(d.target.get() == String{u8"target"} && !d.target.meta().touched, "bare callback borrowed native ticket");
    const auto stamp = d.edit(d.editor, u8"target changed");
    check(d.target.get() == String{u8"target changed"} && d.target.meta().touched && d.outer.get() == String{u8"outer"},
          "native bound control did not publish its exact field");
    d.enter();
    check(d.finishes == (nested ? 2 : 1), "bound control Enter lost host maintenance or submitted twice");
    const auto control = d.form.getFieldInstance(FormNamePath{u8"target"});
    check(control && control->focus() && control->geometry() && control->geometry()->eligible,
          "exact field instance lost native focus/geometry");
    check(!d.tail.focus() && !d.other_editor.focus() && !d.background.submit(), "background gained authority");
    const auto reset_stamp = d.f.inputs.sessions().active();
    check(bool(d.f.inputs.dispatch(input::CompositionChanged{String{u8"候选"}, {}, reset_stamp})),
          "bound-control preedit setup");
    check(d.form.reset() && !d.f.inputs.editors().require(reset_stamp.owner).composition().active &&
              d.target.get() == String{u8"target"} && !d.target.meta().touched,
          "normal whole-Form reset regressed");
    d.hidden.set(true);
    d.sync();
    d.change(u8"hidden");
    check(!d.editor.focus() && d.target.get() == String{u8"target"}, "hidden ticket survived");
    check(d.form.submit() == nested, "another exact Item lost or lent whole-Form maintenance");
    d.hidden.set(false);
    d.sync();
    d.disabled.set(true);
    d.sync();
    d.change(u8"disabled");
    check(!d.editor.focus() && d.target.get() == String{u8"target"}, "disabled ticket survived");
    d.disabled.set(false);
    d.readonly.set(true);
    d.sync();
    d.change(u8"readonly");
    check(d.target.get() == String{u8"target"}, "readonly ticket survived");
    d.readonly.set(false);
    d.sync();
    d.f.services.set_window_active(false);
    d.change(u8"inactive");
    check(!control->focus() && !d.form.submit() && d.target.get() == String{u8"target"},
          "inactive window retained native qualification");
    d.f.services.set_window_active(true);
    check(d.f.services.layout_and_synchronize({720, 900}, {0, 0, 0, 0}), "zero-clip layout");
    d.change(u8"zero clip");
    check(!control->focus() && d.target.get() == String{u8"target"}, "zero clip granted native maintenance");
    check(d.f.services.layout_and_synchronize({720, 900}, {0, 0, 1, 1}), "offscreen layout");
    d.change(u8"offscreen");
    check(!control->focus() && d.target.get() == String{u8"target"}, "offscreen geometry granted native maintenance");
    d.sync();
    check(d.f.services.components().set_branch_active(d.editor_id, false), "native branch deactivation");
    d.change(u8"inactive branch");
    check(d.target.get() == String{u8"target"} && !control->focus(), "inactive native branch retained ticket");
    check(d.f.services.components().set_branch_active(d.editor_id, true), "native branch restoration");
    d.sync();
    d.f.services.interactions().set_input_blocked(d.item, true);
    d.change(u8"legacy blocked");
    check(!d.editor.focus() && d.target.get() == String{u8"target"}, "legacy ancestor block bypassed");
    d.f.services.interactions().set_input_blocked(d.item, false);
    auto block = d.f.services.create_input_constraint(d.f.services.interactions().input_root(d.editor_id),
                                                      input::InputConstraintKind::BlockSubtrees);
    auto batch = d.f.services.interactions().begin_input_constraint_update();
    batch.replace(block, {{d.f.services.interactions().input_root(d.editor_id)}});
    check(d.f.services.publish_input_constraints(batch), "bound-control hard block publication");
    d.change(u8"owned blocked");
    check(d.target.get() == String{u8"target"} && !d.editor.focus(), "owned block bypassed");
    d.f.services.release_input_constraint(block);
    d.edit(d.editor, u8"restored");
    auto second = d.f.services.create_input_constraint(d.f.services.interactions().input_root(d.controls),
                                                       input::InputConstraintKind::RestrictToSubtrees);
    auto intersection = d.f.services.interactions().begin_input_constraint_update();
    intersection.replace(second, {{d.f.services.interactions().input_root(d.controls)}, true});
    check(d.f.services.publish_input_constraints(intersection), "second restriction publication");
    d.change(u8"intersection steal");
    check(!d.editor.focus() && d.target.get() == String{u8"restored"}, "second restriction bypassed");
    d.f.services.release_input_constraint(second);
    d.f.services.destroy(d.editor_id);
    d.change(u8"retired");
    d.bare(u8"retired bare");
    check(!d.editor.bound() && !control->focus() && d.target.get() == String{u8"restored"} &&
              !bool(d.f.inputs.dispatch(input::TextCommitted{String{u8"stale"}, stamp})),
          "native retirement kept an event/ref ticket");
    d.clean();
}

void sibling_is_not_binding() {
    Domain d;
    d.restrict(false, true);
    check(d.sibling.focus() && !d.editor.focus(), "sibling negative fixture missing");
    d.change(u8"borrowed");
    d.bare(u8"bare borrowed");
    d.enter();
    check(d.target.get() == String{u8"target"} && !d.target.meta().touched && d.finishes == 0 && !d.form.submit(),
          "unbound sibling maintained target field/Form/Enter");
    d.clean();
}

void callback_reentry() {
    for (int action = 0; action < 4; ++action) {
        Domain d;
        d.restrict();
        Scope scope;
        bool armed = true;
        auto observer = effect(scope, [&] {
            if (d.target.meta().touched && std::exchange(armed, false)) {
                if (action == 0) {
                    d.f.services.interactions().set_input_blocked(d.editor_id, true);
                }
                if (action == 1) {
                    check(d.form.reset(), "reentrant reset refused");
                }
                if (action == 2) {
                    d.f.services.destroy(d.editor_id);
                }
                if (action == 3) {
                    d.f.services.destroy(d.form_id);
                }
            }
        });
        d.edit(d.editor, u8"stale edit");
        check(d.target.get() == String{u8"target"} && d.changes == 0,
              "meta callback invalidation published stale edit");
        if (action == 0) {
            d.f.services.interactions().set_input_blocked(d.editor_id, false);
        }
        d.clean();
    }
}

void connection_replacement() {
    std::vector<FormValidationComplete> completions;
    std::vector<std::function<void()>> posts;
    int cancels{};
    Domain d{false, [&](auto& field) {
                 field.asyncValidator(
                     [&](String, FormValidationContext, FormValidationComplete complete) {
                         completions.push_back(std::move(complete));
                         return [&] {
                             ++cancels;
                         };
                     },
                     [&](auto post) { posts.push_back(std::move(post)); });
             }};
    d.restrict();
    check(d.form.submitAsync(), "bound-control async did not start");
    const auto old_binding = d.binding_ticket;
    const auto old_change = d.change;
    const auto old_blur = d.blur;
    const auto old_control = d.form.getFieldInstance(FormNamePath{u8"target"});
    const auto old_id = d.editor_id;
    d.f.services.destroy(old_id);
    check(cancels == 1 && !d.target.meta().validating, "native disconnect did not cancel pending validation");
    d.f.services.append_slot(d.region, Content{[&] { d.mount_editor(); }});
    d.sync();
    d.restrict();
    check(d.editor_id != old_id && d.editor.bound(), "native connection did not replace exact generation");
    bool old_adapter_rejected{};
    d.f.services.append_slot(d.region, Content{[&] {
                                 try {
                                     old_binding->control(FormControlTarget{[] { return true; }, [] {}, {}});
                                 } catch (const std::invalid_argument&) {
                                     old_adapter_rejected = true;
                                 }
                             }});
    check(old_adapter_rejected, "old adapter rewired a new native connection");
    old_change(u8"old binding");
    old_blur();
    check(d.target.get() == String{u8"target"} && !d.target.meta().touched && old_control && !old_control->focus(),
          "old binding/field instance borrowed new connection");
    completions.front()({});
    for (auto& post : std::exchange(posts, {})) {
        post();
    }
    check(d.finishes == 0, "old async completion crossed connection epoch");
    d.edit(d.editor, u8"new binding");
    d.enter();
    check(d.target.get() == String{u8"new binding"} && d.finishes == 0 && d.target.meta().validating &&
              completions.size() == 2,
          "new connection lost publish/Enter validation");
    completions.back()({});
    for (auto& post : std::exchange(posts, {})) {
        post();
    }
    check(d.finishes == 1, "new connection async submission lost authority");
    d.clean();
}

void controlled_reentry() {
    for (int action = 0; action < 5; ++action) {
        Signal<String> source{String{u8"target"}};
        Domain* current{};
        bool armed{};
        bool getter = action == 0;
        const auto intervene = [&] {
            if (!std::exchange(armed, false)) {
                return;
            }
            if (action == 0 || action == 1) {
                current->f.services.interactions().set_input_blocked(current->editor_id, true);
            } else if (action == 2) {
                check(current->form.reset(), "setter reset failed");
            } else if (action == 3) {
                check(current->form.setFieldValue(FormNamePath{u8"outer"}, FormValue{String{u8"consumer"}}),
                      "setter other-field mutation failed");
            } else {
                throw std::runtime_error("exact controlled setter failure");
            }
        };
        Domain d{false, [&](auto& field) {
                     field = FormField<String>{String{u8"target"}, bind([&] {
                                                   if (getter && armed) {
                                                       intervene();
                                                   }
                                                   return source.get();
                                               }),
                                               [&](String value) {
                                                   source.set(std::move(value));
                                                   if (!getter && armed) {
                                                       intervene();
                                                   }
                                               }};
                 }};
        current = &d;
        d.restrict();
        armed = true;
        bool threw{};
        try {
            d.change(u8"accepted setter side effect");
        } catch (const std::runtime_error& e) {
            threw = std::string_view{e.what()} == "exact controlled setter failure";
        }
        check(d.changes == 0 && threw == (action == 4), "controlled reentry notified a stale transaction");
        if (action == 0) {
            check(source.get() == String{u8"target"}, "revoked getter still reached setter");
        }
        if (action == 1) {
            check(source.get() == String{u8"accepted setter side effect"},
                  "setter side effect was falsely rolled back");
        }
        if (action == 2) {
            check(!d.target.meta().touched && source.get() == String{u8"target"}, "reset lost authority");
        }
        if (action == 3) {
            check(d.outer.get() == String{u8"consumer"}, "other-field consumer write was rolled back");
        }
        if (action == 4) {
            check(!d.form.bound() && d.background.bound(), "setter failure retired a foreign Form or kept owner");
        }
        if (action <= 1) {
            d.f.services.interactions().set_input_blocked(d.editor_id, false);
        }
        d.clean();
    }
}

void application_geometry_is_not_authority() {
    for (bool typed : {false, true}) {
        Fixture f;
        FormRef form;
        FormField<String> field{String{u8"initial"}};
        InputRef editor;
        runtime::ComponentId control;
        std::function<void(String)> change;
        f.services.mount(Content{[&] {
            Form(FormProps{}.ref(form), FormContent{[&] {
                     const FormControlTarget fake{[] { return true; }, [] {},
                                                  []() -> std::optional<FormControlGeometry> {
                                                      return FormControlGeometry{
                                                          {0, 0, 100, 30}, {0, 0, 600, 600}, true};
                                                  }};
                     FormItem(FormItemProps{}.field(u8"value", field).control(editor), FormItemContent{[&] {
                                  if (typed) {
                                      FormControlBinding<String> binding;
                                      change = binding.onChange();
                                      Input(InputProps{}.value(binding.value()).onChange(change).ref(editor));
                                      binding.control(fake);
                                  } else {
                                      change = field.onChange();
                                      Input(InputProps{}.value(field.value()).onChange(change).ref(editor));
                                  }
                                  control = f.inputs.mounted_inputs().back().component;
                              }});
                 }});
        }});
        check(f.services.layout_and_synchronize({600, 600}, {0, 0, 600, 600}), "fake geometry fixture layout");
        auto restriction = f.services.create_input_constraint(f.services.interactions().input_root(control),
                                                              input::InputConstraintKind::RestrictToSubtrees);
        auto batch = f.services.interactions().begin_input_constraint_update();
        batch.replace(restriction, {{f.services.interactions().input_root(control)}, true});
        check(f.services.publish_input_constraints(batch) && editor.focus(), "fake geometry actual receiver missing");
        change(u8"forged");
        check(field.get() == String{u8"initial"} && !field.meta().touched && !form.submit(),
              "application geometry/explicit binding forged native maintenance");
        f.services.dispose();
        check(f.nodes.size() == 0 && f.services.interactions().size() == 0, "fake geometry cleanup leaked");
    }
}

void blur_ticket() {
    int validations{};
    Domain d{false, [&](auto& field) {
                 field.validation(FormValidateTrigger::Blur, 1).validator([&](String) {
                     ++validations;
                     return String{};
                 });
             }};
    d.restrict();
    d.target.onBlur()();
    check(!d.f.services.form().next_auxiliary_deadline(), "bare blur borrowed native ticket");
    d.blur();
    const auto due = d.f.services.form().next_auxiliary_deadline();
    check(bool(due), "typed native blur lost debounce");
    static_cast<void>(d.f.services.form().tick_auxiliary(*due));
    check(validations == 1, "typed native blur did not validate exact field");
    d.blur();
    check(bool(d.f.services.form().next_auxiliary_deadline()), "second blur did not schedule");
    d.f.services.destroy(d.editor_id);
    d.blur();
    check(!d.f.services.form().next_auxiliary_deadline(), "retired blur kept validation due");
    d.clean();
}

void foreign_native_identity() {
    Fixture foreign;
    FormField<String> foreign_field{String{u8"foreign"}};
    InputRef foreign_ref;
    foreign.services.mount(Content{[&] {
        Form(FormProps{}, FormContent{[&] {
                 FormItem(FormItemProps{}.field(u8"value", foreign_field), FormItemContent{[&] {
                              Input(InputProps{}.value(String{u8"foreign"}).onChange([](String) {}).ref(foreign_ref));
                          }});
             }});
    }});
    check(foreign.services.layout_and_synchronize({600, 600}, {0, 0, 600, 600}), "foreign identity layout");
    Fixture f;
    FormRef form;
    FormField<String> field{String{u8"local"}};
    InputRef local_ref;
    runtime::ComponentId control;
    bool rejected{};
    std::function<void(String)> change;
    f.services.mount(Content{[&] {
        Form(FormProps{}.ref(form), FormContent{[&] {
                 FormItem(FormItemProps{}.field(u8"value", field), FormItemContent{[&] {
                              FormControlBinding<String> binding;
                              change = binding.onChange();
                              Input(InputProps{}.value(binding.value()).onChange(change).ref(local_ref));
                              control = f.inputs.mounted_inputs().back().component;
                              try {
                                  binding.control(foreign_ref);
                              } catch (const std::invalid_argument&) {
                                  rejected = true;
                              }
                              binding.control(local_ref);
                          }});
             }});
    }});
    check(rejected && control == foreign.inputs.mounted_inputs().back().component,
          "foreign window same-id fixture failed to reject native identity");
    check(f.services.layout_and_synchronize({600, 600}, {0, 0, 600, 600}), "local identity layout");
    auto lease =
        f.services.create_input_constraint(f.services.interactions().input_root(f.services.form().mounted().front()),
                                           input::InputConstraintKind::RestrictToSubtrees);
    auto batch = f.services.interactions().begin_input_constraint_update();
    batch.replace(lease, {{f.services.interactions().input_root(control)}, true});
    check(f.services.publish_input_constraints(batch), "local identity restriction");
    change(u8"local changed");
    check(field.get() == String{u8"local changed"} && foreign_field.get() == String{u8"foreign"},
          "rejected native attach poisoned exact local binding or wrote foreign field");
    f.services.dispose();
    foreign.services.dispose();
    check(f.nodes.size() == 0 && foreign.nodes.size() == 0 && f.services.interactions().size() == 0 &&
              foreign.services.interactions().size() == 0,
          "foreign native identity cleanup leaked");
}

void boolean_controls() {
    for (bool use_switch : {false, true}) {
        Fixture f;
        detail::SelectionComponentHost selections{f.services};
        FormRef form;
        FormField<bool> field{false};
        CheckboxRef checkbox;
        SwitchRef toggle;
        runtime::ComponentId control;
        runtime::ComponentId item;
        std::function<void(bool)> change;
        f.services.mount(Content{[&] {
            Form(FormProps{}.ref(form), FormContent{[&] {
                     FormItem(FormItemProps{}.field(u8"value", field), FormItemContent{[&] {
                                  FormControlBinding<bool> binding;
                                  change = binding.onChange();
                                  if (use_switch) {
                                      Switch(SwitchProps{}.checked(binding.value()).onChange(change).ref(toggle));
                                      binding.control(toggle);
                                  } else {
                                      Checkbox(CheckboxProps{}.checked(binding.value()).onChange(change).ref(checkbox));
                                      binding.control(checkbox);
                                  }
                                  control = selections.mounted().back().component;
                              }});
                 }});
        }});
        check(f.services.layout_and_synchronize({600, 600}, {0, 0, 600, 600}), "boolean binding layout");
        item = f.services.form().items().front();
        auto restriction = f.services.create_input_constraint(
            f.services.interactions().input_root(f.services.form().mounted().front()),
            input::InputConstraintKind::RestrictToSubtrees);
        auto batch = f.services.interactions().begin_input_constraint_update();
        batch.replace(restriction, {{f.services.interactions().input_root(control)}, true});
        check(f.services.publish_input_constraints(batch) && !f.services.interactions().component_active(item),
              "boolean native-only fixture not constrained");
        check(use_switch ? toggle.focus() : checkbox.focus(), "native boolean focus failed");
        timeline_test::click(f, control);
        check(field.get() && field.meta().touched, "native boolean binding did not publish under exact domain");
        field.onChange()(false);
        check(field.get(), "bare boolean callback borrowed native qualification");
        f.services.destroy(control);
        change(false);
        check(field.get() && !form.submit(), "retired boolean binding remained authoritative");
        f.services.dispose();
        check(f.services.interactions().size() == 0 && timeline_test::blocked_roots(f.services.interactions()).empty(),
              "boolean native binding cleanup leaked");
    }
}

} // namespace

int main() {
    try {
        const auto run = [](const char* name, auto action) {
            const int before = checks;
            action();
            std::cout << name << ": " << checks - before << " checks passed" << std::endl;
        };
        run("pure-domain/no-nested", [] { admitted(false, false); });
        run("pure-domain/nested", [] { admitted(false, true); });
        run("Modal-companion/no-nested", [] { admitted(true, false); });
        run("Modal-companion/nested", [] { admitted(true, true); });
        run("sibling-negative", sibling_is_not_binding);
        run("meta-reentry", callback_reentry);
        run("connection-replacement", connection_replacement);
        run("controlled-reentry", controlled_reentry);
        run("application-geometry-negative", application_geometry_is_not_authority);
        run("blur-ticket", blur_ticket);
        run("foreign-native-identity", foreign_native_identity);
        run("native-boolean-controls", boolean_controls);
        std::cout << checks << " Form bound-control checks passed\n";
    } catch (const std::exception& e) {
        std::cerr << "check " << checks << ": " << e.what() << '\n';
        return 1;
    }
}
