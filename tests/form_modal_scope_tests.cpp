#include "component/form_component.hpp"
#include "component/image_component.hpp"
#include "component/modal_component.hpp"
#include "support/input_fixture.hpp"
#include <algorithm>
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

enum class Body { Named, Private, Layout, NestedForm };

struct Scoped final {
    Fixture f;
    FormRef form;
    FormRef inner_form;
    FormField<String> outer{String{u8"outer"}};
    FormField<String> own{String{u8"own"}};
    Signal<String> authority{String{u8"own"}};
    Signal<bool> disabled{false};
    Signal<bool> readonly{false};
    Signal<bool> hidden{false};
    Signal<bool> item_disabled{false};
    Signal<bool> item_readonly{false};
    Signal<bool> spinning{false};
    Signal<bool> foreign_open{false};
    InputRef tail;
    InputRef editor;
    InputRef private_editor;
    ImageRef image;
    runtime::ComponentId form_id;
    runtime::ComponentId item_id;
    runtime::ComponentId editor_id;
    runtime::ComponentId tail_id;
    std::function<void(String)> old_change;
    std::function<void()> setter;
    bool accept{true};
    bool throw_setter{};
    int finishes{};
    int inner_finishes{};
    int values{};
    int failures{};

    explicit Scoped(Body body = Body::Named, bool controlled = false,
                    std::function<void(FormField<String>&)> configure = {}) {
        if (controlled) {
            own = FormField<String>{String{u8"own"}, authority, [this](String value) {
                                        if (throw_setter) {
                                            throw std::runtime_error("controlled setter");
                                        }
                                        if (accept) {
                                            authority.set(std::move(value));
                                        }
                                        if (setter) {
                                            setter();
                                        }
                                    }};
        }
        own.required();
        outer.required();
        if (configure) {
            configure(own);
        }
        const std::array bytes{std::byte{255}, std::byte{40}, std::byte{80}, std::byte{255}};
        const auto source = ImageSource{ImagePixels::copy(1, 1, 4, bytes)};
        f.services.mount(Content{[&] {
            Spin(SpinProps{}.spinning(spinning), SpinContent{[&] {
                     Form(FormProps{}
                              .ref(form)
                              .disabled(disabled)
                              .readOnly(readonly)
                              .onFinish([&](FormResult) { ++finishes; })
                              .onFinishFailed([&](FormResult) { ++failures; })
                              .onValuesChange([&](FormFieldValue, FormData) { ++values; }),
                          FormContent{[&] {
                              FormItem(
                                  FormItemProps{}.field(u8"outer", outer), FormItemContent{[&] {
                                      Image(ImageProps{}.source(source).ref(image).actionsRender(
                                          std::make_shared<const ImageActionsRenderer>([&, body](
                                                                                           const ImagePreviewActions&) {
                                              Input(InputProps{}.defaultValue(u8"private").ref(private_editor));
                                              const auto named = [&] {
                                                  FormItem(FormItemProps{}
                                                               .field(u8"own", own)
                                                               .hidden(hidden)
                                                               .disabled(item_disabled)
                                                               .readOnly(item_readonly),
                                                           FormItemContent{[&] {
                                                               old_change = own.onChange();
                                                               Input(InputProps{}.ref(editor));
                                                               editor_id = f.inputs.mounted_inputs().back().component;
                                                           }});
                                              };
                                              if (body == Body::Named) {
                                                  named();
                                              } else if (body == Body::NestedForm) {
                                                  Form(FormProps{}.ref(inner_form).onFinish([&](FormResult) {
                                                      ++inner_finishes;
                                                  }),
                                                       FormContent{named});
                                              } else if (body == Body::Layout) {
                                                  FormItem(FormItemProps{}, FormItemContent{[] { Text(u8"layout"); }});
                                              }
                                          })));
                                      Input(InputProps{}.ref(tail));
                                      tail_id = f.inputs.mounted_inputs().back().component;
                                  }});
                          }});
                 }});
            Modal(ModalProps{}.open(foreign_open).footer(ModalFooterMode::None),
                  ModalContent{[] { Text(u8"independent foreign modal"); }});
        }});
        sync();
        form_id = f.services.form().mounted().front();
        if (editor_id.valid()) {
            for (auto current = f.services.components().parent(editor_id); current;
                 current = f.services.components().parent(*current)) {
                if (std::ranges::find(f.services.form().items(), *current) != f.services.form().items().end()) {
                    item_id = *current;
                    break;
                }
            }
        }
    }

