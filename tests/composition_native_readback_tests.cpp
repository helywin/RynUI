#include "renderer/common/scene_resources.hpp"
#include "renderer/sdl/scene_renderer.hpp"
#include "component/card_component.hpp"
#include "component/image_component.hpp"
#include "support/input_fixture.hpp"
#include "support/composition_reference.hpp"
#include "component/app_component.hpp"
#include "component/modal_component.hpp"
#include <chrono>
#include <SDL3/SDL.h>
#if defined(_WIN32)
#ifndef NOMINMAX
#define NOMINMAX
#endif
#include <windows.h>
#include <cstdlib>
#if defined(_MSC_VER)
#include <crtdbg.h>
#endif
#endif
#include <algorithm>
#include <array>
#include <cmath>
#include <iostream>
#include <stdexcept>
#include <string>

namespace {
using namespace ryn;
using namespace ryn::detail;
using namespace ryn::graphics;

void require(bool value, const std::string& message) {
    if (!value) {
        throw std::runtime_error(message);
    }
}

void no_dialogs() {
#if defined(_WIN32)
    SetErrorMode(SEM_FAILCRITICALERRORS | SEM_NOGPFAULTERRORBOX | SEM_NOOPENFILEERRORBOX);
#if defined(_MSC_VER)
    _set_error_mode(_OUT_TO_STDERR);
    _set_abort_behavior(0, _WRITE_ABORT_MSG | _CALL_REPORTFAULT);
    for (const int kind : {_CRT_WARN, _CRT_ERROR, _CRT_ASSERT}) {
        _CrtSetReportMode(kind, _CRTDBG_MODE_FILE);
        _CrtSetReportFile(kind, _CRTDBG_FILE_STDERR);
    }
#endif
#endif
    SDL_SetAssertionHandler(
        [](const SDL_AssertData* data, void*) -> SDL_AssertState {
            std::cerr << data->condition << '\n';
            return SDL_ASSERTION_ABORT;
        },
        nullptr);
}

double coverage(double x, double y, runtime::RoundedContentClip s, double scale) {
    const double l = double(s.bounds.x) * scale;
    const double t = double(s.bounds.y) * scale;
    const double w = double(s.bounds.width) * scale;
    const double h = double(s.bounds.height) * scale;
    const double r = std::min(double(s.radius) * scale, std::min(w, h) * .5);
    const double qx = std::abs(x - (l + w * .5)) - (w * .5 - r);
    const double qy = std::abs(y - (t + h * .5)) - (h * .5 - r);
    const double d = std::hypot(std::max(0.0, qx), std::max(0.0, qy)) + std::min(std::max(qx, qy), 0.0) - r;
    const double u = std::clamp(d + .5, 0.0, 1.0);
    return 1 - u * u * (3 - 2 * u);
}

int quantize(double v) {
    return int(std::lround(std::clamp(v, 0.0, 1.0) * 255));
}

void pixel(const SceneRgbaReadback& f, unsigned x, unsigned y, std::array<int, 4> expected, int tolerance = 1) {
    const auto offset = std::size_t(y) * f.row_pitch + x * 4;
    for (unsigned c = 0; c < 4; ++c) {
        const int actual = std::to_integer<unsigned char>(f.pixels[offset + c]);
        require(std::abs(actual - expected[c]) <= tolerance,
                "RGBA at " + std::to_string(x) + "," + std::to_string(y) + " channel " + std::to_string(c) +
                    " actual=" + std::to_string(actual) + " expected=" + std::to_string(expected[c]));
    }
}

struct RawScene {
    SdlSceneRenderer& renderer;
    GlyphAtlas atlas;
    GlyphInstanceStore glyphs;
    RoundedEffectStore effects;
    ImageStore images;
    SceneResources resources;
    OrderedScene order;
    SceneDeviceMetrics metrics;

