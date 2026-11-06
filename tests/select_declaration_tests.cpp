#include "component/select_component.hpp"
#include "support/input_fixture.hpp"
#include <iostream>
using namespace ryn;

namespace {
int checks{};

void check(bool value, const char* message) {
    ++checks;
    if (!value) {
        throw std::runtime_error(message);
    }
}

void run(bool next_boundary) {
    ryn_test::input_component::Fixture f;
    Signal<bool> loading{true};
    Signal<bool> open{true};
    int closes = 0;
    runtime::ComponentId select;
    f.services.mount(Content{[&] {
        Skeleton(SkeletonProps{}.loading(loading), SkeletonContent{[&] {
                     Select(SelectProps{}.options({{1, u8"One"}}).open(open).onOpenChange([&](bool value) {
                         if (!value) {
                             ++closes;
                         }
                     }));
                     select = f.services.select().mounted().back();
                     check(closes == 0, "Select internal mount gate delivered close before returning to user body");
                     if (next_boundary) {
                         Skeleton(SkeletonProps{}.loading(true), SkeletonContent{[] { Text(u8"trigger"); }});
                         check(closes == 1,
                               "external declaration boundary did not immediately deliver pending Select close");
                     }
                 }});
    }});
    f.synchronize();
    check(closes == 1 && !f.services.select().snapshot(select).visible,
          "inactive initial Select lost or repeated close intent");
    f.synchronize();
    check(closes == 1, "settled Select repeated suppression close");
    loading.set(false);
    f.synchronize();
    check(!f.services.select().snapshot(select).visible && closes == 1,
          "ancestor recovery reopened suppressed controlled Select");
    open.set(false);
    open.set(true);
    f.synchronize();
    check(f.services.select().snapshot(select).visible, "explicit controlled reopen failed after initial suppression");
    loading.set(true);
    f.synchronize();
    check(closes == 2 && !f.services.select().snapshot(select).visible,
          "later ancestor gate lost exactly-once close behavior");
    f.services.dispose();
    check(f.services.surfaces().size() == 0 && f.services.interactions().declaration_order().empty(),
          "Select declaration lifecycle leaked resources");
}
} // namespace

int main() {
    try {
        run(false);
        run(true);
        std::cout << checks << " Select declaration checks passed\n";
        return 0;
    } catch (const std::exception& e) {
        std::cerr << e.what() << '\n';
        return 1;
    }
}
