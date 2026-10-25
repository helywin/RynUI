#include "support/popover_fixture.hpp"
#include "../examples/native_popover.hpp"

int main() {
    using namespace ryn_test::popover;
    try {
        auto fixture = std::make_unique<Fixture>();
        auto& f = *fixture;
        Signal<bool> open{false};
        InputRef draft;
        int saves{};
        f.services.mount(Content{[&] { native_popover_example(open, draft, [&] { ++saves; }); }});
        sync(f);
        const auto owner = f.services.popover().mounted()[0];
        click(f, trigger(f, owner));
        check(open.get() && draft.focus(), "example did not open real editor");
        const auto popup = f.services.popover().snapshot(owner).popup;
        for (const auto& button : f.buttons.mounted_buttons()) {
            if (descendant(f, button.component, popup)) {
                click(f, button.component);
                break;
            }
        }
        check(saves == 1, "example Save button did not activate");
        key(f, Key::escape);
        check(!open.get(), "example controlled close did not reconcile");
    } catch (const std::exception& e) {
        std::cerr << e.what() << '\n';
        return 1;
    }
}
