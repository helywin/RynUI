#include "support/paint_opacity_fixture.hpp"
#include "support/retained_media_fixture.hpp"
#include "renderer/common/scene_resources.hpp"
#include "renderer/sdl/scene_renderer.hpp"
#include <SDL3/SDL.h>
#include <cmath>
#include <filesystem>
#include <iostream>

namespace {
using namespace paint_test;
int checks{};
using Frame = std::unique_ptr<SDL_Surface, decltype(&SDL_DestroySurface)>;

std::array<int, 3> pixel(SDL_Surface* frame, int x, int y) {
    Uint8 red{};
    Uint8 green{};
    Uint8 blue{};
    Uint8 alpha{};
    check(SDL_ReadSurfacePixel(frame, x, y, &red, &green, &blue, &alpha), "GPU opacity pixel outside frame");
    ++checks;
    return {red, green, blue};
}

void expected(SDL_Surface* frame, int x, int y, std::array<int, 3> value) {
    const auto actual = pixel(frame, x, y);
    for (std::size_t i = 0; i < value.size(); ++i) {
        if (std::abs(actual[i] - value[i]) >= 10) {
            throw std::runtime_error("opacity GPU pixel " + std::to_string(x) + "," + std::to_string(y) +
                                     " channel=" + std::to_string(i) + " expected=" + std::to_string(value[i]) +
                                     " actual=" + std::to_string(actual[i]));
        }
        ++checks;
    }
}

std::array<int, 3> blend(std::array<int, 3> foreground, std::array<int, 3> background, float opacity) {
    for (std::size_t i = 0; i < foreground.size(); ++i) {
        foreground[i] = int(std::round(foreground[i] * opacity + background[i] * (1 - opacity)));
    }
    return foreground;
}
} // namespace

