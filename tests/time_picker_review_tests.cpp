#include "component/time_picker_component.hpp"
#include "component/form_component.hpp"
#include "support/input_fixture.hpp"
#include <iostream>
#include <string_view>
#if defined(_MSC_VER) && defined(_DEBUG)
#include <crtdbg.h>
#endif

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
    check(f.services.layout_and_synchronize({640, 640}, {0, 0, 640, 640}), "review layout");
}

runtime::Rect bounds(Fixture& f, runtime::ComponentId id) {
    const auto& n = f.nodes.require(f.services.components().root(id));
    return {n.bounds.x + n.translation.x, n.bounds.y + n.translation.y, n.bounds.width, n.bounds.height};
}

void click(Fixture& f, runtime::ComponentId id) {
    const auto b = bounds(f, id);
    for (auto action : {PointerAction::down, PointerAction::up}) {
        f.services.pointer().dispatch(
            {PointerIdentity::mouse(), action, PointerButton::primary, b.x + b.width / 2, b.y + b.height / 2});
    }
    sync(f);
}

void key(Fixture& f, Key key) {
    f.services.focus().dispatch({key, KeyAction::down});
    f.services.focus().dispatch({key, KeyAction::up});
    sync(f);
}

void type(Fixture& f, const InputRef& ref, String text) {
    check(ref.focus({InputFocusCursor::All}), "review input focus");
    check(bool(f.inputs.dispatch(TextCommitted{std::move(text), f.inputs.sessions().active()})), "review text commit");
    sync(f);
}

void controlled_open_authority() {
    for (int action = 0; action < 4; ++action) {
        for (bool accept : {false, true}) {
            Fixture f;
            Signal<bool> open{true};
            TimePickerRef ref;
            int closes = 0;
            int changes = 0;
            f.services.mount(Content{[&] {
                TimePicker(TimePickerProps{}
                               .open(open)
                               .ref(ref)
                               .defaultValue(TimeOfDay{8})
                               .onChange([&](TimePickerValue) { ++changes; })
                               .layout(LayoutStyle{}.width(dp(240)))
                               .onOpenChange([&](bool next) {
                                   if (!next) {
                                       ++closes;
                                       if (accept) {
                                           open.set(false);
                                       }
                                   }
                               }));
                Button(ButtonProps{}.layout(LayoutStyle{}.margin_left(dp(400))), [] { Text(u8"Outside"); });
            }});
            sync(f);
            const auto id = f.services.time_picker().mounted()[0];
            const auto outside = f.buttons.mounted_buttons().back().component;
            check(ref.focus(), "controlled pending focus");
            key(f, Key::down);
            check(f.services.time_picker().snapshot(id).pending == TimeOfDay{9} && changes == 0,
                  "controlled pending close fixture");
            const auto request = [&] {
                const auto s = f.services.time_picker().snapshot(id);
                if (action == 0) {
                    check(ref.focus(), "controlled focus");
                    key(f, Key::escape);
                }
                if (action == 1) {
                    click(f, s.confirm);
                }
                if (action == 2) {
                    click(f, s.cancel);
                }
                if (action == 3) {
                    const auto b = bounds(f, outside);
                    const auto hit = f.services.hit_test().hit_test({b.x + b.width / 2, b.y + b.height / 2});
                    check(hit && f.services.interactions().require(*hit).component == outside,
                          "outside fixture hit is covered");
                    click(f, outside);
                }
            };
            request();
            check(f.services.time_picker().snapshot(id).visible == !accept,
                  "R1: ordinary controlled close request overrode parent authority");
            check(closes == 1, "one user close action did not emit exactly one request");
            check(f.services.time_picker().snapshot(id).value == TimeOfDay{action == 1 ? 9 : 8} &&
                      changes == (action == 1 ? 1 : 0),
                  "close action committed or discarded the wrong pending value");
            if (!accept) {
                request();
                check(closes == 2 && f.services.time_picker().snapshot(id).visible, "controlled retry lost");
                check(changes == (action == 1 ? 1 : 0), "close retry emitted a duplicate value");
            }
        }
    }
    Fixture f;
    Signal<bool> open{true};
    Signal<bool> busy{false};
    Signal<bool> modal{false};
    int closes = 0;
    f.services.mount(Content{[&] {
        Spin(SpinProps{}.spinning(busy), SpinContent{[&] {
                 TimePicker(TimePickerProps{}.open(open).onOpenChange([&](bool next) {
                     if (!next) {
                         ++closes;
                     }
                 }));
             }});
        Modal(ModalProps{}.open(modal).footer(ModalFooterMode::None), ModalContent{[] { Text(u8"Boundary"); }});
    }});
    sync(f);
    const auto id = f.services.time_picker().mounted()[0];
    const auto selector = f.services.time_picker().snapshot(id).selector;
    for (int boundary = 0; boundary < 4; ++boundary) {
        const int before = closes;
        if (boundary == 0) {
            busy.set(true);
        }
        if (boundary == 1) {
            modal.set(true);
        }
        if (boundary == 2) {
            f.services.components().set_branch_active(id, false);
            f.services.mark_scene_structure_dirty();
        }
        if (boundary == 3) {
            f.services.set_window_active(false);
        }
        sync(f);
        check(open.get() && !f.services.time_picker().snapshot(id).visible && closes == before + 1,
              "forced boundary did not suppress rejected controlled close exactly once");
        if (boundary == 0) {
            busy.set(false);
        }
        if (boundary == 1) {
            modal.set(false);
        }
        if (boundary == 2) {
            f.services.components().set_branch_active(id, true);
            f.services.mark_scene_structure_dirty();
        }
        if (boundary == 3) {
            f.services.set_window_active(true);
        }
        sync(f);
        check(!f.services.time_picker().snapshot(id).visible, "boundary recovery resurrected old popup");
        click(f, selector);
        check(f.services.time_picker().snapshot(id).visible, "explicit open did not clear boundary suppression");
    }
}

