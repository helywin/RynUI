#include "component/modal_component.hpp"
#include "component/button_component.hpp"
#include "component/input_component.hpp"
#include "component/select_component.hpp"
#include "component/tooltip_component.hpp"
#include "platform/default_font_chain.hpp"
#include "platform/sdl/platform_state.hpp"
#include "renderer/sdl/scene_renderer.hpp"
#include "renderer/common/scene_resources.hpp"
#include "token_gallery_definition.hpp"
#include <chrono>
#include <filesystem>
#include <iostream>
#include <ryn/rynui.hpp>
#include <thread>
#include <SDL3/SDL_video.h>

namespace rynui::example {
namespace {
using namespace ryn;

void require(bool condition, const char* message) {
    if (!condition) {
        throw std::runtime_error(message);
    }
}
} // namespace

int run_modal_acceptance(int argc, char** argv) {
    try {
        std::optional<float> requested_scale;
        bool dark{};
        std::filesystem::path directory;
        for (int index = 1; index < argc; ++index) {
            const std::string_view argument = argv[index];
            if (argument.starts_with("--acceptance-scale=")) {
                requested_scale = std::stof(std::string{argument.substr(19)});
            }
            if (argument.starts_with("--evidence-dir=")) {
                directory = std::filesystem::path{argument.substr(15)};
            }
            if (argument == "--acceptance-theme=dark") {
                dark = true;
            }
        }
        require(!directory.empty(), "Modal acceptance requires --evidence-dir");
        std::filesystem::create_directories(directory);
        detail::PlatformConfig config;
        config.title = "RynUI Modal Acceptance";
        config.width = 1280;
        config.height = 1000;
        auto created = detail::PlatformState::create(config);
        require(bool(created), "native Modal window creation failed");
        auto& platform = *created.state;
        auto metrics = platform.window_metrics();
        const float scale = requested_scale.value_or(metrics.display_scale);
        require(scale == 1 || scale == 1.25F || scale == 1.5F || scale == 2, "invalid acceptance scale");
        const auto executable = std::filesystem::absolute(argv[0]).parent_path();
        auto font_result = font::FontRuntime::create();
        require(bool(font_result), "native Modal font runtime failed");
        auto fonts = std::move(font_result.runtime);
        detail::DefaultFontChainRequest font_request;
        font_request.raster = {14, scale};
        font_request.fallback_latin = executable / "fonts/latin.ttf";
        font_request.fallback_cjk = executable / "fonts/cjk.otf";
        auto font_chain = detail::load_default_ui_font_chain(*fonts, font_request);
        require(bool(font_chain), "native Modal font chain failed");
        auto resolver = detail::make_default_ui_font_resolver(*fonts, font_chain, scale);
        ThemeConfig initial_theme;
        initial_theme.seed.motion = false;
        if (dark) {
            initial_theme.algorithms = {ThemeAlgorithm::Dark};
        }
        Signal<ThemeConfig> theme{initial_theme};
        Signal<bool> open{false}, child_open{false}, loading{false}, centered{false};
        Signal<ModalMask> mask{ModalMask{}};
        InputRef outside, editor_ref, end_ref, child_ref;
        int body_runs{}, content_runs{}, confirmed{}, closed{};
        bool accept{true};
        std::vector<ModalCancelReason> cancelled;
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
        runtime::ComponentId tooltip_button, nested_button;
        services.mount(Content{[&] {
            ++content_runs;
            Theme(ThemeProps{}.config(theme), ThemeContent{[&] {
                      Input(InputProps{}.ref(outside).defaultValue(u8"Background / 背景输入"));
                      Button(ButtonProps{}.onClick([&] { open.set(true); }), [] { Text(u8"Open Modal / 打开对话框"); });
                      Modal(ModalProps{}
                                .open(open)
                                .title(u8"Edit account / 编辑账户")
                                .centered(centered)
                                .mask(mask)
                                .confirmLoading(loading)
                                .onOk([&] {
                                    ++confirmed;
                                    if (accept) {
                                        open.set(false);
                                    }
                                })
                                .onCancel([&](ModalCancelReason reason) {
                                    cancelled.push_back(reason);
                                    if (accept) {
                                        open.set(false);
                                    }
                                })
                                .afterClose([&] { ++closed; }),
                            ModalContent{[&] {
                                ++body_runs;
                                Input(InputProps{}.ref(editor_ref).defaultValue(u8"Retained draft / 保留草稿"));
                                TextArea(TextAreaProps{}.rows(3).defaultValue(u8"Notes / 备注\n中文 English 🙂"));
                                Select(SelectProps{}.options({{1, u8"One / 一"}, {2, u8"Two / 二"}}).showSearch());
                                Tooltip(TooltipProps{}
                                            .title(String{u8"Escape closes the popup first / 先关闭提示"})
                                            .trigger(TooltipTriggerMode::Click),
                                        TooltipTrigger{[&] {
                                            Button(ButtonProps{}, [] { Text(u8"Help / 帮助"); });
                                            tooltip_button = buttons.mounted_buttons().back().component;
                                        }});
                                Button(ButtonProps{}.onClick([&] { child_open.set(true); }),
                                       [] { Text(u8"Nested confirmation / 二级确认"); });
                                nested_button = buttons.mounted_buttons().back().component;
                                Modal(ModalProps{}
                                          .open(child_open)
                                          .title(u8"Nested / 嵌套")
                                          .onOk([&] { child_open.set(false); })
                                          .onCancel([&](ModalCancelReason) { child_open.set(false); }),
                                      ModalContent{[&] {
                                          Input(InputProps{}.ref(child_ref).defaultValue(u8"Child draft / 子层草稿"));
                                      }});
                                Slider(SliderProps{}
                                           .dots(true)
                                           .limits(SliderLimits{0, 100, 25})
                                           .layout(LayoutStyle{}.width(dp(240))));
                                for (int row = 0; row < 28; ++row) {
                                    Text(u8"Long retained body / 保留的长正文内容");
                                }
                                Input(InputProps{}.ref(end_ref).defaultValue(u8"End editor / 末尾编辑"));
                            }});
                  }});
        }});
        detail::SdlSceneRenderer renderer{platform};
        detail::SceneBackend& backend = renderer;
        detail::SceneResources resources{renderer};
        runtime::Size viewport;
        std::size_t frame_number{};
        const auto draw = [&](const std::string& name) {
            for (int settle = 0; settle < 3; ++settle) {
                (void)platform.poll_events();
                metrics = platform.window_metrics();
                viewport = {metrics.pixel_width / scale, metrics.pixel_height / scale};
                const auto time = animation::AnimationTime::microseconds(++frame_number * 250'000);
                (void)services.tick_animations(time);
                require(services.layout_and_synchronize(viewport, {0, 0, viewport.width, viewport.height}, {16, 12}),
                        "native Modal layout failed");
                require(resources.synchronize({&services.surfaces().instances(),
                                               scene.atlas(),
                                               scene.glyph_scene().instances(),
                                               &services.rounded_effects(),
                                               {static_cast<std::uint32_t>(metrics.pixel_width),
                                                static_cast<std::uint32_t>(metrics.pixel_height), scale}}),
                        "native Modal GPU upload failed");
                require(backend.attach_scene(resources.attach(services.scene_composer().ordered_scene())),
                        "native Modal scene attachment failed");
                renderer.set_clear_color(resolve_theme(theme.get()).alias().color_background_container);
                require(renderer.submit_frame(time) != runtime::FrameSubmissionResult::failed,
                        "native Modal frame failed");
                std::this_thread::sleep_for(std::chrono::milliseconds(25));
            }
            require(renderer.save_frame_bmp(directory / (name + ".bmp")), "native Modal readback failed");
        };
        const auto bounds = [&](runtime::ComponentId id) {
            const auto& node = nodes.require(services.components().root(id));
            auto result = node.bounds;
            result.x += node.translation.x;
            result.y += node.translation.y;
            return result;
        };
        const auto click_at = [&](float x, float y) {
            for (auto action : {input::PointerAction::down, input::PointerAction::up}) {
                services.pointer().dispatch(
                    {input::PointerIdentity::mouse(), action, input::PointerButton::primary, x, y});
            }
        };
        const auto click = [&](runtime::ComponentId id) {
            const auto rect = bounds(id);
            click_at(rect.x + rect.width / 2, rect.y + rect.height / 2);
        };
        const auto key = [&](input::Key value, input::KeyModifier modifiers = input::KeyModifier::none) {
            services.focus().dispatch({value, input::KeyAction::down, modifiers});
            services.focus().dispatch({value, input::KeyAction::up, modifiers});
        };
        const auto focus = [&](runtime::ComponentId id) {
            for (auto hit : services.interactions().declaration_order()) {
                if (services.interactions().require(hit).component == id) {
                    return services.focus().request_focus(hit, input::FocusModality::keyboard);
                }
            }
            return false;
        };
        auto& modals = services.modal();
        const auto parent = modals.mounted()[0], child = modals.mounted()[1];
        const auto trigger = buttons.mounted_buttons()[0];
        const auto editor = inputs.mounted_inputs()[1];
        const auto select = services.select().mounted()[0], tooltip = services.tooltip().mounted()[0];
        draw("initial");
        const auto count = services.components().component_count();
        require(!modals.visible() && outside.focus(), "native Modal hidden focus failed");
        require(focus(trigger.component), "native Modal trigger focus failed");
        click(trigger.component);
        draw("opened");
        const auto initial = modals.snapshot(parent);
        require(initial.topmost && !outside.focus() && inputs.sessions().active().valid(),
                "native Modal boundary failed");
        for (int step = 0; step < 12; ++step) {
            key(input::Key::tab, step % 2 ? input::KeyModifier::shift : input::KeyModifier::none);
            require(services.focus().state().focused &&
                        services.interactions().branch_active(*services.focus().state().focused),
                    "native Modal Tab escaped");
        }
        require(editor_ref.focus(), "native Modal editor focus failed");
        static_cast<void>(inputs.editors().require(editor.editor).select_all());
        require(
            bool(inputs.dispatch(input::CompositionChanged{String{u8"caogao"}, {6, 0}, inputs.sessions().active()})),
            "native Modal composition failed");
        draw("composition");
        const String draft{u8"Native retained draft / 原生保留草稿"};
        require(bool(inputs.dispatch(input::TextCommitted{draft, inputs.sessions().active()})),
                "native Modal text commit failed");
        draw("edited");
        click(services.select().snapshot(select).selector);
        draw("select");
        require(services.select().snapshot(select).visible, "native Modal Select failed");
        key(input::Key::escape);
        draw("select-closed");
        require(!services.select().snapshot(select).visible && modals.snapshot(parent).visible,
                "native Select Escape escaped Modal");
        require(focus(tooltip_button), "native Tooltip trigger focus failed");
        draw("tooltip-trigger");
        click(tooltip_button);
        draw("tooltip");
        require(services.tooltip().snapshot(tooltip).visible, "native Modal Tooltip failed");
        key(input::Key::escape);
        draw("tooltip-closed");
        require(!services.tooltip().snapshot(tooltip).visible && modals.snapshot(parent).visible,
                "native Tooltip Escape escaped Modal");
        require(focus(nested_button), "native nested trigger focus failed");
        draw("nested-trigger");
        click(nested_button);
        draw("nested");
        require(modals.snapshot(child).topmost && !modals.snapshot(parent).topmost && !editor_ref.focus(),
                "native nested Modal boundary failed");
        key(input::Key::escape);
        draw("nested-closed");
        require(modals.snapshot(parent).topmost && !modals.snapshot(child).visible, "native nested restoration failed");
        loading.set(true);
        draw("loading");
        click(modals.snapshot(parent).ok);
        require(confirmed == 0 && modals.snapshot(parent).visible, "native loading confirmed");
        loading.set(false);
        accept = false;
        key(input::Key::escape);
        draw("controlled-refused");
        require(modals.snapshot(parent).visible && cancelled.back() == ModalCancelReason::Escape,
                "native controlled refusal failed");
        accept = true;
        require(end_ref.focus(), "native Modal end focus failed");
        draw("body-end");
        auto scrolled = modals.snapshot(parent);
        require(scrolled.scroll > 0 && bounds(inputs.mounted_inputs().back().component).y <
                                           scrolled.body_bounds.y + scrolled.body_bounds.height,
                "native body focus reveal failed");
        require(inputs.dispatch(input::ScrollInputEvent{0, 1, scrolled.body_bounds.x + 4, scrolled.body_bounds.y + 4}),
                "native Modal wheel failed");
        draw("body-wheel");
        require(modals.snapshot(parent).scroll < scrolled.scroll, "native Modal body did not scroll");
        centered.set(true);
        mask.set({false, true});
        draw("centered-maskless");
        require(SDL_SetWindowSize(static_cast<SDL_Window*>(platform.window()), 1100, 800),
                "native Modal resize failed");
        platform.delay(100);
        draw("resized");
        const auto resized = modals.snapshot(parent);
        require(resized.bounds.x >= 0 && resized.bounds.y >= 0 &&
                    resized.bounds.x + resized.bounds.width <= viewport.width &&
                    resized.bounds.y + resized.bounds.height <= viewport.height,
                "native Modal escaped resized viewport");
        click_at(2, 2);
        draw("mask-closed");
        require(!modals.visible() && cancelled.back() == ModalCancelReason::Mask &&
                    services.focus().state().focused == trigger.interaction,
                "native mask close/restoration failed");
        click(trigger.component);
        draw("reopened");
        require(inputs.editors().require(editor.editor).value() == draft.bytes() &&
                    modals.snapshot(parent).body == initial.body,
                "native Modal lost retained draft");
        click(modals.snapshot(parent).ok);
        draw("confirmed");
        require(confirmed == 1 && !modals.visible(), "native Modal OK failed");
        services.set_window_active(false);
        draw("inactive");
        static_cast<void>(frames.consume_request());
        for (int idle = 0; idle < 3; ++idle) {
            (void)services.tick_animations(animation::AnimationTime::microseconds(++frame_number * 250'000));
            require(!frames.pending() && !services.next_frame_deadline(), "native Modal did not settle idle");
        }
        require(content_runs == 1 && body_runs == 1 && services.components().component_count() == count && closed >= 2,
                "native Modal remounted content or lost lifecycle");
        std::cout << "modal_acceptance=passed gpu_driver=" << renderer.gpu_driver()
                  << " shader_format=" << renderer.shader_format() << " system_display_scale=" << metrics.display_scale
                  << " render_scale=" << scale << " theme=" << (dark ? "dark" : "light")
                  << " boundary=passed keyboard=passed controlled=passed automatic_ime=passed draft=passed "
                     "popup_priority=passed nested=passed body_scroll=passed resize=passed idle=passed\n";
        for (auto weight : {400U, 600U}) {
            const auto face = detail::resolve_platform_face(SystemFontFamily::ui_sans, weight, false);
            require(bool(face), "native Modal system font missing");
            std::cout << "font=" << weight << " family=" << face->family_name << " path=" << face->source_path.string()
                      << " face_index=" << face->face_index << '\n';
        }
        services.dispose();
        require(modals.mounted().empty() && inputs.mounted_inputs().empty() && services.interactions().size() == 0 &&
                    nodes.size() == 0 && scene.size() == 0 && services.rounded_effects().live_count() == 0 &&
                    !editor_ref.bound() && !services.next_frame_deadline(),
                "native Modal disposal leaked");
        std::cout << "disposed=1 exit_code=0\n";
        return 0;
    } catch (const std::exception& error) {
        std::cerr << "modal_acceptance_error=" << error.what() << "\nexit_code=1\n";
        return 1;
    }
}
} // namespace rynui::example
