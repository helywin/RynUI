#include "acceptance_events.hpp"
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
#include <cmath>
#include <filesystem>
#include <functional>
#include <iostream>
#include <string>

namespace rynui::example {
namespace {
void require_area(bool condition, const char* message) {
    if (!condition) {
        throw std::runtime_error(message);
    }
}

class AreaInputPlatform final : public ryn::input::TextInputPlatform {
public:
    explicit AreaInputPlatform(ryn::detail::PlatformState& platform) : platform_(platform) {}

    bool start(ryn::input::TextInputSessionStamp stamp,
               const ryn::input::TextInputProperties& properties) noexcept override {
        if (!platform_.start(stamp, properties)) {
            return false;
        }
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

    ryn::input::WindowTextInputArea last_area;
    std::size_t starts{};
    std::size_t areas{};

private:
    ryn::detail::PlatformState& platform_;
};
} // namespace

int run_text_area_acceptance(int argc, char** argv) {
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
        require_area(!directory.empty(), "TextArea requires --evidence-dir");
        std::filesystem::create_directories(directory);
        detail::PlatformConfig config;
        config.title = "RynUI TextArea Acceptance";
        config.width = 1600;
        config.height = 1100;
        auto created = detail::PlatformState::create(config);
        require_area(bool(created), "TextArea window failed");
        auto& platform = *created.state;
        acceptance::FixtureEventFilter event_filter{true};
        auto metrics = platform.window_metrics();
        const auto scale = requested_scale.value_or(metrics.display_scale);
        require_area(std::isfinite(scale) && scale > 0, "TextArea scale invalid");
        const auto executable = std::filesystem::absolute(argv[0]).parent_path();
        auto font_result = font::FontRuntime::create();
        require_area(bool(font_result), "TextArea font runtime failed");
        auto fonts = std::move(font_result.runtime);
        detail::DefaultFontChainRequest request;
        request.raster = {14, scale};
        request.fallback_latin = executable / "fonts/latin.ttf";
        request.fallback_cjk = executable / "fonts/cjk.otf";
        auto chain = detail::load_default_ui_font_chain(*fonts, request);
        require_area(bool(chain), "TextArea font chain failed");
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
        AreaInputPlatform traced{platform};
        detail::InputComponentHost inputs{services, traced, platform};
        inputs.set_display_scale(scale);
        services.set_motion_preference(animation::MotionPreference::reduced);
        Signal<ThemeConfig> theme{ThemeConfig{}};
        Signal<String> automatic{String{u8"autoSize\n第二行"}};
        Signal<bool> popup{false};
        TextAreaRef first_ref;
        TextAreaRef automatic_ref;
        TextAreaRef scrolling_ref;
        int content_runs{};
        int submits{};
        int clears{};
        int resize_notifications{};
        int pointer_events{};
        int wheel_events{};
        services.mount(Content{[&] {
            ++content_runs;
            Theme(ThemeProps{}.config(theme), ThemeContent{[&] {
                      Flex(FlexProps{}.vertical(true).align(FlexAlign::Start).gap(dp(8)), FlexContent{[&] {
                               Text(u8"TextArea：多行选择 / IME / autoSize / wheel / resize");
                               const auto row = [](std::function<void()> content) {
                                   Space(SpaceProps{}.size(dp(16)).align(SpaceAlign::Start), SpaceContent{content});
                               };
                               const auto cell = [](String label, std::function<void()> content) {
                                   Flex(FlexProps{}
                                            .vertical(true)
                                            .align(FlexAlign::Start)
                                            .gap(dp(2))
                                            .layout(LayoutStyle{}.width(dp(270))),
                                        FlexContent{[label, content] {
                                            Text(label);
                                            content();
                                        }});
                               };
                               row([&] {
                                   cell(String{u8"Outlined / Enter / selection / count"}, [&] {
                                       TextArea(TextAreaProps{}
                                                    .ref(first_ref)
                                                    .autoFocus()
                                                    .rows(2)
                                                    .defaultValue(u8"abcdef\nx\nabcdef\n中文 🙂\n最后一行")
                                                    .allowClear(true)
                                                    .showCount()
                                                    .count(InputCountOptions{60, InputCountUnit::Grapheme})
                                                    .onSubmit([&](String) { ++submits; }));
                                   });
                                   cell(String{u8"Filled / Error / Small"}, [] {
                                       TextArea(TextAreaProps{}
                                                    .rows(2)
                                                    .variant(InputVariant::Filled)
                                                    .status(InputStatus::Error)
                                                    .size(ControlSize::Small)
                                                    .defaultValue(u8"中文 filled\n第二行"));
                                   });
                               });
                               row([&] {
                                   cell(String{u8"Borderless / Warning / Large"}, [] {
                                       TextArea(TextAreaProps{}
                                                    .rows(2)
                                                    .variant(InputVariant::Borderless)
                                                    .status(InputStatus::Warning)
                                                    .size(ControlSize::Large)
                                                    .defaultValue(u8"borderless\n中文 🙂"));
                                   });
                                   cell(String{u8"Underlined / disabled"}, [] {
                                       TextArea(TextAreaProps{}
                                                    .rows(2)
                                                    .variant(InputVariant::Underlined)
                                                    .disabled(true)
                                                    .defaultValue(u8"禁用多行\ndisabled"));
                                   });
                               });
                               row([&] {
                                   cell(String{u8"autoSize 2–4 / clear / count"}, [&] {
                                       TextArea(TextAreaProps{}
                                                    .ref(automatic_ref)
                                                    .value(automatic)
                                                    .autoSize(TextAreaAutoSize{true, 2, 4})
                                                    .allowClear(true)
                                                    .showCount()
                                                    .onChange([&](String value) { automatic.set(std::move(value)); })
                                                    .onClear([&] { ++clears; }));
                                   });
                                   cell(String{u8"No wrap / wheel / resize Both"}, [&] {
                                       TextArea(
                                           TextAreaProps{}
                                               .ref(scrolling_ref)
                                               .rows(2)
                                               .wrap(false)
                                               .resize(TextAreaResize::Both)
                                               .defaultValue(u8"abcdefghijklmnopqrstuvwxyz 0123456789 中文\nsecond "
                                                             u8"line\nthird line\nfourth line\nfifth line\nsixth line")
                                               .onResize([&](TextAreaSize) { ++resize_notifications; }));
                                   });
                               });
                               row([&] {
                                   cell(String{u8"readOnly / copy / scroll"}, [] {
                                       TextArea(TextAreaProps{}.rows(2).readOnly(true).showCount().defaultValue(
                                           u8"只读可以选择复制\n第二行\n第三行\n第四行"));
                                   });
                                   cell(String{u8"Wrap / combining / emoji"}, [] {
                                       TextArea(
                                           TextAreaProps{}
                                               .rows(2)
                                               .resize(TextAreaResize::None)
                                               .defaultValue(u8"ffi é🙂 中文软换行 abcdefgh abcdefgh abcdefgh\n尾行"));
                                   });
                               });
                               Tooltip(TooltipProps{}.open(popup),
                                       TooltipTrigger{[] { Text(u8"保留浮层与多行编辑身份"); }},
                                       TooltipTitle{[] { Text(u8"TextArea / retained scene"); }});
                           }});
                  }});
        }});
        require_area(inputs.mounted_inputs().size() == 8, "TextArea sample inventory differs");
        const std::vector<detail::MountedInputComponent> mounted{inputs.mounted_inputs().begin(),
                                                                 inputs.mounted_inputs().end()};
        const auto first_scene = inputs.text_scene(mounted[0].component);
        const auto clear_action = [&] {
            for (const auto interaction : services.interactions().declaration_order()) {
                if (services.interactions().require(interaction).parent == mounted[4].interaction) {
                    return interaction;
                }
            }
            throw std::runtime_error("TextArea clear action absent");
        }();
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
                require_area(
                    services.layout_and_synchronize(viewport, {0, 0, viewport.width, viewport.height}, {16, 12}),
                    "TextArea layout failed");
                require_area(inputs.synchronize_input_area(double(metrics.coordinate_width) / viewport.width,
                                                           metrics.coordinate_width, metrics.coordinate_height),
                             "TextArea native input area failed");
                require_area(resources.synchronize({&services.surfaces().instances(),
                                                    scene.atlas(),
                                                    scene.glyph_scene().instances(),
                                                    &services.rounded_effects(),
                                                    {static_cast<std::uint32_t>(metrics.pixel_width),
                                                     static_cast<std::uint32_t>(metrics.pixel_height), scale}}),
                             "TextArea upload failed");
                const auto attachment = resources.attach(services.scene_composer().ordered_scene());
                if (!renderer.attach_scene(attachment)) {
                    std::cerr << "attach_state=" << name << " diagnostic=" << renderer.last_error() << '\n';
                    for (const auto command : attachment.scene()->commands()) {
                        const auto end = std::uint64_t{command.first_instance} + command.instance_count;
                        const auto limit = command.kind == graphics::SceneDrawKind::quad ? attachment.quad_count()
                                           : command.kind == graphics::SceneDrawKind::glyph
                                               ? attachment.glyph_count()
                                               : attachment.effects()->instance_count();
                        if (end > limit || !command.instance_count) {
                            std::cerr << "invalid_command kind=" << static_cast<int>(command.kind)
                                      << " first=" << command.first_instance << " count=" << command.instance_count
                                      << " limit=" << limit << '\n';
                        }
                    }
                    throw std::runtime_error("TextArea attach failed");
                }
                renderer.set_clear_color(resolve_theme(theme.get()).alias().color_background_container);
                require_area(renderer.submit_frame(now) != runtime::FrameSubmissionResult::failed,
                             "TextArea frame failed");
                platform.delay(25);
            }
            require_area(renderer.save_frame_bmp(directory / (name + ".bmp")), "TextArea GPU readback failed");
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
            require_area(SDL_PushEvent(&event), "TextArea SDL pointer injection failed");
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
        const auto click = [&](float x, float y) {
            pump_pointer(SDL_EVENT_MOUSE_MOTION, x, y);
            pump_pointer(SDL_EVENT_MOUSE_BUTTON_DOWN, x, y);
            pump_pointer(SDL_EVENT_MOUSE_BUTTON_UP, x, y);
        };
        const auto key = [&](input::Key value, input::KeyModifier modifiers = input::KeyModifier::none) {
            services.focus().dispatch({value, input::KeyAction::down, modifiers});
            services.focus().dispatch({value, input::KeyAction::up, modifiers});
        };
        draw("initial");
        require_area(SDL_TextInputActive(static_cast<SDL_Window*>(platform.window())) && traced.areas > 0,
                     "TextArea native input session inactive");
        ThemeConfig dark;
        dark.algorithms = {ThemeAlgorithm::Dark};
        theme.set(dark);
        draw("dark");
        dark.algorithms.push_back(ThemeAlgorithm::Compact);
        theme.set(dark);
        draw("compact");
        theme.set(ThemeConfig{});
        require_area(first_ref.select(1, 11), "TextArea multiline ref selection failed");
        draw("selection");
        auto& first_editor = inputs.editors().require(mounted[0].editor);
        require_area(first_editor.selection() == input::TextSelection{1, 11} &&
                         inputs.layout_snapshot(mounted[0].component).vertical_scroll > 0,
                     "TextArea selection did not span/reveal rows");
        require_area(traced.last_area.height <= std::ceil(22 * scale / metrics.pixel_density) + 1,
                     "TextArea IME area covered whole multiline viewport");
        const auto before = std::string{first_editor.value()};
        key(input::Key::enter);
        draw("enter");
        require_area(first_editor.value() != before && submits == 0, "TextArea plain Enter did not insert LF");
        key(input::Key::enter, input::KeyModifier::control);
        require_area(submits == 1, "TextArea primary Enter did not submit");
        key(input::Key::z, input::KeyModifier::control);
        draw("undo");
        require_area(first_editor.value() == before, "TextArea native LF undo failed");
        require_area(first_ref.focus({InputFocusCursor::End}), "TextArea end focus failed");
        const auto stamp = inputs.sessions().active();
        require_area(bool(inputs.dispatch(input::CompositionChanged{String{u8"预\n编"}, {}, stamp})),
                     "TextArea preedit failed");
        key(input::Key::enter);
        draw("preedit");
        require_area(first_editor.value() == before && first_editor.composition().active && submits == 1,
                     "TextArea IME Enter changed committed text");
        require_area(bool(inputs.dispatch(input::TextCommitted{String{u8"中\r\n文"}, stamp})),
                     "TextArea IME commit failed");
        draw("commit");
        require_area(first_editor.value() == before + "中\n文", "TextArea IME normalization failed");
        automatic.set(String{u8"autoSize\n二\n三\n四\n五\n六"});
        draw("autosize");
        const auto auto_height = nodes.require(mounted[4].node).bounds.height;
        require_area(inputs.caret_map(mounted[4].component).line_count() == 6 && auto_height > 90,
                     "TextArea autoSize maxRows failed");
        const auto& action_node = nodes.require(services.interactions().require(clear_action).node);
        click(action_node.bounds.x + action_node.translation.x + action_node.bounds.width / 2,
              action_node.bounds.y + action_node.translation.y + action_node.bounds.height / 2);
        draw("clear-empty");
        require_area(clears == 1 && automatic.get().empty() && inputs.count_value(mounted[4].component) == 0 &&
                         nodes.require(mounted[4].node).bounds.height < auto_height,
                     "TextArea clear/count/autoSize transaction failed");
        require_area(scrolling_ref.focus({InputFocusCursor::Start}), "TextArea scroll focus failed");
        draw("scroll-start");
        const auto scroll_scene = inputs.text_scene(mounted[5].component);
        const auto shapes = scene.text_state(scroll_scene).counters().shape_count;
        const auto rasters = fonts->counters().rasterizations;
        const auto viewport = inputs.layout_snapshot(mounted[5].component).viewport;
        SDL_Event wheel{};
        wheel.type = SDL_EVENT_MOUSE_WHEEL;
        wheel.common.timestamp = acceptance::fixture_timestamp;
        wheel.wheel.windowID = SDL_GetWindowID(static_cast<SDL_Window*>(platform.window()));
        wheel.wheel.x = -1;
        wheel.wheel.y = -1;
        wheel.wheel.mouse_x = (viewport.x + 4) * scale / metrics.pixel_density;
        wheel.wheel.mouse_y = (viewport.y + 4) * scale / metrics.pixel_density;
        require_area(SDL_PushEvent(&wheel), "TextArea SDL wheel injection failed");
        for (const auto& normalized : platform.poll_events().input.events()) {
            if (const auto* event = std::get_if<input::ScrollInputEvent>(&normalized)) {
                auto logical = *event;
                logical.x *= metrics.display_scale / scale;
                logical.y *= metrics.display_scale / scale;
                require_area(inputs.dispatch(logical), "TextArea SDL wheel not consumed");
                ++wheel_events;
            }
        }
        draw("wheel");
        require_area(wheel_events == 1 && inputs.layout_snapshot(mounted[5].component).vertical_scroll > 0 &&
                         scene.text_state(scroll_scene).counters().shape_count == shapes &&
                         fonts->counters().rasterizations == rasters,
                     "TextArea native wheel lost position or shaped/rasterized");
        const auto& resize_node = nodes.require(mounted[5].node);
        const auto bounds = resize_node.bounds;
        const auto offset = resize_node.translation;
        const auto mouse = input::PointerIdentity::mouse();
        pump_pointer(SDL_EVENT_MOUSE_BUTTON_DOWN, bounds.x + offset.x + bounds.width - 4,
                     bounds.y + offset.y + bounds.height - 4);
        require_area(services.pointer().state(mouse)->capture == mounted[5].interaction,
                     "TextArea native resize capture failed");
        pump_pointer(SDL_EVENT_MOUSE_MOTION, bounds.x + offset.x + bounds.width - 44,
                     bounds.y + offset.y + bounds.height + 20);
        pump_pointer(SDL_EVENT_MOUSE_BUTTON_UP, bounds.x + offset.x + bounds.width - 44,
                     bounds.y + offset.y + bounds.height + 20);
        draw("resized-area");
        require_area(nodes.require(mounted[5].node).bounds.width < bounds.width &&
                         nodes.require(mounted[5].node).bounds.height > bounds.height &&
                         !services.pointer().state(mouse)->capture && resize_notifications >= 2,
                     "TextArea native resize dimensions/capture failed");
        require_area(scrolling_ref.select(0, 0), "TextArea resized selection reset failed");
        draw("resize-pointer");
        const auto visible = inputs.layout_snapshot(mounted[5].component).viewport;
        const auto stop = inputs.caret_map(mounted[5].component).line_stops(1)[1];
        click(visible.x + stop.x, visible.y + 27);
        draw("pointer-hit");
        require_area(inputs.editors().require(mounted[5].editor).selection().caret == stop.byte,
                     "TextArea resized two-dimensional pointer missed caret");
        auto* window = static_cast<SDL_Window*>(platform.window());
        require_area(SDL_SetWindowSize(window, 1420, 1000), "TextArea window resize failed");
        platform.delay(100);
        draw("resized-window");
        const auto after = inputs.layout_snapshot(mounted[5].component).viewport;
        click(after.x + stop.x, after.y + 27);
        draw("window-pointer-hit");
        require_area(inputs.editors().require(mounted[5].editor).selection().caret == stop.byte,
                     "TextArea window resize pointer missed caret");
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
            require_area(!frames.pending() && !services.next_frame_deadline() &&
                             renderer.counters().frame_submissions == submissions,
                         "TextArea native idle failed");
        }
        require_area(content_runs == 1 && inputs.text_scene(mounted[0].component) == first_scene &&
                         std::ranges::equal(mounted, inputs.mounted_inputs(),
                                            [](const auto& before, const auto& after) {
                                                return before.component == after.component &&
                                                       before.editor == after.editor && before.node == after.node &&
                                                       before.interaction == after.interaction;
                                            }),
                     "TextArea native owners rebuilt");
        services.dispose();
        require_area(!first_ref.bound() && !automatic_ref.bound() && !scrolling_ref.bound() && nodes.size() == 0 &&
                         scene.size() == 0 && inputs.editors().size() == 0 && services.interactions().size() == 0 &&
                         !services.pointer().state(mouse)->capture && services.animations().diagnostics().scopes == 0 &&
                         services.animations().diagnostics().targets == 0,
                     "TextArea native disposal leaked");
        resources.retire();
        scene_owner.reset();
        std::cout << "text_area_acceptance=passed gpu_driver=" << renderer.gpu_driver()
                  << " shader_format=" << renderer.shader_format() << " system_display_scale=" << metrics.display_scale
                  << " render_scale=" << scale << " inputs=" << mounted.size() << " content_runs=" << content_runs
                  << " pointer_events=" << pointer_events << " wheel_events=" << wheel_events
                  << " native_starts=" << traced.starts << " native_areas=" << traced.areas
                  << " resize_notifications=" << resize_notifications << " submits=" << submissions
                  << " variants=4 selection=passed ime_area=passed ime_stamp=passed history=passed"
                     " autosize=passed count=passed clear=passed wheel=passed resize=passed pointer_hit=passed"
                     " window_resize=1420x1000 popup=passed idle_polls=3 deadline=none disposed=1 exit_code=0\n";
        return 0;
    } catch (const std::exception& error) {
        std::cerr << "text_area_acceptance_error=" << error.what() << '\n';
        return 1;
    }
}
} // namespace rynui::example
