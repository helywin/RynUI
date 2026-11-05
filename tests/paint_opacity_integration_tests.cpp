#include "support/paint_opacity_fixture.hpp"
#include <iostream>
#include <cmath>

namespace {
using namespace paint_test;

void near(float actual, float expected, const char* message) {
    check(std::abs(actual - expected) < .0001F, message);
}

void image_and_preview() {
    RichScene scene;
    auto& f = scene.fixture;
    const auto original = scene.instance();
    std::vector<float> glyphs;
    for (const auto& glyph : f.scene.glyph_scene().instances().instances()) {
        glyphs.push_back(glyph.translation_opacity[2]);
    }
    check(!glyphs.empty(), "rich native glyph fixture is empty");
    scene.opacity(.5F);
    const auto half_glyphs = f.scene.glyph_scene().instances().instances();
    check(half_glyphs.size() == glyphs.size(), "native opacity rebuilt glyph topology");
    for (std::size_t i = 0; i < glyphs.size(); ++i) {
        near(half_glyphs[i].translation_opacity[2], glyphs[i] * .5F,
             "Button/Input/TextArea/Icon/Text glyph envelope diverged");
    }
    near(scene.instance().opacity, .5F, "native Image omitted ancestor opacity");
    f.nodes.set_paint_opacity(f.services.components().root(scene.image_owner), .5F);
    sync(f);
    near(scene.instance().opacity, .25F, "native Image nested opacity applied twice or omitted");
    scene.source.set(pixels(true));
    sync(f);
    check(scene.instance().pixels != original.pixels && scene.instance().bounds == original.bounds,
          "Image source replacement lost bounds or ownership");
    near(scene.instance().opacity, .25F, "Image source replacement lost envelope");
    scene.opacity(0);
    near(scene.instance().opacity, 0, "zero Image paint discarded its retained image");
    scene.opacity(1);
    f.nodes.set_paint_opacity(f.services.components().root(scene.image_owner), 1);
    sync(f);
    near(scene.instance().opacity, 1, "Image reset accumulated alpha");
    scene.visible.set(false);
    sync(f);
    scene.opacity(.5F);
    scene.visible.set(true);
    sync(f);
    near(scene.instance().opacity, .5F, "hidden Image restoration missed current envelope");
    check(scene.image.preview(), "Image preview could not open");
    sync(f);
    check(scene.image.action(ImageAction::RotateRight), "preview rotation failed");
    sync(f);
    bool preview{};
    for (const auto& slot : f.services.images().slots()) {
        if (slot.live && slot.instance.angle_degrees == 90) {
            near(slot.instance.opacity, .5F, "preview omitted ancestor envelope");
            preview = true;
        }
    }
    check(preview, "rotated preview image missing");
    check(scene.image.preview(false), "preview close failed");
    f.services.dispose();
    check(!f.nodes.has_paint_opacity() && f.services.images().live_count() == 0 && !f.services.next_frame_deadline(),
          "opacity/preview retirement leaked");
}

void carousel_media() {
    Fixture f;
    f.buttons.set_motion_preference(animation::MotionPreference::normal);
    CarouselRef ref;
    UploadFile file;
    file.uid = u8"thumbnail";
    file.name = u8"thumbnail.png";
    file.status = UploadFileStatus::Done;
    file.thumbnail = pixels();
    f.services.mount(Content{[&] {
        Carousel(CarouselProps{}
                     .ref(ref)
                     .effect(CarouselEffect::Fade)
                     .easing(CubicBezier{0, 0, 1, 1})
                     .speed(Duration::milliseconds(500))
                     .slides({{u8"image", CarouselContent{[] {
                                   Flex(FlexProps{}.vertical(true), [] {
                                       Image(ImageProps{}.source(pixels()).layout(
                                           LayoutStyle{}.width(dp(96)).height(dp(48))));
                                       PreviewGroup(PreviewGroupProps{}.items(
                                           std::vector<ImagePreviewItem>{{u8"group", pixels(), u8"group"}}));
                                   });
                               }}},
                              {u8"upload", CarouselContent{[file] {
                                   Upload(UploadProps{}.defaultFileList({file}).listType(UploadListType::Picture));
                               }}}})
                     .layout(LayoutStyle{}.width(dp(400)).height(dp(300))));
    }});
    sync(f);
    const auto owner = f.services.carousel().mounted().front();
    const auto original = f.services.carousel().snapshot(owner);
    check(ref.next(), "media Carousel next rejected");
    f.services.tick_animations(animation::AnimationTime::microseconds(250000));
    sync(f);
    sync(f); // Newly visible media can resolve its deferred source after geometry publication.
    check(f.services.carousel().snapshot(owner).transitioning, "media fade did not reach midpoint");
    std::size_t images{};
    for (const auto id : f.services.image().mounted()) {
        const auto view = f.services.image().snapshot(id);
        if (view.image) {
            near(f.services.images().find(*view.image)->instance.opacity, .5F,
                 "Carousel Image/PreviewGroup/Upload thumbnail missed fade");
            ++images;
        }
    }
    check(images >= 3, "media Image/PreviewGroup/Upload fixture omitted a native image");
    check(!f.services.interactions().component_active(original.slides[0]), "outgoing media slide retained input");
    check(f.services.interactions().component_active(original.slides[1]), "incoming media slide lost input");
    f.services.tick_animations(animation::AnimationTime::microseconds(500000));
    sync(f);
    check(ref.goTo(0, true), "media Carousel reset failed");
    sync(f);
    check(f.services.carousel().snapshot(owner).slides == original.slides, "media panels were cloned");
    f.services.dispose();
    check(f.services.images().live_count() == 0 && !f.nodes.has_paint_opacity() && !f.services.next_frame_deadline(),
          "media Carousel retained images/opacity/deadline");
}
} // namespace

int main() {
    try {
        image_and_preview();
        carousel_media();
    } catch (const std::exception& error) {
        std::cerr << error.what() << '\n';
        return 1;
    }
}
