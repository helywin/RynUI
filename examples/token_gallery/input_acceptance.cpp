#include "acceptance_events.hpp"
#include "icon_samples.hpp"
#include "token_gallery_definition.hpp"
#include "component/button_component.hpp"
#include "component/input_component.hpp"
#include "platform/default_font_chain.hpp"
#include "platform/sdl/platform_state.hpp"
#include "renderer/common/scene_resources.hpp"
#include "renderer/sdl/scene_renderer.hpp"

#include <SDL3/SDL.h>
#include <ryn/rynui.hpp>

#include <algorithm>
#include <array>
#include <cmath>
#include <filesystem>
#include <functional>
#include <iostream>
#include <string>

namespace rynui::example {
namespace {
void require_input(bool condition, const char* message) {
    if (!condition) {
        throw std::runtime_error(message);
    }
}

class TracedInputPlatform final : public ryn::input::TextInputPlatform {
public:
    explicit TracedInputPlatform(ryn::detail::PlatformState& platform) : platform_(platform) {}

    bool start(ryn::input::TextInputSessionStamp stamp,
               const ryn::input::TextInputProperties& value) noexcept override {
        if (!platform_.start(stamp, value)) {
            return false;
        }
        properties = value;
        ++starts;
        return true;
    }

    bool stop() noexcept override {
        return platform_.stop();
    }

    bool cancel() noexcept override {
        ++cancels;
        return platform_.cancel();
    }

    bool set_area(const ryn::input::WindowTextInputArea& area) noexcept override {
        if (!platform_.set_area(area)) {
            return false;
        }
        ++areas;
        return true;
    }

