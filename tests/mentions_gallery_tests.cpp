#include "gallery_sample_registry.hpp"
#include "gallery_sample_source.hpp"
#include "gallery_test_inventory.hpp"
#include "reference_surface.hpp"
#include "support/mentions_fixture.hpp"
#include <iostream>

namespace {
using namespace mentions_test;
using namespace rynui::example;

struct GalleryFixture final : Fixture {
    ReferenceSurfaceHost surfaces{buttons};
    GallerySamplePage page;

    void update() {
        check(services.layout_and_synchronize({900, 1800}, {0, 0, 900, 1800}, {}, 0, true), "Mentions Gallery layout");
    }

    void mount() {
        GalleryPageModel model;
        const auto* entry = model.find("ant.component.mentions");
        check(entry && entry->native_scope(), "Mentions page remained a planned reference placeholder");
        page = make_gallery_sample_page(entry->route(), {});
        page.set_width(760);
        surfaces.mount(page.content, &inputs);
        update();
    }

    void focus(runtime::ComponentId owner) {
        for (const auto& input : inputs.mounted_inputs()) {
            if (descendant(*this, input.component, owner)) {
                services.focus().defer_focus(input.interaction, FocusModality::keyboard);
                update();
                return;
            }
        }
        throw std::runtime_error("Gallery native editor absent");
    }

    void action(std::string_view id) {
        click(*this, page.target(id));
        update();
    }
};

void run() {
    check(gallery_samples().size() >= 88 && gallery_sample_sources().size() == gallery_samples().size() &&
              gallery_legacy_test_ids().size() == 458 && gallery_legacy_test_targets().size() == 458,
          "Mentions changed the existing Gallery source or frozen legacy inventory");
    GalleryFixture f;
    f.mount();
    check(gallery_sample_source("gallery.sample.mentions.editing") &&
              f.page.sample_runs("gallery.sample.mentions.editing") == 1 && f.services.mentions().mounted().size() == 3,
          "real Mentions factory/source missing");
    const auto main = f.page.target("gallery.mentions.editor");
    const auto original = snapshot(f, main).input;
    f.focus(main);
    edit(f, u8"消息\n@Ad");
    check(snapshot(f, main).query == String{u8"Ad"}, "Gallery prefix/search");
    key(f, Key::enter);
    f.update();
    check(snapshot(f, main).value == String{u8"消息\n@Ada "}, "Gallery exact native replacement");
    f.action("gallery.mentions.accept");
    f.focus(main);
    edit(f, u8"@Grace");
    check(snapshot(f, main).value == String{u8"消息\n@Ada "}, "Gallery controlled refusal");
    f.action("gallery.mentions.accept");
    f.action("gallery.mentions.disabled");
    check(!snapshot(f, main).visible, "Gallery disabled");
    f.action("gallery.mentions.disabled");
    f.action("gallery.mentions.readonly");
    f.action("gallery.mentions.readonly");
    f.action("gallery.mentions.visible");
    f.action("gallery.mentions.visible");
    f.action("gallery.mentions.rtl");
    f.action("gallery.mentions.placement");
    check(snapshot(f, main).input == original, "Gallery lifecycle controls remounted editor");
    f.action("gallery.mentions.reset");
    check(snapshot(f, main).value == String{u8"在这里输入 @ 或 #\n"} && !snapshot(f, main).visible,
          "Gallery Form reset");
    const auto long_list = f.page.target("gallery.mentions.long");
    f.focus(long_list);
    edit(f, u8"@");
    f.update();
    check(snapshot(f, long_list).total_rows == 10000 && snapshot(f, long_list).rows.size() <= 10,
          "Gallery long suggestions unbounded");
    key(f, Key::escape);
    const auto async = f.page.target("gallery.mentions.async");
    f.focus(async);
    edit(f, u8"@old");
    edit(f, u8"@latest");
    f.update();
    check(snapshot(f, async).pending, "Gallery async source did not request");
    f.action("gallery.mentions.deliver");
    auto s = snapshot(f, async);
    check(!s.pending && s.total_rows == 1 && s.query == String{u8"latest"}, "Gallery async stale authority");
    click(f, s.rows.front());
    f.update();
    check(snapshot(f, async).value == String{u8"@latest-synthetic "}, "Gallery async choice");
    f.page.set_width(480);
    f.update();
    check(f.page.sample_runs("gallery.sample.mentions.editing") == 1 && snapshot(f, main).input == original,
          "Gallery resize reran factory");
    clean(f);
}
} // namespace

int main() {
    try {
        run();
        std::cout << "Mentions Gallery passed\n";
        return 0;
    } catch (const std::exception& e) {
        std::cerr << e.what() << '\n';
        return 1;
    }
}
