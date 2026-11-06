#include "component/form_component.hpp"
#include "component/form_control_binding.hpp"
#include "component/modal_component.hpp"
#include "support/timeline_fixture.hpp"
#include <array>
#include <utility>
#include <iostream>

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
    FormField<String> other{String{u8"other"}};
    InputRef editor;
    InputRef private_editor;
    InputRef tail;
    InputRef other_editor;
    InputRef modal_target;
    Signal<bool> modal_open{false};
    Signal<bool> hidden{false};
    Signal<bool> disabled{false};
    runtime::ComponentId form_id;
    runtime::ComponentId controls;
    runtime::ComponentId item;
    runtime::ComponentId editor_id;
    runtime::ComponentId target_id;
    runtime::ComponentId tail_id;
    input::InputConstraintLease allow;
    detail::ModalInputCompanion companion;
    std::function<void(String)> saved_change;
    int finishes{};
    int changes{};

    explicit Domain(bool named = true, std::function<void(FormField<String>&)> configure = {}) {
        if (configure) {
            configure(nested);
        }
        f.services.mount(Content{[&] {
            Form(
                FormProps{}
                    .ref(form)
                    .onFinish([&](FormResult) { ++finishes; })
                    .onValuesChange([&](FormFieldValue, FormData) { ++changes; }),
                FormContent{[&] {
                    FormItem(
                        FormItemProps{}.field(u8"outer", outer), FormItemContent{[&] {
                            Flex(
                                FlexProps{}.vertical(true), FlexContent{[&] {
                                    detail::FormPrivateEditorScope scope;
                                    Input(InputProps{}.defaultValue(u8"private").ref(private_editor));
                                    controls =
                                        *f.services.components().parent(f.inputs.mounted_inputs().back().component);
                                    if (named) {
                                        FormItem(
                                            FormItemProps{}.field(u8"nested", nested).hidden(hidden).disabled(disabled),
                                            FormItemContent{[&] {
                                                saved_change = nested.onChange();
                                                Input(InputProps{}.ref(editor));
                                                editor_id = f.inputs.mounted_inputs().back().component;
                                            }});
                                    }
                                }});
                            Input(InputProps{}.ref(tail));
                            tail_id = f.inputs.mounted_inputs().back().component;
                        }});
                }});
            Form(FormProps{}.ref(background), FormContent{[&] {
                     FormItem(FormItemProps{}.field(u8"other", other),
                              FormItemContent{[&] { Input(InputProps{}.ref(other_editor)); }});
                 }});
            Modal(ModalProps{}.open(modal_open).footer(ModalFooterMode::None), ModalContent{[&] {
                      Input(InputProps{}.defaultValue(u8"target").ref(modal_target));
                      target_id = f.inputs.mounted_inputs().back().component;
                  }});
        }});
        sync();
        form_id = f.services.form().mounted().front();
        if (named) {
            for (auto id : f.services.form().items()) {
                if (f.services.components().parent(id) == controls) {
                    item = id;
                }
            }
            check(item.valid(), "nested registration fixture missing");
        }
        const auto root = f.services.interactions().input_root(controls);
        allow = f.services.create_input_constraint(root, input::InputConstraintKind::RestrictToSubtrees);
    }

    void sync() {
        check(f.services.layout_and_synchronize({720, 800}, {0, 0, 720, 800}), "domain layout");
    }

    void restrict(bool modal = false) {
        if (modal) {
            modal_open.set(true);
            sync();
            auto root = f.services.interactions().input_root(controls);
            companion = f.services.modal().register_tour_controls(root, {root});
        }
        input::InputConstraintSpec spec{{f.services.interactions().input_root(controls)}, true};
        if (modal) {
            spec.roots.push_back(f.services.interactions().input_root(target_id));
            const std::array changes{input::InputConstraintUpdate{allow, std::move(spec)}};
            check(companion.update(true, changes), "atomic companion/domain publication");
        } else {
            auto batch = f.services.interactions().begin_input_constraint_update();
            batch.replace(allow, std::move(spec));
            check(f.services.publish_input_constraints(batch), "domain publication");
        }
    }

    input::TextInputSessionStamp edit(InputRef ref, String value) {
        check(ref.focus(), "domain focus");
        auto stamp = f.inputs.sessions().active();
        static_cast<void>(f.inputs.editors().require(stamp.owner).select_all());
        check(bool(f.inputs.dispatch(input::TextCommitted{std::move(value), stamp})), "domain commit");
        return stamp;
    }

    void enter() {
        f.services.focus().dispatch(input::KeyboardInputEvent{input::Key::enter, input::KeyAction::down});
        f.services.focus().dispatch(input::KeyboardInputEvent{input::Key::enter, input::KeyAction::up});
    }

    void clean() {
        f.services.dispose();
        const auto diagnostics = f.services.interactions().input_constraint_diagnostics();
        check(f.services.components().component_count() == 0 && f.nodes.size() == 0 &&
                  f.services.interactions().size() == 0 && !f.inputs.sessions().active().valid() &&
                  diagnostics.leases == 0 && diagnostics.companions == 0 &&
                  timeline_test::blocked_roots(f.services.interactions()).empty(),
              "domain retirement leaked resources");
    }
};

