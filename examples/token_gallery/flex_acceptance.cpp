#include "acceptance_events.hpp"
#include "component/button_component.hpp"
#include "component/input_component.hpp"
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
void require_flex(bool value, const char* message) {
    if (!value) {
        throw std::runtime_error(message);
    }
}
} // namespace

int run_flex_acceptance(int argc, char** argv) {
    using namespace ryn;
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
        require_flex(!directory.empty(), "Flex requires --evidence-dir");
        std::filesystem::create_directories(directory);
        detail::PlatformConfig config;
        config.title = "RynUI Flex Acceptance";
        config.width = 1600;
        config.height = 1000;
        auto created = detail::PlatformState::create(config);
        require_flex(bool(created), "Flex window failed");
        auto& platform = *created.state;
        acceptance::FixtureEventFilter event_filter{true};
        auto metrics = platform.window_metrics();
        const auto scale = requested_scale.value_or(metrics.display_scale);
        require_flex(std::isfinite(scale) && scale > 0, "Flex scale invalid");
        const auto executable = std::filesystem::absolute(argv[0]).parent_path();
        auto font_result = font::FontRuntime::create();
        require_flex(bool(font_result), "Flex font runtime failed");
        auto fonts = std::move(font_result.runtime);
        detail::DefaultFontChainRequest request;
        request.raster = {14, scale};
        request.fallback_latin = executable / "fonts/latin.ttf";
        request.fallback_cjk = executable / "fonts/cjk.otf";
        auto chain = detail::load_default_ui_font_chain(*fonts, request);
        require_flex(bool(chain), "Flex system font chain failed");
        auto resolver = detail::make_default_ui_font_resolver(*fonts, chain, scale);
        runtime::NodeStore nodes;
        runtime::FrameRequestState frames;
        runtime::DirtyQueues dirty{nodes, &frames};
        layout::LayoutEngine layout{nodes};
        text::TextEngine engine{*fonts};
        detail::TextSceneService scene{*fonts, engine, frames};
        detail::WindowComponentServices services{nodes, layout, dirty, scene, resolver, frames};
        detail::ButtonComponentHost buttons{services};
        detail::InputComponentHost inputs{services, platform, platform};
        Signal<FlexWrap> wrap{FlexWrap::NoWrap};
        Signal<FlexDirection> direction{FlexDirection::LeftToRight};
        Signal<FlexJustify> justify{FlexJustify::Start};
        Signal<bool> vertical{false};
        Signal<LogicalLength> width{dp(320)};
        ThemeConfig large;
        large.text.tokens.font_size = dp(28);
        large.text.tokens.line_height = dp(40);
        Signal<ThemeConfig> font_theme{large};
        Signal<ThemeConfig> theme{ThemeConfig{}};
        ButtonRef reference;
        int content_runs{};
        int labels{};
        int clicks{};
        int normalized{};
        services.mount(Content{[&] {
            ++content_runs;
            Theme(ThemeProps{}.config(theme), ThemeContent{[&] {
                      Flex(FlexProps{}.vertical(true).align(FlexAlign::Start).gap(dp(12)), FlexContent{[&] {
                               Text(u8"Flex 真实字体基线 / 反向换行 / RTL / 默认 Stretch");
                               Flex(
                                   FlexProps{}.align(FlexAlign::Baseline).gap(dp(8)), FlexContent{[&] {
                                       Text(u8"Ag 小字");
                                       Theme(ThemeProps{}.config(font_theme), ThemeContent{[] { Text(u8"Ag 大字"); }});
                                       Button(ButtonProps{}, [&] {
                                           ++labels;
                                           Text(u8"Ag 按钮");
                                       });
                                       Text(TypographyProps{}.content(u8"Ag 排版"));
                                       Input(
                                           InputProps{}.defaultValue(u8"Ag 输入").layout(LayoutStyle{}.width(dp(100))));
                                   }});
                               Text(u8"布局项：声明顺序 A / B / C，位置由 typed Props 改变");
                               Flex(FlexProps{}
                                        .align(FlexAlign::Start)
                                        .vertical(vertical)
                                        .wrap(wrap)
                                        .direction(direction)
                                        .justify(justify)
                                        .gap(dp(8))
                                        .layout(LayoutStyle{}.width(width).height(dp(100))),
                                    FlexContent{[&] {
                                        for (const auto label : {u8"项目 A", u8"项目 B", u8"项目 C"}) {
                                            auto props = ButtonProps{}.layout(LayoutStyle{}.width(dp(90))).onClick([&] {
                                                ++clicks;
                                            });
                                            if (labels == 1) {
                                                props.ref(reference);
                                            }
                                            Button(std::move(props), [&] {
                                                ++labels;
                                                Text(String::from_utf8(reinterpret_cast<const char*>(label)).value());
                                            });
                                        }
                                    }});
                               Text(u8"嵌套默认 Stretch：70 dp 按钮与外层文字按实际基线对齐");
                               Flex(FlexProps{}.align(FlexAlign::Baseline).gap(dp(8)), FlexContent{[&] {
                                        Flex(FlexProps{}.layout(LayoutStyle{}.height(dp(70))), FlexContent{[&] {
                                                 Button(ButtonProps{}, [&] {
                                                     ++labels;
                                                     Text(u8"拉伸按钮 Ag");
                                                 });
                                                 Text(u8"相邻 Ag");
                                             }});
                                        Text(u8"外层 Ag");
                                    }});
                           }});
                  }});
        }});
        detail::SdlSceneRenderer renderer{platform, executable / "shaders"};
        detail::SceneResources resources{renderer};
        std::int64_t microseconds{};
        const auto draw = [&](const std::string& name) {
            for (int settle = 0; settle < 2; ++settle) {
                static_cast<void>(platform.poll_events());
                metrics = platform.window_metrics();
                const runtime::Size viewport{metrics.pixel_width / scale, metrics.pixel_height / scale};
                microseconds += 200000;
                const auto now = animation::AnimationTime::microseconds(microseconds);
                static_cast<void>(services.tick_animations(now));
                require_flex(
                    services.layout_and_synchronize(viewport, {0, 0, viewport.width, viewport.height}, {12, 8}),
                    "Flex layout failed");
                require_flex(resources.synchronize({&services.surfaces().instances(),
                                                    scene.atlas(),
                                                    scene.glyph_scene().instances(),
                                                    &services.rounded_effects(),
                                                    {static_cast<std::uint32_t>(metrics.pixel_width),
                                                     static_cast<std::uint32_t>(metrics.pixel_height), scale}}),
                             "Flex upload failed");
                require_flex(renderer.attach_scene(resources.attach(services.scene_composer().ordered_scene())),
                             "Flex attach failed");
                renderer.set_clear_color(resolve_theme(theme.get()).alias().color_background_container);
                require_flex(renderer.submit_frame(now) != runtime::FrameSubmissionResult::failed, "Flex frame failed");
                platform.delay(25);
            }
            require_flex(renderer.save_frame_bmp(directory / (name + ".bmp")), "Flex readback failed");
        };
        const auto baseline = [&](std::size_t index) {
            const auto item = services.text().mounted_texts()[index];
            return nodes.require(services.components().root(item.component)).bounds.y +
                   scene.text_state(item.scene).measurement().first_baseline;
        };
        const auto check_baselines = [&] {
            for (std::size_t index = 2; index <= 4; ++index) {
                require_flex(std::abs(baseline(index) - baseline(1)) < .001F, "Flex rendered mixed baseline differs");
            }
            require_flex(std::abs(inputs.layout_snapshot(inputs.mounted_inputs()[0].component).baseline - baseline(1)) <
                             .001F,
                         "Flex rendered Input baseline differs");
            const auto texts = services.text().mounted_texts();
            require_flex(std::abs(baseline(texts.size() - 3) - baseline(texts.size() - 1)) < .001F,
                         "Flex stretched control baseline differs");
        };
        draw("initial");
        check_baselines();
        const auto a = buttons.mounted_buttons()[1];
        const auto b = buttons.mounted_buttons()[2];
        const auto c = buttons.mounted_buttons()[3];
        width.set(dp(220));
        wrap.set(FlexWrap::Wrap);
        draw("wrap");
        require_flex(nodes.require(c.node).bounds.y > nodes.require(a.node).bounds.y, "Flex wrap failed");
        wrap.set(FlexWrap::WrapReverse);
        draw("wrap-reverse");
        require_flex(nodes.require(c.node).bounds.y < nodes.require(a.node).bounds.y, "Flex reverse wrap failed");
        const auto measure_count = nodes.require(a.node).measure_count;
        const auto shape_count = scene.text_state(services.text().mounted_texts()[1].scene).counters().shape_count;
        direction.set(FlexDirection::RightToLeft);
        draw("rtl");
        require_flex(nodes.require(a.node).bounds.x > nodes.require(b.node).bounds.x &&
                         nodes.require(a.node).measure_count == measure_count &&
                         scene.text_state(services.text().mounted_texts()[1].scene).counters().shape_count ==
                             shape_count,
                     "Flex RTL remeasured or misplaced");
        justify.set(FlexJustify::Left);
        draw("physical-left");
        justify.set(FlexJustify::Right);
        draw("physical-right");
        vertical.set(true);
        wrap.set(FlexWrap::Wrap);
        draw("vertical-rtl");
        require_flex(nodes.require(c.node).bounds.x < nodes.require(a.node).bounds.x,
                     "Flex vertical RTL columns failed");
        wrap.set(FlexWrap::WrapReverse);
        draw("vertical-reverse-rtl");
        require_flex(nodes.require(c.node).bounds.x > nodes.require(a.node).bounds.x, "Flex cross reversal XOR failed");
        auto* window = static_cast<SDL_Window*>(platform.window());
        const auto window_id = SDL_GetWindowID(window);
        for (const auto type : {SDL_EVENT_MOUSE_MOTION, SDL_EVENT_MOUSE_BUTTON_DOWN, SDL_EVENT_MOUSE_BUTTON_UP}) {
            const auto bounds = nodes.require(a.node).bounds;
            SDL_Event event{};
            event.type = type;
            event.common.timestamp = acceptance::fixture_timestamp;
            const auto x = (bounds.x + bounds.width / 2) * scale / metrics.pixel_density;
            const auto y = (bounds.y + bounds.height / 2) * scale / metrics.pixel_density;
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
            require_flex(SDL_PushEvent(&event), "Flex pointer injection failed");
            for (const auto& value : platform.poll_events().input.events()) {
                if (const auto* pointer = std::get_if<input::PointerInputEvent>(&value)) {
                    auto logical = *pointer;
                    logical.x *= metrics.display_scale / scale;
                    logical.y *= metrics.display_scale / scale;
                    services.pointer().dispatch(logical);
                    ++normalized;
                }
            }
        }
        require_flex(clicks == 1 && normalized == 3 && reference.blur(), "Flex updated hit test or ref failed");
        draw("pointer-hit");
        large.text.tokens.font_size = dp(36);
        large.text.tokens.line_height = dp(50);
        font_theme.set(large);
        draw("large-font");
        check_baselines();
        ThemeConfig dark;
        dark.algorithms = {ThemeAlgorithm::Dark};
        theme.set(dark);
        draw("dark");
        check_baselines();
        require_flex(SDL_SetWindowSize(window, 1420, 900), "Flex resize failed");
        platform.delay(100);
        draw("resized");
        check_baselines();
        require_flex(metrics.coordinate_width == 1420 && content_runs == 1 && labels == 5, "Flex remounted on resize");
        services.set_window_active(false);
        draw("inactive");
        static_cast<void>(frames.consume_request());
        const auto submissions = renderer.counters().frame_submissions;
        for (int idle = 0; idle < 3; ++idle) {
            platform.delay(25);
            static_cast<void>(platform.poll_events());
            microseconds += 50000;
            static_cast<void>(services.tick_animations(animation::AnimationTime::microseconds(microseconds)));
            require_flex(!frames.pending() && !services.next_frame_deadline() &&
                             renderer.counters().frame_submissions == submissions,
                         "Flex idle requested frame");
        }
        services.dispose();
        require_flex(!reference.bound() && nodes.size() == 0 && services.interactions().size() == 0 &&
                         scene.size() == 0 && services.rounded_effects().diagnostics().live_instances == 0,
                     "Flex cleanup leaked resources");
        std::cout << "flex_acceptance=passed gpu_driver=" << renderer.gpu_driver()
                  << " shader_format=" << renderer.shader_format() << " system_display_scale=" << metrics.display_scale
                  << " render_scale=" << scale << " normalized_events=" << normalized << " clicks=" << clicks
                  << " submits=" << submissions << " resize=1420x900 content_runs=" << content_runs
                  << " label_runs=" << labels
                  << " baselines=matched idle_polls=3 deadline=none disposed=1 exit_code=0\n";
        return 0;
    } catch (const std::exception& error) {
        std::cerr << "flex_acceptance_error=" << error.what() << '\n';
        return 1;
    }
}
} // namespace rynui::example
