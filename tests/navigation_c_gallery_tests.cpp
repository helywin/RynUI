#include "component/pagination_component.hpp"
#include "component/steps_component.hpp"
#include "component/tabs_component.hpp"
#include "gallery_page_model.hpp"
#include "gallery_sample_registry.hpp"
#include "reference_surface.hpp"
#include "support/input_fixture.hpp"

#include <iostream>

int main(int argc, char** argv) {
    try {
        using namespace ryn;
        using namespace rynui::example;
        if (argc != 2) {
            throw std::runtime_error("expected a component name");
        }
        const std::string name{argv[1]};
        ryn_test::input_component::Fixture f;
        ReferenceSurfaceHost surfaces{f.buttons};
        GalleryPageModel model;
        const auto* entry = model.find("ant.component." + name);
        if (!entry || !entry->native_scope()) {
            throw std::runtime_error("missing native component page");
        }
        auto page = make_gallery_sample_page(entry->route(), {});
        page.set_width(760);
        surfaces.mount(page.content, &f.inputs);
        const auto sync = [&] {
            for (int frame = 0; frame < 3; ++frame) {
                if (!f.buttons.layout_and_synchronize({1000, 900}, {0, 0, 1000, 900}, {}, 0, true)) {
                    throw std::runtime_error("independent page layout failed");
                }
            }
        };
        sync();
        if (name == "pagination") {
            const auto mounted = f.services.pagination().mounted();
            if (mounted.size() != 4 || !f.services.pagination().snapshot(mounted[2]).simple ||
                !f.services.pagination().snapshot(mounted[1]).size_changer.valid()) {
                throw std::runtime_error("Pagination page omitted extended controls");
            }
        } else if (name == "steps") {
            if (f.services.steps().mounted().size() < 6) {
                throw std::runtime_error("Steps page omitted dynamic/variant samples");
            }
        } else if (name == "tabs") {
            if (f.services.tabs().mounted().size() < 16) {
                throw std::runtime_error("Tabs page omitted editable/vertical samples");
            }
        } else {
            throw std::runtime_error("unknown component");
        }
        page.set_width(420);
        sync();
        const auto count = f.services.components().component_count();
        sync();
        if (f.services.components().component_count() != count) {
            throw std::runtime_error("page reflow grew retained content unexpectedly");
        }
        f.services.dispose();
        if (f.nodes.size() != 0 || f.scene.size() != 0 || f.services.interactions().size() != 0 ||
            f.services.animations().diagnostics().active != 0) {
            throw std::runtime_error("page left live resources after teardown");
        }
        std::cout << name << " independent Gallery page passed\n";
    } catch (const std::exception& error) {
        std::cerr << error.what() << '\n';
        return 1;
    }
}