void valid_row_activation() {
    for (bool controlled : {false, true}) {
        Fixture f;
        Signal<TimePickerValue> value{TimePickerValue{}};
        int requests = 0;
        f.services.mount(Content{[&] {
            auto props = TimePickerProps{}.defaultOpen().needConfirm(false).onChange([&](TimePickerValue next) {
                check(next == TimeOfDay{}, "zero row requested wrong value");
                ++requests;
            });
            if (controlled) {
                props.value(value);
            }
            TimePicker(std::move(props));
        }});
        sync(f);
        const auto id = f.services.time_picker().mounted()[0];
        auto s = f.services.time_picker().snapshot(id);
        click(f, s.rows[0][0]);
        s = f.services.time_picker().snapshot(id);
        check(requests == 1 && s.visible && (controlled ? !s.value : s.value == TimeOfDay{}),
              "R2: legal unchanged zero row did not activate immediate empty selection");
        if (controlled) {
            click(f, s.rows[0][0]);
            check(requests == 2, "rejected zero row could not retry");
        }
    }
    Fixture f;
    std::vector<TimePickerValue> changes;
    f.services.mount(Content{[&] {
        TimePickerRules rules;
        rules.disabled = [](TimeOfDay v) {
            return v.minute() == 2;
        };
        TimePicker(TimePickerProps{}
                       .defaultOpen()
                       .defaultValue(TimeOfDay{})
                       .needConfirm(false)
                       .rules(rules)
                       .onChange([&](TimePickerValue v) { changes.push_back(v); }));
    }});
    sync(f);
    const auto id = f.services.time_picker().mounted()[0];
    const auto initial = f.services.time_picker().snapshot(id);
    click(f, initial.rows[1][0]);
    check(changes.empty(), "current accepted row emitted a duplicate value");
    key(f, Key::down);
    check(f.services.time_picker().snapshot(id).value == TimeOfDay{0, 1} && changes.size() == 1,
          "selected minute row did not take active-column ownership");
    click(f, initial.rows[1][2]);
    check(f.services.time_picker().snapshot(id).value == TimeOfDay{0, 1} && changes.size() == 1,
          "disabled row activated or published");
    key(f, Key::down);
    check(f.services.time_picker().snapshot(id).value == TimeOfDay{0, 3} && changes.size() == 2,
          "column navigation did not skip disabled row");
}

