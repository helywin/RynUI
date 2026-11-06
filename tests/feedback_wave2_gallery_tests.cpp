#include "gallery_sample_registry.hpp"
#include "gallery_sample_source.hpp"
#include "gallery_test_inventory.hpp"
#include "reference_surface.hpp"
#include "component/empty_result_component.hpp"
#include "component/feedback_component.hpp"
#include "component/skeleton_component.hpp"
#include "support/input_fixture.hpp"
#include <algorithm>
#include <iostream>

namespace {
using namespace ryn;
using namespace rynui::example;

void check(bool value, const char* why) {
    if (!value) {
        throw std::runtime_error(why);
    }
}

struct Gallery final : ryn_test::input_component::Fixture {
    ReferenceSurfaceHost surfaces{buttons};
    GallerySamplePage page;

    void update() {
        check(services.layout_and_synchronize({1000, 5000}, {0, 0, 1000, 5000}, {}, 0, true), "Gallery layout failed");
    }

    void mount(std::string_view identity) {
        GalleryPageModel model;
        page = make_gallery_sample_page(model.find(identity)->route(), {});
        page.set_width(900);
        surfaces.mount(page.content, &inputs);
        update();
    }

    void action(std::string_view target) {
        const auto id = page.target(target);
        check(id.valid(), "new stable Gallery target missing");
        const auto r = nodes.require(services.components().root(id)).bounds;
        for (auto action : {input::PointerAction::down, input::PointerAction::up}) {
            services.pointer().dispatch({input::PointerIdentity::mouse(), action, input::PointerButton::primary,
                                         r.x + r.width / 2, r.y + r.height / 2});
        }
        update();
    }

    void cleanup() {
        services.dispose();
        check(services.images().live_count() == 0 && services.interactions().size() == 0 &&
                  !services.next_frame_deadline(),
              "feedback Gallery retirement leaked resources/deadlines");
    }
};

void run(std::string_view name) {
    Gallery f;
    f.mount(std::string{"ant.component."} + std::string{name});
    if (name == "result") {
        const auto id = f.services.empty_result().results().front();
        const auto initial = f.services.empty_result().snapshot(id);
        f.action("gallery.result.topology");
        f.action("gallery.result.status");
        f.action("gallery.result.topology");
        check(f.services.empty_result().snapshot(id).content == initial.content &&
                  f.services.empty_result().snapshot(id).status != initial.status,
              "Result Gallery actions did not update native status/slots");
    } else if (name == "skeleton") {
        f.services.set_motion_preference(animation::MotionPreference::normal);
        const auto id = f.services.skeleton().mounted().front();
        if (!f.services.skeleton().snapshot(id).loading) {
            f.action("gallery.skeleton.loading");
        }
        f.action("gallery.skeleton.active");
        check(f.services.skeleton().snapshot(id).animating && f.services.images().live_count() > 0,
              "Skeleton Gallery did not activate actual shimmer images");
        check(std::ranges::any_of(f.services.skeleton().mounted(),
                                  [&](auto owner) {
                                      return f.services.skeleton().snapshot(owner).kind ==
                                             ryn::detail::SkeletonKind::Image;
                                  }) &&
                  std::ranges::any_of(f.services.skeleton().mounted(),
                                      [&](auto owner) {
                                          return f.services.skeleton().snapshot(owner).kind ==
                                                 ryn::detail::SkeletonKind::Node;
                                      }),
              "Skeleton Gallery lacks real Image/Node mounts");
        f.action("gallery.skeleton.active");
        check(!f.services.skeleton().next_auxiliary_deadline(), "inactive Skeleton retained a shimmer deadline");
        static_cast<void>(f.services.tick_animations(animation::AnimationTime::microseconds(1000000)));
        f.update();
        static_cast<void>(f.services.tick_animations(animation::AnimationTime::microseconds(2000000)));
        f.update();
        check(!f.services.next_frame_deadline(), "Skeleton Gallery static mode did not settle");
    } else if (name == "spin") {
        f.services.set_motion_preference(animation::MotionPreference::normal);
        const auto id = f.services.feedback().spins().front();
        if (f.services.feedback().spin_snapshot(id).requested) {
            f.action("gallery.spin.toggle");
        }
        f.action("gallery.spin.percent");
        f.action("gallery.spin.delay");
        f.action("gallery.spin.toggle");
        check(!f.services.feedback().spin_snapshot(id).spinning, "Gallery delay did not defer visibility");
        static_cast<void>(f.services.tick_animations(animation::AnimationTime::microseconds(500000)));
        f.update();
        check(f.services.feedback().spin_snapshot(id).spinning && f.services.feedback().spin_snapshot(id).automatic,
              "Gallery delay/automatic percent did not use real component state");
        f.action("gallery.spin.fullscreen");
        const auto full = f.services.feedback().spin_snapshot(f.services.feedback().spins().back());
        check(full.spinning && full.fullscreen && full.overlay_bounds == runtime::Rect{0, 0, 1000, 5000},
              "Gallery fullscreen is not a viewport layer");
        f.action("gallery.spin.dismiss");
        check(!f.services.feedback().spin_snapshot(f.services.feedback().spins().back()).spinning,
              "real fullscreen dismissal button did not release its gate");
    } else {
        throw std::runtime_error("Choose result, skeleton or spin");
    }
    f.cleanup();
}
} // namespace

int main(int argc, char** argv) {
    try {
        check(argc == 2, "Choose one feedback page");
        run(argv[1]);
    } catch (const std::exception& error) {
        std::cerr << error.what() << '\n';
        return 1;
    }
    return 0;
}
