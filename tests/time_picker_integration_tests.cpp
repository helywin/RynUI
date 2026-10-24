#include "component/time_picker_component.hpp"
#include "component/select_component.hpp"
#include "component/table_component.hpp"
#include "component/modal_component.hpp"
#include "component/date_picker_component.hpp"
#include "component/form_component.hpp"
#include "component/dropdown_component.hpp"
#include "support/input_fixture.hpp"
#include <iostream>
#include <cmath>
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
    check(f.services.layout_and_synchronize({640, 640}, {0, 0, 640, 640}), "integration layout");
}

runtime::Rect bounds(Fixture& f, runtime::ComponentId id) {
    const auto& n = f.nodes.require(f.services.components().root(id));
    return {n.bounds.x + n.translation.x, n.bounds.y + n.translation.y, n.bounds.width, n.bounds.height};
}

void click(Fixture& f, runtime::ComponentId id) {
    const auto b = bounds(f, id);
    for (auto a : {PointerAction::down, PointerAction::up}) {
        f.services.pointer().dispatch(
            {PointerIdentity::mouse(), a, PointerButton::primary, b.x + b.width / 2, b.y + b.height / 2});
    }
    sync(f);
}

void key(Fixture& f, Key key) {
    f.services.focus().dispatch({key, KeyAction::down});
    f.services.focus().dispatch({key, KeyAction::up});
    sync(f);
}

std::vector<SelectOption> options() {
    std::vector<SelectOption> result;
    for (int i = 0; i < 30; ++i) {
        result.push_back({i, String{u8"Option"}});
    }
    return result;
}

struct Row final {
    String id;
    friend bool operator==(const Row&, const Row&) = default;
};

std::vector<Row> rows() {
    std::vector<Row> result;
    for (int i = 0; i < 12; ++i) {
        result.push_back({String::from_utf8(std::to_string(i)).value()});
    }
    return result;
}

void table(Signal<std::vector<Row>> source) {
    Table(TableProps<Row>{}
              .dataSource(source)
              .rowKey([](const Row& r) { return r.id; })
              .columns({TableColumn<Row>{u8"value", u8"Value", [](TableCell<Row>) { Text(u8"Cell"); }}.width(600)})
              .scrollY(70)
              .layout(LayoutStyle{}.width(dp(300))));
}

void footer_content_before_boundary() {
    Fixture f;
    Signal<std::vector<Row>> data{rows()};
    Signal<bool> child_open{false};
    ThemeConfig config;
    config.time_picker.tokens.column_height = dp(84);
    f.services.mount(Content{[&] {
        Theme(ThemeProps{}.config(config), ThemeContent{[&] {
                  TimePicker(TimePickerProps{}.defaultOpen().layout(LayoutStyle{}.width(dp(340))),
                             TimePickerFooter{[&] {
                                 Flex(FlexProps{}.vertical(true), FlexContent{[&] {
                                          table(data);
                                          Select(SelectProps{}
                                                     .options(options())
                                                     .open(child_open)
                                                     .listHeight(70)
                                                     .layout(LayoutStyle{}.width(dp(250))));
                                      }});
                             }});
                  table(data);
              }});
    }});
    sync(f);
    const auto picker = f.services.time_picker().mounted()[0];
    const auto original = f.services.time_picker().snapshot(picker);
    const auto footer_table = f.services.table().mounted()[0];
    const auto underneath = f.services.table().mounted()[1];
    const auto select = f.services.select().mounted()[0];
    auto t = f.services.table().snapshot(footer_table);
    std::cout << "footer Table body=" << t.body.x << ',' << t.body.y << ',' << t.body.width << ',' << t.body.height
              << " max_y=" << t.maximum_y << '\n';
    check(t.maximum_y > 0 && t.body.height == 70, "footer Table needs real scroll body");
    check(f.services.dispatch_scroll({0, -1, t.body.x + 10, t.body.y + 10}), "footer Table wheel unhandled");
    sync(f);
    const auto scroll = f.services.table().snapshot(footer_table).scroll_y;
    check(scroll > 0 && f.services.time_picker().snapshot(picker).scroll == original.scroll &&
              f.services.table().snapshot(underneath).scroll_y == 0,
          "TimePicker boundary swallowed child Table wheel");
    child_open.set(true);
    sync(f);
    const auto child = f.services.select().snapshot(select);
    check(child.visible && child.popup.height > 0, "footer Select popup missing");
    check(f.services.dispatch_scroll({0, -1, child.popup.x + 5, child.popup.y + 5}), "child Select wheel unhandled");
    sync(f);
    check(f.services.select().snapshot(select).scroll > child.scroll &&
              f.services.table().snapshot(footer_table).scroll_y == scroll &&
              f.services.table().snapshot(underneath).scroll_y == 0,
          "nearest child popup lost wheel ownership");
    child_open.set(false);
    data.set({});
    sync(f);
    t = f.services.table().snapshot(footer_table);
    check(t.maximum_x > 0 && t.rows.empty(), "empty footer Table fixture");
    check(f.services.dispatch_scroll({1, 0, t.body.x + 10, t.body.y + 10}), "empty Table body wheel unhandled");
    sync(f);
    check(f.services.table().snapshot(footer_table).scroll_x > 0, "empty Table background lost wheel");
    check(f.services.dispatch_scroll({0, -1, original.bounds.x + 1, original.bounds.y + 1}),
          "popup padding not consumed");
    check(f.services.table().snapshot(underneath).scroll_y == 0, "popup padding scrolled underlying Table");
    const auto stable = f.services.time_picker().snapshot(picker);
    check(stable.selector == original.selector && stable.footer == original.footer && stable.rows == original.rows,
          "footer interaction remounted picker");
}

