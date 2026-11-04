#include "component/masonry_component.hpp"
#include "gallery_page_model.hpp"
#include "gallery_sample_registry.hpp"
#include "reference_surface.hpp"
#include "support/input_fixture.hpp"

#include <algorithm>
#include <iostream>
#include <stdexcept>

int main() {
    try {
        using namespace ryn;
        using namespace rynui::example;
        ryn_test::input_component::Fixture f;
        ReferenceSurfaceHost surfaces{f.buttons};
        GalleryPageModel model;
        const auto* entry = model.find("ant.component.masonry");
        if (!entry || !entry->native_scope()) {
            throw std::runtime_error("Masonry page is not native");
        }
        auto page = make_gallery_sample_page(entry->route(), {});
        page.set_width(760);
        surfaces.mount(page.content, &f.inputs);
        const auto sync = [&](float width) {
            if (!f.buttons.layout_and_synchronize({width, 2400}, {0, 0, width, 2400}, {}, 0, true)) {
                throw std::runtime_error("Masonry Gallery layout failed");
            }
        };
        sync(1000);
        const auto owner = page.target("gallery.masonry.measured");
        const auto wide = f.services.masonry().snapshot(owner);
        const auto dynamic = page.target("gallery.masonry.dynamic");
        if (wide.size() != 6 || wide[2].column != 2 || !dynamic.valid() ||
            f.services.masonry().snapshot(dynamic).size() != 2) {
            throw std::runtime_error("Masonry independent page omitted real measured/keyed examples");
        }
        const auto initial = f.services.masonry().snapshot(dynamic);
        const auto click = [&](std::string_view identity) {
            const auto id = page.target(identity);
            const auto bounds = f.nodes.require(f.services.components().root(id)).bounds;
            for (const auto action : {input::PointerAction::down, input::PointerAction::up}) {
                f.services.pointer().dispatch({input::PointerIdentity::mouse(), action, input::PointerButton::primary,
                                               bounds.x + bounds.width / 2, bounds.y + bounds.height / 2});
            }
            sync(1000);
        };
        click("gallery.masonry.resize");
        const auto taller = f.services.masonry().snapshot(dynamic);
        if (taller[0].bounds.height <= initial[0].bounds.height || taller[0].component != initial[0].component) {
            throw std::runtime_error("Masonry Gallery resize button did not remeasure retained content");
        }
        click("gallery.masonry.toggle");
        click("gallery.masonry.reverse");
        const auto reordered = f.services.masonry().snapshot(dynamic);
        const auto kept = std::ranges::find_if(reordered, [&](const auto& item) { return item.key == initial[0].key; });
        if (reordered.size() != 3 || kept == reordered.end() || kept->component != initial[0].component) {
            throw std::runtime_error("Masonry Gallery insert/reorder button lost keyed identity");
        }
        click("gallery.masonry.toggle");
        const auto removed = f.services.masonry().snapshot(dynamic);
        if (removed.size() != 2 ||
            std::ranges::any_of(removed, [](const auto& item) { return item.key == MasonryKey{u8"added"}; })) {
            throw std::runtime_error("Masonry Gallery remove button retired the wrong key after reorder");
        }
        sync(500);
        const auto narrow = f.services.masonry().snapshot(owner);
        if (narrow[2].column != 0 || narrow[0].component != wide[0].component ||
            page.sample_runs("gallery.sample.masonry.layouts") != 1 ||
            page.sample_runs("gallery.sample.grid.layouts") != 0) {
            throw std::runtime_error("Masonry page resize remounted content or ran unrelated factories");
        }
    } catch (const std::exception& error) {
        std::cerr << error.what() << '\n';
        return 1;
    }
    return 0;
}
