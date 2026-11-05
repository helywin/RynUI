#include "gallery_sample_registry.hpp"
#include "gallery_sample_source.hpp"
#include "gallery_test_inventory.hpp"
#include "reference_surface.hpp"
#include "component/calendar_component.hpp"
#include "support/input_fixture.hpp"
#include <iostream>

int main() {
    try {
        using namespace ryn;
        using namespace rynui::example;
        ryn_test::input_component::Fixture f;
        ReferenceSurfaceHost surfaces{f.buttons};
        GalleryPageModel model;
        const auto entry = model.find("ant.component.calendar");
        if (!entry || !entry->native_scope()) {
            throw std::runtime_error("Calendar page missing");
        }
        const auto route = entry->route();
        auto page = make_gallery_sample_page(route, {});
        page.set_width(720);
        surfaces.mount(page.content, &f.inputs);
        if (!f.services.layout_and_synchronize({900, 2000}, {0, 0, 900, 2000}, {}, 0, true)) {
            return 1;
        }
        if (page.sample_runs("gallery.sample.calendar.schedule") != 1 ||
            !gallery_sample_source("gallery.sample.calendar.schedule") ||
            !gallery_sample_source("gallery.sample.calendar.custom") || gallery_legacy_test_ids().size() != 458 ||
            f.services.calendar().mounted().size() != 2) {
            throw std::runtime_error("Calendar Gallery/source/legacy");
        }
        const auto id = f.services.calendar().mounted().back();
        const auto original = f.services.calendar().snapshot(id);
        if (original.cells.size() != 42 || f.inputs.mounted_inputs().size() < 42) {
            throw std::runtime_error("Calendar rich native content missing");
        }
        f.services.dispose();
    } catch (const std::exception& e) {
        std::cerr << e.what() << '\n';
        return 1;
    }
}
