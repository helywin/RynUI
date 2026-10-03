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
#include <fstream>
#include <iomanip>
#include <iostream>

namespace rynui::example {
namespace {
using namespace ryn;

void require_radio(bool condition, const char* message) {
    if (!condition) {
        throw std::runtime_error(message);
    }
}
} // namespace

int run_radio_acceptance(int argc, char** argv) {
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
        require_radio(!directory.empty(), "Radio requires --evidence-dir");
        std::filesystem::create_directories(directory);
        detail::PlatformConfig config;
        config.title = "RynUI Radio Acceptance";
        config.width = 1600;
        config.height = 1000;
        auto created = detail::PlatformState::create(config);
        require_radio(bool(created), "Radio window creation failed");
        auto& platform = *created.state;
        acceptance::FixtureEventFilter event_filter{true};
        auto metrics = platform.window_metrics();
        const float scale = requested_scale.value_or(metrics.display_scale);
        require_radio(std::isfinite(scale) && scale > 0, "Radio scale invalid");
        const auto executable = std::filesystem::absolute(argv[0]).parent_path();
        auto font_result = font::FontRuntime::create();
        require_radio(bool(font_result), "Radio font runtime failed");
        auto fonts = std::move(font_result.runtime);
        detail::DefaultFontChainRequest request;
        request.raster = {14, scale};
        request.fallback_latin = executable / "fonts/latin.ttf";
        request.fallback_cjk = executable / "fonts/cjk.otf";
        auto chain = detail::load_default_ui_font_chain(*fonts, request);
        require_radio(bool(chain), "Radio system font chain failed");
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
        Signal<RadioDirection> direction{RadioDirection::LeftToRight};
        Signal<RadioGroupOrientation> orientation{RadioGroupOrientation::Horizontal};
        Signal<RadioSize> size{RadioSize::Middle};
        Signal<RadioOptionType> type{RadioOptionType::Button};
        Signal<RadioButtonStyle> style{RadioButtonStyle::Outline};
        Signal<bool> block{false};
        Signal<std::vector<RadioOption>> options{
            std::vector<RadioOption>{{String{u8"a"}, String{u8"选项 A"}}, {String{u8"b"}, String{u8"选项 B"}}}};
        Signal<LogicalLength> width{dp(metrics.pixel_width / scale - 24)};
        RadioRef reference;
        RadioRef disabled_reference;
        RadioRef controlled_reference;
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
                               Text(u8"Radio 原生动态组 / 键盘 / 连接按钮 / RTL / ref / 有限 wave");
                               Radio(RadioProps{}
                                         .checked(checked)
                                         .disabled(disabled)
                                         .ref(reference)
                                         .autoFocus(true)
                                         .onChange([&](bool value) {
                                             checked.set(value);
                                             ++changes;
                                         })
                                         .onClick([&](bool value) {
                                             require_radio(value == checked.get(), "Radio click candidate mismatch");
                                             ++clicks;
                                         }),
                                     RadioLabel{[&] {
                                         ++label_runs;
                                         Text(u8"受控文字：Space 与鼠标选中");
                                     }});
                               Radio(RadioProps{}.checked(false).ref(controlled_reference).onChange([&](bool value) {
                                   require_radio(value, "Radio controlled candidate false");
                                   ++candidates;
                               }),
                                     RadioLabel{[&] {
                                         ++label_runs;
                                         Text(u8"受控候选不回写：保持未选");
                                     }});
                               Radio(RadioProps{}.defaultChecked(true).disabled(true).ref(disabled_reference),
                                     RadioLabel{[&] {
                                         ++label_runs;
                                         Text(u8"禁用 checked 与标签");
                                     }});
                               ThemeConfig custom;
                               custom.radio.algorithm = true;
                               custom.radio.seed.color_primary = Color::rgba8(114, 46, 209);
                               Theme(ThemeProps{}.config(custom), ThemeContent{[&] {
                                         RadioButton(RadioProps{}.defaultChecked(true).direction(direction),
                                                     RadioLabel{[&] {
                                                         ++label_runs;
                                                         Icon(IconProps{}.name(IconName::CheckOutlined));
                                                         Text(u8"独立紫色 / 富标签 / RTL");
                                                     }});
                                     }});
                               Text(u8"动态按钮：保留、重排、删除捕获、空组恢复、尺寸与连接角");
                               RadioGroup(RadioGroupProps{}
                                              .options(options)
                                              .defaultValue(String{u8"a"})
                                              .optionType(type)
                                              .buttonStyle(style)
                                              .size(size)
                                              .block(block)
                                              .direction(direction)
                                              .orientation(orientation)
                                              .layout(LayoutStyle{}.width(width)));
                               Text(u8"受控 Group：String / double / bool（禁用）");
                               RadioGroup(RadioGroupProps{}
                                              .options({{String{u8"desktop"}, String{u8"桌面"}},
                                                        {1.0, String{u8"数字"}},
                                                        {true, String{u8"禁用布尔"}, true}})
                                              .selection(RadioSelection{String{u8"desktop"}})
                                              .onValueChange([&](const RadioValue& value) {
                                                  require_radio(value == RadioValue{1.0},
                                                                "Radio Group candidate wrong");
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
                require_radio(
                    services.layout_and_synchronize(viewport, {0, 0, viewport.width, viewport.height}, {12, 8}),
                    "Radio layout failed");
                require_radio(resources.synchronize({&services.surfaces().instances(),
                                                     scene.atlas(),
                                                     scene.glyph_scene().instances(),
                                                     &services.rounded_effects(),
                                                     {static_cast<std::uint32_t>(metrics.pixel_width),
                                                      static_cast<std::uint32_t>(metrics.pixel_height), scale}}),
                              "Radio upload failed");
                require_radio(renderer.attach_scene(resources.attach(services.scene_composer().ordered_scene())),
                              "Radio attach failed");
                renderer.set_clear_color(resolve_theme(theme.get()).alias().color_background_container);
                require_radio(renderer.submit_frame(now) != runtime::FrameSubmissionResult::failed,
                              "Radio frame failed");
                platform.delay(25);
            }
            require_radio(renderer.save_frame_bmp(directory / (name + ".bmp")), "Radio GPU readback failed");
        };
        draw("initial");
        require_radio(host.mounted().size() == 9 && host.radio_groups().size() == 2 && reference.bound() &&
                          label_runs == 4 && content_runs == 1,
                      "Radio initial inventory wrong");
        const auto main = host.mounted()[0];
        const auto controlled = host.mounted()[1];
        const auto a = host.mounted()[4];
        const auto b = host.mounted()[5];
        const auto numeric = host.mounted()[7];
        const auto group = host.radio_groups()[0];
        require_radio(services.focus().state().focused == main.interaction, "Radio autoFocus failed");
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
        const auto key = [&](Uint32 event_type, SDL_Keycode code = SDLK_SPACE) {
            SDL_Event event{};
            event.type = event_type;
            event.common.timestamp = acceptance::fixture_timestamp;
            event.key.windowID = window_id;
            event.key.key = code;
            event.key.down = event_type == SDL_EVENT_KEY_DOWN;
            require_radio(SDL_PushEvent(&event), "Radio key injection failed");
            poll();
        };
        const auto pointer = [&](Uint32 event_type, const detail::MountedSelectionComponent& item) {
            const auto bounds = nodes.require(item.node).bounds;
            const runtime::Point point{bounds.x + bounds.width / 2, bounds.y + bounds.height / 2};
            SDL_Event event{};
            event.type = event_type;
            event.common.timestamp = acceptance::fixture_timestamp;
            if (event_type == SDL_EVENT_MOUSE_MOTION) {
                event.motion.windowID = window_id;
                event.motion.x = point.x * scale / metrics.pixel_density;
                event.motion.y = point.y * scale / metrics.pixel_density;
            } else {
                event.button.windowID = window_id;
                event.button.button = SDL_BUTTON_LEFT;
                event.button.down = event_type == SDL_EVENT_MOUSE_BUTTON_DOWN;
                event.button.x = point.x * scale / metrics.pixel_density;
                event.button.y = point.y * scale / metrics.pixel_density;
            }
            require_radio(SDL_PushEvent(&event), "Radio pointer injection failed");
            poll();
        };
        key(SDL_EVENT_KEY_DOWN);
        draw("keyboard-pressed");
        require_radio(host.snapshot(main.component).focus.keyboard_pressed, "Radio keyboard press failed");
        key(SDL_EVENT_KEY_UP);
        require_radio(changes == 1 && clicks == 1 && checked.get() && host.snapshot(main.component).wave_active,
                      "Radio Space activation failed");
        draw("keyboard-wave", 20000);
        draw("keyboard-settled");
        checked.set(false);
        pointer(SDL_EVENT_MOUSE_MOTION, main);
        draw("hover");
        pointer(SDL_EVENT_MOUSE_BUTTON_DOWN, main);
        draw("pointer-pressed");
        require_radio(host.snapshot(main.component).pointer_pressed, "Radio pointer press failed");
        pointer(SDL_EVENT_MOUSE_BUTTON_UP, main);
        require_radio(changes == 2 && clicks == 2 && checked.get(), "Radio pointer activation failed");
        draw("wave-start", 20000);
        const auto progress = host.snapshot(main.component).wave_progress;
        draw("wave-middle", 50000);
        require_radio(host.snapshot(main.component).wave_active &&
                          host.snapshot(main.component).wave_progress > progress,
                      "Radio wave advancement failed");
        draw("wave-finished");
        pointer(SDL_EVENT_MOUSE_BUTTON_DOWN, main);
        disabled.set(true);
        draw("disabled-cancel");
        pointer(SDL_EVENT_MOUSE_BUTTON_UP, main);
        require_radio(!reference.focus() && !disabled_reference.focus() &&
                          !host.snapshot(main.component).pointer_pressed && changes == 2,
                      "Radio disabled cancellation failed");
        disabled.set(false);
        require_radio(reference.focus() && reference.blur() && controlled_reference.focus(), "Radio refs failed");
        key(SDL_EVENT_KEY_DOWN);
        key(SDL_EVENT_KEY_UP);
        draw("controlled-no-echo");
        require_radio(candidates == 1 && !host.snapshot(controlled.component).checked,
                      "Radio controlled authority failed");
        require_radio(services.focus().request_focus(numeric.interaction, input::FocusModality::keyboard),
                      "Radio Group focus failed");
        key(SDL_EVENT_KEY_DOWN);
        key(SDL_EVENT_KEY_UP);
        draw("group-candidate");
        require_radio(group_candidates == 1 && !host.snapshot(numeric.component).checked,
                      "Radio Group authority failed");
        require_radio(services.focus().request_focus(a.interaction, input::FocusModality::keyboard),
                      "Radio dynamic focus failed");
        key(SDL_EVENT_KEY_DOWN, SDLK_RIGHT);
        key(SDL_EVENT_KEY_UP, SDLK_RIGHT);
        require_radio(host.snapshot(b.component).wave_active, "Radio button wave did not start");
        draw("arrow-selected", 20000);
        draw("button-wave-middle", 50000);
        draw("button-wave-finished");
        require_radio(services.focus().state().focused == b.interaction && host.snapshot(b.component).checked,
                      "Radio arrow navigation failed");
        options.set({{String{u8"b"}, String{u8"B · retained"}},
                     {String{u8"a"}, String{u8"选项 A"}},
                     {2.0, String{u8"新增 C"}}});
        draw("options-reordered");
        require_radio(host.mounted().size() == 10 && services.focus().state().focused == b.interaction &&
                          nodes.require(b.node).bounds.x < nodes.require(a.node).bounds.x,
                      "Radio dynamic order/identity failed");
        key(SDL_EVENT_KEY_DOWN, SDLK_RIGHT);
        key(SDL_EVENT_KEY_UP, SDLK_RIGHT);
        require_radio(services.focus().state().focused == a.interaction, "Radio reordered navigation failed");
        pointer(SDL_EVENT_MOUSE_BUTTON_DOWN, a);
        require_radio(services.pointer().state(input::PointerIdentity::mouse())->capture == a.interaction,
                      "Radio dynamic capture failed");
        options.set({{String{u8"b"}, String{u8"B · retained"}}, {2.0, String{u8"新增 C"}, true}});
        draw("options-removed");
        require_radio(!services.components().contains(a.component) && !services.focus().state().focused &&
                          !services.pointer().state(input::PointerIdentity::mouse())->capture &&
                          !host.radio_group_value(group),
                      "Radio deletion retained focus/capture/value");
        options.set({});
        draw("options-empty");
        options.set({{false, String{u8"false · 恢复"}}, {true, String{u8"true"}}});
        draw("options-recovered");
        require_radio(host.mounted().size() == 9, "Radio empty Group failed recovery");
        const auto recovered = host.mounted()[7];
        const auto recovered_last = host.mounted()[8];
        pointer(SDL_EVENT_MOUSE_BUTTON_DOWN, recovered);
        pointer(SDL_EVENT_MOUSE_BUTTON_UP, recovered);
        style.set(RadioButtonStyle::Solid);
        size.set(RadioSize::Large);
        block.set(true);
        draw("solid-large-block");
        require_radio(nodes.require(recovered.node).bounds.height == 40 &&
                          nodes.require(recovered.node).bounds.width == nodes.require(recovered_last.node).bounds.width,
                      "Radio large block geometry failed");
        direction.set(RadioDirection::RightToLeft);
        draw("rtl-buttons");
        require_radio(nodes.require(recovered.node).bounds.x > nodes.require(recovered_last.node).bounds.x,
                      "Radio RTL positions failed");
        orientation.set(RadioGroupOrientation::Vertical);
        size.set(RadioSize::Small);
        draw("vertical-small");
        require_radio(nodes.require(recovered.node).bounds.height == 24 &&
                          host.snapshot(recovered.component).rounded_corners == std::array{true, true, false, false},
                      "Radio vertical small geometry failed");
        std::ofstream fills{directory / "solid-fill.csv", std::ios::binary};
        require_radio(bool(fills), "Radio fill evidence failed to open");
        fills << "name,rtl,vertical,x,y,width,height,radius,red,green,blue,background_red,background_green,background_"
                 "blue\n"
              << std::setprecision(9);
        int solid_samples{};
        pointer(SDL_EVENT_MOUSE_MOTION, main);
        for (const bool dark_theme : {false, true}) {
            ThemeConfig config;
            config.algorithms = {dark_theme ? ThemeAlgorithm::Dark : ThemeAlgorithm::Default};
            theme.set(config);
            for (const bool rtl : {false, true}) {
                direction.set(rtl ? RadioDirection::RightToLeft : RadioDirection::LeftToRight);
                for (const bool vertical : {false, true}) {
                    orientation.set(vertical ? RadioGroupOrientation::Vertical : RadioGroupOrientation::Horizontal);
                    size.set(RadioSize::Large);
                    services.focus().clear_focus();
                    const auto name = std::string{dark_theme ? "solid-dark-" : "solid-light-"} +
                                      (rtl ? "rtl-" : "ltr-") + (vertical ? "vertical" : "horizontal");
                    draw(name);
                    require_radio(host.snapshot(recovered.component).checked, "Radio fill fixture lost selection");
                    const auto rect = nodes.require(recovered.node).bounds;
                    const auto resolved = resolve_theme(config);
                    const auto color = resolved.radio().button_solid_checked_background;
                    const auto background = resolved.alias().color_background_container;
                    fills << name << ',' << rtl << ',' << vertical << ',' << rect.x << ',' << rect.y << ','
                          << rect.width << ',' << rect.height << ',' << resolved.radio().button_radius_large << ','
                          << color.red() << ',' << color.green() << ',' << color.blue() << ',' << background.red()
                          << ',' << background.green() << ',' << background.blue() << '\n';
                    ++solid_samples;
                }
            }
        }
        require_radio(bool(fills), "Radio fill evidence write failed");
        type.set(RadioOptionType::Default);
        draw("default-circles");
        require_radio(!host.snapshot(recovered.component).radio_button, "Radio type switch failed");
        ThemeConfig dark;
        dark.algorithms = {ThemeAlgorithm::Dark};
        theme.set(dark);
        draw("dark");
        ThemeConfig compact;
        compact.algorithms = {ThemeAlgorithm::Compact};
        theme.set(compact);
        draw("compact");
        require_radio(SDL_SetWindowSize(window, 1420, 900), "Radio resize failed");
        platform.delay(100);
        draw("resized");
        require_radio(metrics.coordinate_width == 1420, "Radio resize geometry wrong");
        services.set_window_active(false);
        draw("inactive");
        require_radio(content_runs == 1 && label_runs == 4 && !services.next_frame_deadline(),
                      "Radio reran content or retained deadline");
        static_cast<void>(frames.consume_request());
        const auto idle_submissions = renderer.counters().frame_submissions;
        for (int idle = 0; idle < 3; ++idle) {
            platform.delay(25);
            poll();
            microseconds += 50000;
            static_cast<void>(services.tick_animations(animation::AnimationTime::microseconds(microseconds)));
            require_radio(!frames.pending() && !services.next_frame_deadline() &&
                              renderer.counters().frame_submissions == idle_submissions,
                          "Radio idle requested a frame");
        }
        services.dispose();
        require_radio(!reference.bound() && !disabled_reference.bound() && !controlled_reference.bound() &&
                          services.rounded_effects().diagnostics().live_instances == 0 &&
                          services.interactions().size() == 0 && scene.size() == 0,
                      "Radio disposal leaked resources");
        std::cout << "radio_acceptance=passed gpu_driver=" << renderer.gpu_driver()
                  << " shader_format=" << renderer.shader_format() << " system_display_scale=" << metrics.display_scale
                  << " render_scale=" << scale << " normalized_events=" << normalized << " changes=" << changes
                  << " clicks=" << clicks << " candidates=" << candidates << " group_candidates=" << group_candidates
                  << " submits=" << renderer.counters().frame_submissions
                  << " resize=1420x900 content_runs=" << content_runs << " label_runs=" << label_runs
                  << " solid_samples=" << solid_samples << " idle_polls=3 deadline=none disposed=1 exit_code=0\n";
        return 0;
    } catch (const std::exception& error) {
        std::cerr << "radio_acceptance_error=" << error.what() << '\n';
        return 1;
    }
}
} // namespace rynui::example