int main(int argc, char** argv) {
    using namespace ryn::detail;
    try {
        check(argc == 2, "GPU opacity output path required");
        const std::filesystem::path output{argv[1]};
        std::filesystem::create_directories(output);
        auto host = PlatformState::create({"RynUI private subtree opacity contract", 640, 600, false});
        check(bool(host), "GPU opacity host creation failed");
        SdlSceneRenderer renderer(*host.state);
        check(std::string{renderer.gpu_driver()} == "vulkan", "GPU opacity requires actual Vulkan");
        renderer.set_clear_color(Color::rgba8(255, 255, 255));
        SceneResources resources(renderer);
        const auto render = [&](Fixture& f, const char* name) {
            sync(f);
            check(resources.synchronize({&f.services.surfaces().instances(),
                                         f.scene.atlas(),
                                         f.scene.glyph_scene().instances(),
                                         &f.services.rounded_effects(),
                                         {640, 600, 1},
                                         &f.services.images()}),
                  "GPU opacity upload failed");
            graphics::OrderedScene scene;
            f.services.scene_composer().build_visible_scene(f.nodes, {0, 0, 640, 600}, scene);
            check(renderer.attach_scene(resources.attach(scene)), "GPU opacity attachment failed");
            const auto path = output / name;
            check(renderer.save_frame_bmp(path), "GPU opacity readback failed");
            Frame frame{SDL_LoadBMP(path.string().c_str()), SDL_DestroySurface};
            check(bool(frame), "GPU opacity readback decode failed");
            checks += 4;
            return frame;
        };
        RichScene rich;
        auto& f = rich.fixture;
        auto full = render(f, "native-full.bmp");
        const auto image = rich.instance().bounds;
        const int red_x = int(image.x + image.width / 4);
        const int green_x = int(image.x + image.width * .75F);
        const int image_y = int(image.y + image.height / 2);
        expected(full.get(), red_x, image_y, {255, 0, 0});
        expected(full.get(), green_x, image_y, {127, 255, 127});
        std::vector<std::pair<int, int>> samples;
        const auto button = f.nodes.require(f.buttons.mounted_buttons().front().node).bounds;
        samples.emplace_back(int(button.x + 10), int(button.y + button.height / 2));
        for (const auto& input : f.inputs.mounted_inputs()) {
            const auto rect = f.nodes.require(input.node).bounds;
            samples.emplace_back(int(rect.x + rect.width / 2), int(rect.y));
        }
        const auto text = f.nodes.require(f.services.components().root(rich.text_owner)).bounds;
        std::pair<int, int> darkest{};
        int minimum = 765;
        for (int y = int(text.y); y < int(text.y + text.height); ++y) {
            for (int x = int(text.x); x < int(text.x + text.width); ++x) {
                const auto value = pixel(full.get(), x, y);
                const int sum = value[0] + value[1] + value[2];
                if (sum < minimum) {
                    minimum = sum;
                    darkest = {x, y};
                }
            }
        }
        check(minimum < 300, "native Text had no actual glyph pixel");
        samples.push_back(darkest);
        rich.opacity(.5F);
        auto half = render(f, "native-half.bmp");
        expected(half.get(), red_x, image_y, {255, 127, 127});
        expected(half.get(), green_x, image_y, {191, 255, 191});
        for (const auto [x, y] : samples) {
            const auto base = pixel(full.get(), x, y);
            check(base != std::array<int, 3>{255, 255, 255}, "native opacity sample is only background");
            expected(half.get(), x, y, blend(base, {255, 255, 255}, .5F));
        }
        rich.opacity(0);
        auto zero = render(f, "native-zero.bmp");
        expected(zero.get(), red_x, image_y, {255, 255, 255});
        for (const auto [x, y] : samples) {
            expected(zero.get(), x, y, {255, 255, 255});
        }
        rich.opacity(1);
        auto reset = render(f, "native-reset.bmp");
        expected(reset.get(), red_x, image_y, {255, 0, 0});
        for (const auto [x, y] : samples) {
            expected(reset.get(), x, y, pixel(full.get(), x, y));
        }
        rich.opacity(.5F);
        check(rich.image.preview() && rich.image.action(ImageAction::RotateRight), "native preview transform failed");
        sync(f);
        const auto view = f.services.image().snapshot(rich.image_owner);
        const auto canvas = f.services.components().root(view.canvas);
        f.nodes.set_paint_opacity(canvas, 0);
        auto backdrop = render(f, "preview-background.bmp");
        f.nodes.set_paint_opacity(canvas, .5F);
        auto nested = render(f, "preview-nested-rotated.bmp");
        bool transformed{};
        for (const auto& slot : f.services.images().slots()) {
            if (!slot.live || slot.instance.angle_degrees != 90) {
                continue;
            }
            const auto& instance = slot.instance;
            check(std::abs(instance.opacity - .25F) < .0001F, "preview nested ancestor opacity is not .25");
            const int x = int(instance.pivot.x);
            const int y = int(instance.pivot.y - instance.bounds.width / 4);
            expected(nested.get(), x, y, blend({255, 0, 0}, pixel(backdrop.get(), x, y), .25F));
            transformed = true;
        }
        check(transformed, "native rotated preview resource absent");
        resources.retire();
        f.services.dispose();
        Fixture fade;
        fade.buttons.set_motion_preference(animation::MotionPreference::normal);
        CarouselRef ref;
        ThemeConfig theme;
        theme.carousel.tokens.background = Color::rgba8(0, 0, 0, 0);
        theme.image.tokens.background = Color::rgba8(0, 0, 0, 0);
        theme.image.tokens.border_width = dp(0);
        fade.services.mount(Content{[&] {
            Theme(ThemeProps{}.config(theme), ThemeContent{[&] {
                      Carousel(CarouselProps{}
                                   .ref(ref)
                                   .dots(false)
                                   .effect(CarouselEffect::Fade)
                                   .easing(CubicBezier{0, 0, 1, 1})
                                   .speed(Duration::milliseconds(500))
                                   .slides({{u8"red", CarouselContent{[] {
                                                 Image(ImageProps{}
                                                           .source(pixels())
                                                           .fit(ImageFit::Fill)
                                                           .layout(LayoutStyle{}.width(dp(320)).height(dp(160))));
                                             }}},
                                            {u8"blue", CarouselContent{[] {
                                                 Image(ImageProps{}
                                                           .source(pixels(true))
                                                           .fit(ImageFit::Fill)
                                                           .layout(LayoutStyle{}.width(dp(320)).height(dp(160))));
                                             }}}})
                                   .layout(LayoutStyle{}.width(dp(320)).height(dp(160))));
                  }});
        }});
        auto first = render(fade, "carousel-first.bmp");
        expected(first.get(), 80, 80, {255, 0, 0});
        check(ref.next(), "native media Carousel next failed");
        fade.services.tick_animations(animation::AnimationTime::microseconds(250000));
        auto middle = render(fade, "carousel-midpoint.bmp");
        expected(middle.get(), 80, 80, {128, 64, 191});
        const auto root = fade.services.components().root(fade.services.components().root_components().front());
        fade.nodes.set_paint_opacity(root, .5F);
        auto nested_fade = render(fade, "carousel-nested-midpoint.bmp");
        expected(nested_fade.get(), 80, 80, {191, 143, 207});
        resources.retire();
        fade.services.dispose();
        const auto before_retained = checks;
        retained_media_test::Scene retained;
        auto& media = retained.fixture;
        retained.opacity(.5F);
        retained.open();
        const auto media_bounds = retained.image().bounds;
        const int media_x = int(media_bounds.x + media_bounds.width / 4);
        const int media_y = int(media_bounds.y + media_bounds.height / 2);
        auto lazy_half = render(media, "retained-lazy-half.bmp");
        expected(lazy_half.get(), media_x, media_y, {255, 127, 127});
        check(retained.input.focus({InputFocusCursor::All}), "GPU rich Form input focus failed");
        check(
            bool(media.inputs.dispatch(input::TextCommitted{String{u8"GPU form"}, media.inputs.sessions().active()})) &&
                retained.field.get() == String{u8"GPU form"},
            "GPU rich Form automatic binding failed");
        retained.opacity(0);
        auto rich_zero = render(media, "retained-zero.bmp");
        expected(rich_zero.get(), media_x, media_y, {255, 255, 255});
        retained.opacity(1);
        auto rich_reset = render(media, "retained-reset.bmp");
        expected(rich_reset.get(), media_x, media_y, {255, 0, 0});
        retained.opacity(.5F);
        retained.revision.set(1);
        retained.sync();
        retained.sync();
        const auto replaced_bounds = retained.image().bounds;
        auto replaced = render(media, "retained-replaced-half.bmp");
        expected(replaced.get(), int(replaced_bounds.x + replaced_bounds.width / 4),
                 int(replaced_bounds.y + replaced_bounds.height / 2), {127, 127, 255});
        check(retained.field.get() == String{u8"GPU form"}, "GPU local region replacement lost Form authority");
        resources.retire();
        media.services.dispose();
        check(media.services.images().live_count() == 0 && !media.nodes.has_paint_opacity() &&
                  !media.services.next_frame_deadline(),
              "GPU retained media composition leaked");
        std::cout << checks - before_retained
                  << " additional Vulkan Carousel/Collapse/Descriptions/Image/Form composition checks passed\n";
        std::cout << checks << " Vulkan/SPIR-V native component/Image/glyph/effect opacity pixel checks passed\n";
    } catch (const std::exception& error) {
        std::cerr << error.what() << '\n';
        return 1;
    }
}
