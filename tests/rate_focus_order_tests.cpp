#include "support/rate_fixture.hpp"
#include <string_view>

namespace {
using namespace rate_test;

bool belongs(Fixture& f, runtime::ComponentId child, runtime::ComponentId owner) {
    for (auto current = std::optional{child}; current; current = f.services.components().parent(*current)) {
        if (*current == owner) {
            return true;
        }
    }
    return false;
}

auto item_input(Fixture& f, runtime::ComponentId item) {
    for (const auto& input : f.inputs.mounted_inputs()) {
        if (belongs(f, input.component, item)) {
            return input;
        }
    }
    throw std::runtime_error("Rate item has no native Input");
}

std::vector<InteractionId> foreign_order(Fixture& f, runtime::ComponentId owner) {
    std::vector<InteractionId> result;
    for (auto hit : f.services.interactions().declaration_order()) {
        if (!belongs(f, f.services.interactions().require(hit).component, owner)) {
            result.push_back(hit);
        }
    }
    return result;
}

void ordered_owned(Fixture& f, runtime::ComponentId owner) {
    const auto snapshot = f.services.rate().snapshot(owner);
    std::vector<InteractionId> expected;
    for (const auto& item : snapshot.items) {
        for (auto hit : f.services.interactions().declaration_order()) {
            if (belongs(f, f.services.interactions().require(hit).component, item.component)) {
                expected.push_back(hit);
            }
        }
    }
    const auto all = f.services.interactions().declaration_order();
    const auto root = std::ranges::find(all, snapshot.focus);
    check(root != all.end() && static_cast<std::size_t>(all.end() - root - 1) >= expected.size() &&
              std::equal(expected.begin(), expected.end(), root + 1),
          "complete retained item interactions are not contiguous in logical order after Rate root");
}

void tab_to(Fixture& f, InteractionId expected, const char* message) {
    key(f, Key::tab);
    check(f.services.focus().state().focused == expected, message);
}

void minimal(bool before) {
    Fixture f;
    Signal<int> count{0};
    InputRef ref;
    f.services.mount(Content{[&] {
        Rate(RateProps{}.count(count).tooltips({RateTooltip{TooltipTitle{[&] { Input(InputProps{}.ref(ref)); }}}}));
        Button(ButtonProps{}, [] { Text(u8"After"); });
    }});
    sync(f);
    const auto id = f.services.rate().mounted()[0];
    const auto root = f.services.rate().snapshot(id).focus;
    const auto after = f.buttons.mounted_buttons()[0].interaction;
    count.set(1);
    sync(f);
    const auto input = item_input(f, f.services.rate().snapshot(id).items[0].component);
    tab_to(f, root, "count 0->1 did not create Rate Tab entry");
    check(f.services.tooltip().snapshot(f.services.tooltip().mounted()[0]).visible &&
              f.services.interactions().branch_active(input.interaction),
          "Rate Tab did not activate rich Tooltip Input");
    key(f, Key::tab);
    if (before) {
        const auto& registry = f.services.interactions();
        std::cout << "BEFORE count0->1: Tab skipped Input to external Button="
                  << (f.services.focus().state().focused == after)
                  << "; declaration Root=" << registry.require(root).declaration_order
                  << " Button=" << registry.require(after).declaration_order
                  << " Input=" << registry.require(input.interaction).declaration_order << '\n';
        check(f.services.focus().state().focused == after, "dynamic append P2 did not reproduce");
        return;
    }
    check(f.services.focus().state().focused == input.interaction && ref.focus(),
          "count 0->1 Tab skipped native Input");
    ordered_owned(f, id);
    tab_to(f, after, "Input Tab did not leave to original external Button");
}

void growth_and_isolation() {
    Fixture f;
    Signal<int> count_a{1};
    Signal<int> count_b{1};
    std::array<InputRef, 4> refs_a;
    std::array<InputRef, 4> refs_b;
    const auto tips = [](std::array<InputRef, 4>& refs) {
        std::vector<RateTooltip> result;
        for (std::size_t i = 0; i < refs.size(); ++i) {
            result.emplace_back(TooltipTitle{[&refs, i] { Input(InputProps{}.defaultValue(u8"Draft").ref(refs[i])); }});
        }
        return result;
    };
    InteractionId ancestor;
    f.services.mount(Content{[&] {
        Button(ButtonProps{}, [] { Text(u8"Before"); });
        auto& build = runtime::require_component_build_context();
        const auto parent = build.mount_component<int>(0);
        const auto node = build.root(parent);
        f.layout.set_layout(node, layout::FlexLayout{layout::FlexDirection::vertical});
        ancestor = f.services.interactions().create({parent, node, {}, true, false, {}, false, false});
        const auto fragment = build.register_scene_fragment(parent, runtime::SceneFragmentPlacement::before_children);
        f.services.scene_composer().set_fragment(fragment, {}, ancestor);
        build.on_resource_cleanup(parent, [&f, node, ancestor, fragment] {
            f.services.interactions().remove(ancestor);
            f.services.scene_composer().remove_fragment(fragment);
            f.layout.remove_layout(node);
        });
        build.mount_slot(parent, Content{[&] {
                             Rate(RateProps{}.count(count_a).tooltips(tips(refs_a)));
                             Button(ButtonProps{}, [] { Text(u8"Between"); });
                             Rate(RateProps{}.count(count_b).tooltips(tips(refs_b)));
                         }});
        Button(ButtonProps{}, [] { Text(u8"After"); });
    }});
    sync(f);
    const auto a = f.services.rate().mounted()[0];
    const auto b = f.services.rate().mounted()[1];
    const auto root_a = f.services.rate().snapshot(a).focus;
    const auto root_b = f.services.rate().snapshot(b).focus;
    const auto before_button = f.buttons.mounted_buttons()[0].interaction;
    const auto middle_button = f.buttons.mounted_buttons()[1].interaction;
    const auto after_button = f.buttons.mounted_buttons()[2].interaction;
    const auto first_a = f.services.rate().snapshot(a).items[0];
    const auto first_b = f.services.rate().snapshot(b).items[0];
    const auto input_a = item_input(f, first_a.component);
    const auto input_b = item_input(f, first_b.component);
    tab_to(f, before_button, "before Button order drifted");
    tab_to(f, root_a, "Rate A order drifted");
    tab_to(f, input_a.interaction, "initial Input A order drifted");
    check(refs_a[0].focus({InputFocusCursor::All}), "draft fixture could not select Input A");
    check(bool(f.inputs.dispatch(TextCommitted{String{u8"Saved A"}, f.inputs.sessions().active()})),
          "draft fixture failed");
    const auto foreign_a = foreign_order(f, a);
    count_a.set(3);
    sync(f);
    check(foreign_order(f, a) == foreign_a, "A growth moved ancestor/neighbor/foreign owner interactions");
    ordered_owned(f, a);
    const auto a_three = f.services.rate().snapshot(a);
    const auto input_a2 = item_input(f, a_three.items[2].component);
    check(a_three.items[0].component == first_a.component &&
              item_input(f, first_a.component).editor == input_a.editor &&
              f.inputs.editors().require(input_a.editor).value() == "Saved A",
          "growth remounted or reset original editor/draft");
    f.services.focus().clear_focus();
    tab_to(f, before_button, "before Button moved after A growth");
    tab_to(f, root_a, "Rate A entry moved after growth");
    key(f, Key::end);
    tab_to(f, input_a2.interaction, "appended last-item Input A is not before neighboring Button");
    tab_to(f, middle_button, "middle Button moved after appended Input A");
    tab_to(f, root_b, "Rate B moved after A growth");
    tab_to(f, input_b.interaction, "Input B moved after A growth");
    const auto foreign_b = foreign_order(f, b);
    count_b.set(3);
    sync(f);
    check(foreign_order(f, b) == foreign_b, "B growth moved A or foreign neighbors");
    ordered_owned(f, a);
    ordered_owned(f, b);
    const auto b_three = f.services.rate().snapshot(b);
    const auto input_b2 = item_input(f, b_three.items[2].component);
    tab_to(f, after_button, "hidden appended B Inputs entered Tab order");
    tab_to(f, before_button, "external after/before declaration order drifted");
    tab_to(f, root_a, "A root entry drifted while B grew");
    tab_to(f, input_a2.interaction, "A last item lost its order when B grew");
    count_a.set(1);
    sync(f);
    check(!f.services.interactions().branch_active(input_a2.interaction), "shrunk item stayed eligible");
    const auto foreign_before_four = foreign_order(f, a);
    count_a.set(4);
    sync(f); // Old items 1/2 are inactive while item 3 is being appended.
    check(foreign_order(f, a) == foreign_before_four, "growth with inactive retained items moved foreign owner");
    ordered_owned(f, a);
    ordered_owned(f, b);
    const auto a_four = f.services.rate().snapshot(a);
    const auto input_a3 = item_input(f, a_four.items[3].component);
    check(a_four.items[2].component == a_three.items[2].component &&
              item_input(f, a_four.items[2].component).editor == input_a2.editor,
          "hidden retained editor was remounted");
    f.services.focus().clear_focus();
    tab_to(f, before_button, "before Button moved after retained-item growth");
    tab_to(f, root_a, "A entry moved after retained-item growth");
    key(f, Key::end);
    tab_to(f, input_a3.interaction, "new fourth Input A could not be entered by Tab");
    count_a.set(1);
    sync(f);
    f.services.focus().clear_focus();
    tab_to(f, before_button, "before Button moved after shrink");
    tab_to(f, root_a, "A entry moved after shrink");
    tab_to(f, input_a.interaction, "shrink did not retain original Input A order");
    tab_to(f, middle_button, "inactive retained A Inputs entered Tab traversal");
    count_a.set(3);
    sync(f);
    f.services.focus().clear_focus();
    tab_to(f, before_button, "before Button moved after restore");
    tab_to(f, root_a, "A entry moved after restore");
    tab_to(f, input_a2.interaction, "restored retained Input A lost logical Tab order");
    check(refs_a[2].focus() && item_input(f, a_three.items[2].component).editor == input_a2.editor &&
              f.inputs.editors().require(input_a.editor).value() == "Saved A",
          "restore lost retained editor/ref/draft");
    tab_to(f, middle_button, "hidden fourth A Input entered Tab traversal");
    tab_to(f, root_b, "B root moved after A shrink/restore");
    key(f, Key::end);
    tab_to(f, input_b2.interaction, "B appended Input lost logical Tab order");
    check(refs_b[2].focus() && b_three.items[0].component == first_b.component &&
              item_input(f, first_b.component).editor == input_b.editor,
          "other Rate owner lost original editor/ref");
    tab_to(f, after_button, "external after Button moved after multiple-owner growth");
    ordered_owned(f, a);
    ordered_owned(f, b);
    check(f.services.interactions().require(ancestor).declaration_order <
              f.services.interactions().require(root_a).declaration_order,
          "Rate reorder moved its ancestor");
    f.services.dispose();
    check(blocked(f.services.interactions()).empty(), "focus-order teardown leaked owned blocks");
}
} // namespace

int main(int argc, char** argv) {
    try {
        const bool before = argc > 1 && std::string_view{argv[1]} == "--before";
        minimal(before);
        if (!before) {
            growth_and_isolation();
        }
        std::cout << "Rate dynamic focus order passed\n";
        return 0;
    } catch (const std::exception& e) {
        std::cerr << e.what() << '\n';
        return 1;
    }
}
