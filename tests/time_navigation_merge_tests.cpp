#include "component/breadcrumb_component.hpp"
#include "component/steps_component.hpp"
#include "component/time_picker_component.hpp"
#include "support/input_fixture.hpp"
#include <iostream>

namespace {
using namespace ryn;
using namespace ryn::input;
using Fixture = ryn_test::input_component::Fixture;

void check(bool value, const char* message) {
    if (!value) {
        throw std::runtime_error(message);
    }
}

void sync(Fixture& f) {
    check(f.services.layout_and_synchronize({640, 640}, {0, 0, 640, 640}), "merge layout");
}

void edit(Fixture& f, const InputRef& ref, String value) {
    check(ref.focus({InputFocusCursor::All}), "merged native Input focus");
    check(bool(f.inputs.dispatch(TextCommitted{std::move(value), f.inputs.sessions().active()})), "merged native edit");
    sync(f);
}

void coexistence() {
    Fixture f;
    TimePickerRef time;
    InputRef breadcrumb_input;
    InputRef steps_input;
    Signal<ThemeConfig> theme{ThemeConfig{}};
    Signal<bool> breadcrumb_visible{true};
    Signal<bool> steps_disabled{false};
    int slots = 0;
    int changes = 0;
    f.services.mount(Content{[&] {
        Theme(ThemeProps{}.config(theme), ThemeContent{[&] {
                  TimePicker(TimePickerProps{}
                                 .defaultOpen()
                                 .defaultValue(TimeOfDay{8})
                                 .needConfirm(false)
                                 .ref(time)
                                 .layout(LayoutStyle{}.width(dp(400)))
                                 .onChange([&](TimePickerValue) { ++changes; }),
                             TimePickerFooter{[&] {
                                 Breadcrumb(BreadcrumbProps{}, {BreadcrumbItem{u8"current", u8"Current"}
                                                                    .visible(breadcrumb_visible)
                                                                    .richTitle(BreadcrumbTitle{[&] {
                                                                        ++slots;
                                                                        Input(InputProps{}.ref(breadcrumb_input));
                                                                    }})});
                                 Steps(StepsProps{}, {StepsItem{u8"entry", u8"Entry"}
                                                          .disabled(steps_disabled)
                                                          .richDescription(StepsDescription{[&] {
                                                              ++slots;
                                                              Input(InputProps{}.ref(steps_input));
                                                          }})});
                             }});
              }});
    }});
    sync(f);
    const auto owner = f.services.time_picker().mounted()[0];
    const auto before = f.services.time_picker().snapshot(owner);
    check(f.services.breadcrumb().mounted().size() == 1 && f.services.steps().mounted().size() == 1 && slots == 2,
          "coexisting hosts or typed slots missing");
    const auto breadcrumb = f.inputs.mounted_inputs()[1];
    const auto steps = f.inputs.mounted_inputs()[2];
    edit(f, breadcrumb_input, String{u8"Breadcrumb draft"});
    ThemeConfig next;
    next.time_picker.tokens.background = Color::rgba8(25, 35, 45);
    next.breadcrumb.tokens.current = Color::rgba8(60, 70, 80);
    next.steps.tokens.title = Color::rgba8(90, 100, 110);
    theme.set(next);
    sync(f);
    check(slots == 2 && f.services.time_picker().snapshot(owner).rows == before.rows &&
              f.inputs.display_snapshot(breadcrumb.component).text == "Breadcrumb draft" &&
              f.inputs.sessions().active().owner == breadcrumb.editor,
          "combined Theme update remounted or reconciled an unrelated draft/session");

    const auto& node = f.nodes.require(f.services.components().root(before.rows[0][1]));
    const float x = node.bounds.x + node.translation.x + node.bounds.width / 2;
    const float y = node.bounds.y + node.translation.y + node.bounds.height / 2;
    const auto hit = f.services.hit_test().hit_test({x, y});
    check(hit && f.services.interactions().require(*hit).component == before.rows[0][1], "merged row hit clipped");
    for (auto action : {PointerAction::down, PointerAction::up}) {
        f.services.pointer().dispatch({PointerIdentity::mouse(), action, PointerButton::primary, x, y});
    }
    sync(f);
    check(changes == 1 && f.services.time_picker().snapshot(owner).value == TimeOfDay{1} &&
              f.inputs.display_snapshot(breadcrumb.component).text == "Breadcrumb draft",
          "Nav child blur altered Time activation or native draft");
    edit(f, steps_input, String{u8"Steps draft"});
    check(time.focus(), "parent selector focus");
    breadcrumb_visible.set(false);
    steps_disabled.set(true);
    sync(f);
    check(!breadcrumb_input.focus() && !steps_input.focus() && f.services.time_picker().snapshot(owner).visible,
          "Nav input blocking affected the wrong owner");
    breadcrumb_visible.set(true);
    steps_disabled.set(false);
    sync(f);
    check(slots == 2 && breadcrumb_input.focus() && steps_input.focus() &&
              f.inputs.display_snapshot(steps.component).text == "Steps draft" &&
              f.services.time_picker().snapshot(owner).rows == before.rows,
          "Nav recovery changed retained identities or left input blocked");
    f.services.focus().dispatch({Key::escape, KeyAction::down});
    f.services.focus().dispatch({Key::escape, KeyAction::up});
    sync(f);
    check(!f.services.time_picker().snapshot(owner).visible && !breadcrumb_input.focus() && !steps_input.focus(),
          "Nav child Escape lost parent popup ownership");
    check(time.blur(), "merged selector restore");
    sync(f);
    check(!f.inputs.sessions().active().owner.valid() && !f.inputs.next_caret_deadline(),
          "merged hidden caret/session");
}
} // namespace

int main() {
    try {
        coexistence();
        std::cout << "Time/Breadcrumb/Steps coexistence passed\n";
        return 0;
    } catch (const std::exception& error) {
        std::cerr << error.what() << '\n';
        return 1;
    }
}