    void sync() {
        check(f.services.layout_and_synchronize({720, 800}, {0, 0, 720, 800}), "scope layout failed");
    }

    void open() {
        check(image.preview(), "scope preview open failed");
        sync();
    }

    void ensure_open() {
        const auto owner = f.services.image().mounted().front();
        if (!f.services.image().snapshot(owner).open) {
            open();
        }
    }

    input::TextInputSessionStamp edit(String text) {
        check(editor.focus(), "scope editor focus failed");
        const auto stamp = f.inputs.sessions().active();
        static_cast<void>(f.inputs.editors().require(stamp.owner).select_all());
        check(bool(f.inputs.dispatch(input::TextCommitted{std::move(text), stamp})), "scope text commit rejected");
        sync();
        return stamp;
    }

    void enter() {
        f.services.focus().dispatch(input::KeyboardInputEvent{input::Key::enter, input::KeyAction::down});
        f.services.focus().dispatch(input::KeyboardInputEvent{input::Key::enter, input::KeyAction::up});
    }

    void dispose() {
        f.services.dispose();
        check(f.services.components().component_count() == 0 && f.nodes.size() == 0 &&
                  f.services.interactions().declaration_order().empty() && !f.inputs.sessions().active().valid() &&
                  f.services.surfaces().size() == 0 && f.services.images().live_count() == 0 && f.scene.size() == 0,
              "scope retirement leaked components/resources/session");
    }
};

void commands_and_epochs() {
    Scoped s;
    check(s.tail.focus(), "background tail did not bind before preview");
    const auto tail_stamp = s.f.inputs.sessions().active();
    s.open();
    check(!s.tail.focus() && !bool(s.f.inputs.dispatch(input::TextCommitted{String{u8"blocked"}, tail_stamp})),
          "Modal scope admitted background tail input");
    auto stamp = s.edit(u8"accepted own");
    check(s.own.get() == String{u8"accepted own"} && s.own.meta().touched && s.outer.get() == String{u8"outer"} &&
              !s.outer.meta().touched && s.values == 1,
          "scope store/touched/notification authority mismatch");
    const auto validation = s.form.validate();
    check(validation && validation->valid(), "same Form validation refused its active explicit Item");
    s.enter();
    check(s.finishes == 1, "explicit Item Enter lost its original Form");
    s.outer.set(String{});
    const auto invalid = s.form.validate();
    check(invalid && !invalid->valid() && s.form.submit() && s.failures == 1 && !s.tail.focus(),
          "whole Form validation ignored background fields or focused background");
    check(s.form.reset() && s.outer.get() == String{u8"outer"} && s.own.get() == String{u8"own"},
          "same Form reset lost background/explicit authorities");
    check(s.editor.focus(), "reset lost explicit ref");
    stamp = s.f.inputs.sessions().active();
    check(bool(s.f.inputs.dispatch(input::CompositionChanged{String{u8"候选"}, {}, stamp})), "scope preedit setup");
    check(s.form.reset() && !s.f.inputs.editors().require(stamp.owner).composition().active,
          "same-value reset retained explicit preedit");
    const auto old_component = s.editor_id;
    stamp = s.f.inputs.sessions().active();
    check(s.image.preview(false), "scope close failed");
    s.sync();
    check(!s.editor.focus() && !bool(s.f.inputs.dispatch(input::TextCommitted{String{u8"stale"}, stamp})) &&
              !bool(s.f.inputs.dispatch(input::CompositionChanged{String{u8"stale"}, {}, stamp})),
          "closed Modal admitted an old text/preedit epoch");
    s.open();
    check(s.editor_id == old_component && s.f.services.components().contains(old_component), "reopen remounted Item");
    s.edit(u8"reopened");
    check(s.own.get() == String{u8"reopened"}, "reopen lost original binding");
    check(s.image.preview(false), "final scope close failed");
    s.sync();
    check(s.tail.focus(), "tail failed after Modal close");
    const auto tail = s.f.inputs.sessions().active();
    static_cast<void>(s.f.inputs.editors().require(tail.owner).select_all());
    check(bool(s.f.inputs.dispatch(input::TextCommitted{String{u8"tail restored"}, tail})), "tail edit after close");
    s.enter();
    check(s.outer.get() == String{u8"tail restored"} && s.finishes == 2, "tail authority/Enter failed after close");
    s.dispose();
}

