#include "acceptance_events.hpp"
#include "icon_samples.hpp"
#include "token_gallery_definition.hpp"
#include "component/button_component.hpp"
#include "component/input_component.hpp"
#include "component/tooltip_component.hpp"
#include "platform/default_font_chain.hpp"
#include "platform/sdl/platform_state.hpp"
#include "renderer/common/scene_resources.hpp"
#include "renderer/sdl/scene_renderer.hpp"

#include <SDL3/SDL.h>
#include <ryn/rynui.hpp>
#include <cmath>
#include <filesystem>
#include <iostream>

namespace rynui::example {
namespace {
void require_icon(bool value, const char* message) {
    if (!value) {
        throw std::runtime_error(message);
    }
}
} // namespace

int run_icon_acceptance(int argc, char** argv) {
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
        require_icon(!directory.empty(), "Icon requires --evidence-dir");
        std::filesystem::create_directories(directory);
        detail::PlatformConfig config;
        config.title = "RynUI Icon Acceptance";
        config.width = 1600;
        config.height = 1100;
        auto created = detail::PlatformState::create(config);
        require_icon(bool(created), "Icon window failed");
        auto& platform = *created.state;
        acceptance::FixtureEventFilter event_filter{true};
        auto metrics = platform.window_metrics();
        const float scale = requested_scale.value_or(metrics.display_scale);
        require_icon(std::isfinite(scale) && scale > 0, "Icon scale invalid");
        const auto executable = std::filesystem::absolute(argv[0]).parent_path();
        auto font_result = font::FontRuntime::create();
        require_icon(bool(font_result), "Icon font runtime failed");
        auto fonts = std::move(font_result.runtime);
        detail::DefaultFontChainRequest request;
        request.raster = {14, scale};
        request.fallback_latin = executable / "fonts/latin.ttf";
        request.fallback_cjk = executable / "fonts/cjk.otf";
        auto chain = detail::load_default_ui_font_chain(*fonts, request);
        require_icon(bool(chain), "Icon system font chain failed");
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
        services.set_motion_preference(animation::MotionPreference::normal);
        Signal<ThemeConfig> theme{ThemeConfig{}};
        Signal<IconSource> source{IconSource{IconName::HeartTwoTone}};
        Signal<IconTwoToneColor> colors{{Color::rgba8(22, 119, 255), Color::rgba8(230, 244, 255)}};
        Signal<float> angle{0};
        Signal<bool> spin{false};
        Signal<bool> visible{true};
        Signal<bool> open{false};
        int content_runs{};
        int clicks{};
        int pointer_events{};
        ButtonRef ref;
        services.mount(Content{[&] {
            ++content_runs;
            Theme(
                ThemeProps{}.config(theme), ThemeContent{[&] {
                    Flex(
                        FlexProps{}.vertical(true).align(FlexAlign::Start).gap(dp(12)), FlexContent{[&] {
                            Text(u8"Icon：离线三类 / 四层双色 / 贝塞尔与孔洞 / 偏移宽视框 / rotate 与 spin");
                            ThemeConfig large;
                            large.text.tokens.font_size = dp(64);
                            large.text.tokens.line_height = dp(80);
                            Theme(ThemeProps{}.config(large), ThemeContent{[&] {
                                      Space(SpaceProps{}.size(dp(20)), SpaceContent{[&] {
                                                Icon(IconProps{}.name(IconName::HomeOutlined));
                                                Icon(IconProps{}.name(IconName::HomeFilled));
                                                Icon(IconProps{}.name(IconName::HeartTwoTone));
                                                Icon(IconProps{}
                                                         .name(IconName::WalletTwoTone)
                                                         .twoToneColor(colors)
                                                         .rotate(angle));
                                                Icon(IconProps{}.source(source).rotate(angle).visible(visible));
                                                Icon(IconProps{}.name(IconName::LoadingOutlined).spin(spin).rotate(15));
                                                Icon(IconProps{}
                                                         .source(icon_vector_sample())
                                                         .twoToneColor(colors)
                                                         .rotate(angle));
                                                Icon(IconProps{}.source(icon_wide_sample()).rotate(angle));
                                            }});
                                  }});
                            Text(u8"左：祖先 clip（48 dp）  右：相同旋转，无 clip");
                            Theme(ThemeProps{}.config(large), ThemeContent{[&] {
                                      Space(SpaceProps{}.size(dp(50)), SpaceContent{[&] {
                                                Flex(FlexProps{}.layout(LayoutStyle{}.width(dp(48)).height(dp(80))),
                                                     FlexContent{[&] {
                                                         Icon(IconProps{}
                                                                  .name(IconName::WalletTwoTone)
                                                                  .twoToneColor(colors)
                                                                  .rotate(angle));
                                                     }});
                                                Icon(IconProps{}
                                                         .name(IconName::WalletTwoTone)
                                                         .twoToneColor(colors)
                                                         .rotate(angle));
                                            }});
                                  }});
                            Space(SpaceProps{}.size(dp(12)), SpaceContent{[&] {
                                      Button(ButtonProps{}.ref(ref).onClick([&] {
                                          ++clicks;
                                          source.set(icon_vector_sample());
                                      }),
                                             ButtonContent{[] { Text(u8"切换图形"); }},
                                             ButtonIcon{[] { Icon(IconProps{}.name(IconName::HeartTwoTone)); }});
                                      Tooltip(TooltipProps{}.open(open),
                                              TooltipTrigger{[] { Text(u8"保留浮层里的旋转图标"); }}, TooltipTitle{[] {
                                                  Icon(IconProps{}.name(IconName::LoadingOutlined).spin(true));
                                              }});
                                  }});
                            Space(SpaceProps{}.size(dp(10)), SpaceContent{[] {
                                      Input(InputProps{}
                                                .defaultValue(u8"中文 clear")
                                                .allowClear(true)
                                                .layout(LayoutStyle{}.width(dp(180))));
                                      Password(
                                          PasswordProps{}.defaultValue(u8"秘密").layout(LayoutStyle{}.width(dp(150))));
                                      Search(SearchProps{}.defaultValue(u8"搜索").layout(LayoutStyle{}.width(dp(200))));
                                  }});
                        }});
                }});
        }});
        std::vector<runtime::ComponentId> icons;
        for (const auto& text : services.text().mounted_texts()) {
            try {
                static_cast<void>(services.text().icon_snapshot(text.component));
                icons.push_back(text.component);
            } catch (const std::out_of_range&) {
            }
        }
        require_icon(icons.size() >= 12, "Icon sample inventory differs");
        const auto changed_id = icons[4];
        const auto primary_scene = services.text().icon_snapshot(changed_id).layers[0];
        const auto clip_node = *nodes.require(services.components().root(icons[8])).parent;
        nodes.require(clip_node).clip_content = true;
        detail::SdlSceneRenderer renderer{platform, executable / "shaders"};
        detail::SceneResources resources{renderer};
        std::int64_t microseconds{};
        const auto draw = [&](const std::string& name) {
            for (int settle = 0; settle < 2; ++settle) {
                static_cast<void>(platform.poll_events());
                metrics = platform.window_metrics();
                const runtime::Size viewport{metrics.pixel_width / scale, metrics.pixel_height / scale};
                microseconds += 100000;
                const auto now = animation::AnimationTime::microseconds(microseconds);
                static_cast<void>(services.tick_animations(now));
                require_icon(
                    services.layout_and_synchronize(viewport, {0, 0, viewport.width, viewport.height}, {16, 12}),
                    "Icon layout failed");
                require_icon(resources.synchronize({&services.surfaces().instances(),
                                                    scene.atlas(),
                                                    scene.glyph_scene().instances(),
                                                    &services.rounded_effects(),
                                                    {static_cast<std::uint32_t>(metrics.pixel_width),
                                                     static_cast<std::uint32_t>(metrics.pixel_height), scale}}),
                             "Icon upload failed");
                require_icon(renderer.attach_scene(resources.attach(services.scene_composer().ordered_scene())),
                             "Icon attach failed");
                renderer.set_clear_color(resolve_theme(theme.get()).alias().color_background_container);
                require_icon(renderer.submit_frame(now) != runtime::FrameSubmissionResult::failed, "Icon frame failed");
                platform.delay(25);
            }
            require_icon(renderer.save_frame_bmp(directory / (name + ".bmp")), "Icon GPU readback failed");
        };
        draw("initial");
        require_icon(services.text().icon_snapshot(icons[3]).layers.size() == 4 &&
                         services.text().icon_snapshot(icons[6]).layers.size() == 3,
                     "four-layer/vector inventory differs");
        const auto owned_font = scene.text_state(primary_scene).shaped().glyphs[0].font;
        const auto shape_count = scene.text_state(primary_scene).counters().shape_count;
        const auto raster_count = fonts->counters().rasterizations;
        const auto texture_uploads = renderer.counters().texture_transfer_maps;
        angle.set(45);
        draw("rotate-45");
        angle.set(90);
        draw("rotate-90");
        require_icon(scene.text_state(primary_scene).counters().shape_count == shape_count &&
                         fonts->counters().rasterizations == raster_count &&
                         renderer.counters().texture_transfer_maps == texture_uploads,
                     "angle update reshaped/rasterized/uploaded coverage");
        colors.set({Color::rgba8(235, 47, 150), {}});
        draw("derived-colors");
        ThemeConfig dark;
        dark.algorithms = {ThemeAlgorithm::Dark};
        dark.seed.color_primary = Color::rgba8(114, 46, 209);
        theme.set(dark);
        draw("dark-primary");
        angle.set(45);
        draw("clip-on");
        const auto clipped_scene = services.text().icon_snapshot(icons[8]).layers[0];
        const auto clipped_range = scene.primitive(clipped_scene).instances;
        const auto clip = scene.glyph_scene().instances().at(clipped_range.first).clip_bounds;
        require_icon(std::abs(clip[2] - clip[0] - 48) < .01F, "ancestor clip did not reach rotated glyph");
        nodes.require(clip_node).clip_content = false;
        dirty.invalidate(clip_node, runtime::DirtyFlags::Geometry);
        draw("clip-off");
        nodes.require(clip_node).clip_content = true;
        dirty.invalidate(clip_node, runtime::DirtyFlags::Geometry);
        spin.set(true);
        draw("spin-first");
        const auto first_angle = services.text().icon_snapshot(icons[5]).angle_degrees;
        draw("spin-second");
        require_icon(services.text().icon_snapshot(icons[5]).angle_degrees != first_angle &&
                         services.next_frame_deadline().has_value(),
                     "native spin did not advance/schedule");
        services.set_motion_preference(animation::MotionPreference::reduced);
        draw("reduced-motion");
        require_icon(!services.next_frame_deadline() && services.text().icon_snapshot(icons[5]).angle_degrees == 15,
                     "reduced motion retained native spin");
        services.set_motion_preference(animation::MotionPreference::normal);
        dark.seed.motion = false;
        theme.set(dark);
        draw("theme-motion-off");
        require_icon(!services.next_frame_deadline(), "Theme motion=false retained deadline");
        spin.set(false);
        dark.seed.motion = true;
        theme.set(dark);
        source.set(IconSource{IconName::WalletTwoTone});
        draw("source-four-layers");
        require_icon(services.text().icon_snapshot(changed_id).layers.size() == 4 &&
                         services.text().icon_snapshot(changed_id).layers[0] == primary_scene,
                     "native source switch replaced primary scene");
        visible.set(false);
        draw("hidden");
        for (const auto layer : services.text().icon_snapshot(changed_id).layers) {
            require_icon(scene.primitive(layer).instances.count == 0, "hidden native layer retained glyphs");
        }
        visible.set(true);
        auto* window = static_cast<SDL_Window*>(platform.window());
        require_icon(SDL_SetWindowSize(window, 1420, 1000), "Icon resize failed");
        platform.delay(100);
        draw("resized");
        const auto button = buttons.mounted_buttons()[0];
        const auto bounds = nodes.require(button.node).bounds;
        for (const auto type : {SDL_EVENT_MOUSE_MOTION, SDL_EVENT_MOUSE_BUTTON_DOWN, SDL_EVENT_MOUSE_BUTTON_UP}) {
            SDL_Event event{};
            event.type = type;
            event.common.timestamp = acceptance::fixture_timestamp;
            const float x = (bounds.x + bounds.width / 2) * scale / metrics.pixel_density;
            const float y = (bounds.y + bounds.height / 2) * scale / metrics.pixel_density;
            if (type == SDL_EVENT_MOUSE_MOTION) {
                event.motion.windowID = SDL_GetWindowID(window);
                event.motion.x = x;
                event.motion.y = y;
            } else {
                event.button.windowID = SDL_GetWindowID(window);
                event.button.button = SDL_BUTTON_LEFT;
                event.button.down = type == SDL_EVENT_MOUSE_BUTTON_DOWN;
                event.button.x = x;
                event.button.y = y;
            }
            require_icon(SDL_PushEvent(&event), "Icon SDL pointer injection failed");
            for (const auto& normalized : platform.poll_events().input.events()) {
                if (const auto* pointer = std::get_if<input::PointerInputEvent>(&normalized)) {
                    auto logical = *pointer;
                    logical.x *= metrics.display_scale / scale;
                    logical.y *= metrics.display_scale / scale;
                    services.pointer().dispatch(logical);
                    ++pointer_events;
                }
            }
        }
        draw("pointer-custom");
        require_icon(clicks == 1 && pointer_events == 3 && services.text().icon_snapshot(changed_id).layers.size() == 3,
                     "resized native Button icon hit/source switch failed");
        open.set(true);
        draw("popup-spin");
        require_icon(services.next_frame_deadline().has_value(), "popup spin did not start");
        open.set(false);
        draw("popup-closed");
        require_icon(!services.next_frame_deadline(), "closed popup retained spin");
        services.set_window_active(false);
        draw("inactive");
        static_cast<void>(frames.consume_request());
        const auto submissions = renderer.counters().frame_submissions;
        for (int idle = 0; idle < 3; ++idle) {
            platform.delay(25);
            static_cast<void>(platform.poll_events());
            microseconds += 50000;
            static_cast<void>(services.tick_animations(animation::AnimationTime::microseconds(microseconds)));
            require_icon(!frames.pending() && !services.next_frame_deadline() &&
                             renderer.counters().frame_submissions == submissions,
                         "native Icon did not idle");
        }
        require_icon(content_runs == 1 && metrics.coordinate_width == 1420,
                     "native Icon remounted/resized incorrectly");
        services.dispose();
        require_icon(!ref.bound() && nodes.size() == 0 && scene.size() == 0 && services.interactions().size() == 0 &&
                         services.animations().diagnostics().scopes == 0 &&
                         services.animations().diagnostics().targets == 0,
                     "native Icon disposal leaked retained resources");
        resources.retire();
        scene_owner.reset();
        require_icon(!fonts->metrics(owned_font), "window text service retained cached icon font");
        std::cout << "icon_acceptance=passed gpu_driver=" << renderer.gpu_driver()
                  << " shader_format=" << renderer.shader_format() << " system_display_scale=" << metrics.display_scale
                  << " render_scale=" << scale << " pointer_events=" << pointer_events << " clicks=" << clicks
                  << " submits=" << submissions << " content_runs=" << content_runs
                  << " icon_components=" << icons.size()
                  << " catalog=848 layer_max=4 custom=3 rotation_cache=stable clip=passed motion=passed "
                     "resize=1420x1000 idle_polls=3 deadline=none font_cleanup=passed disposed=1 exit_code=0\n";
        return 0;
    } catch (const std::exception& error) {
        std::cerr << "icon_acceptance_error=" << error.what() << '\n';
        return 1;
    }
}
} // namespace rynui::example
