#include "component/feedback_component.hpp"
#include "component/form_component.hpp"
#include "component/modal_component.hpp"
#include "component/select_component.hpp"
#include "component/window_overlay_stack.hpp"
#include "support/input_fixture.hpp"
#include "support/allocation_probe.hpp"
#include <algorithm>
#include <array>
#include <cstdio>
#include <cstdlib>
#include <iostream>
#include <limits>
#include <string_view>
#if defined(_MSC_VER)
#include <crtdbg.h>
#endif

namespace {
using namespace ryn;
using namespace ryn::input;
using Fixture = ryn_test::input_component::Fixture;

void suppress_dialogs() {
#if defined(_MSC_VER)
    _set_error_mode(_OUT_TO_STDERR);
    _set_abort_behavior(0, _CALL_REPORTFAULT);
#if defined(_DEBUG)
    _CrtSetReportMode(_CRT_ERROR, _CRTDBG_MODE_FILE);
    _CrtSetReportFile(_CRT_ERROR, _CRTDBG_FILE_STDERR);
    _CrtSetReportMode(_CRT_ASSERT, _CRTDBG_MODE_FILE);
    _CrtSetReportFile(_CRT_ASSERT, _CRTDBG_FILE_STDERR);
#endif
#endif
}

void check(bool value, const char* message) {
    if (!value) {
        throw std::runtime_error(message);
    }
}

void sync(Fixture& f) {
    check(f.services.layout_and_synchronize({720, 720}, {0, 0, 720, 720}), "overlay layout failed");
}

runtime::Rect bounds(Fixture& f, runtime::ComponentId id) {
    const auto& node = f.nodes.require(f.services.components().root(id));
    return {node.bounds.x + node.translation.x, node.bounds.y + node.translation.y, node.bounds.width,
            node.bounds.height};
}

void pointer(Fixture& f, PointerAction action, runtime::Point point) {
    f.services.pointer().dispatch({PointerIdentity::mouse(), action, PointerButton::primary, point.x, point.y});
}

void click(Fixture& f, runtime::ComponentId id) {
    const auto rect = bounds(f, id);
    const runtime::Point point{rect.x + rect.width / 2, rect.y + rect.height / 2};
    pointer(f, PointerAction::down, point);
    pointer(f, PointerAction::up, point);
    sync(f);
}

void key(Fixture& f, Key value, KeyModifier modifiers = KeyModifier::none) {
    for (const auto action : {KeyAction::down, KeyAction::up}) {
        f.services.focus().dispatch({value, action, modifiers});
    }
}

std::size_t paint_position(Fixture& f, runtime::ComponentId root) {
    const auto entries = f.services.components().paint_traversal();
    const auto position = std::ranges::find_if(entries, [&](const auto& entry) {
        for (auto current = std::optional{entry.component}; current;
             current = f.services.components().parent(*current)) {
            if (*current == root) {
                return true;
            }
        }
        return false;
    });
    check(position != entries.end(), "overlay has no actual painted fragment");
    return static_cast<std::size_t>(position - entries.begin());
}

runtime::ComponentId group(Fixture& f, Content content) {
    auto& build = runtime::require_component_build_context();
    const auto id = build.mount_component<int>(0);
    const auto node = build.root(id);
    f.layout.set_layout(node, layout::BoxLayout{{}, true});
    build.on_resource_cleanup(id, [&f, node] { f.layout.remove_layout(node); });
    build.mount_slot(id, content);
    return id;
}

void order() {
    Fixture f;
    Signal<bool> modal{false};
    Signal<bool> spin{false};
    InputRef background;
    InputRef editor;
    ButtonRef spin_ref;
    runtime::ComponentId modal_button;
    runtime::ComponentId spin_button;
    int modal_clicks{};
    int spin_clicks{};
    f.services.mount(Content{[&] {
        Flex(FlexProps{}.vertical(true), FlexContent{[&] {
                 Input(InputProps{}.ref(background));
                 Modal(ModalProps{}.open(modal).title(u8"Modal"), ModalContent{[&] {
                           Input(InputProps{}.ref(editor));
                           Button(ButtonProps{}.onClick([&] { ++modal_clicks; }), [] { Text(u8"Modal action"); });
                           modal_button = f.buttons.mounted_buttons().back().component;
                       }});
                 Spin(SpinProps{}.spinning(spin).fullscreen(true),
                      SpinSlots{.indicator = SpinIndicator{[&] {
                                    Button(ButtonProps{}.ref(spin_ref).onClick([&] { ++spin_clicks; }),
                                           [] { Text(u8"Spin action"); });
                                    spin_button = f.buttons.mounted_buttons().back().component;
                                }}});
             }});
    }});
    sync(f);
    const auto panel = f.services.modal().mounted().front();
    const auto spinner = f.services.feedback().spins().front();
    check(background.focus(), "background did not focus before opening");
    modal.set(true);
    sync(f);
    check(editor.focus() && !background.focus(), "Modal did not confine actual input");
    spin.set(true);
    sync(f);
    auto m = f.services.modal().snapshot(panel);
    auto s = f.services.feedback().spin_snapshot(spinner);
    check(paint_position(f, m.overlay) < paint_position(f, s.overlay) && !m.topmost &&
              f.services.interactions().input_boundary() == s.overlay,
          "Modal then Spin paint/input order diverged");
    click(f, modal_button);
    check(modal_clicks == 0 && spin_ref.focus() && !editor.focus(), "Spin did not freeze the Modal");
    click(f, spin_button);
    check(spin_clicks == 1, "top Spin custom control had an empty input domain");

    modal.set(false);
    modal.set(true);
    sync(f);
    m = f.services.modal().snapshot(panel);
    check(m.topmost && paint_position(f, s.overlay) < paint_position(f, m.overlay) && editor.focus() &&
              !spin_ref.focus(),
          "later reopened independent Modal did not cover Spin");
    click(f, modal_button);
    check(modal_clicks == 1, "reopened top Modal was not clickable");
    spin.set(false);
    spin.set(true);
    sync(f);
    check(spin_ref.focus() && !editor.focus(), "Spin reopening used mounted scan order");
    const auto focus_changes = f.services.focus().diagnostics().focus_changes;
    const auto publications = f.services.interactions().input_constraint_diagnostics().publications;
    for (int frame = 0; frame < 5; ++frame) {
        sync(f);
    }
    check(f.services.focus().diagnostics().focus_changes == focus_changes &&
              f.services.interactions().input_constraint_diagnostics().publications == publications,
          "stable overlay frames reselected authority or autofocus");
    key(f, Key::enter);
    check(spin_clicks == 2, "keyboard activation did not reach the painted Spin winner");
}

void domain() {
    Fixture f;
    Signal<bool> spin{false};
    InputRef editor;
    runtime::ComponentId area;
    runtime::ComponentId action;
    int clicks{};
    int submits{};
    int cancelled{};
    f.services.mount(Content{[&] {
        Modal(ModalProps{}.open(true).onCancel([&](ModalCancelReason) { ++cancelled; }), ModalContent{[&] {
                  Input(InputProps{}.ref(editor).defaultValue(u8"retained").onSubmit([&](String) { ++submits; }));
                  TextArea(TextAreaProps{}
                               .defaultValue(u8"one\ntwo\nthree\nfour\nfive\nsix\nseven\neight")
                               .layout(LayoutStyle{}.width(dp(200)).height(dp(56))));
                  area = f.inputs.mounted_inputs().back().component;
                  Button(ButtonProps{}.onClick([&] { ++clicks; }), [] { Text(u8"Held action"); });
                  action = f.buttons.mounted_buttons().back().component;
              }});
        Spin(SpinProps{}.fullscreen(true).spinning(spin),
             SpinSlots{.indicator = SpinIndicator{[] { Text(u8"Busy"); }}});
    }});
    sync(f);
    check(editor.focus(), "Modal editor did not focus");
    const auto action_bounds = bounds(f, action);
    const runtime::Point point{action_bounds.x + action_bounds.width / 2, action_bounds.y + action_bounds.height / 2};
    pointer(f, PointerAction::down, point);
    check(editor.focus(), "captured pointer setup did not restore the editor session");
    const auto stamp = f.inputs.sessions().active();
    check(bool(f.inputs.dispatch(CompositionChanged{String{u8"ni"}, {2, 0}, stamp})), "preedit setup failed");
    spin.set(true);
    sync(f);
    pointer(f, PointerAction::up, point);
    check(clicks == 0 && !editor.focus() && !f.inputs.sessions().active().valid(),
          "Spin retained the Modal pointer press/editor session");
    check(!bool(f.inputs.dispatch(TextCommitted{String{u8"forbidden"}, stamp})) &&
              !bool(f.inputs.dispatch(CompositionChanged{String{u8"stale"}, {0, 0}, stamp})),
          "old text/composition route crossed the new winner");
    const auto old_scroll = f.inputs.layout_snapshot(area).vertical_scroll;
    const auto viewport = f.inputs.layout_snapshot(area).viewport;
    check(bool(f.inputs.dispatch(ScrollInputEvent{0, -1, viewport.x + 3, viewport.y + 3})),
          "fullscreen winner did not consume background wheel");
    check(f.inputs.layout_snapshot(area).vertical_scroll == old_scroll, "background editor scrolled through Spin");
    key(f, Key::escape);
    key(f, Key::enter);
    key(f, Key::tab);
    check(cancelled == 0 && submits == 0 && !f.services.focus().state().focused,
          "Modal Escape/Enter/Tab escaped the nonfocusable Spin winner");
    const auto spinner = f.services.feedback().spin_snapshot(f.services.feedback().spins().front());
    check(f.services.hit_test().hit_test({719, 719}) == spinner.shield, "Spin shield was blocked by a second lease");
    spin.set(false);
    sync(f);
    check(editor.focus() && bool(f.inputs.dispatch(TextCommitted{String{u8"!"}, f.inputs.sessions().active()})),
          "Modal input did not resume after the shared shield closed");
    const auto focused = f.services.focus().state().focused;
    const auto changes = f.services.focus().diagnostics().focus_changes;
    sync(f);
    sync(f);
    check(f.services.focus().state().focused == focused && f.services.focus().diagnostics().focus_changes == changes,
          "focus restoration ran more than once");
}

void nested() {
    Fixture f;
    Signal<bool> delayed{false};
    Signal<bool> later{false};
    InputRef first;
    InputRef remembered;
    InputRef second;
    ButtonRef indicator;
    f.services.mount(Content{[&] {
        Modal(ModalProps{}.open(true).title(u8"First"), ModalContent{[&] {
                  Input(InputProps{}.ref(first));
                  Input(InputProps{}.ref(remembered));
                  Select(SelectProps{}.options({{1, u8"One"}, {2, u8"Two"}}).showSearch());
                  Spin(SpinProps{}.fullscreen(true).spinning(delayed).delay(std::chrono::milliseconds{100}),
                       SpinSlots{.indicator = SpinIndicator{
                                     [&] { Button(ButtonProps{}.ref(indicator), [] { Text(u8"Nested Spin"); }); }}});
              }});
        Modal(ModalProps{}.open(later).title(u8"Later"), ModalContent{[&] { Input(InputProps{}.ref(second)); }});
    }});
    sync(f);
    check(remembered.focus(), "nested remembered editor did not focus");
    delayed.set(true);
    later.set(true);
    sync(f);
    check(second.focus(), "later independent Modal did not focus");
    static_cast<void>(f.services.tick_animations(animation::AnimationTime::microseconds(150000)));
    sync(f);
    const auto spinner = f.services.feedback().spin_snapshot(f.services.feedback().spins().front());
    const auto second_panel = f.services.modal().snapshot(f.services.modal().mounted().back());
    check(spinner.spinning && second_panel.topmost && second.focus() && !indicator.focus() &&
              paint_position(f, spinner.overlay) < paint_position(f, second_panel.overlay),
          "delayed nested Spin crossed a newer independent Modal family");
    later.set(false);
    sync(f);
    check(indicator.focus() && !remembered.focus(), "nested Spin did not become the family's winner");
    delayed.set(false);
    sync(f);
    const auto saved = f.inputs.mounted_inputs()[1].interaction;
    check(f.services.focus().state().focused == saved && !second.focus(),
          "nested Spin restored another window/root instead of the remembered parent focus");
}

void companion() {
    Fixture f;
    Signal<bool> spin{false};
    Signal<bool> disabled{true};
    FormField<String> field{String{u8"blocked"}};
    InputRef target;
    InputRef blocked;
    InputRef controls;
    runtime::ComponentId target_id;
    runtime::ComponentId controls_id;
    f.services.mount(Content{[&] {
        Modal(ModalProps{}.open(true), ModalContent{[&] {
                  Input(InputProps{}.ref(target));
                  target_id = f.inputs.mounted_inputs().back().component;
                  Form(FormProps{}, FormContent{[&] {
                           FormItem(FormItemProps{}.field(u8"blocked", field).disabled(disabled),
                                    FormItemContent{[&] { Input(InputProps{}.ref(blocked)); }});
                       }});
              }});
        Input(InputProps{}.ref(controls));
        controls_id = f.inputs.mounted_inputs().back().component;
        Spin(SpinProps{}.fullscreen(true).spinning(spin),
             SpinSlots{.indicator = SpinIndicator{[] { Text(u8"Busy"); }}});
    }});
    sync(f);
    const auto root = [&](runtime::ComponentId id) {
        return f.services.interactions().input_root(id);
    };
    const auto spinner = f.services.feedback().spin_snapshot(f.services.feedback().spins().front());
    auto allow = f.services.create_input_constraint(root(controls_id), InputConstraintKind::RestrictToSubtrees);
    auto grant = f.services.modal().register_tour_controls(root(controls_id), {root(controls_id)});
    const std::array changes{InputConstraintUpdate{
        allow, InputConstraintSpec{{root(target_id), root(controls_id), root(spinner.overlay)}, true}}};
    grant.update(true, changes);
    check(target.focus() && controls.focus() && !blocked.focus(), "Modal companion/Form setup failed");
    spin.set(true);
    sync(f);
    check(!controls.focus() && !target.focus() && !blocked.focus() &&
              f.services.hit_test().hit_test({719, 719}) == spinner.shield,
          "an old Modal companion pierced the fullscreen shield");
    check(allow.valid(), "overlay selection retired an independent Tour restriction");
    spin.set(false);
    sync(f);
    check(target.focus() && controls.focus() && !blocked.focus(),
          "Modal companion did not reauthorize or Form block was lost");
    const std::array close{InputConstraintUpdate{allow, std::nullopt}};
    grant.update(false, close);
    disabled.set(false);
    sync(f);
    check(blocked.focus() && !controls.focus(), "Form's own release or Modal companion retirement was ignored");
}

void focus() {
    Fixture f;
    Signal<bool> open{false};
    Signal<bool> spin{false};
    InputRef background;
    InputRef modal;
    InputRef first_indicator;
    InputRef chosen_indicator;
    bool choose_on_blur{};
    f.services.mount(Content{[&] {
        Input(InputProps{}.ref(background));
        Modal(ModalProps{}.open(open).focusTrap(false), ModalContent{[&] {
                  Input(InputProps{}.ref(modal).onBlur([&] {
                      if (choose_on_blur) {
                          choose_on_blur = false;
                          static_cast<void>(chosen_indicator.focus());
                      }
                  }));
              }});
        Spin(SpinProps{}.fullscreen(true).spinning(spin), SpinSlots{.indicator = SpinIndicator{[&] {
                                                                        Input(InputProps{}.ref(first_indicator));
                                                                        Input(InputProps{}.ref(chosen_indicator));
                                                                    }}});
    }});
    sync(f);
    open.set(true);
    sync(f);
    check(!f.services.interactions().input_boundary() && background.focus() && modal.focus(),
          "nontrapping Modal introduced a confinement lease");
    choose_on_blur = true;
    spin.set(true);
    sync(f);
    const auto chosen = f.inputs.mounted_inputs().back().interaction;
    check(f.services.focus().state().focused == chosen && !background.focus(),
          "overlay autofocus replaced a newer explicit blur focus intent");
    const auto changes = f.services.focus().diagnostics().focus_changes;
    sync(f);
    check(f.services.focus().state().focused == chosen && f.services.focus().diagnostics().focus_changes == changes,
          "stable frame replayed the retired focus ticket");
    open.set(false);
    open.set(true);
    sync(f);
    check(f.services.modal().snapshot(f.services.modal().mounted().front()).topmost && modal.focus() &&
              !chosen_indicator.focus() && f.services.interactions().input_boundary().has_value(),
          "nontrapping newer Modal failed to preserve an underlying shield boundary");
    spin.set(false);
    sync(f);
    check(!f.services.interactions().input_boundary() && background.focus(),
          "last shield release converted a nontrapping Modal into a trap");
}

void drawer() {
    Fixture f;
    Signal<bool> child{false};
    Signal<bool> grandchild{false};
    Signal<bool> independent{false};
    InputRef editor;
    runtime::ComponentId editor_id;
    runtime::ComponentId independent_button;
    int clicks{};
    f.services.mount(Content{[&] {
        Modal(ModalProps{}.open(true), ModalContent{[&] {
                  Drawer(DrawerProps{}.open(child).width(360), DrawerContent{[&] {
                             Input(InputProps{}.ref(editor));
                             editor_id = f.inputs.mounted_inputs().back().component;
                             Drawer(DrawerProps{}.open(grandchild).width(220).push(DrawerPush{true, 40}),
                                    DrawerContent{[] { Text(u8"Nested drawer"); }});
                         }});
              }});
        Drawer(DrawerProps{}.open(independent).width(180), DrawerContent{[&] {
                   Button(ButtonProps{}.onClick([&] { ++clicks; }), [] { Text(u8"Independent"); });
                   independent_button = f.buttons.mounted_buttons().back().component;
               }});
    }});
    sync(f);
    const auto ids = f.services.modal().mounted();
    const auto middle = ids[1];
    const auto separate = ids[3];
    child.set(true);
    sync(f);
    const auto original = f.services.modal().snapshot(middle).bounds;
    const auto original_editor = bounds(f, editor_id);
    grandchild.set(true);
    sync(f);
    check(f.services.modal().snapshot(middle).bounds.x == original.x - 40 &&
              bounds(f, editor_id).x == original_editor.x - 40,
          "nested Drawer push did not move its exact parent and real input together");
    independent.set(true);
    sync(f);
    const auto separate_bounds = f.services.modal().snapshot(separate).bounds;
    click(f, independent_button);
    check(clicks == 1 && !editor.focus() && f.services.modal().snapshot(middle).bounds.x == original.x - 40,
          "independent Drawer changed another family's push or lost the input winner");
    grandchild.set(false);
    sync(f);
    check(f.services.modal().snapshot(middle).bounds == original &&
              f.services.modal().snapshot(separate).bounds == separate_bounds,
          "child close pushed an unrelated Drawer or failed to restore its parent");
    independent.set(false);
    sync(f);
    check(editor.focus(), "closing independent Drawer did not restore the surviving nested family");
}

void reentry() {
    Fixture f;
    Signal<bool> first_open{false};
    Signal<bool> second_open{false};
    Signal<bool> spin{false};
    bool open_on_blur{};
    InputRef first;
    InputRef second;
    runtime::ComponentId first_owner;
    int closes{};
    f.services.mount(Content{[&] {
        Modal(ModalProps{}.open(first_open), ModalContent{[&] {
                  Input(InputProps{}.ref(first).onBlur([&] {
                      if (open_on_blur) {
                          open_on_blur = false;
                          second_open.set(true);
                      }
                  }));
              }});
        Modal(ModalProps{}.open(second_open).afterClose([&] {
            ++closes;
            if (first_owner.valid()) {
                f.services.destroy(first_owner);
                first_owner = {};
            }
        }),
              ModalContent{[&] { Input(InputProps{}.ref(second)); }});
        Spin(SpinProps{}.spinning(spin).fullscreen(true),
             SpinSlots{.indicator = SpinIndicator{[] { Text(u8"Busy"); }}});
    }});
    first_owner = f.services.modal().mounted().front();
    sync(f);
    first_open.set(true);
    sync(f);
    check(first.focus(), "reentry first editor did not focus");
    open_on_blur = true;
    spin.set(true);
    sync(f);
    check(second_open.get() && second.focus() && !first.focus(), "blur-opened newer Modal lost its focus ticket");
    second_open.set(false);
    sync(f);
    check(closes == 1 && !first.bound() && !f.services.focus().state().focused,
          "afterClose owner destruction reused stale state or restored blocked focus");
    spin.set(false);
    sync(f);
    f.services.dispose();
    check(f.services.interactions().input_constraint_diagnostics().leases == 0 && !f.inputs.sessions().active().valid(),
          "reentrant close/dispose leaked input resources");
}

enum class RollbackPath { spin, modal, companion_spin, close_previous, close_last };

struct FaultResult final {
    std::size_t allocations{};
    bool failed{};
};

FaultResult rollback_attempt(RollbackPath path, std::size_t failure) {
    Fixture f;
    std::array<runtime::ComponentId, 4> owners;
    std::array<runtime::ComponentId, 4> buttons;
    std::array<ButtonRef, 4> refs;
    std::array<int, 4> clicks{};
    f.services.mount(Content{[&] {
        Flex(FlexProps{}.vertical(true), FlexContent{[&] {
                 for (std::size_t i = 0; i < owners.size(); ++i) {
                     owners[i] = group(f, Content{[&, i] {
                                           Button(ButtonProps{}.ref(refs[i]).onClick([&, i] { ++clicks[i]; }),
                                                  [] { Text(u8"Action"); });
                                           buttons[i] = f.buttons.mounted_buttons().back().component;
                                       }});
                 }
             }});
    }});
    sync(f);
    check(refs[2].focus(), "rollback trigger did not focus");
    auto& registry = f.services.interactions();
    const auto root = [&](std::size_t i) {
        return registry.input_root(owners[i]);
    };
    auto independent = f.services.create_input_constraint(root(2), path == RollbackPath::close_last
                                                                       ? InputConstraintKind::BlockSubtrees
                                                                       : InputConstraintKind::RestrictToSubtrees);
    auto block = f.services.create_input_constraint(root(3), InputConstraintKind::BlockSubtrees);
    auto setup = registry.begin_input_constraint_update();
    if (path == RollbackPath::close_last) {
        setup.replace(independent, {{root(3)}});
    } else {
        setup.replace(independent, {{root(0), root(1), root(2)}, true});
    }
    setup.replace(block, {{root(3)}});
    static_cast<void>(f.services.publish_input_constraints(setup));

    detail::WindowOverlayStack stack{f.services};
    const bool has_companion = path != RollbackPath::spin;
    std::array<detail::WindowOverlayTicket, 2> tickets;
    for (std::size_t i = 0; i < tickets.size(); ++i) {
        const auto kind = !has_companion || (i == 1 && path == RollbackPath::companion_spin)
                              ? detail::WindowOverlayKind::FullscreenSpin
                              : detail::WindowOverlayKind::Modal;
        tickets[i] = stack.add(root(i), root(i), kind);
        detail::WindowOverlayState state;
        state.input_visible = i == 0;
        state.paint_visible = i == 0;
        stack.update(tickets[i], state);
    }
    static_cast<void>(stack.reconcile(true));
    sync(f);
    stack.synchronize_focus();
    detail::WindowOverlayCompanion grant;
    if (has_companion) {
        grant = stack.register_tour_controls(root(2), {root(2)}, true);
        check(refs[2].focus(), "rollback companion was not authorized");
    }
    check(refs[0].focus() && !refs[3].focus(), "rollback old domain/independent block setup failed");
    const bool closing_previous = path == RollbackPath::close_previous;
    if (closing_previous) {
        detail::WindowOverlayState opened;
        opened.input_visible = true;
        opened.paint_visible = true;
        stack.update(tickets[1], opened);
        static_cast<void>(stack.reconcile(true));
        sync(f);
        stack.synchronize_focus();
        check(refs[1].focus(), "rollback closing winner did not focus");
    }
    const std::size_t old_index = closing_previous ? 1 : 0;
    const auto rect = bounds(f, buttons[old_index]);
    const runtime::Point point{rect.x + rect.width / 2, rect.y + rect.height / 2};
    pointer(f, PointerAction::down, point);
    const auto old_interaction = f.buttons.mounted_buttons()[old_index].interaction;
    const auto held = f.services.pointer().state(PointerIdentity::mouse());
    check(held && held->primary_down && held->capture == old_interaction && held->press_origin == old_interaction,
          "rollback did not start a real captured Button press");
    const auto focus_before = f.services.focus().state();
    const auto focus_ticket = f.services.focus().focus_request_revision();
    const auto focus_changes = f.services.focus().diagnostics().focus_changes;
    const auto cancels = f.services.pointer().diagnostics().cancels;
    const auto top_before = stack.top();
    const auto domain_before = stack.input_domain();
    const auto boundary_before = registry.input_boundary();
    const auto revision = registry.input_boundary_revision();
    const auto diagnostics = registry.input_constraint_diagnostics();
    const auto ordered = stack.ordered();
    const std::vector<detail::WindowOverlayView> views_before(ordered.begin(), ordered.end());
    const auto paint = f.services.components().paint_traversal();
    const std::vector<runtime::SceneFragmentPaintEntry> paint_before(paint.begin(), paint.end());
    const std::array branches{f.services.components().branch_active(owners[0]),
                              f.services.components().branch_active(owners[1])};
    detail::WindowOverlayState next;
    const bool closing_last = path == RollbackPath::close_last;
    if (!closing_previous && !closing_last) {
        next.input_visible = true;
        next.paint_visible = true;
    }
    stack.update(tickets[closing_last ? 0 : 1], next);

    // Only the production quiet transaction is faulted. Snapshot vectors, fixture construction,
    // diagnostics, pointer dispatch, focus requests and notifications all run with the probe off.
    std::fprintf(stderr, "overlay rollback path=%d failure=%zu\n", static_cast<int>(path), failure);
    FaultResult result;
    ryn_test::allocation::begin(failure);
    try {
        static_cast<void>(stack.reconcile(true));
    } catch (const std::bad_alloc&) {
        result.failed = true;
    } catch (...) {
        static_cast<void>(ryn_test::allocation::end());
        throw;
    }
    result.allocations = ryn_test::allocation::end();
    if (result.failed) {
        const auto after = registry.input_constraint_diagnostics();
        check(registry.input_boundary_revision() == revision && registry.input_boundary() == boundary_before &&
                  stack.input_domain() == domain_before && stack.top() == top_before &&
                  std::ranges::equal(stack.ordered(), views_before) &&
                  std::ranges::equal(f.services.components().paint_traversal(), paint_before) &&
                  f.services.components().branch_active(owners[0]) == branches[0] &&
                  f.services.components().branch_active(owners[1]) == branches[1] &&
                  after.leases == diagnostics.leases && after.active_leases == diagnostics.active_leases &&
                  after.companions == diagnostics.companions &&
                  after.active_companions == diagnostics.active_companions &&
                  after.publications == diagnostics.publications,
              "failed candidate changed published policy/paint or leaked a lease/grant");
        const auto focused = f.services.focus().state();
        const auto current = f.services.pointer().state(PointerIdentity::mouse());
        check(focused.focused == focus_before.focused && focused.modality == focus_before.modality &&
                  focused.window_active == focus_before.window_active &&
                  focused.focus_visible == focus_before.focus_visible &&
                  focused.keyboard_pressed == focus_before.keyboard_pressed &&
                  f.services.focus().focus_request_revision() == focus_ticket &&
                  f.services.focus().diagnostics().focus_changes == focus_changes && current &&
                  current->primary_down == held->primary_down && current->capture == held->capture &&
                  current->press_origin == held->press_origin && current->hover_target == held->hover_target &&
                  current->position == held->position && f.services.pointer().diagnostics().cancels == cancels,
              "quiet prepare failure changed live focus/capture/press/hover");
        pointer(f, PointerAction::up, point);
        check(clicks[old_index] == 1 && clicks[1 - old_index] == 0,
              "rollback did not complete the original real pointer press exactly once");
        check(refs[old_index].focus() && !refs[1 - old_index].focus() && !refs[3].focus(),
              "rollback did not preserve actual old focus eligibility");
        if (has_companion) {
            click(f, buttons[2]);
            check(clicks[2] == 1 && refs[2].focus(), "rollback lost the old companion authority");
        }
        pointer(f, PointerAction::down, point);
        static_cast<void>(stack.reconcile(true));
    }

    const auto expected =
        closing_last ? std::optional<runtime::ComponentId>{} : std::optional{owners[closing_previous ? 0 : 1]};
    check(stack.input_domain() == expected && registry.input_boundary() == expected &&
              (expected ? stack.top() && stack.top()->owner == *expected : !stack.top()),
          "candidate retry did not converge to the new top/domain");
    static_cast<void>(f.services.synchronize_input_constraints());
    sync(f);
    stack.synchronize_focus();
    pointer(f, PointerAction::up, point);
    check(clicks[old_index] == (result.failed ? 1 : 0), "successful switch completed the retired pointer press");
    check(independent.valid() && block.valid() && !refs[3].focus(), "switch erased independent Tour/Form policy");
    const auto selected = closing_last ? 2 : closing_previous ? 0 : 1;
    if (closing_last || closing_previous) {
        check(f.services.focus().state().focused == f.buttons.mounted_buttons()[selected].interaction,
              "closing winner did not restore its legal trigger");
    }
    click(f, buttons[selected]);
    check(clicks[selected] == 1 + (selected == 2 && result.failed ? 1 : 0) && refs[selected].focus(),
          "successful candidate did not publish actual pointer/focus routing");
    if (path == RollbackPath::companion_spin) {
        check(!refs[2].focus(), "successful Spin retained the old Modal companion");
    } else if (has_companion) {
        check(refs[2].focus(), "successful Modal reauthorization lost the companion");
    }
    const auto stable_focus = f.services.focus().diagnostics().focus_changes;
    const auto stable_revision = registry.input_boundary_revision();
    static_cast<void>(f.services.synchronize_input_constraints());
    stack.synchronize_focus();
    check(f.services.focus().diagnostics().focus_changes == stable_focus &&
              registry.input_boundary_revision() == stable_revision,
          "stable synchronization repeated focus recovery/publication");
    stack.dispose();
    static_cast<void>(f.services.release_input_constraint(block));
    check(!refs[3].focus(), "overlay retirement erased the independent remaining policy");
    static_cast<void>(f.services.release_input_constraint(independent));
    check(refs[3].focus(), "independent policy did not release its exact restriction");
    const auto retired = registry.input_constraint_diagnostics();
    check(retired.leases == 0 && retired.companions == 0, "rollback fixture leaked a lease/grant after retirement");
    return result;
}

void notification_failure() {
    Fixture f;
    std::array<runtime::ComponentId, 2> owners;
    std::array<ButtonRef, 2> refs;
    f.services.mount(Content{[&] {
        Flex(FlexProps{}.vertical(true), FlexContent{[&] {
                 for (std::size_t i = 0; i < owners.size(); ++i) {
                     owners[i] =
                         group(f, Content{[&, i] { Button(ButtonProps{}.ref(refs[i]), [] { Text(u8"Action"); }); }});
                 }
             }});
    }});
    detail::WindowOverlayStack stack{f.services};
    std::array<detail::WindowOverlayTicket, 2> tickets;
    for (std::size_t i = 0; i < owners.size(); ++i) {
        const auto root = f.services.interactions().input_root(owners[i]);
        tickets[i] = stack.add(root, root, detail::WindowOverlayKind::Modal);
        detail::WindowOverlayState state;
        state.input_visible = i == 0;
        state.paint_visible = i == 0;
        stack.update(tickets[i], state);
    }
    static_cast<void>(stack.reconcile(true));
    sync(f);
    check(refs[0].focus(), "notification failure setup did not focus");

    struct Participant final : detail::WindowComponentParticipant {
        detail::WindowComponentServices* services{};
        unsigned calls{};

        ~Participant() override {
            if (services) {
                services->detach(*this);
            }
        }

        void synchronize_auxiliary_geometry(runtime::Size, runtime::Rect) override {}

        void on_input_boundary_changed() override {
            if (++calls == 1) {
                throw std::runtime_error("committed notification");
            }
        }
    } participant;

    participant.services = &f.services;
    f.services.attach(participant);
    detail::WindowOverlayState next;
    next.input_visible = true;
    next.paint_visible = true;
    stack.update(tickets[1], next);
    bool rejected{};
    try {
        static_cast<void>(stack.reconcile(false));
    } catch (const std::runtime_error&) {
        rejected = true;
    }
    const auto revision = f.services.interactions().input_boundary_revision();
    check(rejected && participant.calls == 1 && stack.top()->owner == owners[1] && stack.input_domain() == owners[1] &&
              f.services.interactions().input_boundary() == owners[1] &&
              paint_position(f, owners[0]) < paint_position(f, owners[1]) && !refs[0].focus() && refs[1].focus(),
          "notification exception rolled back an already committed top/domain/paint");
    static_cast<void>(f.services.synchronize_input_constraints());
    check(participant.calls == 2 && f.services.interactions().input_boundary_revision() == revision,
          "committed notification could not retry without republishing");
    static_cast<void>(f.services.synchronize_input_constraints());
    check(participant.calls == 2, "successful notification was not acknowledged");
}

void rollback() {
    constexpr std::size_t limit = 512;
    for (const auto path : {RollbackPath::spin, RollbackPath::modal, RollbackPath::companion_spin,
                            RollbackPath::close_previous, RollbackPath::close_last}) {
        const auto measured = rollback_attempt(path, std::numeric_limits<std::size_t>::max());
        check(!measured.failed && measured.allocations > 0 && measured.allocations < limit,
              "overlay allocation count is empty or exceeds the bounded sweep");
        std::size_t failures{};
        for (std::size_t failure = 0; failure <= measured.allocations; ++failure) {
            const auto result = rollback_attempt(path, failure);
            check(result.failed == (failure < measured.allocations), "overlay fault sweep skipped an allocation index");
            if (result.failed) {
                ++failures;
            } else {
                check(result.allocations == measured.allocations, "overlay success sentinel allocation count drifted");
            }
        }
        check(failures == measured.allocations, "overlay fault sweep was incomplete");
        std::cout << "overlay path=" << static_cast<int>(path) << " allocations=" << measured.allocations
                  << " caught=" << failures << " success_sentinel=1\n";
    }
    notification_failure();
}

void cleanup() {
    Fixture f;
    Signal<bool> first{false};
    Signal<bool> second{false};
    ButtonRef first_ref;
    ButtonRef second_ref;
    runtime::ComponentId first_button;
    int activations{};
    f.services.mount(Content{[&] {
        Spin(SpinProps{}.fullscreen(true).spinning(first),
             SpinSlots{.indicator = SpinIndicator{[&] {
                           Button(ButtonProps{}.ref(first_ref).onClick([&] { ++activations; }),
                                  [] { Text(u8"First"); });
                           first_button = f.buttons.mounted_buttons().back().component;
                       }}});
        Spin(SpinProps{}.fullscreen(true).spinning(second),
             SpinSlots{.indicator =
                           SpinIndicator{[&] { Button(ButtonProps{}.ref(second_ref), [] { Text(u8"Second"); }); }}});
    }});
    sync(f);
    for (int repeat = 0; repeat < 5; ++repeat) {
        second.set(true);
        first.set(true);
        sync(f);
        check(first_ref.focus() && !second_ref.focus(), "Spin order followed mount instead of activation");
        first.set(false);
        first.set(true);
        sync(f);
        check(first_ref.focus(), "Spin reopen did not retain the new activation order");
        first.set(false);
        second.set(false);
        sync(f);
        const auto state = f.services.interactions().input_constraint_diagnostics();
        check(state.leases == 0 && state.companions == 0 && !f.services.next_frame_deadline(),
              "closed overlays left a lease, companion or deadline");
    }
    first.set(true);
    sync(f);
    const auto rect = bounds(f, first_button);
    const runtime::Point point{rect.x + 2, rect.y + 2};
    pointer(f, PointerAction::down, point);
    f.services.set_window_active(false);
    pointer(f, PointerAction::up, point);
    f.services.set_window_active(true);
    check(activations == 0, "window reactivation completed a stale overlay pointer press");
    f.services.destroy(f.services.feedback().spins().front());
    sync(f);
    f.services.dispose();
    const auto state = f.services.interactions().input_constraint_diagnostics();
    check(state.leases == 0 && state.companions == 0 && f.services.interactions().size() == 0 &&
              !f.services.next_frame_deadline(),
          "overlay teardown did not return resources to baseline");
}

void two_windows() {
    Fixture a;
    Fixture b;
    Signal<bool> spin_a{false};
    Signal<bool> spin_b{false};
    InputRef input_a;
    InputRef input_b;
    const auto mount = [](Fixture& f, Signal<bool> spin, InputRef input) {
        f.services.mount(Content{[&f, spin, input] {
            Modal(ModalProps{}.open(true), ModalContent{[input] { Input(InputProps{}.ref(input)); }});
            Spin(SpinProps{}.fullscreen(true).spinning(spin),
                 SpinSlots{.indicator = SpinIndicator{[] { Text(u8"Busy"); }}});
        }});
        sync(f);
    };
    mount(a, spin_a, input_a);
    mount(b, spin_b, input_b);
    check(input_a.focus() && input_b.focus(), "two-window setup failed");
    const auto b_revision = b.services.interactions().input_boundary_revision();
    spin_a.set(true);
    sync(a);
    check(!input_a.focus() && input_b.focus() && b.services.interactions().input_boundary_revision() == b_revision,
          "window A changed window B's domain or focus");
    detail::WindowOverlayStack foreign{a.services};
    bool rejected{};
    try {
        const auto root = b.services.interactions().input_root(
            b.services.modal().snapshot(b.services.modal().mounted().front()).overlay);
        static_cast<void>(foreign.add(root, root, detail::WindowOverlayKind::Modal));
    } catch (const std::invalid_argument&) {
        rejected = true;
    }
    check(rejected && input_b.focus() && b.services.interactions().input_boundary_revision() == b_revision,
          "foreign overlay root partially changed the other window");
    a.services.dispose();
    check(input_b.focus() && b.services.interactions().input_boundary_revision() == b_revision,
          "window A teardown retired window B's authority");
}
} // namespace

int main(int argc, char** argv) {
    suppress_dialogs();
    try {
        const std::string_view name = argc > 1 ? argv[1] : "order";
        if (name == "order") {
            order();
        } else if (name == "domain") {
            domain();
        } else if (name == "nested") {
            nested();
        } else if (name == "companion") {
            companion();
        } else if (name == "focus") {
            focus();
        } else if (name == "drawer") {
            drawer();
        } else if (name == "reentry") {
            reentry();
        } else if (name == "rollback") {
            rollback();
        } else if (name == "cleanup") {
            cleanup();
        } else if (name == "two_windows") {
            two_windows();
        } else {
            throw std::runtime_error("unknown window overlay case");
        }
    } catch (const std::exception& error) {
        static_cast<void>(ryn_test::allocation::end());
        std::cerr << error.what() << '\n';
        return 1;
    }
    return 0;
}
