#include "gallery_sample_registry.hpp"
#include "gallery_sample_source.hpp"
#include "reference_surface.hpp"
#include "component/date_picker_component.hpp"
#include "component/time_picker_component.hpp"
#include "support/navigation_fixture.hpp"

namespace {
using namespace ryn_test::navigation;
using namespace rynui::example;

struct GalleryFixture final : Fixture {
    ReferenceSurfaceHost surfaces{buttons};
    GallerySamplePage page;

    void update() {
        check(buttons.layout_and_synchronize({900, 1800}, {0, 0, 900, 1800}, {}, 0, true), "picker Gallery layout");
    }

    void mount(std::string_view identity) {
        GalleryPageModel model;
        page = make_gallery_sample_page(model.find(identity)->route(), {});
        page.set_width(760);
        surfaces.mount(page.content, &inputs);
        update();
        for (const auto& sample : gallery_samples()) {
            if (sample.component_identity == identity) {
                check(page.sample_runs(sample.identity) == 1 && gallery_sample_source(sample.identity),
                      "actual picker sample/source missing");
            }
        }
    }

    void action(std::string_view identity) {
        const auto target = page.target(identity);
        check(target.valid(), "picker Gallery action missing");
        for (const auto& button : buttons.mounted_buttons()) {
            if (descendant(*this, button.component, target)) {
                click(*this, button.component);
                update();
                return;
            }
        }
        throw std::runtime_error("picker Gallery action is not a real button");
    }

    void cleanup() {
        services.dispose();
        check(services.components().component_count() == 0 && services.interactions().size() == 0 &&
                  services.surfaces().size() == 0 && inputs.mounted_inputs().empty(),
              "picker Gallery leaked resources");
    }
};

void date_page() {
    GalleryFixture f;
    f.mount("ant.component.date-picker");
    check(f.services.date_picker().mounted().size() == 9 && f.services.time_picker().mounted().size() == 3,
          "DatePicker page missing native range/multiple/time composition");
    const auto original = f.services.date_picker().mounted().front();
    std::vector<runtime::ComponentId> initial;
    for (auto id : f.services.date_picker().mounted()) {
        initial.push_back(id);
    }
    f.action("gallery.date-picker.picker-mode");
    f.action("gallery.date-picker.locale");
    f.action("gallery.date-picker.rtl");
    f.action("gallery.date-picker.range-authority");
    f.page.set_width(520);
    f.update();
    check(f.services.date_picker().mounted().front() == original &&
              f.services.date_picker().mounted().size() == initial.size(),
          "DatePicker page remounted ordinary updates");
    bool week{};
    for (auto id : f.services.date_picker().mounted()) {
        week |= f.services.date_picker().snapshot(id).mode == DatePickerMode::Week;
    }
    check(week, "Gallery mode action did not update real picker");
    f.cleanup();
}

void time_page() {
    GalleryFixture f;
    f.mount("ant.component.time-picker");
    check(f.services.time_picker().mounted().size() == 5, "TimePicker page missing precise/range controls");
    const auto original = f.services.time_picker().mounted().front();
    f.action("gallery.time-picker.twelve");
    f.action("gallery.time-picker.scroll-select");
    f.action("gallery.time-picker.range-authority");
    f.page.set_width(520);
    f.update();
    check(f.services.time_picker().mounted().front() == original && f.services.time_picker().mounted().size() == 5,
          "TimePicker page remounted ordinary updates");
    f.cleanup();
}
} // namespace

int main(int argc, char** argv) {
    try {
        if (argc > 1 && std::string_view{argv[1]} == "time") {
            time_page();
        } else {
            date_page();
        }
        std::cout << "picker Gallery passed\n";
    } catch (const std::exception& e) {
        std::cerr << e.what() << '\n';
        return 1;
    }
}
