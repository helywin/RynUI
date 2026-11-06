#include "renderer/common/scene_resources.hpp"
#include "support/input_fixture.hpp"
#include "component/image_component.hpp"
#include "renderer/sdl/scene_renderer.hpp"
#include <SDL3/SDL.h>
#include <filesystem>
#include <iostream>
#include <cmath>
#include <numbers>
using namespace ryn;
using namespace ryn::detail;

namespace {
std::uint64_t checks{};

void check(bool value, const char* message) {
    ++checks;
    if (!value) {
        throw std::runtime_error(message);
    }
}

using Surface = std::unique_ptr<SDL_Surface, decltype(&SDL_DestroySurface)>;

std::array<Uint8, 4> pixel(SDL_Surface* surface, int x, int y) {
    std::array<Uint8, 4> value{};
    check(SDL_ReadSurfacePixel(surface, x, y, &value[0], &value[1], &value[2], &value[3]), "GPU readback pixel failed");
    return value;
}
} // namespace

int main(int argc, char** argv) {
    try {
        check(argc == 2, "output path required");
        std::filesystem::create_directories(argv[1]);
        auto host = PlatformState::create({"RynUI rounded image private readback", 192, 128, false});
        check(bool(host), "private GPU host failed");
        SdlSceneRenderer renderer{*host.state};
        renderer.set_clear_color(Color::rgba8(11, 37, 73));
        check(std::string(renderer.gpu_driver()) == "vulkan", "Vulkan required");
        graphics::GlyphAtlas atlas;
        graphics::GlyphInstanceStore glyphs;
        graphics::ImageStore images;
        SceneResources resources{renderer};
        std::array<std::byte, 16> bytes{};
        for (std::size_t i = 0; i < 4; ++i) {
            bytes[i * 4] = std::byte{230};
            bytes[i * 4 + 1] = std::byte{80};
            bytes[i * 4 + 2] = std::byte{150};
            bytes[i * 4 + 3] = std::byte{128};
        }
        const auto source = ImagePixels::copy(2, 2, 8, bytes);
        std::vector<graphics::ImageId> ids;
        graphics::OrderedScene scene;
        const auto draw = [&](const std::vector<graphics::ImageInstance>& pieces, float scale, const char* name) {
            for (auto id : ids) {
                images.destroy(id);
            }
            ids.clear();
            scene.clear();
            for (auto piece : pieces) {
                const auto id = images.create(piece);
                ids.push_back(id);
                scene.append_command({graphics::SceneDrawKind::image, id.index, 1, id.generation});
            }
            check(resources.synchronize({nullptr, atlas, glyphs, nullptr, {192, 128, scale}, &images}),
                  "rounded GPU upload failed");
            check(renderer.attach_scene(resources.attach(scene)), "rounded GPU scene attach failed");
            const auto path = std::filesystem::path(argv[1]) / name;
            check(renderer.save_frame_bmp(path), "rounded GPU readback failed");
            Surface surface{SDL_LoadBMP(path.string().c_str()), &SDL_DestroySurface};
            check(bool(surface), "rounded GPU bitmap missing");
            return surface;
        };
        for (float scale : {1.F, 1.25F, 1.5F, 2.F}) {
            for (bool top : {false, true}) {
                for (float radius : {0.F, 8.F, 1000.F}) {
                    for (float opacity : {1.F, .5F, 0.F, 1.F}) {
                        const runtime::Rect canvas{12.25F, 10.5F, 64.5F, 42.5F};
                        graphics::ImageInstance full{
                            source, {8.25F, 6.5F, 76.5F, 50.5F}, {14.F, 10.F, 68.F, 44.F}, {46.5F, 31.75F}};
                        full.opacity = opacity;
                        full.flip_x = true;
                        auto reference = draw({full}, scale, "reference.bmp");
                        auto frame =
                            draw(graphics::decompose_rounded_image(full, canvas, radius, top), scale, "rounded.bmp");
                        const float r = std::min(radius, std::min(canvas.width, canvas.height) / 2);
                        for (int y = 0; y < frame->h; ++y) {
                            for (int x = 0; x < frame->w; ++x) {
                                const float px = (x + .5F) / scale;
                                const float py = (y + .5F) / scale;
                                const bool inside = px > canvas.x && px < canvas.x + canvas.width && py > canvas.y &&
                                                    py < canvas.y + canvas.height;
                                const bool full_coverage =
                                    (px > canvas.x + r && px < canvas.x + canvas.width - r) ||
                                    (py > canvas.y + r && (top || py < canvas.y + canvas.height - r));
                                if (inside && full_coverage) {
                                    check(pixel(frame.get(), x, y) == pixel(reference.get(), x, y),
                                          "rounded geometry seam blended RGBA twice or remapped UV");
                                }
                            }
                        }
                        if (r > 5 && opacity > 0) {
                            const int x = static_cast<int>((canvas.x + 2.5F) * scale);
                            const int y = static_cast<int>((canvas.y + .25F) * scale);
                            const auto corner = pixel(frame.get(), x, y);
                            check(corner[0] == 11 && corner[1] == 37 && corner[2] == 73,
                                  "rounded top corner did not preserve transparent background");
                            const int bottom = static_cast<int>((canvas.y + canvas.height) * scale - 1.F);
                            if (top) {
                                check(pixel(frame.get(), x, bottom) == pixel(reference.get(), x, bottom),
                                      "top-only policy rounded bottom corner");
                            } else {
                                const auto low = pixel(frame.get(), x, bottom);
                                check(low[0] == 11 && low[1] == 37 && low[2] == 73,
                                      "rounded bottom corner was not clipped");
                            }
                        }
                        const auto upload_count = images.live_count();
                        check(upload_count <= 7, "rounded GPU live image count unbounded");
                    }
                }
            }
        }
        for (float scale : {1.F, 1.25F, 1.5F, 2.F}) {
            for (float angle : {30.F, 90.F}) {
                const runtime::Rect canvas{24.25F, 20.5F, 64.5F, 42.5F};
                graphics::ImageInstance full{source, canvas, {0, 0, 192 / scale, 128 / scale}, {56.5F, 41.75F}};
                full.angle_degrees = angle;
                full.opacity = .5F;
                auto reference = draw({full}, scale, "rotated-reference.bmp");
                auto frame = draw(graphics::decompose_rounded_image(full, canvas, 8), scale, "rotated-rounded.bmp");
                const double radians = angle * std::numbers::pi / 180;
                for (int y = 0; y < frame->h; ++y) {
                    for (int x = 0; x < frame->w; ++x) {
                        const double dx = (x + .5) / scale - full.pivot.x;
                        const double dy = (y + .5) / scale - full.pivot.y;
                        const double px = full.pivot.x + std::cos(radians) * dx + std::sin(radians) * dy;
                        const double py = full.pivot.y - std::sin(radians) * dx + std::cos(radians) * dy;
                        const bool inside = px > canvas.x + 1 && px < canvas.x + canvas.width - 1 &&
                                            py > canvas.y + 1 && py < canvas.y + canvas.height - 1;
                        const bool central = (px > canvas.x + 9 && px < canvas.x + canvas.width - 9) ||
                                             (py > canvas.y + 9 && py < canvas.y + canvas.height - 9);
                        if (inside && central) {
                            check(pixel(frame.get(), x, y) == pixel(reference.get(), x, y),
                                  "rotated rounded pieces doubled RGBA coverage or lost full-image transform");
                        }
                    }
                }
            }
        }
        renderer.set_clear_color(Color::rgba8(0, 0, 0, 0));
        for (float scale : {1.F, 1.25F, 1.5F, 2.F}) {
            for (float opacity : {1.F, .5F, 0.F, 1.F}) {
                const runtime::Rect canvas{16.25F, 12.5F, 64.5F, 48.5F};
                graphics::ImageInstance full{source, canvas, {0, 0, 192 / scale, 128 / scale}, {48.5F, 36.75F}};
                full.opacity = opacity;
                auto reference = draw({full}, scale, "transparent-reference.bmp");
                auto frame =
                    draw(graphics::decompose_rounded_image(full, canvas, 12), scale, "transparent-rounded.bmp");
                const auto center = pixel(frame.get(), int(48.5F * scale), int(36.75F * scale));
                check(center[3] == Uint8(128 * opacity), "transparent readback lost source alpha/paint opacity");
                for (int y = int(26 * scale); y < int(47 * scale); ++y) {
                    for (int x = int(18 * scale); x < int(78 * scale); ++x) {
                        check(pixel(frame.get(), x, y) == pixel(reference.get(), x, y),
                              "transparent RGBA seam blended twice or changed source alpha");
                    }
                }
            }
        }
        renderer.set_clear_color(Color::rgba8(11, 37, 73));
        const std::array<std::byte, 16> colored{std::byte{255}, std::byte{0},   std::byte{0},   std::byte{128},
                                                std::byte{0},   std::byte{255}, std::byte{0},   std::byte{64},
                                                std::byte{0},   std::byte{0},   std::byte{255}, std::byte{0},
                                                std::byte{240}, std::byte{120}, std::byte{10},  std::byte{255}};
        const auto gradient = ImagePixels::copy(2, 2, 8, colored);
        for (float scale : {1.F, 1.25F, 1.5F, 2.F}) {
            for (bool flip : {false, true}) {
                graphics::ImageInstance original{gradient, {8, 8, 80, 48}, {0, 0, 96, 64}, {48, 32}};
                original.flip_x = flip;
                original.opacity = .5F;
                auto reference = draw({original}, scale, "uv-reference.bmp");
                auto frame =
                    draw(graphics::decompose_rounded_image(original, {10, 10, 72, 40}, 8), scale, "uv-rounded.bmp");
                for (auto point :
                     {runtime::Point{20, 15}, runtime::Point{73, 15}, runtime::Point{20, 47}, runtime::Point{73, 47}}) {
                    const int x = static_cast<int>(point.x * scale);
                    const int y = static_cast<int>(point.y * scale);
                    check(pixel(frame.get(), x, y) == pixel(reference.get(), x, y),
                          "rounded source UV/flip/alpha differs from full image reference");
                }
            }
        }
        for (auto id : ids) {
            images.destroy(id);
        }
        check(resources.synchronize({nullptr, atlas, glyphs, nullptr, {192, 128, 1}, &images}),
              "rounded GPU retire failed");
        resources.retire();
        ryn_test::input_component::Fixture native;
        ThemeConfig config;
        config.image.tokens.background = Color::rgba8(70, 150, 190, 96);
        config.image.tokens.border_width = dp(0);
        config.image.tokens.radius = dp(0);
        Signal<ThemeConfig> theme{config};
        Signal<ImageFit> fit{ImageFit::Contain};
        native.services.mount(Content{[&] {
            Theme(ThemeProps{}.config(theme), ThemeContent{[&] {
                      Flex(FlexProps{}, FlexContent{[&] {
                               Image(ImageProps{}
                                         .source(ImageSource{gradient})
                                         .fit(fit)
                                         .preview(false)
                                         .layout(LayoutStyle{}.width(dp(64.5F)).height(dp(42.5F))));
                           }});
                  }});
        }});
        const auto owner = native.services.image().mounted().front();
        const auto parent = *native.services.components().parent(owner);
        const auto node = native.services.components().root(owner);
        const auto render_native = [&](float scale, const char* name) {
            check(native.services.layout_and_synchronize({192 / scale, 128 / scale}, {0, 0, 192 / scale, 128 / scale}),
                  "native rounded Image layout failed");
            check(resources.synchronize({&native.services.surfaces().instances(),
                                         native.scene.atlas(),
                                         native.scene.glyph_scene().instances(),
                                         &native.services.rounded_effects(),
                                         {192, 128, scale},
                                         &native.services.images()}),
                  "native Image upload failed");
            graphics::OrderedScene ordered;
            native.services.scene_composer().build_visible_scene(native.nodes, {0, 0, 192 / scale, 128 / scale},
                                                                 ordered);
            check(renderer.attach_scene(resources.attach(ordered)), "native Image attach failed");
            const auto path = std::filesystem::path(argv[1]) / name;
            check(renderer.save_frame_bmp(path), "native Image readback failed");
            Surface surface{SDL_LoadBMP(path.string().c_str()), &SDL_DestroySurface};
            check(bool(surface), "native Image bitmap missing");
            return surface;
        };
        for (float scale : {1.F, 1.25F, 1.5F, 2.F}) {
            for (auto mode : {ImageFit::Cover, ImageFit::Contain, ImageFit::None, ImageFit::ScaleDown}) {
                fit.set(mode);
                for (float opacity : {1.F, .5F, 0.F, 1.F}) {
                    native.nodes.set_paint_opacity(node, opacity);
                    native.services.image().set_cover_rounding(parent, 0);
                    auto reference = render_native(scale, "native-reference.bmp");
                    native.services.image().set_cover_rounding(parent, 13);
                    auto frame = render_native(scale, "native-top-rounded.bmp");
                    for (int y = 1; y < int(42.5F * scale) - 1; ++y) {
                        for (int x = 1; x < int(64.5F * scale) - 1; ++x) {
                            if (y / scale > 14 || (x / scale > 14 && x / scale < 50)) {
                                check(pixel(frame.get(), x, y) == pixel(reference.get(), x, y),
                                      "native cover background/pixels overlapped or rounded lower corners");
                            }
                        }
                    }
                    const auto corner = pixel(frame.get(), 1, 1);
                    check(corner[0] == 11 && corner[1] == 37 && corner[2] == 73,
                          "native cover background leaked through excluded top corner");
                }
            }
        }
        resources.retire();
        native.services.dispose();
        check(native.services.images().live_count() == 0 && native.services.surfaces().size() == 0,
              "native rounded Image retirement leaked resources");
        check(images.live_count() == 0, "rounded GPU ImageStore leak");
        std::cout << checks << " Vulkan rounded Image RGBA/readback checks passed\n";
        return 0;
    } catch (const std::exception& e) {
        std::cerr << e.what() << '\n';
        return 1;
    }
}
