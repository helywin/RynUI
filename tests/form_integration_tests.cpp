#include "support/mentions_fixture.hpp"
#include "component/selection_component.hpp"
#include "component/transfer_component.hpp"
#include <ryn/auto_complete.hpp>
#include <ryn/form.hpp>
#include <ryn/date_picker.hpp>
#include <ryn/time_picker.hpp>
#include <ryn/cascader.hpp>
#include <ryn/transfer.hpp>
#include <ryn/color_picker.hpp>

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

} // namespace

int main() {
    try {
        private_editors_leave_automatic_binding_to_public_controls();
        popup_editors_keep_their_own_value_and_enter();
        other_private_editors_preserve_public_binding();
        return 0;
    } catch (const std::exception& error) {
        std::cerr << error.what() << '\n';
        return 1;
    }
}
