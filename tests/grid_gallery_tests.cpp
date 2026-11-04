#include "component/grid_component.hpp"
#include "gallery_page_model.hpp"
#include "gallery_sample_registry.hpp"
#include "reference_surface.hpp"
#include "support/input_fixture.hpp"

#include <iostream>
#include <stdexcept>

int main() {
    try {
        using namespace ryn;
        using namespace rynui::example;
        ryn_test::input_component::Fixture f;
        ReferenceSurfaceHost surfaces{f.buttons};
        GalleryPageModel model;
        const auto* entry = model.find("ant.component.grid");
        if (!entry || !entry->native_scope()) {
            throw std::runtime_error("Grid page is not native");
        }
        auto page = make_gallery_sample_page(entry->route(), {});
        page.set_width(760);
        surfaces.mount(page.content, &f.inputs);
        if (!f.buttons.layout_and_synchronize({1000, 900}, {0, 0, 1000, 900}, {}, 0, true)) {
            throw std::runtime_error("Grid Gallery layout failed");
        }
        const auto row = page.target("gallery.grid.responsive");
        const auto wide = ryn::detail::grid_columns(f.services.components(), row);
        if (wide.size() != 4 || wide[0].options.span != 6 || !page.target("gallery.grid.offset").valid() ||
            !page.target("gallery.grid.flex").valid() || !page.target("gallery.grid.rtl").valid()) {
            throw std::runtime_error("Grid independent page omitted real component examples");
        }
        if (!f.buttons.layout_and_synchronize({500, 900}, {0, 0, 500, 900}, {}, 0, true)) {
            throw std::runtime_error("Grid narrow Gallery layout failed");
        }
        const auto narrow = ryn::detail::grid_columns(f.services.components(), row);
        if (narrow[0].component != wide[0].component || narrow[0].options.span != 24 ||
            page.sample_runs("gallery.sample.grid.layouts") != 1 ||
            page.sample_runs("gallery.sample.flex.layouts") != 0) {
            throw std::runtime_error("Grid page reflow remounted content or ran an unrelated factory");
        }
    } catch (const std::exception& error) {
        std::cerr << error.what() << '\n';
        return 1;
    }
    return 0;
}
