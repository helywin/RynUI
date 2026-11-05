#include "component/tree_select_component.hpp"
#include "component/tree_component.hpp"
#include "support/timeline_fixture.hpp"
#include <ryn/form.hpp>

namespace {
using namespace timeline_test;

void multiple() {
    Fixture f;
    Signal<TreeKeys> accepted{TreeKeys{}};
    TreeKeys request;
    int changed = 0;
    f.services.mount(Content{[&] {
        TreeSelect(
            TreeSelectProps{}
                .multiple()
                .value(accepted)
                .defaultOpen()
                .treeDefaultExpandedKeys({u8"r"})
                .maxTagCount(TreeSelectMaxTagCount{std::size_t{1}})
                .onValuesChange([&](TreeKeys keys) {
                    request = std::move(keys);
                    ++changed;
                })
                .layout(LayoutStyle{}.width(dp(320))),
            {TreeSelectItem{u8"r", u8"Root", {TreeSelectItem{u8"a", u8"Alpha"}, TreeSelectItem{u8"b", u8"Beta"}}}});
    }});
    sync(f);
    const auto id = f.services.tree_select().mounted().front();
    auto s = f.services.tree_select().snapshot(id);
    auto tree = f.services.tree().snapshot(s.tree);
    click(f, tree.rows[1].title);
    check(request == TreeKeys{u8"a"} && f.services.tree_select().snapshot(id).values.empty(),
          "controlled multiple refused authority");
    accepted.set(request);
    sync(f);
    click(f, tree.rows[2].title);
    check(changed == 2 && request == TreeKeys{u8"a", u8"b"},
          "multiple did not toggle without Control or preserve accepted keys");
    accepted.set({u8"a", u8"b"});
    sync(f);
    s = f.services.tree_select().snapshot(id);
    check(s.tags.size() == 2 && s.open, "maxTagCount overflow or multiple popup missing");
    f.services.dispose();
    check(f.inputs.editors().size() == 0 && !f.services.next_frame_deadline(), "multiple disposal leaked");
}

void dynamic_search() {
    Fixture f;
    Signal<String> query{String{}};
    int mounts = 0;
    InputRef rich;
    auto a = TreeSelectItem{u8"a", u8"Alpha"}.richTitle(TreeTitle{[&] {
        ++mounts;
        Input(InputProps{}.ref(rich));
    }});
    Signal<std::vector<TreeSelectItem>> items{std::vector<TreeSelectItem>{a, TreeSelectItem{u8"b", u8"Beta"}}};
    f.services.mount(Content{[&] {
        TreeSelect(TreeSelectProps{}.items(items).showSearch().searchValue(query).defaultOpen().layout(
            LayoutStyle{}.width(dp(320))));
        Tree(TreeProps{}, {TreeItem{u8"separate", u8"Separate"}});
    }});
    sync(f);
    const auto id = f.services.tree_select().mounted().front();
    auto s = f.services.tree_select().snapshot(id);
    const auto old = f.services.tree().snapshot(s.tree).rows[0].title;
    items.set({TreeSelectItem{u8"b", u8"Updated"}, a});
    sync(f);
    check(f.services.tree().snapshot(s.tree).rows[1].title == old && mounts == 1,
          "TreeSelect dynamic data remounted rich node");
    query.set(u8"Alpha");
    sync(f);
    auto rows = f.services.tree().snapshot(s.tree).rows;
    check(!rows[0].shown && rows[1].shown, "controlled search did not filter Tree");
    check(f.services.tree().mounted().size() == 2, "TreeSelect stole standalone Tree host");
    query.set(u8"no matching node");
    sync(f);
    check(std::ranges::any_of(f.services.text().mounted_texts(),
                              [&](const auto& text) {
                                  return f.services.components().branch_active(text.component) &&
                                         f.scene.text_state(text.scene).content().bytes() == "No data";
                              }),
          "filtered TreeSelect did not show actual empty content");

    f.services.dispose();
    check(!rich.bound(), "rich search node ref leaked");
}

void checkbox() {
    Fixture f;
    TreeKeys request;
    TreeSelectLabelValues labels;
    f.services.mount(Content{[&] {
        TreeSelect(
            TreeSelectProps{}
                .treeCheckable()
                .defaultOpen()
                .treeDefaultExpandedKeys({u8"r"})
                .showCheckedStrategy(TreeSelectCheckedStrategy::ShowChild)
                .onValuesChange([&](TreeKeys value) { request = std::move(value); })
                .onLabelChange([&](auto value) { labels = std::move(value); })
                .layout(LayoutStyle{}.width(dp(320))),
            {TreeSelectItem{u8"r", u8"Root", {TreeSelectItem{u8"a", u8"Alpha"}, TreeSelectItem{u8"b", u8"Beta"}}}});
    }});
    sync(f);
    const auto id = f.services.tree_select().mounted().front();
    const auto tree = f.services.tree_select().snapshot(id).tree;
    click(f, f.services.tree().snapshot(tree).rows[0].checkbox);
    check(request == TreeKeys{u8"a", u8"b"} && labels.size() == 2 && labels[0].label == u8"Alpha" &&
              f.services.tree_select().snapshot(id).tags.size() == 2,
          "checkbox strategy/labeled values mismatch");
    f.services.dispose();

    Fixture strict;
    Signal<TreeSelectLabelValues> accepted{TreeSelectLabelValues{
        {u8"a", u8"Saved Alpha", false}, {u8"r", u8"Root", true}, {u8"missing", u8"Retained label", false}}};
    TreeSelectLabelValues requested;
    strict.services.mount(Content{[&] {
        TreeSelect(TreeSelectProps{}
                       .treeCheckable()
                       .treeCheckStrictly()
                       .labelInValue()
                       .value(accepted)
                       .defaultOpen()
                       .treeDefaultExpandedKeys({u8"r"})
                       .onLabelChange([&](auto next) { requested = std::move(next); })
                       .layout(LayoutStyle{}.width(dp(320))),
                   {TreeSelectItem{u8"r", u8"Root", {TreeSelectItem{u8"a", u8"Alpha"}}}});
    }});
    sync(strict);
    const auto selector = strict.services.tree_select().mounted().front();
    const auto strict_tree = strict.services.tree_select().snapshot(selector).tree;
    const auto before = strict.services.tree().snapshot(strict_tree);
    check(before.checked == TreeKeys{u8"a"} && before.half_checked == TreeKeys{u8"r"},
          "labeled strict input lost checked/half authority");
    click(strict, before.rows[1].checkbox);
    check(requested == TreeSelectLabelValues{{u8"missing", u8"Retained label", false}, {u8"r", u8"Root", true}} &&
              strict.services.tree_select().snapshot(selector).values == TreeKeys{u8"a", u8"missing"},
          "strict callback lost retained label/half or changed refused authority");
    accepted.set(requested);
    sync(strict);
    check(strict.services.tree_select().snapshot(selector).values == TreeKeys{u8"missing"},
          "labeled controlled value did not accept request");
    strict.services.dispose();
}

void form_reset_search() {
    Fixture f;
    FormField<String> field{String{u8"kept"}};
    FormRef form;
    TreeSelectRef reference;
    std::shared_ptr<const std::atomic_bool> cancelled;
    TreeLoader::Complete complete;
    TreeLoader source{[&](AsyncOptionsRequest request, TreeLoader::Complete done) {
                          cancelled = request.cancelled;
                          complete = std::move(done);
                          return AsyncOptionsCancel{};
                      },
                      [](auto apply) { apply(); }};
    f.services.mount(Content{[&] {
        Form(FormProps{}.ref(form), FormContent{[&] {
                 FormItem(FormItemProps{}.field(u8"node", field).control(reference), FormItemContent{[&] {
                              TreeSelect(TreeSelectProps{}
                                             .value(field.value())
                                             .onChange(field.onChange())
                                             .ref(reference)
                                             .showSearch()
                                             .loadData(source)
                                             .layout(LayoutStyle{}.width(dp(320))),
                                         {TreeSelectItem{u8"lazy", u8"Lazy"}});
                          }});
             }});
    }});
    sync(f);
    const auto id = f.services.tree_select().mounted().front();
    const auto before = f.services.tree_select().snapshot(id);
    click(f, before.selector);
    auto rows = f.services.tree().snapshot(before.tree).rows;
    click(f, rows[0].switcher);
    check(cancelled && !cancelled->load(), "TreeSelect did not start node loader");
    check(reference.focus(), "search selector ref did not focus");
    const auto stamp = f.inputs.sessions().active();
    check(stamp.valid(), "search did not start native editor session");
    check(bool(f.inputs.dispatch(TextCommitted{String{u8"draft"}, stamp})), "search text dispatch failed");
    sync(f);
    check(!f.services.tree_select().snapshot(id).search.empty(), "search draft did not update");
    const auto native = std::ranges::find_if(f.inputs.mounted_inputs(),
                                             [&](const auto& input) { return input.component == before.selector; });
    check(native != f.inputs.mounted_inputs().end(), "String Form selector editor missing");
    const auto selector_editor = *native;
    const auto selector_box = bounds(f, before.selector);
    f.services.pointer().dispatch({PointerIdentity::mouse(), PointerAction::down, PointerButton::primary,
                                   selector_box.x + 4, selector_box.y + selector_box.height / 2});
    check(bool(f.inputs.dispatch(CompositionChanged{String{u8"候选"}, {}, f.inputs.sessions().active()})),
          "String Form preedit setup failed");
    check(form.reset(), "same-value Form reset failed");
    sync(f);
    auto after = f.services.tree_select().snapshot(id);
    check(cancelled->load() && after.search.empty() && !after.open && after.value == u8"kept" &&
              after.selector == before.selector && after.tree == before.tree,
          "same-value Form reset retained async/query or replaced controls");
    check(f.services.focus().state().focused == selector_editor.interaction &&
              f.inputs.sessions().active().owner == selector_editor.editor &&
              !f.inputs.editors().require(selector_editor.editor).composition().active &&
              !f.services.pointer().state(PointerIdentity::mouse())->capture,
          "String Form reset lost selector focus or retained preedit/capture");
    complete({{TreeItem{u8"stale", u8"Stale"}}, {}});
    check(f.services.tree().snapshot(before.tree).rows.size() == 1, "TreeSelect reset accepted stale loader result");
    f.services.dispose();
    check(!reference.bound(), "Form search ref leaked");

    Fixture tags;
    FormField<String> adapter{String{u8"unchanged"}};
    Signal<TreeKeys> accepted{TreeKeys{u8"a"}};
    FormRef tag_form;
    TreeSelectRef tag_selector;
    InputRef tag_editor;
    int factories = 0;
    tags.services.mount(Content{[&] {
        Form(FormProps{}.ref(tag_form), FormContent{[&] {
                 FormItem(FormItemProps{}.field(u8"adapter", adapter).control(tag_selector), FormItemContent{[&] {
                              TreeSelect(
                                  TreeSelectProps{}
                                      .multiple()
                                      .value(accepted)
                                      .ref(tag_selector)
                                      .tagRender([&](TreeSelectTagInfo) {
                                          return TreeSelectTag{[&] {
                                              ++factories;
                                              Input(InputProps{}.ref(tag_editor).layout(LayoutStyle{}.width(dp(80))));
                                          }};
                                      })
                                      .layout(LayoutStyle{}.width(dp(320))),
                                  {TreeSelectItem{u8"a", u8"Alpha"}});
                          }});
             }});
    }});
    sync(tags);
    const auto owner = tags.services.tree_select().mounted().front();
    const auto original = tags.services.tree_select().snapshot(owner);
    const auto editor = std::ranges::find_if(tags.inputs.mounted_inputs(), [&](const auto& item) {
        return item.component != original.selector && descendant(tags, item.component, owner);
    });
    check(editor != tags.inputs.mounted_inputs().end() && tag_editor.focus(), "rich tag editor focus failed");
    const auto box = bounds(tags, editor->component);
    tags.services.pointer().dispatch({PointerIdentity::mouse(), PointerAction::down, PointerButton::primary,
                                      box.x + box.width / 2, box.y + box.height / 2});
    const auto tag_stamp = tags.inputs.sessions().active();
    check(bool(tags.inputs.dispatch(CompositionChanged{String{u8"preedit"}, {0, 1}, tag_stamp})) &&
              tags.services.pointer().state(PointerIdentity::mouse())->capture.has_value(),
          "rich tag reset fixture lacked composition/capture");
    check(tag_form.reset(), "rich tag adapter reset failed");
    sync(tags);
    check(!tags.inputs.sessions().active().valid() &&
              !tags.inputs.editors().require(editor->editor).composition().active &&
              !tags.services.pointer().state(PointerIdentity::mouse())->capture && factories == 1 &&
              tags.services.tree_select().snapshot(owner).selector == original.selector &&
              accepted.get() == TreeKeys{u8"a"},
          "same-value reset retained rich tag session/capture or remounted accepted content");
    tags.services.dispose();
    check(!tag_editor.bound() && !tag_selector.bound(), "rich tag reset leaked refs");
}

void responsive_idle() {
    Fixture f;
    f.services.mount(Content{[&] {
        TreeSelect(TreeSelectProps{}
                       .multiple()
                       .defaultValue(TreeKeys{u8"a", u8"b", u8"c"})
                       .maxTagCount(TreeSelectMaxTagCount{TreeSelectTagCount::Responsive})
                       .layout(LayoutStyle{}.width(dp(180))),
                   {TreeSelectItem{u8"a", u8"Long Alpha"}, TreeSelectItem{u8"b", u8"Long Beta"},
                    TreeSelectItem{u8"c", u8"Long Gamma"}});
    }});
    for (int i = 0; i < 4; ++i) {
        sync(f);
    }
    const auto id = f.services.tree_select().mounted().front();
    const auto selector = f.services.tree_select().snapshot(id).selector;
    const auto node = f.services.components().root(selector);
    const auto measured = f.nodes.require(node).measure_count;
    for (int i = 0; i < 5; ++i) {
        sync(f);
    }
    check(f.nodes.require(node).measure_count == measured && !f.services.next_frame_deadline(),
          "responsive tags manufactured idle layout/animation work");
    f.services.dispose();
}

void rich_slots() {
    Fixture f;
    InputRef suffix;
    int prefix_calls = 0;
    int popup_calls = 0;
    int factories = 0;
    f.services.mount(Content{[&] {
        TreeSelect(
            TreeSelectProps{}
                .showSearch()
                .prefix(TreeSelectAffix{
                    [&] { Button(ButtonProps{}.onClick([&] { ++prefix_calls; }), [] { Text(u8"Prefix"); }); }})
                .suffix(TreeSelectAffix{[&] { Input(InputProps{}.ref(suffix).layout(LayoutStyle{}.width(dp(60)))); }})
                .popupRender([&](std::function<void()> original) {
                    return TreeSelectPopup{[&, original] {
                        ++factories;
                        original();
                        Button(ButtonProps{}.onClick([&] { ++popup_calls; }), [] { Text(u8"Footer"); });
                    }};
                })
                .layout(LayoutStyle{}.width(dp(400))),
            {TreeSelectItem{u8"a", u8"Alpha"}});
    }});
    sync(f);
    const auto id = f.services.tree_select().mounted().front();
    const auto selector = f.services.tree_select().snapshot(id).selector;
    const auto press = [&](runtime::ComponentId owner) {
        for (auto hit : f.services.interactions().declaration_order()) {
            const auto& record = f.services.interactions().require(hit);
            if (record.component == owner && record.focusable) {
                check(f.services.focus().request_focus(hit, FocusModality::keyboard), "rich action focus failed");
                f.services.focus().dispatch({Key::enter, KeyAction::down});
                f.services.focus().dispatch({Key::enter, KeyAction::up});
                sync(f);
                return;
            }
        }
        throw std::runtime_error("rich action interaction missing");
    };
    press(f.buttons.mounted_buttons().front().component);
    check(prefix_calls == 1 && !f.services.tree_select().snapshot(id).open, "selector consumed rich affix Enter");
    click(f, f.buttons.mounted_buttons().front().component);
    check(prefix_calls == 2 && !f.services.tree_select().snapshot(id).open,
          "selector consumed rich affix pointer action");
    check(suffix.bound() && suffix.focus(), "rich suffix editor could not focus");
    f.services.focus().dispatch({Key::enter, KeyAction::down});
    f.services.focus().dispatch({Key::enter, KeyAction::up});
    sync(f);
    check(!f.services.tree_select().snapshot(id).open, "selector consumed rich suffix editor command");
    click(f, selector);
    press(f.buttons.mounted_buttons().back().component);
    check(popup_calls == 1 && factories == 1 && f.services.tree_select().snapshot(id).open,
          "public popup origin or retained action contract failed");
    f.services.dispose();
    check(!suffix.bound(), "rich suffix ref leaked");
}

void callback_failure() {
    Fixture f;
    TreeSelectRef reference;
    int healthy = 0;
    f.services.mount(Content{[&] {
        TreeSelect(TreeSelectProps{}
                       .multiple()
                       .ref(reference)
                       .defaultOpen()
                       .onValuesChange([](auto) { throw std::runtime_error("callback"); })
                       .layout(LayoutStyle{}.width(dp(300))),
                   {TreeSelectItem{u8"a", u8"Alpha"}});
        Button(ButtonProps{}.onClick([&] { ++healthy; }), [] { Text(u8"Healthy"); });
    }});
    sync(f);
    const auto id = f.services.tree_select().mounted().front();
    const auto tree = f.services.tree_select().snapshot(id).tree;
    bool threw = false;
    try {
        click(f, f.services.tree().snapshot(tree).rows[0].title);
    } catch (const std::runtime_error&) {
        threw = true;
    }
    check(threw && !reference.bound() && f.services.tree_select().mounted().empty() &&
              f.services.tree().mounted().empty(),
          "throwing multi-value callback retained a partial compound");
    click(f, f.buttons.mounted_buttons().front().component);
    check(healthy == 1, "callback failure damaged unrelated control");
    f.services.dispose();
}

} // namespace

int main(int argc, char** argv) {
    try {
        const std::string_view name = argc > 1 ? argv[1] : "multiple";
        if (name == "multiple") {
            multiple();
        } else if (name == "dynamic_search") {
            dynamic_search();
        } else if (name == "form_reset_search") {
            form_reset_search();
        } else if (name == "responsive_idle") {
            responsive_idle();
        } else if (name == "rich_slots") {
            rich_slots();
        } else if (name == "callback_failure") {
            callback_failure();
        } else if (name == "checkbox") {
            checkbox();
        } else {
            return 2;
        }
        std::cout << name << " passed\n";
        return 0;
    } catch (const std::exception& e) {
        std::cerr << e.what() << '\n';
        return 1;
    }
}