void admitted(bool modal) {
    Domain d;
    d.restrict(modal);
    check(!d.f.services.interactions().component_active(d.form_id) &&
              d.f.services.interactions().subtree_intersects_input_domain(d.item),
          "scope/input receiving distinction");
    check(bool(d.f.services.modal().effective_input_domain()) == modal, "pure restriction unexpectedly needs Modal");
    d.edit(d.private_editor, u8"private changed");
    d.enter();
    check(d.finishes == 0 && d.outer.get() == String{u8"outer"} && !d.outer.meta().touched,
          "private editor claimed Form binding/Enter");
    const auto stamp = d.edit(d.editor, u8"accepted");
    check(d.nested.get() == String{u8"accepted"} && d.nested.meta().touched && d.changes == 1 &&
              d.outer.get() == String{u8"outer"},
          "legal explicit Item did not publish exact field");
    d.enter();
    check(d.finishes == 1, "explicit Item Enter did not submit its Form");
    if (modal) {
        auto batch = d.f.services.interactions().begin_input_constraint_update();
        batch.replace(d.allow, {{d.f.services.interactions().input_root(d.controls),
                                 d.f.services.interactions().input_root(d.tail_id)},
                                true});
        check(d.f.services.publish_input_constraints(batch) && !d.tail.focus() && d.editor.focus() &&
                  !d.modal_target.focus(),
              "companion granted a target outside the actual Modal intersection");
    }
    check(!d.tail.focus() && !d.other_editor.focus() && !d.background.submit(),
          "scope authorized background input/Form");
    check(d.form.reset() && d.nested.get() == String{u8"nested"} && d.outer.get() == String{u8"outer"},
          "scope lost whole Form reset");
    d.hidden.set(true);
    d.sync();
    d.saved_change(u8"hidden stale");
    check(!d.editor.focus() && !d.form.submit() && d.nested.get() == String{u8"nested"},
          "hidden Item kept Form authority");
    d.hidden.set(false);
    d.sync();
    d.disabled.set(true);
    d.sync();
    d.saved_change(u8"disabled stale");
    check(d.nested.get() == String{u8"nested"} && !d.editor.focus(), "disabled Item accepted input");
    d.disabled.set(false);
    d.sync();
    d.edit(d.editor, u8"restored");
    auto block = d.f.services.create_input_constraint(d.f.services.interactions().input_root(d.controls),
                                                      input::InputConstraintKind::BlockSubtrees);
    auto batch = d.f.services.interactions().begin_input_constraint_update();
    batch.replace(block, {{d.f.services.interactions().input_root(d.controls)}});
    check(d.f.services.publish_input_constraints(batch), "independent block publication");
    d.saved_change(u8"blocked stale");
    check(!d.form.submit() && !d.editor.focus() && d.nested.get() == String{u8"restored"} &&
              !bool(d.f.inputs.dispatch(input::TextCommitted{String{u8"stale"}, stamp})),
          "full intersection ignored block/text epoch");
    d.f.services.release_input_constraint(block);
    d.edit(d.editor, u8"unblocked");
    const auto stale = d.saved_change;
    const auto retired = d.item;
    d.f.services.destroy(retired);
    stale(u8"retired write");
    check(!d.editor.bound() && !d.form.submit() && d.nested.get() == String{u8"unblocked"},
          "last exact registration retirement kept authority");
    d.clean();
}