void nested_and_overlapping_popups() {
    {
        Fixture f;
        TimePickerRef child_ref;
        ThemeConfig config;
        config.time_picker.tokens.column_height = dp(84);
        f.services.mount(Content{[&] {
            Theme(ThemeProps{}.config(config), ThemeContent{[&] {
                      TimePicker(TimePickerProps{}.defaultOpen().layout(LayoutStyle{}.width(dp(280))),
                                 TimePickerFooter{[&] { TimePicker(TimePickerProps{}.defaultOpen().ref(child_ref)); }});
                  }});
        }});
        sync(f);
        const auto parent = f.services.time_picker().mounted().back();
        const auto child = f.services.time_picker().mounted().front();
        const auto p = f.services.time_picker().snapshot(parent);
        const auto c = f.services.time_picker().snapshot(child);
        const auto anchor = bounds(f, c.selector);
        check(p.visible && c.visible && c.bounds.y >= anchor.y + anchor.height,
              "nested popup not positioned after parent");
        const auto clip = c.column_clips[0];
        check(f.services.dispatch_scroll({0, -2, clip.x + 5, clip.y + 5}), "nested TimePicker wheel");
        sync(f);
        check(f.services.time_picker().snapshot(child).scroll[0] > c.scroll[0] &&
                  f.services.time_picker().snapshot(parent).scroll == p.scroll,
              "parent stole child wheel");
        check(child_ref.focus(), "nested child focus");
        key(f, Key::escape);
        check(!f.services.time_picker().snapshot(child).visible && f.services.time_picker().snapshot(parent).visible,
              "child Escape closed parent");
    }
    {
        Fixture f;
        ThemeConfig config;
        config.time_picker.tokens.column_height = dp(100);
        f.services.mount(Content{[&] {
            Theme(ThemeProps{}.config(config), ThemeContent{[] {
                      TimePicker(TimePickerProps{}.defaultOpen().layout(LayoutStyle{}.width(dp(280))));
                      Select(SelectProps{}.options(options()).defaultOpen(true).listHeight(100).layout(
                          LayoutStyle{}.width(dp(280))));
                  }});
        }});
        sync(f);
        const auto picker = f.services.time_picker().mounted()[0];
        const auto select = f.services.select().mounted()[0];
        const auto p = f.services.time_picker().snapshot(picker);
        const auto s = f.services.select().snapshot(select);
        const float x = s.popup.x + 10;
        const float y = s.popup.y + 10;
        check(y >= p.bounds.y && y < p.bounds.y + p.bounds.height, "sibling fixture does not overlap");
        check(f.services.dispatch_scroll({0, -1, x, y}), "overlapping sibling wheel");
        sync(f);
        check(f.services.select().snapshot(select).scroll > s.scroll &&
                  f.services.time_picker().snapshot(picker).scroll == p.scroll,
              "sibling popup stole topmost wheel");
    }
}

