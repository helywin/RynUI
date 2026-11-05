#include "component/form_component.hpp"
#include "component/selection_component.hpp"
#include "support/input_fixture.hpp"
#include <iostream>
#include <thread>

namespace {
using namespace ryn;
using namespace ryn::input;
using Fixture = ryn_test::input_component::Fixture;

void check(bool value, const char* message) {
    if (!value) {
        throw std::runtime_error(message);
    }
}

void sync(Fixture& f, float width = 600, runtime::Rect clip = {0, 0, 600, 900}) {
    check(f.services.layout_and_synchronize({width, 900}, clip), "Form sync failed");
}

runtime::Rect bounds(Fixture& f, runtime::ComponentId id) {
    return f.nodes.require(f.services.components().root(id)).bounds;
}

void key(Fixture& f, Key key) {
    f.services.focus().dispatch({key, KeyAction::down});
    f.services.focus().dispatch({key, KeyAction::up});
}

void layouts_and_bindings() {
    Fixture f;
    detail::SelectionComponentHost selection{f.services};
    FormRef form;
    FormField<String> name;
    FormField<InputNumberStringValue> exact{Decimal{"9007199254740993.0001"}};
    FormField<SelectValues> roles{SelectValues{SelectValue{u8"one"}}};
    FormField<bool> agreed;
    name.required();
    Signal<FormPresentation> presentation{FormPresentation{}};
    Signal<bool> hidden{false};
    int mounts{};
    f.services.mount(Content{[&] {
        Form(FormProps{}.ref(form).presentation(presentation), FormContent{[&] {
                 FormItem(FormItemProps{}
                              .field(String{u8"name"}, name)
                              .label(String{u8"Name"})
                              .extra(String{u8"Extra"})
                              .hasFeedback()
                              .hidden(hidden),
                          FormItemContent{[&] {
                              ++mounts;
                              Input(InputProps{});
                          }});
                 FormItem(FormItemProps{}.field(String{u8"exact"}, exact).label(String{u8"Exact"}),
                          FormItemContent{[] { InputNumber(InputNumberProps{}); }});
                 FormItem(FormItemProps{}.field(String{u8"roles"}, roles), FormItemContent{[] {
                              FormControl(SelectProps{}.options({{SelectValue{u8"one"}, String{u8"One"}}}));
                          }});
                 FormItem(FormItemProps{}.field(String{u8"agreed"}, agreed).noStyle(),
                          FormItemContent{[] { Checkbox(CheckboxProps{}); }});
             }});
    }});
    sync(f);
    const auto item = f.services.form().items().front();
    const auto parts = f.services.form().snapshot(item);
    const auto input = f.inputs.mounted_inputs().front();
    check(bounds(f, parts.control).y > bounds(f, parts.label).y, "vertical layout not applied");
    check(f.services.focus().request_focus(input.interaction, FocusModality::keyboard),
          "automatic control focus failed");
    check(bool(f.inputs.dispatch(TextCommitted{String{u8"native"}, f.inputs.sessions().active()})),
          "automatic control edit failed");
    check(name.get() == String{u8"native"} && name.meta().touched, "automatic control value/event authority missing");
    auto options = presentation.get();
    options.layout = FormLayout::Horizontal;
    options.size = ControlSize::Large;
    options.variant = InputVariant::Filled;
    options.label_wrap = true;
    options.label_col = Responsive<FormColumn>{FormColumn{24, 0}}
                            .at(Breakpoint::Sm, FormColumn{8, 0})
                            .at(Breakpoint::Md, FormColumn{12, 0});
    options.wrapper_col = Responsive<FormColumn>{FormColumn{24, 0}}
                              .at(Breakpoint::Sm, FormColumn{16, 0})
                              .at(Breakpoint::Md, FormColumn{12, 0});
    presentation.set(options);
    sync(f);
    check(bounds(f, parts.control).x >= 145 && bounds(f, parts.control).y == bounds(f, parts.label).y &&
              f.inputs.size(input.component) == ControlSize::Large &&
              f.inputs.variant(input.component) == InputVariant::Filled && mounts == 1,
          "horizontal layout or inherited control context is cosmetic only");
    check(bounds(f, parts.control).x == 200, "Form responsive label columns did not use current window width");
    sync(f, 900, {0, 0, 900, 900});
    check(bounds(f, parts.control).x == 450, "Form responsive columns did not update at the Md breakpoint");
    sync(f, 400, {0, 0, 400, 900});
    check(bounds(f, parts.control).y > bounds(f, parts.label).y && bounds(f, parts.control).width > 350 &&
              bounds(f, parts.control).width <= 400,
          "24-span responsive columns did not stack at narrow width");
    sync(f);
    options.direction = TextDirection::RightToLeft;
    presentation.set(options);
    sync(f);
    check(bounds(f, parts.label).x > bounds(f, parts.control).x, "RTL did not mirror actual Form geometry");
    hidden.set(true);
    sync(f);
    check(!f.services.focus().request_focus(input.interaction, FocusModality::keyboard) &&
              form.getFieldValue(FormNamePath{u8"name"}).has_value(),
          "hidden field either interactive or not collected");
    hidden.set(false);
    options.layout = FormLayout::Inline;
    presentation.set(options);
    sync(f);
    const auto second_parts = f.services.form().snapshot(f.services.form().items()[1]);
    check(bounds(f, parts.control).y == bounds(f, second_parts.control).y,
          "Inline Form did not place fitting items on the same line");
    check(mounts == 1 && exact.get()->text() == String{u8"9007199254740993.0001"} && roles.get().size() == 1,
          "layout remounted controls or lost modern typed values");
    check(form.reset() && name.get().empty(), "automatic reset bridge failed");
    f.services.dispose();
    check(!form.bound() && f.inputs.mounted_inputs().empty(), "Form disposal retained automatic controls");
}

void enter_ime_and_provider() {
    Fixture f;
    FormRef outer;
    FormRef inner;
    FormField<String> name;
    FormField<String> nested;
    FormField<String> text;
    int outer_submits{};
    int inner_submits{};
    int provider_finishes{};
    int provider_changes{};
    f.services.mount(Content{[&] {
        FormProvider(
            FormProviderProps{}
                .onFormFinish([&](FormProviderEvent event) {
                    check(event.forms.size() == 2 && event.form.bound(), "provider form map missing");
                    ++provider_finishes;
                })
                .onFormChange([&](FormProviderEvent) { ++provider_changes; }),
            FormProviderContent{[&] {
                Form(FormProps{}.ref(outer).name(String{u8"outer"}).onFinish([&](FormResult) { ++outer_submits; }),
                     FormContent{[&] {
                         FormItem(FormItemProps{}.field(String{u8"name"}, name),
                                  FormItemContent{[] { FormControl(InputProps{}); }});
                         FormItem(FormItemProps{}.field(String{u8"text"}, text),
                                  FormItemContent{[] { FormControl(TextAreaProps{}); }});
                         Form(FormProps{}.ref(inner).name(String{u8"inner"}).onFinish([&](FormResult) {
                             ++inner_submits;
                         }),
                              FormContent{[&] {
                                  check(useFormInstance().bound(), "useFormInstance did not resolve native context");
                                  FormItem(FormItemProps{}.field(String{u8"nested"}, nested),
                                           FormItemContent{[] { FormControl(InputProps{}); }});
                              }});
                     }});
            }});
    }});
    sync(f);
    const auto inputs = f.inputs.mounted_inputs();
    check(f.services.focus().request_focus(inputs[0].interaction, FocusModality::keyboard), "single line focus failed");
    key(f, Key::enter);
    check(outer_submits == 1 && inner_submits == 0 && provider_finishes == 1, "Enter did not route to owner Form");
    check(bool(f.inputs.dispatch(CompositionChanged{String{u8"候选"}, {0, 0}, f.inputs.sessions().active()})),
          "IME setup failed");
    key(f, Key::enter);
    check(outer_submits == 1, "IME Enter submitted a Form");
    check(outer.reset(), "IME reset failed");
    check(f.services.focus().request_focus(inputs[1].interaction, FocusModality::keyboard), "TextArea focus failed");
    key(f, Key::enter);
    check(text.get() == String{u8"\n"} && outer_submits == 1 && provider_changes == 1,
          "TextArea Enter lost newline or provider event");
    check(f.services.focus().request_focus(inputs[2].interaction, FocusModality::keyboard),
          "nested input focus failed");
    key(f, Key::enter);
    check(inner_submits == 1 && outer_submits == 1 && provider_finishes == 2, "nested Form Enter crossed scope");
    f.services.set_window_active(false);
    key(f, Key::enter);
    check(inner_submits == 1, "inactive window submitted Form");
}

void scroll_geometry_and_stale_acknowledgements() {
    Fixture f;
    FormRef form;
    InputRef ref;
    FormField<String> name;
    name.required();
    std::function<void(bool)> reveal;
    FormScrollRequest request;
    f.services.mount(Content{[&] {
        Form(FormProps{}.ref(form).scrollTarget([&](FormScrollRequest value, auto callback) {
            request = std::move(value);
            reveal = std::move(callback);
        }),
             FormContent{[&] {
                 FormItem(FormItemProps{}.field(String{u8"name"}, name).control(ref), FormItemContent{[&] {
                              Input(InputProps{}
                                        .value(name.value())
                                        .onChange(name.onChange())
                                        .ref(ref)
                                        .layout(LayoutStyle{}.width(dp(80))));
                          }});
             }});
    }});
    sync(f, 600, {200, 0, 400, 900});
    check(form.scrollToField(FormNamePath{u8"name"}) && request.control.bounds.width == 80 &&
              request.control.clip.x >= 200,
          "scroll used FormItem wrapper instead of actual control geometry");
    reveal(true);
    sync(f, 600, {200, 0, 400, 900});
    check(!f.services.focus().state().focused, "unrevealed clipped control was focused");
    sync(f);
    check(form.scrollToField(FormNamePath{u8"name"}), "visible scroll command refused");
    auto old = reveal;
    check(form.reset(), "same-value scroll reset failed");
    old(true);
    sync(f);
    check(!f.services.focus().state().focused, "reset retained old scroll focus authority");
    check(form.scrollToField(FormNamePath{u8"name"}), "current scroll command refused");
    reveal(true);
    sync(f);
    check(f.services.focus().state().focused == f.inputs.mounted_inputs().front().interaction,
          "scroll acknowledgement did not focus actual control");
}

void boolean_scroll_geometry_and_rebound_ref() {
    Fixture f;
    detail::SelectionComponentHost selection{f.services};
    FormRef form;
    FormField<bool> checked;
    FormField<bool> switched;
    CheckboxRef checkbox;
    SwitchRef toggle;
    std::function<void(bool)> reveal;
    FormScrollRequest request;
    f.services.mount(Content{[&] {
        Form(FormProps{}.ref(form).scrollTarget([&](FormScrollRequest value, auto ack) {
            request = value;
            reveal = std::move(ack);
        }),
             FormContent{[&] {
                 FormItem(FormItemProps{}.field(String{u8"check"}, checked).control(checkbox),
                          FormItemContent{[&] { Checkbox(CheckboxProps{}.ref(checkbox)); }});
                 FormItem(FormItemProps{}.field(String{u8"switch"}, switched).control(toggle),
                          FormItemContent{[&] { Switch(SwitchProps{}.ref(toggle)); }});
             }});
    }});
    sync(f);
    check(form.scrollToField(FormNamePath{u8"check"}) && request.control.bounds.width > 0,
          "Checkbox lacked real Form scroll geometry");
    reveal(true);
    sync(f);
    const auto first_focus = f.services.focus().state().focused;
    check(bool(first_focus), "Checkbox scroll did not focus");
    check(form.scrollToField(FormNamePath{u8"switch"}) && request.control.bounds.width > 0,
          "Switch lacked real Form scroll geometry");
    const auto target = form.getFieldInstance(FormNamePath{u8"switch"});
    check(target && target->geometry() && target->geometry()->bounds == request.control.bounds,
          "typed field instance did not expose its actual native control");
    bool foreign_focus{true};
    bool foreign_geometry{true};
    std::thread worker([&] {
        foreign_focus = target->focus();
        foreign_geometry = target->geometry().has_value();
    });
    worker.join();
    check(!foreign_focus && !foreign_geometry, "typed control instance entered owner state from a worker thread");
    const auto ack = reveal;
    ack(true);
    sync(f);
    check(f.services.focus().state().focused != first_focus && bool(f.services.focus().state().focused),
          "Switch scroll did not focus its own control");
    check(checkbox.focus(), "Checkbox ref focus failed");
    ack(true);
    sync(f);
    check(f.services.focus().state().focused == first_focus, "duplicate acknowledgement stole focus");
    check(form.scrollToField(FormNamePath{u8"switch"}), "Switch stale ref setup failed");
    const auto stale = reveal;
    const auto item = f.services.form().items()[1];
    check(f.services.destroy(item), "Switch field destroy failed");
    f.services.append_slot(f.services.form().mounted().front(), Content{[&] { Switch(SwitchProps{}.ref(toggle)); }});
    sync(f);
    stale(true);
    sync(f);
    check(!target->focus() && !target->geometry(), "removed native field instance retained authority");
    check(f.services.focus().state().focused == first_focus, "stale Switch acknowledgement focused a rebound ref");
    f.services.set_window_active(false);
    check(!form.scrollToField(FormNamePath{u8"check"}), "inactive Form allowed native focus routing");
    f.services.dispose();
}

} // namespace

int main() {
    try {
        layouts_and_bindings();
        enter_ime_and_provider();
        scroll_geometry_and_stale_acknowledgements();
        boolean_scroll_geometry_and_rebound_ref();
        std::cout << "Form native interaction contracts passed (4 cases)\n";
        return 0;
    } catch (const std::exception& error) {
        std::cerr << error.what() << '\n';
        return 1;
    }
}