void retirement_during_edit() {
    Domain d;
    d.restrict();
    Scope observers;
    bool armed = true;
    auto observer = effect(observers, [&] {
        if (d.nested.meta().touched && std::exchange(armed, false)) {
            d.f.services.destroy(d.controls);
        }
    });
    d.edit(d.editor, u8"retired mid-write");
    check(!d.editor.bound() && !d.f.inputs.sessions().active().valid() && d.nested.get() == String{u8"nested"} &&
              d.changes == 0,
          "same-turn owner retirement published a stale field write");
    d.saved_change(u8"retired callback");
    check(d.nested.get() == String{u8"nested"}, "retired registration callback survived owned-domain cleanup");
    d.clean();
}

void async_domain_withdrawal() {
    std::vector<FormValidationComplete> completions;
    std::vector<std::function<void()>> posts;
    std::vector<std::shared_ptr<const std::atomic_bool>> tickets;
    int cancellations{};
    Domain d{true, [&](auto& field) {
                 field.asyncValidator(
                     [&](String, FormValidationContext context, FormValidationComplete complete) {
                         completions.push_back(std::move(complete));
                         tickets.push_back(context.cancelled);
                         return [&] {
                             ++cancellations;
                         };
                     },
                     [&](auto post) { posts.push_back(std::move(post)); });
             }};
    d.restrict();
    check(d.form.submitAsync() && d.nested.meta().validating, "owned-domain async transaction did not start");
    auto block = d.f.services.create_input_constraint(d.f.services.interactions().input_root(d.controls),
                                                      input::InputConstraintKind::BlockSubtrees);
    auto batch = d.f.services.interactions().begin_input_constraint_update();
    batch.replace(block, {{d.f.services.interactions().input_root(d.controls)}});
    check(d.f.services.publish_input_constraints(batch), "async domain block publication");
    check(cancellations == 1 && tickets.front()->load() && !d.nested.meta().validating,
          "owned-domain withdrawal did not cancel exact async transaction");
    completions.front()({});
    for (auto& post : std::exchange(posts, {})) {
        post();
    }
    check(d.finishes == 0, "old async result crossed owned domain withdrawal");
    d.f.services.release_input_constraint(block);
    check(d.form.submitAsync(), "restored owned domain did not start a new async transaction");
    completions.back()({});
    for (auto& post : std::exchange(posts, {})) {
        post();
    }
    check(d.finishes == 1 && !d.nested.meta().validating, "fresh owned-domain async completion lost authority");
    d.clean();
}

void private_only() {
    Domain d{false};
    d.restrict();
    d.edit(d.private_editor, u8"private");
    d.enter();
    check(!d.form.submit() && !d.form.validate() && d.finishes == 0 && d.changes == 0 &&
              d.outer.get() == String{u8"outer"} && !d.outer.meta().touched,
          "allowed private descendant qualified an ancestor registration");
    d.clean();
}
} // namespace

int main() {
    try {
        admitted(false);
        admitted(true);
        private_only();
        retirement_during_edit();
        async_domain_withdrawal();
        std::cout << checks << " Form input domain checks passed\n";
    } catch (const std::exception& e) {
        std::cerr << "check " << checks << ": " << e.what() << '\n';
        return 1;
    }
}