void reset_owner_draft() {
    for (int mode = 0; mode < 4; ++mode) {
        Fixture f;
        FormRef form;
        FormField<String> field{String{u8"08:00:00"}};
        field.validator([](String) { return String{u8"Validation message"}; });
        InputRef ref;
        Signal<String> help{String{u8"Help"}};
        Signal<ThemeConfig> theme{ThemeConfig{}};
        int changes = 0;
        f.services.mount(Content{[&] {
            Theme(ThemeProps{}.config(theme), ThemeContent{[&] {
                      Form(FormProps{}.ref(form), FormContent{[&] {
                               FormItem(FormItemProps{}.field(String{u8"time"}, field).control(ref).help(help),
                                        FormItemContent{[&] {
                                            TimePicker(TimePickerProps{}
                                                           .ref(ref)
                                                           .defaultOpen(mode == 3)
                                                           .needConfirm(mode != 3)
                                                           .value(bind([=] {
                                                               return parse_time_of_day(field.get().view().bytes());
                                                           }))
                                                           .onChange([&, change = field.onChange()](TimePickerValue v) {
                                                               ++changes;
                                                               if (mode != 3) {
                                                                   change(v ? format_time_of_day(*v) : String{});
                                                               }
                                                           })
                                                           .status(field.status()));
                                        }});
                           }});
                  }});
        }});
        sync(f);
        const auto id = f.services.time_picker().mounted()[0];
        const auto original = f.services.time_picker().snapshot(id);
        if (mode == 3) {
            check(ref.focus(), "pending reset focus");
            sync(f);
            key(f, Key::down);
            check(changes == 1 && field.get() == String{u8"08:00:00"} &&
                      f.services.time_picker().snapshot(id).pending == TimeOfDay{9},
                  "controlled rejected pending reset fixture");
        } else {
            type(f, ref, mode == 1 ? String{u8"09:"} : String{u8"09:30:00"});
        }
        const auto draft = f.services.time_picker().snapshot(id).draft;
        if (mode == 2) {
            check(bool(f.inputs.dispatch(CompositionChanged{String{u8"zhong"}, {}, f.inputs.sessions().active()})),
                  "reset preedit fixture");
        }
        const int before = changes;
        check(form.validate().has_value() && f.inputs.status(original.selector) == InputStatus::Error,
              "Form validation");
        help.set(String{u8"New help"});
        ThemeConfig config;
        config.time_picker.tokens.background = Color::rgba8(20, 30, 40);
        theme.set(config);
        sync(f);
        check(f.services.time_picker().snapshot(id).draft == draft && changes == before,
              "ordinary Form/Theme update erased draft");
        if (mode == 2) {
            check(f.inputs.display_snapshot(original.selector).composing, "ordinary update cancelled preedit");
        }
        check(form.reset(), "Form reset");
        sync(f);
        auto s = f.services.time_picker().snapshot(id);
        check(s.draft == String{u8"08:00:00"} && s.pending == TimeOfDay{8} && s.value == TimeOfDay{8} &&
                  f.inputs.display_snapshot(original.selector).text == "08:00:00" &&
                  !f.inputs.display_snapshot(original.selector).composing,
              "R3: same accepted Form reset retained TimePicker model/input draft");
        key(f, Key::enter);
        key(f, Key::enter);
        s = f.services.time_picker().snapshot(id);
        check(changes == before && field.get() == String{u8"08:00:00"} && s.selector == original.selector &&
                  s.rows == original.rows && s.footer == original.footer,
              "reset resurrected draft or remounted retained controls");
    }
    Fixture life;
    InputRef reused;
    const auto render = [&] {
        TimePicker(TimePickerProps{}.ref(reused).value(TimeOfDay{8}));
    };
    life.services.mount(Content{[&] { Flex(FlexProps{}, FlexContent{render}); }});
    sync(life);
    const auto container = life.services.components().root_components()[0];
    const auto old = life.services.time_picker().mounted()[0];
    const auto stale_reset = detail::capture_form_input(reused).second;
    check(bool(stale_reset) && life.services.destroy(old), "reset generation fixture");
    life.services.append_slot(container, Content{render});
    sync(life);
    const auto fresh = life.services.time_picker().mounted()[0];
    check(fresh != old, "component generation did not advance");
    type(life, reused, String{u8"09:30:00"});
    stale_reset();
    sync(life);
    check(life.services.time_picker().snapshot(fresh).draft == String{u8"09:30:00"}, "stale reset crossed generation");
    const auto current_reset = detail::capture_form_input(reused).second;
    current_reset();
    sync(life);
    check(life.services.time_picker().snapshot(fresh).draft == String{u8"08:00:00"}, "fresh reset owner missing");
    life.services.dispose();
    stale_reset();
    current_reset();
}