void suppression_and_lifetime() {
    Fixture f;
    TimePickerRef ref;
    Signal<bool> open{true};
    Signal<bool> busy{false};
    Signal<bool> modal{false};
    int closes = 0;
    f.services.mount(Content{[&] {
        Spin(SpinProps{}.spinning(busy), SpinContent{[&] {
                 TimePicker(TimePickerProps{}.open(open).ref(ref).onOpenChange([&](bool value) {
                     if (!value) {
                         ++closes;
                     }
                 }));
             }});
        Modal(ModalProps{}.open(modal).footer(ModalFooterMode::None), ModalContent{[] { Text(u8"Modal"); }});
    }});
    sync(f);
    const auto id = f.services.time_picker().mounted()[0];
    const auto s = f.services.time_picker().snapshot(id);
    check(s.visible, "initial controlled open");
    busy.set(true);
    sync(f);
    busy.set(false);
    sync(f);
    check(!f.services.time_picker().snapshot(id).visible && closes == 1, "Spin resurrected controlled popup");
    click(f, s.selector);
    check(f.services.time_picker().snapshot(id).visible, "explicit request did not unsuppress");
    modal.set(true);
    sync(f);
    modal.set(false);
    sync(f);
    check(!f.services.time_picker().snapshot(id).visible, "Modal restored suppressed popup");
    click(f, s.selector);
    f.services.components().set_branch_active(id, false);
    sync(f);
    f.services.components().set_branch_active(id, true);
    sync(f);
    check(!f.services.time_picker().snapshot(id).visible, "hidden branch resurrected popup");
    click(f, s.selector);
    f.services.set_window_active(false);
    sync(f);
    f.services.set_window_active(true);
    sync(f);
    check(!f.services.time_picker().snapshot(id).visible, "inactive window resurrected popup");
    f.services.dispose();
    check(!ref.bound() && !ref.focus() && f.services.interactions().size() == 0, "dispose leaked ref/hits");
    for (bool destroy_on_open : {false, true}) {
        Fixture d;
        TimePickerRef stale;
        std::optional<runtime::ComponentId> owner;
        d.services.mount(Content{[&] {
            TimePicker(TimePickerProps{}
                           .ref(stale)
                           .defaultValue(TimeOfDay{})
                           .onChange([&](TimePickerValue) {
                               if (!destroy_on_open && owner) {
                                   d.services.destroy(*owner);
                               }
                           })
                           .onOpenChange([&](bool value) {
                               if (destroy_on_open && value && owner) {
                                   d.services.destroy(*owner);
                               }
                           }));
        }});
        sync(d);
        owner = d.services.time_picker().mounted()[0];
        click(d, d.services.time_picker().snapshot(*owner).selector);
        if (!destroy_on_open) {
            key(d, Key::down);
            key(d, Key::enter);
        }
        sync(d);
        check(!stale.bound() && !stale.focus() && !d.services.components().contains(*owner),
              "callback destruction unsafe");
    }
}

void inside_modal_and_read_only() {
    Fixture f;
    TimePickerRef ref;
    Signal<bool> disabled{false};
    f.services.mount(Content{[&] {
        Modal(ModalProps{}.open(true).footer(ModalFooterMode::None), ModalContent{[&] {
                  TimePicker(TimePickerProps{}.defaultOpen().ref(ref).inputReadOnly().disabled(disabled));
              }});
    }});
    sync(f);
    const auto id = f.services.time_picker().mounted()[0];
    auto s = f.services.time_picker().snapshot(id);
    check(s.visible && ref.focus(), "picker inside Modal unavailable");
    key(f, Key::down);
    key(f, Key::enter);
    s = f.services.time_picker().snapshot(id);
    check(s.value == TimeOfDay{1}, "read-only Input blocked column selection");
    disabled.set(true);
    sync(f);
    click(f, s.selector);
    check(!f.services.time_picker().snapshot(id).visible && !ref.focus(), "disabled picker opened/focused");
    disabled.set(false);
    sync(f);
    check(!f.services.time_picker().snapshot(id).visible, "re-enabling resurrected popup");
}

