#include "support/timeline_fixture.hpp"
#include "component/select_component.hpp"
#include "component/feedback_component.hpp"
#include "component/modal_component.hpp"
#if defined(_MSC_VER) && defined(_DEBUG)
#include <crtdbg.h>
#endif

namespace {
using namespace timeline_test;

void popup_authority() {
    Fixture f;
    Signal<bool> shown{true};
    Signal<bool> open{true};
    Signal<String> placeholder{String{u8"Choose"}};
    int closes = 0;
    f.services.mount(Content{[&] {
        Timeline(TimelineProps{}, {TimelineItem{u8"select", u8""}.visible(shown).richContent(TimelineContent{[&] {
                     Select(SelectProps{}
                                .options({{1, u8"One"}, {2, u8"Two"}})
                                .open(open)
                                .placeholder(placeholder)
                                .onOpenChange([&](bool next) { closes += !next; }));
                 }})});
    }});
    sync(f);
    const auto select = f.services.select().mounted()[0];
    check(f.services.select().snapshot(select).visible, "controlled initial popup missing");
    click(f, f.services.select().snapshot(select).selector);
    check(f.services.select().snapshot(select).visible && closes > 0, "ordinary close ignored controlled refusal");
    shown.set(false);
    sync(f);
    check(!f.services.select().snapshot(select).visible, "hidden uncontrolled boundary retained refused popup");
    shown.set(true);
    placeholder.set(u8"Changed");
    sync(f);
    check(!f.services.select().snapshot(select).visible, "ordinary update resurrected suppressed refused popup");
    open.set(false);
    open.set(true);
    sync(f);
    check(f.services.select().snapshot(select).visible, "new authority epoch failed to reopen");
    f.services.dispose();
    check(blocked_roots(f.services.interactions()).empty(), "popup disposal leaked block");
}

void callbacks_and_blocks() {
    Fixture f;
    runtime::ComponentId append;
    f.services.mount(Content{[&] {
        Button(ButtonProps{}, [] { Text(u8"Foreign owner"); });
        auto& build = runtime::require_component_build_context();
        append = build.mount_component<int>(0);
        const auto node = build.root(append);
        f.layout.set_layout(node, layout::BoxLayout{{}, true});
        build.on_resource_cleanup(append, [&f, node] { f.layout.remove_layout(node); });
    }});
    sync(f);
    const auto foreign = f.buttons.mounted_buttons()[0].component;
    f.services.set_input_blocked(foreign, true);
    const auto baseline = blocked_roots(f.services.interactions());
    const auto count = f.services.components().component_count();
    for (int cycle = 0; cycle < 4; ++cycle) {
        Signal<bool> visible{cycle % 2 != 0};
        InputRef ref;
        runtime::ComponentId owner;
        bool destroy = false;
        f.services.append_slot(append, Content{[&] {
                                   Timeline(
                                       TimelineProps{},
                                       {TimelineItem{u8"input", u8""}.visible(visible).richContent(TimelineContent{[&] {
                                           Input(InputProps{}.defaultValue(u8"Draft").ref(ref).onBlur([&] {
                                               if (destroy) {
                                                   check(f.services.destroy(owner), "blur callback failed to unmount");
                                               }
                                           }));
                                       }})});
                               }});
        sync(f);
        owner = f.services.timeline().mounted()[0];
        if (cycle % 2 != 0) {
            check(ref.focus(), "Input focus failed");
            const auto input = f.inputs.mounted_inputs()[0];
            const auto b = f.inputs.layout_snapshot(input.component).viewport;
            f.services.pointer().dispatch(
                {PointerIdentity::mouse(), PointerAction::down, PointerButton::primary, b.x + 4, b.y + b.height / 2});
            check(f.services.pointer().state(PointerIdentity::mouse())->capture == input.interaction,
                  "capture fixture failed");
            destroy = true;
            visible.set(false);
            sync(f);
            check(!f.services.pointer().state(PointerIdentity::mouse())->capture, "unmounted Input retained capture");
        } else {
            check(!ref.focus(), "hidden Input ref accepted focus");
            check(f.services.destroy(owner), "hidden owner teardown failed");
            sync(f);
        }
        check(blocked_roots(f.services.interactions()) == baseline &&
                  f.services.components().component_count() == count &&
                  !f.services.interactions().component_active(foreign),
              "owned cleanup leaked or removed foreign block");
    }
    f.services.set_input_blocked(foreign, false);
    sync(f);
    check(blocked_roots(f.services.interactions()).empty(), "foreign block did not recover");

    Fixture g;
    Signal<bool> shown{true};
    runtime::ComponentId owner;
    int closes = 0;
    g.services.mount(Content{[&] {
        Timeline(TimelineProps{}, {TimelineItem{u8"popup", u8""}.visible(shown).richContent(TimelineContent{[&] {
                     Select(SelectProps{}.options({{1, u8"One"}}).defaultOpen(true).onOpenChange([&](bool next) {
                         if (!next) {
                             ++closes;
                             check(g.services.destroy(owner), "close callback destruction failed");
                         }
                     }));
                 }})});
    }});
    sync(g);
    owner = g.services.timeline().mounted()[0];
    shown.set(false);
    sync(g);
    check(closes == 1 && g.services.components().component_count() == 0 &&
              blocked_roots(g.services.interactions()).empty(),
          "reentrant popup close retained stale state");
}

void reentrant_rebind() {
    Fixture f;
    runtime::ComponentId parent;
    f.services.mount(Content{[&] {
        auto& build = runtime::require_component_build_context();
        parent = build.mount_component<int>(0);
        const auto node = build.root(parent);
        f.layout.set_layout(node, layout::BoxLayout{{}, true});
        build.on_resource_cleanup(parent, [&f, node] { f.layout.remove_layout(node); });
    }});
    Signal<bool> shown{true};
    runtime::ComponentId retired;
    InputRef old_ref;
    InputRef new_ref;
    bool replacing = false;
    f.services.append_slot(
        parent, Content{[&] {
            Timeline(TimelineProps{}.visible(shown), {TimelineItem{u8"old", u8""}.richContent(TimelineContent{[&] {
                         Input(InputProps{}.ref(old_ref).onBlur([&] {
                             if (replacing) {
                                 replacing = false;
                                 check(f.services.destroy(retired), "rebind teardown failed");
                                 f.services.append_slot(
                                     parent, Content{[&] {
                                         Timeline(TimelineProps{},
                                                  {TimelineItem{u8"new", u8""}.richContent(TimelineContent{[&] {
                                                      Input(InputProps{}.ref(new_ref).defaultValue(u8"Replacement"));
                                                  }})});
                                     }});
                             }
                         }));
                     }})});
        }});
    sync(f);
    retired = f.services.timeline().mounted()[0];
    check(old_ref.focus(), "old Input focus failed");
    const auto stale_session = f.inputs.sessions().active();
    check(bool(f.inputs.dispatch(CompositionChanged{String{u8"临时"}, {0, 1}, stale_session})),
          "hidden preedit fixture failed");
    replacing = true;
    shown.set(false);
    sync(f);
    check(f.services.timeline().mounted().size() == 1 && f.services.timeline().mounted()[0] != retired &&
              !old_ref.focus() && new_ref.focus() &&
              !bool(f.inputs.dispatch(TextCommitted{String{u8"late"}, stale_session})) &&
              blocked_roots(f.services.interactions()).empty(),
          "callback rebind used stale generation or leaked owned block/session");
    f.services.dispose();
    check(blocked_roots(f.services.interactions()).empty(), "rebind disposal leaked block");
}

void boundaries_theme_idle() {
    Fixture f;
    Signal<bool> spinning{false};
    Signal<bool> modal{false};
    Signal<ThemeConfig> theme{ThemeConfig{}};
    Signal<bool> loading{true};
    InputRef ref;
    f.services.mount(Content{[&] {
        Theme(ThemeProps{}.config(theme), ThemeContent{[&] {
                  Spin(SpinProps{}.spinning(spinning), SpinContent{[&] {
                           Timeline(TimelineProps{},
                                    {TimelineItem{u8"input", u8""}.loading(loading).richContent(TimelineContent{
                                         [&] { Input(InputProps{}.defaultValue(u8"Initial").ref(ref)); }}),
                                     TimelineItem{u8"select", u8""}.richContent(
                                         TimelineContent{[] { Select(SelectProps{}.options({{1, u8"One"}})); }})});
                       }});
                  Modal(ModalProps{}.open(modal).restoreFocusAfterClose(false),
                        ModalContent{[] { Text(u8"Boundary"); }});
              }});
    }});
    sync(f);
    const auto input = f.inputs.mounted_inputs()[0];
    const auto select = f.services.select().mounted()[0];
    check(ref.focus({InputFocusCursor::All}), "Input focus failed");
    check(bool(f.inputs.dispatch(TextCommitted{String{u8"Saved draft"}, f.inputs.sessions().active()})),
          "draft commit failed");
    const auto session = f.inputs.sessions().active();
    check(bool(f.inputs.dispatch(CompositionChanged{String{u8"预编辑"}, {0, 1}, session})), "preedit fixture failed");
    auto config = std::make_shared<ThemeConfig>();
    config->algorithms = {ThemeAlgorithm::Dark};
    theme.set(*config);
    sync(f);
    check(f.inputs.sessions().active() == session &&
              f.inputs.editors().require(input.editor).value() == "Saved draft" &&
              f.inputs.editors().require(input.editor).composition().active,
          "Theme reset draft/session/preedit");
    modal.set(true);
    sync(f);
    check(!ref.focus() && !f.inputs.sessions().active().valid() &&
              !f.inputs.editors().require(input.editor).composition().active &&
              !bool(f.inputs.dispatch(TextCommitted{String{u8"late"}, session})),
          "Modal did not retire input session/preedit");
    modal.set(false);
    sync(f);
    check(ref.focus(), "Modal recovery failed");
    spinning.set(true);
    sync(f);
    check(!ref.focus() && !f.inputs.sessions().active().valid(), "Spin failed to block Input");
    spinning.set(false);
    sync(f);
    click(f, f.services.select().snapshot(select).selector);
    check(f.services.select().snapshot(select).visible, "Select did not open after boundary");
    modal.set(true);
    sync(f);
    check(!f.services.select().snapshot(select).visible, "Modal retained popup");
    modal.set(false);
    sync(f);
    check(!f.services.select().snapshot(select).visible && f.inputs.mounted_inputs()[0].editor == input.editor &&
              f.inputs.editors().require(input.editor).value() == "Saved draft",
          "boundary recovery resurrected popup or editor");
    const auto diagnostics = f.services.surfaces().diagnostics();
    static_cast<void>(f.frames.consume_request());
    sync(f);
    check(!f.frames.pending() && !f.services.next_frame_deadline() &&
              f.services.surfaces().diagnostics().geometry_updates == diagnostics.geometry_updates &&
              f.services.surfaces().diagnostics().material_updates == diagnostics.material_updates,
          "static loading Timeline did not idle");
    f.services.dispose();
    check(blocked_roots(f.services.interactions()).empty(), "boundary disposal leaked block");
}
} // namespace

int main() {
#if defined(_MSC_VER) && defined(_DEBUG)
    _CrtSetReportMode(_CRT_ASSERT, _CRTDBG_MODE_FILE);
    _CrtSetReportFile(_CRT_ASSERT, _CRTDBG_FILE_STDERR);
#endif
    try {
        popup_authority();
        callbacks_and_blocks();
        reentrant_rebind();
        boundaries_theme_idle();
        std::cout << "Timeline integration passed\n";
        return 0;
    } catch (const std::exception& e) {
        std::cerr << e.what() << '\n';
        return 1;
    }
}
