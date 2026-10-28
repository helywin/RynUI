#include "component/tree_select_component.hpp"
#include "component/tree_component.hpp"
#include "component/popover_component.hpp"
#include "support/timeline_fixture.hpp"
#include <string_view>

namespace {
using namespace timeline_test;

void key(Fixture& f, Key value) {
    f.services.focus().dispatch({value, KeyAction::down});
    f.services.focus().dispatch({value, KeyAction::up});
    sync(f);
}

void outside(Fixture& f) {
    for (auto action : {PointerAction::down, PointerAction::up}) {
        f.services.pointer().dispatch({PointerIdentity::mouse(), action, PointerButton::primary, 550, 700});
    }
    sync(f);
}

void clean(Fixture& f) {
    f.services.dispose();
    check(f.nodes.size() == 0 && f.scene.size() == 0 && f.services.surfaces().size() == 0 &&
              f.services.interactions().size() == 0 && f.inputs.editors().size() == 0 &&
              !f.inputs.sessions().active().valid() && blocked_roots(f.services.interactions()).empty() &&
              !f.services.next_frame_deadline(),
          "TreeSelect R1 cleanup leaked resources");
}

void outside_retry() {
    for (bool direct_popover : {false, true}) {
        for (bool throw_first : {false, true}) {
            Fixture f;
            Signal<bool> open{true};
            bool accept = false;
            bool retire = false;
            runtime::ComponentId original;
            int closes = 0;
            const auto change = [&](bool value) {
                check(!value, "outside close delivered an opening request");
                ++closes;
                if (retire) {
                    f.services.destroy(original);
                    return;
                }
                if (throw_first && closes == 1) {
                    throw std::runtime_error("original close failure");
                }
                if (accept) {
                    open.set(false);
                }
            };
            f.services.mount(Content{[&] {
                if (direct_popover) {
                    Popover(PopoverProps{}.open(open).trigger(PopoverTriggerMode::Manual).onOpenChange(change),
                            PopoverTrigger{[] { Button(ButtonProps{}, ButtonContent{[] { Text(u8"Trigger"); }}); }},
                            PopoverSlots{.content = PopoverContent{[] { Text(u8"Controlled manual content"); }}});
                } else {
                    TreeSelect(TreeSelectProps{}.open(open).defaultValue(u8"a").onOpenChange(change),
                               {TreeSelectItem{u8"a", u8"Alpha"}});
                }
            }});
            sync(f);
            original =
                direct_popover ? f.services.popover().mounted().front() : f.services.tree_select().mounted().front();
            const auto visible = [&] {
                return direct_popover
                           ? f.services.popover().snapshot(f.services.popover().mounted().front()).geometry.visible
                           : f.services.tree_select().snapshot(f.services.tree_select().mounted().front()).open;
            };
            check(!f.services.focus().state().focused && visible(),
                  "outside retry fixture acquired focus or stayed closed");
            for (int cycle = 0; cycle < 2; ++cycle) {
                accept = false;
                bool caught = false;
                try {
                    outside(f);
                } catch (const std::runtime_error& error) {
                    check(throw_first && cycle == 0 && std::string_view{error.what()} == "original close failure",
                          "manual request replaced or unexpectedly threw callback exception");
                    caught = true;
                    f.services.pointer().dispatch(
                        {PointerIdentity::mouse(), PointerAction::up, PointerButton::primary, 550, 700});
                    sync(f);
                }
                check(caught == (throw_first && cycle == 0), "manual close exception was swallowed");
                check(closes == cycle * 2 + 1 && visible() && open.get(),
                      "first outside request overrode controlled authority");
                accept = true;
                outside(f);
                std::cout << "outside_retry direct=" << direct_popover << " throw=" << throw_first << " cycle=" << cycle
                          << " closes=" << closes << " visible=" << visible() << '\n';
                check(closes == cycle * 2 + 2 && !visible() && !open.get(),
                      "controlled outside close could not retry after rejection");
                outside(f);
                check(closes == cycle * 2 + 2, "closed popup emitted another outside close");
                if (cycle == 0) {
                    open.set(true);
                    sync(f);
                    check(visible(), "explicit reopen did not restore controlled popup");
                }
            }
            open.set(true);
            sync(f);
            retire = true;
            outside(f);
            check(!f.services.components().contains(original), "close callback could not retire original generation");
            clean(f);
        }
    }
}

void disabled_entry() {
    for (int scenario = 0; scenario < 5; ++scenario) {
        Fixture f;
        TreeSelectRef ref;
        Signal<bool> disabled_a{scenario == 0 || scenario == 1 || scenario == 3};
        Signal<bool> disabled_b{scenario == 2 || scenario == 3};
        f.services.mount(Content{[&] {
            auto props = TreeSelectProps{}.ref(ref);
            if (scenario == 1) {
                props.defaultValue(u8"a");
            } else if (scenario == 2) {
                props.defaultValue(u8"b");
            }
            TreeSelect(props, {TreeSelectItem{u8"a", u8"Alpha"}.disabled(disabled_a).visible(scenario != 4),
                               TreeSelectItem{u8"b", u8"Beta"}.disabled(disabled_b)});
        }});
        sync(f);
        const auto id = f.services.tree_select().mounted().front();
        const auto s = f.services.tree_select().snapshot(id);
        check(ref.focus(), "disabled entry selector ref failed");
        key(f, Key::down);
        const auto rows = f.services.tree().snapshot(s.tree).rows;
        if (scenario == 3) {
            check(f.services.focus().state().focused == f.inputs.mounted_inputs().front().interaction,
                  "all-disabled tree should retain selector focus");
            disabled_b.set(false);
            sync(f);
            key(f, Key::down);
        }
        const std::size_t expected = scenario == 2 ? 0 : 1;
        const auto focused = f.services.focus().state();
        std::cout << "disabled_entry scenario=" << scenario
                  << " selector=" << (focused.focused == f.inputs.mounted_inputs().front().interaction) << '\n';
        check(focused.focused == rows[expected].interaction && focused.focus_visible,
              "keyboard open did not skip disabled/currently disabled row");
        check(f.services.tree_select().snapshot(id).open, "disabled entry navigation closed popup");
        clean(f);
    }
}
} // namespace

int main(int argc, char** argv) {
    try {
        const std::string_view name = argc > 1 ? argv[1] : "outside_retry";
        if (name == "outside_retry") {
            outside_retry();
        } else if (name == "disabled_entry") {
            disabled_entry();
        } else {
            throw std::runtime_error("unknown TreeSelect review case");
        }
        std::cout << name << " passed\n";
        return 0;
    } catch (const std::exception& e) {
        std::cerr << e.what() << '\n';
        return 1;
    }
}
