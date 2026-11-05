#include "support/retained_media_fixture.hpp"
#include "support/data_display_fixture.hpp"
#include <cmath>

int main() {
    using namespace ryn;
    using namespace ryn::input;
    using paint_test::check;
    try {
        retained_media_test::Scene scene;
        auto& f = scene.fixture;
        check(f.inputs.mounted_inputs().empty() && f.services.images().live_count() == 0,
              "lazy Collapse pre-mounted its body");
        scene.opacity(.5F);
        scene.open();
        check(std::abs(scene.image().opacity - .5F) < .0001F && scene.factories == 1,
              "lazy rich image created under non-1 ancestor omitted envelope");
        check(scene.input.focus({InputFocusCursor::All}), "rich Form input did not auto-bind/focus");
        check(bool(f.inputs.dispatch(TextCommitted{String{u8"edited draft"}, f.inputs.sessions().active()})),
              "rich Form commit failed");
        scene.sync();
        check(scene.field.get() == String{u8"edited draft"}, "rich nested FormItem lost automatic binding");
        data_display_test::key(f, Key::enter);
        check(scene.submissions == 1, "outer data-display controls stole rich Form Enter");
        check(scene.form.reset() && scene.input.focus(), "rich Form reset/focus failed");
        const auto stamp = f.inputs.sessions().active();
        check(bool(f.inputs.dispatch(CompositionChanged{String{u8"候选"}, {}, stamp})), "rich Form preedit setup");
        check(scene.form.reset() && !f.inputs.editors().require(stamp.owner).composition().active &&
                  scene.field.get() == String{u8"initial draft"},
              "same-value rich Form reset retained preedit");
        const auto old_image = scene.image_owner();
        scene.revision.set(1);
        scene.sync();
        scene.sync();
        check(!f.services.components().contains(old_image) && scene.factories == 2 &&
                  std::abs(scene.image().opacity - .5F) < .0001F && scene.input.focus(),
              "rich replacement under opacity lost generation/envelope/Form binding");
        check(f.inputs.editors().require(f.inputs.sessions().active().owner).value() == "initial draft",
              "local content revision lost authoritative Form value");
        scene.opacity(0);
        check(scene.image().opacity == 0, "zero rich media envelope discarded source");
        scene.opacity(1);
        check(scene.image().opacity == 1, "rich media envelope accumulated across restore");
        const auto popup = f.services.popover().mounted().front();
        const auto trigger = f.services.popover().snapshot(popup).trigger;
        runtime::ComponentId trigger_button;
        for (const auto& button : f.buttons.mounted_buttons()) {
            for (auto parent = std::optional{button.component}; parent;
                 parent = f.services.components().parent(*parent)) {
                if (*parent == trigger) {
                    trigger_button = button.component;
                    break;
                }
            }
        }
        check(trigger_button.valid(), "rich Popover native trigger Button missing");
        data_display_test::click(f, trigger_button);
        check(f.services.popover().snapshot(popup).geometry.visible, "rich Popover could not open");
        const auto select = f.services.select().mounted().front();
        data_display_test::click(f, f.services.select().snapshot(select).selector);
        check(f.services.select().snapshot(select).visible, "nested rich Select could not open");
        data_display_test::key(f, Key::escape);
        check(!f.services.select().snapshot(select).visible && f.services.popover().snapshot(popup).geometry.visible,
              "data-display wrapper stole innermost Escape");
        check(scene.carousel.next(), "retained rich Carousel page change failed");
        scene.sync();
        check(!f.services.popover().snapshot(popup).geometry.visible && !f.inputs.sessions().active().valid() &&
                  !scene.input.focus(),
              "outgoing rich panel leaked popup/session/focus");
        check(scene.carousel.goTo(0, true), "retained rich Carousel restore failed");
        scene.sync();
        scene.sync();
        check(scene.factories == 2 && scene.input.focus(), "rich panel restoration rebuilt drafts");
        scene.active.set({});
        scene.sync();
        check(!f.inputs.sessions().active().valid() && !scene.input.focus(), "Collapse close retained rich input");
        while (f.frames.consume_request()) {
        }
        const auto requests = f.frames.counters().requests;
        scene.sync();
        scene.sync();
        check(f.frames.counters().requests == requests && !f.services.next_frame_deadline(),
              "settled retained composition continued requesting frames");
        f.services.dispose();
        check(f.nodes.size() == 0 && f.services.images().live_count() == 0 && !f.nodes.has_paint_opacity() &&
                  !f.services.next_frame_deadline() && f.services.interactions().size() == 0,
              "retained data-display composition leaked media/input/opacity/timer");
    } catch (const std::exception& error) {
        std::cerr << error.what() << '\n';
        return 1;
    }
}