void gates_and_registrations() {
    Scoped s;
    s.open();
    s.edit(u8"before gates");
    const auto blocked = [&] {
        const auto value = s.own.get();
        const auto count = s.values;
        s.old_change(u8"forbidden callback");
        check(!s.editor.focus() && s.own.get() == value && s.values == count && !s.form.submit() && !s.form.validate(),
              "inactive scope admitted input/store/whole Form commands");
    };
    s.disabled.set(true);
    s.sync();
    blocked();
    s.disabled.set(false);
    s.sync();
    s.ensure_open();
    s.hidden.set(true);
    s.sync();
    blocked();
    s.hidden.set(false);
    s.sync();
    s.ensure_open();
    s.spinning.set(true);
    s.sync();
    blocked();
    s.spinning.set(false);
    s.sync();
    s.ensure_open();
    s.f.services.components().set_branch_active(s.form_id, false);
    s.sync();
    blocked();
    s.f.services.components().set_branch_active(s.form_id, true);
    s.sync();
    s.ensure_open();
    s.f.services.set_input_blocked(s.item_id, true);
    s.sync();
    blocked();
    s.f.services.set_input_blocked(s.item_id, false);
    s.sync();
    for (auto* gate : {&s.readonly, &s.item_disabled, &s.item_readonly}) {
        gate->set(true);
        s.sync();
        const auto value = s.own.get();
        const auto count = s.values;
        s.old_change(u8"readonly callback");
        if (s.editor.focus()) {
            const auto stamp = s.f.inputs.sessions().active();
            check(!bool(s.f.inputs.dispatch(input::TextCommitted{String{u8"readonly input"}, stamp})),
                  "readonly focused Input accepted a native text commit");
        }
        check(s.own.get() == value && s.values == count, "readonly/disabled Item accepted a write");
        gate->set(false);
        s.sync();
    }
    const auto old_id = s.item_id;
    const auto old_input = s.editor_id;
    const auto stale = s.old_change;
    check(s.f.services.destroy(old_id), "explicit Item retirement failed");
    s.sync();
    blocked();
    const auto actions = s.f.services.image().snapshot(s.f.services.image().mounted().front()).actions;
    s.f.services.append_slot(actions, Content{[&] {
                                 FormItem(FormItemProps{}.field(u8"own", s.own), FormItemContent{[&] {
                                              Input(InputProps{}.ref(s.editor));
                                              s.editor_id = s.f.inputs.mounted_inputs().back().component;
                                          }});
                             }});
    s.sync();
    stale(u8"stale registration");
    check(!s.f.services.components().contains(old_id) && !s.f.services.components().contains(old_input) &&
              s.own.get() == String{u8"before gates"},
          "old ComponentId or registration wrote replacement");
    s.edit(u8"new registration");
    check(s.own.get() == String{u8"new registration"} && s.form.submit(),
          "replacement Item failed to qualify its Form");
    s.dispose();
}

