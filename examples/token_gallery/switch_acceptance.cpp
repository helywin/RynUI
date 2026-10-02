#include "acceptance_events.hpp"
#include "component/selection_component.hpp"
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

void require_switch(bool condition, const char* message) {
    if (!condition) {
        throw std::runtime_error(message);
    }
}
} // namespace

int run_switch_acceptance(int argc, char** argv) {
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
        require_switch(!directory.empty(), "Switch requires --evidence-dir");
        std::filesystem::create_directories(directory);
        detail::PlatformConfig config;
        config.title = "RynUI Switch Acceptance";
        config.width = 1600;
        config.height = 1000;
        auto created = detail::PlatformState::create(config);
        require_switch(bool(created), "Switch window creation failed");
        auto& platform = *created.state;
        acceptance::FixtureEventFilter event_filter{true};
        auto metrics = platform.window_metrics();
        const float scale = requested_scale.value_or(metrics.display_scale);
        require_switch(std::isfinite(scale) && scale > 0, "Switch scale invalid");
        const auto executable = std::filesystem::absolute(argv[0]).parent_path();
        auto font_result = font::FontRuntime::create();
        require_switch(bool(font_result), "Switch font runtime failed");
        auto fonts = std::move(font_result.runtime);
        detail::DefaultFontChainRequest request;
        request.raster = {14, scale};
        request.fallback_latin = executable / "fonts/latin.ttf";
        request.fallback_cjk = executable / "fonts/cjk.otf";
        auto chain = detail::load_default_ui_font_chain(*fonts, request);
        require_switch(bool(chain), "Switch font chain failed");
        auto resolver = detail::make_default_ui_font_resolver(*fonts, chain, scale);
        runtime::NodeStore nodes;
        runtime::FrameRequestState frames;
        runtime::DirtyQueues dirty{nodes, &frames};
        layout::LayoutEngine layout{nodes};
        text::TextEngine engine{*fonts};
        detail::TextSceneService scene{*fonts, engine, frames};
        detail::WindowComponentServices services{nodes, layout, dirty, scene, resolver, frames};
        detail::SelectionComponentHost host{services};
        Signal<ThemeConfig> theme{ThemeConfig{}};
        Signal<bool> checked{false};
        Signal<bool> disabled{false};
        Signal<bool> loading{false};
        Signal<SwitchDirection> direction{SwitchDirection::LeftToRight};
        Signal<LogicalLength> narrow_width{dp(100)};
        Signal<LogicalLength> width{dp(metrics.pixel_width / scale - 24)};
        SwitchRef reference;
        SwitchRef disabled_reference;
        SwitchRef controlled_reference;
        int content_runs{};
        int slot_runs{};
        int changes{};
        int clicks{};
        int normalized{};
        int candidates{};
        services.mount(Content{[&] {
            ++content_runs;
            Theme(ThemeProps{}.config(theme), ThemeContent{[&] {
                      Flex(FlexProps{}.vertical(true).gap(dp(8)).layout(LayoutStyle{}.width(width)), FlexContent{[&] {
                               Text(u8"Switch 原生状态内容 / RTL / focus / loading / finite wave");
                               Text(u8"受控文字、ref、autoFocus、onChange → onClick");
                               Switch(SwitchProps{}
                                          .checked(checked)
                                          .ref(reference)
                                          .autoFocus(true)
                                          .disabled(disabled)
                                          .loading(loading)
                                          .onChange([&](bool value) {
                                              checked.set(value);
                                              ++changes;
                                          })
                                          .onClick([&](bool value) {
                                              require_switch(value == checked.get(),
                                                             "Switch callback candidate mismatch");
                                              ++clicks;
                                          }),
                                      SwitchSlots{SwitchCheckedContent{[&] {
                                                      ++slot_runs;
                                                      Text(u8"开启");
                                                  }},
                                                  SwitchUncheckedContent{[&] {
                                                      ++slot_runs;
                                                      Text(u8"关闭");
                                                  }}});
                               Text(u8"Small 图标 / 可切换 RTL");
                               Switch(SwitchProps{}.size(SwitchSize::Small).direction(direction),
                                      SwitchSlots{SwitchCheckedContent{[&] {
                                                      ++slot_runs;
                                                      Icon(IconProps{}.name(IconName::CheckOutlined));
                                                  }},
                                                  SwitchUncheckedContent{[&] {
                                                      ++slot_runs;
                                                      Icon(IconProps{}.name(IconName::CloseCircleFilled));
                                                  }}});
                               Text(u8"Disabled / Loading");
                               Space(SpaceProps{}.align(SpaceAlign::Center), SpaceContent{[&] {
                                         Switch(SwitchProps{}.defaultChecked(true).disabled(disabled).ref(
                                                    disabled_reference),
                                                SwitchSlots{SwitchCheckedContent{[&] {
                                                                ++slot_runs;
                                                                Text(u8"禁用");
                                                            }},
                                                            SwitchUncheckedContent{[&] {
                                                                ++slot_runs;
                                                                Text(u8"关闭");
                                                            }}});
                                         Switch(SwitchProps{}.defaultChecked(true).loading(loading));
                                     }});
                               Text(u8"受控候选不回写：展示继续关闭");
                               Switch(SwitchProps{}
                                          .checked(false)
                                          .ref(controlled_reference)
                                          .onChange([&](bool value) {
                                              require_switch(value, "Switch controlled candidate false");
                                              ++candidates;
                                          })
                                          .onClick([&](bool value) {
                                              require_switch(value && candidates == 1,
                                                             "Switch controlled callback order wrong");
                                          }),
                                      SwitchSlots{SwitchCheckedContent{[&] {
                                                      ++slot_runs;
                                                      Text(u8"开启");
                                                  }},
                                                  SwitchUncheckedContent{[&] {
                                                      ++slot_runs;
                                                      Text(u8"关闭");
                                                  }}});
                               Text(u8"100 / 40 / 10 logical pixels：内容裁剪与恢复");
                               Switch(SwitchProps{}.layout(LayoutStyle{}.width(narrow_width)),
                                      SwitchSlots{SwitchCheckedContent{[&] {
                                                      ++slot_runs;
                                                      Text(u8"长文字状态");
                                                  }},
                                                  SwitchUncheckedContent{[&] {
                                                      ++slot_runs;
                                                      Text(u8"长文字关闭");
                                                  }}});
                               ThemeConfig custom;
                               custom.switch_.algorithm = true;
                               custom.switch_.seed.color_primary = Color::rgba8(114, 46, 209);
                               Theme(ThemeProps{}.config(custom), ThemeContent{[&] {
                                         Text(u8"组件独立主色 / 保留手柄阴影");
                                         Switch(SwitchProps{}.defaultChecked(true),
                                                SwitchSlots{SwitchCheckedContent{[&] {
                                                                ++slot_runs;
                                                                Text(u8"紫色");
                                                            }},
                                                            SwitchUncheckedContent{[&] {
                                                                ++slot_runs;
                                                                Text(u8"关闭");
                                                            }}});
                                     }});
                           }});
                  }});
        }});
        detail::SdlSceneRenderer renderer{platform, executable / "shaders"};
        detail::SceneResources resources{renderer};
        std::int64_t microseconds{};
        const auto draw = [&](const std::string& name, std::int64_t increment = 200000) {
            for (int settle = 0; settle < 2; ++settle) {
                static_cast<void>(platform.poll_events());
                metrics = platform.window_metrics();
                const runtime::Size viewport{metrics.pixel_width / scale, metrics.pixel_height / scale};
                width.set(dp(viewport.width - 24));
                microseconds += increment;
                const auto now = animation::AnimationTime::microseconds(microseconds);
                static_cast<void>(services.tick_animations(now));
                require_switch(
                    services.layout_and_synchronize(viewport, {0, 0, viewport.width, viewport.height}, {12, 8}),
                    "Switch layout failed");
                require_switch(resources.synchronize({&services.surfaces().instances(),
                                                      scene.atlas(),
                                                      scene.glyph_scene().instances(),
                                                      &services.rounded_effects(),
                                                      {static_cast<std::uint32_t>(metrics.pixel_width),
                                                       static_cast<std::uint32_t>(metrics.pixel_height), scale}}),
                               "Switch upload failed");
                require_switch(renderer.attach_scene(resources.attach(services.scene_composer().ordered_scene())),
                               "Switch attach failed");
                renderer.set_clear_color(resolve_theme(theme.get()).alias().color_background_container);
                require_switch(renderer.submit_frame(now) != runtime::FrameSubmissionResult::failed,
                               "Switch frame failed");
                platform.delay(25);
            }
            require_switch(renderer.save_frame_bmp(directory / (name + ".bmp")), "Switch GPU readback failed");
        };
        draw("initial");
        const auto mounted = host.mounted();
        require_switch(mounted.size() == 7 && reference.bound() && slot_runs == 12 && content_runs == 1,
                       "Switch inventory wrong");
        const auto main = mounted[0];
        require_switch(services.focus().state().focused == main.interaction, "Switch autoFocus failed");
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
        const auto key = [&](Uint32 type) {
            SDL_Event event{};
            event.type = type;
            event.common.timestamp = acceptance::fixture_timestamp;
            event.key.windowID = window_id;
            event.key.key = SDLK_SPACE;
            event.key.down = type == SDL_EVENT_KEY_DOWN;
            require_switch(SDL_PushEvent(&event), "Switch key injection failed");
            poll();
        };
        const auto pointer = [&](Uint32 type) {
            const auto bounds = nodes.require(main.node).bounds;
            const runtime::Point point{bounds.x + bounds.width / 2, bounds.y + bounds.height / 2};
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
            require_switch(SDL_PushEvent(&event), "Switch pointer injection failed");
            poll();
        };
        key(SDL_EVENT_KEY_DOWN);
        draw("keyboard-pressed");
        require_switch(host.snapshot(main.component).focus.keyboard_pressed, "Switch Space press failed");
        key(SDL_EVENT_KEY_UP);
        require_switch(changes == 1 && clicks == 1 && checked.get() && host.snapshot(main.component).wave_active,
                       "Switch Space activation failed");
        draw("keyboard-wave", 20000);
        draw("keyboard-settled");
        pointer(SDL_EVENT_MOUSE_MOTION);
        draw("hover");
        pointer(SDL_EVENT_MOUSE_BUTTON_DOWN);
        draw("pointer-pressed");
        require_switch(host.snapshot(main.component).pointer_pressed, "Switch pointer press failed");
        pointer(SDL_EVENT_MOUSE_BUTTON_UP);
        require_switch(changes == 2 && clicks == 2 && !checked.get(), "Switch pointer activation failed");
        draw("wave-start", 20000);
        const auto progress = host.snapshot(main.component).wave_progress;
        draw("wave-middle", 50000);
        require_switch(host.snapshot(main.component).wave_active &&
                           host.snapshot(main.component).wave_progress > progress,
                       "Switch wave advancement failed");
        draw("wave-finished");
        require_switch(!host.snapshot(main.component).wave_active, "Switch wave did not finish");
        require_switch(reference.focus(), "Switch ref focus failed");
        key(SDL_EVENT_KEY_DOWN);
        loading.set(true);
        draw("loading-cancel");
        require_switch(!host.snapshot(main.component).focus.keyboard_pressed &&
                           services.focus().state().focused == main.interaction,
                       "Switch loading did not cancel press or retain focus");
        key(SDL_EVENT_KEY_UP);
        require_switch(changes == 2 && clicks == 2, "Switch loading activated");
        loading.set(false);
        pointer(SDL_EVENT_MOUSE_BUTTON_DOWN);
        disabled.set(true);
        draw("disabled-cancel");
        pointer(SDL_EVENT_MOUSE_BUTTON_UP);
        require_switch(!reference.focus() && !disabled_reference.focus() &&
                           !host.snapshot(main.component).pointer_pressed && changes == 2,
                       "Switch disabled cancellation failed");
        disabled.set(false);
        require_switch(reference.focus() && reference.blur() && controlled_reference.focus(),
                       "Switch ref focus/blur failed");
        key(SDL_EVENT_KEY_DOWN);
        key(SDL_EVENT_KEY_UP);
        draw("controlled-no-echo");
        require_switch(candidates == 1 && !host.snapshot(mounted[4].component).checked,
                       "Switch controlled authority failed");
        direction.set(SwitchDirection::RightToLeft);
        draw("rtl-small-icon");
        narrow_width.set(dp(40));
        draw("narrow-40");
        require_switch(nodes.require(mounted[5].node).bounds.width == 40, "Switch narrow width failed");
        narrow_width.set(dp(10));
        draw("narrow-10");
        narrow_width.set(dp(100));
        draw("narrow-recovered");
        ThemeConfig dark;
        dark.algorithms = {ThemeAlgorithm::Dark};
        theme.set(dark);
        draw("dark");
        ThemeConfig compact;
        compact.algorithms = {ThemeAlgorithm::Compact};
        theme.set(compact);
        draw("compact");
        require_switch(SDL_SetWindowSize(window, 1420, 900), "Switch resize failed");
        platform.delay(100);
        draw("resized");
        require_switch(metrics.coordinate_width == 1420 && nodes.require(mounted[6].node).bounds.width > 0,
                       "Switch resize lost content");
        services.set_window_active(false);
        draw("inactive");
        require_switch(content_runs == 1 && slot_runs == 12 && !services.next_frame_deadline(),
                       "Switch reran content or retained animation deadline");
        services.dispose();
        require_switch(!reference.bound() && !disabled_reference.bound() && !controlled_reference.bound() &&
                           services.rounded_effects().diagnostics().live_instances == 0 &&
                           services.interactions().size() == 0 && scene.size() == 0,
                       "Switch disposal leaked resources/ref");
        std::cout << "switch_acceptance=passed gpu_driver=" << renderer.gpu_driver()
                  << " shader_format=" << renderer.shader_format() << " system_display_scale=" << metrics.display_scale
                  << " render_scale=" << scale << " normalized_events=" << normalized << " changes=" << changes
                  << " clicks=" << clicks << " candidates=" << candidates
                  << " submits=" << renderer.counters().frame_submissions
                  << " resize=1420x900 content_runs=" << content_runs << " slot_runs=" << slot_runs
                  << " deadline=none disposed=1 exit_code=0\n";
        return 0;
    } catch (const std::exception& error) {
        std::cerr << "switch_acceptance_error=" << error.what() << '\n';
        return 1;
    }
}
} // namespace rynui::example
