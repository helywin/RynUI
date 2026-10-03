#include "acceptance_events.hpp"
#include "component/button_component.hpp"
#include "platform/default_font_chain.hpp"
#include "platform/sdl/platform_state.hpp"
#include "renderer/common/scene_resources.hpp"
#include "renderer/sdl/scene_renderer.hpp"
#include "token_gallery_definition.hpp"

#include <SDL3/SDL.h>
#include <ryn/rynui.hpp>

#include <array>
#include <filesystem>
#include <iostream>

namespace rynui::example {
namespace {
using namespace ryn;

void require_button(bool condition, const char* message) {
    if (!condition) {
        throw std::runtime_error(message);
    }
}
} // namespace

int run_button_acceptance(int argc, char** argv) {
    try {
        std::optional<float> requested_scale;
        std::filesystem::path directory;
        for (int i = 1; i < argc; ++i) {
            const std::string_view argument = argv[i];
            if (argument.starts_with("--acceptance-scale=")) {
                requested_scale = std::stof(std::string{argument.substr(19)});
            }
            if (argument.starts_with("--evidence-dir=")) {
                directory = std::filesystem::path{argument.substr(15)};
            }
        }
        require_button(!directory.empty(), "Button requires --evidence-dir");
        std::filesystem::create_directories(directory);
        detail::PlatformConfig config;
        config.title = "RynUI Button Acceptance";
        config.width = 1600;
        config.height = 1000;
        auto created = detail::PlatformState::create(config);
        require_button(bool(created), "Button window creation failed");
        auto& platform = *created.state;
        acceptance::FixtureEventFilter event_filter{true};
        auto metrics = platform.window_metrics();
        const float scale = requested_scale.value_or(metrics.display_scale);
        require_button(std::isfinite(scale) && scale > 0, "Button scale invalid");
        const auto executable = std::filesystem::absolute(argv[0]).parent_path();
        auto font_result = font::FontRuntime::create();
        require_button(bool(font_result), "Button font runtime failed");
        auto fonts = std::move(font_result.runtime);
        detail::DefaultFontChainRequest font_request;
        font_request.raster = {14, scale};
        font_request.fallback_latin = executable / "fonts/latin.ttf";
        font_request.fallback_cjk = executable / "fonts/cjk.otf";
        auto chain = detail::load_default_ui_font_chain(*fonts, font_request);
        require_button(bool(chain), "Button font chain failed");
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
        Signal<ButtonVariant> variant{ButtonVariant::Outlined};
        Signal<ButtonIconPlacement> placement{ButtonIconPlacement::End};
        Signal<bool> loading{false};
        Signal<bool> builtin_loading{false};
        Signal<bool> disabled{false};
        Signal<LogicalLength> width{dp(metrics.pixel_width / scale - 24)};
        ButtonRef ref;
        ButtonRef disabled_ref;
        int content_runs{};
        int icon_runs{};
        int loading_runs{};
        int clicks{};
        constexpr std::array names{u8"Default", u8"Primary",  u8"Danger", u8"Blue", u8"Purple", u8"Cyan",
                                   u8"Green",   u8"Magenta",  u8"Pink",   u8"Red",  u8"Orange", u8"Yellow",
                                   u8"Volcano", u8"Geekblue", u8"Lime",   u8"Gold"};
        services.mount(Content{[&] {
            ++content_runs;
            Theme(
                ThemeProps{}.config(theme), ThemeContent{[&] {
                    Flex(FlexProps{}.vertical(true).gap(dp(12)).layout(LayoutStyle{}.width(width)), FlexContent{[&] {
                             Text(u8"Button 原生桌面颜色、变体、图标、聚焦、loading 与 wave");
                             Flex(FlexProps{}.wrap(true).gap(dp(8)).layout(LayoutStyle{}.width(dp(560))),
                                  FlexContent{[&] {
                                      for (std::size_t i = 0; i < names.size(); ++i) {
                                          Button(ButtonProps{}
                                                     .color(static_cast<ButtonColor>(i))
                                                     .variant(variant)
                                                     .layout(LayoutStyle{}.width(dp(132))),
                                                 ButtonContent{[i, &names] {
                                                     Text(TextProps{}.content(
                                                         String::from_utf8(std::u8string_view{names[i]}).value()));
                                                 }});
                                      }
                                  }});
                             Flex(FlexProps{}.gap(dp(12)), FlexContent{[&] {
                                      Button(ButtonProps{}
                                                 .color(ButtonColor::Blue)
                                                 .variant(ButtonVariant::Solid)
                                                 .iconPlacement(placement)
                                                 .loading(loading)
                                                 .loadingDelay(Duration::milliseconds(120))
                                                 .ref(ref)
                                                 .autoFocus(true)
                                                 .onClick([&] { ++clicks; }),
                                             ButtonContent{[] { Text(u8"图标 / Loading"); }}, ButtonIcon{[&] {
                                                 ++icon_runs;
                                                 Icon(IconProps{}.name(IconName::CheckOutlined));
                                             }},
                                             ButtonLoadingIcon{[&] {
                                                 ++loading_runs;
                                                 Icon(IconProps{}.name(IconName::DownOutlined));
                                             }});
                                      ButtonSlots icon_only;
                                      icon_only.icon =
                                          ButtonIcon{[] { Icon(IconProps{}.name(IconName::CheckOutlined)); }};
                                      Button(ButtonProps{}.shape(ButtonShape::Circle), std::move(icon_only));
                                      Button(ButtonProps{}.shape(ButtonShape::Square),
                                             ButtonContent{[] { Text(u8"Square"); }});
                                      Button(ButtonProps{}
                                                 .color(ButtonColor::Blue)
                                                 .variant(ButtonVariant::Dashed)
                                                 .ghost(true),
                                             ButtonContent{[] { Text(u8"Ghost Dashed"); }});
                                  }});
                             Button(ButtonProps{}.shape(ButtonShape::Round).block(true),
                                    ButtonContent{[] { Text(u8"Round block 随窗口宽度变化"); }});
                             Flex(
                                 FlexProps{}.gap(dp(12)), FlexContent{[&] {
                                     Button(
                                         ButtonProps{}.loading(builtin_loading).iconPlacement(ButtonIconPlacement::End),
                                         ButtonContent{[] {
                                             Text(u8"Built-in");
                                             Text(u8"Spinner");
                                         }});
                                     Button(ButtonProps{}.type(ButtonType::Primary).onClick([&] { ++clicks; }),
                                            ButtonContent{[] { Text(u8"Wave 激活"); }});
                                     Button(ButtonProps{}.disabled(disabled).ref(disabled_ref),
                                            ButtonContent{[] { Text(u8"Disabled / Focus"); }});
                                 }});
                         }});
                }});
        }});
        detail::SdlSceneRenderer renderer{platform, executable / "shaders"};
        detail::SceneResources resources{renderer};
        std::int64_t microseconds{};
        std::uint64_t normalized{};
        const auto draw = [&](const std::string& name, std::int64_t increment = 200000) {
            for (int settle = 0; settle < 2; ++settle) {
                static_cast<void>(platform.poll_events());
                metrics = platform.window_metrics();
                const runtime::Size viewport{metrics.pixel_width / scale, metrics.pixel_height / scale};
                width.set(dp(viewport.width - 24));
                microseconds += increment;
                const auto now = animation::AnimationTime::microseconds(microseconds);
                static_cast<void>(services.tick_animations(now));
                require_button(
                    services.layout_and_synchronize(viewport, {0, 0, viewport.width, viewport.height}, {12, 8}),
                    "Button layout failed");
                require_button(resources.synchronize({&services.surfaces().instances(),
                                                      scene.atlas(),
                                                      scene.glyph_scene().instances(),
                                                      &services.rounded_effects(),
                                                      {static_cast<std::uint32_t>(metrics.pixel_width),
                                                       static_cast<std::uint32_t>(metrics.pixel_height), scale}}),
                               "Button upload failed");
                require_button(renderer.attach_scene(resources.attach(services.scene_composer().ordered_scene())),
                               "Button attach failed");
                renderer.set_clear_color(resolve_theme(theme.get()).alias().color_background_container);
                require_button(renderer.submit_frame(now) != runtime::FrameSubmissionResult::failed,
                               "Button frame failed");
                platform.delay(25);
            }
            require_button(renderer.save_frame_bmp(directory / (name + ".bmp")), "Button GPU readback failed");
        };
        draw("initial");
        const auto mounted = buttons.mounted_buttons();
        require_button(mounted.size() == 24 && ref.bound() && disabled_ref.bound(), "Button native inventory wrong");
        const auto custom = mounted[16];
        const auto block = mounted[20];
        const auto wave = mounted[22];
        const auto initial_block_width = nodes.require(block.node).bounds.width;
        auto* window = static_cast<SDL_Window*>(platform.window());
        const auto window_id = SDL_GetWindowID(window);
        const auto poll = [&] {
            for (const auto& event : platform.poll_events().input.events()) {
                if (const auto* keyboard = std::get_if<input::KeyboardInputEvent>(&event)) {
                    services.focus().dispatch(*keyboard);
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
                event.common.timestamp = acceptance::fixture_timestamp;
                event.key.windowID = window_id;
                event.key.key = code;
                event.key.down = type == SDL_EVENT_KEY_DOWN;
                require_button(SDL_PushEvent(&event), "Button key injection failed");
                poll();
            }
        };
        const auto pointer = [&](Uint32 type, runtime::Point point) {
            SDL_Event event{};
            event.type = type;
            event.common.timestamp = acceptance::fixture_timestamp;
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
            require_button(SDL_PushEvent(&event), "Button pointer injection failed");
            poll();
        };
        const auto center = [&](runtime::NodeId id) {
            const auto bounds = nodes.require(id).bounds;
            return runtime::Point{bounds.x + bounds.width / 2, bounds.y + bounds.height / 2};
        };
        constexpr std::array variants{ButtonVariant::Outlined, ButtonVariant::Dashed, ButtonVariant::Solid,
                                      ButtonVariant::Filled,   ButtonVariant::Text,   ButtonVariant::Link};
        constexpr std::array variant_names{"outlined", "dashed", "solid", "filled", "text", "link"};
        for (std::size_t i = 0; i < variants.size(); ++i) {
            variant.set(variants[i]);
            draw(variant_names[i]);
            for (std::size_t color = 0; color < names.size(); ++color) {
                const auto snapshot = buttons.snapshot(mounted[color].component);
                require_button(snapshot.color == static_cast<ButtonColor>(color) && snapshot.variant == variants[i],
                               "Button native color/variant update failed");
            }
        }
        variant.set(ButtonVariant::Dashed);
        draw("ghost-dashed");
        const auto ghost = buttons.snapshot(mounted[19].component);
        require_button(ghost.ghost && ghost.presentation_background.alpha() == 0 && ghost.dashed_effects > 0,
                       "Button native ghost is not transparent/dashed");
        require_button(ref.focus() && services.focus().state().focused == custom.interaction,
                       "Button ref focus failed");
        draw("focus");
        key(SDLK_RETURN);
        require_button(clicks == 1 && buttons.snapshot(custom.component).wave_active,
                       "Button keyboard activation failed");
        draw("keyboard-wave", 20000);
        draw("keyboard-settled");
        placement.set(ButtonIconPlacement::Start);
        draw("icon-start");
        placement.set(ButtonIconPlacement::End);
        loading.set(true);
        draw("loading-pending", 20000);
        require_button(buttons.snapshot(custom.component).loading_pending &&
                           !buttons.snapshot(custom.component).loading,
                       "Button loading delay did not remain pending");
        key(SDLK_RETURN);
        require_button(clicks == 2, "Button pending loading incorrectly blocked activation");
        draw("custom-loading");
        require_button(buttons.snapshot(custom.component).loading &&
                           !buttons.snapshot(custom.component).loading_pending,
                       "Button delayed custom loading did not start");
        key(SDLK_RETURN);
        require_button(clicks == 2, "Button active loading did not block activation");
        builtin_loading.set(true);
        draw("builtin-loading");
        require_button(buttons.snapshot(mounted[21].component).spinner_running, "Button built-in end spinner failed");
        loading.set(false);
        builtin_loading.set(false);
        disabled.set(true);
        require_button(!disabled_ref.focus(), "Button disabled ref focus succeeded");
        draw("disabled");
        disabled.set(false);
        require_button(disabled_ref.focus() && disabled_ref.blur(), "Button focus/blur failed");
        const auto point = center(wave.node);
        pointer(SDL_EVENT_MOUSE_MOTION, point);
        draw("hover");
        require_button(buttons.snapshot(wave.component).hovered, "Button native hover failed");
        pointer(SDL_EVENT_MOUSE_BUTTON_DOWN, point);
        draw("pressed");
        require_button(buttons.snapshot(wave.component).pointer_pressed, "Button native press failed");
        pointer(SDL_EVENT_MOUSE_BUTTON_UP, point);
        require_button(clicks == 3 && buttons.snapshot(wave.component).wave_active,
                       "Button pointer wave did not start");
        draw("wave-start", 20000);
        const auto first_progress = buttons.snapshot(wave.component).wave_progress;
        draw("wave-middle", 50000);
        require_button(buttons.snapshot(wave.component).wave_active &&
                           buttons.snapshot(wave.component).wave_progress > first_progress,
                       "Button finite wave did not advance");
        draw("wave-finished", 1000000);
        require_button(!buttons.snapshot(wave.component).wave_active, "Button finite wave did not finish");
        pointer(SDL_EVENT_MOUSE_MOTION, {4, 4});
        ThemeConfig dark;
        dark.algorithms = {ThemeAlgorithm::Dark};
        theme.set(dark);
        draw("dark");
        ThemeConfig compact;
        compact.algorithms = {ThemeAlgorithm::Compact};
        theme.set(compact);
        draw("compact");
        require_button(SDL_SetWindowSize(window, 1420, 900), "Button resize failed");
        platform.delay(100);
        draw("resized");
        require_button(metrics.coordinate_width == 1420 && nodes.require(block.node).bounds.width < initial_block_width,
                       "Button block did not respond to window resize");
        services.set_window_active(false);
        draw("inactive");
        require_button(!services.next_frame_deadline() && content_runs == 1 && icon_runs == 1 && loading_runs == 1,
                       "Button leaked animation work or reran retained content");
        std::cout << "button_acceptance=passed gpu_driver=" << renderer.gpu_driver()
                  << " shader_format=" << renderer.shader_format() << " system_display_scale=" << metrics.display_scale
                  << " render_scale=" << scale << " normalized_events=" << normalized << " clicks=" << clicks
                  << " submits=" << renderer.counters().frame_submissions
                  << " resize=1420x900 content_runs=" << content_runs << " icon_runs=" << icon_runs
                  << " loading_runs=" << loading_runs << " exit_code=0\n";
        services.dispose();
        return 0;
    } catch (const std::exception& error) {
        std::cerr << "button_acceptance_error=" << error.what() << '\n';
        return 1;
    }
}
} // namespace rynui::example
