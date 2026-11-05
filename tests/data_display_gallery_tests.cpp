#include "support/data_display_fixture.hpp"
#include "ant_design_reference_catalog.hpp"
#include "gallery_sample_registry.hpp"
#include "gallery_sample_source.hpp"
#include "gallery_test_inventory.hpp"
#include "reference_surface.hpp"
#include "component/selection_component.hpp"
#include <string_view>

namespace {
using namespace data_display_test;
using namespace rynui::example;

struct GalleryFixture final : Fixture {
    ReferenceSurfaceHost surfaces{buttons};
    ryn::detail::SelectionComponentHost selections{services};
    GallerySamplePage page;

    void update() {
        check(buttons.layout_and_synchronize({1000, 4000}, {0, 0, 1000, 4000}, {}, 0, true),
              "DataDisplay Gallery layout failed");
    }

    void mount(std::string_view name) {
        GalleryPageModel model;
        page = make_gallery_sample_page(model.find(name)->route(), {});
        page.set_width(760);
        surfaces.mount(page.content, &inputs);
        update();
    }

    void action(std::string_view target) {
        const auto component = page.target(target);
        check(component.valid(), "Gallery action target missing");
        for (auto hit : services.interactions().declaration_order()) {
            if (services.interactions().require(hit).component == component) {
                check(services.focus().state().focused == hit ||
                          services.focus().request_focus(hit, FocusModality::keyboard),
                      "Gallery action focus rejected");
                services.focus().dispatch({Key::enter, KeyAction::down});
                services.focus().dispatch({Key::enter, KeyAction::up});
                update();
                return;
            }
        }
        throw std::runtime_error("Gallery action is not a real native control");
    }

    void cleanup() {
        services.dispose();
        check(services.components().component_count() == 0 && inputs.mounted_inputs().empty() &&
                  services.interactions().size() == 0 && services.surfaces().size() == 0,
              "Gallery page retirement leaked resources");
    }
};

void run(std::string_view name) {
    GalleryFixture f;
    const auto identity = std::string{"ant.component."} + std::string{name};
    const auto* support = find_ant_design_reference_entry(identity);
    check(support && support->support_status == GallerySupportStatus::implemented &&
              support->evidence_identifiers.find("openspec:107-") != std::string_view::npos &&
              support->evidence_identifiers.find(std::string{"test:rynui."} + std::string{name} + "_contract") !=
                  std::string_view::npos,
          "completed DataDisplay page lost its implementation evidence");
    f.mount(identity);
    if (name == "collapse") {
        const auto id = f.page.target("gallery.collapse.dynamic");
        const auto original = f.services.collapse().snapshot(id);
        f.action("gallery.collapse.reorder");
        check(f.services.collapse().snapshot(id).panels[1].body == original.panels[0].body,
              "Gallery Collapse reorder remounted body");
        f.action("gallery.collapse.dynamic-toggle");
        f.action("gallery.collapse.dynamic-toggle");
        f.action("gallery.collapse.ghost");
    } else if (name == "segmented") {
        const auto id = f.page.target("gallery.segmented.dynamic");
        const auto original = f.services.segmented().snapshot(id);
        click(f, original.items[1]);
        f.update();
        check(f.services.segmented().snapshot(id).selected_value == SegmentedValue{2.0},
              "Gallery Segmented callback did not update typed authority");
        f.action("gallery.segmented.reorder");
        f.action("gallery.segmented.rtl");
        check(f.services.segmented().snapshot(id).items[0] == original.items[1], "Gallery Segmented lost keyed item");
    } else if (name == "descriptions") {
        const auto id = f.page.target("gallery.descriptions.dynamic");
        const auto original = f.services.descriptions().snapshot(id);
        f.action("gallery.descriptions.reorder");
        f.action("gallery.descriptions.rtl");
        check(f.services.descriptions().snapshot(id).items[2].content == original.items[0].content,
              "Gallery Descriptions lost keyed cell");
        check(f.services.layout_and_synchronize({480, 4000}, {0, 0, 480, 4000}, {}, 0, true),
              "Gallery breakpoint resize failed");
        check(f.services.descriptions().snapshot(id).rows.size() == 3,
              "Gallery Descriptions did not use native window breakpoints");
    } else if (name == "timeline") {
        const auto id = f.page.target("gallery.timeline.dynamic");
        const auto original = f.services.timeline().snapshot(id);
        check(original.items[0].tail.width > original.items[0].tail.height, "Gallery Timeline is not horizontal");
        f.action("gallery.timeline.reorder");
        f.action("gallery.timeline.orientation");
        f.action("gallery.timeline.title-span");
        f.action("gallery.timeline.tone-motion");
        f.action("gallery.timeline.loading-motion");
        f.action("gallery.timeline.loading-motion");
        check(f.services.timeline().snapshot(id).items[1].content == original.items[1].content,
              "Gallery Timeline remounted keyed content");
    } else {
        throw std::runtime_error("Unknown DataDisplay page");
    }
    check(gallery_samples().size() == 97 && gallery_legacy_test_ids().size() == 458,
          "existing sample or legacy inventory changed");
    for (const auto& sample : gallery_samples()) {
        check(gallery_sample_source(sample.identity), "registered sample lacks generated real source");
    }
    f.cleanup();
}
} // namespace

int main(int argc, char** argv) {
    try {
        check(argc == 2, "Choose one DataDisplay page");
        run(argv[1]);
        std::cout << argv[1] << " Gallery actions passed; 91 samples and 458 legacy IDs retained\n";
    } catch (const std::exception& e) {
        std::cerr << e.what() << '\n';
        return 1;
    }
}
