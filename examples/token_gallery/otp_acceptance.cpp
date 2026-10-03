#include "acceptance_events.hpp"
#include "token_gallery_definition.hpp"
#include "component/button_component.hpp"
#include "component/input_component.hpp"
#include "component/otp_component.hpp"
#include "platform/default_font_chain.hpp"
#include "platform/sdl/platform_state.hpp"
#include "renderer/common/scene_resources.hpp"
#include "renderer/sdl/scene_renderer.hpp"

#include <SDL3/SDL.h>
#include <ryn/rynui.hpp>

#include <algorithm>
#include <cmath>
#include <filesystem>
#include <iostream>
#include <string>

namespace rynui::example {
namespace {
void require_otp(bool condition, const char* message) {
    if (!condition) {
        throw std::runtime_error(message);
    }
}

class OTPInputPlatform final : public ryn::input::TextInputPlatform {
public:
    explicit OTPInputPlatform(ryn::detail::PlatformState& platform) : platform_(platform) {}

    bool start(ryn::input::TextInputSessionStamp stamp,
               const ryn::input::TextInputProperties& properties) noexcept override {
        if (!platform_.start(stamp, properties)) {
            return false;
        }
        last_properties = properties;
        ++starts;
        return true;
    }

    bool stop() noexcept override {
        return platform_.stop();
    }

    bool cancel() noexcept override {
        return platform_.cancel();
    }

    bool set_area(const ryn::input::WindowTextInputArea& value) noexcept override {
        if (!platform_.set_area(value)) {
            return false;
        }
        last_area = value;
        ++areas;
        return true;
    }