void fresh_open_cycles() {
    Fixture f;
    Signal<bool> open{true};
    TimePickerRef ref;
    f.services.mount(Content{
        [&] { TimePicker(TimePickerProps{}.open(open).ref(ref).onOpenChange([&](bool next) { open.set(next); })); }});
    sync(f);
    const auto id = f.services.time_picker().mounted()[0];
    check(ref.focus(), "cycle focus");
    sync(f);
    check(ref.blur(), "cycle blur");
    sync(f);
    check(!open.get(), "blur close not accepted");
    for (int i = 0; i < 3; ++i) {
        open.set(true);
        sync(f);
        check(f.services.time_picker().snapshot(id).visible,
              "R4: stale focus tracking closed programmatic new open cycle");
        sync(f);
        check(f.services.time_picker().snapshot(id).visible, "unfocused programmatic open did not settle");
        open.set(false);
        sync(f);
    }
}

void footer_blur_delivery() {
    {
        Fixture f;
        f.buttons.set_motion_preference(animation::MotionPreference::normal);
        TimePickerRef selector_ref;
        InputRef footer_ref;
        int blurred = 0;
        f.services.mount(Content{[&] {
            TimePicker(TimePickerProps{}.defaultOpen().ref(selector_ref),
                       TimePickerFooter{[&] { Input(InputProps{}.ref(footer_ref).onBlur([&] { ++blurred; })); }});
        }});
        sync(f);
        const auto id = f.services.time_picker().mounted()[0];
        const auto selector = f.inputs.mounted_inputs().front();
        const auto footer = f.inputs.mounted_inputs().back();
        check(footer_ref.focus(), "footer ref focus");
        sync(f);
        const auto old_session = f.inputs.sessions().active();
        check(f.inputs.focused(footer.component) && old_session.owner == footer.editor &&
                  f.inputs.next_caret_deadline().has_value(),
              "NORMAL footer caret fixture");
        key(f, Key::escape);
        check(blurred == 1, "R5: footer Input lost exactly-once onBlur during keyboard close");
        check(!f.services.time_picker().snapshot(id).visible && !f.inputs.focused(footer.component) &&
                  f.inputs.focused(selector.component) && f.services.focus().state().focused == selector.interaction &&
                  f.inputs.sessions().active().owner == selector.editor,
              "hidden footer focus/session or selector restore mismatch");
        check(selector_ref.blur(), "selector was not restored");
        sync(f);
        check(!f.inputs.focused(selector.component) && !f.inputs.sessions().active().owner.valid() &&
                  !f.inputs.next_caret_deadline(),
              "hidden NORMAL footer retained focus/session/caret deadline");
        static_cast<void>(f.frames.consume_request());
        static_cast<void>(f.services.tick_animations(animation::AnimationTime::microseconds(2000000)));
        sync(f);
        static_cast<void>(f.frames.consume_request());
        check(f.services.tick_animations(animation::AnimationTime::microseconds(3000000)) == 0,
              "hidden footer animated after finite focus transitions settled");
        sync(f);
        check(!f.frames.pending() && !f.services.next_frame_deadline() && blurred == 1,
              "hidden footer did offscreen work");
    }
    {
        Fixture f;
        f.buttons.set_motion_preference(animation::MotionPreference::normal);
        TimePickerRef selector_ref;
        InputRef footer_ref;
        std::optional<runtime::ComponentId> owner;
        runtime::ComponentId container;
        int blurred = 0;
        std::function<void()> render;
        render = [&] {
            TimePicker(TimePickerProps{}.ref(selector_ref).defaultOpen(!owner), TimePickerFooter{[&] {
                           Input(InputProps{}.ref(footer_ref).onBlur([&] {
                               ++blurred;
                               if (blurred == 1) {
                                   check(f.services.destroy(*owner), "blur destroy");
                                   f.services.append_slot(container, Content{render});
                               }
                           }));
                       }});
        };
        f.services.mount(Content{[&] { Flex(FlexProps{}, FlexContent{render}); }});
        sync(f);
        container = f.services.components().root_components()[0];
        owner = f.services.time_picker().mounted()[0];
        const auto old = *owner;
        check(footer_ref.focus(), "destroy footer focus");
        sync(f);
        key(f, Key::escape);
        const auto fresh = f.services.time_picker().mounted()[0];
        check(blurred == 1 && fresh != old && !f.services.components().contains(old) &&
                  !f.services.focus().state().focused && !f.inputs.sessions().active().owner.valid() &&
                  !f.inputs.next_caret_deadline(),
              "blur destruction/recreation retained stale focus generation");
        check(selector_ref.focus(), "new ref generation unusable");
        sync(f);
        check(f.inputs.focused(f.services.time_picker().snapshot(fresh).selector), "new generation did not focus");
    }
    {
        Fixture f;
        f.buttons.set_motion_preference(animation::MotionPreference::normal);
        Signal<bool> open{true};
        TimePickerRef selector_ref;
        InputRef footer_ref;
        int blurred = 0;
        f.services.mount(Content{[&] {
            TimePicker(TimePickerProps{}.open(open).ref(selector_ref).onOpenChange([&](bool next) { open.set(next); }),
                       TimePickerFooter{[&] {
                           Input(InputProps{}.ref(footer_ref).onBlur([&] {
                               if (++blurred == 1) {
                                   open.set(true);
                               }
                           }));
                       }});
        }});
        sync(f);
        const auto id = f.services.time_picker().mounted()[0];
        const auto footer = f.inputs.mounted_inputs().back();
        check(footer_ref.focus(), "reopen footer focus");
        sync(f);
        key(f, Key::escape);
        check(blurred == 1 && open.get() && f.services.time_picker().snapshot(id).visible &&
                  !f.inputs.focused(footer.component),
              "synchronous blur reopen lost new cycle or old Input focus");
        sync(f);
        check(f.services.time_picker().snapshot(id).visible, "reopened cycle immediately closed");
        check(selector_ref.blur(), "reopened selector restore");
        sync(f);
        check(!f.inputs.next_caret_deadline() && blurred == 1, "reopen/close leaked hidden caret or repeated blur");
    }
}
} // namespace

int main(int argc, char** argv) {
#if defined(_MSC_VER) && defined(_DEBUG)
    _CrtSetReportMode(_CRT_ASSERT, _CRTDBG_MODE_FILE);
    _CrtSetReportFile(_CRT_ASSERT, _CRTDBG_FILE_STDERR);
#endif
    try {
        const std::string_view name = argc > 1 ? argv[1] : "";
        if (name == "open") {
            controlled_open_authority();
        } else if (name == "activation") {
            valid_row_activation();
        } else if (name == "reset") {
            reset_owner_draft();
        } else if (name == "cycles") {
            fresh_open_cycles();
        } else if (name == "focus") {
            footer_blur_delivery();
        } else {
            throw std::runtime_error("unknown TimePicker review case");
        }
        std::cout << "TimePicker review " << name << " passed\n";
        return 0;
    } catch (const std::exception& e) {
        std::cerr << e.what() << '\n';
        return 1;
    }
}
