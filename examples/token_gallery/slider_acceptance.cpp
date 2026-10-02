#include "component/slider_component.hpp"
#include "component/tooltip_component.hpp"
#include "platform/default_font_chain.hpp"
#include "platform/sdl/platform_state.hpp"
#include "renderer/common/scene_resources.hpp"
#include "renderer/sdl/scene_renderer.hpp"
#include "token_gallery_definition.hpp"
#include "acceptance_events.hpp"
#include <SDL3/SDL.h>
#include <ryn/rynui.hpp>
#include <filesystem>
#include <iostream>
#include <limits>

namespace rynui::example {
namespace {
using namespace ryn;
using acceptance::fixture_timestamp;
using acceptance::FixtureEventFilter;

void require(bool condition, const char* message) {
    if (!condition) {
        throw std::runtime_error(message);
    }
}
} // namespace

int run_slider_acceptance(int argc, char** argv) {
    try {
        std::optional<float> requested_scale;
        bool with_marks{};
        bool with_editing{};
        std::filesystem::path directory;
        for (int i = 1; i < argc; ++i) {
            const std::string_view arg = argv[i];
            with_marks = with_marks || arg == "--slider-marks-acceptance";
            with_editing = with_editing || arg == "--slider-editing-acceptance";
            if (arg.starts_with("--acceptance-scale=")) {
                requested_scale = std::stof(std::string{arg.substr(19)});
            }
            if (arg.starts_with("--evidence-dir=")) {
                directory = std::filesystem::path{arg.substr(15)};
            }
        }
        require(!directory.empty(), "Slider acceptance requires --evidence-dir");
        with_marks = with_marks || with_editing;
        std::filesystem::create_directories(directory);
        detail::PlatformConfig config;
        config.title = "RynUI Slider Acceptance";
        config.width = with_editing ? 1600 : 1100;
        config.height = with_editing ? 1300 : (with_marks ? 1020 : 850);
        auto created = detail::PlatformState::create(config);
        require(bool(created), "Slider window creation failed");
        auto& platform = *created.state;
        // Keep the scripted SDL adapter journey independent of live desktop
        // pointer/keyboard/focus changes. Resize and renderer events remain real.
        FixtureEventFilter fixture_filter{with_editing};
        auto metrics = platform.window_metrics();
        const float scale = requested_scale.value_or(metrics.display_scale);
        require(std::isfinite(scale) && scale > 0, "Slider scale invalid");
        const auto executable = std::filesystem::absolute(argv[0]).parent_path();
        auto font_result = font::FontRuntime::create();
        require(bool(font_result), "Slider font runtime failed");
        auto fonts = std::move(font_result.runtime);
        detail::DefaultFontChainRequest font_request;
        font_request.raster = {14, scale};
        font_request.fallback_latin = executable / "fonts/latin.ttf";
        font_request.fallback_cjk = executable / "fonts/cjk.otf";
        auto chain = detail::load_default_ui_font_chain(*fonts, font_request);
        require(bool(chain), "Slider font chain failed");
        auto resolver = detail::make_default_ui_font_resolver(*fonts, chain, scale);
        runtime::NodeStore nodes;
        runtime::FrameRequestState frames;
        runtime::DirtyQueues dirty{nodes, &frames};
        layout::LayoutEngine layout{nodes};
        text::TextEngine engine{*fonts};
        detail::TextSceneService scene{*fonts, engine, frames};
        detail::WindowComponentServices services{nodes, layout, dirty, scene, resolver, frames};
        Signal<double> value{30};
        Signal<SliderRange> range{SliderRange{20, 80}};
        Signal<SliderRange> track_value{SliderRange{20, 80}};
        Signal<SliderValues> edit_value{SliderValues{20, 50, 80}};
        Signal<SliderDisabledHandles> edit_disabled{SliderDisabledHandles{}};
        SliderRef edit_ref;
        Signal<bool> disabled{false};
        Signal<ThemeConfig> theme{ThemeConfig{}};
        Signal<SliderMarks> marks{
            SliderMarks{{0, String{u8"低 Low"}}, {50, String{u8"中 Middle"}}, {100, String{u8"高 High"}}}};
        const Prop<SliderMarks> mark_prop = with_marks ? Prop<SliderMarks>{marks} : Prop<SliderMarks>{SliderMarks{}};
        int changes{};
        int completes{};
        int runs{};
        Signal<LogicalLength> width{dp(350)};
        services.mount(Content{[&] {
            ++runs;
            Theme(ThemeProps{}.config(theme), ThemeContent{[&] {
                      Flex(FlexProps{}.gap(dp(32)), FlexContent{[&] {
                               Flex(FlexProps{}.vertical(true).gap(dp(8)), FlexContent{[&] {
                                        Text(u8"Slider · 单值 / Range / Reverse / Disabled / Vertical");
                                        Slider(SliderProps{}
                                                   .value(value)
                                                   .marks(mark_prop)
                                                   .disabled(disabled)
                                                   .onChange([&](double next) {
                                                       ++changes;
                                                       value.set(next);
                                                   })
                                                   .onChangeComplete([&](double) { ++completes; })
                                                   .layout(LayoutStyle{}.width(width)));
                                        RangeSlider(RangeSliderProps{}
                                                        .value(range)
                                                        .marks(with_marks ? SliderMarks{{20, String{u8"20"}},
                                                                                        {50, String{u8"50"}},
                                                                                        {80, String{u8"80"}}}
                                                                          : SliderMarks{})
                                                        .marksOnly(with_marks)
                                                        .dots(with_marks)
                                                        .onChange([&](SliderRange next) {
                                                            ++changes;
                                                            range.set(next);
                                                        })
                                                        .onChangeComplete([&](SliderRange) { ++completes; })
                                                        .layout(LayoutStyle{}.width(width)));
                                        Slider(SliderProps{}
                                                   .defaultValue(35)
                                                   .limits(SliderLimits{0, 100, with_marks ? 10.0 : 1.0})
                                                   .dots(with_marks)
                                                   .included(!with_marks)
                                                   .reverse(true)
                                                   .layout(LayoutStyle{}.width(width)));
                                        Slider(SliderProps{}.defaultValue(60).disabled(true).layout(
                                            LayoutStyle{}.width(width)));
                                        Slider(SliderProps{}
                                                   .defaultValue(40)
                                                   .orientation(SliderOrientation::Vertical)
                                                   .limits(SliderLimits{0, 100, with_marks ? 10.0 : 1.0})
                                                   .marks(with_marks ? SliderMarks{{0, String{u8"低"}},
                                                                                   {50, String{u8"中"}},
                                                                                   {100, String{u8"高"}}}
                                                                     : SliderMarks{})
                                                   .dots(with_marks)
                                                   .layout(LayoutStyle{}.height(dp(160))));
                                    }});
                               if (with_editing) {
                                   Flex(FlexProps{}.vertical(true).gap(dp(8)), FlexContent{[&] {
                                            Text(u8"整段轨道 · 保持快照 / 边界");
                                            RangeSlider(RangeSliderProps{}
                                                            .value(track_value)
                                                            .draggableTrack(true)
                                                            .onChange([&](SliderRange next) {
                                                                ++changes;
                                                                track_value.set(next);
                                                            })
                                                            .onChangeComplete([&](SliderRange) { ++completes; })
                                                            .layout(LayoutStyle{}.width(width)));
                                            Text(u8"MultiSlider · 插入 / Delete / 拖出删除");
                                            MultiSlider(MultiSliderProps{}
                                                            .value(edit_value)
                                                            .ref(edit_ref)
                                                            .autoFocus(true)
                                                            .handleDisabled(edit_disabled)
                                                            .rangeOptions(SliderRangeOptions{false, true, 0, 6})
                                                            .onChange([&](SliderValues next) {
                                                                ++changes;
                                                                edit_value.set(std::move(next));
                                                            })
                                                            .onChangeComplete([&](SliderValues) { ++completes; })
                                                            .layout(LayoutStyle{}.width(width)));
                                            Text(u8"逐端点禁用 · 中间不可操作");
                                            MultiSlider(MultiSliderProps{}
                                                            .defaultValue({20, 50, 80})
                                                            .handleDisabled(SliderDisabledHandles{false, true, false})
                                                            .layout(LayoutStyle{}.width(width)));
                                            Text(u8"纵向反向 MultiSlider · 拖出删除 / Right hint");
                                            MultiSlider(MultiSliderProps{}
                                                            .defaultValue({20, 80})
                                                            .orientation(SliderOrientation::Vertical)
                                                            .reverse(true)
                                                            .rangeOptions(SliderRangeOptions{false, true, 1, 4})
                                                            .layout(LayoutStyle{}.height(dp(160))));
                                        }});
                               }
                           }});
                  }});
        }});
        detail::SdlSceneRenderer renderer{platform, executable / "shaders"};
        detail::SceneResources resources{renderer};
        std::uint64_t frame{};
        std::uint64_t normalized{};
        const auto draw = [&](std::string name) {
            for (int settle = 0; settle < 2; ++settle) {
                static_cast<void>(platform.poll_events());
                metrics = platform.window_metrics();
                const runtime::Size viewport{metrics.pixel_width / scale, metrics.pixel_height / scale};
                require(services.layout_and_synchronize(viewport, {0, 0, viewport.width, viewport.height}, {20, 16}),
                        "Slider layout failed");
                require(resources.synchronize({&services.surfaces().instances(),
                                               scene.atlas(),
                                               scene.glyph_scene().instances(),
                                               &services.rounded_effects(),
                                               {static_cast<std::uint32_t>(metrics.pixel_width),
                                                static_cast<std::uint32_t>(metrics.pixel_height), scale}}),
                        "Slider upload failed");
                require(renderer.attach_scene(resources.attach(services.scene_composer().ordered_scene())),
                        "Slider attachment failed");
                renderer.set_clear_color(resolve_theme(theme.get()).alias().color_background_container);
                require(renderer.submit_frame(animation::AnimationTime::microseconds(++frame * 100000)) !=
                            runtime::FrameSubmissionResult::failed,
                        "Slider frame failed");
                platform.delay(35);
            }
            require(renderer.save_frame_bmp(directory / (name + ".bmp")), "Slider GPU readback failed");
        };
        draw("initial");
        auto& host = services.slider();
        require(host.mounted().size() == (with_editing ? 9 : 5), "Slider inventory wrong");
        const auto single = host.mounted()[0];
        const auto dual = host.mounted()[1];
        const auto reverse = host.mounted()[2];
        const auto blocked = host.mounted()[3];
        const auto vertical = host.mounted()[4];
        auto* window = static_cast<SDL_Window*>(platform.window());
        const auto window_id = SDL_GetWindowID(window);
        const auto poll = [&] {
            const auto& events = platform.poll_events();
            for (const auto& event : events.input.events()) {
                if (const auto* key = std::get_if<input::KeyboardInputEvent>(&event)) {
                    services.focus().dispatch(*key);
                    ++normalized;
                }
                if (const auto* pointer = std::get_if<input::PointerInputEvent>(&event)) {
                    auto value = *pointer;
                    value.x *= metrics.display_scale / scale;
                    value.y *= metrics.display_scale / scale;
                    services.pointer().dispatch(value);
                    ++normalized;
                }
                if (const auto* window_event = std::get_if<input::WindowInputEvent>(&event)) {
                    if (window_event->action == input::WindowInputAction::focus_lost) {
                        services.set_window_active(false);
                    } else if (window_event->action == input::WindowInputAction::focus_gained) {
                        services.set_window_active(true);
                    }
                }
            }
        };
        const auto key = [&](SDL_Keycode code) {
            for (auto type : {SDL_EVENT_KEY_DOWN, SDL_EVENT_KEY_UP}) {
                SDL_Event event{};
                event.type = type;
                event.common.timestamp = fixture_timestamp;
                event.key.windowID = window_id;
                event.key.key = code;
                event.key.down = type == SDL_EVENT_KEY_DOWN;
                require(SDL_PushEvent(&event), "Slider key injection failed");
                poll();
            }
        };
        require(services.focus().request_focus(single.thumbs[0], input::FocusModality::keyboard),
                "Slider native focus failed");
        key(SDLK_UP);
        key(SDLK_PAGEUP);
        key(SDLK_PAGEDOWN);
        require(value.get() == 31 && completes == 3, "Slider normalized keyboard failed");
        require(services.focus().request_focus(dual.thumbs[1], input::FocusModality::keyboard),
                "Range native focus failed");
        key(SDLK_LEFT);
        require(range.get() == SliderRange{20, with_marks ? 50.0 : 79.0}, "Range keyboard changed wrong endpoint");
        draw("keyboard");
        if (with_marks) {
            const auto hint = services.tooltip().mounted()[2];
            require(services.tooltip().snapshot(hint).visible, "Range keyboard hint missing");
            key(SDLK_ESCAPE);
            draw("dismissed");
            require(!services.tooltip().snapshot(hint).visible && services.focus().state().focused == dual.thumbs[1],
                    "Range hint Escape changed focus or stayed visible");
            services.focus().clear_focus();
            services.focus().request_focus(dual.thumbs[1], input::FocusModality::keyboard);
            draw("reopened");
            require(services.tooltip().snapshot(hint).visible, "Range hint did not reopen");
        }
        const auto point = [&](const detail::MountedSliderComponent& item, double ratio) {
            const auto b = nodes.require(item.node).bounds;
            const auto& m = services.components().theme_scope(item.component)->snapshot().slider().metrics;
            const bool v = host.snapshot(item.component).orientation == SliderOrientation::Vertical;
            const float length = v ? b.height : b.width;
            const float inset = std::min(length / 2, m.handle_size_hover / 2 + m.handle_line_width_hover);
            const float pos = inset + (length - 2 * inset) * static_cast<float>(ratio);
            const auto center = host.snapshot(item.component).centers[0];
            return v ? runtime::Point{center.x, b.y + pos} : runtime::Point{b.x + pos, center.y};
        };
        const auto pointer = [&](Uint32 type, runtime::Point p) {
            SDL_Event event{};
            event.type = type;
            event.common.timestamp = fixture_timestamp;
            const float x = p.x * scale / metrics.pixel_density;
            const float y = p.y * scale / metrics.pixel_density;
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
            require(SDL_PushEvent(&event), "Slider mouse injection failed");
            poll();
        };
        if (with_marks) {
            const auto label = services.components().children(single.component).back();
            const auto b = nodes.require(services.components().root(label)).bounds;
            pointer(SDL_EVENT_MOUSE_BUTTON_DOWN, {b.x + 3, b.y + 3});
            pointer(SDL_EVENT_MOUSE_BUTTON_UP, {b.x + 3, b.y + 3});
            require(value.get() == 100, "native label click failed");
            draw("label");
        }
        pointer(SDL_EVENT_MOUSE_BUTTON_DOWN, point(single, 0.3));
        pointer(SDL_EVENT_MOUSE_MOTION, point(single, 1.2));
        if (with_marks) {
            draw("captured");
            require(services.tooltip().snapshot(services.tooltip().mounted()[0]).visible &&
                        host.snapshot(single.component).dragging,
                    "native captured value hint missing");
        }
        pointer(SDL_EVENT_MOUSE_BUTTON_UP, point(single, 1.2));
        require(value.get() == 100 && !host.snapshot(single.component).dragging, "Slider native capture/clamp failed");
        draw("dragged");
        pointer(SDL_EVENT_MOUSE_BUTTON_DOWN, point(dual, 0.2));
        pointer(SDL_EVENT_MOUSE_MOTION, point(dual, 0.95));
        pointer(SDL_EVENT_MOUSE_BUTTON_UP, point(dual, 0.95));
        require(range.get() == (with_marks ? SliderRange{50, 50} : SliderRange{79, 79}),
                "Range native crossing failed");
        draw("range");
        if (with_marks) {
            const auto thumb = single.thumbs[0];
            const auto hint = services.tooltip().mounted()[0];
            marks.set(
                {{0, String{u8"低 Low"}}, {25, String{u8"25"}}, {75, String{u8"75"}}, {100, String{u8"高 High"}}});
            draw("dynamic");
            require(host.mounted()[0].thumbs[0] == thumb && services.tooltip().mounted()[0] == hint && runs == 1,
                    "native marks update remounted retained components");
        }
        pointer(SDL_EVENT_MOUSE_BUTTON_DOWN, point(reverse, 0.1));
        pointer(SDL_EVENT_MOUSE_BUTTON_UP, point(reverse, 0.1));
        pointer(SDL_EVENT_MOUSE_BUTTON_DOWN, point(vertical, 0.1));
        pointer(SDL_EVENT_MOUSE_BUTTON_UP, point(vertical, 0.1));
        require(host.snapshot(reverse.component).value.lower == 90 &&
                    host.snapshot(vertical.component).value.lower == 90,
                "Slider native direction failed");
        pointer(SDL_EVENT_MOUSE_BUTTON_DOWN, point(blocked, 0.1));
        pointer(SDL_EVENT_MOUSE_BUTTON_UP, point(blocked, 0.1));
        require(host.snapshot(blocked.component).value.lower == 60, "disabled native Slider changed");
        pointer(SDL_EVENT_MOUSE_BUTTON_DOWN, point(single, 0.5));
        const auto completed_before = completes;
        disabled.set(true);
        pointer(SDL_EVENT_MOUSE_BUTTON_UP, point(single, 0.8));
        require(completes == completed_before && !host.snapshot(single.component).dragging,
                "disabled native capture completed");
        disabled.set(false);
        if (with_editing) {
            const auto track = host.mounted()[5];
            const auto edit = host.mounted()[6];
            const auto per_disabled = host.mounted()[7];
            const auto multi_vertical = host.mounted()[8];
            const auto original = edit.thumbs;
            pointer(SDL_EVENT_MOUSE_BUTTON_DOWN, point(track, 0.35));
            pointer(SDL_EVENT_MOUSE_MOTION, point(track, 0.45));
            pointer(SDL_EVENT_MOUSE_BUTTON_UP, point(track, 0.45));
            require(track_value.get() == SliderRange{30, 90}, "native whole track drag failed");
            draw("track");
            pointer(SDL_EVENT_MOUSE_BUTTON_DOWN, point(edit, 0.35));
            pointer(SDL_EVENT_MOUSE_MOTION, point(edit, 0.4));
            draw("insert-captured");
            require(edit_value.get() == SliderValues{20, 40, 50, 80} && host.snapshot(edit.component).dragging,
                    "native controlled insert echo lost capture");
            pointer(SDL_EVENT_MOUSE_BUTTON_UP, point(edit, 0.4));
            require(host.mounted()[6].thumbs[0] == original[0] && host.mounted()[6].thumbs[2] == original[1] &&
                        host.mounted()[6].thumbs[3] == original[2],
                    "native insertion replaced retained endpoints");
            services.focus().request_focus(host.mounted()[6].thumbs[1], input::FocusModality::keyboard);
            key(SDLK_DELETE);
            require(edit_value.get() == SliderValues{20, 50, 80} && services.focus().state().focused == original[1],
                    "native Delete or focus transfer failed");
            draw("deleted-key");
            auto outside = point(edit, 0.5);
            outside.y += 131;
            pointer(SDL_EVENT_MOUSE_BUTTON_DOWN, point(edit, 0.5));
            pointer(SDL_EVENT_MOUSE_MOTION, outside);
            draw("delete-preview");
            require(host.snapshot(edit.component).delete_preview, "native delete preview missing");
            const auto complete_before_loss = completes;
            for (auto type : {SDL_EVENT_WINDOW_FOCUS_LOST, SDL_EVENT_WINDOW_FOCUS_GAINED}) {
                SDL_Event event{};
                event.type = type;
                event.common.timestamp = fixture_timestamp;
                event.window.windowID = window_id;
                require(SDL_PushEvent(&event), "native window focus event injection failed");
                poll();
            }
            require(!host.snapshot(edit.component).dragging && !host.snapshot(edit.component).delete_preview &&
                        edit_value.get() == SliderValues{20, 50, 80} && completes == complete_before_loss,
                    "native window loss committed deletion or retained capture");
            draw("cancelled-delete");
            pointer(SDL_EVENT_MOUSE_BUTTON_DOWN, point(edit, 0.5));
            pointer(SDL_EVENT_MOUSE_MOTION, outside);
            pointer(SDL_EVENT_MOUSE_BUTTON_UP, outside);
            require(edit_value.get() == SliderValues{20, 80}, "native drag delete failed");
            draw("deleted-drag");
            require(edit_ref.focus() && services.focus().state().focused == original[0] && edit_ref.blur(),
                    "native SliderRef focus/blur failed");
            services.focus().request_focus(host.mounted()[6].thumbs.back(), input::FocusModality::keyboard);
            key(SDLK_BACKSPACE);
            require(edit_value.get() == SliderValues{20}, "native Backspace failed");
            draw("backspace");
            services.focus().request_focus(per_disabled.thumbs[0], input::FocusModality::keyboard);
            key(SDLK_TAB);
            require(services.focus().state().focused == per_disabled.thumbs[2],
                    "native Tab did not skip disabled endpoint");
            pointer(SDL_EVENT_MOUSE_BUTTON_DOWN, point(per_disabled, 0.5));
            pointer(SDL_EVENT_MOUSE_BUTTON_UP, point(per_disabled, 0.6));
            require(host.snapshot(per_disabled.component).values[1] == 50, "native disabled endpoint changed");
            draw("per-disabled");
            pointer(SDL_EVENT_MOUSE_BUTTON_DOWN, point(edit, 0.2));
            const auto before = completes;
            edit_disabled.set({true, false});
            pointer(SDL_EVENT_MOUSE_BUTTON_UP, point(edit, 0.4));
            require(!host.snapshot(edit.component).dragging && completes == before,
                    "native disabled edit retained capture");
            edit_disabled.set({});
            auto vertical_outside = point(multi_vertical, 0.2);
            vertical_outside.x += 131;
            pointer(SDL_EVENT_MOUSE_BUTTON_DOWN, point(multi_vertical, 0.2));
            pointer(SDL_EVENT_MOUSE_MOTION, vertical_outside);
            pointer(SDL_EVENT_MOUSE_BUTTON_UP, vertical_outside);
            require(host.snapshot(multi_vertical.component).values == SliderValues{80}, "native vertical edit failed");
            draw("vertical-edit");
        }
        ThemeConfig dark;
        dark.algorithms = {ThemeAlgorithm::Dark};
        theme.set(dark);
        draw("dark");
        ThemeConfig compact;
        compact.algorithms = {ThemeAlgorithm::Compact};
        theme.set(compact);
        draw("compact");
        const int resized_height = with_editing ? 1180 : (with_marks ? 960 : 720);
        const int resized_width = with_editing ? 1420 : 900;
        require(SDL_SetWindowSize(window, resized_width, resized_height), "Slider native resize failed");
        platform.delay(100);
        metrics = platform.window_metrics();
        width.set(dp(280));
        draw("resized");
        require(metrics.coordinate_width == resized_width && metrics.coordinate_height == resized_height && runs == 1,
                "Slider resize rebuilt content or extent wrong");
        std::cout << (with_editing ? "slider_editing_acceptance=passed gpu_driver="
                      : with_marks ? "slider_marks_acceptance=passed gpu_driver="
                                   : "slider_acceptance=passed gpu_driver=")
                  << renderer.gpu_driver() << " shader_format=" << renderer.shader_format()
                  << " system_display_scale=" << metrics.display_scale << " render_scale=" << scale
                  << " normalized_events=" << normalized << " changes=" << changes << " completes=" << completes
                  << " submits=" << renderer.counters().frame_submissions << " resize=" << resized_width << "x"
                  << resized_height << " content_runs=" << runs << " exit_code=0\n";
        services.dispose();
        return 0;
    } catch (const std::exception& e) {
        std::cerr << "slider_acceptance_error=" << e.what() << '\n';
        return 1;
    }
}
} // namespace rynui::example
