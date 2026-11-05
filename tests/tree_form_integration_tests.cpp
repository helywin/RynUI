#include "support/mentions_fixture.hpp"
#include "component/tree_component.hpp"
#include "component/tree_select_component.hpp"
#include "component/selection_component.hpp"
#include <ryn/form.hpp>
#include <map>

namespace {
using namespace mentions_test;

struct Test final : Fixture {
    detail::SelectionComponentHost selection{services};
};

void verify_cleanup(Test& f, const char* label) {
    try {
        clean(f);
    } catch (const std::exception& error) {
        throw std::runtime_error(std::string{label} + ": " + error.what() + " nodes=" + std::to_string(f.nodes.size()) +
                                 " scenes=" + std::to_string(f.scene.size()) +
                                 " glyphs=" + std::to_string(f.scene.glyph_scene().instances().size()) +
                                 " surfaces=" + std::to_string(f.services.surfaces().size()) +
                                 " hits=" + std::to_string(f.services.interactions().size()) +
                                 " blocks=" + std::to_string(blocked_roots(f.services.interactions()).size()) +
                                 " deadline=" + std::to_string(f.services.next_frame_deadline().has_value()));
    }
}

void private_controls() {
    for (int kind = 0; kind < 2; ++kind) {
        Test f;
        FormField<String> field{String{u8"public authority"}};
        InputRef tail;
        int submitted{};
        f.services.mount(Content{[&] {
            Form(FormProps{}.onFinish([&](FormResult) { ++submitted; }), FormContent{[&] {
                     FormItem(FormItemProps{}.field(u8"public", field), FormItemContent{[&] {
                                  if (kind == 0) {
                                      Tree(TreeProps{}.layout(LayoutStyle{}.width(dp(320)).height(dp(80))),
                                           {TreeItem{u8"a", u8"Alpha"}.richTitle(TreeTitle{
                                               [] { Input(InputProps{}.defaultValue(u8"private Tree draft")); }})});
                                  } else {
                                      TreeSelect(TreeSelectProps{}.showSearch(), {TreeSelectItem{u8"a", u8"Alpha"}});
                                  }
                                  Input(InputProps{}.ref(tail));
                              }});
                 }});
        }});
        sync(f, 720, 600);
        check(tail.focus(), "public Form control focus after Tree/TreeSelect");
        check(f.inputs.editors().require(f.inputs.sessions().active().owner).value() == "public authority",
              "Tree/TreeSelect private editor claimed the automatic Form slot");
        edit(f, u8"accepted");
        key(f, Key::enter);
        check(field.get() == String{u8"accepted"} && submitted == 1,
              "Tree/TreeSelect private editor consumed public value or Enter");
        verify_cleanup(f, kind == 0 ? "private Tree" : "private TreeSelect");
    }
}

void late_rich_tags() {
    Test f;
    FormRef form;
    FormField<String> outer{String{u8"outer authority"}};
    Signal<TreeKeys> values{TreeKeys{}};
    TreeSelectRef selector;
    std::map<std::string, InputRef> refs;
    int submitted{};
    int factories{};
    f.services.mount(Content{[&] {
        Form(FormProps{}.ref(form).onFinish([&](FormResult) { ++submitted; }), FormContent{[&] {
                 FormItem(FormItemProps{}.field(u8"outer", outer).control(selector), FormItemContent{[&] {
                              TreeSelect(TreeSelectProps{}
                                             .multiple()
                                             .value(values)
                                             .ref(selector)
                                             .layout(LayoutStyle{}.width(dp(360)))
                                             .tagRender([&](TreeSelectTagInfo info) {
                                                 return TreeSelectTag{[&, key = std::string{info.value.bytes()}] {
                                                     ++factories;
                                                     Input(InputProps{}
                                                               .ref(refs[key])
                                                               .defaultValue(u8"private tag")
                                                               .layout(LayoutStyle{}.width(dp(90))));
                                                 }};
                                             }),
                                         {TreeSelectItem{u8"a", u8"Alpha"}, TreeSelectItem{u8"b", u8"Beta"}});
                          }});
             }});
    }});
    sync(f, 720, 600);
    values.set({u8"a", u8"b"});
    sync(f, 720, 600);
    check(factories == 2 && refs.at("a").focus(), "late rich tags not mounted/focusable");
    const auto owner = f.inputs.sessions().active().owner;
    check(f.inputs.editors().require(owner).value() == "private tag", "late tag captured outer Form value");
    edit(f, u8"tag draft");
    key(f, Key::enter);
    check(outer.get() == String{u8"outer authority"} && submitted == 0, "late rich tag edited or submitted outer Form");
    check(form.reset() && values.get() == TreeKeys({u8"a", u8"b"}) && factories == 2,
          "same-value reset replaced accepted rich tags");
    verify_cleanup(f, "late tags");
}

void explicit_popup_item() {
    Test f;
    FormField<String> field{String{u8"popup authority"}};
    InputRef ref;
    int submitted{};
    f.services.mount(Content{[&] {
        Form(FormProps{}.onFinish([&](FormResult) { ++submitted; }), FormContent{[&] {
                 TreeSelect(TreeSelectProps{}
                                .defaultOpen()
                                .listHeight(80)
                                .layout(LayoutStyle{}.width(dp(300)))
                                .popupRender([&](auto original) {
                                    return TreeSelectPopup{[&, original] {
                                        Flex(FlexProps{}.vertical(true), FlexContent{[&, original] {
                                                 original();
                                                 FormItem(FormItemProps{}.field(u8"popup", field).noStyle(),
                                                          FormItemContent{[&] { Input(InputProps{}.ref(ref)); }});
                                             }});
                                    }};
                                }),
                            {TreeSelectItem{u8"a", u8"Alpha"}});
             }});
    }});
    sync(f, 720, 600);
    check(ref.focus(), "explicit popup FormItem did not focus");
    check(f.inputs.editors().require(f.inputs.sessions().active().owner).value() == "popup authority",
          "window-layer shield suppressed an explicitly declared popup FormItem");
    edit(f, u8"accepted popup");
    key(f, Key::enter);
    check(field.get() == String{u8"accepted popup"} && submitted == 1,
          "explicit popup FormItem lost its own value or Enter scope");
    verify_cleanup(f, "explicit popup Item");
}

void keyed_callbacks() {
    Test f;
    FormField<String> a{String{u8"A"}};
    FormField<String> b{String{u8"B"}};
    FormField<String> c{String{u8"C"}};
    InputRef a_ref;
    InputRef b_ref;
    InputRef c_ref;
    std::function<void(String)> old_change;
    const auto item = [&](String key, FormField<String> field, InputRef ref, bool capture) {
        return TreeItem{key, key}.richTitle(TreeTitle{[&, key, field, ref, capture] {
            FormItem(FormItemProps{}.field(key, field).noStyle(), FormItemContent{[&, field, ref, capture] {
                         if (capture) {
                             old_change = field.onChange();
                         }
                         Input(InputProps{}.ref(ref).size(ControlSize::Small));
                     }});
        }});
    };
    const auto first = item(u8"a", a, a_ref, true);
    const auto second = item(u8"b", b, b_ref, false);
    Signal<std::vector<TreeItem>> data{std::vector<TreeItem>{first, second}};
    f.services.mount(Content{[&] {
        Form(FormProps{},
             FormContent{[&] { Tree(TreeProps{}.items(data).layout(LayoutStyle{}.width(dp(360)).height(dp(120)))); }});
    }});
    sync(f, 720, 600);
    check(a_ref.bound() && b_ref.bound() && bool(old_change), "nested Form fields in Tree did not bind");
    const auto retained = old_change;
    data.set({second, first});
    sync(f, 720, 600);
    retained(u8"A after move");
    check(a.get() == String{u8"A after move"} && b.get() == String{u8"B"} && a_ref.bound(),
          "Tree key reorder redirected a registered Form callback");
    data.set({second, item(u8"c", c, c_ref, false)});
    sync(f, 720, 600);
    retained(u8"stale");
    check(!a_ref.bound() && c_ref.bound() && b.get() == String{u8"B"} && c.get() == String{u8"C"},
          "retired Tree key callback changed a replacement Form field");
    verify_cleanup(f, "keyed callbacks");
}
} // namespace

int main() {
    try {
        private_controls();
        late_rich_tags();
        explicit_popup_item();
        keyed_callbacks();
        return 0;
    } catch (const std::exception& error) {
        std::cerr << error.what() << '\n';
        return 1;
    }
}
