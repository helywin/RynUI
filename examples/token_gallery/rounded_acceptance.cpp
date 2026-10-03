#include "acceptance_events.hpp"
#include "token_gallery_definition.hpp"
#include "component/button_component.hpp"
#include "component/input_component.hpp"
#include "platform/default_font_chain.hpp"
#include "platform/sdl/platform_state.hpp"
#include "renderer/common/rounded_effect_packing.hpp"
#include "renderer/common/scene_resources.hpp"
#include "renderer/sdl/scene_renderer.hpp"

#include <SDL3/SDL.h>
#include <ryn/rynui.hpp>

#include <algorithm>
#include <cmath>
#include <filesystem>
#include <fstream>
#include <iostream>

namespace rynui::example {
namespace {
using namespace ryn;

void require_rounded(bool value, const char* message) {
    if (!value) {
        throw std::runtime_error(message);
    }
}

struct CoverageCase final {
    std::string name;
    graphics::LogicalRoundedRect shape;
    std::vector<graphics::RoundedEffectInstance> layers;
};

void capture_coverage(detail::PlatformState& platform, detail::SdlSceneRenderer& renderer,
                      const std::filesystem::path& directory, float scale, bool dark) {
    const auto metrics = platform.window_metrics();
    const detail::SceneDeviceMetrics device{static_cast<std::uint32_t>(metrics.pixel_width),
                                            static_cast<std::uint32_t>(metrics.pixel_height), scale};
    const Color background = dark ? Color::rgba8(24, 24, 24) : Color::rgba8(255, 255, 255);
    const Color ink = dark ? Color::rgba8(235, 235, 235) : Color::rgba8(20, 20, 20);
    std::vector<CoverageCase> cases;
    const auto add = [&](std::string name, std::size_t cell, std::vector<graphics::RoundedEffectInstance> layers) {
        const graphics::LogicalRoundedRect shape{{20.25F + (cell % 5) * 145, 30.5F + (cell / 5) * 100, 110.5F, 56.5F},
                                                 10.25F};
        for (auto& layer : layers) {
            layer.geometry.shape.rect = shape.rect;
            layer.geometry.shape.radius = shape.radius;
        }
        cases.push_back({std::move(name), shape, std::move(layers)});
    };
    const graphics::LogicalRoundedRect placeholder{{0, 0, 110.5F, 56.5F}, 10.25F};
    add("fill", 0, {graphics::make_shadow_effect(placeholder, {ShadowKind::outer, {}, 0, 0, ink})});
    add("thin-border", 1, {graphics::make_outline_effect(placeholder, 1, 0, ink)});
    add("inset", 2, {graphics::make_shadow_effect(placeholder, {ShadowKind::inset, {2, -1}, 4, 2, ink})});
    // All 16 corner masks exercise single rounded corners, joined sides and diagonal masks.
    for (unsigned mask = 0; mask < 16; ++mask) {
        const std::size_t cell = mask + 5;
        const graphics::LogicalRoundedRect shape{{20.25F + (cell % 5) * 145, 30.5F + (cell / 5) * 100, 110.5F, 56.5F},
                                                 10.25F};
        std::array<bool, 4> corners;
        for (unsigned corner = 0; corner < 4; ++corner) {
            corners[corner] = (mask & (1U << corner)) != 0;
        }
        const auto fills = graphics::make_corner_fill_effects(shape, corners, ink);
        cases.push_back({"corners-" + std::to_string(mask), shape, {fills.begin(), fills.end()}});
        if (mask == 9 || mask == 6) {
            const auto next_cell = mask == 9 ? 23U : 24U;
            const graphics::LogicalRoundedRect border_shape{
                {20.25F + (next_cell % 5) * 145, 30.5F + (next_cell / 5) * 100, 110.5F, 56.5F}, 10.25F};
            const auto borders = graphics::make_corner_outline_effects(border_shape, corners, 1, 0, ink);
            cases.push_back({"joined-border-" + std::to_string(mask), border_shape, {borders.begin(), borders.end()}});
        }
    }
    const graphics::LogicalRoundedRect translated{{310.25F, 430.5F, 110.5F, 56.5F}, 10.25F};
    cases.push_back({"translated-clip",
                     translated,
                     {graphics::make_shadow_effect(translated, {ShadowKind::outer, {}, 0, 0, ink}, {.25F, 1.25F},
                                                   graphics::EffectClip{99, {315, 420, 120, 80}})}});
    graphics::RoundedEffectStore effects;
    graphics::QuadInstanceStore quads;
    graphics::GlyphAtlas atlas;
    graphics::GlyphInstanceStore glyphs;
    graphics::OrderedScene ordered;
    for (const auto& item : cases) {
        static_cast<void>(effects.add_batch(item.layers));
    }
    require_rounded(effects.compact({0, 0, metrics.pixel_width / scale, metrics.pixel_height / scale}),
                    "Coverage compact");
    ordered.append_command(
        {graphics::SceneDrawKind::rounded_effect, 0, static_cast<std::uint32_t>(effects.packed_instances().size())});
    const graphics::QuadInstance quad{
        {600.25F, 30.5F, 110.5F, 56.5F}, {ink.red(), ink.green(), ink.blue(), ink.alpha()}, 1, 10.25F};
    static_cast<void>(quads.append(std::span{&quad, 1}));
    ordered.append_quad(0, 1);
    detail::SceneResources resources{renderer};
    require_rounded(resources.synchronize({&quads, atlas, glyphs, &effects, device}), "Coverage upload");
    require_rounded(renderer.attach_scene(resources.attach(ordered)), "Coverage attach");
    renderer.set_clear_color(background);
    for (int frame = 0; frame < 2; ++frame) {
        static_cast<void>(platform.poll_events());
        require_rounded(renderer.submit_frame(animation::AnimationTime{}) != runtime::FrameSubmissionResult::failed,
                        "Coverage GPU frame");
        platform.delay(25);
    }
    const std::string name = dark ? "coverage-dark" : "coverage-light";
    require_rounded(renderer.save_frame_bmp(directory / (name + ".bmp")), "Coverage GPU readback");
    std::ofstream pixels(directory / (name + ".rgb"), std::ios::binary);
    std::ofstream regions(directory / (name + ".csv"));
    require_rounded(bool(pixels) && bool(regions), "Coverage reference output");
    regions << "name,x,y,width,height,offset,partial\n";
    std::size_t offset{};
    for (const auto& item : cases) {
        std::vector<detail::RoundedEffectGpuInstance> packed;
        for (const auto& layer : item.layers) {
            packed.push_back(detail::pack_rounded_effect_instance(layer, device));
        }
        const int x0 = int(std::floor(item.shape.rect.x * scale)) - 3;
        const int y0 = int(std::floor(item.shape.rect.y * scale)) - 3;
        const int width = int(std::ceil((item.shape.rect.width + 7) * scale));
        const int height = int(std::ceil((item.shape.rect.height + 7) * scale));
        std::size_t partial{};
        for (int y = y0; y < y0 + height; ++y) {
            for (int x = x0; x < x0 + width; ++x) {
                const runtime::Point point{x + .5F, y + .5F};
                std::array<float, 3> color{background.red(), background.green(), background.blue()};
                for (const auto& layer : packed) {
                    // Match the rasterizer's exclusive right/bottom ownership at quadrant split lines.
                    if (point.x < layer.clip_bounds[0] || point.y < layer.clip_bounds[1] ||
                        point.x >= layer.clip_bounds[2] || point.y >= layer.clip_bounds[3]) {
                        continue;
                    }
                    const auto fragment = detail::rounded_effect_gpu_fragment_reference(point, layer);
                    partial += fragment[3] > .01F && fragment[3] < .99F ? 1U : 0U;
                    for (std::size_t channel = 0; channel < color.size(); ++channel) {
                        color[channel] = fragment[channel] * fragment[3] + color[channel] * (1 - fragment[3]);
                    }
                }
                for (const auto channel : color) {
                    pixels.put(static_cast<char>(std::lround(std::clamp(channel, 0.0F, 1.0F) * 255)));
                }
            }
        }
        regions << item.name << ',' << x0 << ',' << y0 << ',' << width << ',' << height << ',' << offset << ','
                << partial << '\n';
        offset += std::size_t(width) * height * 3;
    }
    require_rounded(bool(pixels) && bool(regions), "Coverage reference write");
    resources.retire();
}
} // namespace

int run_rounded_acceptance(int argc, char** argv) {
    using namespace ryn;
    try {
        std::optional<float> requested_scale;
        std::filesystem::path directory;
        for (int index = 1; index < argc; ++index) {
            const std::string_view argument = argv[index];
            if (argument.starts_with("--acceptance-scale=")) {
                requested_scale = std::stof(std::string{argument.substr(19)});
            }
            if (argument.starts_with("--evidence-dir=")) {
                directory = argument.substr(15);
            }
        }
        require_rounded(!directory.empty(), "Rounded requires --evidence-dir");
        std::filesystem::create_directories(directory);
        detail::PlatformConfig config;
        config.title = "RynUI Rounded AA and Button Wave Acceptance";
        config.width = 1600;
        config.height = 1100;
        auto created = detail::PlatformState::create(config);
        require_rounded(bool(created), "Rounded window");
        auto& platform = *created.state;
        acceptance::FixtureEventFilter filter{true};
        auto metrics = platform.window_metrics();
        const float scale = requested_scale.value_or(metrics.display_scale);
        require_rounded(std::isfinite(scale) && scale > 0, "Rounded scale");
        const auto executable = std::filesystem::absolute(argv[0]).parent_path();
        detail::SdlSceneRenderer renderer{platform, executable / "shaders"};
        capture_coverage(platform, renderer, directory, scale, false);
        capture_coverage(platform, renderer, directory, scale, true);
        auto font_result = font::FontRuntime::create();
        require_rounded(bool(font_result), "Rounded fonts");
        auto fonts = std::move(font_result.runtime);
        detail::DefaultFontChainRequest request;
        request.raster = {14, scale};
        request.fallback_latin = executable / "fonts/latin.ttf";
        request.fallback_cjk = executable / "fonts/cjk.otf";
        auto chain = detail::load_default_ui_font_chain(*fonts, request);
        require_rounded(bool(chain) && chain.uses_system_fonts, "Rounded system fonts");
        const auto resolver = detail::make_default_ui_font_resolver(*fonts, chain, scale);
        runtime::NodeStore nodes;
        runtime::FrameRequestState frames;
        runtime::DirtyQueues dirty{nodes, &frames};
        layout::LayoutEngine layout{nodes};
        text::TextEngine engine{*fonts};
        auto scene_owner = std::make_unique<detail::TextSceneService>(*fonts, engine, frames);
        auto& scene = *scene_owner;
        detail::WindowComponentServices services{nodes, layout, dirty, scene, resolver, frames};
        detail::ButtonComponentHost buttons{services};
        detail::InputComponentHost inputs{services, platform, platform};
        inputs.set_display_scale(scale);
        Signal<ThemeConfig> theme{ThemeConfig{}};
        Signal<LogicalLength> width{dp(metrics.pixel_width / scale - 32)};
        TextAreaRef area_ref;
        std::array<ButtonRef, 6> refs;
        int content_runs{};
        int clicks{};
        services.mount(Content{[&] {
            ++content_runs;
            Theme(
                ThemeProps{}.config(theme), ThemeContent{[&] {
                    Flex(
                        FlexProps{}
                            .vertical(true)
                            .align(FlexAlign::Start)
                            .gap(dp(20))
                            .layout(LayoutStyle{}.width(width)),
                        FlexContent{[&] {
                            Text(u8"圆角 AA / 单侧圆角 / Compact 外侧角 / Button wave");
                            TextArea(TextAreaProps{}
                                         .defaultValue(u8"autoSize 随编辑与宽度换行扩展\n保留空行\n\n")
                                         .autoSize(TextAreaAutoSize{true, 3, 6})
                                         .allowClear(true)
                                         .autoFocus(true)
                                         .ref(area_ref)
                                         .layout(LayoutStyle{}.width(dp(420))));
                            Flex(FlexProps{}.gap(dp(24)), FlexContent{[&] {
                                     Button(ButtonProps{}.ref(refs[0]).onClick([&] { ++clicks; }),
                                            ButtonContent{[] { Text(u8"Outlined"); }});
                                     Button(ButtonProps{}.type(ButtonType::Primary).ref(refs[1]),
                                            ButtonContent{[] { Text(u8"Primary"); }});
                                     Button(ButtonProps{}.shape(ButtonShape::Round).ref(refs[2]),
                                            ButtonContent{[] { Text(u8"Round"); }});
                                     Button(ButtonProps{}.shape(ButtonShape::Circle).ref(refs[3]),
                                            ButtonContent{[] { Text(u8"○"); }});
                                 }});
                            SpaceCompact(
                                SpaceCompactProps{}, SpaceCompactContent{[&] {
                                    Button(ButtonProps{}.ref(refs[4]), ButtonContent{[] { Text(u8"左圆 / 右直"); }});
                                    Button(ButtonProps{}.ref(refs[5]), ButtonContent{[] { Text(u8"左直 / 右圆"); }});
                                }});
                            SpaceCompact(
                                SpaceCompactProps{}.orientation(SpaceOrientation::Vertical), SpaceCompactContent{[] {
                                    Input(InputProps{}.defaultValue(u8"上圆下直").layout(LayoutStyle{}.width(dp(260))));
                                    Input(InputProps{}.defaultValue(u8"上直下圆").layout(LayoutStyle{}.width(dp(260))));
                                }});
                            SpaceCompact(
                                SpaceCompactProps{}, SpaceCompactContent{[] {
                                    SpaceAddon(SpaceAddonProps{}, SpaceAddonContent{[] { Text(u8"https://"); }});
                                    Input(InputProps{}.defaultValue(u8"单侧圆角").layout(LayoutStyle{}.width(dp(260))));
                                    SpaceAddon(SpaceAddonProps{}, SpaceAddonContent{[] { Text(u8".com"); }});
                                }});
                        }});
                }});
        }});
        detail::SceneResources resources{renderer};
        std::int64_t now_us{};
        const auto draw = [&](const std::string& name, std::int64_t time) {
            now_us = time;
            for (int settle = 0; settle < 2; ++settle) {
                static_cast<void>(platform.poll_events());
                metrics = platform.window_metrics();
                const runtime::Size viewport{metrics.pixel_width / scale, metrics.pixel_height / scale};
                width.set(dp(viewport.width - 32));
                const auto now = animation::AnimationTime::microseconds(now_us);
                static_cast<void>(services.tick_animations(now));
                require_rounded(
                    services.layout_and_synchronize(viewport, {0, 0, viewport.width, viewport.height}, {16, 12}),
                    "Rounded layout");
                require_rounded(resources.synchronize({&services.surfaces().instances(),
                                                       scene.atlas(),
                                                       scene.glyph_scene().instances(),
                                                       &services.rounded_effects(),
                                                       {static_cast<std::uint32_t>(metrics.pixel_width),
                                                        static_cast<std::uint32_t>(metrics.pixel_height), scale}}),
                                "Rounded scene upload");
                require_rounded(renderer.attach_scene(resources.attach(services.scene_composer().ordered_scene())),
                                "Rounded scene attach");
                renderer.set_clear_color(resolve_theme(theme.get()).alias().color_background_container);
                require_rounded(renderer.submit_frame(now) != runtime::FrameSubmissionResult::failed,
                                "Rounded UI frame");
                platform.delay(25);
            }
            if (!name.empty()) {
                require_rounded(renderer.save_frame_bmp(directory / (name + ".bmp")), "Rounded UI readback");
            }
        };
        std::size_t normalized{};
        const auto key = [&] {
            for (const auto type : {SDL_EVENT_KEY_DOWN, SDL_EVENT_KEY_UP}) {
                SDL_Event event{};
                event.type = type;
                event.common.timestamp = acceptance::fixture_timestamp;
                event.key.windowID = SDL_GetWindowID(static_cast<SDL_Window*>(platform.window()));
                event.key.key = SDLK_RETURN;
                event.key.down = type == SDL_EVENT_KEY_DOWN;
                require_rounded(SDL_PushEvent(&event), "Rounded key injection");
                for (const auto& input : platform.poll_events().input.events()) {
                    if (const auto* keyboard = std::get_if<input::KeyboardInputEvent>(&input)) {
                        services.focus().dispatch(*keyboard);
                        ++normalized;
                    }
                }
            }
        };
        draw("", 300000);
        require_rounded(area_ref.blur() && area_ref.focus(), "Rounded light TextArea focus");
        draw("textarea-light", 600000);
        const std::vector<detail::MountedButtonComponent> mounted{buttons.mounted_buttons().begin(),
                                                                  buttons.mounted_buttons().end()};
        require_rounded(mounted.size() == refs.size(), "Rounded button inventory");
        const auto click = [&](runtime::Point point) {
            for (const auto type : {SDL_EVENT_MOUSE_MOTION, SDL_EVENT_MOUSE_BUTTON_DOWN, SDL_EVENT_MOUSE_BUTTON_UP}) {
                SDL_Event event{};
                event.type = type;
                event.common.timestamp = acceptance::fixture_timestamp;
                const auto window_id = SDL_GetWindowID(static_cast<SDL_Window*>(platform.window()));
                if (type == SDL_EVENT_MOUSE_MOTION) {
                    event.motion.windowID = window_id;
                    event.motion.x = point.x * scale / metrics.pixel_density;
                    event.motion.y = point.y * scale / metrics.pixel_density;
                } else {
                    event.button.windowID = window_id;
                    event.button.button = SDL_BUTTON_LEFT;
                    event.button.down = type == SDL_EVENT_MOUSE_BUTTON_DOWN;
                    event.button.x = point.x * scale / metrics.pixel_density;
                    event.button.y = point.y * scale / metrics.pixel_density;
                }
                require_rounded(SDL_PushEvent(&event), "Rounded pointer injection");
                for (const auto& input : platform.poll_events().input.events()) {
                    if (const auto* pointer = std::get_if<input::PointerInputEvent>(&input)) {
                        auto logical = *pointer;
                        logical.x *= metrics.display_scale / scale;
                        logical.y *= metrics.display_scale / scale;
                        services.pointer().dispatch(logical);
                        ++normalized;
                    }
                }
            }
        };
        const auto capture_wave = [&](const std::string& prefix, std::int64_t start, bool pointer) {
            for (std::size_t index = 0; index < refs.size(); ++index) {
                if (pointer) {
                    const auto bounds = nodes.require(mounted[index].node).bounds;
                    click({bounds.x + bounds.width / 2, bounds.y + bounds.height / 2});
                } else {
                    require_rounded(refs[index].focus(), "Rounded Button focus");
                    key();
                }
            }
            for (const std::int64_t elapsed : {0LL, 100000LL, 400000LL, 1000000LL, 2000000LL}) {
                draw(prefix + "-" + std::to_string(elapsed / 1000), start + elapsed);
                const auto easing =
                    animation::resolve_motion_policy(resolve_theme(theme.get()), animation::MotionPreference::normal)
                        .tokens()
                        .easing(animation::MotionEasingToken::ease_out_circ);
                for (const auto& item : mounted) {
                    const auto snapshot = buttons.snapshot(item.component);
                    require_rounded(snapshot.wave_active == (elapsed < 2000000), "Rounded wave fade lifetime");
                    require_rounded(elapsed < 400000 || snapshot.wave_progress == 1, "Rounded wave spread completion");
                    require_rounded(elapsed >= 2000000 || snapshot.wave_fade < 1, "Rounded wave independent fade");
                    require_rounded(std::abs(snapshot.wave_progress -
                                             easing.sample(std::min(1.0F, float(elapsed) / 400000))) < .0001F &&
                                        std::abs(snapshot.wave_fade - easing.sample(float(elapsed) / 2000000)) < .0001F,
                                    "Rounded wave easing/time");
                }
            }
        };
        capture_wave("wave-light", 600000, false);
        ThemeConfig dark;
        dark.algorithms = {ThemeAlgorithm::Dark};
        theme.set(dark);
        require_rounded(area_ref.focus(), "Rounded dark TextArea focus");
        draw("textarea-dark", 3000000);
        capture_wave("wave-dark", 3000000, true);
        require_rounded(refs[0].focus(), "Rounded restart focus");
        key();
        draw("wave-restart", 5100000);
        services.set_motion_preference(animation::MotionPreference::reduced);
        draw("reduced", 5200000);
        require_rounded(!buttons.snapshot(mounted[0].component).wave_active && !services.next_frame_deadline(),
                        "Rounded reduced cancellation");
        services.set_motion_preference(animation::MotionPreference::normal);
        key();
        services.set_window_active(false);
        inputs.set_window_active(false);
        require_rounded(!buttons.snapshot(mounted[0].component).wave_active, "Rounded immediate inactive wave cancel");
        // The pointer hover color may still finish its independent 200ms transition.
        draw("inactive", 5500000);
        require_rounded(!buttons.snapshot(mounted[0].component).wave_active && !services.next_frame_deadline(),
                        "Rounded inactive cancellation");
        require_rounded(SDL_SetWindowSize(static_cast<SDL_Window*>(platform.window()), 1420, 1000), "Rounded resize");
        platform.delay(100);
        draw("resized", 5600000);
        static_cast<void>(frames.consume_request());
        const auto submissions = renderer.counters().frame_submissions;
        for (int poll = 0; poll < 3; ++poll) {
            static_cast<void>(platform.poll_events());
            require_rounded(!frames.pending() && !services.next_frame_deadline() &&
                                renderer.counters().frame_submissions == submissions,
                            "Rounded idle");
        }
        require_rounded(content_runs == 1 && std::ranges::equal(mounted, buttons.mounted_buttons(),
                                                                [](const auto& left, const auto& right) {
                                                                    return left.component == right.component &&
                                                                           left.node == right.node &&
                                                                           left.scene == right.scene;
                                                                }),
                        "Rounded retained identity");
        services.dispose();
        require_rounded(!area_ref.bound() && nodes.size() == 0 && scene.size() == 0 &&
                            services.interactions().size() == 0 && !services.next_frame_deadline(),
                        "Rounded dispose");
        resources.retire();
        scene_owner.reset();
        std::cout << "rounded_acceptance=passed gpu_driver=" << renderer.gpu_driver()
                  << " shader_format=" << renderer.shader_format() << " system_display_scale=" << metrics.display_scale
                  << " render_scale=" << scale << " corner_masks=16 coverage_cases=22 normalized_events=" << normalized
                  << " clicks=" << clicks << " content_runs=" << content_runs << " submits=" << submissions
                  << " wave_times=0,100,400,1000,2000 resize=1420x1000 reduced=passed inactive=passed idle_polls=3"
                     " deadline=none disposed=1 exit_code=0\n";
        return 0;
    } catch (const std::exception& error) {
        std::cerr << "rounded_acceptance_error=" << error.what() << '\n';
        return 1;
    }
}
} // namespace rynui::example

#if defined(RYNUI_ROUNDED_ACCEPTANCE_STANDALONE)
int main(int argc, char** argv) {
    return rynui::example::run_rounded_acceptance(argc, argv);
}
#endif
