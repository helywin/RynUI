#include "support/mentions_fixture.hpp"
#include "component/form_component.hpp"
#include "component/selection_component.hpp"
#include "component/transfer_component.hpp"
#include <ryn/auto_complete.hpp>
#include <ryn/form.hpp>
#include <ryn/date_picker.hpp>
#include <ryn/time_picker.hpp>
#include <ryn/cascader.hpp>
#include <ryn/transfer.hpp>
#include <ryn/color_picker.hpp>
#include <ryn/calendar.hpp>
#include <ryn/carousel.hpp>

namespace {
using namespace mentions_test;

void private_editors_leave_automatic_binding_to_public_controls() {
    Fixture f;
    FormRef form;
    FormField<String> mention_field{String{u8"mention authority"}};
    FormField<String> complete_field{String{u8"complete authority"}};
    InputRef first;
    InputRef second;
    int submitted{};
    f.services.mount(Content{[&] {
        Form(FormProps{}.ref(form).onFinish([&](FormResult) { ++submitted; }), FormContent{[&] {
                 FormItem(FormItemProps{}.field(u8"mentions", mention_field), FormItemContent{[&] {
                              Mentions(MentionsProps{}.defaultValue(u8"private mentions"));
                              Input(InputProps{}.ref(first));
                          }});
                 FormItem(FormItemProps{}.field(u8"complete", complete_field), FormItemContent{[&] {
                              AutoComplete(AutoCompleteProps{}.defaultValue(u8"private complete"));
                              Input(InputProps{}.ref(second));
                          }});
             }});
    }});
    sync(f, 720, 600);
    const auto inputs = f.inputs.mounted_inputs();
    check(inputs.size() == 4, "compound integration editor count");
    check(f.inputs.display_snapshot(inputs[0].component).text == "private mentions" &&
              f.inputs.display_snapshot(inputs[1].component).text == "mention authority" &&
              f.inputs.display_snapshot(inputs[2].component).text == "private complete" &&
              f.inputs.display_snapshot(inputs[3].component).text == "complete authority",
          "compound private editor claimed a FormItem automatic control slot");
    check(first.focus(), "public Input focus after Mentions");
    edit(f, u8"accepted first");
    check(mention_field.get() == String{u8"accepted first"}, "public control lost typed Form binding");
    check(second.focus(), "public Input focus after AutoComplete");
    edit(f, u8"accepted second");
    check(complete_field.get() == String{u8"accepted second"}, "second public control lost typed binding");
    key(f, Key::enter);
    check(submitted == 1, "private editor consumed automatic Enter ownership");
    check(form.reset() && mention_field.get() == String{u8"mention authority"} &&
              complete_field.get() == String{u8"complete authority"},
          "compound integration reset lost accepted authorities");
    clean(f);
}

void popup_editors_keep_their_own_value_and_enter() {
    Fixture f;
    FormRef form;
    FormField<String> field{String{u8"document"}};
    MentionsRef mentions;
    InputRef draft;
    int submissions{};
    const MentionsRichLabel rich{
        MentionsOptionLabel{[&] { Input(InputProps{}.ref(draft).defaultValue(u8"private draft")); }}};
    f.services.mount(Content{[&] {
        Form(FormProps{}.ref(form).onFinish([&](FormResult) { ++submissions; }), FormContent{[&] {
                 FormItem(FormItemProps{}.field(u8"text", field).control(mentions), FormItemContent{[&] {
                              Mentions(MentionsProps{}
                                           .ref(mentions)
                                           .value(field.value())
                                           .onChange(field.onChange())
                                           .options({{u8"a", u8"alpha", u8"Alpha", false, rich}}));
                          }});
             }});
    }});
    sync(f, 720, 600);
    check(mentions.focus(), "Form Mentions focus");
    edit(f, u8"@a");
    check(draft.focus(), "Form rich popup editor focus");
    const auto rich_owner = f.inputs.sessions().active().owner;
    check(f.inputs.editors().require(rich_owner).value() == "private draft",
          "popup Input inherited an unrelated outer FormItem value");
    edit(f, u8"saved rich");
    key(f, Key::enter);
    check(field.get() == String{u8"@a"} && submissions == 0, "popup editor changed or submitted its outer Form");
    check(form.reset() && mentions.focus(), "Form Mentions reset lost explicit ref ownership");
    const auto main_owner = f.inputs.sessions().active();
    check(bool(f.inputs.dispatch(CompositionChanged{String{u8"候选"}, {}, main_owner})), "Form reset preedit setup");
    check(form.reset() && !f.inputs.editors().require(main_owner.owner).composition().active &&
              field.get() == String{u8"document"},
          "same-value Form reset targeted the rich child instead of Mentions");
    clean(f);
}

void other_private_editors_preserve_public_binding() {
    Signal<std::vector<TransferItem>> transfer_data{
        std::vector<TransferItem>{{0.0, String{u8"A"}}, {1.0, String{u8"B"}}}};
    const std::vector<std::function<void()>> controls{
        [] { DatePicker(DatePickerProps{}); },
        [] { TimePicker(TimePickerProps{}); },
        [] { Cascader(CascaderProps{}); },
        [transfer_data] { Transfer(TransferProps{}.dataSource(transfer_data).showSearch(true)); },
        [] { ColorPicker(ColorPickerProps{}); },
        [] { Calendar(CalendarProps{}.defaultValue({2024, 2, 29}).fullscreen(false)); },
        [] { Carousel(CarouselProps{}.arrows().slides({{u8"panel", CarouselContent{[] { Text(u8"panel"); }}}})); },
    };
    for (std::size_t control_index = 0; control_index < controls.size(); ++control_index) {
        const auto& control = controls[control_index];
        Fixture f;
        detail::SelectionComponentHost selections{f.services};
        FormField<String> field{String{u8"public authority"}};
        InputRef ref;
        int submissions{};
        f.services.mount(Content{[&] {
            Form(FormProps{}.onFinish([&](FormResult) { ++submissions; }), FormContent{[&] {
                     FormItem(FormItemProps{}.field(u8"public", field), FormItemContent{[&] {
                                  control();
                                  Input(InputProps{}.ref(ref));
                              }});
                 }});
        }});
        sync(f, 720, 800);
        check(ref.focus(), "public control after private editor could not focus");
        const auto stamp = f.inputs.sessions().active();
        if (f.inputs.editors().require(stamp.owner).value() != "public authority") {
            throw std::runtime_error("private editor captured automatic binding, control=" +
                                     std::to_string(control_index));
        }
        edit(f, u8"accepted public");
        check(field.get() == String{u8"accepted public"}, "public typed event authority missing");
        key(f, Key::enter);
        check(submissions == 1, "compound private editor consumed public Enter ownership");
        if (control_index == 3) {
            transfer_data.set({});
            sync(f, 720, 800);
            check(std::ranges::all_of(blocked_roots(f.services.interactions()),
                                      [&](auto root) { return f.services.components().contains(root); }),
                  "Transfer data shrink retained stale row input blocks");
            check(f.services.destroy(f.services.transfer().mounted().front()), "Transfer partial retirement failed");
            sync(f, 720, 800);
            check(blocked_roots(f.services.interactions()).empty() && ref.focus(),
                  "retired Transfer retained private input blocks or damaged the public field");
            edit(f, u8"after Transfer retirement");
            check(field.get() == String{u8"after Transfer retirement"}, "Transfer retirement lost typed authority");
        }
        try {
            clean(f);
        } catch (const std::exception& error) {
            throw std::runtime_error("cleanup control=" + std::to_string(control_index) + ": " + error.what() +
                                     " nodes=" + std::to_string(f.nodes.size()) +
                                     " scenes=" + std::to_string(f.scene.size()) +
                                     " blocks=" + std::to_string(blocked_roots(f.services.interactions()).size()) +
                                     " glyphs=" + std::to_string(f.scene.glyph_scene().instances().size()) +
                                     " surfaces=" + std::to_string(f.services.surfaces().size()) +
                                     " hits=" + std::to_string(f.services.interactions().size()) +
                                     " deadline=" + std::to_string(f.services.next_frame_deadline().has_value()));
        }
    }
}

void calendar_nested_user_items_keep_their_binding_and_reset() {
    Fixture f;
    FormRef form;
    FormField<String> outer{String{u8"outer"}};
    FormField<String> header{String{u8"header"}};
    FormField<String> cell{String{u8"cell"}};
    InputRef public_ref;
    InputRef header_ref;
    InputRef cell_ref;
    int submissions{};
    f.services.mount(Content{[&] {
        Form(FormProps{}.ref(form).onFinish([&](FormResult) { ++submissions; }), FormContent{[&] {
                 FormItem(FormItemProps{}.field(u8"outer", outer), FormItemContent{[&] {
                              Calendar(
                                  CalendarProps{}
                                      .defaultValue({2024, 2, 29})
                                      .headerRender(CalendarHeaderRenderer{[&](CalendarHeaderContext) {
                                          return CalendarHeader{[&] {
                                              Input(InputProps{}.defaultValue(u8"private header"));
                                              FormItem(FormItemProps{}.field(u8"header", header),
                                                       FormItemContent{[&] { Input(InputProps{}.ref(header_ref)); }});
                                          }};
                                      }})
                                      .cellRender(CalendarCellRenderer{[&](Binding<CalendarCellInfo> info) {
                                          return CalendarCell{[&, info] {
                                              if (info.get().date == CivilDate{2024, 2, 29}) {
                                                  FormItem(FormItemProps{}.field(u8"cell", cell),
                                                           FormItemContent{[&] { Input(InputProps{}.ref(cell_ref)); }});
                                              }
                                          }};
                                      }}));
                              Input(InputProps{}.ref(public_ref));
                          }});
             }});
    }});
    sync(f, 720, 1000);
    check(public_ref.focus(), "public Input after Calendar lost focus");
    edit(f, u8"outer edited");
    check(outer.get() == String{u8"outer edited"} && header.get() == String{u8"header"} &&
              cell.get() == String{u8"cell"},
          "Calendar private header consumed outer binding");
    check(header_ref.focus(), "explicit header FormItem suppressed by private scope");
    edit(f, u8"header edited");
    key(f, Key::enter);
    check(header.get() == String{u8"header edited"} && submissions == 1, "header Item lost binding or Enter");
    check(cell_ref.focus(), "explicit cell FormItem lost focus");
    edit(f, u8"cell edited");
    check(cell.get() == String{u8"cell edited"}, "cell Item lost typed binding");
    check(form.reset() && cell_ref.focus(), "Calendar Form reset failed");
    const auto stamp = f.inputs.sessions().active();
    check(bool(f.inputs.dispatch(CompositionChanged{String{u8"候选"}, {}, stamp})), "Calendar Form preedit setup");
    check(form.reset() && !f.inputs.editors().require(stamp.owner).composition().active &&
              cell.get() == String{u8"cell"},
          "same-value reset failed to clear cell preedit");
    clean(f);
}

void boolean_binding_survives_focus_callbacks() {
    for (int kind = 0; kind < 2; ++kind) {
        for (int mode = 0; mode < 5; ++mode) {
            Fixture f;
            detail::SelectionComponentHost selection{f.services};
            FormRef form;
            FormField<bool> field;
            CheckboxRef checkbox;
            SwitchRef toggle;
            Signal<bool> disabled{false};
            runtime::ComponentId root;
            int focuses{};
            int blurs{};
            const auto on_focus = [&] {
                ++focuses;
                if (mode == 1) {
                    check(f.services.destroy(root), "Form focus callback destruction failed");
                }
                if (mode == 4) {
                    throw std::runtime_error("Form focus callback");
                }
            };
            const auto on_blur = [&] {
                ++blurs;
                if (mode == 2) {
                    check(f.services.destroy(root), "Form blur callback destruction failed");
                }
                if (mode == 3) {
                    throw std::runtime_error("Form blur callback");
                }
            };
            f.services.mount(Content{[&] {
                Form(FormProps{}.ref(form), FormContent{[&] {
                         FormItem(FormItemProps{}.field(u8"enabled", field), FormItemContent{[&] {
                                      if (kind == 0) {
                                          Checkbox(
                                              CheckboxProps{}.ref(checkbox).disabled(disabled).onFocus(on_focus).onBlur(
                                                  on_blur));
                                      } else {
                                          Switch(SwitchProps{}.ref(toggle).disabled(disabled).onFocus(on_focus).onBlur(
                                              on_blur));
                                      }
                                  }});
                     }});
            }});
            sync(f, 720, 600);
            root = f.services.form().mounted().front();
            const auto target = form.getFieldInstance(FormNamePath{u8"enabled"});
            check(target && target->geometry() && target->geometry()->bounds.width > 0,
                  "automatic boolean control lost actual Form geometry");
            const auto stale = field.onChange();
            bool threw{};
            try {
                static_cast<void>(target->focus());
            } catch (const std::runtime_error& error) {
                check(std::string_view{error.what()} == "Form focus callback", "unexpected Form focus exception");
                threw = true;
            }
            check(threw == (mode == 4) && focuses == 1, "Form focus callback propagation/transition mismatch");
            if (mode != 1 && mode != 4) {
                key(f, Key::space);
                check(field.get() && field.meta().touched, "focus callbacks displaced automatic boolean binding");
                if (mode == 0) {
                    check(form.reset() && !field.get(), "automatic boolean reset lost authority");
                    disabled.set(true);
                    check(blurs == 1 && !target->focus(), "Form boolean disabled gate lost blur/focus eligibility");
                    disabled.set(false);
                    check(target->focus() && focuses == 2, "Form boolean refocus failed");
                }
                const auto row = selection.mounted().front();
                const auto area = bounds(f, row.component);
                f.services.pointer().dispatch({PointerIdentity::mouse(), PointerAction::down, PointerButton::primary,
                                               area.x + area.width / 2, area.y + area.height / 2});
                const auto captured = f.services.pointer().state(PointerIdentity::mouse());
                check(captured && captured->capture, "Form boolean retirement fixture lacks capture");
                if (mode == 2) {
                    static_cast<void>(kind == 0 ? checkbox.blur() : toggle.blur());
                } else {
                    check(f.services.destroy(root), "Form boolean captured retirement failed");
                }
            } else if (mode == 4) {
                check(f.services.destroy(root), "throwing Form focus callback prevented retirement");
            }
            const bool accepted = field.get();
            stale(!accepted);
            check(field.get() == accepted && !checkbox.bound() && !toggle.bound() && !target->focus() &&
                      !target->geometry() && selection.mounted().empty() && f.services.form().mounted().empty(),
                  "Form boolean retirement retained value/ref/generation authority");
            const auto pointer = f.services.pointer().state(PointerIdentity::mouse());
            check((!pointer || !pointer->capture) && !f.services.focus().state().focused,
                  "Form boolean callback retirement retained capture/focus");
            clean(f);
        }
    }
}

} // namespace

int main() {
    try {
        private_editors_leave_automatic_binding_to_public_controls();
        popup_editors_keep_their_own_value_and_enter();
        other_private_editors_preserve_public_binding();
        calendar_nested_user_items_keep_their_binding_and_reset();
        boolean_binding_survives_focus_callbacks();
        return 0;
    } catch (const std::exception& error) {
        std::cerr << error.what() << '\n';
        return 1;
    }
}