void mixed_date_time_footer_ownership() {
    for (bool date_parent : {false, true}) {
        Fixture f;
        DatePickerRef date_ref;
        TimePickerRef time_ref;
        Signal<bool> dropdown_open{false};
        ThemeConfig config;
        config.time_picker.tokens.column_height = dp(84);
        f.services.mount(Content{[&] {
            Theme(ThemeProps{}.config(config), ThemeContent{[&] {
                      if (date_parent) {
                          DatePicker(
                              DatePickerProps{}.defaultOpen().ref(date_ref).defaultPanelDate(CivilDate{2026, 10, 25}),
                              DatePickerFooter{[&] { TimePicker(TimePickerProps{}.defaultOpen().ref(time_ref)); }});
                      } else {
                          TimePicker(TimePickerProps{}.defaultOpen().ref(time_ref), TimePickerFooter{[&] {
                                         DatePicker(DatePickerProps{}.defaultOpen().ref(date_ref).defaultPanelDate(
                                                        CivilDate{2026, 10, 25}),
                                                    DatePickerFooter{[&] {
                                                        Dropdown(DropdownProps{}
                                                                     .open(dropdown_open)
                                                                     .menu(MenuProps{}.items({{u8"one", u8"One"}})),
                                                                 DropdownTrigger{[] {
                                                                     Button(ButtonProps{}, [] { Text(u8"Actions"); });
                                                                 }});
                                                    }});
                                     }});
                      }
                  }});
        }});
        sync(f);
        const auto time = f.services.time_picker().mounted()[0];
        const auto date = f.services.date_picker().mounted()[0];
        const auto t = f.services.time_picker().snapshot(time);
        const auto d = f.services.date_picker().snapshot(date);
        const auto input = bounds(f, date_parent ? t.selector : d.input);
        const auto popup = date_parent ? t.bounds : d.bounds;
        const auto gap = date_parent ? config.time_picker.tokens.gap.value_or(dp(4)).value()
                                     : f.services.components().theme_scope(date)->snapshot().date_picker().metrics.gap;
        check(t.visible && d.visible && std::abs(popup.y - (input.y + input.height + gap)) < .01F,
              "mixed footer popup did not use the final parent anchor on its first frame");
        if (date_parent) {
            check(time_ref.focus(), "mixed child Time focus");
            key(f, Key::escape);
            check(!f.services.time_picker().snapshot(time).visible && f.services.date_picker().snapshot(date).visible,
                  "child Time Escape closed parent Date");
            click(f, t.selector);
            const auto c = t.column_clips[0];
            check(f.services.dispatch_scroll({0, -2, c.x + 4, c.y + 4}), "Date footer Time wheel unhandled");
            sync(f);
            check(f.services.time_picker().snapshot(time).scroll[0] > t.scroll[0] &&
                      f.services.date_picker().snapshot(date).bounds == d.bounds,
                  "Date boundary consumed child Time column wheel");
        } else {
            check(date_ref.focus(), "mixed child Date focus");
            key(f, Key::escape);
            check(!f.services.date_picker().snapshot(date).visible && f.services.time_picker().snapshot(time).visible,
                  "child Date Escape closed parent Time");
            click(f, d.input);
            check(f.services.dispatch_scroll({0, -2, d.bounds.x + 2, d.bounds.y + 2}),
                  "Time footer Date boundary unhandled");
            sync(f);
            check(f.services.time_picker().snapshot(time).scroll == t.scroll,
                  "Time column scrolled through its Date child popup");
            dropdown_open.set(true);
            sync(f);
            const auto dropdown = f.services.dropdown().snapshot(f.services.dropdown().mounted()[0]);
            check(dropdown.geometry.visible && f.services.dispatch_scroll({0, -2, dropdown.geometry.bounds.x + 2,
                                                                           dropdown.geometry.bounds.y + 2}),
                  "mixed Dropdown boundary unhandled");
            sync(f);
            check(f.services.time_picker().snapshot(time).scroll == t.scroll &&
                      f.services.date_picker().snapshot(date).bounds == d.bounds,
                  "mixed boundary chain leaked child Dropdown wheel");
        }
    }
}

