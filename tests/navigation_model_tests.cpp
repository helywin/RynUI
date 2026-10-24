#include "component/navigation_model.hpp"
#include <limits>
#include <iostream>

using namespace ryn;
using namespace ryn::detail;

void check(bool value) {
    if (!value) {
        throw std::runtime_error("Navigation model contract failed");
    }
}

int main() {
    BreadcrumbModel b;
    check(!b.current() && !b.eligible(0));
    b.items = {{u8"a"}, {u8"hidden", false, false}, {u8"b"}};
    validate_navigation_items(b.items);
    check(b.current() == 2 && b.eligible(0) && !b.eligible(1) && !b.eligible(2));
    b.items[2].visible = false;
    check(b.current() == 0 && !b.eligible(0));
    b.items[0].visible = false;
    check(!b.current());
    for (auto invalid :
         {std::vector<NavigationItemModel>{{u8""}}, std::vector<NavigationItemModel>{{u8"a"}, {u8"a"}}}) {
        bool rejected = false;
        try {
            validate_navigation_items(invalid);
        } catch (const std::invalid_argument&) {
            rejected = true;
        }
        check(rejected);
    }
    StepsModel s{{{u8"a"}, {u8"hidden", false, false}, {u8"b"}}, 0, true, true};
    check(s.status(0) == StepsStatus::Process && s.status(2) == StepsStatus::Wait);
    check(s.request(2) && s.current == 0 && s.request(2));
    check(!s.request(1));
    s.items[2].disabled = true;
    check(!s.request(2));
    s.items[2].disabled = false;
    s.controlled = false;
    check(s.request(2) && s.current == 2 && s.status(0) == StepsStatus::Finish);
    s.current_status = StepsStatus::Error;
    check(s.status(2) == StepsStatus::Error);
    s.items[0].status = StepsStatus::Wait;
    check(s.status(0) == StepsStatus::Wait);
    s.current = std::numeric_limits<std::size_t>::max();
    check(s.status(1) == StepsStatus::Finish && s.status(2) == StepsStatus::Finish);
    s.interactive = false;
    check(!s.request(0));
    BreadcrumbItem{u8"a", u8"A"}.richTitle(BreadcrumbTitle{[] {}}).icon(BreadcrumbIcon{[] {}}).visible(true);
    BreadcrumbProps{}.separator(BreadcrumbSeparator{[] {}}).onActivate([](BreadcrumbActivation) {});
    StepsItem{u8"a", u8"A"}
        .richTitle(StepsTitle{[] {}})
        .richSubtitle(StepsSubtitle{[] {}})
        .richDescription(StepsDescription{[] {}})
        .icon(StepsIcon{[] {}})
        .status(StepsStatus::Error);
    StepsProps{}.current(std::size_t{0}).size(StepsSize::Small).onChange([](std::size_t) {});
    std::cout << "navigation model/public API contracts passed\n";
}
