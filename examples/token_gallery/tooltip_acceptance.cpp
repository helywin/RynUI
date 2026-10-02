#include "component/button_component.hpp"
#include "component/tooltip_component.hpp"
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

void require_tooltip(bool condition, const char* message) {
    if (!condition) {
        throw std::runtime_error(message);
    }
}
} // namespace

int run_tooltip_acceptance(int argc, char** argv) {
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
        require_tooltip(!directory.empty(), "Tooltip requires --evidence-dir");
        std::filesystem::create_directories(directory);
        detail::PlatformConfig config;
        config.title = "RynUI Tooltip Acceptance";
        config.width = 1000;
        config.height = 800;
        auto created = detail::PlatformState::create(config);
        require_tooltip(bool(created), "Tooltip window creation failed");
        auto& platform = *created.state;
        auto metrics = platform.window_metrics();
        const float scale = requested_scale.value_or(metrics.display_scale);
        require_tooltip(std::isfinite(scale) && scale > 0, "Tooltip scale invalid");
        const auto executable = std::filesystem::absolute(argv[0]).parent_path();
        auto font_result = font::FontRuntime::create();
        require_tooltip(bool(font_result), "Tooltip font runtime failed");
        auto fonts = std::move(font_result.runtime);
        detail::DefaultFontChainRequest font_request;
        font_request.raster = {14, scale};
        font_request.fallback_latin = executable / "fonts/latin.ttf";
        font_request.fallback_cjk = executable / "fonts/cjk.otf";
        auto chain = detail::load_default_ui_font_chain(*fonts, font_request);
        require_tooltip(bool(chain), "Tooltip font chain failed");
        auto resolver = detail::make_default_ui_font_resolver(*fonts, chain, scale);
        runtime::NodeStore nodes;
        runtime::FrameRequestState frames;
        runtime::DirtyQueues dirty{nodes, &frames};
        layout::LayoutEngine layout{nodes};
        text::TextEngine engine{*fonts};
        detail::TextSceneService scene{*fonts, engine, frames};
        detail::WindowComponentServices services{nodes, layout, dirty, scene, resolver, frames};
        detail::ButtonComponentHost buttons{services};
        Signal<ThemeConfig> theme{ThemeConfig{}};
        Signal<bool> open{false};
        Signal<bool> arrow{true};
        int runs{};
        int requests{};
        int clicks{};
        services.mount(Content{[&] {
            ++runs;
            Theme(ThemeProps{}.config(theme), ThemeContent{[&] {
                      Flex(FlexProps{}.vertical(true).gap(dp(48)), FlexContent{[&] {
                               Tooltip(
                                   TooltipProps{}
                                       .title(String{u8"长文字提示：中文与 English "
                                                     u8"使用统一字体和窗口裁剪。悬停、Tab、Escape 后焦点仍在按钮。"})
                                       .arrow(arrow),
                                   TooltipTrigger{[&] {
                                       Button(ButtonProps{}.onClick([&] { ++clicks; }),
                                              ButtonContent{[] { Text(u8"Hover / Focus / Escape"); }});
                                   }});
                               Tooltip(TooltipProps{}
                                           .title(String{u8"Disabled child remains a hover anchor / 禁用按钮说明"})
                                           .placement(TooltipPlacement::Right),
                                       TooltipTrigger{[] {
                                           Button(ButtonProps{}.disabled(true),
                                                  ButtonContent{[] { Text(u8"Disabled child"); }});
                                       }});
                               Tooltip(TooltipProps{}
                                           .title(String{u8"Controlled open / 受控提示"})
                                           .open(open)
                                           .onOpenChange([&](bool next) {
                                               ++requests;
                                               open.set(next);
                                           }),
                                       TooltipTrigger{[] {
                                           Button(ButtonProps{}, ButtonContent{[] { Text(u8"Controlled open"); }});
                                       }});
                               Text(u8"普通 sibling 在声明顺序较后，浮层仍绘制在最上方。");
                           }});
                  }});
        }});
        detail::SdlSceneRenderer renderer{platform, executable / "shaders"};
        detail::SceneResources resources{renderer};
        std::uint64_t frame{};
        std::uint64_t normalized{};
        const auto draw = [&](const std::string& name) {
            for (int settle = 0; settle < 2; ++settle) {
                static_cast<void>(platform.poll_events());
                metrics = platform.window_metrics();
                const runtime::Size viewport{metrics.pixel_width / scale, metrics.pixel_height / scale};
                const auto now = animation::AnimationTime::microseconds(++frame * 100000);
                static_cast<void>(services.tick_animations(now));
                require_tooltip(
                    services.layout_and_synchronize(viewport, {0, 0, viewport.width, viewport.height}, {12, 8}),
                    "Tooltip layout failed");
                require_tooltip(resources.synchronize({&services.surfaces().instances(),
                                                       scene.atlas(),
                                                       scene.glyph_scene().instances(),
                                                       &services.rounded_effects(),
                                                       {static_cast<std::uint32_t>(metrics.pixel_width),
                                                        static_cast<std::uint32_t>(metrics.pixel_height), scale}}),
                                "Tooltip upload failed");
                require_tooltip(renderer.attach_scene(resources.attach(services.scene_composer().ordered_scene())),
                                "Tooltip attach failed");
                renderer.set_clear_color(resolve_theme(theme.get()).alias().color_background_container);
                require_tooltip(renderer.submit_frame(now) != runtime::FrameSubmissionResult::failed,
                                "Tooltip frame failed");
                platform.delay(35);
            }
            require_tooltip(renderer.save_frame_bmp(directory / (name + ".bmp")), "Tooltip GPU readback failed");
        };
        draw("initial");
        auto& host = services.tooltip();
        require_tooltip(host.mounted().size() == 3 && buttons.mounted_buttons().size() == 3, "Tooltip inventory wrong");
        const auto first = host.mounted()[0];
        const auto blocked = host.mounted()[1];
        const auto controlled = host.mounted()[2];
        const auto button = buttons.mounted_buttons()[0];
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
                require_tooltip(SDL_PushEvent(&event), "Tooltip key injection failed");
                poll();
            }
        };
        const auto hover = [&](runtime::ComponentId id) {
            const auto b = nodes.require(services.components().root(id)).bounds;
            SDL_Event event{};
            event.type = SDL_EVENT_MOUSE_MOTION;
            event.motion.windowID = window_id;
            event.motion.x = (b.x + b.width / 2) * scale / metrics.pixel_density;
            event.motion.y = (b.y + b.height / 2) * scale / metrics.pixel_density;
            require_tooltip(SDL_PushEvent(&event), "Tooltip pointer injection failed");
            poll();
        };
        hover(first);
        draw("hover-edge");
        require_tooltip(host.snapshot(first).visible && host.snapshot(first).placement == TooltipPlacement::Bottom,
                        "Tooltip hover/edge flip failed");
        const auto popup = host.snapshot(first).bounds;
        require_tooltip(popup.x >= 0 && popup.width <= 250 && popup.y >= 0, "Tooltip window clipping failed");
        key(SDLK_ESCAPE);
        draw("escape");
        require_tooltip(!host.snapshot(first).visible, "Tooltip Escape failed");
        hover(blocked);
        draw("disabled-child");
        require_tooltip(host.snapshot(blocked).visible && !host.snapshot(first).visible,
                        "Tooltip disabled child hover failed");
        hover(controlled);
        draw("controlled");
        require_tooltip(open.get() && requests == 1 && host.snapshot(controlled).visible,
                        "Tooltip controlled trigger failed");
        key(SDLK_ESCAPE);
        require_tooltip(!open.get() && requests == 2, "Tooltip controlled Escape failed");
        require_tooltip(services.focus().request_focus(button.interaction, input::FocusModality::keyboard),
                        "Tooltip focus failed");
        draw("focus");
        require_tooltip(host.snapshot(first).visible, "Tooltip keyboard focus failed");
        key(SDLK_ESCAPE);
        key(SDLK_RETURN);
        require_tooltip(services.focus().state().focused == button.interaction && clicks == 1,
                        "Tooltip lost child focus/activation");
        services.focus().clear_focus();
        draw("dismissed");
        require_tooltip(services.focus().request_focus(button.interaction, input::FocusModality::keyboard),
                        "Tooltip refocus failed");
        ThemeConfig dark;
        dark.algorithms = {ThemeAlgorithm::Dark};
        theme.set(dark);
        draw("dark");
        ThemeConfig compact;
        compact.algorithms = {ThemeAlgorithm::Compact};
        theme.set(compact);
        draw("compact");
        arrow.set(false);
        draw("no-arrow");
        require_tooltip(SDL_SetWindowSize(window, 760, 600), "Tooltip resize failed");
        platform.delay(100);
        draw("resized");
        require_tooltip(runs == 1 && host.snapshot(first).visible && metrics.coordinate_width == 760,
                        "Tooltip resize rebuilt content");
        services.set_window_active(false);
        draw("inactive");
        require_tooltip(!host.snapshot(first).visible && !services.next_frame_deadline(),
                        "Tooltip inactive window leaked deadline");
        std::cout << "tooltip_acceptance=passed gpu_driver=" << renderer.gpu_driver()
                  << " shader_format=" << renderer.shader_format() << " system_display_scale=" << metrics.display_scale
                  << " render_scale=" << scale << " normalized_events=" << normalized << " requests=" << requests
                  << " child_clicks=" << clicks << " submits=" << renderer.counters().frame_submissions
                  << " resize=760x600 content_runs=" << runs << " exit_code=0\n";
        services.dispose();
        return 0;
    } catch (const std::exception& e) {
        std::cerr << "tooltip_acceptance_error=" << e.what() << '\n';
        return 1;
    }
}
} // namespace rynui::example
