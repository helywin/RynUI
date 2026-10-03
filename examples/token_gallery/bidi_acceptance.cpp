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
#include <iostream>
#include <string>

namespace rynui::example {
namespace {
void require_bidi(bool value, const char* message) {
    if (!value) {
        throw std::runtime_error(message);
    }
}

class BidiInputPlatform final : public ryn::input::TextInputPlatform {
public:
    explicit BidiInputPlatform(ryn::detail::PlatformState& platform) : platform_(platform) {}

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
        area = value;
        ++areas;
        return true;
    }

    ryn::input::WindowTextInputArea area;
    std::size_t starts{};
    std::size_t areas{};

private:
    ryn::detail::PlatformState& platform_;
};
} // namespace

int run_bidi_acceptance(int argc, char** argv) {
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
        require_bidi(!directory.empty(), "Bidi requires --evidence-dir");
        std::filesystem::create_directories(directory);
        detail::PlatformConfig config;
        config.title = "RynUI Bidirectional Text Acceptance";
        config.width = 1600;
        config.height = 1100;
        auto created = detail::PlatformState::create(config);
        require_bidi(bool(created), "Bidi window failed");
        auto& platform = *created.state;
        acceptance::FixtureEventFilter filter{true};
        auto metrics = platform.window_metrics();
        const auto scale = requested_scale.value_or(metrics.display_scale);
        require_bidi(std::isfinite(scale) && scale > 0, "Bidi scale invalid");
        const auto executable = std::filesystem::absolute(argv[0]).parent_path();
        auto font_result = font::FontRuntime::create();
        require_bidi(bool(font_result), "Bidi font runtime failed");
        auto fonts = std::move(font_result.runtime);
        detail::DefaultFontChainRequest request;
        request.raster = {14, scale};
        request.fallback_latin = executable / "fonts/latin.ttf";
        request.fallback_cjk = executable / "fonts/cjk.otf";
        auto chain = detail::load_default_ui_font_chain(*fonts, request);
        require_bidi(bool(chain) && chain.uses_system_fonts, "Bidi system chain absent");
        for (const auto scalar : {U'م', U'ר', U'ب', U'א'}) {
            require_bidi(bool(fonts->find_glyph(chain.identities(), scalar, {})),
                         "System font missing Arabic/Hebrew coverage");
        }
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
        BidiInputPlatform traced{platform};
        detail::InputComponentHost inputs{services, traced, platform};
        inputs.set_display_scale(scale);
        services.set_motion_preference(animation::MotionPreference::reduced);
        Signal<TextDirection> direction{TextDirection::Auto};
        Signal<bool> popup{false};
        Signal<ThemeConfig> theme{ThemeConfig{}};
        InputRef mixed_ref;
        InputRef rtl_ref;
        InputRef mask_ref;
        TextAreaRef area_ref;
        int content_runs{};
        int pointer_events{};
        services.mount(Content{[&] {
            ++content_runs;
            Theme(ThemeProps{}.config(theme), ThemeContent{[&] {
                      Flex(FlexProps{}.vertical(true).align(FlexAlign::Start).gap(dp(3)), FlexContent{[&] {
                               Text(u8"Unicode 17 · Arabic / Hebrew / numbers / brackets / logical editing");
                               Title(TitleProps{}
                                         .level(TypographyLevel::H3)
                                         .content(u8"مرحبا (12) אבג RynUI")
                                         .direction(direction));
                               Paragraph(TypographyProps{}
                                             .content(u8"אבג (12) مرحبا 中文\nمرحبا 34 אבג (office)")
                                             .direction(direction)
                                             .underline(true)
                                             .layout(LayoutStyle{}.width(dp(380))));
                               Text(u8"Mixed selection: A + space + Alef; visual gap stays clear");
                               Input(InputProps{}
                                         .defaultValue(u8"A אבג 12 B")
                                         .direction(direction)
                                         .ref(mixed_ref)
                                         .autoFocus()
                                         .layout(LayoutStyle{}.width(dp(380))));
                               Input(InputProps{}.defaultValue(u8"אבג").ref(rtl_ref).layout(
                                   LayoutStyle{}.width(dp(380))));
                               Password(PasswordProps{}
                                            .defaultValue(u8"א👩‍💻ب")
                                            .direction(direction)
                                            .ref(mask_ref)
                                            .layout(LayoutStyle{}.width(dp(380))));
                               TextArea(TextAreaProps{}
                                            .defaultValue(u8"אבג 12 דהו 34 مرحبا office 中文\nمرحبا 56 אבג\n\nאבג")
                                            .direction(direction)
                                            .rows(3)
                                            .ref(area_ref)
                                            .layout(LayoutStyle{}.width(dp(220))));
                               OTP(OTPProps{}
                                       .length(3)
                                       .defaultValue(u8"אבג")
                                       .direction(OTPDirection::RightToLeft)
                                       .mask(true));
                               Tooltip(TooltipProps{}.open(popup), TooltipTrigger{[] { Text(u8"保留浮层 / Bidi"); }},
                                       TooltipTitle{[] { Text(u8"مرحبا אבג 123"); }});
                           }});
                  }});
        }});
        const std::vector<detail::MountedInputComponent> mounted{inputs.mounted_inputs().begin(),
                                                                 inputs.mounted_inputs().end()};
        require_bidi(mounted.size() == 7, "Bidi editor inventory");
        const auto mixed_scene = inputs.text_scene(mounted[0].component);
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
                require_bidi(
                    services.layout_and_synchronize(viewport, {0, 0, viewport.width, viewport.height}, {16, 12}),
                    "Bidi layout");
                require_bidi(inputs.synchronize_input_area(double(metrics.coordinate_width) / viewport.width,
                                                           metrics.coordinate_width, metrics.coordinate_height),
                             "Bidi native input area");
                require_bidi(resources.synchronize({&services.surfaces().instances(),
                                                    scene.atlas(),
                                                    scene.glyph_scene().instances(),
                                                    &services.rounded_effects(),
                                                    {static_cast<std::uint32_t>(metrics.pixel_width),
                                                     static_cast<std::uint32_t>(metrics.pixel_height), scale}}),
                             "Bidi upload");
                require_bidi(renderer.attach_scene(resources.attach(services.scene_composer().ordered_scene())),
                             "Bidi attach");
                renderer.set_clear_color(resolve_theme(theme.get()).alias().color_background_container);
                require_bidi(renderer.submit_frame(now) != runtime::FrameSubmissionResult::failed, "Bidi frame");
                platform.delay(25);
            }
            require_bidi(renderer.save_frame_bmp(directory / (name + ".bmp")), "Bidi GPU readback");
        };
        const auto key = [&](input::Key value, input::KeyModifier modifiers = input::KeyModifier::none) {
            services.focus().dispatch({value, input::KeyAction::down, modifiers});
            services.focus().dispatch({value, input::KeyAction::up, modifiers});
        };
        const auto click = [&](float x, float y) {
            for (const auto type : {SDL_EVENT_MOUSE_MOTION, SDL_EVENT_MOUSE_BUTTON_DOWN, SDL_EVENT_MOUSE_BUTTON_UP}) {
                SDL_Event event{};
                event.type = type;
                event.common.timestamp = acceptance::fixture_timestamp;
                const auto window_id = SDL_GetWindowID(static_cast<SDL_Window*>(platform.window()));
                if (type == SDL_EVENT_MOUSE_MOTION) {
                    event.motion.windowID = window_id;
                    event.motion.x = x * scale / metrics.pixel_density;
                    event.motion.y = y * scale / metrics.pixel_density;
                } else {
                    event.button.windowID = window_id;
                    event.button.button = SDL_BUTTON_LEFT;
                    event.button.down = type == SDL_EVENT_MOUSE_BUTTON_DOWN;
                    event.button.x = x * scale / metrics.pixel_density;
                    event.button.y = y * scale / metrics.pixel_density;
                }
                require_bidi(SDL_PushEvent(&event), "Bidi SDL pointer injection");
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
        };
        draw("initial");
        require_bidi(SDL_TextInputActive(static_cast<SDL_Window*>(platform.window())) && traced.areas > 0,
                     "Bidi SDL session inactive");
        require_bidi(mixed_ref.select(0, 4), "Bidi mixed select");
        draw("selection");
        const auto& carets = inputs.caret_map(mounted[0].component);
        std::size_t pieces{};
        require_bidi(carets.visit_coverage(0, 4, carets.revision(), [&](const auto&) { ++pieces; }) && pieces == 2,
                     "Bidi disjoint coverage absent");
        const auto shapes = scene.text_state(mixed_scene).counters().shape_count;
        key(input::Key::c, input::KeyModifier::control);
        const auto copied = platform.read_text();
        require_bidi(copied.text && *copied.text == String{u8"A א"}, "Bidi native clipboard order");
        require_bidi(rtl_ref.focus({InputFocusCursor::Start}), "Bidi RTL focus");
        draw("rtl-start");
        const auto right_x = inputs.layout_snapshot(mounted[1].component).caret_x;
        key(input::Key::left);
        draw("rtl-left");
        auto& rtl_editor = inputs.editors().require(mounted[1].editor);
        require_bidi(rtl_editor.selection().caret == 2 &&
                         inputs.layout_snapshot(mounted[1].component).caret_x < right_x,
                     "Bidi visual Left");
        key(input::Key::home);
        require_bidi(rtl_editor.selection().caret == 6, "Bidi visual Home");
        key(input::Key::end);
        require_bidi(rtl_editor.selection().caret == 0, "Bidi visual End");
        require_bidi(mixed_ref.focus() && mixed_ref.select(2, 2), "Bidi direction focus");
        direction.set(TextDirection::RightToLeft);
        draw("direction-rtl");
        require_bidi(scene.text_state(mixed_scene).direction() == TextDirection::RightToLeft &&
                         scene.text_state(mixed_scene).counters().shape_count == shapes + 1,
                     "Bidi direction did not reshape once");
        direction.set(TextDirection::LeftToRight);
        draw("direction-ltr");
        direction.set(TextDirection::Auto);
        require_bidi(mixed_ref.select(0, 4), "Bidi preedit replacement");
        const auto stamp = inputs.sessions().active();
        require_bidi(bool(inputs.dispatch(input::CompositionChanged{String{u8"A אבג 12"}, {0, 4}, stamp})),
                     "Bidi preedit");
        draw("preedit");
        key(input::Key::left);
        require_bidi(inputs.editors().require(mounted[0].editor).composition().active &&
                         inputs.editors().require(mounted[0].editor).value() == "A אבג 12 B",
                     "IME ownership changed logical value");
        require_bidi(bool(inputs.dispatch(input::TextCommitted{String{u8"مرحبا"}, stamp})), "Bidi commit");
        draw("commit");
        require_bidi(inputs.editors().require(mounted[0].editor).value() == String{u8"مرحبا"
                                                                                   u8"בג 12 B"}
                                                                                .bytes(),
                     "Bidi commit logical replacement");
        key(input::Key::z, input::KeyModifier::control);
        draw("undo");
        require_bidi(inputs.editors().require(mounted[0].editor).value() == "A אבג 12 B", "Bidi undo");
        require_bidi(mask_ref.focus({InputFocusCursor::End}), "Bidi mask focus");
        draw("mask");
        key(input::Key::left);
        const auto& masked = inputs.editors().require(mounted[2].editor);
        require_bidi(masked.selection().caret == masked.boundaries().grapheme_bytes()[2],
                     "Bidi mask committed mapping");
        require_bidi(area_ref.focus({InputFocusCursor::End}), "Bidi area focus");
        draw("wrapped-area");
        require_bidi(inputs.layout_snapshot(mounted[3].component).vertical_scroll > 0 &&
                         traced.area.height < 50 * scale,
                     "Bidi area scroll/IME row");
        require_bidi(area_ref.select(0, 8), "Bidi area selection");
        draw("area-selection");
        ThemeConfig dark;
        dark.algorithms = {ThemeAlgorithm::Dark, ThemeAlgorithm::Compact};
        theme.set(dark);
        draw("dark-compact");
        theme.set(ThemeConfig{});
        SDL_SetWindowSize(static_cast<SDL_Window*>(platform.window()), 1420, 1000);
        platform.delay(100);
        draw("resized-window");
        require_bidi(mixed_ref.focus() && mixed_ref.select(2, 2), "Bidi affinity focus");
        draw("pointer-before");
        const auto& map = inputs.caret_map(mounted[0].component);
        const auto upstream = map.at(2, map.revision(), text::TextCaretAffinity::Upstream).value();
        const auto viewport = inputs.layout_snapshot(mounted[0].component).viewport;
        click(viewport.x + upstream.x, viewport.y + 5);
        draw("pointer-hit");
        require_bidi(std::abs(inputs.layout_snapshot(mounted[0].component).caret_x - viewport.x - upstream.x) < .01F,
                     "Bidi resized pointer affinity");
        popup.set(true);
        draw("popup");
        popup.set(false);
        draw("popup-closed");
        services.focus().set_window_active(false);
        inputs.set_window_active(false);
        draw("inactive");
        static_cast<void>(frames.consume_request());
        const auto submissions = renderer.counters().frame_submissions;
        for (int poll = 0; poll < 3; ++poll) {
            static_cast<void>(platform.poll_events());
            require_bidi(!frames.pending() && !services.next_frame_deadline() &&
                             renderer.counters().frame_submissions == submissions,
                         "Bidi idle");
        }
        require_bidi(content_runs == 1 && inputs.text_scene(mounted[0].component) == mixed_scene &&
                         std::ranges::equal(mounted, inputs.mounted_inputs(),
                                            [](const auto& before, const auto& after) {
                                                return before.component == after.component &&
                                                       before.editor == after.editor &&
                                                       before.interaction == after.interaction;
                                            }),
                     "Bidi retained owners replaced");
        services.dispose();
        require_bidi(!mixed_ref.bound() && !rtl_ref.bound() && !mask_ref.bound() && !area_ref.bound() &&
                         nodes.size() == 0 && scene.size() == 0 && inputs.editors().size() == 0 &&
                         services.interactions().size() == 0 &&
                         !inputs.dispatch(input::TextCommitted{String{u8"stale"}, stamp}),
                     "Bidi dispose/stale stamp");
        resources.retire();
        scene_owner.reset();
        std::cout << "bidi_acceptance=passed gpu_driver=" << renderer.gpu_driver()
                  << " shader_format=" << renderer.shader_format() << " system_display_scale=" << metrics.display_scale
                  << " render_scale=" << scale << " inputs=" << mounted.size() << " content_runs=" << content_runs
                  << " pointer_events=" << pointer_events << " native_starts=" << traced.starts
                  << " native_areas=" << traced.areas << " submits=" << submissions
                  << " system_fonts=passed arabic_hebrew=passed visual_navigation=passed disjoint_selection=passed"
                     " direction=passed clipboard=passed history=passed preedit=passed mask=passed ime_area=passed"
                     " window_resize=1420x1000 pointer_hit=passed popup=passed idle_polls=3 deadline=none disposed=1 "
                     "exit_code=0\n";
        return 0;
    } catch (const std::exception& error) {
        std::cerr << "bidi_acceptance_error=" << error.what() << '\n';
        return 1;
    }
}
} // namespace rynui::example
