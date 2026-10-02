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

void require_checkbox(bool condition, const char* message) {
    if (!condition) {
        throw std::runtime_error(message);
    }
}
} // namespace

int run_checkbox_acceptance(int argc, char** argv) {
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
        require_checkbox(!directory.empty(), "Checkbox requires --evidence-dir");
        std::filesystem::create_directories(directory);
        detail::PlatformConfig config;
        config.title = "RynUI Checkbox Acceptance";
        config.width = 1600;
        config.height = 1000;
        auto created = detail::PlatformState::create(config);
        require_checkbox(bool(created), "Checkbox window creation failed");
        auto& platform = *created.state;
        acceptance::FixtureEventFilter event_filter{true};
        auto metrics = platform.window_metrics();
        const float scale = requested_scale.value_or(metrics.display_scale);
        require_checkbox(std::isfinite(scale) && scale > 0, "Checkbox scale invalid");
        const auto executable = std::filesystem::absolute(argv[0]).parent_path();
        auto font_result = font::FontRuntime::create();
        require_checkbox(bool(font_result), "Checkbox font runtime failed");
        auto fonts = std::move(font_result.runtime);
        detail::DefaultFontChainRequest request;
        request.raster = {14, scale};
        request.fallback_latin = executable / "fonts/latin.ttf";
        request.fallback_cjk = executable / "fonts/cjk.otf";
        auto chain = detail::load_default_ui_font_chain(*fonts, request);
        require_checkbox(bool(chain), "Checkbox system font chain failed");
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
        Signal<CheckboxDirection> direction{CheckboxDirection::LeftToRight};
        Signal<std::vector<CheckboxOption>> options{
            std::vector<CheckboxOption>{{String{u8"a"}, String{u8"选项 A"}}, {String{u8"b"}, String{u8"选项 B"}}}};
        Signal<LogicalLength> width{dp(metrics.pixel_width / scale - 24)};
        CheckboxRef reference;
        CheckboxRef disabled_reference;
        CheckboxRef controlled_reference;
        int content_runs{};
        int label_runs{};
        int changes{};
        int clicks{};
        int normalized{};
        int candidates{};
        int group_candidates{};
        services.mount(Content{[&] {
            ++content_runs;
            Theme(ThemeProps{}.config(theme), ThemeContent{[&] {
                      Flex(FlexProps{}.vertical(true).gap(dp(8)).layout(LayoutStyle{}.width(width)), FlexContent{[&] {
                               Text(u8"Checkbox 原生 Group / 动态选项 / RTL / ref / 有限 wave");
                               Checkbox(CheckboxProps{}
                                            .checked(checked)
                                            .disabled(disabled)
                                            .ref(reference)
                                            .autoFocus(true)
                                            .onChange([&](bool value) {
                                                checked.set(value);
                                                ++changes;
                                            })
                                            .onClick([&](bool value) {
                                                require_checkbox(value == checked.get(),
                                                                 "Checkbox click candidate mismatch");
                                                ++clicks;
                                            }),
                                        CheckboxLabel{[&] {
                                            ++label_runs;
                                            Text(u8"受控文字：Space 与鼠标切换");
                                        }});
                               Checkbox(CheckboxProps{}
                                            .checked(false)
                                            .ref(controlled_reference)
                                            .onChange([&](bool value) {
                                                require_checkbox(value, "Checkbox controlled candidate false");
                                                ++candidates;
                                            })
                                            .onClick([&](bool value) {
                                                require_checkbox(value && candidates == 1,
                                                                 "Checkbox controlled click order wrong");
                                            }),
                                        CheckboxLabel{[&] {
                                            ++label_runs;
                                            Text(u8"受控候选不回写：保持未选");
                                        }});
                               Checkbox(CheckboxProps{}.indeterminate(true).direction(direction), CheckboxLabel{[&] {
                                            ++label_runs;
                                            Icon(IconProps{}.name(IconName::CheckOutlined));
                                            Text(u8"半选 / 图标和文字 / 可切换 RTL");
                                        }});
                               Checkbox(CheckboxProps{}.defaultChecked(true).disabled(true).ref(disabled_reference),
                                        CheckboxLabel{[&] {
                                            ++label_runs;
                                            Text(u8"禁用 checked 与标签");
                                        }});
                               ThemeConfig custom;
                               custom.checkbox.algorithm = true;
                               custom.checkbox.seed.color_primary = Color::rgba8(114, 46, 209);
                               custom.checkbox.tokens.size = dp(20);
                               Theme(ThemeProps{}.config(custom), ThemeContent{[&] {
                                         Checkbox(CheckboxProps{}.defaultChecked(true), CheckboxLabel{[&] {
                                                      ++label_runs;
                                                      Text(u8"组件独立紫色 / 20 dp indicator");
                                                  }});
                                     }});
                               Text(u8"动态选项：保留、重排、删除捕获与空组恢复");
                               CheckboxGroup(CheckboxGroupProps{}.options(options).defaultValue({String{u8"b"}}));
                               Text(u8"受控 Group：String / double / bool（禁用）");
                               CheckboxGroup(
                                   CheckboxGroupProps{}
                                       .options(std::vector<CheckboxOption>{{String{u8"desktop"}, String{u8"桌面"}},
                                                                            {1.0, String{u8"数字"}},
                                                                            {true, String{u8"禁用布尔"}, true}})
                                       .value(CheckboxValues{String{u8"desktop"}})
                                       .onChange([&](const CheckboxValues& value) {
                                           require_checkbox(value == CheckboxValues{String{u8"desktop"}, 1.0},
                                                            "Checkbox Group candidate wrong");
                                           ++group_candidates;
                                       }));
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
                require_checkbox(
                    services.layout_and_synchronize(viewport, {0, 0, viewport.width, viewport.height}, {12, 8}),
                    "Checkbox layout failed");
                require_checkbox(resources.synchronize({&services.surfaces().instances(),
                                                        scene.atlas(),
                                                        scene.glyph_scene().instances(),
                                                        &services.rounded_effects(),
                                                        {static_cast<std::uint32_t>(metrics.pixel_width),
                                                         static_cast<std::uint32_t>(metrics.pixel_height), scale}}),
                                 "Checkbox upload failed");
                require_checkbox(renderer.attach_scene(resources.attach(services.scene_composer().ordered_scene())),
                                 "Checkbox attach failed");
                renderer.set_clear_color(resolve_theme(theme.get()).alias().color_background_container);
                require_checkbox(renderer.submit_frame(now) != runtime::FrameSubmissionResult::failed,
                                 "Checkbox frame failed");
                platform.delay(25);
            }
            require_checkbox(renderer.save_frame_bmp(directory / (name + ".bmp")), "Checkbox GPU readback failed");
        };
        draw("initial");
        require_checkbox(host.mounted().size() == 10 && host.checkbox_groups().size() == 2 && reference.bound() &&
                             label_runs == 5 && content_runs == 1,
                         "Checkbox initial inventory wrong");
        const auto main = host.mounted()[0];
        const auto controlled = host.mounted()[1];
        const auto a = host.mounted()[5];
        const auto b = host.mounted()[6];
        const auto numeric = host.mounted()[8];
        const auto group = host.checkbox_groups()[0];
        require_checkbox(services.focus().state().focused == main.interaction, "Checkbox autoFocus failed");
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
        const auto key = [&](Uint32 type, SDL_Keycode code = SDLK_SPACE) {
            SDL_Event event{};
            event.type = type;
            event.common.timestamp = acceptance::fixture_timestamp;
            event.key.windowID = window_id;
            event.key.key = code;
            event.key.down = type == SDL_EVENT_KEY_DOWN;
            require_checkbox(SDL_PushEvent(&event), "Checkbox key injection failed");
            poll();
        };
        const auto pointer = [&](Uint32 type, const detail::MountedSelectionComponent& item) {
            const auto bounds = nodes.require(item.node).bounds;
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
            require_checkbox(SDL_PushEvent(&event), "Checkbox pointer injection failed");
            poll();
        };
        key(SDL_EVENT_KEY_DOWN);
        draw("keyboard-pressed");
        require_checkbox(host.snapshot(main.component).focus.keyboard_pressed, "Checkbox keyboard press failed");
        key(SDL_EVENT_KEY_UP);
        require_checkbox(changes == 1 && clicks == 1 && checked.get() && host.snapshot(main.component).wave_active,
                         "Checkbox Space activation failed");
        draw("keyboard-wave", 20000);
        draw("keyboard-settled");
        pointer(SDL_EVENT_MOUSE_MOTION, main);
        draw("hover");
        pointer(SDL_EVENT_MOUSE_BUTTON_DOWN, main);
        draw("pointer-pressed");
        require_checkbox(host.snapshot(main.component).pointer_pressed, "Checkbox pointer press failed");
        pointer(SDL_EVENT_MOUSE_BUTTON_UP, main);
        require_checkbox(changes == 2 && clicks == 2 && !checked.get(), "Checkbox pointer activation failed");
        draw("wave-start", 20000);
        const auto progress = host.snapshot(main.component).wave_progress;
        draw("wave-middle", 50000);
        require_checkbox(host.snapshot(main.component).wave_active &&
                             host.snapshot(main.component).wave_progress > progress,
                         "Checkbox wave advancement failed");
        draw("wave-finished");
        pointer(SDL_EVENT_MOUSE_BUTTON_DOWN, main);
        disabled.set(true);
        draw("disabled-cancel");
        pointer(SDL_EVENT_MOUSE_BUTTON_UP, main);
        require_checkbox(!reference.focus() && !disabled_reference.focus() &&
                             !host.snapshot(main.component).pointer_pressed && changes == 2,
                         "Checkbox disabled cancellation failed");
        disabled.set(false);
        require_checkbox(reference.focus() && reference.blur() && controlled_reference.focus(),
                         "Checkbox focus/blur ref failed");
        key(SDL_EVENT_KEY_DOWN);
        key(SDL_EVENT_KEY_UP);
        draw("controlled-no-echo");
        require_checkbox(candidates == 1 && !host.snapshot(controlled.component).checked,
                         "Checkbox controlled authority failed");
        require_checkbox(services.focus().request_focus(numeric.interaction, input::FocusModality::keyboard),
                         "Checkbox Group focus failed");
        key(SDL_EVENT_KEY_DOWN);
        key(SDL_EVENT_KEY_UP);
        draw("group-candidate");
        require_checkbox(group_candidates == 1 && !host.snapshot(numeric.component).checked,
                         "Checkbox Group controlled authority failed");
        require_checkbox(services.focus().request_focus(b.interaction, input::FocusModality::keyboard),
                         "Checkbox dynamic focus failed");
        options.set({{String{u8"b"}, String{u8"B · retained"}},
                     {String{u8"a"}, String{u8"选项 A"}},
                     {2.0, String{u8"新增 C"}}});
        draw("options-reordered");
        require_checkbox(host.mounted().size() == 11 && services.focus().state().focused == b.interaction &&
                             nodes.require(b.node).bounds.x < nodes.require(a.node).bounds.x,
                         "Checkbox options did not retain focus/identity/order");
        key(SDL_EVENT_KEY_DOWN, SDLK_TAB);
        key(SDL_EVENT_KEY_UP, SDLK_TAB);
        require_checkbox(services.focus().state().focused == a.interaction, "Checkbox options Tab order wrong");
        pointer(SDL_EVENT_MOUSE_BUTTON_DOWN, a);
        require_checkbox(services.pointer().state(input::PointerIdentity::mouse())->capture == a.interaction,
                         "Checkbox dynamic capture failed");
        options.set({{String{u8"b"}, String{u8"B · retained"}}, {2.0, String{u8"新增 C"}, true}});
        draw("options-removed");
        require_checkbox(!services.components().contains(a.component) && !services.focus().state().focused &&
                             !services.pointer().state(input::PointerIdentity::mouse())->capture &&
                             host.checkbox_group_value(group.component) == CheckboxValues{String{u8"b"}},
                         "Checkbox option deletion retained capture/focus/value");
        options.set({});
        draw("options-empty");
        options.set({{false, String{u8"false · 恢复"}}});
        draw("options-recovered");
        require_checkbox(host.mounted().size() == 9, "Checkbox empty Group failed recovery");
        direction.set(CheckboxDirection::RightToLeft);
        draw("rtl-rich");
        ThemeConfig dark;
        dark.algorithms = {ThemeAlgorithm::Dark};
        theme.set(dark);
        draw("dark");
        ThemeConfig compact;
        compact.algorithms = {ThemeAlgorithm::Compact};
        theme.set(compact);
        draw("compact");
        require_checkbox(SDL_SetWindowSize(window, 1420, 900), "Checkbox resize failed");
        platform.delay(100);
        draw("resized");
        require_checkbox(metrics.coordinate_width == 1420, "Checkbox resize geometry wrong");
        services.set_window_active(false);
        draw("inactive");
        require_checkbox(content_runs == 1 && label_runs == 5 && !services.next_frame_deadline(),
                         "Checkbox reran content or retained idle deadline");
        static_cast<void>(frames.consume_request());
        const auto idle_submissions = renderer.counters().frame_submissions;
        for (int idle = 0; idle < 3; ++idle) {
            platform.delay(25);
            poll();
            microseconds += 50000;
            static_cast<void>(services.tick_animations(animation::AnimationTime::microseconds(microseconds)));
            require_checkbox(!frames.pending() && !services.next_frame_deadline() &&
                                 renderer.counters().frame_submissions == idle_submissions,
                             "Checkbox idle polling requested/submitted a frame");
        }
        services.dispose();
        require_checkbox(!reference.bound() && !disabled_reference.bound() && !controlled_reference.bound() &&
                             services.rounded_effects().diagnostics().live_instances == 0 &&
                             services.interactions().size() == 0 && scene.size() == 0,
                         "Checkbox disposal leaked refs/resources");
        std::cout << "checkbox_acceptance=passed gpu_driver=" << renderer.gpu_driver()
                  << " shader_format=" << renderer.shader_format() << " system_display_scale=" << metrics.display_scale
                  << " render_scale=" << scale << " normalized_events=" << normalized << " changes=" << changes
                  << " clicks=" << clicks << " candidates=" << candidates << " group_candidates=" << group_candidates
                  << " submits=" << renderer.counters().frame_submissions
                  << " resize=1420x900 content_runs=" << content_runs << " label_runs=" << label_runs
                  << " idle_polls=3 deadline=none disposed=1 exit_code=0\n";
        return 0;
    } catch (const std::exception& error) {
        std::cerr << "checkbox_acceptance_error=" << error.what() << '\n';
        return 1;
    }
}
} // namespace rynui::example
