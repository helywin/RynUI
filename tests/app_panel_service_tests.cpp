#include "component/app_component.hpp"
#include "component/modal_component.hpp"
#include "component/popconfirm_component.hpp"
#include "component/feedback_component.hpp"
#include "support/input_fixture.hpp"
#include "support/i18n_test.hpp"
#include <chrono>
#include <algorithm>
#include <string_view>
#include <thread>

namespace {
using namespace ryn;
using namespace ryn::input;
using Fixture = ryn_test::input_component::Fixture;
using ryn_test::i18n::require;

void sync(Fixture& f) {
    require(f.services.layout_and_synchronize({720, 600}, {0, 0, 720, 600}), "panel layout failed");
}

runtime::ComponentId owner() {
    return runtime::require_component_build_context().nearest_state<detail::AppState>()->binding->owner;
}

void click(Fixture& f, runtime::ComponentId id) {
    sync(f);
    const auto& node = f.nodes.require(f.services.components().root(id));
    const float x = node.bounds.x + node.translation.x + node.bounds.width / 2;
    const float y = node.bounds.y + node.translation.y + node.bounds.height / 2;
    for (const auto action : {PointerAction::down, PointerAction::up}) {
        f.services.pointer().dispatch({PointerIdentity::mouse(), action, PointerButton::primary, x, y});
    }
    sync(f);
}

template <class Handle> bool ready(const Handle& handle) {
    return handle.closed().wait_for(std::chrono::seconds(0)) == std::future_status::ready;
}

template <class Fn> void rejected(Fn fn) {
    bool caught{};
    try {
        fn();
    } catch (const std::exception&) {
        caught = true;
    }
    require(caught, "invalid panel request accepted");
}

void lifecycle() {
    Fixture f;
    AppRef app;
    PopconfirmRef anchor;
    runtime::ComponentId root;
    runtime::ComponentId anchor_button;
    InputRef input;
    int mounts{};
    int confirmed{};
    PopconfirmHandle keyboard_pop;
    f.services.mount(Content{[&] {
        App(AppProps{}.ref(app), AppContent{[&] {
                root = owner();
                Popconfirm(PopconfirmProps{}.serviceAnchor(anchor), PopconfirmTrigger{[&] {
                               Button(ButtonProps{}.onClick([&] {
                                   if (!keyboard_pop.bound()) {
                                       keyboard_pop =
                                           app.popconfirm().confirm(anchor, PopconfirmProps{}.title(u8"Keyboard"));
                                   }
                               }),
                                      [] { Text(u8"Anchor"); });
                               anchor_button = f.buttons.mounted_buttons().back().component;
                           }});
            }});
    }});
    sync(f);
    const auto baseline = f.services.components().component_count();
    const auto interaction_count = f.services.interactions().size();
    const auto modal =
        app.modal().confirm(ModalProps{}.title(u8"Confirm").onOk([&] { ++confirmed; }), ModalContent{[&] {
                                ++mounts;
                                Input(InputProps{}.ref(input).defaultValue(u8"draft"));
                            }});
    sync(f);
    const auto id = f.services.modal().mounted().back();
    const auto before = f.services.modal().snapshot(id);
    require(before.visible && before.topmost && input.bound() && !ready(modal),
            "service did not mount an actual Modal");
    require(modal.update({.title = Prop<String>{String{u8"Updated"}}, .width = Prop<float>{600.0F}}),
            "Modal patch failed");
    sync(f);
    require(mounts == 1 && f.services.modal().snapshot(id).body == before.body, "ordinary update remounted body");
    click(f, before.ok);
    require(confirmed == 1 && ready(modal) && modal.closed().get().kind == PanelCloseKind::Confirmed && !input.bound(),
            "actual footer did not run action/settle/retire");
    require(!modal.close() && !modal.update({}), "retired handle revived");

    const auto drawer =
        app.drawer().open(DrawerProps{}.title(u8"Drawer"), DrawerContent{[&] { Input(InputProps{}.ref(input)); }});
    sync(f);
    const auto d = f.services.modal().snapshot(f.services.modal().mounted().back());
    require(d.drawer && !d.ok.valid() && input.bound(), "Drawer service used a confirmation placeholder");
    require(drawer.update({.width = Prop<float>{420.0F}, .loading = Prop<bool>{true}}), "Drawer update failed");
    require(drawer.close() && !ready(drawer) && !drawer.bound(), "Drawer close did not enter pending retirement");
    sync(f); // Reduced terminal scene.
    sync(f); // One safe completion notification pass.
    require(ready(drawer) && drawer.closed().get().kind == PanelCloseKind::Closed, "Drawer close failed");

    const auto pop = app.popconfirm().confirm(anchor, PopconfirmProps{}.title(u8"Delete?"));
    sync(f);
    const auto p = f.services.popconfirm().snapshot(f.services.popconfirm().mounted().back());
    require(p.geometry.visible && anchor.bound(), "anchored service did not show real Popconfirm");
    require(pop.update({.title = Prop<String>{String{u8"Changed?"}}}), "Popconfirm patch failed");
    click(f, p.confirm);
    if (!ready(pop) || pop.closed().get().kind != PanelCloseKind::Confirmed || !anchor.bound()) {
        std::cerr << "pop lifecycle ready=" << ready(pop) << " bound=" << pop.bound() << " anchor=" << anchor.bound();
        if (ready(pop)) {
            std::cerr << " result=" << static_cast<int>(pop.closed().get().kind);
        }
        std::cerr << '\n';
    }
    require(ready(pop) && pop.closed().get().kind == PanelCloseKind::Confirmed && anchor.bound(),
            "Popconfirm did not preserve anchor");
    const auto focused = f.services.focus().state().focused;
    const auto* focus_record = focused ? f.services.interactions().find(*focused) : nullptr;
    require(focus_record && focus_record->component == anchor_button,
            "service close did not restore its real retained anchor");
    f.services.focus().dispatch({Key::enter, KeyAction::down});
    f.services.focus().dispatch({Key::enter, KeyAction::up});
    sync(f);
    require(keyboard_pop.bound() && f.services.popconfirm().mounted().size() == 2 &&
                !f.services.popconfirm().snapshot(f.services.popconfirm().mounted().front()).geometry.visible,
            "service anchor swallowed keyboard activation or opened a duplicate declarative popup");
    require(keyboard_pop.close() && ready(keyboard_pop), "keyboard service cleanup failed");
    sync(f);
    for (int index = 0; index < 20; ++index) {
        const auto transient = app.modal().warning(ModalProps{}, ModalContent{[] { Input(InputProps{}); }});
        sync(f);
        require(transient.destroy() && ready(transient), "repeated service cleanup failed");
    }
    require(f.services.components().component_count() == baseline &&
                f.services.interactions().size() == interaction_count && !f.services.next_frame_deadline(),
            "panel cleanup did not return to baseline");
    require(f.services.destroy(root), "owner cleanup failed");
}

void actions() {
    Fixture f;
    AppRef app;
    runtime::ComponentId root;
    f.services.mount(Content{[&] { App(AppProps{}.ref(app), AppContent{[&] { root = owner(); }}); }});
    ConfirmAction::Complete complete;
    std::vector<std::function<void()>> posted;
    std::shared_ptr<const std::atomic_bool> ticket;
    int runs{};
    int cancels{};
    int errors{};
    const ConfirmAction action{[&](ConfirmActionRequest request, ConfirmAction::Complete done) {
                                   ++runs;
                                   ticket = request.cancelled;
                                   complete = std::move(done);
                                   return [&] {
                                       ++cancels;
                                       require(ticket->load(), "cancel preceded ticket invalidation");
                                   };
                               },
                               [&](std::function<void()> fn) { posted.push_back(std::move(fn)); },
                               std::chrono::milliseconds(10)};
    const auto modal =
        app.modal().confirm(ModalProps{}.onOkAsync(action).onActionError([&](ConfirmActionError) { ++errors; }),
                            ModalContent{[] { Input(InputProps{}); }});
    sync(f);
    const auto snapshot = f.services.modal().snapshot(f.services.modal().mounted().back());
    click(f, snapshot.ok);
    click(f, snapshot.ok);
    click(f, snapshot.cancel);
    require(runs == 1 && !ready(modal), "pending action accepted duplicate/ordinary cancel");
    std::thread worker([&] {
        complete({ConfirmActionDecision::KeepOpen});
        complete({});
    });
    worker.join();
    require(posted.size() == 1 && !ready(modal), "worker escaped post/once gate");
    posted.front()();
    posted.clear();
    sync(f);
    click(f, snapshot.ok);
    complete(
        {ConfirmActionDecision::KeepOpen, ConfirmActionError{ConfirmActionErrorCode::Business, String{u8"Retry"}}});
    posted.front()();
    posted.clear();
    sync(f);
    require(runs == 2 && errors == 1 && !ready(modal), "business failure closed service or kept loading");
    click(f, snapshot.ok);
    f.services.tick_animations(animation::AnimationTime::microseconds(10000));
    require(errors == 2 && cancels == 1 && !ready(modal) && !f.services.next_frame_deadline(),
            "timeout did not restore retry");
    click(f, snapshot.ok);
    const auto late = complete;
    require(modal.destroy() && ready(modal) && modal.closed().get().kind == PanelCloseKind::Destroyed && cancels == 2,
            "hard destroy did not cancel/settle");
    std::thread stale([&] { late({}); });
    stale.join();
    require(posted.empty(), "retired action posted a late completion");
    require(f.services.destroy(root), "action owner cleanup failed");
}

void controlled() {
    Fixture f;
    Signal<bool> open{true};
    InputRef ref;
    int closes{};
    f.services.mount(Content{[&] {
        Modal(ModalProps{}
                  .open(open)
                  .onOkAsync(ConfirmAction::synchronous([] { return ConfirmActionResult{}; }))
                  .onOk([&] { ++closes; })
                  .onCancel([&](ModalCancelReason) { ++closes; }),
              ModalContent{[&] { Input(InputProps{}.ref(ref).defaultValue(u8"a中文z")); }});
    }});
    sync(f);
    const auto id = f.services.modal().mounted().back();
    const auto s = f.services.modal().snapshot(id);
    const auto input = f.inputs.mounted_inputs().back();
    auto& editor = f.inputs.editors().require(input.editor);
    require(ref.focus() && ref.select(7, 1), "controlled selection failed");
    const auto selection = editor.selection();
    click(f, s.ok);
    click(f, s.cancel);
    require(closes == 2 && open.get() && f.services.modal().snapshot(id).visible && ref.bound() &&
                editor.selection() == selection,
            "async completion or refused close stole declarative authority/draft");
    open.set(false);
    sync(f);
    require(!f.services.next_frame_deadline(), "controlled close leaked action deadline");
    const auto popup = std::make_unique<Fixture>();
    Signal<bool> popup_open{true};
    InputRef popup_editor;
    int popup_confirms{};
    popup->services.mount(Content{[&] {
        Popconfirm(PopconfirmProps{}
                       .open(popup_open)
                       .onConfirmAsync(ConfirmAction::synchronous([] { return ConfirmActionResult{}; }))
                       .onConfirm([&] { ++popup_confirms; }),
                   PopconfirmTrigger{[] { Text(u8"Controlled popup"); }},
                   {.description =
                        PopconfirmDescription{[&] { Input(InputProps{}.ref(popup_editor).defaultValue(u8"draft")); }}});
    }});
    sync(*popup);
    const auto popup_id = popup->services.popconfirm().mounted().back();
    const auto popup_input = popup->inputs.mounted_inputs().back().editor;
    require(popup_editor.focus(), "controlled Popconfirm focus failed");
    click(*popup, popup->services.popconfirm().snapshot(popup_id).confirm);
    require(
        popup_confirms == 1 && popup_open.get() && popup->services.popconfirm().snapshot(popup_id).geometry.visible &&
            popup_editor.bound() && popup->inputs.mounted_inputs().back().editor == popup_input &&
            popup->inputs.editors().require(popup_input).value() == "draft" && !popup->services.next_frame_deadline(),
        "async Popconfirm accepted result stole controlled open or draft");
}

void failures() {
    Fixture f;
    AppRef app;
    runtime::ComponentId root;
    InputRef ref;
    f.services.mount(Content{[&] { App(AppProps{}.ref(app), AppContent{[&] { root = owner(); }}); }});
    const auto before = f.services.components().component_count();
    rejected([&] {
        static_cast<void>(app.modal().confirm(ModalProps{}, ModalContent{[] { throw std::runtime_error("slot"); }}));
    });
    require(f.services.components().component_count() == before, "failed mount leaked candidate");
    const auto modal = app.modal().confirm(ModalProps{}, ModalContent{[&] { Input(InputProps{}.ref(ref)); }});
    sync(f);
    const auto input = f.inputs.mounted_inputs().back().editor;
    require(ref.focus(), "update-failure editor focus failed");
    const auto session = f.inputs.sessions().active();
    require(bool(f.inputs.dispatch(CompositionChanged{String{u8"拼"}, {0, 1}, session})),
            "update-failure composition failed");
    rejected([&] {
        modal.update({.content = ModalContent{[] {
                          Input(InputProps{}.autoFocus(true));
                          throw std::runtime_error("update");
                      }}});
    });
    require(ref.bound() && f.inputs.mounted_inputs().front().editor == input && !ready(modal) &&
                f.inputs.sessions().active() == session && f.inputs.editors().require(input).composition().active,
            "failed slot update destroyed prior body/future");
    rejected([&] { modal.update({.width = Prop<float>{-1.0F}}); });
    require(ref.bound() && !ready(modal), "invalid update destroyed live service");
    ModalHandle reopened;
    require(modal.destroy(), "initial cleanup failed");
    int callbacks{};
    const auto first = app.modal().confirm(ModalProps{}
                                               .afterOpenChange([&](bool visible) {
                                                   if (!visible) {
                                                       ++callbacks;
                                                       throw std::runtime_error("afterOpenChange");
                                                   }
                                               })
                                               .afterClose([&] {
                                                   ++callbacks;
                                                   reopened = app.modal().info(
                                                       ModalProps{}, ModalContent{[] { Text(u8"New generation"); }});
                                               }),
                                           ModalContent{[] {}});
    sync(f);
    rejected([&] { first.close(); });
    require(callbacks == 2 && ready(first) && reopened.bound() && !ready(reopened), "throw/reentry lost cleanup order");
    require(f.services.destroy(root) && ready(reopened) &&
                reopened.closed().get().kind == PanelCloseKind::OwnerDisposed,
            "owner hard dispose did not settle reopened service");
}

void windows() {
    Fixture a;
    Fixture b;
    AppRef app;
    AppRef other;
    AppRef captured;
    PopconfirmRef first_anchor;
    PopconfirmRef second_anchor;
    PopconfirmRef controlled_anchor;
    runtime::ComponentId source;
    a.services.mount(Content{[&] {
        App(AppProps{}.ref(app), AppContent{[&] {
                Flex(FlexProps{}, FlexContent{[&] {
                         captured = useApp();
                         source = *runtime::require_component_build_context().capture_context().component_owner;
                         Popconfirm(PopconfirmProps{}.ref(first_anchor),
                                    PopconfirmTrigger{[] { Button(ButtonProps{}, [] { Text(u8"A"); }); }});
                         Popconfirm(PopconfirmProps{}.ref(controlled_anchor).open(false),
                                    PopconfirmTrigger{[] { Text(u8"Controlled"); }});
                     }});
            }});
    }});
    b.services.mount(Content{[&] {
        App(AppProps{}.ref(other), AppContent{[&] {
                Popconfirm(PopconfirmProps{}.ref(second_anchor),
                           PopconfirmTrigger{[] { Button(ButtonProps{}, [] { Text(u8"B"); }); }});
            }});
    }});
    sync(a);
    sync(b);
    rejected([&] { static_cast<void>(app.popconfirm().confirm(second_anchor, PopconfirmProps{})); });
    rejected([&] { static_cast<void>(app.popconfirm().confirm(controlled_anchor, PopconfirmProps{})); });
    const auto modal = captured.modal().confirm(ModalProps{}, ModalContent{[] {}});
    const auto drawer = other.drawer().open(DrawerProps{}, DrawerContent{[] {}});
    sync(a);
    sync(b);
    bool worker_bound{true};
    std::thread worker([&] {
        worker_bound = modal.bound();
        require(!modal.close(), "worker entered panel host");
    });
    worker.join();
    require(!worker_bound && a.services.destroy(source) && ready(modal) && app.bound() && !captured.bound() &&
                !ready(drawer),
            "source/worker/window boundary crossed ownership");
    const auto old = app;
    const auto old_anchor = first_anchor;
    a.services.dispose();
    Fixture rebound;
    rebound.services.mount(Content{[&] {
        App(AppProps{}.ref(app), AppContent{[&] {
                Popconfirm(PopconfirmProps{}.ref(first_anchor), PopconfirmTrigger{[] { Text(u8"Rebound"); }});
            }});
    }});
    require(app.bound() && !old.bound() && !old.modal().bound(), "old App generation revived");
    require(first_anchor.bound() && !old_anchor.bound(), "old anchor ref revived after rebinding");
    rejected([&] { static_cast<void>(app.popconfirm().confirm(old_anchor, PopconfirmProps{})); });
    b.services.dispose();
    require(ready(drawer) && drawer.closed().get().kind == PanelCloseKind::OwnerDisposed,
            "second window future leaked");
}

void domain() {
    Fixture f;
    AppRef app;
    AppRef nested;
    Signal<bool> spin{false};
    Signal<bool> covering_open{false};
    InputRef background;
    InputRef editor;
    InputRef covering_editor;
    f.services.mount(Content{[&] {
        App(AppProps{}.ref(app), AppContent{[&] {
                Input(InputProps{}.ref(background));
                Modal(ModalProps{}.open(true), ModalContent{[&] { nested = useApp(); }});
            }});
        Modal(ModalProps{}.open(covering_open), ModalContent{[&] { Input(InputProps{}.ref(covering_editor)); }});
        Spin(SpinProps{}.fullscreen(true).spinning(spin));
    }});
    sync(f);
    const auto parent = f.services.modal().mounted().front();
    covering_open.set(true);
    sync(f);
    const auto child = nested.modal().confirm(ModalProps{}, ModalContent{[&] { Input(InputProps{}.ref(editor)); }});
    sync(f);
    const auto id = f.services.modal().mounted().back();
    bool retained_parent{};
    for (auto ancestor = f.services.components().parent(id); ancestor;
         ancestor = f.services.components().parent(*ancestor)) {
        retained_parent |= *ancestor == parent;
    }
    require(retained_parent && covering_editor.focus() && !editor.focus() && !background.focus(),
            "service lost captured overlay family");
    covering_open.set(false);
    sync(f);
    require(editor.focus(), "uncovered service did not gain its family's input authority");
    spin.set(true);
    sync(f);
    require(!editor.focus(), "service bypassed Spin input authority");
    spin.set(false);
    sync(f);
    require(editor.focus() && child.close() && ready(child), "service did not restore/release real input domain");
    require(f.services.modal().snapshot(parent).topmost && !background.focus() && !f.services.next_frame_deadline(),
            "child cleanup left domain/deadline behind");
}

void retirement() {
    Fixture f;
    AppRef app;
    runtime::ComponentId root;
    f.services.mount(Content{[&] { App(AppProps{}.ref(app), AppContent{[&] { root = owner(); }}); }});
    std::array<std::shared_ptr<const std::atomic_bool>, 2> tickets;
    std::array<ConfirmAction::Complete, 2> completions;
    int cancels{};
    const auto action = [&](std::size_t index) {
        return ConfirmAction{
            [&, index](ConfirmActionRequest request, ConfirmAction::Complete done) {
                tickets[index] = request.cancelled;
                completions[index] = std::move(done);
                return [&] {
                    ++cancels;
                    require(tickets[0]->load() && tickets[1]->load(),
                            "owner cancel ran before invalidating all actions");
                    rejected([&] { static_cast<void>(app.modal().confirm(ModalProps{}, ModalContent{[] {}})); });
                    completions[0]({});
                    completions[1]({});
                    throw std::runtime_error("cancel reentry");
                };
            },
            [](std::function<void()> fn) { fn(); }};
    };
    const auto first = app.modal().confirm(ModalProps{}.onOkAsync(action(0)), ModalContent{[] {}});
    sync(f);
    click(f, f.services.modal().snapshot(f.services.modal().mounted().back()).ok);
    const auto second = app.modal().confirm(ModalProps{}.onOkAsync(action(1)), ModalContent{[] {}});
    sync(f);
    click(f, f.services.modal().snapshot(f.services.modal().mounted().back()).ok);
    require(tickets[0] && tickets[1] && f.services.destroy(root), "pending owner setup/retirement failed");
    require(cancels == 2 && ready(first) && ready(second) &&
                first.closed().get().kind == PanelCloseKind::OwnerDisposed &&
                second.closed().get().kind == PanelCloseKind::OwnerDisposed && !f.services.next_frame_deadline() &&
                f.services.interactions().size() == 0 && f.services.modal().mounted().empty() && f.nodes.size() == 0 &&
                !f.services.modal().effective_input_domain() &&
                f.services.interactions().input_constraint_diagnostics().leases == 0 &&
                f.services.interactions().input_constraint_diagnostics().companions == 0,
            "hard dispose retained action/future/input authority");
    const auto immediate = std::make_unique<Fixture>();
    AppRef immediate_app;
    runtime::ComponentId immediate_root;
    int returned_cancel{};
    immediate->services.mount(
        Content{[&] { App(AppProps{}.ref(immediate_app), AppContent{[&] { immediate_root = owner(); }}); }});
    const auto synchronous = immediate_app.modal().confirm(
        ModalProps{}
            .onOkAsync(ConfirmAction{[&](ConfirmActionRequest request, ConfirmAction::Complete complete) {
                                         complete({});
                                         return [&, cancelled = request.cancelled] {
                                             require(cancelled->load(),
                                                     "returned cancel preceded synchronous retirement");
                                             ++returned_cancel;
                                         };
                                     },
                                     [](std::function<void()> fn) { fn(); }})
            .onOk([&] { require(immediate->services.destroy(immediate_root), "synchronous owner destroy failed"); }),
        ModalContent{[] { Input(InputProps{}); }});
    sync(*immediate);
    click(*immediate, immediate->services.modal().snapshot(immediate->services.modal().mounted().back()).ok);
    require(returned_cancel == 1 && ready(synchronous) &&
                synchronous.closed().get().kind == PanelCloseKind::OwnerDisposed &&
                immediate->services.interactions().size() == 0 && !immediate->services.next_frame_deadline(),
            "synchronous Complete/owner retirement/returned cancel did not converge");
}

void dispatch() {
    for (const int mode : {0, 1, 2}) {
        Fixture f;
        AppRef app;
        f.services.mount(Content{[&] { App(AppProps{}.ref(app), AppContent{[] {}}); }});
        ConfirmAction::Complete done;
        int errors{};
        const ConfirmAction source{
            [&](ConfirmActionRequest, ConfirmAction::Complete complete) -> ConfirmAction::Cancel {
                if (mode == 0) {
                    throw std::runtime_error("provider");
                }
                done = std::move(complete);
                return {};
            },
            [mode](std::function<void()> fn) {
                if (mode == 1) {
                    throw std::runtime_error("dispatcher");
                }
                fn();
            },
            std::chrono::milliseconds(10)};
        const auto modal = app.modal().confirm(
            ModalProps{}.onOkAsync(source).onActionError([&](ConfirmActionError) { ++errors; }), ModalContent{[] {}});
        sync(f);
        click(f, f.services.modal().snapshot(f.services.modal().mounted().back()).ok);
        if (mode != 0) {
            std::thread worker([&] { done({}); });
            worker.join();
            require(errors == 0, "worker called UI error callback");
            f.services.tick_animations(animation::AnimationTime::microseconds(10000));
        }
        require(errors == 1 && !ready(modal) && !f.services.next_frame_deadline() && modal.destroy(),
                "provider/dispatcher failure did not restore a live retryable panel");
    }
}
} // namespace

int main(int argc, char** argv) {
    ryn_test::i18n::suppress_dialogs();
    try {
        const std::string_view name = argc > 1 ? argv[1] : "lifecycle";
        if (name == "lifecycle") {
            lifecycle();
        } else if (name == "actions") {
            actions();
        } else if (name == "controlled") {
            controlled();
        } else if (name == "failures") {
            failures();
        } else if (name == "windows") {
            windows();
        } else if (name == "domain") {
            domain();
        } else if (name == "retirement") {
            retirement();
        } else if (name == "dispatch") {
            dispatch();
        } else {
            throw std::invalid_argument("Unknown panel test case");
        }
    } catch (const std::exception& error) {
        std::cerr << error.what() << '\n';
        return 1;
    }
}