void scope_isolation() {
    for (auto body : {Body::Private, Body::Layout, Body::NestedForm}) {
        Scoped s{body};
        s.open();
        check(!s.form.submit() && !s.form.validate(), "private/layout/another Form's Item authorized outer Form");
        check(s.private_editor.focus(), "private editor in unqualified scope failed");
        const auto stamp = s.f.inputs.sessions().active();
        static_cast<void>(s.f.inputs.editors().require(stamp.owner).select_all());
        check(bool(s.f.inputs.dispatch(input::TextCommitted{String{u8"private edit"}, stamp})), "private commit");
        s.enter();
        check(s.outer.get() == String{u8"outer"} && !s.outer.meta().touched && s.values == 0 && s.finishes == 0,
              "private editor claimed background binding/Enter");
        if (body == Body::NestedForm) {
            s.edit(u8"inner");
            check(s.inner_form.submit() && s.inner_finishes == 1 && !s.form.submit(),
                  "nested Form scope crossed model");
        }
        s.dispose();
    }
    Scoped a;
    Scoped b;
    check(a.form_id == b.form_id && a.editor_id == b.editor_id, "window isolation fixture IDs differ");
    a.open();
    b.open();
    b.foreign_open.set(true);
    b.sync();
    check(a.form.submit() && !b.form.submit() && !b.editor.focus(), "same-number IDs crossed windows/foreign Modal");
    const auto prior = b.own.get();
    b.old_change(u8"foreign callback");
    check(b.own.get() == prior, "foreign Modal accepted old field callback");
    b.foreign_open.set(false);
    b.sync();
    b.edit(u8"recovered window");
    check(b.own.get() == String{u8"recovered window"} && a.own.get() == String{u8"own"},
          "foreign recovery crossed window");
    a.dispose();
    b.dispose();
}

void controlled_and_reentry() {
    Scoped s{Body::Named, true};
    s.open();
    s.accept = false;
    s.edit(u8"rejected");
    check(s.own.get() == String{u8"own"} && s.values == 0, "controlled refusal lost authority");
    s.accept = true;
    s.edit(u8"accepted");
    check(s.own.get() == String{u8"accepted"} && s.values == 1, "controlled acceptance failed");
    s.setter = [&] {
        s.f.services.destroy(s.form_id);
    };
    s.edit(u8"retire in setter");
    check(!s.form.bound() && !s.editor.bound() && !s.f.inputs.sessions().active().valid(),
          "controlled setter retirement retained authority");
    s.dispose();

    Scoped metadata;
    metadata.open();
    Scope observers;
    bool armed = true;
    auto observer = effect(observers, [&] {
        if (metadata.own.meta().touched && armed) {
            armed = false;
            metadata.f.services.destroy(metadata.item_id);
        }
    });
    const auto prior = metadata.own.get();
    metadata.edit(u8"retire in touched");
    check(!metadata.editor.bound() && metadata.own.get() == prior && metadata.values == 0 && !metadata.form.submit(),
          "metadata retirement published stale value/notification or kept scope");
    metadata.dispose();
}

