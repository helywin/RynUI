#include "gallery_sample_registry.hpp"
#include "gallery_sample_source.hpp"
#include "gallery_test_inventory.hpp"
#include "reference_surface.hpp"
#include "component/avatar_component.hpp"
#include "component/image_component.hpp"
#include "component/tag_badge_component.hpp"
#include "support/timeline_fixture.hpp"
#include <iostream>

namespace {
using namespace timeline_test;
using namespace rynui::example;
int checks{};

void expect(bool value, const char* why) {
    ++checks;
    check(value, why);
}

struct Gallery final : Fixture {
    ReferenceSurfaceHost surfaces{buttons};
    GallerySamplePage page;

    void update() {
        expect(services.layout_and_synchronize({1000, 2000}, {0, 0, 1000, 2000}, {}, 0, true),
               "Status Gallery layout failed");
    }

    void mount(std::string_view identity) {
        GalleryPageModel model;
        page = make_gallery_sample_page(model.find(identity)->route(), {});
        page.set_width(900);
        surfaces.mount(page.content, &inputs);
        update();
    }

    void action(std::string_view target) {
        click(*this, page.target(target));
        update();
    }

    void cleanup() {
        services.dispose();
        expect(services.images().live_count() == 0 && services.interactions().size() == 0 &&
                   !services.next_frame_deadline(),
               "Status Gallery retirement leaked resources");
    }
};

void avatar() {
    Gallery f;
    f.mount("ant.component.avatar");
    expect(f.page.sample_runs("gallery.sample.avatar.images") == 1, "Avatar image Gallery factory absent");
    runtime::ComponentId image;
    for (auto id : f.services.avatar().mounted()) {
        const auto child = f.services.avatar().snapshot(id).image;
        if (child.valid() && f.services.image().snapshot(child).status == ImageStatus::Loading) {
            image = child;
            break;
        }
    }
    expect(image.valid(), "Avatar Gallery loading owner missing");
    f.action("gallery.avatar.image-success");
    expect(f.services.image().snapshot(image).status == ImageStatus::Ready &&
               f.services.image().snapshot(image).image.has_value(),
           "Avatar Gallery did not publish actual pixels");
    f.action("gallery.avatar.image-shape");
    const auto rounded = f.services.image().snapshot(image);
    expect(rounded.image && !rounded.modal.valid() &&
               f.services.images().find(*rounded.image)->instance.source_uv != runtime::Rect{0, 0, 1, 1},
           "Avatar Square did not use final rounded pieces or mounted a disabled preview");
    f.action("gallery.avatar.image-retry");
    f.action("gallery.avatar.image-error");
    expect(f.services.image().snapshot(image).status == ImageStatus::Error && !f.services.image().snapshot(image).image,
           "Failed Avatar Gallery retained stale pixels");
    f.action("gallery.avatar.image-retry");
    f.action("gallery.avatar.image-success");
    runtime::ComponentId group;
    for (auto id : f.services.avatar().groups()) {
        if (f.services.avatar().group_snapshot(id).items.size() == 3) {
            group = id;
            break;
        }
    }
    expect(group.valid(), "Avatar Gallery dynamic group missing");
    const auto before = f.services.avatar().group_snapshot(group);
    f.action("gallery.avatar.keyed-reorder");
    expect(f.services.avatar().group_snapshot(group).items.back().inline_avatar == before.items.front().inline_avatar,
           "Gallery Avatar reorder rebuilt key");
    click(f, f.services.avatar().group_snapshot(group).overflow);
    f.update();
    expect(f.services.avatar().group_snapshot(group).overflow_count == 2, "Avatar Gallery overflow count wrong");
    f.cleanup();
}

void badge() {
    Gallery f;
    f.services.set_motion_preference(animation::MotionPreference::normal);
    f.mount("ant.component.badge");
    runtime::ComponentId count;
    for (auto id : f.services.tag_badge().badges()) {
        if (f.services.tag_badge().badge_snapshot(id).display_count == String{u8"8"}) {
            count = id;
            break;
        }
    }
    expect(count.valid(), "Animated Badge Gallery missing");
    f.action("gallery.badge.native-increment");
    expect(f.services.tag_badge().badge_snapshot(count).display_count == String{u8"9"} &&
               f.services.tag_badge().badge_snapshot(count).number_motion == 0,
           "Gallery Badge button did not start roll");
    f.services.tick_animations(animation::AnimationTime::microseconds(50000));
    f.update();
    expect(f.services.tag_badge().badge_snapshot(count).number_motion > 0 &&
               f.services.tag_badge().badge_snapshot(count).number_motion < 1,
           "Gallery Badge skipped rendered midpoint");
    f.action("gallery.badge.processing-toggle");
    expect(std::ranges::any_of(f.services.tag_badge().badges(),
                               [&](auto id) { return f.services.tag_badge().badge_snapshot(id).ribbon; }),
           "Gallery Ribbon missing");
    f.cleanup();
}

void tag() {
    Gallery f;
    f.mount("ant.component.tag");
    expect(f.page.sample_runs("gallery.sample.tag.native") == 1, "Tag native Gallery missing");
    runtime::ComponentId group;
    for (auto id : f.services.tag_badge().groups()) {
        if (f.services.tag_badge().group_snapshot(id).items.size() == 3) {
            group = id;
            break;
        }
    }
    expect(group.valid(), "Tag Gallery draggable group missing");
    auto before = f.services.tag_badge().group_snapshot(group);
    const auto row = f.services.components().parent(before.items.front().second).value();
    const auto handle = f.services.components().children(row).front();
    input::InteractionId hit;
    for (auto h : f.services.interactions().declaration_order()) {
        if (f.services.interactions().require(h).component == handle) {
            hit = h;
            break;
        }
    }
    expect(f.services.focus().request_focus(hit, FocusModality::keyboard), "Gallery Tag handle focus failed");
    f.services.focus().dispatch({Key::right, KeyAction::down});
    f.update();
    expect(f.services.tag_badge().group_snapshot(group).items.front().first == String{u8"draft"},
           "Gallery controlled reorder refusal failed");
    f.action("gallery.tag.allow-reorder");
    expect(f.services.focus().request_focus(hit, FocusModality::keyboard), "Tag refocus failed");
    f.services.focus().dispatch({Key::right, KeyAction::down});
    f.update();
    expect(f.services.tag_badge().group_snapshot(group).items[1].second == before.items.front().second,
           "Gallery controlled reorder lost retained key");
    f.action("gallery.tag.allow-close");
    f.cleanup();
}
} // namespace

int main(int argc, char** argv) {
    try {
        expect(gallery_samples().size() == 100 && gallery_sample_sources().size() == 100 &&
                   gallery_legacy_test_ids().size() == 458,
               "Gallery inventory drift");
        const std::string mode = argc > 1 ? argv[1] : "all";
        if (mode == "avatar" || mode == "all") {
            avatar();
        }
        if (mode == "badge" || mode == "all") {
            badge();
        }
        if (mode == "tag" || mode == "all") {
            tag();
        }
        std::cout << checks << " " << mode << " Gallery checks passed\n";
        return 0;
    } catch (const std::exception& e) {
        std::cerr << e.what() << '\n';
        return 1;
    }
}
