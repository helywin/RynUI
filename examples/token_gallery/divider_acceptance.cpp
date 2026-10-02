#include "component/divider_component.hpp"
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

void require_divider(bool condition, const char* message) {
    if (!condition) {
        throw std::runtime_error(message);
    }
}
} // namespace

int run_divider_acceptance(int argc, char** argv) {
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
        require_divider(!directory.empty(), "Divider requires --evidence-dir");
        std::filesystem::create_directories(directory);
        detail::PlatformConfig config;
        config.title = "RynUI Divider Acceptance";
        config.width = 1600;
        config.height = 1000;
        auto created = detail::PlatformState::create(config);
        require_divider(bool(created), "Divider window creation failed");
        auto& platform = *created.state;
        auto metrics = platform.window_metrics();
        const float scale = requested_scale.value_or(metrics.display_scale);
        require_divider(std::isfinite(scale) && scale > 0, "Divider scale invalid");
        const auto executable = std::filesystem::absolute(argv[0]).parent_path();
        auto font_result = font::FontRuntime::create();
        require_divider(bool(font_result), "Divider font runtime failed");
        auto fonts = std::move(font_result.runtime);
        detail::DefaultFontChainRequest request;
        request.raster = {14, scale};
        request.fallback_latin = executable / "fonts/latin.ttf";
        request.fallback_cjk = executable / "fonts/cjk.otf";
        auto chain = detail::load_default_ui_font_chain(*fonts, request);
        require_divider(bool(chain), "Divider font chain failed");
        auto resolver = detail::make_default_ui_font_resolver(*fonts, chain, scale);
        runtime::NodeStore nodes;
        runtime::FrameRequestState frames;
        runtime::DirtyQueues dirty{nodes, &frames};
        layout::LayoutEngine layout{nodes};
        text::TextEngine engine{*fonts};
        detail::TextSceneService scene{*fonts, engine, frames};
        detail::WindowComponentServices services{nodes, layout, dirty, scene, resolver, frames};
        Signal<ThemeConfig> theme{ThemeConfig{}};
        Signal<DividerVariant> variant{DividerVariant::Dotted};
        Signal<ControlSize> size{ControlSize::Small};
        Signal<DividerOrientation> orientation{DividerOrientation::Start};
        Signal<DividerDirection> direction{DividerDirection::LeftToRight};
        Signal<DividerOrientationMargin> margin{DividerOrientationMargin::length(dp(20))};
        Signal<LogicalLength> width{dp(metrics.pixel_width / scale - 24)};
        int content_runs{};
        int title_runs{};
        services.mount(Content{[&] {
            ++content_runs;
            Theme(ThemeProps{}.config(theme), ThemeContent{[&] {
                      Flex(FlexProps{}.vertical(true).layout(LayoutStyle{}.width(width)), FlexContent{[&] {
                               Text(u8"Divider 原生桌面变体、尺寸、逻辑方位与长度间距");
                               Divider(DividerProps{}.size(size));
                               Divider(DividerProps{}.variant(variant).size(size).orientation(DividerOrientation::Left),
                                       DividerText{[&] {
                                           ++title_runs;
                                           Text(u8"保留的文字 slot / Physical Left");
                                       }});
                               Divider(DividerProps{}
                                           .variant(variant)
                                           .size(size)
                                           .orientation(orientation)
                                           .direction(direction)
                                           .content(u8"Start / End / LTR / RTL"));
                               Divider(DividerProps{}
                                           .variant(variant)
                                           .size(size)
                                           .orientation(DividerOrientation::End)
                                           .direction(direction)
                                           .orientationMargin(margin)
                                           .content(u8"Length 0 / 20 dp"));
                               Divider(
                                   DividerProps{}.variant(variant).size(size).plain(true).content(u8"Plain / Center"));
                               Divider(DividerProps{}.variant(variant).size(size));
                               Space(SpaceProps{}.align(SpaceAlign::Center), SpaceContent{[&] {
                                         Text(u8"Vertical");
                                         Divider(DividerProps{}.type(DividerType::Vertical).variant(variant));
                                         Text(u8"Dotted / Dashed / Solid");
                                         Divider(DividerProps{}.type(DividerType::Vertical).dashed(true));
                                         Text(u8"Legacy dashed");
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
                width.set(dp(viewport.width - 24));
                microseconds += 200000;
                const auto now = animation::AnimationTime::microseconds(microseconds);
                static_cast<void>(services.tick_animations(now));
                require_divider(
                    services.layout_and_synchronize(viewport, {0, 0, viewport.width, viewport.height}, {12, 8}),
                    "Divider layout failed");
                require_divider(resources.synchronize({&services.surfaces().instances(),
                                                       scene.atlas(),
                                                       scene.glyph_scene().instances(),
                                                       &services.rounded_effects(),
                                                       {static_cast<std::uint32_t>(metrics.pixel_width),
                                                        static_cast<std::uint32_t>(metrics.pixel_height), scale}}),
                                "Divider upload failed");
                require_divider(renderer.attach_scene(resources.attach(services.scene_composer().ordered_scene())),
                                "Divider attach failed");
                renderer.set_clear_color(resolve_theme(theme.get()).alias().color_background_container);
                require_divider(renderer.submit_frame(now) != runtime::FrameSubmissionResult::failed,
                                "Divider frame failed");
                platform.delay(25);
            }
            require_divider(renderer.save_frame_bmp(directory / (name + ".bmp")), "Divider GPU readback failed");
        };
        draw("initial");
        auto& host = services.divider();
        const auto mounted = host.mounted();
        require_divider(mounted.size() == 8 && services.interactions().size() == 0, "Divider native inventory wrong");
        const auto logical = mounted[2];
        const auto length = mounted[3];
        const auto blank = mounted[5];
        const auto initial_width = host.snapshot(blank).left.width;
        ThemeConfig thick;
        thick.divider.tokens.line_width = dp(4);
        theme.set(thick);
        draw("thick-dotted");
        require_divider(services.rounded_effects().diagnostics().live_instances > 0 &&
                            host.snapshot(blank).line_width == 4,
                        "Divider thick circles failed");
        for (const auto [value, name] :
             {std::pair{ControlSize::Small, "small"}, std::pair{ControlSize::Middle, "middle"},
              std::pair{ControlSize::Large, "large"}}) {
            size.set(value);
            draw(name);
            require_divider(host.snapshot(blank).size == value, "Divider native size update failed");
        }
        size.set(ControlSize::Small);
        draw("ltr-start");
        const auto ltr = host.snapshot(logical);
        direction.set(DividerDirection::RightToLeft);
        draw("rtl-start");
        require_divider(std::abs(host.snapshot(logical).right.width - ltr.left.width) < 0.01F,
                        "Divider native RTL Start failed");
        orientation.set(DividerOrientation::End);
        draw("rtl-end");
        require_divider(std::abs(host.snapshot(logical).left.width - ltr.left.width) < 0.01F,
                        "Divider native RTL End failed");
        margin.set(DividerOrientationMargin::length(dp(0)));
        draw("length-zero");
        require_divider(host.snapshot(length).left.width == 0 && host.snapshot(length).label.x == 0,
                        "Divider native zero length failed");
        margin.set(DividerOrientationMargin::length(dp(20)));
        draw("length-20");
        require_divider(host.snapshot(length).label.x == 20, "Divider native explicit length failed");
        for (const auto [value, name] :
             {std::pair{DividerVariant::Solid, "solid"}, std::pair{DividerVariant::Dashed, "dashed"},
              std::pair{DividerVariant::Dotted, "dotted"}}) {
            variant.set(value);
            draw(name);
            require_divider(host.snapshot(blank).variant == value, "Divider native variant update failed");
        }
        ThemeConfig dark;
        dark.algorithms = {ThemeAlgorithm::Dark};
        theme.set(dark);
        draw("dark");
        ThemeConfig compact;
        compact.algorithms = {ThemeAlgorithm::Compact};
        theme.set(compact);
        draw("compact");
        auto* window = static_cast<SDL_Window*>(platform.window());
        require_divider(SDL_SetWindowSize(window, 1420, 900), "Divider resize failed");
        platform.delay(100);
        draw("resized");
        require_divider(metrics.coordinate_width == 1420 && host.snapshot(blank).left.width < initial_width,
                        "Divider rails did not follow resize");
        services.set_window_active(false);
        draw("inactive");
        require_divider(content_runs == 1 && title_runs == 1 && !services.next_frame_deadline() &&
                            services.interactions().size() == 0,
                        "Divider reran content or acquired input/animation work");
        std::cout << "divider_acceptance=passed gpu_driver=" << renderer.gpu_driver()
                  << " shader_format=" << renderer.shader_format() << " system_display_scale=" << metrics.display_scale
                  << " render_scale=" << scale << " submits=" << renderer.counters().frame_submissions
                  << " resize=1420x900 content_runs=" << content_runs << " title_runs=" << title_runs
                  << " interactions=0 exit_code=0\n";
        services.dispose();
        require_divider(services.rounded_effects().diagnostics().live_instances == 0, "Divider dispose leaked effects");
        return 0;
    } catch (const std::exception& error) {
        std::cerr << "divider_acceptance_error=" << error.what() << '\n';
        return 1;
    }
}
} // namespace rynui::example