void form_with_typed_picker_adapters() {
    Fixture f;
    FormRef form;
    FormField<String> date_text;
    FormField<String> time_text;
    date_text.required();
    time_text.required();
    DatePickerRef date_ref;
    TimePickerRef time_ref;
    Signal<bool> disabled{false};
    int submits = 0;
    f.services.mount(Content{[&] {
        Form(FormProps{}.ref(form).disabled(disabled).onFinish([&](FormResult r) {
            check(r.valid(), "Form submitted invalid pickers");
            ++submits;
        }),
             FormContent{[&] {
                 // Form075 has a finite String field type. The application adapter
                 // keeps both picker APIs typed; no new built-in Form value kind.
                 FormItem(FormItemProps{}.field(String{u8"date"}, date_text).control(date_ref), FormItemContent{[&] {
                              DatePicker(
                                  DatePickerProps{}
                                      .value(bind([=] { return parse_iso_date(date_text.get().view().bytes()); }))
                                      .onChange([change = date_text.onChange()](DatePickerValue v) {
                                          change(v ? format_iso_date(*v) : String{});
                                      })
                                      .disabled(date_text.uneditable())
                                      .status(date_text.status())
                                      .ref(date_ref));
                          }});
                 FormItem(FormItemProps{}.field(String{u8"time"}, time_text).control(time_ref), FormItemContent{[&] {
                              TimePicker(
                                  TimePickerProps{}
                                      .value(bind([=] { return parse_time_of_day(time_text.get().view().bytes()); }))
                                      .onChange([change = time_text.onChange()](TimePickerValue v) {
                                          change(v ? format_time_of_day(*v) : String{});
                                      })
                                      .disabled(time_text.uneditable())
                                      .status(time_text.status())
                                      .ref(time_ref));
                          }});
             }});
    }});
    sync(f);
    const auto time = f.services.time_picker().mounted()[0];
    const auto date = f.services.date_picker().mounted()[0];
    const auto original_time = f.services.time_picker().snapshot(time);
    const auto original_date = f.services.date_picker().snapshot(date);
    check(form.submit() && submits == 0 && f.inputs.status(original_time.selector) == InputStatus::Error,
          "Form required/status adapter failed");
    check(date_ref.focus({InputFocusCursor::All}), "Form Date focus");
    check(bool(f.inputs.dispatch(TextCommitted{String{u8"2026-10-25"}, f.inputs.sessions().active()})),
          "Form Date text");
    key(f, Key::enter);
    check(time_ref.focus({InputFocusCursor::All}), "Form Time focus");
    check(bool(f.inputs.dispatch(TextCommitted{String{u8"09:30:00"}, f.inputs.sessions().active()})), "Form Time text");
    key(f, Key::enter);
    check(form.submit() && submits == 1 && date_text.get() == String{u8"2026-10-25"} &&
              time_text.get() == String{u8"09:30:00"},
          "Form typed picker adapter submit");
    disabled.set(true);
    sync(f);
    check(!time_ref.focus() && !date_ref.focus(), "Form disabled did not reach picker Inputs");
    disabled.set(false);
    check(form.reset(), "Form reset");
    sync(f);
    check(!f.services.time_picker().snapshot(time).value && !f.services.date_picker().snapshot(date).value &&
              f.services.time_picker().snapshot(time).selector == original_time.selector &&
              f.services.date_picker().snapshot(date).input == original_date.input,
          "Form reset changed picker identities or retained accepted values");
}
} // namespace

int main() {
#if defined(_MSC_VER) && defined(_DEBUG)
    _CrtSetReportMode(_CRT_ASSERT, _CRTDBG_MODE_FILE);
    _CrtSetReportFile(_CRT_ASSERT, _CRTDBG_FILE_STDERR);
#endif
    try {
        footer_content_before_boundary();
        nested_and_overlapping_popups();
        suppression_and_lifetime();
        inside_modal_and_read_only();
        mixed_date_time_footer_ownership();
        form_with_typed_picker_adapters();
        std::cout << "TimePicker integration contracts passed\n";
        return 0;
    } catch (const std::exception& e) {
        std::cerr << e.what() << '\n';
        return 1;
    }
}