    RawScene(SdlSceneRenderer& r, PlatformState& p, float scale = 1) : renderer(r), resources(r) {
        const auto m = p.window_metrics();
        metrics = {std::uint32_t(m.pixel_width), std::uint32_t(m.pixel_height), scale};
    }

    SceneRgbaReadback read() {
        require(resources.synchronize({nullptr, atlas, glyphs, &effects, metrics, &images, &order}),
                "composition upload");
        require(renderer.attach_scene(resources.attach(order)), "composition attach");
        SceneRgbaReadback result;
        require(renderer.read_frame_rgba(result), renderer.last_error());
        require(result.row_pitch == result.width * 4 &&
                    result.pixels.size() == std::size_t(result.row_pitch) * result.height,
                "owned tight RGBA");
        return result;
    }
};

void masks(SdlSceneRenderer& renderer, PlatformState& platform) {
    renderer.set_clear_color(Color{0, 0, 0, 0});
    for (const float scale : {.25F, .75F, 1.0F, 1.25F, 1.5F, 2.0F}) {
        for (const bool self : {false, true}) {
            RawScene f(renderer, platform, scale);
            const runtime::RoundedContentClip shape{{7.25F / scale, 9.5F / scale, 62.5F / scale, 51.25F / scale},
                                                    11.5F / scale};
            (void)f.effects.add(make_shadow_effect({{0, 0, 1, 1}, 0}, {ShadowKind::outer, {}, 0, 0, Color{0, 1, 0}}));
            const auto fill =
                self ? LogicalRoundedRect{shape.bounds, shape.radius}
                     : LogicalRoundedRect{
                           {0, 0, float(f.metrics.pixel_width) / scale, float(f.metrics.pixel_height) / scale}, 0};
            (void)f.effects.add(make_shadow_effect(fill, {ShadowKind::outer, {}, 0, 0, Color{1, 0, 0, .5F}}));
            f.order.append_command({SceneDrawKind::rounded_effect, 1, 1});
            auto p = std::make_shared<SceneCompositionProgram>();
            p->scene_revision = f.order.revision();
            p->requests.push_back(
                {{1, 1}, shape, {0, 0, float(f.metrics.pixel_width) / scale, float(f.metrics.pixel_height) / scale}});
            p->operations = {{SceneCompositionOpKind::BeginMaskedGroup, 0, 0, 0},
                             {SceneCompositionOpKind::DrawRange, 0, 1, 0,
                              self ? DrawCoverageRole::GroupOwnsOuterCoverage : DrawCoverageRole::Normal},
                             {SceneCompositionOpKind::EndMaskedGroup, 0, 0, 0}};
            f.order.set_composition(p);
            const auto frame = f.read();
            for (unsigned y = 0; y < frame.height; ++y) {
                for (unsigned x = 0; x < frame.width; ++x) {
                    const int a = quantize((128.0 / 255) * coverage(x + .5, y + .5, shape, scale));
                    pixel(frame, x, y, {a, 0, 0, a}, 1); // One RGBA8 write per group; no coverage squared.
                }
            }
            require(f.order.commands()[0].first_instance == 1, "readback rebuilt command first");
        }
    }
}

void real_card(SdlSceneRenderer& renderer, float scale) {
    ryn_test::input_component::Fixture f;
    f.font_scale = scale;
    f.services.set_display_scale(scale);
    std::array<std::byte, 16> bytes;
    bytes.fill(std::byte{255});
    const auto pixels = ImagePixels::copy(2, 2, 8, bytes);
    Signal<bool> clip{true};
    int runs = 0;
    f.services.mount(Content{[&] {
        Card(CardProps{}.clipContent(clip).layout(LayoutStyle{}.width(dp(100)).height(dp(80))), CardContent{[&] {
                 ++runs;
                 Input(InputProps{}.defaultValue(u8"Glyph atlas and caret"));
                 Image(ImageProps{}.source(ImageSource{pixels}).layout(LayoutStyle{}.width(dp(24)).height(dp(24))));
             }});
    }});
    const auto card = f.services.card().mounted().front();
    ThemeConfig theme;
    theme.card.tokens.background = Color{1, 1, 1, 1};
    theme.card.tokens.border = Color{0, 0, 1, 1};
    f.services.components().theme_scope(card)->update(theme);
    f.synchronize(128 / scale, {0, 0, 128 / scale, 96 / scale});
    const auto editor = f.inputs.mounted_inputs().front().editor;
    SceneResources resources(renderer);
    for (const bool value : {true, false, true}) {
        clip.set(value);
        f.synchronize(128 / scale, {0, 0, 128 / scale, 96 / scale});
        const auto& order = f.services.scene_composer().ordered_scene();
        require(resources.synchronize({&f.services.surfaces().instances(),
                                       f.scene.atlas(),
                                       f.scene.glyph_scene().instances(),
                                       &f.services.rounded_effects(),
                                       {128, 96, scale},
                                       &f.services.images(),
                                       &order}),
                "real Card upload");
        require(renderer.attach_scene(resources.attach(order)), "real Card attach");
        SceneRgbaReadback frame;
        require(renderer.read_frame_rgba(frame), renderer.last_error());
        require(runs == 1 && f.inputs.mounted_inputs().front().editor == editor, "real Card toggle remounted content");
        if (value) {
            const auto bounds = f.nodes.require(f.services.components().root(card)).bounds;
            const auto center = static_cast<unsigned>((bounds.x + bounds.width / 2) * scale);
            // The straight top edge is independent of the rounded SDF. White
            // background plus a blue 1dp inset border must retain its width.
            for (unsigned y = 0; y < static_cast<unsigned>(std::ceil(2 * scale)); ++y) {
                const double depth = y + .5 - double(bounds.y) * scale;
                const double t = std::clamp(depth - scale + .5, 0.0, 1.0);
                const int white = quantize(t * t * (3 - 2 * t));
                pixel(frame, center, y, {white, white, 255, 255}, 2);
            }
        }
    }
}
namespace reference = ryn_test::composition_reference;

reference::Image from_readback(const SceneRgbaReadback& frame) {
    reference::Image result(std::size_t(frame.width) * frame.height);
    for (unsigned y = 0; y < frame.height; ++y) {
        for (unsigned x = 0; x < frame.width; ++x) {
            for (unsigned c = 0; c < 4; ++c) {
                result[std::size_t(y) * frame.width + x][c] =
                    std::to_integer<unsigned char>(frame.pixels[std::size_t(y) * frame.row_pitch + x * 4 + c]) / 255.0;
            }
        }
    }
    return result;
}

void compare(const SceneRgbaReadback& frame, const reference::Image& expected, int tolerance = 3) {
    // At <=513 taps, coefficient/FP32 accumulation error is <0.01 byte.
    // A rounding-boundary flip can cost one byte per Gaussian pass and one
    // replacement write. Three is the fixed format-derived bound.
    for (unsigned y = 0; y < frame.height; ++y) {
        for (unsigned x = 0; x < frame.width; ++x) {
            std::array<int, 4> p;
            for (unsigned c = 0; c < 4; ++c) {
                p[c] = quantize(expected[std::size_t(y) * frame.width + x][c]);
            }
            pixel(frame, x, y, p, tolerance);
        }
    }
}

reference::Image apply_filter(const reference::Image& input, unsigned width, unsigned height,
                              const SceneCompositionRequest& request, float scale) {
    if (request.activation == 0) {
        return input;
    }
    auto filtered = reference::gaussian(input, width, height, double(request.sigma) * scale);
    const auto t = request.tint;
    const reference::Pixel tint{double(t.red) * t.alpha, double(t.green) * t.alpha, double(t.blue) * t.alpha, t.alpha};
    reference::Image result = input;
    for (unsigned y = 0; y < height; ++y) {
        for (unsigned x = 0; x < width; ++x) {
            const double px = x + .5;
            const double py = y + .5;
            const auto h = request.hard_clip;
            if (px < double(h.x) * scale || py < double(h.y) * scale || px >= (double(h.x) + h.width) * scale ||
                py >= (double(h.y) + h.height) * scale) {
                continue;
            }
            const double m = coverage(px, py, request.shape, scale) * request.activation;
            result[std::size_t(y) * width + x] = reference::replacement(input[std::size_t(y) * width + x],
                                                                        filtered[std::size_t(y) * width + x], tint, m);
        }
    }
    return result;
}

void nested_masks_and_shadows(SdlSceneRenderer& renderer, PlatformState& platform) {
    renderer.set_clear_color(Color{0, 0, 0, 0});
    RawScene f(renderer, platform);
    const runtime::Rect full{0, 0, float(f.metrics.pixel_width), float(f.metrics.pixel_height)};
    const runtime::RoundedContentClip outer{{12.25F, 10.5F, 70, 58}, 14};
    const runtime::RoundedContentClip inner{{14.5F, 8.75F, 65, 62}, 15};
    (void)f.effects.add(make_shadow_effect({full, 0}, {ShadowKind::outer, {}, 0, 0, Color{1, 0, 0, .5F}}));
    f.order.append_command({SceneDrawKind::rounded_effect, 0, 1});
    auto p = std::make_shared<SceneCompositionProgram>();
    p->scene_revision = f.order.revision();
    p->requests = {{{1, 1}, outer, full}, {{2, 1}, inner, full}};
    p->operations = {{SceneCompositionOpKind::BeginMaskedGroup, 0, 0, 0},
                     {SceneCompositionOpKind::BeginMaskedGroup, 0, 0, 1},
                     {SceneCompositionOpKind::DrawRange, 0, 1},
                     {SceneCompositionOpKind::EndMaskedGroup, 0, 0, 1},
                     {SceneCompositionOpKind::EndMaskedGroup, 0, 0, 0}};
    f.order.set_composition(p);
    const auto frame = f.read();
    for (unsigned y = 0; y < frame.height; ++y) {
        for (unsigned x = 0; x < frame.width; ++x) {
            const double child = reference::quantized((128.0 / 255) * coverage(x + .5, y + .5, inner, 1));
            const int a = quantize(child * coverage(x + .5, y + .5, outer, 1));
            pixel(frame, x, y, {a, 0, 0, a}, 2);
        }
    }
    RawScene shadows(renderer, platform);
    (void)shadows.effects.add(
        make_shadow_effect({outer.bounds, outer.radius}, {ShadowKind::outer, {}, 8, 0, Color{0, 0, 1, .7F}}));
    (void)shadows.effects.add(
        make_shadow_effect({{8, 20, 24, 24}, 4}, {ShadowKind::outer, {}, 6, 0, Color{0, 1, 0, .7F}}));
    shadows.order.append_command({SceneDrawKind::rounded_effect, 0, 1});
    shadows.order.barrier();
    shadows.order.append_command({SceneDrawKind::rounded_effect, 1, 1});
    p = std::make_shared<SceneCompositionProgram>();
    p->scene_revision = shadows.order.revision();
    p->requests = {{{3, 1}, outer, full}};
    p->operations = {{SceneCompositionOpKind::DrawRange, 0, 1},
                     {SceneCompositionOpKind::BeginMaskedGroup, 0, 0, 0},
                     {SceneCompositionOpKind::DrawRange, 1, 1},
                     {SceneCompositionOpKind::EndMaskedGroup, 0, 0, 0}};
    shadows.order.set_composition(p);
    const auto image = shadows.read();
    const auto outside = std::size_t(30) * image.row_pitch + 10 * 4;
    require(std::to_integer<unsigned char>(image.pixels[outside + 2]) > 0 && image.pixels[outside + 1] == std::byte{0},
            "own outer shadow disappeared or child shadow escaped rounded mask");
    const auto inside = std::size_t(30) * image.row_pitch + 22 * 4;
    require(std::to_integer<unsigned char>(image.pixels[inside + 1]) > 0, "real child shadow missing inside mask");
}

void backdrop_numerics(SdlSceneRenderer& renderer, PlatformState& platform) {
    renderer.set_clear_color(Color{0, 0, 0, 0});
    for (unsigned pattern = 0; pattern < 4; ++pattern) {
        for (const float scale : {.75F, 1.25F, 2.0F}) {
            RawScene f(renderer, platform, scale);
            const auto w = f.metrics.pixel_width;
            const auto h = f.metrics.pixel_height;
            std::vector<std::byte> bytes(std::size_t(w) * h * 4);
            reference::Image initial(std::size_t(w) * h);
            const auto generate = [&](bool updated) {
                for (unsigned y = 0; y < h; ++y) {
                    for (unsigned x = 0; x < w; ++x) {
                        const bool high = ((x + y + unsigned(updated)) % 2) == 0;
                        std::array<unsigned, 4> value{255, 64, 128, 128};
                        if (pattern == 1) {
                            value = {high ? 255U : 0U, high ? 0U : 255U, 64, 255};
                        }
                        if (pattern == 2) {
                            value = {x == w / 2 && y == h / 2 ? 255U : 0U, 0, 0, 255};
                        }
                        if (pattern == 3) {
                            value = {x % 4 == 0 ? 255U : 0U, x % 4 == 1 ? 255U : 0U, x % 4 == 2 ? 255U : 0U, 128};
                        }
                        if (updated && pattern != 1) {
                            value[1] = 255 - value[1];
                        }
                        auto& expected = initial[std::size_t(y) * w + x];
                        for (unsigned c = 0; c < 4; ++c) {
                            bytes[(std::size_t(y) * w + x) * 4 + c] = std::byte(value[c]);
                            expected[c] =
                                c == 3 ? value[c] / 255.0 : reference::quantized(value[c] / 255.0 * (value[3] / 255.0));
                        }
                    }
                }
            };
            generate(false);
            ImageInstance image{ImagePixels::copy(w, h, w * 4, bytes),
                                {0, 0, float(w) / scale, float(h) / scale},
                                {0, 0, float(w) / scale, float(h) / scale},
                                {}};
            const auto id = f.images.create(image);
            f.order.append_command({SceneDrawKind::image, id.index, 1, id.generation});
            const auto baseline = f.read();
            compare(baseline, initial, 1);
            for (const float sigma : {0.0F, 1.75F, 4.0F}) {
                for (const float activation : {0.0F, 1.0F}) {
                    auto p = std::make_shared<SceneCompositionProgram>();
                    p->scene_revision = f.order.revision();
                    // Partial rounded output exercises source halo, hard output clip and clamp-to-window independently.
                    SceneCompositionRequest request{
                        {7, 1},
                        {{5.25F / scale, 3.5F / scale, float(w - 12) / scale, float(h - 9) / scale}, 7.25F / scale},
                        {0, 0, float(w) / scale, float(h) / scale},
                        true,
                        sigma,
                        {0, 0, 0, 0},
                        activation};
                    if (pattern == 0) {
                        request.shape = {{0, 0, float(w) / scale, float(h) / scale}, 0};
                    }
                    p->requests = {request};
                    p->operations = {{SceneCompositionOpKind::DrawRange, 0, 1},
                                     {SceneCompositionOpKind::Backdrop, 0, 0, 0}};
                    f.order.set_composition(p);
                    const auto frame = f.read();
                    compare(frame, apply_filter(initial, w, h, request, scale));
                    if (sigma == 0 || activation == 0) {
                        require(frame.pixels == baseline.pixels, "sigma0/m0 must be exact identity");
                    }
                    if (pattern == 0) {
                        pixel(frame, w / 2, h / 2, {128, 32, 64, 128}, 1);
                    } // F over B would produce alpha192.
                }
            }
            generate(true);
            image.pixels = ImagePixels::copy(w, h, w * 4, bytes);
            require(f.images.update(id, image), "late/background Image revision");
            auto p = std::make_shared<SceneCompositionProgram>(*f.order.composition());
            p->requests[0].tint = {.2F, .4F, .7F, .25F};
            p->requests[0].activation = .65F;
            f.order.set_composition(p);
            compare(f.read(), apply_filter(initial, w, h, p->requests[0], scale));
        }
    }
}

void multiple_prefixes(SdlSceneRenderer& renderer, PlatformState& platform) {
    RawScene f(renderer, platform);
    const auto w = f.metrics.pixel_width;
    const auto h = f.metrics.pixel_height;
    std::vector<std::byte> bytes(std::size_t(w) * h * 4);
    reference::Image initial(std::size_t(w) * h);
    for (unsigned y = 0; y < h; ++y) {
        for (unsigned x = 0; x < w; ++x) {
            const unsigned c = (x + y) % 2 ? 255 : 0;
            for (unsigned k = 0; k < 4; ++k) {
                const unsigned v = k == 3 ? 128 : c;
                bytes[(std::size_t(y) * w + x) * 4 + k] = std::byte(v);
                initial[std::size_t(y) * w + x][k] =
                    k == 3 ? 128.0 / 255 : reference::quantized(c / 255.0 * (128.0 / 255));
            }
        }
    }
    const auto image = f.images.create(
        {ImagePixels::copy(w, h, w * 4, bytes), {0, 0, float(w), float(h)}, {0, 0, float(w), float(h)}, {}});
    (void)f.effects.add(make_shadow_effect({{30, 16, 1, 40}, 0}, {ShadowKind::outer, {}, 0, 0, Color{0, 1, 0}}));
    (void)f.effects.add(make_shadow_effect({{44, 12, 1, 48}, 0}, {ShadowKind::outer, {}, 0, 0, Color{1, 0, 0}}));
    f.order.append_command({SceneDrawKind::image, image.index, 1, image.generation});
    f.order.append_command({SceneDrawKind::rounded_effect, 0, 1});
    f.order.barrier();
    f.order.append_command({SceneDrawKind::rounded_effect, 1, 1});
    auto p = std::make_shared<SceneCompositionProgram>();
    p->scene_revision = f.order.revision();
    const runtime::Rect full{0, 0, float(w), float(h)};
    p->requests = {{{1, 1}, {full, 0}, full, true, 2, {0, 0, 0, .15F}, 1},
                   {{2, 1}, {full, 0}, full, true, 1, {0, 0, 0, 0}, 1}};
    p->operations = {{SceneCompositionOpKind::DrawRange, 0, 1},
                     {SceneCompositionOpKind::Backdrop, 0, 0, 0},
                     {SceneCompositionOpKind::DrawRange, 1, 1},
                     {SceneCompositionOpKind::Backdrop, 0, 0, 1},
                     {SceneCompositionOpKind::DrawRange, 2, 1}};
    f.order.set_composition(p);
    auto expected = apply_filter(initial, w, h, p->requests[0], 1);
    for (unsigned y = 16; y < 56; ++y) {
        expected[std::size_t(y) * w + 30] = {0, 1, 0, 1};
    }
    expected = apply_filter(expected, w, h, p->requests[1], 1);
    for (unsigned y = 12; y < 60; ++y) {
        expected[std::size_t(y) * w + 44] = {1, 0, 0, 1};
    }
    const auto frame = f.read();
    compare(frame, expected);
    pixel(frame, 44, 30, {255, 0, 0, 255}, 0); // This dialog and higher layers were never in either prefix.
}

void service_readback(SdlSceneRenderer& renderer) {
    ryn_test::input_component::Fixture f;
    AppRef app;
    f.services.mount(Content{[&] {
        App(AppProps{}.ref(app), AppContent{[] { Input(InputProps{}.defaultValue(u8"Real background glyph atlas")); }});
    }});
    const auto root = f.services.components().root_components().front();
    ThemeConfig theme;
    theme.modal.tokens.mask = Color{0, 0, 0, 0};
    theme.modal.tokens.shadow = ShadowList{};
    f.services.components().theme_scope(root)->update(theme);
    const auto sync = [&] {
        require(f.services.layout_and_synchronize({128, 96}, {0, 0, 128, 96}), "service native layout");
    };
    SceneResources resources(renderer);
    const auto read = [&] {
        sync();
        const auto& scene = f.services.scene_composer().ordered_scene();
        require(resources.synchronize({&f.services.surfaces().instances(),
                                       f.scene.atlas(),
                                       f.scene.glyph_scene().instances(),
                                       &f.services.rounded_effects(),
                                       {128, 96, 1},
                                       &f.services.images(),
                                       &scene}),
                "service native upload");
        require(renderer.attach_scene(resources.attach(scene)), "service native attach");
        SceneRgbaReadback frame;
        require(renderer.read_frame_rgba(frame), renderer.last_error());
        return frame;
    };
    const auto baseline = read();
    const auto handle =
        app.modal().confirm(ModalProps{}.mask(ModalMask{true, true, true}).centered(true).width(64).title(u8"Sharp"),
                            ModalContent{[] { Text(u8"Dialog"); }});
    const auto frame = read();
    const auto modal = f.services.modal().mounted().back();
    const auto bounds = f.services.modal().snapshot(modal).bounds;
    const auto expected = reference::gaussian(from_readback(baseline), 128, 96, 4);
    unsigned checked = 0;
    for (unsigned y = 0; y < 96; ++y) {
        for (unsigned x = 0; x < 128; ++x) {
            if (x + .5 >= bounds.x - 2 && x + .5 < bounds.x + bounds.width + 2 && y + .5 >= bounds.y - 2 &&
                y + .5 < bounds.y + bounds.height + 2) {
                continue;
            }
            std::array<int, 4> e;
            for (unsigned c = 0; c < 4; ++c) {
                e[c] = quantize(expected[std::size_t(y) * 128 + x][c]);
            }
            pixel(frame, x, y, e, 3);
            ++checked;
        }
    }
    require(checked > 100, "service native oracle did not inspect backdrop pixels");
    require(handle.destroy() && handle.closed().wait_for(std::chrono::seconds(0)) == std::future_status::ready,
            "service close waited for GPU lease");
    const auto restored = read();
    require(restored.pixels == baseline.pixels, "service cleanup left backdrop contents/request");
}

} // namespace

