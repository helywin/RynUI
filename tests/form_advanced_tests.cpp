#include "component/form_component.hpp"
#include "support/input_fixture.hpp"
#include <iostream>
#include <map>

namespace {
using namespace ryn;
using Fixture = ryn_test::input_component::Fixture;

void check(bool value, const char* message) {
    if (!value) {
        throw std::runtime_error(message);
    }
}

void sync(Fixture& f) {
    check(f.services.layout_and_synchronize({600, 900}, {0, 0, 600, 900}), "Form layout failed");
}

void retained_lists() {
    Fixture f;
    FormRef form;
    FormListRef list;

    struct Row final {
        FormField<String> field;
        InputRef ref;
        std::function<void(String)> change;
    };

    std::map<std::uint64_t, Row> rows;
    int mounts{};
    f.services.mount(Content{[&] {
        Form(FormProps{}.ref(form), FormContent{[&] {
                 FormList(
                     FormListProps{}
                         .name(FormNamePath{u8"people"})
                         .ref(list)
                         .initialValue({FormData{}, FormData{}, FormData{}}),
                     [&](FormListField row) {
                         ++mounts;
                         auto& state = rows[row.key()];
                         FormItem(
                             FormItemProps{}.field(FormNamePath{u8"name"}, state.field).control(state.ref),
                             FormItemContent{[&state] {
                                 state.change = state.field.onChange();
                                 Input(InputProps{}.value(state.field.value()).onChange(state.change).ref(state.ref));
                             }});
                     });
             }});
    }});
    sync(f);
    auto initial = list.fields();
    const auto key = initial[1].key();
    auto& row = rows.at(key);
    row.change(String{u8"draft"});
    check(row.ref.focus({InputFocusCursor::End}), "row focus failed");
    const auto owner = f.inputs.sessions().active();
    const auto input = f.inputs.mounted_inputs()[1];
    check(list.move(1, 0), "list move failed");
    sync(f);
    check(mounts == 3 && list.fields()[0].key() == key && initial[1].index() == 0, "move remounted stable rows");
    check(f.inputs.mounted_inputs()[1].component == input.component && f.inputs.sessions().active() == owner,
          "move lost rich input identity or focus session");
    row.change(String{u8"same person"});
    check(form.getFieldValue(FormNamePath{u8"people", 0, u8"name"}) == FormValue{String{u8"same person"}},
          "moved callback wrote wrong path");
    const auto before = form.getFieldsValue();
    check(!list.remove({0, 0}) && !list.move(99, 0) && !list.add({}, 99), "invalid list operation accepted");
    check(form.getFieldsValue() == before && mounts == 3, "invalid operation changed list atomically");
    check(list.add({}, 1), "list insert failed");
    sync(f);
    check(mounts == 4 && list.fields()[0].key() == key && row.ref.bound(), "insert remounted existing row");
    auto callback = row.change;
    check(list.remove({0}), "list remove failed");
    sync(f);
    callback(String{u8"late"});
    check(row.field.get() == String{u8"same person"} && !row.ref.bound(), "removed callback/editor survived");
    check(f.inputs.sessions().active() != owner, "removed editor retained active session");
    check(form.validate()->values.size() == 3, "removed row remained in submitted values");
    check(form.reset(), "list initial shape reset failed");
    sync(f);
    check(list.fields().size() == 3 && form.validate()->values.size() == 3, "list reset did not restore initial shape");
}

void nested_lists_and_forms() {
    Fixture f;
    FormRef outer;
    FormRef inner;
    FormListRef parents;
    std::map<std::uint64_t, FormListRef> children;
    int mounts{};
    FormField<String> outside{String{u8"outer"}};
    FormField<String> inside{String{u8"inner"}};
    f.services.mount(Content{[&] {
        Form(FormProps{}.ref(outer), FormContent{[&] {
                 FormItem(FormItemProps{}.field(String{u8"same"}, outside), FormItemContent{[] { Text(u8"outside"); }});
                 Form(FormProps{}.ref(inner), FormContent{[&] {
                          FormItem(FormItemProps{}.field(String{u8"same"}, inside),
                                   FormItemContent{[] { Text(u8"inside"); }});
                      }});
                 FormList(FormListProps{}.name(FormNamePath{u8"groups"}).ref(parents).initialValue({{}, {}}),
                          [&](FormListField parent) {
                              FormList(FormListProps{}
                                           .name(FormNamePath{u8"members"})
                                           .ref(children[parent.key()])
                                           .initialValue({{}}),
                                       [&](FormListField) {
                                           ++mounts;
                                           FormField<String> field{String{u8"member"}};
                                           FormItem(
                                               FormItemProps{}.field(FormNamePath{u8"name"}, field),
                                               FormItemContent{[field] {
                                                   Input(InputProps{}.value(field.value()).onChange(field.onChange()));
                                               }});
                                       });
                          });
             }});
    }});
    sync(f);
    const auto old = parents.fields()[0];
    check(parents.move(0, 1), "nested parent move failed");
    check(children.at(old.key()).add({}), "nested child insert failed");
    sync(f);
    check(mounts == 3 && outer.getFieldValue(FormNamePath{u8"groups", 1, u8"members", 1, u8"name"}).has_value(),
          "nested paths did not shift");
    check(inner.validate()->values.size() == 1 && outer.validate()->values.size() == 4,
          "nested form registrations crossed scopes");
}

void empty_list_leaf_alias_is_atomic() {
    Fixture f;
    FormRef form;
    FormListRef list;
    f.services.mount(Content{[&] {
        Form(FormProps{}.ref(form), FormContent{[&] {
                 FormList(FormListProps{}.name(FormNamePath{u8"rows"}).ref(list), [](FormListField) { Text(u8"row"); });
             }});
    }});
    sync(f);
    const auto before = form.getFieldsValue();
    FormField<String> alias;
    bool rejected{};
    try {
        f.services.append_slot(f.services.form().mounted().front(), Content{[&] {
                                   FormItem(FormItemProps{}.field(String{u8"rows"}, alias),
                                            FormItemContent{[] { Input(InputProps{}); }});
                               }});
    } catch (const std::invalid_argument&) {
        rejected = true;
    }
    sync(f);
    check(rejected && list.bound() && list.fields().empty() && form.getFieldsValue() == before &&
              f.inputs.mounted_inputs().empty() && form.validate()->valid(),
          "leaf alias replaced an empty List array or corrupted the surrounding Form");
}

} // namespace

int main() {
    try {
        retained_lists();
        nested_lists_and_forms();
        empty_list_leaf_alias_is_atomic();
        std::cout << "Form advanced contracts passed (3 cases)\n";
        return 0;
    } catch (const std::exception& error) {
        std::cerr << error.what() << '\n';
        return 1;
    }
}
