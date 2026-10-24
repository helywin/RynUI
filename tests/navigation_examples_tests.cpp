#include <ryn/rynui.hpp>

void breadcrumb_example(ryn::Signal<ryn::String>& title, ryn::Signal<bool>& details_visible,
                        std::function<void(ryn::BreadcrumbActivation)> navigate) {
    using namespace ryn;
    Breadcrumb(BreadcrumbProps{}.onActivate(std::move(navigate)).separator(BreadcrumbSeparator{[] { Text(u8">"); }}),
               {BreadcrumbItem{u8"home", u8"Home"}.icon(
                    BreadcrumbIcon{[] { Icon(IconProps{}.name(IconName::HomeOutlined)); }}),
                BreadcrumbItem{u8"list", u8"Records"}.title(title),
                BreadcrumbItem{u8"detail", u8"Details"}.visible(details_visible)});
}

#include <ryn/rynui.hpp>

void steps_example(ryn::Signal<std::size_t>& current, ryn::Signal<bool>& review_visible) {
    using namespace ryn;
    Steps(StepsProps{}.current(current).size(StepsSize::Small).onChange([&current](std::size_t next) {
        current.set(next);
    }),
          {StepsItem{u8"details", u8"Details"}.description(u8"Enter details"),
           StepsItem{u8"review", u8"Review"}.subtitle(u8"Optional").visible(review_visible).icon(StepsIcon{[] {
               Icon(IconProps{}.name(IconName::SearchOutlined));
           }}),
           StepsItem{u8"complete", u8"Complete"}.richDescription(StepsDescription{[] { Text(u8"Ready to submit"); }})});
}

#include "component/breadcrumb_component.hpp"
#include "component/steps_component.hpp"
#include "support/navigation_fixture.hpp"
#include <iostream>

int main() {
    using namespace ryn_test::navigation;
    try {
        Fixture f;
        Signal<String> title{String{u8"Records"}};
        Signal<bool> visible{true};
        Signal<std::size_t> current{0};
        int activations = 0;
        f.services.mount(Content{[&] {
            breadcrumb_example(title, visible, [&](BreadcrumbActivation) { ++activations; });
            steps_example(current, visible);
        }});
        sync(f);
        const auto breadcrumb = f.services.breadcrumb().mounted()[0];
        const auto steps = f.services.steps().mounted()[0];
        click(f, f.services.breadcrumb().snapshot(breadcrumb).items[0]);
        click(f, f.services.steps().snapshot(steps).items[2]);
        check(activations == 1 && current.get() == 2, "documented examples did not navigate");
        title.set(String{u8"Updated records"});
        visible.set(false);
        sync(f);
        check(f.services.breadcrumb().snapshot(breadcrumb).current == 1 &&
                  f.services.steps().snapshot(steps).current == 2,
              "documented reactive examples failed");
        Fixture empty;
        empty.services.mount(Content{[] {
            Flex(FlexProps{}.vertical(true).gap(dp(23)), FlexContent{[] {
                     Breadcrumb({}, {});
                     Steps({}, {});
                     Text(u8"Only content");
                 }});
        }});
        sync(empty);
        const auto only = bounds(empty, find_text(empty, String{u8"Only content"}.view()).component);
        check(only.y == 0 && empty.services.interactions().size() == 0,
              "empty navigation hosts retained ordinary flow gaps/interactions");
        std::cout << "Documented Breadcrumb/Steps examples and empty hosts passed\n";
        return 0;
    } catch (const std::exception& e) {
        std::cerr << e.what() << '\n';
        return 1;
    }
}
