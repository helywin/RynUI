#include "gallery_sample_registry.hpp"
#include "gallery_sample_source.hpp"
#include "reference_surface.hpp"
#include "component/anchor_component.hpp"
#include "component/breadcrumb_component.hpp"
#include "component/dropdown_component.hpp"
#include "support/navigation_fixture.hpp"
#include <iostream>

namespace {
using namespace ryn_test::navigation;
using namespace rynui::example;

struct GalleryFixture final : Fixture {
    ReferenceSurfaceHost surfaces{buttons};
    GallerySamplePage page;

    void update() {
        check(buttons.layout_and_synchronize({900, 1800}, {0, 0, 900, 1800}, {}, 0, true),
              "navigation Gallery layout failed");
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
                      "navigation Gallery factory/source missing");
            }
        }
    }

    void action(std::string_view label) {
        for (const auto& t : services.text().mounted_texts()) {
            if (scene.text_state(t.scene).content().bytes().find(label) == std::string_view::npos) {
                continue;
            }
            for (const auto& b : buttons.mounted_buttons()) {
                if (descendant(*this, t.component, b.component)) {
                    click(*this, b.component);
                    update();
                    return;
                }
            }
        }
        throw std::runtime_error("navigation Gallery control missing");
    }

    void cleanup() {
        services.dispose();
        check(services.components().component_count() == 0 && services.interactions().size() == 0 &&
                  services.surfaces().size() == 0 && inputs.mounted_inputs().empty(),
              "navigation Gallery leaked resources");
    }
};

void anchor_page() {
    GalleryFixture f;
    f.mount("ant.component.anchor");
    check(f.services.anchor().mounted().size() == 1, "Anchor page has no real Anchor");
    const auto id = f.services.anchor().mounted()[0];
    const auto initial = f.services.anchor().snapshot(id);
    check(initial.keys.size() == 5 && initial.active == String{u8"overview"}, "Anchor page initial projection wrong");
    runtime::ComponentId api_link;
    for (const auto& b : f.buttons.mounted_buttons()) {
        if (descendant(f, b.component, initial.items[2])) {
            api_link = b.component;
        }
    }
    check(api_link.valid(), "Gallery Anchor API link missing");
    click(f, api_link);
    f.update();
    const auto& viewport = f.nodes.require(f.services.components().root(f.page.target("gallery.anchor.viewport")));
    check(!viewport.children.empty() &&
              std::abs(viewport.bounds.y - f.nodes.require(viewport.children.front()).bounds.y - 480) < .01F,
          "Gallery Anchor callback did not scroll real content");
    f.action("gallery.anchor.direction");
    check(!f.services.components().branch_active(initial.items[1]), "Gallery direction is not reactive");
    f.action("gallery.anchor.direction");
    f.action("gallery.anchor.affix");
    f.action("gallery.anchor.affix");
    f.action("gallery.anchor.topology");
    check(f.services.anchor().snapshot(id).keys.size() == 4, "Gallery topology did not remove an anchor");
    f.action("gallery.anchor.topology");
    check(f.services.anchor().snapshot(id).items[0] == initial.items[0], "Gallery topology remounted retained Anchor");
    f.page.set_width(520);
    f.update();
    check(f.page.sample_runs("gallery.sample.anchor.navigation") == 1, "Anchor Gallery width remounted factory");
    f.cleanup();
}

void breadcrumb_page() {
    GalleryFixture f;
    f.mount("ant.component.breadcrumb");
    const auto id = f.page.target("gallery.breadcrumb.dynamic-component");
    const auto legacy = f.page.target("gallery.breadcrumb.component");
    const auto original = f.services.breadcrumb().snapshot(id);
    check(original.items.size() == 4, "dynamic Breadcrumb Gallery absent");
    runtime::ComponentId editor;
    for (const auto& input : f.inputs.mounted_inputs()) {
        if (descendant(f, input.component, original.items[1])) {
            editor = input.component;
        }
    }
    check(editor.valid(), "dynamic Breadcrumb Gallery has no retained editor");
    click(f, editor);
    f.update();
    check(static_cast<bool>(f.inputs.dispatch(TextCommitted{String{u8" actual draft"}, f.inputs.sessions().active()})),
          "Gallery Breadcrumb editor did not accept native text");
    f.update();
    const auto draft = std::string{f.inputs.display_snapshot(editor).text};
    f.action("gallery.breadcrumb.reorder");
    const auto reversed = f.services.breadcrumb().snapshot(id);
    check(reversed.items.front() == original.items.back() && reversed.items.back() == original.items.front() &&
              f.services.components().contains(editor),
          "Gallery Breadcrumb reorder lost keyed draft");
    check(std::string{f.inputs.display_snapshot(editor).text} == draft,
          "Gallery Breadcrumb reorder reset the real draft");
    f.action("gallery.breadcrumb.topology");
    check(f.services.breadcrumb().snapshot(id).items.size() == 3 && f.services.components().contains(legacy),
          "Gallery Breadcrumb topology changed unrelated trail");
    f.action("gallery.breadcrumb.direction");
    const auto rtl = f.services.breadcrumb().snapshot(id);
    check(bounds(f, rtl.items[0]).x > bounds(f, rtl.items[1]).x, "Gallery Breadcrumb direction not connected");
    click(f, rtl.items.back());
    f.update();
    check(f.services.dropdown().mounted().size() == 1 &&
              f.services.dropdown().snapshot(f.services.dropdown().mounted()[0]).geometry.visible,
          "Gallery Breadcrumb built-in menu is not interactive");
    f.page.set_width(520);
    f.update();
    check(f.page.sample_runs("gallery.sample.breadcrumb.dynamic") == 1 && f.services.components().contains(editor),
          "Gallery Breadcrumb resize remounted draft");
    f.cleanup();
}
} // namespace

int main(int argc, char** argv) {
    try {
        if (argc > 1 && std::string_view{argv[1]} == "breadcrumb") {
            breadcrumb_page();
        } else {
            anchor_page();
        }
        std::cout << "Navigation Gallery contracts passed\n";
        return 0;
    } catch (const std::exception& e) {
        std::cerr << e.what() << '\n';
        return 1;
    }
}
