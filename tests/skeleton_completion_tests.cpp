#include "component/skeleton_component.hpp"
#include "support/input_fixture.hpp"
#include <array>
#include <cmath>
#include <iostream>

namespace {
using namespace ryn;
using Fixture = ryn_test::input_component::Fixture;

void check(bool value, const char* message) {
    if (!value) {
        throw std::runtime_error(message);
    }
}

void shimmer_lifecycle() {
    Fixture f;
    Signal<bool> active{false};
    Signal<bool> loading{true};
    int mounts{};
    f.services.mount(Content{[&] {
        Skeleton(SkeletonProps{}.loading(loading).active(active).avatar(true).layout(LayoutStyle{}.width(dp(240))),
                 SkeletonContent{[&] {
                     ++mounts;
                     Input(InputProps{}.defaultValue(u8"retained draft"));
                 }});
    }});
    f.services.set_motion_preference(animation::MotionPreference::normal);
    f.synchronize();
    const auto id = f.services.skeleton().mounted()[0];
    const auto content = f.services.skeleton().snapshot(id).content;
    check(!f.services.next_frame_deadline(), "static Skeleton scheduled frames");
    active.set(true);
    f.synchronize();
    check(f.services.skeleton().snapshot(id).animating && f.services.images().live_count() > 0,
          "active Skeleton did not publish common image geometry");
    std::vector<graphics::ImageId> identities;
    runtime::Rect uv;
    for (std::size_t i = 0; i < f.services.images().slots().size(); ++i) {
        const auto& slot = f.services.images().slots()[i];
        if (slot.live) {
            identities.push_back({static_cast<std::uint32_t>(i), slot.generation});
            uv = slot.instance.source_uv;
        }
    }
    check(f.services.next_frame_deadline().has_value(), "active Skeleton has no deadline");
    static_cast<void>(f.services.tick_animations(animation::AnimationTime::microseconds(400000)));
    f.synchronize();
    const auto* image = f.services.images().find(identities.back());
    check(image && image->instance.source_uv != uv, "shimmer did not move the immutable ramp's sampling window");
    const auto first = image->instance.pixels;
    static_cast<void>(f.services.tick_animations(animation::AnimationTime::microseconds(600000)));
    f.synchronize();
    check(f.services.images().find(identities.back())->instance.pixels == first,
          "shimmer uploaded new texture data per frame");
    f.services.set_motion_preference(animation::MotionPreference::reduced);
    f.synchronize();
    check(!f.services.next_frame_deadline() && f.services.images().live_count() == 0,
          "reduced motion kept shimmer resources/deadlines");
    f.services.set_motion_preference(animation::MotionPreference::normal);
    f.synchronize(320, {300, 200, 10, 10});
    check(!f.services.next_frame_deadline(), "clipped Skeleton continued animating");
    f.synchronize();
    loading.set(false);
    f.synchronize();
    check(!f.services.next_frame_deadline() && f.services.skeleton().snapshot(id).content == content && mounts == 1,
          "loading transition remounted content or retained motion");
    loading.set(true);
    f.synchronize();
    f.services.set_window_active(false);
    check(!f.services.next_frame_deadline(), "inactive window kept shimmer deadline");
    f.services.set_window_active(true);
    f.synchronize();
    check(f.services.destroy(id) && f.services.images().live_count() == 0 && !f.services.next_frame_deadline(),
          "Skeleton destruction leaked shimmer images/deadlines");
}

void image_node_and_failure() {
    Fixture f;
    int mounts{};
    f.services.mount(Content{[&] {
        Flex(FlexProps{}.gap(dp(8)), FlexContent{[&] {
                 SkeletonImage(SkeletonImageProps{}.layout(LayoutStyle{}.width(dp(120)).height(dp(80))));
                 SkeletonNode(SkeletonNodeProps{}.layout(LayoutStyle{}.width(dp(100)).height(dp(60))),
                              SkeletonNodeContent{[&] {
                                  ++mounts;
                                  Text(u8"Native node");
                              }});
             }});
    }});
    f.synchronize();
    const auto ids = std::vector(f.services.skeleton().mounted().begin(), f.services.skeleton().mounted().end());
    check(ids.size() == 2 && f.services.skeleton().snapshot(ids[0]).kind == detail::SkeletonKind::Image &&
              f.services.skeleton().snapshot(ids[1]).kind == detail::SkeletonKind::Node,
          "standalone native APIs did not mount");
    const auto image_content = f.services.skeleton().snapshot(ids[0]).content;
    const auto image_icon = f.services.components().children(image_content).front();
    const auto& icon_bounds = f.nodes.require(f.services.components().root(image_icon)).bounds;
    const auto& token = f.services.components().theme_scope(ids[0])->snapshot().skeleton();
    const auto icon_size = token.metrics.control_height * 1.5F;
    check(std::abs(icon_bounds.width - icon_size) < .01F && std::abs(icon_bounds.height - icon_size) < .01F,
          "SkeletonImage allocated a box without sizing its actual icon");
    const auto icon = f.services.text().icon_snapshot(image_icon);
    check(!icon.layers.empty(), "SkeletonImage has no actual icon glyph");
    const auto range = f.scene.primitive(icon.layers.front()).instances;
    check(range.count > 0, "SkeletonImage icon glyph is not realized");
    const auto color = f.scene.glyph_scene().instances().at(range.first).color;
    check(color == std::array{token.colors.image.red(), token.colors.image.green(), token.colors.image.blue(),
                              token.colors.image.alpha()},
          "SkeletonImage ignored its image color token");
    const auto body = f.services.skeleton().snapshot(ids[1]).content;
    check(f.services.components().branch_active(body) && mounts == 1, "SkeletonNode hid its actual retained content");
    f.services.dispose();
    check(f.services.images().live_count() == 0, "standalone dispose leaked images");
    auto recovery = std::make_unique<Fixture>();
    recovery->services.mount(Content{[&] {
        try {
            SkeletonNode({}, SkeletonNodeContent{[] { throw std::runtime_error("slot failure"); }});
        } catch (const std::runtime_error&) {
        }
        SkeletonImage();
    }});
    recovery->synchronize();
    check(recovery->services.skeleton().mounted().size() == 1,
          "locally caught SkeletonNode failure left an incomplete owner");
}
} // namespace

int main() {
    try {
        shimmer_lifecycle();
        image_node_and_failure();
    } catch (const std::exception& error) {
        std::cerr << error.what() << '\n';
        return 1;
    }
    return 0;
}
