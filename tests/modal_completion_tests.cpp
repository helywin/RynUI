#include "component/app_component.hpp"
#include "component/modal_component.hpp"
#include "support/input_fixture.hpp"
#include <chrono>
#include <cmath>
#include <iostream>
#include <limits>
#include <stdexcept>
#include <string_view>
#include <vector>

namespace {
using namespace ryn;
using namespace ryn::input;
using Fixture = ryn_test::input_component::Fixture;
constexpr std::int64_t duration = 300000;

void check(bool value, const char* message) {
    if (!value) {
        throw std::runtime_error(message);
    }
}

bool near(float a, float b) {
    return std::abs(a - b) < .02F;
}

void sync(Fixture& f, float width = 720, float height = 600) {
    for (int i = 0; i < 2; ++i) {
        check(f.services.layout_and_synchronize({width, height}, {0, 0, width, height}), "Modal completion sync");
    }
}

void at(Fixture& f, std::int64_t time) {
    static_cast<void>(f.services.tick_animations(animation::AnimationTime::microseconds(time)));
    sync(f);
}

runtime::Rect bounds(Fixture& f, runtime::ComponentId component) {
    const auto& node = f.nodes.require(f.services.components().root(component));
    return {node.bounds.x + node.translation.x, node.bounds.y + node.translation.y, node.bounds.width,
            node.bounds.height};
}

void pointer(Fixture& f, PointerAction action, float x, float y) {
    f.services.pointer().dispatch({PointerIdentity::mouse(), action,
                                   action == PointerAction::move ? PointerButton::none : PointerButton::primary, x, y});
}

void lazy_motion() {
    Fixture f;
    f.services.set_motion_preference(animation::MotionPreference::normal);
    Signal<bool> open{false};
    InputRef input;
    int mounts = 0;
    int closed = 0;
    std::vector<bool> changed;
    f.services.mount(Content{[&] {
        Modal(ModalProps{}
                  .open(open)
                  .animated()
                  .lazy()
                  .destroyOnHidden()
                  .title(u8"Dialog")
                  .mask(ModalMask{true, true, true})
                  .afterClose([&] { ++closed; })
                  .afterOpenChange([&](bool value) { changed.push_back(value); }),
              ModalContent{[&] {
                  ++mounts;
                  Input(InputProps{}.ref(input).defaultValue(u8"Draft"));
              }});
    }});
    sync(f);
    const auto id = f.services.modal().mounted().front();
    check(mounts == 0 && !input.bound() && changed.empty(), "lazy Modal eagerly mounted body");
    open.set(true);
    sync(f);
    check(mounts == 1 && input.bound() && f.services.modal().snapshot(id).transitioning,
          "Modal did not start finite opening");
    at(f, duration / 2);
    const auto middle = f.services.modal().snapshot(id);
    check(near(middle.progress, .5F) && middle.paint_visible && changed.empty(), "Modal midpoint or completion timing");
    const auto& program = *f.services.scene_composer().ordered_scene().composition();
    check(program.requests.size() == 1 && near(program.requests.front().activation, .5F),
          "blur activation did not follow motion once");
    at(f, duration);
    check(changed == std::vector{true} && !f.services.modal().next_auxiliary_deadline(),
          "Modal opening did not settle");
    open.set(false);
    sync(f);
    check(!f.services.modal().snapshot(id).visible && f.services.modal().snapshot(id).paint_visible && input.bound() &&
              !input.focus() && closed == 0,
          "Modal exit admitted input or destroyed content early");
    at(f, duration + duration / 2);
    const float before = f.services.modal().snapshot(id).progress;
    open.set(true);
    sync(f);
    check(near(before, f.services.modal().snapshot(id).progress), "Modal reversal jumped");
    at(f, duration * 3);
    check(mounts == 1 && closed == 0 && changed == std::vector{true, true}, "aborted close destroyed or notified");
    open.set(false);
    at(f, duration * 4);
    check(!input.bound() && !f.services.modal().snapshot(id).body_mounted && closed == 1 &&
              changed == std::vector{true, true, false} && !f.services.modal().next_auxiliary_deadline(),
          "Modal terminal cleanup or callback count");
}

void geometry() {
    Fixture f;
    Signal<bool> resize{true};
    int mounts = 0;
    int moves = 0;
    int sizes = 0;
    f.services.mount(Content{[&] {
        Modal(ModalProps{}
                  .open()
                  .width(300)
                  .title(u8"Move dialog")
                  .draggable()
                  .resizable(resize)
                  .onDrag([&](float, float) { ++moves; })
                  .onResize([&](float, float) { ++sizes; }),
              ModalContent{[&] {
                  ++mounts;
                  Input(InputProps{}.defaultValue(u8"Retained"));
              }});
    }});
    sync(f);
    const auto id = f.services.modal().mounted().front();
    auto snapshot = f.services.modal().snapshot(id);
    const auto original = snapshot.bounds;
    const auto title = bounds(f, snapshot.title);
    const auto title_hit = f.services.hit_test().hit_test({title.x + 8, title.y + title.height / 2});
    pointer(f, PointerAction::down, title.x + 8, title.y + title.height / 2);
    pointer(f, PointerAction::move, title.x + 58, title.y + title.height / 2 + 25);
    pointer(f, PointerAction::up, title.x + 58, title.y + title.height / 2 + 25);
    sync(f);
    snapshot = f.services.modal().snapshot(id);
    if (!near(snapshot.bounds.x, original.x + 50) || !near(snapshot.bounds.y, original.y + 25) || moves != 1) {
        std::cerr << "title=" << title.x << ',' << title.y << ',' << title.width << ',' << title.height
                  << " hit=" << (title_hit ? f.services.interactions().require(*title_hit).component.index : 0)
                  << " title_id=" << snapshot.title.index << " dialog_id=" << snapshot.dialog.index
                  << " moved=" << snapshot.bounds.x - original.x << ',' << snapshot.bounds.y - original.y
                  << " callbacks=" << moves << '\n';
        for (const auto& record : f.services.hit_test().records()) {
            const auto& hit = f.services.interactions().require(record.interaction);
            std::cerr << "record " << hit.component.index << " raw=" << hit.eligible
                      << " eligible=" << record.effective_eligible << " committed=" << record.has_committed_layout
                      << " branch=" << f.services.interactions().branch_active(record.interaction)
                      << " rect=" << record.translated_bounds.x << ',' << record.translated_bounds.y << ','
                      << record.translated_bounds.width << ',' << record.translated_bounds.height
                      << " clip=" << record.effective_clip.x << ',' << record.effective_clip.y << ','
                      << record.effective_clip.width << ',' << record.effective_clip.height << '\n';
        }
    }
    check(near(snapshot.bounds.x, original.x + 50) && near(snapshot.bounds.y, original.y + 25) && moves == 1,
          "actual title drag failed");
    const auto before = snapshot.bounds;
    const auto grip = bounds(f, snapshot.resize_handle);
    pointer(f, PointerAction::down, grip.x + grip.width / 2, grip.y + grip.height / 2);
    pointer(f, PointerAction::move, grip.x + grip.width / 2 + 64, grip.y + grip.height / 2 + 48);
    pointer(f, PointerAction::up, grip.x + grip.width / 2 + 64, grip.y + grip.height / 2 + 48);
    sync(f);
    snapshot = f.services.modal().snapshot(id);
    check(near(snapshot.bounds.width, before.width + 64) && near(snapshot.bounds.height, before.height + 48) &&
              near(snapshot.bounds.x, before.x) && near(snapshot.bounds.y, before.y) && sizes == 1 && mounts == 1,
          "actual corner resize moved anchor or remounted body");
    InteractionId hit;
    for (const auto candidate : f.services.interactions().declaration_order()) {
        if (f.services.interactions().require(candidate).component == snapshot.resize_handle) {
            hit = candidate;
        }
    }
    check(f.services.focus().request_focus(hit, FocusModality::keyboard), "resize grip keyboard focus");
    f.services.focus().dispatch({Key::right, KeyAction::down});
    sync(f);
    check(f.services.modal().snapshot(id).bounds.width > snapshot.bounds.width, "keyboard resize failed");
    resize.set(false);
    sync(f);
    const auto stopped = f.services.modal().snapshot(id).bounds;
    f.services.focus().dispatch({Key::right, KeyAction::down});
    sync(f);
    check(f.services.modal().snapshot(id).bounds == stopped && !f.services.modal().snapshot(id).resizing,
          "disabled resize kept capture or keyboard admission");
}

void responsive_rtl() {
    Fixture f;
    Signal<Responsive<float>> widths{Responsive<float>{260}.at(Breakpoint::Md, 480).at(Breakpoint::Lg, 600)};
    Signal<bool> rtl{false};
    int mounts = 0;
    f.services.mount(Content{[&] {
        Modal(ModalProps{}.open().width(widths).rtl(rtl).title(u8"Responsive"), ModalContent{[&] {
                  ++mounts;
                  Input(InputProps{}.defaultValue(u8"Keep editor"));
              }});
    }});
    for (const auto [viewport, width] : {std::pair{720.0F, 260.0F}, {800.0F, 480.0F}, {1024.0F, 600.0F}}) {
        sync(f, viewport);
        const auto snapshot = f.services.modal().snapshot(f.services.modal().mounted().front());
        check(near(snapshot.bounds.width, width), "responsive Modal width resolved wrong breakpoint");
    }
    const auto id = f.services.modal().mounted().front();
    const auto input = f.inputs.mounted_inputs().front();
    const auto editor = input.editor;
    (void)f.services.focus().request_focus(input.interaction, FocusModality::keyboard);
    check(f.services.focus().state().focused == input.interaction, "responsive editor focus");
    auto& edit = f.inputs.editors().require(editor);
    (void)edit.set_value("dirty draft");
    (void)edit.select({8, 2});
    (void)f.inputs.dispatch(CompositionChanged{String{u8"preedit"}, {0, 1}, f.inputs.sessions().active()});
    const auto selection = edit.selection();
    const auto revision = edit.revision();
    const auto session = f.inputs.sessions().active();
    rtl.set(true);
    sync(f, 1024);
    const auto snapshot = f.services.modal().snapshot(id);
    check(bounds(f, snapshot.close).x < snapshot.bounds.x + snapshot.bounds.width / 2 &&
              bounds(f, snapshot.ok).x < bounds(f, snapshot.cancel).x && mounts == 1 &&
              f.inputs.mounted_inputs().front().editor == editor && edit.selection() == selection &&
              edit.revision() == revision && edit.composition().active && f.inputs.sessions().active() == session,
          "RTL close/footer geometry or retained editor session");
    widths.set(Responsive<float>{280});
    sync(f, 1024);
    check(near(f.services.modal().snapshot(id).bounds.width, 280) && edit.selection() == selection &&
              edit.revision() == revision && edit.composition().active && f.inputs.sessions().active() == session,
          "responsive source update reset editor session");
}

void cleanup_reentry() {
    Fixture f;
    f.services.set_motion_preference(animation::MotionPreference::normal);
    Signal<bool> open{false};
    InputRef input;
    bool reopen = false;
    int mounts = 0;
    int closed = 0;
    f.services.mount(Content{[&] {
        Modal(ModalProps{}.open(open).animated().lazy().destroyOnHidden().afterClose([&] { ++closed; }),
              ModalContent{[&] {
                  ++mounts;
                  Input(InputProps{}.ref(input));
                  runtime::require_component_build_context()
                      .scope(f.inputs.mounted_inputs().back().component)
                      .on_cleanup([&] {
                          if (reopen) {
                              reopen = false;
                              open.set(true);
                          }
                      });
              }});
    }});
    sync(f);
    open.set(true);
    at(f, duration);
    reopen = true;
    open.set(false);
    at(f, duration * 2);
    at(f, duration * 3);
    const auto snapshot = f.services.modal().snapshot(f.services.modal().mounted().front());
    check(snapshot.visible && snapshot.body_mounted && input.bound() && mounts == 2 && closed == 0,
          "cleanup reopen reused old InputRef or delivered stale close callback");
}

void service() {
    Fixture f;
    f.services.set_motion_preference(animation::MotionPreference::normal);
    AppRef app;
    f.services.mount(Content{[&] { App(AppProps{}, AppContent{[&] { app = useApp(); }}); }});
    sync(f);
    InputRef input;
    int closed = 0;
    auto panel = app.modal().confirm(ModalProps{}.animated().lazy().afterClose([&] { ++closed; }),
                                     ModalContent{[&] { Input(InputProps{}.ref(input)); }});
    at(f, duration);
    check(panel.close(), "animated service close rejected");
    sync(f);
    check(panel.closed().wait_for(std::chrono::seconds(0)) != std::future_status::ready && input.bound(),
          "service future or body retired before exit");
    check(!panel.update(ModalUpdate{.title = String{u8"Too late"}}), "service accepted update during closing");
    at(f, duration * 2);
    check(panel.closed().wait_for(std::chrono::seconds(0)) == std::future_status::ready && closed == 1 &&
              !input.bound(),
          "service terminal future/cleanup failed");
    Signal<Responsive<float>> widths{Responsive<float>{280}.at(Breakpoint::Md, 500)};
    auto responsive = app.modal().confirm(ModalProps{}.width(widths), ModalContent{[] { Text(u8"Profile"); }});
    sync(f, 900);
    auto id = f.services.modal().mounted().back();
    check(near(f.services.modal().snapshot(id).bounds.width, 500), "service lost responsive width");
    check(responsive.update(ModalUpdate{.width = 320.0F}), "service width patch rejected");
    widths.set(Responsive<float>{600});
    sync(f, 900);
    check(near(f.services.modal().snapshot(id).bounds.width, 320), "replaced responsive source regained authority");
    check(responsive.destroy(), "service cleanup");
}

void policy() {
    Fixture f;
    f.services.set_motion_preference(animation::MotionPreference::normal);
    Signal<bool> open{false};
    int closed = 0;
    f.services.mount(Content{[&] {
        Modal(ModalProps{}.open(open).animated().afterClose([&] { ++closed; }), ModalContent{[] { Text(u8"Motion"); }});
    }});
    sync(f);
    open.set(true);
    at(f, duration / 2);
    f.services.set_motion_preference(animation::MotionPreference::reduced);
    sync(f);
    check(!f.services.modal().next_auxiliary_deadline(), "reduced motion kept deadline");
    f.services.set_motion_preference(animation::MotionPreference::normal);
    open.set(false);
    f.services.set_window_active(false);
    sync(f);
    check(closed == 1 && !f.services.modal().next_auxiliary_deadline(), "inactive window did not settle exit");
    f.services.dispose();
    check(f.nodes.size() == 0 && f.services.interactions().size() == 0, "Modal completion cleanup leaked");
}
} // namespace

int main(int argc, char** argv) {
    try {
        check(argc == 2, "expected Modal completion case");
        const std::string_view name = argv[1];
        if (name == "lazy_motion") {
            lazy_motion();
        } else if (name == "geometry") {
            geometry();
        } else if (name == "responsive_rtl") {
            responsive_rtl();
        } else if (name == "cleanup_reentry") {
            cleanup_reentry();
        } else if (name == "service") {
            service();
        } else if (name == "policy") {
            policy();
        } else {
            throw std::runtime_error("unknown Modal completion case");
        }
    } catch (const std::exception& error) {
        std::cerr << error.what() << '\n';
        return 1;
    }
}