int main(int argc, char** argv) {
    try {
        no_dialogs();
        require(argc == 2, "expected actual GPU driver argument");
        PlatformConfig config;
        config.title = "RynUI hidden composition readback";
        config.width = 128;
        config.height = 96;
        config.hidden = true;
        config.high_pixel_density = false;
        auto platform = PlatformState::create(config);
        require(bool(platform), "hidden platform");
        SdlSceneRenderer renderer(*platform.state);
        require(std::string(renderer.gpu_driver()) == argv[1], "actual GPU driver differs");
        const auto flags = SDL_GetWindowFlags(static_cast<SDL_Window*>(platform.state->window()));
        require((flags & SDL_WINDOW_HIDDEN) != 0 && (flags & SDL_WINDOW_INPUT_FOCUS) == 0,
                "test window took desktop focus");
        masks(renderer, *platform.state);
        for (const float scale : {.75F, 1.0F, 1.25F, 1.5F, 2.0F}) {
            real_card(renderer, scale);
        }
        nested_masks_and_shadows(renderer, *platform.state);
        backdrop_numerics(renderer, *platform.state);
        multiple_prefixes(renderer, *platform.state);
        service_readback(renderer);
        std::cout << "composition native RGBA passed, driver=" << renderer.gpu_driver() << '\n';
    } catch (const std::exception& e) {
        std::cerr << e.what() << '\n';
        return 1;
    }
}