    ryn::input::TextInputProperties properties;
    std::size_t starts{};
    std::size_t cancels{};
    std::size_t areas{};

private:
    ryn::detail::PlatformState& platform_;
};
} // namespace

int run_input_acceptance(int argc, char** argv) {
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
        require_input(!directory.empty(), "Input requires --evidence-dir");
        std::filesystem::create_directories(directory);
        detail::PlatformConfig config;
        config.title = "RynUI Input Acceptance";
        config.width = 1600;
        config.height = 1100;
        auto created = detail::PlatformState::create(config);
        require_input(bool(created), "Input window failed");
        auto& platform = *created.state;
        acceptance::FixtureEventFilter event_filter{true};
        auto metrics = platform.window_metrics();
        const float scale = requested_scale.value_or(metrics.display_scale);
        require_input(std::isfinite(scale) && scale > 0, "Input scale invalid");
        const auto executable = std::filesystem::absolute(argv[0]).parent_path();
        auto font_result = font::FontRuntime::create();
        require_input(bool(font_result), "Input font runtime failed");
        auto fonts = std::move(font_result.runtime);
        detail::DefaultFontChainRequest request;
        request.raster = {14, scale};
        request.fallback_latin = executable / "fonts/latin.ttf";
        request.fallback_cjk = executable / "fonts/cjk.otf";
        auto chain = detail::load_default_ui_font_chain(*fonts, request);
        require_input(bool(chain), "Input system font chain failed");
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
        TracedInputPlatform traced{platform};
        detail::InputComponentHost inputs{services, traced, platform};
        inputs.set_display_scale(scale);
        services.set_motion_preference(animation::MotionPreference::reduced);
        Signal<ThemeConfig> theme{ThemeConfig{}};
        Signal<InputVariant> variant{InputVariant::Outlined};
        Signal<InputStatus> status{InputStatus::Default};
        Signal<InputCountOptions> count{{20, InputCountUnit::Grapheme}};
        Signal<bool> show_count{true};
        Signal<bool> clear_disabled{true};
        Signal<InputPurpose> purpose{InputPurpose::Email};
        Signal<bool> password_visible{false};
        Signal<bool> password_toggle{true};
        Signal<PasswordAction> password_action{PasswordAction::Hover};
        Signal<String> search_value{String{u8"RynUI"}};
        Signal<bool> popup{false};
        InputRef count_ref;
        InputRef format_ref;
        InputRef email_ref;
        InputRef clear_ref;
        InputRef password_ref;
        InputRef search_ref;
        int content_runs{};
        int clears{};
        int searches{};
        int clear_searches{};
        int pointer_events{};
        int focus_events{};
        services.mount(Content{[&] {
            ++content_runs;
            Theme(
                ThemeProps{}.config(theme), ThemeContent{[&] {
                    Flex(
                        FlexProps{}.vertical(true).align(FlexAlign::Start).gap(dp(8)), FlexContent{[&] {
                            Text(u8"Input / Password / Search：原生四变体、计数、焦点、系统提示与 typed 图标");
                            const auto row = [](std::function<void()> content) {
                                Space(SpaceProps{}.size(dp(16)).align(SpaceAlign::Start), SpaceContent{content});
                            };
                            const auto cell = [](String label, std::function<void()> content) {
                                Flex(FlexProps{}.vertical(true).gap(dp(2)).layout(LayoutStyle{}.width(dp(280))),
                                     FlexContent{[label, content] {
                                         Text(label);
                                         content();
                                     }});
                            };
                            row([&] {
                                cell(String{u8"Outlined → Filled / reactive status"}, [&] {
                                    Input(InputProps{}.variant(variant).status(status).defaultValue(u8"中文 🙂 Input"));
                                });
                                cell(String{u8"Filled / Error / Small"}, [] {
                                    Input(InputProps{}
                                              .variant(InputVariant::Filled)
                                              .status(InputStatus::Error)
                                              .size(ControlSize::Small)
                                              .defaultValue(u8"filled error"));
                                });
                            });
                            row([&] {
                                cell(String{u8"Borderless / Warning / Large"}, [] {
                                    Input(InputProps{}
                                              .variant(InputVariant::Borderless)
                                              .status(InputStatus::Warning)
                                              .size(ControlSize::Large)
                                              .defaultValue(u8"borderless warning"));
                                });
                                cell(String{u8"Underlined / disabled"}, [] {
                                    Input(InputProps{}
                                              .variant(InputVariant::Underlined)
                                              .disabled(true)
                                              .defaultValue(u8"underlined disabled"));
                                });
                            });
                            row([&] {
                                cell(String{u8"Grapheme / soft max / retained counter"}, [&] {
                                    Input(InputProps{}
                                              .ref(count_ref)
                                              .defaultValue(u8"é🙂中文")
                                              .count(count)
                                              .showCount(show_count));
                                });
                                cell(String{u8"Single edit formatter / history"}, [&] {
                                    Input(InputProps{}
                                              .ref(format_ref)
                                              .defaultValue(u8"x")
                                              .showCount()
                                              .count(InputCountOptions{3, InputCountUnit::Scalar})
                                              .exceedFormatter([](String value, std::size_t) {
                                                  auto bytes = std::string{value.bytes()};
                                                  std::erase(bytes, ' ');
                                                  return String::from_utf8(bytes).value();
                                              }));
                                });
                            });
                            row([&] {
                                cell(String{u8"InputRef / autoFocus / Email → Username"}, [&] {
                                    Input(InputProps{}
                                              .ref(email_ref)
                                              .autoFocus()
                                              .purpose(purpose)
                                              .autocorrect(false)
                                              .defaultValue(u8"desktop@example.com")
                                              .onFocus([&] { ++focus_events; }));
                                });
                                cell(String{u8"Vector clear / disabled / onClear"}, [&] {
                                    Input(InputProps{}
                                              .ref(clear_ref)
                                              .defaultValue(u8"clear 中文")
                                              .allowClear(true)
                                              .clearDisabled(clear_disabled)
                                              .clearIcon(icon_vector_sample())
                                              .showCount()
                                              .onClear([&] { ++clears; }));
                                });
                            });
                            row([&] {
                                cell(String{u8"Password / Hover / custom renderer / count"}, [&] {
                                    Password(PasswordProps{}
                                                 .ref(password_ref)
                                                 .defaultValue(u8"秘密🙂")
                                                 .visible(password_visible)
                                                 .visibilityToggle(password_toggle)
                                                 .action(password_action)
                                                 .showCount()
                                                 .iconRender([](bool visible) {
                                                     return visible ? icon_vector_sample()
                                                                    : IconSource{IconName::EyeInvisibleOutlined};
                                                 })
                                                 .onVisibleChange([&](bool visible) { password_visible.set(visible); }),
                                             InputPrefix{[] { Icon(IconProps{}.name(IconName::LockOutlined)); }});
                                });
                                cell(String{u8"Password / no visibility action / Warning"}, [] {
                                    Password(PasswordProps{}
                                                 .defaultValue(u8"hidden password")
                                                 .visibilityToggle(false)
                                                 .status(InputStatus::Warning));
                                });
                            });
                            row([&] {
                                cell(String{u8"Search / Filled / vector / Clear source"}, [&] {
                                    Search(SearchProps{}
                                               .ref(search_ref)
                                               .variant(InputVariant::Filled)
                                               .value(search_value)
                                               .showCount()
                                               .allowClear(true)
                                               .searchIcon(icon_vector_sample())
                                               .onChange([&](String value) { search_value.set(std::move(value)); })
                                               .onSearch([&](String value, SearchSource source) {
                                                   if (source == SearchSource::Clear) {
                                                       require_input(value.empty(), "Search Clear candidate differs");
                                                       ++clear_searches;
                                                   } else {
                                                       ++searches;
                                                   }
                                               }));
                                });
                                cell(String{u8"Search / Underlined / Small / typed content"}, [] {
                                    Search(SearchProps{}
                                               .variant(InputVariant::Underlined)
                                               .size(ControlSize::Small)
                                               .defaultValue(u8"Search"),
                                           SearchButtonContent{[] { Text(u8"查询"); }});
                                });
                            });
                            Tooltip(TooltipProps{}.open(popup), TooltipTrigger{[] { Text(u8"保留浮层与输入身份"); }},
                                    TooltipTitle{[] { Text(u8"Input / IME / retained scene"); }});
                        }});
                }});
        }});
        require_input(inputs.mounted_inputs().size() == 12, "Input sample inventory differs");
        const std::vector<detail::MountedInputComponent> mounted{inputs.mounted_inputs().begin(),
                                                                 inputs.mounted_inputs().end()};
        const auto first_scene = inputs.text_scene(mounted[0].component);
        const auto descendant_actions = [&](std::size_t index) {
            std::vector<input::InteractionId> result;
            for (const auto interaction : services.interactions().declaration_order()) {
                if (interaction == mounted[index].interaction) {
                    continue;
                }
                auto owner = services.interactions().require(interaction).component;
                while (owner.valid() && owner != mounted[index].component) {
                    const auto parent = services.components().parent(owner);
                    if (!parent) {
                        break;
                    }
                    owner = *parent;
                }
                if (owner == mounted[index].component) {
                    result.push_back(interaction);
                }
            }
            return result;
        };
        const auto clear_action = descendant_actions(7).at(0);
        const auto password_toggle_action = descendant_actions(8).at(0);
        const auto search_clear_action = descendant_actions(10).at(0);
        const auto search_parent = services.components().parent(mounted[10].component);
        const auto search_button =
            std::find_if(buttons.mounted_buttons().begin(), buttons.mounted_buttons().end(), [&](const auto& button) {
                return services.components().parent(button.component) == search_parent;
            });
        require_input(search_button != buttons.mounted_buttons().end(), "Search sibling button absent");
        const auto search_action = search_button->interaction;
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
                try {
                    require_input(
                        services.layout_and_synchronize(viewport, {0, 0, viewport.width, viewport.height}, {16, 12}),
                        "Input layout failed");
                } catch (...) {
                    for (std::size_t index = 0; index < mounted.size(); ++index) {
                        const auto& state = scene.text_state(inputs.text_scene(mounted[index].component));
                        if (state.last_error()) {
                            std::cerr << "input_text_error index=" << index
                                      << " kind=" << static_cast<int>(state.last_error().kind)
                                      << " byte=" << state.last_error().byte_offset
                                      << " font_kind=" << static_cast<int>(state.last_error().font_error.kind)
                                      << " diagnostic=" << state.last_error().font_error.diagnostic << '\n';
                        }
                    }
                    throw;
                }
                require_input(inputs.synchronize_input_area(double(metrics.coordinate_width) / viewport.width,
                                                            metrics.coordinate_width, metrics.coordinate_height),
                              "Input native area failed");
                require_input(resources.synchronize({&services.surfaces().instances(),
                                                     scene.atlas(),
                                                     scene.glyph_scene().instances(),
                                                     &services.rounded_effects(),
                                                     {static_cast<std::uint32_t>(metrics.pixel_width),
                                                      static_cast<std::uint32_t>(metrics.pixel_height), scale}}),
                              "Input upload failed");
                require_input(renderer.attach_scene(resources.attach(services.scene_composer().ordered_scene())),
                              "Input attach failed");
                renderer.set_clear_color(resolve_theme(theme.get()).alias().color_background_container);
                require_input(renderer.submit_frame(now) != runtime::FrameSubmissionResult::failed,
                              "Input frame failed");
                platform.delay(25);
            }
            require_input(renderer.save_frame_bmp(directory / (name + ".bmp")), "Input GPU readback failed");
        };
        const auto pointer = [&](input::InteractionId interaction, Uint32 type) {
            const auto& target_node = nodes.require(services.interactions().require(interaction).node);
            const auto bounds = target_node.bounds;
            auto* window = static_cast<SDL_Window*>(platform.window());
            SDL_Event event{};
            event.type = type;
            event.common.timestamp = acceptance::fixture_timestamp;
            const float x = (bounds.x + target_node.translation.x + bounds.width / 2) * scale / metrics.pixel_density;
            const float y = (bounds.y + target_node.translation.y + bounds.height / 2) * scale / metrics.pixel_density;
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
            require_input(SDL_PushEvent(&event), "Input SDL pointer injection failed");
            for (const auto& normalized : platform.poll_events().input.events()) {
                if (const auto* input = std::get_if<input::PointerInputEvent>(&normalized)) {
                    auto logical = *input;
                    logical.x *= metrics.display_scale / scale;
                    logical.y *= metrics.display_scale / scale;
                    services.pointer().dispatch(logical);
                    ++pointer_events;
                }
            }
        };
        const auto click = [&](input::InteractionId interaction) {
            pointer(interaction, SDL_EVENT_MOUSE_MOTION);
            pointer(interaction, SDL_EVENT_MOUSE_BUTTON_DOWN);
            pointer(interaction, SDL_EVENT_MOUSE_BUTTON_UP);
        };
        draw("initial");
        detail::TextSceneId count_label_scene;
        for (const auto& text : services.text().mounted_texts()) {
            if (scene.text_state(text.scene).content().bytes() == "4 / 20") {
                count_label_scene = text.scene;
            }
        }
        require_input(count_label_scene.valid(), "Native count label scene not found");
        require_input(traced.properties.type == input::TextInputType::email && !traced.properties.autocorrect &&
                          traced.areas > 0 && focus_events == 1 &&
                          services.focus().state().focused == mounted[6].interaction &&
                          SDL_TextInputActive(static_cast<SDL_Window*>(platform.window())),
                      "Input native autoFocus/Email hints failed");
        ThemeConfig dark;
        dark.algorithms = {ThemeAlgorithm::Dark};
        theme.set(dark);
        draw("dark");
        dark.algorithms.push_back(ThemeAlgorithm::Compact);
        theme.set(dark);
        draw("compact");
        const auto shape_count = scene.text_state(first_scene).counters().shape_count;
        variant.set(InputVariant::Filled);
        status.set(InputStatus::Warning);
        draw("variants-status");
        require_input(scene.text_state(first_scene).counters().shape_count == shape_count,
                      "Input material/variant update reshaped text");
        count.set({3, InputCountUnit::Grapheme});
        draw("soft-max");
        require_input(inputs.count_value(mounted[4].component) == 4 &&
                          inputs.status(mounted[4].component) == InputStatus::Error,
                      "Input native soft count differs");
        show_count.set(false);
        draw("count-hidden");
        require_input(inputs.count_text(mounted[4].component).empty(), "Hidden native counter kept its label");
        const auto hidden_range = scene.primitive(count_label_scene).instances;
        for (std::uint32_t index = hidden_range.first; index < hidden_range.first + hidden_range.count; ++index) {
            const auto clip = scene.glyph_scene().instances().at(index).clip_bounds;
            require_input(clip[2] <= clip[0] || clip[3] <= clip[1], "Hidden native counter retained GPU coverage");
        }
        show_count.set(true);
        require_input(format_ref.focus({InputFocusCursor::All}), "Input formatter ref focus failed");
        require_input(bool(inputs.dispatch(input::TextCommitted{String{u8"a b c"}, inputs.sessions().active()})),
                      "Input formatted native commit failed");
        draw("count-edit");
        require_input(inputs.editors().require(mounted[5].editor).value() == "abc", "Input formatted result differs");
        services.focus().dispatch({input::Key::z, input::KeyAction::down, input::KeyModifier::control});
        draw("count-undo");
        require_input(inputs.editors().require(mounted[5].editor).value() == "x", "Input native undo failed");
        services.focus().dispatch({input::Key::y, input::KeyAction::down, input::KeyModifier::control});
        draw("count-redo");
        require_input(inputs.editors().require(mounted[5].editor).value() == "abc", "Input native redo failed");
        require_input(email_ref.focus({InputFocusCursor::End}), "Email focus failed");
        const auto email_stamp = inputs.sessions().active();
        require_input(bool(inputs.dispatch(input::CompositionChanged{String{u8"ni"}, {0, 2}, email_stamp})),
                      "Input controlled preedit failed");
        purpose.set(InputPurpose::Username);
        draw("native-preedit");
        require_input(inputs.sessions().active() == email_stamp &&
                          traced.properties.type == input::TextInputType::email,
                      "Input hint change restarted active preedit");
        require_input(bool(inputs.dispatch(input::TextCommitted{String{u8"你"}, email_stamp})),
                      "Input native commit failed");
        draw("native-hints");
        require_input(traced.properties.type == input::TextInputType::username &&
                          inputs.sessions().active() != email_stamp &&
                          !bool(inputs.dispatch(input::TextCommitted{String{u8"stale"}, email_stamp})),
                      "Input native hint refresh/stale stamp failed");
        click(clear_action);
        draw("clear-disabled");
        require_input(clears == 0 && !inputs.editors().require(mounted[7].editor).value().empty(),
                      "Disabled native clear activated");
        clear_disabled.set(false);
        draw("clear-enabled");
        require_input(clear_ref.focus(), "Input clear focus failed");
        const auto clear_stamp = inputs.sessions().active();
        require_input(bool(inputs.dispatch(input::CompositionChanged{String{u8"preedit"}, {0, 7}, clear_stamp})),
                      "Input clear preedit failed");
        click(clear_action);
        draw("clear-empty");
        require_input(clears == 1 && inputs.editors().require(mounted[7].editor).value().empty() &&
                          !inputs.editors().require(mounted[7].editor).composition().active && traced.cancels > 0 &&
                          inputs.count_value(mounted[7].component) == 0,
                      "Native clear/editor/count transaction failed");
        require_input(password_ref.focus(), "Password focus failed");
        const auto password_stamp = inputs.sessions().active();
        require_input(bool(inputs.dispatch(input::CompositionChanged{String{u8"mi"}, {0, 2}, password_stamp})),
                      "Password preedit failed");
        const auto password_selection = inputs.editors().require(mounted[8].editor).selection();
        pointer(password_toggle_action, SDL_EVENT_MOUSE_MOTION);
        draw("password-visible");
        require_input(password_visible.get() && inputs.sessions().active() == password_stamp &&
                          inputs.editors().require(mounted[8].editor).selection() == password_selection &&
                          inputs.editors().require(mounted[8].editor).composition().active,
                      "Native Password Hover lost focus/selection/IME");
        password_toggle.set(false);
        draw("password-action-hidden");
        require_input(!services.interactions().require(password_toggle_action).eligible,
                      "Hidden native Password action remained eligible");
        password_toggle.set(true);
        password_action.set(PasswordAction::Click);
        require_input(bool(inputs.dispatch(input::TextCommitted{String{u8"密"}, password_stamp})),
                      "Password commit failed");
        require_input(search_ref.focus(), "Search focus failed");
        const auto search_stamp = inputs.sessions().active();
        require_input(bool(inputs.dispatch(input::CompositionChanged{String{u8"so"}, {0, 2}, search_stamp})),
                      "Search preedit failed");
        click(search_action);
        draw("search-submit");
        require_input(searches == 1 && inputs.sessions().active() == search_stamp &&
                          services.focus().state().focused == mounted[10].interaction &&
                          inputs.editors().require(mounted[10].editor).composition().active,
                      "Native Search action lost Input focus/IME");
        click(search_clear_action);
        draw("search-clear");
        require_input(clear_searches == 1 && search_value.get().empty(), "Native Search Clear source failed");
        search_value.set(String{u8"after resize"});
        auto* window = static_cast<SDL_Window*>(platform.window());
        require_input(SDL_SetWindowSize(window, 1420, 1000), "Input resize failed");
        platform.delay(100);
        draw("resized");
        click(search_action);
        draw("resize-pointer");
        require_input(searches == 2, "Resized native Search pointer missed action");
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
            require_input(!frames.pending() && !services.next_frame_deadline() &&
                              renderer.counters().frame_submissions == submissions,
                          "Native Input did not idle");
        }
        require_input(content_runs == 1 && metrics.coordinate_width == 1420 &&
                          inputs.text_scene(mounted[0].component) == first_scene &&
                          std::ranges::equal(mounted, inputs.mounted_inputs(),
                                             [](const auto& before, const auto& after) {
                                                 return before.component == after.component &&
                                                        before.editor == after.editor && before.node == after.node &&
                                                        before.interaction == after.interaction;
                                             }),
                      "Native Input replaced retained owners");
        services.dispose();
        require_input(!email_ref.bound() && !password_ref.bound() && !clear_ref.bound() && !count_ref.bound() &&
                          !format_ref.bound() && !search_ref.bound() && nodes.size() == 0 && scene.size() == 0 &&
                          services.interactions().size() == 0 && inputs.editors().size() == 0 &&
                          services.animations().diagnostics().scopes == 0 &&
                          services.animations().diagnostics().targets == 0,
                      "Native Input disposal leaked retained resources");
        resources.retire();
        scene_owner.reset();
        std::cout << "input_acceptance=passed gpu_driver=" << renderer.gpu_driver()
                  << " shader_format=" << renderer.shader_format() << " system_display_scale=" << metrics.display_scale
                  << " render_scale=" << scale << " pointer_events=" << pointer_events << " clears=" << clears
                  << " searches=" << searches << " clear_searches=" << clear_searches << " submits=" << submissions
                  << " content_runs=" << content_runs << " inputs=" << mounted.size()
                  << " native_starts=" << traced.starts << " native_areas=" << traced.areas
                  << " variants=4 count=passed history=passed hints=passed ime_stamp=passed actions=passed "
                     "resize=1420x1000 popup=passed idle_polls=3 deadline=none disposed=1 exit_code=0\n";
        return 0;
    } catch (const std::exception& error) {
        std::cerr << "input_acceptance_error=" << error.what() << '\n';
        return 1;
    }
}
} // namespace rynui::example
