#include <ryn/image_source.hpp>
#include "graphics/image_scene.hpp"
#include "renderer/common/scene_resources.hpp"
#include "renderer/recording/recording_renderer.hpp"
#include <array>
#include <cstring>
#include <fstream>
#include <iostream>
#include <stdexcept>
#include <thread>
using namespace ryn;
using namespace ryn::detail;

namespace {
int checks{};

void check(bool value) {
    ++checks;
    if (!value) {
        throw std::runtime_error("check " + std::to_string(checks));
    }
}

template <class F> void rejects(F f) {
    bool caught{};
    try {
        f();
    } catch (const std::exception&) {
        caught = true;
    }
    check(caught);
}

std::shared_ptr<const EncodedImage> fixture(const char* name) {
    std::ifstream f(std::string(RYNUI_IMAGE_FIXTURES) + "/" + name, std::ios::binary);
    const std::vector<char> bytes{std::istreambuf_iterator<char>(f), {}};
    check(!bytes.empty());
    return EncodedImage::copy(std::as_bytes(std::span(bytes)));
}

std::shared_ptr<const ImagePixels> pixels() {
    return decode_image(*fixture("colors.png")).pixels;
}

struct Scene {
    RecordingRenderer renderer;
    graphics::GlyphAtlas atlas;
    graphics::GlyphInstanceStore glyphs;
    graphics::ImageStore images;
    SceneResources resources{renderer};
    graphics::OrderedScene ordered;
    graphics::ImageId id;

    Scene() {
        id = images.create({pixels(), {0, 0, 30, 20}, {0, 0, 100, 100}, {15, 10}});
        ordered.append_command({graphics::SceneDrawKind::image, id.index, 1, id.generation});
    }

    bool sync() {
        return resources.synchronize({nullptr, atlas, glyphs, nullptr, {100, 100, 1}, &images});
    }

