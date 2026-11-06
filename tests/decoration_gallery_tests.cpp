#include "component/decoration_component.hpp"
#include "gallery_sample_registry.hpp"
#include "gallery_sample_source.hpp"
#include "reference_surface.hpp"
#include "support/timeline_fixture.hpp"

using namespace timeline_test;

int main(int argc, char** argv) {
    try {
        using namespace rynui::example;
        const std::string name = argc > 1 ? argv[1] : "watermark";
        Fixture f;
        ReferenceSurfaceHost surfaces{f.buttons};
        GalleryPageModel model;
        const auto* entry = model.find("ant.component." + name);
        check(entry != nullptr, "Missing decoration Gallery page");
        auto page = make_gallery_sample_page(entry->route(), {});
        page.set_width(720);
        surfaces.mount(page.content, &f.inputs);
        const auto synchronize = [&] {
            for (int frame = 0; frame < 2; ++frame) {
                check(f.services.layout_and_synchronize({900, 1800}, {0, 0, 900, 1800}, {}, 0, true),
                      "Decoration Gallery layout failed");
            }
        };
        const auto activate = [&](const char* target) {
            const auto id = page.target(target);
            check(f.services.components().contains(id), "Missing decoration Gallery action");
            const auto b = bounds(f, id);
            for (auto action : {PointerAction::down, PointerAction::up}) {
                f.services.pointer().dispatch(
                    {PointerIdentity::mouse(), action, PointerButton::primary, b.x + b.width / 2, b.y + b.height / 2});
            }
            synchronize();
        };
        synchronize();
        const auto sample = "gallery.sample." + name + ".native";
        check(page.sample_runs(sample) == 1 && gallery_sample_source(sample), "Missing real decoration sample/source");
        const auto mounted = f.services.decorations().mounted();
        check(mounted.size() == (name == "affix" ? 2U : 1U), "Decoration Gallery omitted actual component");
        const auto id = mounted.front();
        const auto retained = f.services.decorations().snapshot(id).content;
        if (name == "watermark" || name == "border-beam") {
            const auto prefix = "gallery." + name;
            const auto input = bounds(f, page.target(prefix + ".note"));
            const auto detail = bounds(f, page.target(prefix + ".detail"));
            check(input.width > 0 && input.height > 0 && detail.y >= input.y + input.height + 7.5F,
                  "Decoration Gallery explanation overlaps its retained input");
        }
        if (name == "watermark") {
            check(!f.services.decorations().snapshot(id).texts.empty(), "Gallery Watermark is empty");
            activate("gallery.watermark.rotate");
            activate("gallery.watermark.image");
            check(!f.services.decorations().snapshot(id).images.empty(),
                  "Gallery image watermark missing RGBA resources");
            activate("gallery.watermark.async");
            check(f.services.decorations().snapshot(id).image_status == ImageStatus::Loading,
                  "Gallery async action failed");
            activate("gallery.watermark.failure");
            check(f.services.decorations().snapshot(id).image_status == ImageStatus::Error,
                  "Gallery source failure not observable");
            activate("gallery.watermark.async");
            activate("gallery.watermark.complete");
            check(f.services.decorations().snapshot(id).image_status == ImageStatus::Ready, "Gallery retry failed");
        } else if (name == "affix") {
            activate("gallery.affix.target");
            activate("gallery.affix.offset");
            activate("gallery.affix.attach");
            for (auto affix : f.services.decorations().mounted()) {
                check(!f.services.decorations().snapshot(affix).affixed, "Gallery Affix release retained a layer");
            }
        } else if (name == "border-beam") {
            f.services.set_motion_preference(animation::MotionPreference::normal);
            synchronize();
            check(f.services.next_frame_deadline().has_value(), "Gallery Beam does not animate");
            activate("gallery.border-beam.direction");
            activate("gallery.border-beam.duration");
            activate("gallery.border-beam.theme");
            activate("gallery.border-beam.motion");
            check(!f.services.decorations().snapshot(id).animating &&
                      !f.services.decorations().next_auxiliary_deadline(),
                  "Gallery reduced-motion Beam polls");
        } else {
            throw std::runtime_error("Unknown decoration Gallery page");
        }
        check(f.services.decorations().snapshot(id).content == retained && page.sample_runs(sample) == 1,
              "Gallery action rebuilt retained content");
        page.set_width(420);
        synchronize();
        f.services.dispose();
        check(f.services.decorations().mounted().empty() && !f.services.next_frame_deadline() &&
                  f.services.images().live_count() == 0 && f.nodes.size() == 0,
              "Gallery off-page teardown leaked decoration resources or deadlines");
        return 0;
    } catch (const std::exception& error) {
        std::cerr << error.what() << '\n';
        return 1;
    }
}
