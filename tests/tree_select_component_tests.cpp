#include "component/tree_select_component.hpp"
#include "component/tree_component.hpp"
#include "component/popover_component.hpp"
#include "component/form_component.hpp"
#include "support/timeline_fixture.hpp"
#include <ryn/form.hpp>
#include <string_view>

namespace {
using namespace timeline_test;

void key(Fixture& f, Key value, KeyModifier modifiers = KeyModifier::none) {
    f.services.focus().dispatch({value, KeyAction::down, modifiers});
    f.services.focus().dispatch({value, KeyAction::up, modifiers});
    sync(f);
}

std::vector<TreeSelectItem> items() {
    return {TreeSelectItem{u8"root", u8"Root", {TreeSelectItem{u8"a", u8"Alpha"}, TreeSelectItem{u8"b", u8"Beta"}}}};
}

runtime::ComponentId owner(Fixture& f) {
    check(f.services.tree_select().mounted().size() == 1, "TreeSelect owner count is invalid");
    return f.services.tree_select().mounted().front();
}

void clean(Fixture& f) {
    f.services.dispose();
    check(f.nodes.size() == 0 && f.scene.size() == 0 && f.scene.glyph_scene().instances().size() == 0 &&
              f.services.surfaces().size() == 0 && f.services.interactions().size() == 0 &&
              blocked_roots(f.services.interactions()).empty() && !f.services.next_frame_deadline(),
          "TreeSelect leaked resources or blocks");
}

void basic() {
    Fixture f;
    int changes = 0;
    String accepted;
    f.services.mount(Content{[&] {
        TreeSelect(TreeSelectProps{}
                       .allowClear()
                       .treeDefaultExpandedKeys({u8"root"})
                       .onChange([&](String value) {
                           ++changes;
                           accepted = std::move(value);
                       })
                       .layout(LayoutStyle{}.width(dp(280))),
                   items());
    }});
    sync(f);
    const auto id = owner(f);
    auto s = f.services.tree_select().snapshot(id);
    check(!s.open && s.value.empty(), "TreeSelect did not mount closed and empty");
    const auto original_tree = s.tree;
    const auto original_selector = s.selector;
    click(f, s.selector);
    s = f.services.tree_select().snapshot(id);
    check(s.open && s.popup.width <= 280.01F && s.popup.width > 260, "pointer did not open matching-width popup");
    auto tree = f.services.tree().snapshot(s.tree);
    check(tree.rows.size() == 3 && tree.rows[1].shown, "TreeSelect did not mount real expanded Tree");
    click(f, tree.rows[1].title);
    s = f.services.tree_select().snapshot(id);
    check(changes == 1 && accepted == u8"a" && s.value == u8"a" && s.label == u8"Alpha" && !s.open,
          "pointer selection did not commit label and close");
    click(f, s.selector);
    click(f, tree.rows[1].title);
    s = f.services.tree_select().snapshot(id);
    check(changes == 1 && s.value == u8"a" && !s.open, "reselecting current value toggled or emitted change");
    std::optional<InteractionId> clear;
    for (auto hit : f.services.interactions().declaration_order()) {
        const auto& r = f.services.interactions().require(hit);
        if (r.component != s.selector && descendant(f, r.component, s.selector) && r.eligible) {
            clear = hit;
            break;
        }
    }
    check(clear.has_value(), "TreeSelect native clear action is missing");
    click(f, f.services.interactions().require(*clear).component);
    s = f.services.tree_select().snapshot(id);
    check(s.value.empty() && changes == 2 && accepted.empty() && !s.open, "native clear failed or reopened popup");
    check(s.selector == original_selector && s.tree == original_tree, "ordinary selection recreated controls");
    clean(f);
}

void controlled() {
    Fixture f;
    Signal<String> value{String{u8"unknown"}};
    Signal<bool> open{true};
    Signal<TreeKeys> expanded{TreeKeys{}};
    String intent;
    TreeExpandInfo expand;
    int changes = 0;
    int closes = 0;
    f.services.mount(Content{[&] {
        TreeSelect(TreeSelectProps{}
                       .value(value)
                       .open(open)
                       .treeExpandedKeys(expanded)
                       .onChange([&](String v) {
                           intent = std::move(v);
                           ++changes;
                       })
                       .onOpenChange([&](bool v) { closes += !v; })
                       .onTreeExpand([&](auto info) { expand = std::move(info); })
                       .layout(LayoutStyle{}.width(dp(280))),
                   items());
    }});
    sync(f);
    const auto id = owner(f);
    auto s = f.services.tree_select().snapshot(id);
    check(s.open && s.label == u8"unknown", "controlled unknown value was discarded");
    auto tree = f.services.tree().snapshot(s.tree);
    click(f, tree.rows[0].switcher);
    check(expand.expanded_keys == TreeKeys{u8"root"} && !f.services.tree().snapshot(s.tree).rows[1].shown,
          "controlled expansion mutated before acceptance");
    expanded.set(expand.expanded_keys);
    sync(f);
    click(f, tree.rows[2].title);
    s = f.services.tree_select().snapshot(id);
    check(changes == 1 && intent == u8"b" && s.value == u8"unknown" && s.open && closes > 0,
          "controlled rejection changed accepted value/open");
    click(f, tree.rows[2].title);
    check(changes == 2, "controlled rejection suppressed retry");
    value.set(intent);
    open.set(false);
    sync(f);
    s = f.services.tree_select().snapshot(id);
    check(s.value == u8"b" && s.label == u8"Beta" && !s.open, "controlled acceptance failed");
    clean(f);
}

void keyboard() {
    Fixture f;
    InputRef before;
    InputRef selector;
    InputRef after;
    f.services.mount(Content{[&] {
        Input(InputProps{}.ref(before));
        TreeSelect(TreeSelectProps{}.ref(selector).layout(LayoutStyle{}.width(dp(280))), items());
        Input(InputProps{}.ref(after));
    }});
    sync(f);
    const auto id = owner(f);
    check(selector.focus(), "TreeSelect ref did not focus native selector");
    key(f, Key::down);
    auto s = f.services.tree_select().snapshot(id);
    auto tree = f.services.tree().snapshot(s.tree);
    check(s.open && f.services.focus().state().focused == tree.rows[0].interaction &&
              f.services.focus().state().focus_visible,
          "keyboard open did not focus real Tree row");
    key(f, Key::right);
    key(f, Key::right);
    key(f, Key::enter);
    s = f.services.tree_select().snapshot(id);
    check(s.value == u8"a" && !s.open && f.services.focus().state().focused == f.inputs.mounted_inputs()[1].interaction,
          "keyboard selection did not restore selector");
    key(f, Key::down);
    key(f, Key::escape);
    check(!f.services.tree_select().snapshot(id).open &&
              f.services.focus().state().focused == f.inputs.mounted_inputs()[1].interaction,
          "Escape did not restore selector");
    key(f, Key::down);
    key(f, Key::tab);
    check(!f.services.tree_select().snapshot(id).open &&
              f.services.focus().state().focused == f.inputs.mounted_inputs()[2].interaction,
          "Tab from Tree did not move to adjacent native control");
    check(selector.focus(), "ref did not refocus");
    key(f, Key::down);
    key(f, Key::tab, KeyModifier::shift);
    check(f.services.focus().state().focused == f.inputs.mounted_inputs()[0].interaction,
          "Shift Tab from Tree did not move to previous control");
    clean(f);
}

void retained() {
    Fixture f;
    Signal<String> title{String{u8"Alpha"}};
    Signal<bool> hidden{false};
    Signal<bool> disabled{false};
    f.services.mount(Content{[&] {
        TreeSelect(
            TreeSelectProps{}.defaultValue(u8"a").defaultOpen().treeDefaultExpandedKeys({u8"root"}),
            {TreeSelectItem{
                u8"root",
                u8"Root",
                {TreeSelectItem{u8"a", title}.visible(bind([hidden] { return !hidden.get(); })).disabled(disabled)}}});
    }});
    sync(f);
    const auto id = owner(f);
    const auto old = f.services.tree_select().snapshot(id);
    const auto rows = f.services.tree().snapshot(old.tree).rows;
    title.set(u8"Renamed");
    hidden.set(true);
    disabled.set(true);
    sync(f);
    auto next = f.services.tree_select().snapshot(id);
    check(next.selector == old.selector && next.tree == old.tree && next.label == u8"Renamed" && next.value == u8"a" &&
              !f.services.tree().snapshot(next.tree).rows[1].shown,
          "presentation update lost accepted value or replaced retained component");
    hidden.set(false);
    disabled.set(false);
    sync(f);
    check(f.services.tree().snapshot(next.tree).rows[1].component == rows[1].component,
          "hidden recovery recreated row identity");
    clean(f);
}

void boundaries() {
    Fixture f;
    Signal<float> height{32};
    Signal<bool> disabled{false};
    Signal<bool> visible{true};
    Signal<bool> read_only{false};
    InputRef ref;
    f.services.mount(Content{[&] {
        TreeSelect(
            TreeSelectProps{}.defaultOpen().ref(ref).disabled(disabled).visible(visible).readOnly(read_only).layout(
                LayoutStyle{}.width(dp(240)).height(bind([height] { return dp(height.get()); }))),
            items());
    }});
    sync(f);
    const auto id = owner(f);
    const auto original = f.services.tree_select().snapshot(id);
    check(ref.focus(), "boundary selector focus failed");
    height.set(0);
    sync(f);
    check(!f.services.tree_select().snapshot(id).open && !f.services.focus().state().focused,
          "zero area retained popup or focus");
    height.set(32);
    sync(f);
    check(!f.services.tree_select().snapshot(id).open, "geometry recovery resurrected suppressed popup");
    click(f, original.selector);
    check(f.services.tree_select().snapshot(id).open, "explicit geometry recovery open failed");
    disabled.set(true);
    sync(f);
    check(!f.services.tree_select().snapshot(id).open && !ref.focus(), "disabled selector remained operable");
    disabled.set(false);
    read_only.set(true);
    sync(f);
    check(ref.focus(), "readOnly selector should remain focusable");
    key(f, Key::down);
    check(!f.services.tree_select().snapshot(id).open, "readOnly selector opened");
    read_only.set(false);
    visible.set(false);
    sync(f);
    check(!ref.focus(), "hidden selector remained focusable");
    visible.set(true);
    sync(f);
    check(!f.services.tree_select().snapshot(id).open, "visibility recovery resurrected popup");
    clean(f);
}
} // namespace

int main(int argc, char** argv) {
    try {
        const std::string_view name = argc > 1 ? argv[1] : "basic";
        if (name == "basic") {
            basic();
        } else if (name == "controlled") {
            controlled();
        } else if (name == "keyboard") {
            keyboard();
        } else if (name == "retained") {
            retained();
        } else if (name == "boundaries") {
            boundaries();
        } else {
            throw std::runtime_error("Unknown TreeSelect case");
        }
        std::cout << name << " passed\n";
        return 0;
    } catch (const std::exception& e) {
        std::cerr << e.what() << '\n';
        return 1;
    }
}