    void draw() {
        check(renderer.attach_scene(resources.attach(ordered)));
        check(renderer.submit_frame({}) == runtime::FrameSubmissionResult::submitted);
    }
};

void run(std::string mode) {
    if (mode == "decode") {
        for (const auto* file : {"colors.png", "colors.jpg", "colors.bmp"}) {
            const auto decoded = decode_image(*fixture(file));
            check(bool(decoded));
            check(decoded.pixels->width() == 3 && decoded.pixels->height() == 2);
            const auto b = decoded.pixels->bytes();
            check(std::to_integer<int>(b[0]) > 250 && std::to_integer<int>(b[1]) < 3);
            check(std::to_integer<int>(b[12]) < 25 && std::to_integer<int>(b[14]) > 120);
        }
        const auto p = pixels();
        check(p->bytes()[7] == std::byte{128});
        check(p->bytes()[11] == std::byte{0});
        check(decode_image(*EncodedImage::copy({})).error.code == ImageErrorCode::empty);
        std::array<std::byte, 8> bad{std::byte{137}, std::byte{'P'}, std::byte{'N'}, std::byte{'G'}};
        check(decode_image(*EncodedImage::copy(bad)).error.code == ImageErrorCode::malformed);
        check(decode_image(*fixture("colors.png"), {2}).error.code == ImageErrorCode::limits);
        bad[0] = std::byte{'<'};
        check(decode_image(*EncodedImage::copy(bad)).error.code == ImageErrorCode::unsupported_format);
    } else if (mode == "validate") {
        std::array<std::byte, 32> b{};
        rejects([&] { ImagePixels::copy(0, 2, 8, b); });
        rejects([&] { ImagePixels::copy(3, 2, 8, b); });
        rejects([&] { ImagePixels::copy(3, 2, 24, b); });
        rejects([&] { ImagePixels::copy(1, 2, std::size_t(-1), b); });
        rejects([&] { ImagePixels::copy(16385, 1, 65540, b); });
        rejects([&] { validate_image_texture_upload({b, 0, 4, 2, 2}, 2, 2); });
        rejects([&] { validate_image_texture_upload({b, std::size_t(-1), 8, 2, 2}, 2, 2); });
        Scene s;
        bool foreign{};
        std::thread t([&] {
            try {
                s.images.destroy(s.id);
            } catch (...) {
                foreign = true;
            }
        });
        t.join();
        check(foreign);
    } else if (mode == "pixels") {
        Scene s;
        std::array<std::byte, 32> padded{};
        const auto p = pixels();
        std::memcpy(padded.data(), p->bytes().data(), 12);
        std::memcpy(padded.data() + 20, p->bytes().data() + 12, 12);
        auto instance = s.images.find(s.id)->instance;
        instance.pixels = ImagePixels::copy(3, 2, 20, padded);
        check(s.images.update(s.id, instance));
        check(s.sync());
        s.draw();
        auto texture = s.renderer.texture_bytes(s.renderer.draws()[0].texture);
        check(texture.size() == 24);
        check(std::memcmp(texture.data(), p->bytes().data(), 24) == 0);
        check(s.images.destroy(s.id));
        check(s.sync());
        check(!s.renderer.attach_scene(s.resources.attach(s.ordered)));
        auto next = s.images.create(instance);
        check(next.index == s.id.index && next.generation != s.id.generation);
    } else if (mode == "transaction") {
        for (auto failure : {RecordingFailure::begin, RecordingFailure::create, RecordingFailure::upload,
                             RecordingFailure::commit, RecordingFailure::upload_exception}) {
            Scene s;
            s.renderer.fail_next(failure);
            bool failed{};
            try {
                failed = !s.sync();
            } catch (...) {
                failed = true;
            }
            check(failed);
            check(!s.renderer.attach_scene(s.resources.attach(s.ordered)));
            check(s.sync());
            s.draw();
            check(s.renderer.texture_bytes(s.renderer.draws()[0].texture)[7] == std::byte{128});
            s.resources.retire();
            check(s.renderer.live_resources() == 0);
        }
    } else if (mode == "epoch") {
        Scene s;
        check(s.sync());
        s.draw();
        auto old = s.resources.attach(s.ordered);
        s.renderer.reset_device();
        check(!s.renderer.attach_scene(old));
        check(s.sync());
        s.draw();
        check(s.renderer.counters().rejected_releases == 0);
        s.resources.retire();
        check(s.renderer.live_resources() == 0);
    } else if (mode == "capability") {
        Scene s;
        auto c = baseline_scene_capabilities();
        c.images = false;
        s.renderer.reset_device(c);
        rejects([&] { s.sync(); });
        check(s.renderer.live_resources() == 0);
        c.images = true;
        c.image_abi_version = 1;
        s.renderer.reset_device(c);
        rejects([&] { s.sync(); });
        check(s.renderer.live_resources() == 0);
        c.image_abi_version = image_packed_abi_version;
        c.rgba8_sampling = false;
        s.renderer.reset_device(c);
        rejects([&] { s.sync(); });
        check(s.renderer.live_resources() == 0);
        c.rgba8_sampling = true;
        c.maximum_texture_width = 2;
        s.renderer.reset_device(c);
        rejects([&] { s.sync(); });
        s.renderer.reset_device(baseline_scene_capabilities());
        check(s.sync());
        s.draw();
        s.resources.retire();
        c.maximum_texture_width = 2;
        s.renderer.reset_device(c);
        s.renderer.fail_next(RecordingFailure::begin);
        rejects([&] { s.sync(); });
        check(s.renderer.live_resources() == 0);
        s.renderer.reset_device(baseline_scene_capabilities());
        check(!s.sync()); // Preflight after retire must not have entered/consumed begin.
        check(s.sync());
        s.draw();
        s.resources.retire();
        check(s.images.destroy(s.id));
        s.ordered.clear();
        c = baseline_scene_capabilities();
        c.images = false;
        c.rgba8_sampling = false;
        c.image_abi_version = 0;
        c.maximum_buffer_bytes = 1;
        s.renderer.reset_device(c);
        check(s.sync()); // Dead slots need no Image capability, buffer or byte budget.
        s.draw();
    } else if (mode == "idle") {
        Scene s;
        check(s.sync());
        auto n = s.renderer.counters().uploads;
        check(s.sync());
        check(s.renderer.counters().uploads == n);
        auto instance = s.images.find(s.id)->instance;
        instance.angle_degrees = 90;
        check(s.images.update(s.id, instance));
        check(s.sync());
        check(s.renderer.counters().uploads == n + 1);
        s.resources.retire();
        check(s.renderer.live_resources() == 0);
    } else {
        throw std::runtime_error("unknown case");
    }
}
} // namespace

int main(int argc, char** argv) {
    try {
        run(argc > 1 ? argv[1] : "decode");
        std::cout << checks << " checks passed\n";
        return 0;
    } catch (const std::exception& e) {
        std::cerr << e.what() << '\n';
        return 1;
    }
}
