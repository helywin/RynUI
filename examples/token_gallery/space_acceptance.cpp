#include "acceptance_events.hpp"
#include "component/button_component.hpp"
#include "component/input_component.hpp"
#include "component/selection_component.hpp"
#include "component/space_addon.hpp"
#include "component/space_compact.hpp"
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
void require_space(bool value, const char* message) {
    if (!value) {
        throw std::runtime_error(message);
    }
}
} // namespace

int run_space_acceptance(int argc, char** argv) {
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
        require_space(!directory.empty(), "Space requires --evidence-dir");
        std::filesystem::create_directories(directory);
        detail::PlatformConfig config;
        config.title = "RynUI Space Acceptance";
        config.width = 1600;
        config.height = 1000;
        auto created = detail::PlatformState::create(config);
        require_space(bool(created), "Space window failed");
        auto& platform = *created.state;
        acceptance::FixtureEventFilter event_filter{true};
        auto metrics = platform.window_metrics();
        const float scale = requested_scale.value_or(metrics.display_scale);
        require_space(std::isfinite(scale) && scale > 0, "Space scale invalid");
        const auto executable = std::filesystem::absolute(argv[0]).parent_path();
        auto font_result = font::FontRuntime::create();
        require_space(bool(font_result), "Space font runtime failed");
        auto fonts = std::move(font_result.runtime);
        detail::DefaultFontChainRequest request;
        request.raster = {14, scale};
        request.fallback_latin = executable / "fonts/latin.ttf";
        request.fallback_cjk = executable / "fonts/cjk.otf";
        auto chain = detail::load_default_ui_font_chain(*fonts, request);
        require_space(bool(chain), "Space system font chain failed");
        const auto resolver = detail::make_default_ui_font_resolver(*fonts, chain, scale);
        runtime::NodeStore nodes;
        runtime::FrameRequestState frames;
        runtime::DirtyQueues dirty{nodes, &frames};
        layout::LayoutEngine layout{nodes};
        text::TextEngine engine{*fonts};
        detail::TextSceneService scene{*fonts, engine, frames};
        detail::WindowComponentServices services{nodes, layout, dirty, scene, resolver, frames};
        detail::ButtonComponentHost buttons{services};
        detail::InputComponentHost inputs{services, platform, platform};
        inputs.set_display_scale(scale);
        detail::SelectionComponentHost selections{services};
        Signal<ControlSize> size{ControlSize::Middle};
        Signal<FlexDirection> direction{FlexDirection::LeftToRight};
        Signal<SpaceOrientation> orientation{SpaceOrientation::Horizontal};
        Signal<InputStatus> status{InputStatus::Default};
        Signal<InputVariant> variant{InputVariant::Outlined};
        Signal<ThemeConfig> theme{ThemeConfig{}};
        Signal<bool> open{false};
        Signal<LogicalLength> width{dp(560)};
        ButtonRef reference;
        int content_runs{};
        int labels{};
        int clicks{};
        int pointer_events{};
        int text_events{};
        const auto label = [&](String caption) {
            return ButtonContent{[&, caption] {
                ++labels;
                Text(caption);
            }};
        };
        services.mount(Content{[&] {
            ++content_runs;
            Theme(
                ThemeProps{}.config(theme), ThemeContent{[&] {
                    Flex(
                        FlexProps{}.vertical(true).align(FlexAlign::Start).gap(dp(8)), FlexContent{[&] {
                            Text(u8"Space 原生分隔 / 基线 / Compact / Addon / 编辑 / 浮层");
                            Space(
                                SpaceProps{}.align(SpaceAlign::Center).separator(SpaceSeparator{[] { Text(u8" / "); }}),
                                SpaceContent{[&] {
                                    for (const auto caption : {u8"第一项", u8"第二项", u8"第三项"}) {
                                        Button(
                                            ButtonProps{},
                                            label(String::from_utf8(reinterpret_cast<const char*>(caption)).value()));
                                    }
                                }});
                            Text(u8"基线：14 / 16 dp 字体与大尺寸控件");
                            Space(SpaceProps{}.align(SpaceAlign::Baseline), SpaceContent{[&] {
                                      Text(u8"Ag 小字");
                                      Button(ButtonProps{}.size(ControlSize::Large), label(String{u8"Ag 大按钮"}));
                                      Input(InputProps{}
                                                .size(ControlSize::Large)
                                                .defaultValue(u8"Ag 输入")
                                                .layout(LayoutStyle{}.width(dp(140))));
                                  }});
                            Text(u8"Compact：共有边 / 三尺寸 / RTL / H-V / Search");
                            SpaceCompact(SpaceCompactProps{}
                                             .size(size)
                                             .direction(direction)
                                             .orientation(orientation)
                                             .block(true)
                                             .layout(LayoutStyle{}.width(width)),
                                         SpaceCompactContent{[&] {
                                             Button(ButtonProps{}.ref(reference).onClick([&] { ++clicks; }),
                                                    label(String{u8"操作 A"}));
                                             Button(ButtonProps{}.variant(ButtonVariant::Dashed),
                                                    label(String{u8"虚线 B"}));
                                             Input(InputProps{}.status(status).defaultValue(u8"中文").layout(
                                                 LayoutStyle{}.width(dp(140))));
                                             Search(SearchProps{}.status(status).defaultValue(u8"搜索").layout(
                                                 LayoutStyle{}.flex_grow(1).min_width(dp(0))));
                                         }});
                            Text(u8"mixed：Addon / Password / RadioButton / nested");
                            SpaceCompact(
                                SpaceCompactProps{}.size(size).direction(direction), SpaceCompactContent{[&] {
                                    SpaceAddon(SpaceAddonProps{}.variant(variant).status(status),
                                               SpaceAddonContent{[] { Text(u8"https://"); }});
                                    Password(
                                        PasswordProps{}.defaultValue(u8"秘密").layout(LayoutStyle{}.width(dp(140))));
                                    RadioButton(RadioProps{}, RadioLabel{[] { Text(u8"选择"); }});
                                    SpaceCompact(SpaceCompactProps{}, SpaceCompactContent{[&] {
                                                     Button(ButtonProps{}.danger(true), label(String{u8"嵌套 A"}));
                                                     Button(ButtonProps{}, label(String{u8"嵌套 B"}));
                                                 }});
                                }});
                            Space(SpaceProps{}.wrap(true), SpaceContent{[&] {
                                      for (const auto item : {InputVariant::Outlined, InputVariant::Filled,
                                                              InputVariant::Borderless, InputVariant::Underlined}) {
                                          SpaceCompact(SpaceCompactProps{}, SpaceCompactContent{[&, item] {
                                                           SpaceAddon(SpaceAddonProps{}
                                                                          .variant(item)
                                                                          .status(InputStatus::Warning)
                                                                          .disabled(item == InputVariant::Outlined),
                                                                      SpaceAddonContent{[] { Text(u8"附加"); }});
                                                           Button(ButtonProps{}, label(String{u8"操作"}));
                                                       }});
                                      }
                                  }});
                            Tooltip(TooltipProps{}.open(open),
                                    TooltipTrigger{[&] { Button(ButtonProps{}, label(String{u8"浮层编辑"})); }},
                                    TooltipTitle{[] { Input(InputProps{}.defaultValue(u8"浮层")); }});
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
                require_space(
                    services.layout_and_synchronize(viewport, {0, 0, viewport.width, viewport.height}, {12, 8}),
                    "Space layout failed");
                require_space(inputs.synchronize_input_area(scale / metrics.pixel_density, metrics.coordinate_width,
                                                            metrics.coordinate_height),
                              "Space native input area failed");
                require_space(resources.synchronize({&services.surfaces().instances(),
                                                     scene.atlas(),
                                                     scene.glyph_scene().instances(),
                                                     &services.rounded_effects(),
                                                     {static_cast<std::uint32_t>(metrics.pixel_width),
                                                      static_cast<std::uint32_t>(metrics.pixel_height), scale}}),
                              "Space upload failed");
                require_space(renderer.attach_scene(resources.attach(services.scene_composer().ordered_scene())),
                              "Space attach failed");
                renderer.set_clear_color(resolve_theme(theme.get()).alias().color_background_container);
                require_space(renderer.submit_frame(now) != runtime::FrameSubmissionResult::failed,
                              "Space frame failed");
                platform.delay(25);
            }
            require_space(renderer.save_frame_bmp(directory / (name + ".bmp")), "Space readback failed");
        };
        auto* window = static_cast<SDL_Window*>(platform.window());
        const auto window_id = SDL_GetWindowID(window);
        const auto route_events = [&] {
            for (const auto& event : platform.poll_events().input.events()) {
                if (const auto* pointer = std::get_if<input::PointerInputEvent>(&event)) {
                    auto logical = *pointer;
                    logical.x *= metrics.display_scale / scale;
                    logical.y *= metrics.display_scale / scale;
                    services.pointer().dispatch(logical);
                    ++pointer_events;
                } else if (const auto* committed = std::get_if<input::TextCommitted>(&event)) {
                    require_space(bool(inputs.dispatch(*committed)), "Space SDL commit rejected");
                    ++text_events;
                } else if (const auto* composition = std::get_if<input::CompositionChanged>(&event)) {
                    require_space(bool(inputs.dispatch(*composition)), "Space SDL composition rejected");
                    ++text_events;
                }
            }
        };
        const auto pointer = [&](Uint32 type, runtime::Point point) {
            SDL_Event event{};
            event.type = type;
            event.common.timestamp = acceptance::fixture_timestamp;
            const float x = point.x * scale / metrics.pixel_density;
            const float y = point.y * scale / metrics.pixel_density;
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
            require_space(SDL_PushEvent(&event), "Space SDL pointer injection failed");
            route_events();
        };
        const auto center = [&](runtime::NodeId node) {
            const auto bounds = nodes.require(node).bounds;
            return runtime::Point{bounds.x + bounds.width / 2, bounds.y + bounds.height / 2};
        };
        draw("initial");
        const auto root = services.components().root_components().front();
        const auto sections = services.components().children(root);
        const auto separator = sections[1];
        const auto baseline = sections[3];
        const auto compact = services.components().state<detail::SpaceCompactState>(sections[5])->context;
        const auto addon = services.components().children(sections[7]).front();
        require_space(services.components().children(separator).size() == 5,
                      "Space separator did not interleave three items");
        const auto baseline_items = services.components().children(baseline);
        const auto baseline_position = [&](runtime::ComponentId item) {
            const auto& node = nodes.require(services.components().root(item));
            require_space(node.first_baseline.has_value(), "Space baseline missing");
            return node.bounds.y + *node.first_baseline;
        };
        for (const auto item : baseline_items) {
            require_space(std::abs(baseline_position(item) - baseline_position(baseline_items.front())) < .001F,
                          "Space system font baseline differs");
        }
        const auto a = buttons.mounted_buttons()[4];
        const auto b = buttons.mounted_buttons()[5];
        const auto editable = inputs.mounted_inputs()[1];
        const auto popup = inputs.mounted_inputs().back();
        const auto editor_id = editable.editor;
        const auto scene_id = inputs.text_scene(editable.component);
        require_space(!services.focus().request_focus(popup.interaction, input::FocusModality::keyboard),
                      "hidden popup Input accepted focus");
        pointer(SDL_EVENT_MOUSE_MOTION, center(a.node));
        draw("hover");
        require_space(!compact->seams().empty() &&
                          compact->seams().front().material.color == buttons.snapshot(a.component).presentation_border,
                      "Space hover lost shared seam priority");
        pointer(SDL_EVENT_MOUSE_MOTION, {700, 460});
        require_space(services.focus().request_focus(editable.interaction, input::FocusModality::keyboard),
                      "Space Input focus failed");
        const auto stamp = inputs.sessions().active();
        draw("focus");
        const auto measured = nodes.require(a.node).measure_count;
        direction.set(FlexDirection::RightToLeft);
        draw("rtl");
        require_space(nodes.require(a.node).bounds.x > nodes.require(b.node).bounds.x &&
                          nodes.require(a.node).measure_count == measured,
                      "Space RTL remounted or remeasured");
        size.set(ControlSize::Small);
        draw("small");
        size.set(ControlSize::Large);
        draw("large");
        require_space(inputs.sessions().active() == stamp && inputs.mounted_inputs()[1].editor == editor_id &&
                          inputs.text_scene(editable.component) == scene_id,
                      "Space size/RTL replaced editor session or scene");
        orientation.set(SpaceOrientation::Vertical);
        draw("vertical-rtl");
        require_space(buttons.snapshot(a.component).compact_corners == std::array{true, true, false, false},
                      "Space vertical exterior corners differ");
        orientation.set(SpaceOrientation::Horizontal);
        size.set(ControlSize::Middle);
        status.set(InputStatus::Error);
        draw("mixed-status");
        variant.set(InputVariant::Filled);
        draw("filled-error");
        require_space(services.space_addon().snapshot(addon).fill == Color::rgba8(255, 242, 240),
                      "Space filled error palette differs");
        ThemeConfig dark;
        dark.algorithms = {ThemeAlgorithm::Dark};
        theme.set(dark);
        draw("dark");
        auto& editor = inputs.editors().require(editable.editor);
        require_space(bool(editor.move(input::TextCaretMove::end)), "Space caret move failed");
        SDL_Event composition{};
        composition.type = SDL_EVENT_TEXT_EDITING;
        composition.edit.windowID = window_id;
        composition.edit.text = "ni";
        composition.edit.start = 2;
        composition.edit.length = 0;
        require_space(SDL_PushEvent(&composition), "Space SDL IME injection failed");
        route_events();
        require_space(editor.composition().active, "Space normalized SDL preedit did not reach retained editor");
        SDL_Event commit{};
        commit.type = SDL_EVENT_TEXT_INPUT;
        commit.text.windowID = window_id;
        commit.text.text = "你";
        require_space(SDL_PushEvent(&commit), "Space SDL text injection failed");
        route_events();
        require_space(editor.value() == "中文你", "Space SDL Unicode commit differs");
        draw("unicode-edit");
        open.set(true);
        draw("popup-open");
        require_space(services.focus().request_focus(popup.interaction, input::FocusModality::keyboard),
                      "Space popup Input cannot focus");
        pointer(SDL_EVENT_MOUSE_BUTTON_DOWN, center(popup.node));
        const auto popup_stamp = inputs.sessions().active();
        require_space(bool(inputs.dispatch(input::CompositionChanged{String{u8"ni"}, {2, 0}, popup_stamp})),
                      "Space popup IME failed");
        draw("popup-edit");
        open.set(false);
        require_space(!inputs.sessions().active().valid() && !services.focus().state().focused &&
                          !services.pointer().state(input::PointerIdentity::mouse())->capture,
                      "Space popup close retained focus/IME/capture");
        draw("popup-closed");
        for (const auto type : {SDL_EVENT_MOUSE_MOTION, SDL_EVENT_MOUSE_BUTTON_DOWN, SDL_EVENT_MOUSE_BUTTON_UP}) {
            pointer(type, center(a.node));
        }
        require_space(clicks == 1 && reference.blur(), "Space normalized updated pointer hit or ref failed");
        draw("pointer-hit");
        const auto last_nested = buttons.mounted_buttons()[8];
        require_space(services.destroy(last_nested.component), "Space nested deletion failed");
        draw("nested-single");
        require_space(SDL_SetWindowSize(window, 1420, 900), "Space resize failed");
        platform.delay(100);
        width.set(dp(450));
        draw("resized");
        require_space(metrics.coordinate_width == 1420 && content_runs == 1 && labels == 13 && pointer_events == 6 &&
                          text_events == 2,
                      "Space resized topology or normalized event inventory differs");
        services.set_window_active(false);
        draw("inactive");
        static_cast<void>(frames.consume_request());
        const auto submissions = renderer.counters().frame_submissions;
        for (int idle = 0; idle < 3; ++idle) {
            platform.delay(25);
            static_cast<void>(platform.poll_events());
            microseconds += 50000;
            static_cast<void>(services.tick_animations(animation::AnimationTime::microseconds(microseconds)));
            require_space(!frames.pending() && !services.next_frame_deadline() &&
                              renderer.counters().frame_submissions == submissions,
                          "Space idle retained frame deadline");
        }
        services.dispose();
        require_space(!reference.bound() && nodes.size() == 0 && inputs.editors().size() == 0 &&
                          services.interactions().size() == 0 && scene.size() == 0 &&
                          services.rounded_effects().live_count() == 0,
                      "Space disposal leaked retained resources");
        std::cout << "space_acceptance=passed gpu_driver=" << renderer.gpu_driver()
                  << " shader_format=" << renderer.shader_format() << " system_display_scale=" << metrics.display_scale
                  << " render_scale=" << scale << " pointer_events=" << pointer_events << " text_events=" << text_events
                  << " clicks=" << clicks << " submits=" << submissions << " content_runs=" << content_runs
                  << " label_runs=" << labels
                  << " baselines=matched editor_identity=retained popup_cleanup=passed resize=1420x900 idle_polls=3 "
                     "deadline=none disposed=1 exit_code=0\n";
        return 0;
    } catch (const std::exception& error) {
        std::cerr << "space_acceptance_error=" << error.what() << '\n';
        return 1;
    }
}
} // namespace rynui::example
