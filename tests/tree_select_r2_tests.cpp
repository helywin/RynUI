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
    check(!f.services.pointer().dispatch_token(), "pointer token escaped native dispatch");
    sync(f);
}

void clean(Fixture& f) {
    f.services.dispose();
    check(f.nodes.size() == 0 && f.scene.size() == 0 && f.inputs.editors().size() == 0 &&
              f.services.interactions().size() == 0 && f.services.surfaces().size() == 0 &&
              blocked_roots(f.services.interactions()).empty() && !f.services.next_frame_deadline(),
          "TreeSelect R2 cleanup leaked resources");
}

void focused_outside() {
    for (bool tree_focus : {false, true}) {
        for (bool throw_first : {false, true}) {
            Fixture f;
            Signal<bool> open{true};
            TreeSelectRef ref;
            InputRef after;
            int closes = 0;
            bool accept = false;
            bool throw_pending = throw_first;
            std::vector<std::optional<std::uint64_t>> deliveries;
            f.services.mount(Content{[&] {
                TreeSelect(TreeSelectProps{}.open(open).ref(ref).onOpenChange([&](bool value) {
                    check(!value, "unexpected opening intent");
                    ++closes;
                    deliveries.push_back(f.services.pointer().dispatch_token());
                    if (throw_pending) {
                        throw_pending = false;
                        throw std::runtime_error("focused close failure");
                    }
                    if (accept || closes == 2) {
                        open.set(false);
                    }
                }),
                           {TreeSelectItem{u8"a", u8"Alpha"}});
                Input(InputProps{}.ref(after).layout(LayoutStyle{}.margin_top(dp(400))));
            }});
            sync(f);
            const auto id = f.services.tree_select().mounted().front();
            check(ref.focus(), "focused outside fixture could not focus selector");
            if (tree_focus) {
                key(f, Key::down);
                check(f.services.focus().state().focused ==
                          f.services.tree().snapshot(f.services.tree_select().snapshot(id).tree).rows[0].interaction,
                      "focused outside fixture did not enter Tree");
            }
            bool caught = false;
            try {
                outside(f);
            } catch (const std::runtime_error& error) {
                check(throw_first && std::string_view{error.what()} == "focused close failure",
                      "focused close replaced original exception");
                caught = true;
                check(!f.services.pointer().dispatch_token(), "pointer token survived callback exception");
                f.services.pointer().dispatch(
                    {PointerIdentity::mouse(), PointerAction::up, PointerButton::primary, 550, 700});
                sync(f);
            }
            check(caught == throw_first, "focused callback exception was swallowed");
            std::cout << "focused_outside tree=" << tree_focus << " throw=" << throw_first << " first_closes=" << closes
                      << " visible=" << f.services.tree_select().snapshot(id).open << '\n';
            check(closes == 1 && f.services.tree_select().snapshot(id).open && open.get(),
                  "one focused outside click delivered duplicate close requests");
            accept = true;
            outside(f);
            check(closes == 2 && !f.services.tree_select().snapshot(id).open,
                  "focused outside retry did not deliver next click");
            check(deliveries.size() == 2 && deliveries[0] && deliveries[1] && deliveries[0] != deliveries[1],
                  "separate native clicks shared a close token");

            closes = 0;
            accept = false;
            deliveries.clear();
            open.set(true);
            sync(f);
            check(ref.focus(), "Tab fixture could not focus selector");
            key(f, Key::down);
            key(f, Key::tab);
            check(closes == 1 && f.services.tree_select().snapshot(id).open &&
                      f.services.focus().state().focused == f.inputs.mounted_inputs().back().interaction &&
                      deliveries.size() == 1 && !deliveries.front(),
                  "controlled Tab exit duplicated close or failed adjacent traversal");
            accept = true;
            outside(f);
            check(closes == 2 && !f.services.tree_select().snapshot(id).open,
                  "rejected Tab exit prevented later pointer retry");

            closes = 0;
            accept = false;
            deliveries.clear();
            open.set(true);
            sync(f);
            check(ref.focus() && after.focus(), "programmatic focus transition failed");
            check(closes == 1 && f.services.tree_select().snapshot(id).open && deliveries.size() == 1 &&
                      !deliveries.front(),
                  "programmatic focus exit was deferred or inherited a pointer token");
            check(ref.focus() && after.focus(), "second programmatic focus transition failed");
            check(closes == 2 && !f.services.tree_select().snapshot(id).open && !deliveries.back(),
                  "separate programmatic exits were incorrectly coalesced");
            clean(f);
        }
    }
}

void suppressed_close() {
    for (bool compound : {false, true}) {
        Fixture f;
        Signal<bool> open{true};
        Signal<bool> disabled{false};
        int closes = 0;
        const auto change = [&](bool value) {
            check(!value, "suppression delivered opening intent");
            ++closes;
            disabled.set(true);
        };
        f.services.mount(Content{[&] {
            if (compound) {
                TreeSelect(TreeSelectProps{}.open(open).disabled(disabled).onOpenChange(change),
                           {TreeSelectItem{u8"a", u8"Alpha"}});
            } else {
                Popover(PopoverProps{}
                            .open(open)
                            .disabled(disabled)
                            .trigger(PopoverTriggerMode::Manual)
                            .onOpenChange(change),
                        PopoverTrigger{[] { Button(ButtonProps{}, ButtonContent{[] { Text(u8"Trigger"); }}); }},
                        PopoverSlots{.content = PopoverContent{[] { Text(u8"Content"); }}});
            }
        }});
        sync(f);
        const auto visible = [&] {
            return compound ? f.services.tree_select().snapshot(f.services.tree_select().mounted().front()).open
                            : f.services.popover().snapshot(f.services.popover().mounted().front()).geometry.visible;
        };
        outside(f);
        std::cout << "suppressed_close compound=" << compound << " closes=" << closes << " visible=" << visible()
                  << '\n';
        check(closes == 1 && !visible() && disabled.get() && open.get(),
              "callback suppression reactivated an already delivered close request");
        sync(f);
        outside(f);
        check(closes == 1 && !visible(), "suppressed popup repeated close on later observations");
        disabled.set(false);
        sync(f);
        check(!visible(), "disabled recovery resurrected suppressed popup");
        open.set(false);
        open.set(true);
        sync(f);
        check(visible(), "explicit authority reopen did not recover suppressed popup");
        outside(f);
        check(closes == 2 && !visible(), "fresh popup did not emit exactly one suppression request");
        clean(f);
    }
}
} // namespace

int main(int argc, char** argv) {
    try {
        const std::string_view name = argc > 1 ? argv[1] : "focused_outside";
        if (name == "focused_outside") {
            focused_outside();
        } else if (name == "suppressed_close") {
            suppressed_close();
        } else {
            throw std::runtime_error("unknown TreeSelect R2 case");
        }
        std::cout << name << " passed\n";
        return 0;
    } catch (const std::exception& e) {
        std::cerr << e.what() << '\n';
        return 1;
    }
}