    ryn::input::TextInputProperties last_properties;
    ryn::input::WindowTextInputArea last_area;
    std::size_t starts{};
    std::size_t areas{};

private:
    ryn::detail::PlatformState& platform_;
};
} // namespace

int run_otp_acceptance(int argc, char** argv) {
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
        require_otp(!directory.empty(), "OTP requires --evidence-dir");
        std::filesystem::create_directories(directory);
        detail::PlatformConfig config;
        config.title = "RynUI OTP Acceptance";
        config.width = 1600;
        config.height = 1100;
        auto created = detail::PlatformState::create(config);
        require_otp(bool(created), "OTP window failed");
        auto& platform = *created.state;
        acceptance::FixtureEventFilter event_filter{true};
        auto metrics = platform.window_metrics();
        const auto scale = requested_scale.value_or(metrics.display_scale);
        require_otp(std::isfinite(scale) && scale > 0, "OTP scale invalid");
        const auto executable = std::filesystem::absolute(argv[0]).parent_path();
        auto font_result = font::FontRuntime::create();
        require_otp(bool(font_result), "OTP font runtime failed");
        auto fonts = std::move(font_result.runtime);
        detail::DefaultFontChainRequest request;
        request.raster = {14, scale};
        request.fallback_latin = executable / "fonts/latin.ttf";
        request.fallback_cjk = executable / "fonts/cjk.otf";
        auto chain = detail::load_default_ui_font_chain(*fonts, request);
        require_otp(bool(chain), "OTP font chain failed");
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
        OTPInputPlatform traced{platform};
        detail::InputComponentHost inputs{services, traced, platform};
        inputs.set_display_scale(scale);
        services.set_motion_preference(animation::MotionPreference::reduced);
        Signal<ThemeConfig> theme{ThemeConfig{}};
        Signal<String> value{String{u8"12"}};
        Signal<OTPMask> mask{OTPMask{}};
        Signal<std::size_t> length{6};
        Signal<bool> popup{false};
        OTPRef first_ref;
        OTPRef formatter_ref;
        OTPRef dynamic_ref;
        int content_runs{};
        int partials{};
        int completions{};
        int pointer_events{};
        const auto join = [](const std::vector<String>& cells) {
            std::string bytes;
            for (const auto& cell : cells) {
                bytes.append(cell.bytes());
            }
            return String::from_utf8(bytes).value();
        };
        services.mount(Content{[&] {
            ++content_runs;
            Theme(ThemeProps{}.config(theme), ThemeContent{[&] {
                      Flex(FlexProps{}.vertical(true).align(FlexAlign::Start).gap(dp(12)), FlexContent{[&] {
                               Text(u8"OTP：grapheme / paste / mask / IME / RTL / retained length");
                               const auto row = [](std::function<void()> content) {
                                   Space(SpaceProps{}.size(dp(24)).align(SpaceAlign::Start), SpaceContent{content});
                               };
                               const auto cell = [](String label, std::function<void()> content) {
                                   Flex(FlexProps{}
                                            .vertical(true)
                                            .align(FlexAlign::Start)
                                            .gap(dp(4))
                                            .layout(LayoutStyle{}.width(dp(320))),
                                        FlexContent{[label, content] {
                                            Text(label);
                                            content();
                                        }});
                               };
                               row([&] {
                                   cell(String{u8"Outlined / controlled / partial / complete"}, [&] {
                                       OTP(OTPProps{}
                                               .length(4)
                                               .ref(first_ref)
                                               .autoFocus()
                                               .value(value)
                                               .onInput([&](const auto& cells) {
                                                   ++partials;
                                                   value.set(join(cells));
                                               })
                                               .onChange([&](String) { ++completions; }));
                                   });
                                   cell(String{u8"Filled / Small / Error"}, [] {
                                       OTP(OTPProps{}
                                               .length(4)
                                               .variant(InputVariant::Filled)
                                               .size(ControlSize::Small)
                                               .status(InputStatus::Error)
                                               .defaultValue(u8"1234"));
                                   });
                               });
                               row([&] {
                                   cell(String{u8"Borderless / Large / Warning"}, [] {
                                       OTP(OTPProps{}
                                               .length(4)
                                               .variant(InputVariant::Borderless)
                                               .size(ControlSize::Large)
                                               .status(InputStatus::Warning)
                                               .defaultValue(u8"5678"));
                                   });
                                   cell(String{u8"Underlined / disabled"}, [] {
                                       OTP(OTPProps{}
                                               .length(4)
                                               .variant(InputVariant::Underlined)
                                               .disabled(true)
                                               .defaultValue(u8"9012"));
                                   });
                               });
                               row([&] {
                                   cell(String{u8"formatter / reactive mask / preedit"}, [&] {
                                       OTP(OTPProps{}
                                               .ref(formatter_ref)
                                               .mask(mask)
                                               .defaultValue(u8"ab")
                                               .formatter([](String incoming) {
                                                   std::string bytes{incoming.bytes()};
                                                   for (auto& byte : bytes) {
                                                       if (byte >= 'a' && byte <= 'z') {
                                                           byte -= 'a' - 'A';
                                                       }
                                                   }
                                                   return String::from_utf8(bytes).value();
                                               }));
                                   });
                                   cell(String{u8"RTL / indexed separator"}, [] {
                                       OTP(OTPProps{}
                                               .length(4)
                                               .direction(OTPDirection::RightToLeft)
                                               .defaultValue(u8"abcd"),
                                           OTPSeparator{[](std::size_t previous) -> std::optional<OTPSeparatorContent> {
                                               if (previous != 1) {
                                                   return {};
                                               }
                                               return OTPSeparatorContent{[] { Text(u8" / "); }};
                                           }});
                                   });
                               });
                               row([&] {
                                   cell(String{u8"dynamic length 6 / 4 / 6"}, [&] {
                                       OTP(OTPProps{}.length(length).ref(dynamic_ref).defaultValue(u8"abcdef"));
                                   });
                                   cell(String{u8"readOnly / custom mask / sensitive copy"}, [] {
                                       OTP(OTPProps{}.length(4).readOnly(true).mask(u8"*").defaultValue(u8"4321"));
                                   });
                               });
                               Tooltip(TooltipProps{}.open(popup), TooltipTrigger{[] { Text(u8"OTP retained popup"); }},
                                       TooltipTitle{[] { Text(u8"OTP / retained scene"); }});
                           }});
                  }});
        }});
        require_otp(services.otp().mounted().size() == 8 && inputs.mounted_inputs().size() == 36,
                    "OTP inventory differs");
        std::vector<runtime::ComponentId> groups;
        for (const auto& group : services.otp().mounted()) {
            groups.push_back(group.component);
        }
        const auto mounted_cell = [&](std::size_t group, std::size_t index) {
            const auto component = services.otp().cell_components(groups[group]).at(index);
            for (const auto& mounted : inputs.mounted_inputs()) {
                if (mounted.component == component) {
                    return mounted;
                }
            }
            throw std::runtime_error("OTP cell disappeared");
        };
        const auto first = mounted_cell(0, 0);
        const auto first_scene = inputs.text_scene(first.component);
        const auto prefix = services.otp().cell_components(groups[6]);
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
                require_otp(
                    services.layout_and_synchronize(viewport, {0, 0, viewport.width, viewport.height}, {16, 12}),
                    "OTP layout failed");
                require_otp(inputs.synchronize_input_area(double(metrics.coordinate_width) / viewport.width,
                                                          metrics.coordinate_width, metrics.coordinate_height),
                            "OTP native input area failed");
                require_otp(resources.synchronize({&services.surfaces().instances(),
                                                   scene.atlas(),
                                                   scene.glyph_scene().instances(),
                                                   &services.rounded_effects(),
                                                   {static_cast<std::uint32_t>(metrics.pixel_width),
                                                    static_cast<std::uint32_t>(metrics.pixel_height), scale}}),
                            "OTP upload failed");
                require_otp(renderer.attach_scene(resources.attach(services.scene_composer().ordered_scene())),
                            "OTP attach failed");
                renderer.set_clear_color(resolve_theme(theme.get()).alias().color_background_container);
                require_otp(renderer.submit_frame(now) != runtime::FrameSubmissionResult::failed, "OTP frame failed");
                platform.delay(25);
            }
            require_otp(renderer.save_frame_bmp(directory / (name + ".bmp")), "OTP GPU readback failed");
        };
        const auto pump_pointer = [&](Uint32 type, float x, float y) {
            SDL_Event event{};
            event.type = type;
            event.common.timestamp = acceptance::fixture_timestamp;
            const auto window_id = SDL_GetWindowID(static_cast<SDL_Window*>(platform.window()));
            x *= scale / metrics.pixel_density;
            y *= scale / metrics.pixel_density;
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
            require_otp(SDL_PushEvent(&event), "OTP SDL pointer injection failed");
            for (const auto& normalized : platform.poll_events().input.events()) {
                if (const auto* pointer = std::get_if<input::PointerInputEvent>(&normalized)) {
                    auto logical = *pointer;
                    logical.x *= metrics.display_scale / scale;
                    logical.y *= metrics.display_scale / scale;
                    services.pointer().dispatch(logical);
                    ++pointer_events;
                }
            }
        };
        const auto click = [&](const detail::MountedInputComponent& mounted) {
            const auto& node = nodes.require(mounted.node);
            const auto x = node.bounds.x + node.translation.x + node.bounds.width / 2;
            const auto y = node.bounds.y + node.translation.y + node.bounds.height / 2;
            pump_pointer(SDL_EVENT_MOUSE_MOTION, x, y);
            pump_pointer(SDL_EVENT_MOUSE_BUTTON_DOWN, x, y);
            pump_pointer(SDL_EVENT_MOUSE_BUTTON_UP, x, y);
        };
        const auto key = [&](input::Key value, input::KeyModifier modifiers = input::KeyModifier::none) {
            services.focus().dispatch({value, input::KeyAction::down, modifiers});
            services.focus().dispatch({value, input::KeyAction::up, modifiers});
        };
        const auto commit = [&](String text) {
            require_otp(bool(inputs.dispatch(input::TextCommitted{std::move(text), inputs.sessions().active()})),
                        "OTP text commit failed");
        };
        draw("initial");
        require_otp(SDL_TextInputActive(static_cast<SDL_Window*>(platform.window())) && traced.areas > 0,
                    "OTP native input session inactive");
        ThemeConfig dark;
        dark.algorithms = {ThemeAlgorithm::Dark};
        theme.set(dark);
        draw("dark");
        dark.algorithms.push_back(ThemeAlgorithm::Compact);
        theme.set(dark);
        draw("compact");
        theme.set(ThemeConfig{});
        key(input::Key::right);
        key(input::Key::right);
        commit(String{u8"3"});
        draw("partial");
        require_otp(partials == 1 && completions == 0 && value.get() == String{u8"123"}, "OTP partial callback failed");
        commit(String{u8"4"});
        draw("complete");
        require_otp(partials == 2 && completions == 1 && value.get() == String{u8"1234"}, "OTP completion failed");
        require_otp(first_ref.focus() &&
                        platform.write_text(String{u8"é中🙂x尾"}.view()) == input::ClipboardError::none,
                    "OTP clipboard setup failed");
        key(input::Key::v, input::KeyModifier::control);
        draw("paste");
        require_otp(partials == 3 && completions == 2 && value.get() == String{u8"é中🙂x"} &&
                        services.otp().cells(groups[0])[0] == String{u8"é"},
                    "OTP grapheme paste failed");
        require_otp(formatter_ref.focus(), "OTP formatter focus failed");
        commit(String{u8"a1b2c3extra"});
        draw("formatter");
        require_otp(join(services.otp().cells(groups[4])) == String{u8"A1B2C3"}, "OTP formatter failed");
        mask.set({true, String{u8"*"}});
        draw("mask");
        for (std::size_t index = 0; index < 6; ++index) {
            require_otp(inputs.display_snapshot(mounted_cell(4, index).component).text == "*", "OTP mask exposed text");
        }
        require_otp(formatter_ref.focus() &&
                        platform.write_text(String{u8"sentinel"}.view()) == input::ClipboardError::none,
                    "OTP masked copy setup failed");
        key(input::Key::c, input::KeyModifier::control);
        require_otp(platform.read_text().text == std::optional{String{u8"sentinel"}}, "OTP copied sensitive value");
        key(input::Key::right);
        key(input::Key::right);
        const auto stamp = inputs.sessions().active();
        require_otp(bool(inputs.dispatch(input::CompositionChanged{String{u8"ni"}, {}, stamp})), "OTP preedit failed");
        key(input::Key::left);
        key(input::Key::enter);
        draw("preedit");
        require_otp(
            inputs.sessions().active() == stamp && join(services.otp().cells(groups[4])) == String{u8"A1B2C3"} &&
                traced.last_properties.type == input::TextInputType::password_hidden && traced.last_area.height > 0,
            "OTP preedit changed group or lost IME owner/area");
        require_otp(bool(inputs.dispatch(input::TextCommitted{String{u8"中"}, stamp})), "OTP IME commit failed");
        draw("commit");
        require_otp(services.otp().cells(groups[4])[2] == String{u8"中"} &&
                        inputs.sessions().active().owner == mounted_cell(4, 3).editor &&
                        !inputs.dispatch(input::TextCommitted{String{u8"late"}, stamp}),
                    "OTP IME stamp/advance failed");
        const auto rtl_first = mounted_cell(5, 0);
        click(rtl_first);
        key(input::Key::left);
        draw("rtl");
        require_otp(inputs.sessions().active().owner == mounted_cell(5, 1).editor &&
                        nodes.require(rtl_first.node).bounds.x > nodes.require(mounted_cell(5, 3).node).bounds.x,
                    "OTP RTL navigation/layout failed");
        require_otp(dynamic_ref.focus(), "OTP dynamic focus failed");
        for (int index = 0; index < 5; ++index) {
            key(input::Key::right);
        }
        const auto retiring = inputs.sessions().active();
        require_otp(bool(inputs.dispatch(input::CompositionChanged{String{u8"pre"}, {}, retiring})),
                    "OTP retiring preedit failed");
        length.set(4);
        draw("shrunk");
        require_otp(inputs.mounted_inputs().size() == 34 &&
                        inputs.sessions().active().owner == mounted_cell(6, 3).editor &&
                        !inputs.dispatch(input::TextCommitted{String{u8"late"}, retiring}),
                    "OTP shrink leaked owner/stamp");
        length.set(6);
        draw("expanded");
        const auto expanded = services.otp().cell_components(groups[6]);
        require_otp(std::equal(prefix.begin(), prefix.begin() + 4, expanded.begin()) &&
                        inputs.mounted_inputs().size() == 36 &&
                        join(services.otp().cells(groups[6])) == String{u8"abcdef"},
                    "OTP length changed retained prefix or hidden source");
        auto* window = static_cast<SDL_Window*>(platform.window());
        require_otp(SDL_SetWindowSize(window, 1420, 1000), "OTP window resize failed");
        platform.delay(100);
        draw("resized-window");
        click(mounted_cell(5, 2));
        draw("window-pointer-hit");
        require_otp(inputs.sessions().active().owner == mounted_cell(5, 2).editor &&
                        inputs.editors().require(mounted_cell(5, 2).editor).selection() == input::TextSelection{0, 1},
                    "OTP resized pointer missed/all selection failed");
        popup.set(true);
        draw("popup");
        popup.set(false);
        draw("popup-closed");
        services.set_window_active(false);
        draw("inactive");
        static_cast<void>(frames.consume_request());
        const auto submissions = renderer.counters().frame_submissions;
        for (int idle = 0; idle < 3; ++idle) {
            platform.delay(25);
            static_cast<void>(platform.poll_events());
            microseconds += 50000;
            static_cast<void>(services.tick_animations(animation::AnimationTime::microseconds(microseconds)));
            require_otp(!frames.pending() && !services.next_frame_deadline() &&
                            renderer.counters().frame_submissions == submissions,
                        "OTP native idle failed");
        }
        require_otp(content_runs == 1 && inputs.text_scene(first.component) == first_scene &&
                        mounted_cell(0, 0).editor == first.editor,
                    "OTP native owners rebuilt");
        services.dispose();
        require_otp(!first_ref.bound() && !formatter_ref.bound() && !dynamic_ref.bound() && nodes.size() == 0 &&
                        scene.size() == 0 && inputs.editors().size() == 0 && services.otp().mounted().empty() &&
                        services.interactions().size() == 0 && services.animations().diagnostics().scopes == 0 &&
                        services.animations().diagnostics().targets == 0 &&
                        !services.pointer().state(input::PointerIdentity::mouse())->capture,
                    "OTP native disposal leaked");
        resources.retire();
        scene_owner.reset();
        std::cout << "otp_acceptance=passed gpu_driver=" << renderer.gpu_driver()
                  << " shader_format=" << renderer.shader_format() << " system_display_scale=" << metrics.display_scale
                  << " render_scale=" << scale << " groups=8 inputs=36 content_runs=" << content_runs
                  << " pointer_events=" << pointer_events << " native_starts=" << traced.starts
                  << " native_areas=" << traced.areas << " partials=" << partials << " completions=" << completions
                  << " submits=" << submissions
                  << " variants=4 paste=passed formatter=passed mask=passed ime_area=passed ime_stamp=passed"
                     " separator=passed rtl=passed length=passed prefix=passed pointer_hit=passed"
                     " window_resize=1420x1000 popup=passed idle_polls=3 deadline=none disposed=1 exit_code=0\n";
        return 0;
    } catch (const std::exception& error) {
        std::cerr << "otp_acceptance_error=" << error.what() << '\n';
        return 1;
    }
}
} // namespace rynui::example
