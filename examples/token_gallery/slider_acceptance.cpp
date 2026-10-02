#include "component/slider_component.hpp"
#include "platform/default_font_chain.hpp"
#include "platform/sdl/platform_state.hpp"
#include "renderer/common/scene_resources.hpp"
#include "renderer/sdl/scene_renderer.hpp"
#include "token_gallery_definition.hpp"
#include <SDL3/SDL.h>
#include <ryn/rynui.hpp>
#include <filesystem>
#include <iostream>

namespace rynui::example {
namespace {
using namespace ryn;

void require(bool condition, const char* message) {
    if (!condition) {
        throw std::runtime_error(message);
    }
}
} // namespace

int run_slider_acceptance(int argc, char** argv) {
    try {
        std::optional<float> requested_scale;
        std::filesystem::path directory;
        for (int i = 1; i < argc; ++i) {
            const std::string_view arg = argv[i];
            if (arg.starts_with("--acceptance-scale=")) {
                requested_scale = std::stof(std::string{arg.substr(19)});
            }
            if (arg.starts_with("--evidence-dir=")) {
                directory = std::filesystem::path{arg.substr(15)};
            }
        }
        require(!directory.empty(), "Slider acceptance requires --evidence-dir");
        std::filesystem::create_directories(directory);
        detail::PlatformConfig config;
        config.title = "RynUI Slider Acceptance";
        config.width = 1100;
        config.height = 850;
        auto created = detail::PlatformState::create(config);
        require(bool(created), "Slider window creation failed");
        auto& platform = *created.state;
        auto metrics = platform.window_metrics();
        const float scale = requested_scale.value_or(metrics.display_scale);
        require(std::isfinite(scale) && scale > 0, "Slider scale invalid");
        const auto executable = std::filesystem::absolute(argv[0]).parent_path();
        auto font_result = font::FontRuntime::create();
        require(bool(font_result), "Slider font runtime failed");
        auto fonts = std::move(font_result.runtime);
        detail::DefaultFontChainRequest font_request;
        font_request.raster = {14, scale};
        font_request.fallback_latin = executable / "fonts/latin.ttf";
        font_request.fallback_cjk = executable / "fonts/cjk.otf";
        auto chain = detail::load_default_ui_font_chain(*fonts, font_request);
        require(bool(chain), "Slider font chain failed");
        auto resolver = detail::make_default_ui_font_resolver(*fonts, chain, scale);
        runtime::NodeStore nodes;
        runtime::FrameRequestState frames;
        runtime::DirtyQueues dirty{nodes, &frames};
        layout::LayoutEngine layout{nodes};
        text::TextEngine engine{*fonts};
        detail::TextSceneService scene{*fonts, engine, frames};
        detail::WindowComponentServices services{nodes, layout, dirty, scene, resolver, frames};
        Signal<double> value{30};
        Signal<SliderRange> range{SliderRange{20, 80}};
        Signal<bool> disabled{false};
        Signal<ThemeConfig> theme{ThemeConfig{}};
        int changes{};
        int completes{};
        int runs{};
        Signal<LogicalLength> width{dp(350)};
        services.mount(Content{[&] {
            ++runs;
            Theme(ThemeProps{}.config(theme), ThemeContent{[&] {
                      Flex(FlexProps{}.vertical(true).gap(dp(8)), FlexContent{[&] {
                               Text(u8"Slider · 单值 / Range / Reverse / Disabled / Vertical");
                               Slider(SliderProps{}
                                          .value(value)
                                          .disabled(disabled)
                                          .onChange([&](double next) {
                                              ++changes;
                                              value.set(next);
                                          })
                                          .onChangeComplete([&](double) { ++completes; })
                                          .layout(LayoutStyle{}.width(width)));
                               RangeSlider(RangeSliderProps{}
                                               .value(range)
                                               .onChange([&](SliderRange next) {
                                                   ++changes;
                                                   range.set(next);
                                               })
                                               .onChangeComplete([&](SliderRange) { ++completes; })
                                               .layout(LayoutStyle{}.width(width)));
                               Slider(SliderProps{}.defaultValue(35).reverse(true).layout(LayoutStyle{}.width(width)));
                               Slider(SliderProps{}.defaultValue(60).disabled(true).layout(LayoutStyle{}.width(width)));
                               Slider(SliderProps{}
                                          .defaultValue(40)
                                          .orientation(SliderOrientation::Vertical)
                                          .layout(LayoutStyle{}.height(dp(160))));
                           }});
                  }});
        }});
        detail::SdlSceneRenderer renderer{platform, executable / "shaders"};
        detail::SceneResources resources{renderer};
        std::uint64_t frame{};
        std::uint64_t normalized{};
        const auto draw = [&](std::string name) {
            for (int settle = 0; settle < 2; ++settle) {
                static_cast<void>(platform.poll_events());
                metrics = platform.window_metrics();
                const runtime::Size viewport{metrics.pixel_width / scale, metrics.pixel_height / scale};
                require(services.layout_and_synchronize(viewport, {0, 0, viewport.width, viewport.height}, {20, 16}),
                        "Slider layout failed");
                require(resources.synchronize({&services.surfaces().instances(),
                                               scene.atlas(),
                                               scene.glyph_scene().instances(),
                                               &services.rounded_effects(),
                                               {static_cast<std::uint32_t>(metrics.pixel_width),
                                                static_cast<std::uint32_t>(metrics.pixel_height), scale}}),
                        "Slider upload failed");
                require(renderer.attach_scene(resources.attach(services.scene_composer().ordered_scene())),
                        "Slider attachment failed");
                renderer.set_clear_color(resolve_theme(theme.get()).alias().color_background_container);
                require(renderer.submit_frame(animation::AnimationTime::microseconds(++frame * 100000)) !=
                            runtime::FrameSubmissionResult::failed,
                        "Slider frame failed");
                platform.delay(35);
            }
            require(renderer.save_frame_bmp(directory / (name + ".bmp")), "Slider GPU readback failed");
        };
        draw("initial");
        auto& host = services.slider();
        require(host.mounted().size() == 5, "Slider inventory wrong");
        const auto single = host.mounted()[0];
        const auto dual = host.mounted()[1];
        const auto reverse = host.mounted()[2];
        const auto blocked = host.mounted()[3];
        const auto vertical = host.mounted()[4];
        auto* window = static_cast<SDL_Window*>(platform.window());
        const auto window_id = SDL_GetWindowID(window);
        const auto poll = [&] {
            const auto& events = platform.poll_events();
            for (const auto& event : events.input.events()) {
                if (const auto* key = std::get_if<input::KeyboardInputEvent>(&event)) {
                    services.focus().dispatch(*key);
                    ++normalized;
                }
                if (const auto* pointer = std::get_if<input::PointerInputEvent>(&event)) {
                    auto value = *pointer;
                    value.x *= metrics.display_scale / scale;
                    value.y *= metrics.display_scale / scale;
                    services.pointer().dispatch(value);
                    ++normalized;
                }
            }
        };
        const auto key = [&](SDL_Keycode code) {
            for (auto type : {SDL_EVENT_KEY_DOWN, SDL_EVENT_KEY_UP}) {
                SDL_Event event{};
                event.type = type;
                event.key.windowID = window_id;
                event.key.key = code;
                event.key.down = type == SDL_EVENT_KEY_DOWN;
                require(SDL_PushEvent(&event), "Slider key injection failed");
                poll();
            }
        };
        require(services.focus().request_focus(single.thumbs[0], input::FocusModality::keyboard),
                "Slider native focus failed");
        key(SDLK_UP);
        key(SDLK_PAGEUP);
        key(SDLK_PAGEDOWN);
        require(value.get() == 31 && completes == 3, "Slider normalized keyboard failed");
        require(services.focus().request_focus(dual.thumbs[1], input::FocusModality::keyboard),
                "Range native focus failed");
        key(SDLK_LEFT);
        require(range.get() == SliderRange{20, 79}, "Range keyboard changed wrong endpoint");
        draw("keyboard");
        const auto point = [&](const detail::MountedSliderComponent& item, double ratio) {
            const auto b = nodes.require(item.node).bounds;
            const auto& m = services.components().theme_scope(item.component)->snapshot().slider().metrics;
            const bool v = host.snapshot(item.component).orientation == SliderOrientation::Vertical;
            const float length = v ? b.height : b.width;
            const float inset = std::min(length / 2, m.handle_size_hover / 2 + m.handle_line_width_hover);
            const float pos = inset + (length - 2 * inset) * static_cast<float>(ratio);
            return v ? runtime::Point{b.x + b.width / 2, b.y + pos} : runtime::Point{b.x + pos, b.y + b.height / 2};
        };
        const auto pointer = [&](Uint32 type, runtime::Point p) {
            SDL_Event event{};
            event.type = type;
            const float x = p.x * scale / metrics.pixel_density;
            const float y = p.y * scale / metrics.pixel_density;
            if (type == SDL_EVENT_MOUSE_MOTION) {
                event.motion.windowID = window_id;
                event.motion.x = x;
                event.motion.y = y;
            } else {
                event.button.windowID = window_id;
                event.button.button = SDL_BUTTON_LEFT;
                event.button.down = type == SDL_EVENT_MOUSE_BUTTON_DOWN;
                event.button.x = x;
                event.button.y = y;
            }
            require(SDL_PushEvent(&event), "Slider mouse injection failed");
            poll();
        };
        pointer(SDL_EVENT_MOUSE_BUTTON_DOWN, point(single, 0.3));
        pointer(SDL_EVENT_MOUSE_MOTION, point(single, 1.2));
        pointer(SDL_EVENT_MOUSE_BUTTON_UP, point(single, 1.2));
        require(value.get() == 100 && !host.snapshot(single.component).dragging, "Slider native capture/clamp failed");
        draw("dragged");
        pointer(SDL_EVENT_MOUSE_BUTTON_DOWN, point(dual, 0.2));
        pointer(SDL_EVENT_MOUSE_MOTION, point(dual, 0.95));
        pointer(SDL_EVENT_MOUSE_BUTTON_UP, point(dual, 0.95));
        require(range.get() == SliderRange{79, 79}, "Range native crossing failed");
        draw("range");
        pointer(SDL_EVENT_MOUSE_BUTTON_DOWN, point(reverse, 0.1));
        pointer(SDL_EVENT_MOUSE_BUTTON_UP, point(reverse, 0.1));
        pointer(SDL_EVENT_MOUSE_BUTTON_DOWN, point(vertical, 0.1));
        pointer(SDL_EVENT_MOUSE_BUTTON_UP, point(vertical, 0.1));
        require(host.snapshot(reverse.component).value.lower == 90 &&
                    host.snapshot(vertical.component).value.lower == 90,
                "Slider native direction failed");
        pointer(SDL_EVENT_MOUSE_BUTTON_DOWN, point(blocked, 0.1));
        pointer(SDL_EVENT_MOUSE_BUTTON_UP, point(blocked, 0.1));
        require(host.snapshot(blocked.component).value.lower == 60, "disabled native Slider changed");
        pointer(SDL_EVENT_MOUSE_BUTTON_DOWN, point(single, 0.5));
        const auto completed_before = completes;
        disabled.set(true);
        pointer(SDL_EVENT_MOUSE_BUTTON_UP, point(single, 0.8));
        require(completes == completed_before && !host.snapshot(single.component).dragging,
                "disabled native capture completed");
        disabled.set(false);
        ThemeConfig dark;
        dark.algorithms = {ThemeAlgorithm::Dark};
        theme.set(dark);
        draw("dark");
        ThemeConfig compact;
        compact.algorithms = {ThemeAlgorithm::Compact};
        theme.set(compact);
        draw("compact");
        require(SDL_SetWindowSize(window, 900, 720), "Slider native resize failed");
        platform.delay(100);
        metrics = platform.window_metrics();
        width.set(dp(280));
        draw("resized");
        require(metrics.coordinate_width == 900 && metrics.coordinate_height == 720 && runs == 1,
                "Slider resize rebuilt content or extent wrong");
        std::cout << "slider_acceptance=passed gpu_driver=" << renderer.gpu_driver()
                  << " shader_format=" << renderer.shader_format() << " system_display_scale=" << metrics.display_scale
                  << " render_scale=" << scale << " normalized_events=" << normalized << " changes=" << changes
                  << " completes=" << completes << " submits=" << renderer.counters().frame_submissions
                  << " resize=900x720 content_runs=" << runs << " exit_code=0\n";
        services.dispose();
        return 0;
    } catch (const std::exception& e) {
        std::cerr << "slider_acceptance_error=" << e.what() << '\n';
        return 1;
    }
}
} // namespace rynui::example
