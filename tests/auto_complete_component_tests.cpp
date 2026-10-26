#include "support/auto_complete_fixture.hpp"
#include "component/form_component.hpp"
#include "component/form_focus_context.hpp"
#include "../examples/auto_complete_example.hpp"
#if defined(_MSC_VER) && defined(_DEBUG)
#include <crtdbg.h>
#endif

namespace {
using namespace auto_complete_test;

void flow() {
    auto fixture = std::make_unique<Fixture>();
    auto& f = *fixture;
    InputRef ref;
    std::vector<String> changes;
    std::vector<String> searches;
    std::vector<String> selected;
    std::vector<char> order;
    int clears = 0;
    f.services.mount(Content{[&] {
        AutoComplete(AutoCompleteProps{}
                         .options(options())
                         .ref(ref)
                         .allowClear()
                         .backfill()
                         .layout(LayoutStyle{}.width(dp(240)))
                         .onChange([&](String v) {
                             changes.push_back(v);
                             order.push_back('c');
                         })
                         .onSearch([&](String v) {
                             searches.push_back(v);
                             order.push_back('q');
                         })
                         .onSelect([&](String v, AutoCompleteOption o) {
                             check(v == o.value && o.key == String{u8"c"}, "typed selected option");
                             selected.push_back(v);
                             order.push_back('s');
                         })
                         .onClear([&] { ++clears; }));
        Button(ButtonProps{}, [] { Text(u8"Next"); });
    }});
    sync(f);
    const auto id = f.services.auto_complete().mounted()[0];
    const auto original = snapshot(f, id);
    click(f, original.input);
    edit(f, u8"anything 中文");
    check(snapshot(f, id).value == String{u8"anything 中文"} && searches.size() == 1 && changes.size() == 1 &&
              snapshot(f, id).visible,
          "free input events");
    const auto editor = f.inputs.mounted_inputs()[0].editor;
    check(f.inputs.editors().require(editor).value() == "anything 中文", "real Input not edited");
    key(f, Key::down);
    check(snapshot(f, id).display == String{u8"gamma"} && snapshot(f, id).value == String{u8"anything 中文"} &&
              changes.size() == 1,
          "backfill/disabled skip");
    key(f, Key::escape);
    check(!snapshot(f, id).visible && f.inputs.editors().require(editor).value() == "anything 中文",
          "Escape backfill restore");
    key(f, Key::down);
    key(f, Key::down);
    key(f, Key::enter);
    check(snapshot(f, id).value == String{u8"gamma"} && !snapshot(f, id).visible && searches.size() == 1 &&
              selected.size() == 1 && order[order.size() - 2] == 'c' && order.back() == 's',
          "keyboard select events");
    click(f, snapshot(f, id).input);
    click(f, snapshot(f, id).rows[2]);
    check(changes.size() == 2 && selected.size() == 2 && searches.size() == 1, "same-value pointer selection");
    bool cleared = false;
    for (auto hit : f.services.interactions().declaration_order()) {
        const auto* r = f.services.interactions().find(hit);
        if (r && r->eligible && !r->focusable && descendant(f, r->component, original.input)) {
            click(f, r->component);
            cleared = true;
            break;
        }
    }
    check(cleared && snapshot(f, id).value.empty() && clears == 1 && searches.size() == 2, "pointer clear");
    key(f, Key::down);
    key(f, Key::tab);
    check(!snapshot(f, id).visible &&
              f.services.focus().state().focused == f.buttons.mounted_buttons().back().interaction,
          "single Tab entry");
    check(snapshot(f, id).input == original.input && snapshot(f, id).rows == original.rows,
          "ordinary interaction remounted");
    clean(f);
    check(!ref.bound() && !ref.focus(), "disposed ref");
}

void controlled() {
    auto fixture = std::make_unique<Fixture>();
    auto& f = *fixture;
    Signal<String> value{u8"owner"};
    Signal<bool> open{true};
    Signal<bool> disabled{false};
    Signal<bool> read_only{false};
    Signal<std::vector<AutoCompleteOption>> suggestions{options()};
    Signal<AutoCompleteFilter> filter{AutoCompleteFilter{}};
    Signal<InputStatus> status{InputStatus::Default};
    Signal<ControlSize> size{ControlSize::Middle};
    Signal<InputVariant> variant{InputVariant::Outlined};
    InputRef ref;
    bool accept = false;
    int changes = 0;
    int closes = 0;
    f.services.mount(Content{[&] {
        AutoComplete(AutoCompleteProps{}
                         .value(value)
                         .options(suggestions)
                         .filterOption(filter)
                         .open(open)
                         .disabled(disabled)
                         .readOnly(read_only)
                         .status(status)
                         .size(size)
                         .variant(variant)
                         .ref(ref)
                         .onChange([&](String v) {
                             ++changes;
                             if (accept) {
                                 value.set(v);
                             }
                         })
                         .onOpenChange([&](bool v) { closes += !v; }));
    }});
    sync(f);
    const auto id = f.services.auto_complete().mounted()[0];
    const auto original = snapshot(f, id);
    check(ref.focus(), "controlled ref focus");
    edit(f, u8"rejected");
    check(snapshot(f, id).value == String{u8"owner"} && snapshot(f, id).display == String{u8"owner"} &&
              f.inputs.editors().require(f.inputs.mounted_inputs()[0].editor).value() == "owner",
          "controlled rejection");
    key(f, Key::enter);
    check(value.get() == String{u8"owner"} && snapshot(f, id).visible, "controlled selection/open refusal");
    key(f, Key::escape);
    check(snapshot(f, id).visible && closes >= 1, "controlled close authority");
    accept = true;
    edit(f, u8"accepted");
    check(value.get() == String{u8"accepted"} && snapshot(f, id).display == value.get(), "controlled echo");
    suggestions.set({});
    sync(f);
    check(!snapshot(f, id).visible, "empty options displayed popup");
    suggestions.set(options());
    sync(f);
    check(snapshot(f, id).visible, "controlled empty-to-filled authority");
    filter.set(AutoCompleteFilter{true});
    sync(f);
    check(!snapshot(f, id).visible, "empty filtered result popup");
    value.set(u8"alp");
    sync(f);
    check(snapshot(f, id).visible, "filtered result missing");
    read_only.set(true);
    sync(f);
    check(!snapshot(f, id).visible && ref.focus(), "readOnly gate/focus");
    const auto before = changes;
    check(!bool(f.inputs.dispatch(TextCommitted{String{u8"forbidden"}, f.inputs.sessions().active()})),
          "readOnly accepted commit");
    check(changes == before && value.get() == String{u8"alp"}, "readOnly edited");
    read_only.set(false);
    disabled.set(true);
    sync(f);
    check(!ref.focus(), "disabled ref");
    disabled.set(false);
    open.set(false);
    sync(f);
    open.set(true);
    sync(f);
    check(snapshot(f, id).visible, "authority reopen");
    for (auto control : {ControlSize::Small, ControlSize::Large}) {
        size.set(control);
        variant.set(InputVariant::Filled);
        status.set(InputStatus::Warning);
        sync(f);
        check(f.inputs.size(original.input) == control && f.inputs.variant(original.input) == InputVariant::Filled &&
                  f.inputs.status(original.input) == InputStatus::Warning,
              "Input typed forwarding");
    }
    check(snapshot(f, id).input == original.input, "controlled remounted Input");
    clean(f);
}

void retention() {
    auto fixture = std::make_unique<Fixture>();
    auto& f = *fixture;
    InputRef main_ref;
    InputRef child_ref;
    int rich_runs = 0;
    AutoCompleteRichLabel rich{AutoCompleteOptionLabel{[&] {
        ++rich_runs;
        Input(InputProps{}.defaultValue(u8"native child").ref(child_ref));
    }}};
    auto initial = options();
    initial[0].rich_label = rich;
    Signal<std::vector<AutoCompleteOption>> suggestions{initial};
    f.services.mount(
        Content{[&] { AutoComplete(AutoCompleteProps{}.options(suggestions).defaultOpen().ref(main_ref)); }});
    sync(f);
    const auto id = f.services.auto_complete().mounted()[0];
    const auto original = snapshot(f, id);
    const auto main = f.inputs.mounted_inputs()[0];
    const auto child = f.inputs.mounted_inputs()[1];
    check(child_ref.focus(), "rich native focus");
    edit(f, u8"kept native draft");
    const auto child_stamp = f.inputs.sessions().active();
    check(bool(f.inputs.dispatch(CompositionChanged{String{u8"preedit"}, {}, child_stamp})), "child preedit");
    auto next = initial;
    std::swap(next[0], next[2]);
    next[1].label = u8"Updated";
    suggestions.set(next);
    sync(f);
    check(snapshot(f, id).rows[2] == original.rows[0] && snapshot(f, id).input == original.input && rich_runs == 1 &&
              f.inputs.editors().require(child.editor).value() == "kept native draft" &&
              f.inputs.editors().require(child.editor).composition().active,
          "key reorder lost native child/editor/preedit identity");
    (void)f.inputs.dispatch(CompositionChanged{String{}, {}, child_stamp});
    check(main_ref.focus(), "main focus");
    const auto main_stamp = f.inputs.sessions().active();
    check(bool(f.inputs.dispatch(CompositionChanged{String{u8"zhong"}, {}, main_stamp})), "main preedit");
    next.push_back({u8"d", u8"delta", u8"Delta"});
    suggestions.set(next);
    sync(f);
    check(f.inputs.editors().require(main.editor).composition().active &&
              f.inputs.mounted_inputs()[0].editor == main.editor,
          "suggestions canceled main preedit");
    const auto active = snapshot(f, id).active;
    key(f, Key::down);
    key(f, Key::enter);
    check(f.inputs.editors().require(main.editor).composition().active && snapshot(f, id).active == active &&
              snapshot(f, id).value.empty(),
          "IME committed suggestion");
    next.erase(next.begin() + 2);
    suggestions.set(next);
    sync(f);
    check(!child_ref.bound() && !f.inputs.editors().find(child.editor) &&
              !bool(f.inputs.dispatch(TextCommitted{String{u8"late"}, child_stamp})),
          "removed key retained editor/ref/session");
    clean(f);
}

void form() {
    auto fixture = std::make_unique<Fixture>();
    auto& f = *fixture;
    FormRef form_ref;
    FormField<String> field{String{u8"initial"}};
    InputRef ref;
    Signal<bool> readonly{false};
    int events = 0;
    f.services.mount(Content{[&] {
        Form(FormProps{}.ref(form_ref).readOnly(readonly), FormContent{[&] {
                 FormItem(FormItemProps{}.field(u8"name", field).control(ref), FormItemContent{[&] {
                              const auto change = field.onChange();
                              AutoComplete(AutoCompleteProps{}
                                               .value(field.value())
                                               .onChange([&, change](String v) {
                                                   ++events;
                                                   change(v);
                                               })
                                               .options(options())
                                               .backfill()
                                               .status(field.status())
                                               .disabled(field.disabled())
                                               .readOnly(field.readOnly())
                                               .ref(ref));
                          }});
             }});
    }});
    sync(f);
    const auto id = f.services.auto_complete().mounted()[0];
    const auto input = f.inputs.mounted_inputs()[0];
    check(ref.focus(), "Form ref focus");
    edit(f, u8"arbitrary value");
    check(field.get() == String{u8"arbitrary value"}, "Form String not bound");
    check(form_ref.reset(), "Form reset");
    sync(f);
    check(field.get() == String{u8"initial"}, "Form reset value");
    const auto count = events;
    const auto stamp = f.inputs.sessions().active();
    check(bool(f.inputs.dispatch(CompositionChanged{String{u8"preedit"}, {}, stamp})), "Form preedit");
    check(form_ref.reset(), "same value reset");
    sync(f);
    check(!f.inputs.editors().require(input.editor).composition().active &&
              f.inputs.editors().require(input.editor).value() == "initial" && events == count,
          "same value reset lost preedit cleanup/event silence");
    key(f, Key::down);
    key(f, Key::down);
    check(snapshot(f, id).backfilled, "Form backfill fixture");
    check(form_ref.reset(), "backfill reset");
    sync(f);
    check(!snapshot(f, id).backfilled && snapshot(f, id).display == String{u8"initial"} && events == count,
          "Form backfill reset");
    readonly.set(true);
    sync(f);
    check(!snapshot(f, id).visible, "Form readOnly left popup");
    auto result = form_ref.validate();
    check(result && result->valid() && field.get() == String{u8"initial"}, "Form result");
    clean(f);
}

void example() {
    auto fixture = std::make_unique<Fixture>();
    auto& f = *fixture;
    FormRef form;
    FormField<String> name{String{u8"Initial"}};
    AutoCompleteRef input;
    f.services.mount(Content{[=] { AutoCompleteForm(form, name, input); }});
    sync(f);
    check(input.focus(), "documented example focus");
    edit(f, u8"自由输入");
    check(name.get() == String{u8"自由输入"} && form.reset(), "documented example String/reset");
    sync(f);
    check(name.get() == String{u8"Initial"}, "documented example reset result");
    clean(f);
}
} // namespace

int main(int argc, char** argv) {
#if defined(_MSC_VER) && defined(_DEBUG)
    _CrtSetReportMode(_CRT_ASSERT, _CRTDBG_MODE_FILE | _CRTDBG_MODE_DEBUG);
    _CrtSetReportFile(_CRT_ASSERT, _CRTDBG_FILE_STDERR);
#endif
    try {
        check(argc == 2, "case required");
        const std::string name = argv[1];
        if (name == "flow") {
            flow();
        } else if (name == "controlled") {
            controlled();
        } else if (name == "retention") {
            retention();
        } else if (name == "form") {
            form();
        } else if (name == "example") {
            example();
        } else {
            throw std::runtime_error("unknown case");
        }
        std::cout << "AutoComplete " << name << " passed\n";
        return 0;
    } catch (const std::exception& e) {
        std::cerr << e.what() << '\n';
        return 1;
    }
}