void metadata_rebind_and_throw() {
    Scoped s;
    s.open();
    Scope observers;
    bool armed = true;
    InputRef replacement;
    runtime::ComponentId replacement_id;
    const auto old_component = s.editor_id;
    const auto stale = s.old_change;
    const auto actions = s.f.services.image().snapshot(s.f.services.image().mounted().front()).actions;
    auto observer = effect(observers, [&] {
        if (!s.own.meta().touched || !armed) {
            return;
        }
        armed = false;
        s.f.services.destroy(s.item_id);
        s.f.services.append_slot(actions, Content{[&] {
                                     FormItem(FormItemProps{}.field(u8"own", s.own), FormItemContent{[&] {
                                                  Input(InputProps{}.ref(replacement));
                                                  replacement_id = s.f.inputs.mounted_inputs().back().component;
                                              }});
                                 }});
    });
    s.edit(u8"superseded write");
    check(replacement.bound() && !s.editor.bound() && !s.f.services.components().contains(old_component) &&
              s.own.get() == String{u8"own"} && s.values == 0 && s.form.submit(),
          "metadata rebind accepted the superseded generation or lost the new scope");
    stale(u8"old callback");
    check(s.own.get() == String{u8"own"} && replacement.focus(), "old callback wrote rebound field");
    const auto stamp = s.f.inputs.sessions().active();
    static_cast<void>(s.f.inputs.editors().require(stamp.owner).select_all());
    check(bool(s.f.inputs.dispatch(input::TextCommitted{String{u8"fresh"}, stamp})) &&
              s.own.get() == String{u8"fresh"} && s.f.services.components().contains(replacement_id),
          "replacement registration lost its own native write");
    s.dispose();

    Scoped throwing{Body::Named, true};
    throwing.open();
    throwing.throw_setter = true;
    bool threw{};
    try {
        throwing.edit(u8"throwing request");
    } catch (const std::runtime_error&) {
        threw = true;
    }
    check(threw && !throwing.form.bound() && !throwing.editor.bound() && !throwing.f.inputs.sessions().active().valid(),
          "throwing controlled callback left a partial Form");
    throwing.dispose();
}

void settled_idle() {
    Scoped s;
    s.open();
    s.edit(u8"idle");
    check(s.image.preview(false), "idle preview close failed");
    s.sync();
    s.f.services.set_window_active(false);
    s.sync();
    static_cast<void>(s.f.frames.consume_request());
    const auto requests = s.f.frames.counters().requests;
    for (int i = 0; i < 8; ++i) {
        s.sync();
    }
    check(s.f.frames.counters().requests == requests && !s.f.frames.pending() && !s.f.services.next_frame_deadline() &&
              s.f.dirty.layout_roots().empty() && s.f.dirty.transform_nodes().empty() &&
              s.f.dirty.geometry_nodes().empty() && s.f.dirty.hit_test_nodes().empty() &&
              s.f.dirty.material_nodes().empty(),
          "settled Form scope polled or retained dirty work");
    s.dispose();
}

void async_scope() {
    std::vector<FormValidationComplete> completions;
    std::vector<std::function<void()>> posts;
    std::vector<std::shared_ptr<const std::atomic_bool>> tickets;
    int cancelled{};
    Scoped s{Body::Named, false, [&](auto& field) {
                 field.asyncValidator(
                     [&](String, FormValidationContext context, FormValidationComplete complete) {
                         completions.push_back(std::move(complete));
                         tickets.push_back(context.cancelled);
                         return [&] {
                             ++cancelled;
                         };
                     },
                     [&](auto post) { posts.push_back(std::move(post)); });
             }};
    s.open();
    check(s.form.submitAsync() && s.own.meta().validating, "explicit Item scope did not start async transaction");
    s.foreign_open.set(true);
    s.sync();
    check(cancelled == 1 && tickets.front()->load() && !s.own.meta().validating,
          "foreign Modal did not cancel prior Form validation");
    completions.front()({});
    for (auto& post : std::exchange(posts, {})) {
        post();
    }
    check(s.finishes == 0, "old async completion crossed input boundary");
    s.foreign_open.set(false);
    s.sync();
    check(s.form.submitAsync(), "restored explicit Item scope did not start fresh validation");
    completions.back()({});
    for (auto& post : std::exchange(posts, {})) {
        post();
    }
    check(s.finishes == 1 && !s.own.meta().validating, "fresh scoped async transaction failed");
    s.dispose();
}
} // namespace

int main() {
    try {
        commands_and_epochs();
        gates_and_registrations();
        scope_isolation();
        controlled_and_reentry();
        metadata_rebind_and_throw();
        async_scope();
        settled_idle();
        std::cout << checks << " Form Modal scope checks passed\n";
    } catch (const std::exception& error) {
        std::cerr << error.what() << '\n';
        return 1;
    }
}
