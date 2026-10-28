#include "component/tree_select_component.hpp"
#include "component/tree_component.hpp"
#include "support/timeline_fixture.hpp"
#include <string_view>

namespace {
using namespace timeline_test;

void idle(Fixture& f) {
    sync(f);
    static_cast<void>(f.frames.consume_request());
    check(!f.frames.pending() && !f.services.next_frame_deadline(), "TreeSelect fixture did not settle");
}

void clean(Fixture& f) {
    f.services.dispose();
    check(f.nodes.size() == 0 && f.scene.size() == 0 && f.inputs.editors().size() == 0 &&
              f.services.interactions().size() == 0 && f.services.surfaces().size() == 0 &&
              blocked_roots(f.services.interactions()).empty() && !f.services.next_frame_deadline(),
          "TreeSelect R3 cleanup leaked resources");
}

void keyboard_wake() {
    for (auto entry : {Key::down, Key::up, Key::enter, Key::space}) {
        Fixture f;
        TreeSelectRef ref;
        f.services.mount(
            Content{[&] { TreeSelect(TreeSelectProps{}.defaultOpen().ref(ref), {TreeSelectItem{u8"a", u8"Alpha"}}); }});
        sync(f);
        check(ref.focus(), "idle keyboard fixture could not focus selector");
        idle(f);
        const auto id = f.services.tree_select().mounted().front();
        const auto selector = f.services.focus().state();
        check(selector.focus_visible, "idle keyboard fixture was not already in keyboard modality");
        f.services.focus().dispatch({entry, KeyAction::down});
        f.services.focus().dispatch({entry, KeyAction::up});
        std::cout << "keyboard_wake key=" << static_cast<int>(entry) << " pending=" << f.frames.pending() << '\n';
        check(f.frames.pending(), "already-open keyboard entry did not request a frame");
        // Honor the frame request before running geometry; never synchronize unconditionally after the key.
        check(f.frames.consume_request(), "keyboard entry wake disappeared before submission");
        sync(f);
        check(f.services.focus().state().focused ==
                  f.services.tree().snapshot(f.services.tree_select().snapshot(id).tree).rows[0].interaction,
              "requested frame did not enter the eligible Tree row");
        idle(f);
        sync(f);
        check(!f.frames.pending() && !f.services.next_frame_deadline(),
              "completed keyboard entry did not return to idle");
        clean(f);
    }
}

void cancelled_entry() {
    for (int mode = 0; mode < 4; ++mode) {
        Fixture f;
        Signal<bool> open{false};
        TreeSelectRef ref;
        int opens = 0;
        int blurs = 0;
        f.services.mount(Content{[&] {
            TreeSelect(TreeSelectProps{}.open(open).ref(ref).onBlur([&] { ++blurs; }).onOpenChange([&](bool value) {
                if (value) {
                    ++opens;
                    if (mode != 2) {
                        open.set(true);
                    }
                    if (mode == 0) {
                        open.set(false);
                    } else if (mode == 3) {
                        throw std::runtime_error("opening callback failure");
                    }
                }
            }),
                       {TreeSelectItem{u8"a", u8"Alpha"}});
        }});
        sync(f);
        check(ref.focus(), "cancelled entry fixture could not focus selector");
        idle(f);
        const auto selector = f.services.focus().state().focused;
        const auto id = f.services.tree_select().mounted().front();
        bool caught = false;
        try {
            f.services.focus().dispatch({Key::down, KeyAction::down});
        } catch (const std::runtime_error& error) {
            check(mode == 3 && std::string_view{error.what()} == "opening callback failure",
                  "opening replaced original callback failure");
            caught = true;
        }
        check(caught == (mode == 3), "opening callback failure was swallowed");
        f.services.focus().dispatch({Key::down, KeyAction::up});
        if (mode == 1) {
            open.set(false);
        }
        sync(f);
        check(opens == 1 && f.services.tree_select().snapshot(id).open == (mode == 3) && blurs == 0,
              "controlled callback did not cancel opening before Tree focus");
        open.set(true);
        sync(f);
        std::cout << "cancelled_entry mode=" << mode
                  << " retained_selector=" << (f.services.focus().state().focused == selector) << " blurs=" << blurs
                  << '\n';
        check(f.services.tree_select().snapshot(id).open && f.services.focus().state().focused == selector &&
                  blurs == 0,
              "programmatic reopen revived cancelled keyboard entry");
        clean(f);
    }
}
} // namespace

int main(int argc, char** argv) {
    try {
        const std::string_view name = argc > 1 ? argv[1] : "keyboard_wake";
        if (name == "keyboard_wake") {
            keyboard_wake();
        } else if (name == "cancelled_entry") {
            cancelled_entry();
        } else {
            throw std::runtime_error("unknown TreeSelect R3 case");
        }
        std::cout << name << " passed\n";
        return 0;
    } catch (const std::exception& e) {
        std::cerr << e.what() << '\n';
        return 1;
    }
}
