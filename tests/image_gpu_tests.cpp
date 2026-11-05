#include "renderer/common/scene_resources.hpp"
#include "renderer/sdl/scene_renderer.hpp"
#include <ryn/image_source.hpp>
#include <SDL3/SDL.h>
#include <array>
#include <cstring>
#include <filesystem>
#include <fstream>
#include <iostream>
#include <stdexcept>

namespace {
int checks{};

void check(bool v) {
    ++checks;
    if (!v) {
        throw std::runtime_error("GPU image check " + std::to_string(checks));
    }
}

void pixel(SDL_Surface* surface, int x, int y, int r, int g, int b) {
    Uint8 red{};
    Uint8 green{};
    Uint8 blue{};
    Uint8 alpha{};
    check(SDL_ReadSurfacePixel(surface, x, y, &red, &green, &blue, &alpha));
    check(std::abs(int(red) - r) < 10);
    check(std::abs(int(green) - g) < 10);
    check(std::abs(int(blue) - b) < 10);
}
} // namespace

int main(int argc, char** argv) {
    using namespace ryn;
    using namespace ryn::detail;
    try {
        check(argc == 2);
        std::filesystem::create_directories(argv[1]);
        auto host = PlatformState::create({"RynUI private image GPU contract", 96, 64, false});
        check(bool(host));
        SdlSceneRenderer renderer(*host.state);
        renderer.set_clear_color(Color::rgba8(255, 255, 255));
        check(std::string(renderer.gpu_driver()) == "vulkan");
        graphics::GlyphAtlas atlas;
        graphics::GlyphInstanceStore glyphs;
        graphics::ImageStore images;
        std::array<std::byte, 32> source{};
        const unsigned char raw[24]{255, 0,  0,   255, 0,   255, 0,  128, 0,   0,   255, 0,
                                    17,  63, 129, 255, 240, 128, 16, 64,  255, 255, 255, 255};
        std::memcpy(source.data(), raw, 12);
        std::memcpy(source.data() + 20, raw + 12, 12);
        const auto data = ImagePixels::copy(3, 2, 20, source);
        const auto id = images.create({data, {0, 0, 60, 40}, {0, 0, 96, 64}, {30, 20}});
        graphics::OrderedScene scene;
        scene.append_command({graphics::SceneDrawKind::image, id.index, 1, id.generation});
        SceneResources resources(renderer);
        check(resources.synchronize({nullptr, atlas, glyphs, nullptr, {96, 64, 1}, &images}));
        check(renderer.attach_scene(resources.attach(scene)));
        const auto path = std::filesystem::path(argv[1]) / "image-rgba.bmp";
        check(renderer.save_frame_bmp(path));
        auto* surface = SDL_LoadBMP(path.string().c_str());
        check(surface != nullptr);
        pixel(surface, 9, 9, 255, 0, 0);
        pixel(surface, 29, 9, 127, 255, 127);
        pixel(surface, 49, 9, 255, 255, 255);
        pixel(surface, 9, 29, 17, 63, 129);
        pixel(surface, 29, 29, 251, 223, 195);
        pixel(surface, 80, 50, 255, 255, 255);
        SDL_DestroySurface(surface);
        const auto initial = images.find(id)->instance;
        const auto read_transformed = [&](const char* name) {
            check(resources.synchronize({nullptr, atlas, glyphs, nullptr, {96, 64, 1}, &images}));
            check(renderer.attach_scene(resources.attach(scene)));
            const auto file = std::filesystem::path(argv[1]) / name;
            check(renderer.save_frame_bmp(file));
            std::unique_ptr<SDL_Surface, decltype(&SDL_DestroySurface)> frame(SDL_LoadBMP(file.string().c_str()),
                                                                              &SDL_DestroySurface);
            check(bool(frame));
            return frame;
        };
        auto transformed = initial;
        transformed.angle_degrees = 90;
        check(images.update(id, transformed));
        auto rotated = read_transformed("image-rotated.bmp");
        pixel(rotated.get(), 40, 0, 255, 0, 0);
        pixel(rotated.get(), 40, 20, 127, 255, 127);
        pixel(rotated.get(), 0, 20, 255, 255, 255);
        transformed.clip = {20, 8, 24, 28};
        check(images.update(id, transformed));
        auto clipped = read_transformed("image-clipped.bmp");
        pixel(clipped.get(), 40, 0, 255, 255, 255);
        pixel(clipped.get(), 40, 20, 127, 255, 127);
        pixel(clipped.get(), 15, 20, 255, 255, 255);
        transformed = initial;
        transformed.flip_x = true;
        transformed.flip_y = true;
        check(images.update(id, transformed));
        auto flipped = read_transformed("image-flipped.bmp");
        pixel(flipped.get(), 50, 30, 255, 0, 0);
        pixel(flipped.get(), 50, 10, 17, 63, 129);
        check(images.update(id, initial));
        check(resources.synchronize({nullptr, atlas, glyphs, nullptr, {96, 64, 1}, &images}));
        check(renderer.attach_scene(resources.attach(scene)));
        auto attachment = resources.attach(scene);
        check(renderer.begin_upload_batch());
        check(!renderer.upload_image_texture(attachment.images()->texture(id.index), {source, 0, 4, 3, 2}));
        renderer.cancel_upload_batch();
        check(resources.synchronize({nullptr, atlas, glyphs, nullptr, {96, 64, 1}, &images}));
        check(renderer.attach_scene(resources.attach(scene)));
        check(renderer.save_frame_bmp(std::filesystem::path(argv[1]) / "image-retry.bmp"));
        const unsigned char red_pixel[4]{255, 0, 0, 255};
        auto round_pixels = ImagePixels::copy(1, 1, 4, std::as_bytes(std::span(red_pixel)));
        graphics::ImageInstance round{round_pixels, {64, 0, 32, 32}, {64, 0, 32, 32}, {80, 16}};
        round.circle_clip = runtime::Rect{64, 0, 32, 32};
        const auto round_id = images.create(round);
        scene.append_command({graphics::SceneDrawKind::image, round_id.index, 1, round_id.generation});
        check(resources.synchronize({nullptr, atlas, glyphs, nullptr, {96, 64, 1}, &images}));
        check(renderer.attach_scene(resources.attach(scene)));
        const auto circle_path = std::filesystem::path(argv[1]) / "image-circle.bmp";
        check(renderer.save_frame_bmp(circle_path));
        auto* circle = SDL_LoadBMP(circle_path.string().c_str());
        check(circle != nullptr);
        pixel(circle, 80, 16, 255, 0, 0);
        pixel(circle, 65, 1, 255, 255, 255);
        SDL_DestroySurface(circle);
        resources.retire();
        check(!renderer.attach_scene(attachment));
        std::cout << checks << " Vulkan RGBA/alpha/stride/rotation/flip/clip/failure/retirement checks passed; shader="
                  << renderer.shader_format() << '\n';
        return 0;
    } catch (const std::exception& e) {
        std::cerr << e.what() << '\n';
        return 1;
    }
}
