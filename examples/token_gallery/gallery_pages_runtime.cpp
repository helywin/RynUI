#include "gallery_pages_runtime.hpp"
#include "gallery_visual_capture.hpp"
#include "gallery_existing_data_entry_acceptance.hpp"
#include "gallery_status_acceptance.hpp"
#include "gallery_page_acceptance.hpp"
#include "component/select_component.hpp"
#include "component/tree_component.hpp"
#include "component/tree_select_component.hpp"
#include "component/image_component.hpp"
#include "component/qrcode_component.hpp"
#include <fstream>
#include "component/calendar_component.hpp"
#include "component/carousel_component.hpp"
#include "component/upload_component.hpp"
#include "component/transfer_component.hpp"
#include "component/table_component.hpp"
#include "component/auto_complete_component.hpp"
#include "component/mentions_component.hpp"
#include "component/form_component.hpp"
#include "component/color_picker_component.hpp"
#include "component/cascader_component.hpp"
#include "component/date_picker_component.hpp"
#include "component/time_picker_component.hpp"
#include "component/anchor_component.hpp"
#include "component/breadcrumb_component.hpp"
#include "component/dropdown_component.hpp"
#include "component/menu_component.hpp"
#include "component/pagination_component.hpp"
#include "component/steps_component.hpp"
#include "component/tabs_component.hpp"
#include "component/popover_component.hpp"
#include "component/float_button_component.hpp"
#include "component/collapse_component.hpp"
#include "component/segmented_component.hpp"
#include "component/descriptions_component.hpp"
#include "component/timeline_component.hpp"
#include "component/grid_component.hpp"
#include "component/shell_layout_component.hpp"
#include "component/splitter_component.hpp"
#include <numeric>
#include "component/masonry_component.hpp"
#include "component/typography_component.hpp"
#include "gallery_pages_host.hpp"
#include "token_gallery_definition.hpp"

#include "component/input_component.hpp"
#include "component/card_component.hpp"
#include "component/listy_component.hpp"
#include "component/selection_component.hpp"
#include "platform/default_font_chain.hpp"
#include "platform/sdl/platform_state.hpp"
#include "platform/sdl/file_services.hpp"
#include "renderer/common/scene_resources.hpp"
#include "renderer/sdl/scene_renderer.hpp"
#include "text/text_engine.hpp"

#include <chrono>
#include <array>
#include <algorithm>
#include <cmath>
#include <iostream>
#include <stdexcept>
#include <string_view>
#include <memory>
#include <SDL3/SDL.h>
#ifdef __linux__
#include <wayland-client.h>
#endif

namespace rynui::example {
namespace {
using namespace ryn;

#ifdef __linux__
// A submitted Vulkan command is not a compositor frame boundary. Native
// acceptance advances its next resize/action only after this callback, while
// continuing to dispatch the window's normal event queue.
class AcceptanceFrameBoundary final {
public:
    explicit AcceptanceFrameBoundary(SDL_Window* window) {
        const auto properties = SDL_GetWindowProperties(window);
        auto* surface = static_cast<wl_surface*>(
            SDL_GetPointerProperty(properties, SDL_PROP_WINDOW_WAYLAND_SURFACE_POINTER, nullptr));
        if (!surface) {
            throw std::runtime_error("Linux page acceptance requires a Wayland surface");
        }
        callback_ = wl_surface_frame(surface);
        if (!callback_) {
            throw std::runtime_error("Wayland page frame callback allocation failed");
        }
        static const wl_callback_listener listener{
            [](void* data, wl_callback*, std::uint32_t) { static_cast<AcceptanceFrameBoundary*>(data)->done_ = true; }};
        if (wl_callback_add_listener(callback_, &listener, this) != 0) {
            wl_callback_destroy(callback_);
            throw std::runtime_error("Wayland page frame listener registration failed");
        }
    }

    AcceptanceFrameBoundary(const AcceptanceFrameBoundary&) = delete;
    AcceptanceFrameBoundary& operator=(const AcceptanceFrameBoundary&) = delete;

    ~AcceptanceFrameBoundary() {
        wl_callback_destroy(callback_);
    }

    bool done() const noexcept {
        return done_;
    }

private:
    wl_callback* callback_{};
    bool done_{};
};
#endif

class GalleryPageEvents final : public runtime::FrameEventSource {
public:
    ryn::detail::PlatformState& platform;
    GalleryPagesHost& pages;
    ryn::detail::ButtonComponentHost& app;
    ryn::detail::InputComponentHost& inputs;
    runtime::FrameRequestState& frames;
    font::FontRuntime& fonts;
    ryn::detail::DefaultFontChainResult& font_chain;
    runtime::Size viewport;
    float scale;
    bool quit{};
    std::string error;
    std::chrono::steady_clock::time_point started{std::chrono::steady_clock::now()};

    GalleryPageEvents(ryn::detail::PlatformState& host, GalleryPagesHost& gallery,
                      ryn::detail::ButtonComponentHost& application, ryn::detail::InputComponentHost& input,
                      runtime::FrameRequestState& requests, font::FontRuntime& font_runtime,
                      ryn::detail::DefaultFontChainResult& chain)
        : platform(host), pages(gallery), app(application), inputs(input), frames(requests), fonts(font_runtime),
          font_chain(chain), viewport{host.window_metrics().logical_width(), host.window_metrics().logical_height()},
          scale(host.window_metrics().display_scale) {}

    animation::AnimationTime now() const noexcept override {
        return animation::AnimationTime::microseconds(
            std::chrono::duration_cast<std::chrono::microseconds>(std::chrono::steady_clock::now() - started).count());
    }

    bool poll_frame_event() noexcept override {
        return consume(platform.poll_events());
    }

    bool wait_for_frame_event(std::uint32_t timeout) noexcept override {
        return consume(platform.wait_events(timeout));
    }

    void inject_scroll_at(float x, float y, float delta) {
        pages.inject_scroll_at(x, y, delta);
    }

    bool consume(const ryn::detail::PlatformEvents& events) noexcept {
        try {
            app.set_animation_time(now());
            quit = quit || events.quit_requested;
            for (const auto& event : events.input.events()) {
                std::visit(
                    [this](const auto& value) {
                        using T = std::decay_t<decltype(value)>;
                        if constexpr (std::is_same_v<T, input::PointerInputEvent> ||
                                      std::is_same_v<T, input::ScrollInputEvent> ||
                                      std::is_same_v<T, input::FileDropEvent>) {
                            auto mapped = value;
                            mapped.x =
                                token_gallery_pointer_to_render_logical(value.x, platform.display_scale(), scale);
                            mapped.y =
                                token_gallery_pointer_to_render_logical(value.y, platform.display_scale(), scale);
                            pages.dispatch(mapped);
                        } else {
                            if constexpr (std::is_same_v<T, input::WindowInputEvent>) {
                                if (value.action == input::WindowInputAction::resized) {
                                    const auto metrics = platform.window_metrics();
                                    if (metrics.pixel_width > 0 && metrics.pixel_height > 0) {
                                        if (std::abs(metrics.display_scale - scale) > 0.0001F) {
                                            scale = metrics.display_scale;
                                            static_cast<void>(app.text().set_font_resolver(
                                                ryn::detail::make_default_ui_font_resolver(fonts, font_chain, scale)));
                                            inputs.set_display_scale(scale);
                                            static_cast<void>(platform.set_pointer_display_scale(scale));
                                        }
                                        const auto size = token_gallery_logical_viewport(metrics.pixel_width,
                                                                                         metrics.pixel_height, scale);
                                        viewport = {size.width, size.height};
                                        frames.request_frame();
                                    }
                                }
                            }
                            pages.dispatch(value);
                        }
                    },
                    event);
            }
            return frames.pending() || events.redraw_requested || (events.frame_requested && events.input.empty());
        } catch (const std::exception& failure) {
            error = failure.what();
            quit = true;
            return false;
        }
    }
};

class GalleryPageSubmitter final : public runtime::FrameSubmitter {
public:
    GalleryPageEvents& events;
    GalleryPagesHost& pages;
    ryn::detail::ButtonComponentHost& app;
    ryn::detail::InputComponentHost& inputs;
    ryn::detail::TextSceneService& text_scene;
    ryn::detail::SceneResources& resources;
    ryn::detail::SdlSceneRenderer& renderer;
    graphics::OrderedScene visible;
    std::string error;
    std::uint64_t cursor_syncs{};
    bool trace{};
    std::filesystem::path offscreen_frame;
#ifdef __linux__
    std::unique_ptr<AcceptanceFrameBoundary> frame_boundary;
#endif

    GalleryPageSubmitter(GalleryPageEvents& source, GalleryPagesHost& gallery,
                         ryn::detail::ButtonComponentHost& application, ryn::detail::InputComponentHost& input,
                         ryn::detail::TextSceneService& scene, ryn::detail::SceneResources& gpu,
                         ryn::detail::SdlSceneRenderer& backend)
        : events(source), pages(gallery), app(application), inputs(input), text_scene(scene), resources(gpu),
          renderer(backend) {}

    runtime::FrameSubmissionResult submit_frame(animation::AnimationTime time) override {
        try {
            static_cast<void>(app.tick_animations(time));
            if (!pages.synchronize(events.viewport)) {
                throw std::runtime_error("Gallery page layout failed");
            }
            if (trace) {
                std::cout << "page_acceptance_layout_complete\n" << std::flush;
            }
            const auto metrics = events.platform.window_metrics();
            if (metrics.pixel_width <= 0 || metrics.pixel_height <= 0) {
                return runtime::FrameSubmissionResult::deferred;
            }
            if (offscreen_frame.empty() &&
                !inputs.synchronize_input_area(double(metrics.coordinate_width) / events.viewport.width,
                                               metrics.coordinate_width, metrics.coordinate_height)) {
                throw std::runtime_error("Gallery host input area synchronization failed");
            }
            if (offscreen_frame.empty() && !events.platform.synchronize_pointer_cursor()) {
                throw std::runtime_error("Gallery host cursor synchronization failed");
            }
            if (offscreen_frame.empty()) {
                ++cursor_syncs;
            }
            if (!resources.synchronize({&app.services().surfaces().instances(),
                                        text_scene.atlas(),
                                        text_scene.glyph_scene().instances(),
                                        &app.rounded_effects(),
                                        {static_cast<std::uint32_t>(metrics.pixel_width),
                                         static_cast<std::uint32_t>(metrics.pixel_height), events.scale},
                                        &app.services().images()})) {
                throw std::runtime_error(renderer.last_error());
            }
            if (trace) {
                std::cout << "page_acceptance_upload_complete\n" << std::flush;
            }
            const runtime::Rect clip{0, 0, events.viewport.width, events.viewport.height};
            static_cast<void>(app.scene_composer().build_visible_scene(app.nodes(), clip, visible));
            if (!renderer.attach_scene(resources.attach(visible))) {
                throw std::runtime_error("Gallery scene attachment failed");
            }
            renderer.set_clear_color(resolve_theme(pages.theme()).alias().color_background_container);
            if (!offscreen_frame.empty()) {
                return renderer.save_frame_bmp(offscreen_frame) ? runtime::FrameSubmissionResult::submitted
                                                                : runtime::FrameSubmissionResult::failed;
            }
            if (trace) {
                std::cout << "page_acceptance_submit_begin\n" << std::flush;
#ifdef __linux__
                // Arm after layout/input synchronization, immediately before
                // the GPU's surface commit, rather than before polling events.
                frame_boundary =
                    std::make_unique<AcceptanceFrameBoundary>(static_cast<SDL_Window*>(events.platform.window()));
#endif
            }
            const auto submitted = renderer.submit_frame(time);
            if (trace) {
                std::cout << "page_acceptance_submit_complete\n" << std::flush;
            }
            return submitted;
        } catch (const std::exception& failure) {
            error = failure.what();
            return runtime::FrameSubmissionResult::failed;
        }
    }
};

bool owns(ryn::detail::ButtonComponentHost& app, runtime::ComponentId child, runtime::ComponentId root) {
    for (auto current = std::optional{child}; current; current = app.components().parent(*current)) {
        if (*current == root) {
            return true;
        }
    }
    return false;
}

void update_acceptance_theme(GalleryPagesHost& pages, const std::function<void(ThemeConfig&)>& update) {
    auto config = pages.theme();
    update(config);
    pages.set_theme(config);
}

void run_page_acceptance(const GalleryPageOptions& options, GalleryPageAcceptanceResult& result,
                         GalleryPageEvents& events, GalleryPagesHost& pages, ryn::detail::ButtonComponentHost& app,
                         ryn::detail::InputComponentHost& inputs, ryn::detail::SelectionComponentHost& selections,
                         ryn::detail::SdlSceneRenderer& renderer, GalleryPageSubmitter& submitter,
                         runtime::OnDemandFrameLoop& loop, runtime::FrameRequestState& frames) {
    write_gallery_page_acceptance(options.output, result);
    const auto deadline = std::chrono::steady_clock::now() + std::chrono::seconds(options.timeout_seconds);
    const auto check = [&result](bool condition, const char* message) {
        if (!condition) {
            throw std::runtime_error(message);
        }
        ++result.checks;
    };
    const auto render = [&] {
        frames.request_frame();
        while (std::chrono::steady_clock::now() < deadline) {
            const auto step = loop.tick();
            if (step == runtime::FrameLoopStep::failed || !events.error.empty()) {
                throw std::runtime_error(events.error.empty()
                                             ? (submitter.error.empty() ? renderer.last_error() : submitter.error)
                                             : events.error);
            }
            if (step == runtime::FrameLoopStep::submitted) {
                ++result.submitted_frames;
                result.host_input_area = inputs.sessions().diagnostics().areas > 0;
                result.host_cursor_sync = submitter.cursor_syncs > 0;
                write_gallery_page_acceptance(options.output, result);
#ifdef __linux__
                if (!options.visual) {
                    check(bool(submitter.frame_boundary), "Wayland page frame boundary was not armed");
                    const auto frame_deadline =
                        std::min(deadline, std::chrono::steady_clock::now() + std::chrono::seconds(5));
                    while (!submitter.frame_boundary->done()) {
                        if (std::chrono::steady_clock::now() >= frame_deadline) {
                            throw std::runtime_error("Wayland page frame callback deadline exceeded");
                        }
                        events.consume(events.platform.wait_events(10));
                        if (!events.error.empty() || events.quit) {
                            throw std::runtime_error(events.error.empty() ? "Wayland page window closed"
                                                                          : events.error);
                        }
                    }
                    submitter.frame_boundary.reset();
                    // A frame callback permits rendering the next frame; it is not
                    // a presentation fence. Pace the finite host's next input/action
                    // to the display refresh interval instead of bursting actions
                    // into the same compositor refresh cycle.
                    const auto* mode = SDL_GetCurrentDisplayMode(
                        SDL_GetDisplayForWindow(static_cast<SDL_Window*>(events.platform.window())));
                    check(mode && std::isfinite(mode->refresh_rate) && mode->refresh_rate > 0,
                          "native acceptance display has no refresh rate");
                    const auto next_action =
                        std::min(frame_deadline, std::chrono::steady_clock::now() +
                                                     std::chrono::duration_cast<std::chrono::steady_clock::duration>(
                                                         std::chrono::duration<double>{1.0 / mode->refresh_rate}));
                    while (std::chrono::steady_clock::now() < next_action) {
                        events.consume(events.platform.wait_events(1));
                        if (!events.error.empty() || events.quit) {
                            throw std::runtime_error(events.error.empty() ? "Wayland page window closed"
                                                                          : events.error);
                        }
                    }
                    ++result.checks;
                    write_gallery_page_acceptance(options.output, result);
                    std::cout << "page_acceptance_frame_callback\n" << std::flush;
                }
#endif
                return;
            }
            events.consume(events.platform.wait_events(10));
        }
        throw std::runtime_error("Gallery page acceptance deadline exceeded");
    };
    const auto read_frame = [&](const std::string& name) {
        const auto path = options.output.parent_path() / (name + ".bmp");
        check(renderer.save_frame_bmp(path), "Space visual frame readback failed");
        const auto utf8 = path.u8string();
        std::unique_ptr<SDL_Surface, decltype(&SDL_DestroySurface)> image{
            SDL_LoadBMP(reinterpret_cast<const char*>(utf8.c_str())), SDL_DestroySurface};
        check(bool(image), "Space visual BMP could not be decoded");
        return image;
    };
    const auto check_pixel = [&](SDL_Surface* image, runtime::Point point, Color expected) {
        float red{};
        float green{};
        float blue{};
        float alpha{};
        check(SDL_ReadSurfacePixelFloat(image, static_cast<int>(point.x), static_cast<int>(point.y), &red, &green,
                                        &blue, &alpha),
              "Space visual pixel is outside its render target");
        if (std::abs(red - expected.red()) > .02F || std::abs(green - expected.green()) > .02F ||
            std::abs(blue - expected.blue()) > .02F) {
            std::cerr << "space_pixel=" << point.x << ',' << point.y << " expected=" << expected.red() << ','
                      << expected.green() << ',' << expected.blue() << " actual=" << red << ',' << green << ',' << blue
                      << '\n';
            throw std::runtime_error("Space GPU fill or focus has a clipped row or partition seam");
        }
        ++result.checks;
    };
    const auto resize = [&](int width) {
        const auto metrics = events.platform.window_metrics();
        const float coordinate_scale = metrics.coordinate_width / events.viewport.width;
        check(renderer.resize_window(static_cast<int>(std::round(width * coordinate_scale)),
                                     static_cast<int>(std::round(900 * coordinate_scale))),
              "native resize request failed");
        while (std::abs(events.viewport.width - width) > 2 || std::abs(events.viewport.height - 900) > 2) {
            if (std::chrono::steady_clock::now() >= deadline) {
                throw std::runtime_error("native resize did not settle");
            }
            events.consume(events.platform.wait_events(10));
            check(events.error.empty() && !events.quit, "native window closed or input adapter failed during resize");
        }
    };
    const auto activate = [&](runtime::ComponentId owner) {
        for (const auto id : app.interactions().declaration_order()) {
            const auto* record = app.interactions().find(id);
            if (record && record->focusable && record->eligible && app.interactions().branch_active(id) &&
                owns(app, record->component, owner)) {
                check(app.focus().state().window_active &&
                          (app.focus().state().focused == id ||
                           app.focus().request_focus(id, input::FocusModality::keyboard)),
                      "native sample action could not receive keyboard focus");
                pages.dispatch(input::KeyboardInputEvent{input::Key::enter, input::KeyAction::down});
                pages.dispatch(input::KeyboardInputEvent{input::Key::enter, input::KeyAction::up});
                return;
            }
        }
        throw std::runtime_error("native sample action has no active keyboard target");
    };
    const auto edit_input = [&](runtime::ComponentId owner) {
        for (const auto& input : inputs.mounted_inputs()) {
            if (owns(app, input.component, owner) && app.interactions().branch_active(input.interaction)) {
                check(app.focus().request_focus(input.interaction, input::FocusModality::keyboard),
                      "native retained input could not receive focus");
                const auto session = inputs.sessions().active();
                check(session.valid(), "native retained input has no text session");
                pages.dispatch(input::TextCommitted{String{u8" native draft"}, session});
                render();
                return std::pair{input.component, std::string{inputs.display_snapshot(input.component).text}};
            }
        }
        throw std::runtime_error("native layout sample has no retained input");
    };
    const auto key = [&](input::InteractionId interaction, input::Key value) {
        check(app.focus().state().focused == interaction ||
                  app.focus().request_focus(interaction, input::FocusModality::keyboard),
              "native layout action could not receive focus");
        pages.dispatch(input::KeyboardInputEvent{value, input::KeyAction::down});
        pages.dispatch(input::KeyboardInputEvent{value, input::KeyAction::up});
        render();
    };
    const auto bounds = [&](runtime::ComponentId id) {
        const auto& node = app.nodes().require(app.components().root(id));
        auto rect = node.bounds;
        rect.x += node.translation.x;
        rect.y += node.translation.y;
        return rect;
    };
    const auto reveal = [&](runtime::ComponentId id) {
        const auto lane = pages.snapshot().layout.document;
        const auto r = bounds(id);
        events.inject_scroll_at(lane.x + lane.width - 8, lane.y + lane.height / 2,
                                -(r.y - lane.y - lane.height / 3) / 48);
        render();
        const auto visible = bounds(id);
        check(visible.y >= lane.y && visible.y + 30 < lane.y + lane.height,
              "native sample target is outside document viewport");
    };
    const auto pointer = [&](input::PointerAction action, runtime::Point point) {
        pages.dispatch(
            input::PointerInputEvent{input::PointerIdentity::mouse(), action,
                                     action == input::PointerAction::move || action == input::PointerAction::cancel
                                         ? input::PointerButton::none
                                         : input::PointerButton::primary,
                                     point.x, point.y});
    };
    const auto captured = [&] {
        const auto state = app.pointer().state(input::PointerIdentity::mouse());
        return state && state->capture.has_value();
    };
    const auto press = [&](input::Key value) {
        pages.dispatch(input::KeyboardInputEvent{value, input::KeyAction::down});
        pages.dispatch(input::KeyboardInputEvent{value, input::KeyAction::up});
        render();
    };
    const auto focus_editor = [&](runtime::ComponentId owner) {
        for (const auto& editor : inputs.mounted_inputs()) {
            if (owns(app, editor.component, owner) && app.interactions().branch_active(editor.interaction)) {
                check(app.focus().state().focused == editor.interaction ||
                          app.focus().request_focus(editor.interaction, input::FocusModality::keyboard),
                      "native completion editor focus failed");
                check(inputs.sessions().active().valid(), "native completion editor session missing");
                return editor;
            }
        }
        throw std::runtime_error("native completion editor missing");
    };
    const auto replace_text = [&](runtime::ComponentId owner, String text) {
        const auto editor = focus_editor(owner);
        pages.dispatch(input::KeyboardInputEvent{input::Key::a, input::KeyAction::down, input::KeyModifier::control});
        pages.dispatch(input::KeyboardInputEvent{input::Key::a, input::KeyAction::up, input::KeyModifier::control});
        if (text.empty()) {
            pages.dispatch(input::KeyboardInputEvent{input::Key::backspace, input::KeyAction::down});
            pages.dispatch(input::KeyboardInputEvent{input::Key::backspace, input::KeyAction::up});
        } else {
            pages.dispatch(input::TextCommitted{std::move(text), inputs.sessions().active()});
        }
        render();
        return editor;
    };
    const auto click = [&](runtime::ComponentId owner) {
        const auto rect = bounds(owner);
        const runtime::Point center{rect.x + rect.width / 2, rect.y + rect.height / 2};
        pointer(input::PointerAction::down, center);
        pointer(input::PointerAction::up, center);
        render();
    };
    const auto capture_editor = [&](runtime::ComponentId owner) {
        const auto editor = focus_editor(owner);
        const auto area = inputs.layout_snapshot(editor.component).viewport;
        pointer(input::PointerAction::down, {area.x + 4, area.y + area.height / 2});
        render();
        check(captured(), "native completion editor did not capture pointer before retirement");
    };
    const auto has_text = [&](String text) {
        return std::ranges::any_of(app.services().text().mounted_texts(), [&](const auto& item) {
            return app.services().text().scene_service().text_state(item.scene).content().bytes().find(text.bytes()) !=
                   std::string_view::npos;
        });
    };
    constexpr std::array widths{1440, 960, 728, 480, 360};
    input::TextInputSessionStamp previous_session;
    runtime::ComponentId previous_root;
    GalleryPageRoute previous_route;
    for (std::size_t i = 0; i < options.pages.size(); ++i) {
        const auto& route = options.pages[i];
        const int width = options.width ? options.width : widths[i % widths.size()];
        std::cout << "page_acceptance_stage=" << i << " page=" << route.component_identity << " width=" << width << '\n'
                  << std::flush;
        if (options.visual) {
            std::cout << "visual_phase=resize_begin\n" << std::flush;
        }
        resize(width);
        if (options.visual) {
            std::cout << "visual_phase=resize_complete\n" << std::flush;
        }
        if (options.visual && i > 0) {
            pages.request({});
            pages.process();
            render();
        }
        update_acceptance_theme(pages, [&](ThemeConfig& config) {
            config.algorithms = i % 3 == 0   ? std::vector{ThemeAlgorithm::Default}
                                : i % 3 == 1 ? std::vector{ThemeAlgorithm::Dark}
                                             : std::vector{ThemeAlgorithm::Dark, ThemeAlgorithm::Compact};
            if (options.visual && route.component_identity == "ant.component.switch") {
                config.seed.motion = true;
            }
        });
        if (options.visual) {
            std::cout << "visual_phase=theme_complete\n" << std::flush;
        }
        check(pages.request(route), "finite page route rejected");
        static_cast<void>(pages.process());
        if (options.visual) {
            std::cout << "visual_phase=mount_complete\n" << std::flush;
            render();
            std::optional<ryn::detail::MountedInputComponent> retained_listy_editor;
            capture_gallery_visuals(
                route, i, app, pages, events.viewport, events.scale, options.output.parent_path(), render,
                [&](float delta) {
                    const auto lane = pages.snapshot().layout.document;
                    events.inject_scroll_at(lane.x + lane.width - 8, lane.y + lane.height / 2, delta);
                },
                [&] {
                    std::vector<runtime::ComponentId> targets;
                    const auto root = pages.snapshot().page;
                    const auto gather = [&](auto& host, auto trigger) {
                        for (const auto id : host.mounted()) {
                            if (owns(app, id, root)) {
                                const auto target = trigger(id, host.snapshot(id));
                                // Embedded controls have no popup selector.
                                if (target.valid()) {
                                    targets.push_back(target);
                                }
                            }
                        }
                    };
                    const auto name = route.component_identity;
                    if (name == "ant.component.input" || name == "ant.component.mentions") {
                        for (const auto& editor : inputs.mounted_inputs()) {
                            if (owns(app, editor.component, root) &&
                                app.interactions().branch_active(editor.interaction)) {
                                targets.push_back(editor.component);
                            }
                        }
                    } else if (name == "ant.component.switch") {
                        for (const auto& item : selections.mounted()) {
                            if (!item.checkbox && !item.radio && owns(app, item.component, root)) {
                                targets.push_back(item.component);
                            }
                        }
                    } else if (name == "ant.component.select") {
                        gather(app.services().select(), [](auto, const auto& s) { return s.selector; });
                    } else if (name == "ant.component.tree-select") {
                        gather(app.services().tree_select(), [](auto, const auto& s) { return s.selector; });
                    } else if (name == "ant.component.cascader") {
                        gather(app.services().cascader(), [](auto, const auto& s) { return s.selector; });
                    } else if (name == "ant.component.date-picker") {
                        gather(app.services().date_picker(), [](auto, const auto& s) { return s.input; });
                    } else if (name == "ant.component.time-picker") {
                        gather(app.services().time_picker(), [](auto, const auto& s) { return s.selector; });
                    } else if (name == "ant.component.auto-complete") {
                        gather(app.services().auto_complete(), [](auto, const auto& s) { return s.input; });
                    } else if (name == "ant.component.color-picker") {
                        gather(app.services().color_picker(), [](auto id, const auto&) { return id; });
                    } else if (name == "ant.component.dropdown") {
                        gather(app.services().dropdown(), [](auto, const auto& s) { return s.trigger; });
                    } else if (name == "ant.component.popover") {
                        const auto trigger = pages.target("gallery.popover.trigger");
                        if (trigger.valid()) {
                            targets.push_back(trigger);
                        }
                    } else if (name == "ant.component.tooltip") {
                        gather(app.services().tooltip(), [](auto id, const auto&) { return id; });
                    } else {
                        const auto add = [&](std::string_view identity) {
                            const auto id = pages.target(identity);
                            if (id.valid()) {
                                targets.push_back(id);
                            }
                        };
                        if (name == "ant.component.card") {
                            const auto card = pages.target("gallery.card.completion");
                            const auto tabs_region = app.services().card().snapshot(card).tabs;
                            const auto mounted_tabs = app.services().tabs().mounted();
                            const auto tabs = std::ranges::find_if(
                                mounted_tabs, [&](auto item) { return owns(app, item, tabs_region); });
                            check(tabs != mounted_tabs.end(), "native Card completion Tabs instance missing");
                            const auto headers = app.services().tabs().snapshot(*tabs).headers;
                            check(headers.size() == 2, "native Card completion Tabs headers missing");
                            targets.push_back(headers[1]);
                            add("gallery.card.tabs-reorder");
                            targets.push_back(headers[0]);
                            targets.push_back(headers[1]);
                            add("gallery.card.grid-columns");
                            add("gallery.card.grid-columns");
                            add("gallery.card.toggle-loading");
                            add("gallery.card.toggle-loading");
                        } else if (name == "ant.component.empty") {
                            for (const auto id :
                                 {"gallery.empty.fail-image", "gallery.empty.retry-image",
                                  "gallery.empty.complete-image", "gallery.empty.retry-image",
                                  "gallery.empty.dynamic-description", "gallery.empty.dynamic-description",
                                  "gallery.empty.dynamic-extra", "gallery.empty.dynamic-extra"}) {
                                add(id);
                            }
                        } else if (name == "ant.component.listy") {
                            for (const auto id :
                                 {"gallery.listy.grid", "gallery.listy.reorder", "gallery.listy.pagination",
                                  "gallery.listy.scroll-end", "gallery.listy.direction", "gallery.listy.scroll-end",
                                  "gallery.listy.empty", "gallery.listy.empty"}) {
                                add(id);
                            }
                        } else if (name == "ant.component.modal") {
                            for (const auto id : {"gallery.modal.basic", "gallery.modal.confirm-loading",
                                                  "gallery.modal.centered", "gallery.modal.long-body"}) {
                                add(id);
                            }
                        } else if (name == "ant.component.popconfirm") {
                            add("gallery.popconfirm.single");
                            add("gallery.popconfirm.controlled");
                        } else if (name == "ant.component.drawer") {
                            const auto owner = pages.target("gallery.sample.drawer.account");
                            for (const auto& button : app.mounted_buttons()) {
                                if (owns(app, button.component, owner) &&
                                    app.interactions().branch_active(button.interaction)) {
                                    targets.push_back(button.component);
                                    if (targets.size() == 4) {
                                        break;
                                    }
                                }
                            }
                        } else if (name == "ant.component.qr-code") {
                            for (const auto id :
                                 {"gallery.qrcode.expire", "gallery.qrcode.status", "gallery.qrcode.status",
                                  "gallery.qrcode.status", "gallery.qrcode.logo", "gallery.qrcode.invalid"}) {
                                add(id);
                            }
                        } else if (name == "ant.component.upload") {
                            for (const auto id :
                                 {"gallery.upload.fixture", "gallery.upload.progress", "gallery.upload.success",
                                  "gallery.upload.list", "gallery.upload.list", "gallery.upload.custom"}) {
                                add(id);
                            }
                        } else if (name == "ant.component.image") {
                            add("gallery.image.preview");
                        } else if (name == "ant.component.spin" || name == "ant.component.progress") {
                            const auto owner =
                                pages.target(name == "ant.component.spin" ? "gallery.sample.spin.loading"
                                                                          : "gallery.sample.progress.values");
                            for (const auto& button : app.mounted_buttons()) {
                                if (owns(app, button.component, owner) &&
                                    app.interactions().branch_active(button.interaction)) {
                                    targets.push_back(button.component);
                                    break;
                                }
                            }
                        } else if (name == "ant.component.message") {
                            for (const auto id : {"gallery.message.open", "gallery.message.update",
                                                  "gallery.notification.open", "gallery.notification.update",
                                                  "gallery.notification.placement", "gallery.notification.status"}) {
                                add(id);
                            }
                        }
                    }
                    return targets;
                },
                [&](runtime::ComponentId id) {
                    const bool listy = route.component_identity == "ant.component.listy";
                    const auto listy_owner = listy ? pages.target("gallery.listy.retained") : runtime::ComponentId{};
                    if (listy && !retained_listy_editor) {
                        const auto snapshot = app.services().listy().snapshot(listy_owner);
                        if (!snapshot.items.empty()) {
                            retained_listy_editor =
                                replace_text(snapshot.items.front().component, u8"native retained Listy draft");
                        }
                    }
                    if (route.component_identity == "ant.component.color-picker") {
                        const auto order = app.interactions().declaration_order();
                        const bool eligible = std::ranges::any_of(order, [&](auto interaction) {
                            const auto* record = app.interactions().find(interaction);
                            return record && record->focusable && record->eligible &&
                                   app.interactions().branch_active(interaction) && owns(app, record->component, id);
                        });
                        if (eligible) {
                            activate(id);
                        }
                        return;
                    }
                    const auto rect = bounds(id);
                    pointer(input::PointerAction::move, {rect.x + rect.width / 2, rect.y + rect.height / 2});
                    click(id);
                    if (listy && retained_listy_editor) {
                        const auto& editor = *retained_listy_editor;
                        const auto snapshot = app.services().listy().snapshot(listy_owner);
                        if (snapshot.items.empty()) {
                            check(!inputs.editors().find(editor.editor), "native Listy empty retained a stale editor");
                            retained_listy_editor.reset();
                        } else {
                            check(app.components().contains(editor.component) &&
                                      inputs.editors().require(editor.editor).value() ==
                                          String{u8"native retained Listy draft"}.bytes(),
                                  "native Listy mutation lost retained row draft");
                            if (id == pages.target("gallery.listy.scroll-end")) {
                                check(std::abs(snapshot.offset - snapshot.maximum) < 1,
                                      "native Listy scroll-to-end did not reach viewport extent");
                            }
                        }
                    }
                    if (route.component_identity == "ant.component.mentions" && inputs.sessions().active().valid()) {
                        pages.dispatch(input::TextCommitted{String{u8" @a"}, inputs.sessions().active()});
                    }
                    if (route.component_identity == "ant.component.tooltip" ||
                        route.component_identity == "ant.component.dropdown") {
                        SDL_Delay(350);
                    }
                },
                [&] {
                    pages.dispatch(input::KeyboardInputEvent{input::Key::escape, input::KeyAction::down});
                    pages.dispatch(input::KeyboardInputEvent{input::Key::escape, input::KeyAction::up});
                });
            ++result.cases;
            write_gallery_page_acceptance(options.output, result);
            continue;
        }
        const bool replaced = previous_root.valid() && (route.kind != previous_route.kind ||
                                                        route.component_identity != previous_route.component_identity ||
                                                        route.family != previous_route.family);
        if (replaced) {
            check(!app.components().contains(previous_root), "native page owner survived navigation");
            if (previous_route.component_identity == "ant.component.checkbox" ||
                previous_route.component_identity == "ant.component.radio" ||
                previous_route.component_identity == "ant.component.switch" ||
                previous_route.component_identity == "ant.component.slider" ||
                previous_route.component_identity == "ant.component.rate" ||
                previous_route.component_identity == "ant.component.input" ||
                previous_route.component_identity == "ant.component.avatar" ||
                previous_route.component_identity == "ant.component.badge" ||
                previous_route.component_identity == "ant.component.tag") {
                check(!captured(), "native existing DataEntry retirement retained capture");
                check(!app.focus().state().focused ||
                          !owns(app, app.interactions().require(*app.focus().state().focused).component, previous_root),
                      "native existing DataEntry retirement retained focus");
            }
            if (previous_route.component_identity == "ant.component.typography") {
                check(app.services().typography().mounted().empty(), "native Typography survived page retirement");
            }
            if (previous_route.component_identity == "ant.component.float-button") {
                check(app.services().float_button().mounted().empty(), "native FloatButton survived page retirement");
                const auto pointer = app.pointer().state(input::PointerIdentity::mouse());
                check(!pointer || !pointer->capture, "native FloatButton page retirement retained capture");
            }
            if (previous_route.component_identity == "ant.component.breadcrumb") {
                check(app.services().breadcrumb().mounted().empty(), "native Breadcrumb survived page retirement");
                check(!captured(), "native Breadcrumb retirement retained capture");
            }
            if (previous_route.component_identity == "ant.component.anchor") {
                check(app.services().anchor().mounted().empty(), "native Anchor survived page retirement");
                check(!captured(), "native Anchor retirement retained capture");
            }
            if (previous_route.component_identity == "ant.component.layout") {
                check(app.services().shell_layout().mounted().empty(), "native Layout survived page retirement");
                check(!captured(), "native Layout retirement retained capture");
            }
            if (previous_route.component_identity == "ant.component.splitter") {
                check(app.services().splitter().mounted().empty(), "native Splitter survived page retirement");
                check(!captured(), "native Splitter retirement retained capture");
            }
            if (previous_route.component_identity == "ant.component.calendar") {
                check(app.services().calendar().mounted().empty() && !captured(),
                      "native Calendar page retirement retained roots/capture");
            }
            if (previous_route.component_identity == "ant.component.collapse" ||
                previous_route.component_identity == "ant.component.segmented" ||
                previous_route.component_identity == "ant.component.descriptions" ||
                previous_route.component_identity == "ant.component.timeline") {
                check(!captured(), "native retained DataDisplay retirement retained capture");
            }
            if (previous_session.valid()) {
                check(!inputs.editors().find(previous_session.owner), "native editor survived page retirement");
                check(inputs.sessions().active() != previous_session, "native text session survived page retirement");
            }
        }
        render();
        const auto snapshot = pages.snapshot();
        check(!snapshot.failed && snapshot.page.valid(), "native page mount failed");
        check(snapshot.route.kind == route.kind && snapshot.route.component_identity == route.component_identity &&
                  snapshot.route.family == route.family,
              "native page selected the wrong family");
        check(snapshot.layout.content.x >= 0 &&
                  snapshot.layout.content.x + snapshot.layout.content.width <= events.viewport.width &&
                  snapshot.document.viewport_extent > 0,
              "native document lane escaped viewport");
        const auto& title = app.nodes().require(app.components().root(pages.target("gallery.page.usage")));
        check(std::abs(title.bounds.x - snapshot.layout.content.x) < 1 &&
                  std::abs(title.bounds.width - snapshot.layout.content.width) < 1,
              "native host geometry differs from document lane");
        const auto root = snapshot.page;
        previous_session = {};
        for (const auto& input : inputs.mounted_inputs()) {
            if (route.component_identity == "ant.component.input" && owns(app, input.component, root) &&
                app.interactions().branch_active(input.interaction)) {
                static_cast<void>(app.focus().request_focus(input.interaction, input::FocusModality::keyboard));
                if (app.focus().state().focused != input.interaction) {
                    continue;
                }
                previous_session = inputs.sessions().active();
                check(previous_session.valid(), "native input did not start a text session");
                pages.dispatch(input::CompositionChanged{String{u8"ni"}, {2, 0}, previous_session});
                render();
                check(inputs.sessions().active() == previous_session, "native composition lost its owner");
                break;
            }
        }
        accept_existing_data_entry(
            route, {app, inputs, pages, selections, check, render, reveal, activate, click, pointer, press});
        accept_status_data_display(
            route, {app, inputs, pages, selections, check, render, reveal, activate, click, pointer, press}, resize);
        if (route.component_identity == "ant.component.tag") {
            previous_session = inputs.sessions().active();
        }
        const auto reveal_owned = [&](runtime::ComponentId id) {
            const auto lane = pages.snapshot().layout.document;
            const auto r = bounds(id);
            const bool needed = r.y < lane.y || r.y + std::min(r.height, lane.height - 30) > lane.y + lane.height;
            if (needed) {
                reveal(id);
            }
        };
        const bool owned_data_display = route.component_identity == "ant.component.collapse" ||
                                        route.component_identity == "ant.component.segmented" ||
                                        route.component_identity == "ant.component.descriptions" ||
                                        route.component_identity == "ant.component.timeline";
        const auto enable_owned_motion = [&] {
            update_acceptance_theme(pages, [](ThemeConfig& config) { config.seed.motion = true; });
            app.services().set_motion_preference(animation::MotionPreference::normal);
        };
        if (route.component_identity == "ant.component.collapse") {
            auto& host = app.services().collapse();
            const auto owner = pages.target("gallery.collapse.dynamic");
            reveal_owned(owner);
            enable_owned_motion();
            const auto original = host.snapshot(owner);
            const auto editor = replace_text(original.panels[0].body, u8"native-collapse-draft");
            activate(pages.target("gallery.collapse.reorder"));
            render();
            auto current = host.snapshot(owner);
            check(current.panels[1].body == original.panels[0].body &&
                      inputs.editors().require(editor.editor).value() == "native-collapse-draft",
                  "native Collapse keyed reorder lost draft");
            reveal_owned(current.panels[1].header);
            click(current.panels[1].header);
            check(!host.snapshot(owner).panels[1].expanded && !inputs.sessions().active().valid(),
                  "native Collapse closing retained input");
            const auto closing_deadline =
                std::min(deadline, std::chrono::steady_clock::now() + std::chrono::seconds(2));
            while (host.snapshot(owner).panels[1].animating && std::chrono::steady_clock::now() < closing_deadline) {
                render();
            }
            check(!host.snapshot(owner).panels[1].animating, "native Collapse closing motion did not settle");
            click(current.panels[1].header);
            const auto opening_deadline =
                std::min(deadline, std::chrono::steady_clock::now() + std::chrono::seconds(2));
            while (host.snapshot(owner).panels[1].animating && std::chrono::steady_clock::now() < opening_deadline) {
                render();
            }
            check(!host.snapshot(owner).panels[1].animating && host.snapshot(owner).panels[1].motion == 1,
                  "native Collapse opening motion did not settle");
            check(inputs.editors().require(editor.editor).value() == "native-collapse-draft",
                  "native Collapse reopening lost draft");
            activate(pages.target("gallery.collapse.ghost"));
            render();
            reveal_owned(current.panels[1].body);
            capture_editor(current.panels[1].body);
            std::cout
                << "collapse_native=passed keyed_draft=true finite_motion=true ghost=true capture_retirement=armed\n";
        }
        if (route.component_identity == "ant.component.segmented") {
            auto& host = app.services().segmented();
            const auto owner = pages.target("gallery.segmented.dynamic");
            reveal_owned(owner);
            enable_owned_motion();
            const auto original = host.snapshot(owner);
            click(original.items[1]);
            const auto pill_deadline = std::min(deadline, std::chrono::steady_clock::now() + std::chrono::seconds(2));
            while (host.snapshot(owner).motion < 1 && std::chrono::steady_clock::now() < pill_deadline) {
                render();
            }
            check(host.snapshot(owner).motion == 1, "native Segmented pill did not settle");
            check(host.snapshot(owner).selected_value == SegmentedValue{2.0}, "native Segmented typed callback failed");
            activate(pages.target("gallery.segmented.reorder"));
            render();
            check(host.snapshot(owner).items[0] == original.items[1],
                  "native Segmented reorder remounted selected option");
            activate(pages.target("gallery.segmented.rtl"));
            render();
            reveal_owned(owner);
            click(host.snapshot(owner).items[0]);
            press(input::Key::right);
            check(host.snapshot(owner).selected_value == SegmentedValue{1.0},
                  "native Segmented roving keyboard failed");
            std::cout
                << "segmented_native=passed typed_callback=true keyed_identity=true rtl=true keyboard=true pill=true\n";
        }
        if (route.component_identity == "ant.component.descriptions") {
            auto& host = app.services().descriptions();
            const auto owner = pages.target("gallery.descriptions.dynamic");
            reveal_owned(owner);
            enable_owned_motion();
            const auto original = host.snapshot(owner);
            const auto editor = replace_text(original.items[0].content, u8"native-descriptions-draft");
            activate(pages.target("gallery.descriptions.reorder"));
            render();
            check(host.snapshot(owner).items[2].content == original.items[0].content &&
                      inputs.editors().require(editor.editor).value() == "native-descriptions-draft",
                  "native Descriptions reorder lost draft");
            activate(pages.target("gallery.descriptions.rtl"));
            render();
            resize(500);
            render();
            check(host.snapshot(owner).rows.size() == 3, "native Descriptions window breakpoint failed");
            reveal_owned(host.snapshot(owner).items[2].content);
            capture_editor(host.snapshot(owner).items[2].content);
            std::cout
                << "descriptions_native=passed keyed_draft=true responsive=true rtl=true capture_retirement=armed\n";
        }
        if (route.component_identity == "ant.component.timeline") {
            auto& host = app.services().timeline();
            const auto owner = pages.target("gallery.timeline.dynamic");
            reveal_owned(owner);
            enable_owned_motion();
            const auto original = host.snapshot(owner);
            check(original.items[0].tail.width > original.items[0].tail.height,
                  "native Timeline horizontal rail missing");
            const auto editor = replace_text(original.items[1].content, u8"native-timeline-draft");
            activate(pages.target("gallery.timeline.reorder"));
            render();
            check(host.snapshot(owner).items[1].content == original.items[1].content &&
                      inputs.editors().require(editor.editor).value() == "native-timeline-draft",
                  "native Timeline reorder lost draft");
            activate(pages.target("gallery.timeline.tone-motion"));
            render();
            check(has_text(u8"状态色：红色"), "native Timeline status-color callback missing");
            const auto color_deadline = std::min(deadline, std::chrono::steady_clock::now() + std::chrono::seconds(2));
            while (host.snapshot(owner).motion < 1 && std::chrono::steady_clock::now() < color_deadline) {
                render();
            }
            check(host.snapshot(owner).motion == 1, "native Timeline color transition did not settle");
            activate(pages.target("gallery.timeline.loading-motion"));
            render();
            check(app.services().animations().size() > 0, "native Timeline loading has no real motion");
            activate(pages.target("gallery.timeline.orientation"));
            render();
            activate(pages.target("gallery.timeline.title-span"));
            render();
            activate(pages.target("gallery.timeline.loading-motion"));
            render();
            reveal_owned(host.snapshot(owner).items[1].content);
            capture_editor(host.snapshot(owner).items[1].content);
            std::cout << "timeline_native=passed horizontal=true keyed_draft=true loading=true title_span=true "
                         "capture_retirement=armed\n";
        }
        if (owned_data_display) {
            previous_session = inputs.sessions().active();
            update_acceptance_theme(pages, [](ThemeConfig& config) { config.seed.motion = false; });
            check(pages.request(GalleryPageRoute{GalleryPageKind::introduction}),
                  "native retained DataDisplay retirement route rejected");
            pages.process();
            render();
            check(!app.components().contains(root) && !captured() && !inputs.sessions().active().valid(),
                  "native retained DataDisplay navigation leaked root/capture/session");
            if (previous_session.valid()) {
                check(!inputs.editors().find(previous_session.owner),
                      "native retained DataDisplay navigation retained its editor generation");
            }
        }
        if (route.component_identity == "ant.component.input") {
            previous_session = inputs.sessions().active();
        }
        if (route.component_identity == "ant.component.typography") {
            const auto rich = pages.target("gallery.typography.rich-content");
            check(rich.valid(), "native Typography rich sample is missing");
            const auto original = app.services().typography().snapshot(rich).original;
            runtime::ComponentId copy;
            for (const auto id : app.interactions().declaration_order()) {
                const auto* record = app.interactions().find(id);
                if (record && record->focusable && record->eligible && app.interactions().branch_active(id) &&
                    owns(app, record->component, rich)) {
                    copy = record->component;
                }
            }
            check(copy.valid(), "native rich Typography has no copy action");
            activate(copy);
            render();
            check(app.services().typography().snapshot(rich).copied,
                  "native rich Typography clipboard publication failed");
            std::unique_ptr<char, decltype(&SDL_free)> plain{SDL_GetClipboardText(), SDL_free};
            std::size_t html_size{};
            std::unique_ptr<void, decltype(&SDL_free)> html{SDL_GetClipboardData("text/html", &html_size), SDL_free};
            check(plain && std::string_view{plain.get()} == original.bytes(),
                  "native rich clipboard plain fallback differs from full source");
            check(html && html_size > 0, "native rich clipboard has no HTML MIME payload");
            const std::string_view markup{static_cast<const char*>(html.get()), html_size};
            check(markup.find("<strong>") != std::string_view::npos &&
                      markup.find("&lt;&gt;&amp;") != std::string_view::npos &&
                      markup.find("<kbd>") != std::string_view::npos,
                  "native rich clipboard lost semantic tags or escaped source");
            resize(500);
            render();
            const auto narrow = app.services().typography().snapshot(rich);
            check(narrow.original == original && narrow.truncated,
                  "native rich Typography resize changed source or lost ellipsis");
            std::cout << "typography_native=passed rich=true own_mime_roundtrip=true resize=true\n";
        }
        if (route.component_identity == "ant.component.grid") {
            const auto row = pages.target("gallery.grid.responsive");
            check(row.valid(), "native Grid responsive sample is missing");
            const auto draft = edit_input(row);
            resize(1000);
            render();
            const auto wide = ryn::detail::grid_columns(app.components(), row);
            check(wide.size() == 4 && wide.front().options.span == 6,
                  "native Grid did not resolve the window lg breakpoint");
            resize(500);
            render();
            const auto narrow = ryn::detail::grid_columns(app.components(), row);
            check(narrow.size() == 4 && narrow.front().options.span == 24 &&
                      narrow.front().component == wide.front().component &&
                      inputs.display_snapshot(draft.first).text == draft.second,
                  "native Grid narrow reflow lost its responsive span or retained draft");
            check(pages.sample_runs("gallery.sample.grid.layouts") == 1, "native Grid reflow reran its sample factory");
            previous_session = inputs.sessions().active();
            std::cout << "grid_native=passed window_breakpoints=true retained_draft=true\n";
        }
        if (route.component_identity == "ant.component.breadcrumb") {
            const auto owner = pages.target("gallery.breadcrumb.dynamic-component");
            auto& breadcrumbs = app.services().breadcrumb();
            const auto run = [&](runtime::ComponentId target) {
                reveal(target);
                activate(target);
                render();
            };
            const auto original = breadcrumbs.snapshot(owner);
            check(original.items.size() == 4, "native dynamic Breadcrumb absent");
            reveal(original.items[1]);
            const auto draft = edit_input(original.items[1]);
            const auto editor_session = inputs.sessions().active();
            run(pages.target("gallery.breadcrumb.reorder"));
            const auto reordered = breadcrumbs.snapshot(owner);
            check(reordered.items.front() == original.items.back() &&
                      reordered.items.back() == original.items.front() && app.components().contains(draft.first),
                  "native Breadcrumb reorder lost retained draft");
            check(std::string{inputs.display_snapshot(draft.first).text} == draft.second,
                  "native Breadcrumb reordered editor lost its actual draft");
            run(pages.target("gallery.breadcrumb.topology"));
            check(breadcrumbs.snapshot(owner).items.size() == 3, "native Breadcrumb removal failed");
            run(pages.target("gallery.breadcrumb.direction"));
            run(breadcrumbs.snapshot(owner).items.back());
            bool menu_open = false;
            std::vector<runtime::ComponentId> owned_popups;
            for (auto popup : app.services().dropdown().mounted()) {
                if (owns(app, popup, owner) && app.services().dropdown().snapshot(popup).geometry.visible) {
                    menu_open = true;
                    owned_popups.push_back(app.services().dropdown().snapshot(popup).popup);
                }
            }
            check(menu_open, "native Breadcrumb built-in menu did not open");
            pages.dispatch(input::KeyboardInputEvent{input::Key::enter, input::KeyAction::down});
            pages.dispatch(input::KeyboardInputEvent{input::Key::enter, input::KeyAction::up});
            render();
            bool callback_seen = false;
            for (const auto& text : app.services().text().mounted_texts()) {
                const auto content = app.services().text().scene_service().text_state(text.scene).content().bytes();
                callback_seen |= content.find(String{u8"菜单 / overview"}.bytes()) != std::string_view::npos;
            }
            check(callback_seen, "native Breadcrumb menu selection did not deliver its actual callback");
            check(pages.request(GalleryPageRoute{GalleryPageKind::introduction}),
                  "native Breadcrumb retirement route rejected");
            static_cast<void>(pages.process());
            render();
            check(!app.components().contains(owner) && app.services().breadcrumb().mounted().empty() && !captured(),
                  "native Breadcrumb retirement retained its root/focus capture");
            check(!inputs.editors().find(editor_session.owner) && !inputs.sessions().active().valid(),
                  "native Breadcrumb retirement retained editor/session");
            check(std::ranges::none_of(owned_popups, [&](auto popup) { return app.components().contains(popup); }),
                  "native Breadcrumb retirement retained popup owner");
            std::cout << "breadcrumb_native=passed keyed_topology=true retained_draft=true menu=true "
                         "menu_callback=true rtl=true retired=true\n";
        }
        if (route.component_identity == "ant.component.menu") {
            auto& menus = app.services().menu();
            const auto owner = pages.target("gallery.menu.dynamic");
            const auto run = [&](runtime::ComponentId target) {
                reveal(target);
                activate(target);
                render();
            };
            reveal(owner);
            const auto original = menus.snapshot(owner);
            // Eight application rows plus the retained horizontal overflow trigger.
            check(original.rows.size() == 9, "native dynamic Menu rows absent");
            check(original.overflow, "native horizontal Menu overflow absent");
            run(pages.target("gallery.menu.topology"));
            const auto changed = menus.snapshot(owner);
            check(changed.rows.size() == 10 && changed.rows.front() == original.rows[7],
                  "native Menu reorder/add lost retained rows");
            run(pages.target("gallery.menu.topology"));
            run(pages.target("gallery.menu.mode"));
            reveal(owner);
            menus.focus_menu(owner);
            const auto interaction = menus.snapshot(owner).interaction;
            key(interaction, input::Key::down);
            check(menus.snapshot(owner).active_key == String{u8"tools"}, "native Menu keyboard missed submenu");
            key(interaction, input::Key::left);
            check(menus.snapshot(owner).popups.size() == 1 && menus.snapshot(owner).active_key == String{u8"build"},
                  "native RTL Menu flyout did not open");
            key(interaction, input::Key::enter);
            check(std::ranges::any_of(app.services().text().mounted_texts(),
                                      [&](const auto& text) {
                                          return app.services()
                                                     .text()
                                                     .scene_service()
                                                     .text_state(text.scene)
                                                     .content()
                                                     .bytes()
                                                     .find(String{u8"导航命令：build"}.bytes()) !=
                                                 std::string_view::npos;
                                      }),
                  "native Menu flyout command callback missing");
            key(interaction, input::Key::escape);
            check(menus.snapshot(owner).popups.empty(), "native Menu flyout did not close");
            run(pages.target("gallery.menu.mode"));
            run(pages.target("gallery.menu.collapsed"));
            check(std::abs(bounds(owner).width - 80) < 1 && menus.snapshot(owner).rows == original.rows,
                  "native collapsed Menu changed retained rows or width");
            reveal(owner);
            menus.focus_menu(owner);
            key(interaction, input::Key::down);
            key(interaction, input::Key::left);
            check(!menus.snapshot(owner).popups.empty(), "native collapsed Menu flyout did not reopen");
            check(pages.request(GalleryPageRoute{GalleryPageKind::introduction}),
                  "native Menu retirement route rejected");
            static_cast<void>(pages.process());
            render();
            check(!app.components().contains(owner) && menus.mounted().empty() && !captured(),
                  "native Menu retirement retained rows/flyout/capture");
            std::cout
                << "menu_native=passed keyed_topology=true overflow=true rtl_flyout=true collapsed=true retired=true\n";
        }
        if (route.component_identity == "ant.component.dropdown") {
            auto& dropdowns = app.services().dropdown();
            auto& menus = app.services().menu();
            const auto run = [&](runtime::ComponentId target) {
                reveal(target);
                activate(target);
                render();
            };
            const auto press = [&](input::Key value) {
                pages.dispatch(input::KeyboardInputEvent{value, input::KeyAction::down});
                pages.dispatch(input::KeyboardInputEvent{value, input::KeyAction::up});
                render();
            };
            const auto feedback = [&](std::string_view expected) {
                return std::ranges::any_of(app.services().text().mounted_texts(), [&](const auto& text) {
                    return app.services()
                               .text()
                               .scene_service()
                               .text_state(text.scene)
                               .content()
                               .bytes()
                               .find(expected) != std::string_view::npos;
                });
            };
            const auto controlled = pages.target("gallery.dropdown.actions");
            run(controlled);
            check(dropdowns.snapshot(controlled).geometry.visible, "native controlled Dropdown did not open");
            press(input::Key::enter);
            check(!dropdowns.snapshot(controlled).geometry.visible && feedback(String{u8"已执行命令：refresh"}.bytes()),
                  "native controlled Dropdown selection did not callback/close");
            const auto split = pages.target("gallery.dropdown.split");
            run(split);
            check(feedback(String{u8"分离按钮：已运行默认操作"}.bytes()), "native split primary callback missing");
            runtime::ComponentId split_dropdown;
            for (auto id : dropdowns.mounted()) {
                if (owns(app, id, split)) {
                    split_dropdown = id;
                }
            }
            check(split_dropdown.valid(), "native split Dropdown missing");
            const auto trigger = dropdowns.snapshot(split_dropdown).trigger;
            reveal(trigger);
            const auto r = bounds(trigger);
            pointer(input::PointerAction::move, {r.x + r.width / 2, r.y + r.height / 2});
            const auto hover_deadline = std::min(deadline, std::chrono::steady_clock::now() + std::chrono::seconds(2));
            do {
                render();
            } while (!dropdowns.snapshot(split_dropdown).geometry.visible &&
                     std::chrono::steady_clock::now() < hover_deadline);
            const auto opened = dropdowns.snapshot(split_dropdown);
            check(opened.geometry.visible && opened.arrow_visible, "native split hover/arrow did not open");
            menus.focus_menu(opened.menu);
            render();
            press(input::Key::down);
            press(input::Key::right);
            check(menus.snapshot(opened.menu).popups.size() == 1, "native Dropdown Menu flyout did not open");
            press(input::Key::enter);
            check(feedback(String{u8"分离菜单：logs"}.bytes()), "native split flyout callback missing");
            press(input::Key::escape);
            press(input::Key::escape);
            check(!dropdowns.snapshot(split_dropdown).geometry.visible, "native split Dropdown did not close");
            const auto custom = pages.target("gallery.dropdown.custom");
            run(custom);
            const auto popup = dropdowns.snapshot(custom).popup;
            check(dropdowns.snapshot(custom).geometry.visible, "native custom Dropdown did not open");
            const auto draft = edit_input(custom);
            const auto session = inputs.sessions().active();
            press(input::Key::escape);
            check(!dropdowns.snapshot(custom).geometry.visible && !inputs.sessions().active().valid(),
                  "native custom Dropdown close retained input session");
            run(custom);
            check(dropdowns.snapshot(custom).popup == popup &&
                      std::string{inputs.display_snapshot(draft.first).text} == draft.second,
                  "native custom Dropdown reopen lost retained popup/draft");
            check(pages.request(GalleryPageRoute{GalleryPageKind::introduction}),
                  "native Dropdown retirement route rejected");
            static_cast<void>(pages.process());
            render();
            check(dropdowns.mounted().empty() && menus.mounted().empty() && !app.components().contains(popup) &&
                      !inputs.editors().find(session.owner) && !inputs.sessions().active().valid() && !captured(),
                  "native Dropdown retirement retained popup/editor/session/capture");
            std::cout << "dropdown_native=passed controlled=true hover=true split=true flyout=true retained_draft=true "
                         "retired=true\n";
        }
        if (route.component_identity == "ant.component.date-picker" ||
            route.component_identity == "ant.component.time-picker") {
            auto& dates = app.services().date_picker();
            auto& times = app.services().time_picker();
            const auto run = [&](runtime::ComponentId target) {
                reveal(target);
                activate(target);
                render();
            };
            const auto labelled = [&](runtime::ComponentId owner, String label) {
                for (const auto& text : app.services().text().mounted_texts()) {
                    if (!owns(app, text.component, owner) ||
                        app.services().text().scene_service().text_state(text.scene).content().bytes() !=
                            label.bytes()) {
                        continue;
                    }
                    for (const auto& button : app.mounted_buttons()) {
                        if (owns(app, text.component, button.component) &&
                            app.interactions().branch_active(button.interaction)) {
                            return button.component;
                        }
                    }
                }
                throw std::runtime_error("native picker action label is missing");
            };
            const auto focus_selector = [&](runtime::ComponentId component) {
                const auto editor = std::ranges::find_if(
                    inputs.mounted_inputs(), [&](const auto& input) { return input.component == component; });
                check(editor != inputs.mounted_inputs().end(), "native picker selector missing");
                check(app.focus().state().focused == editor->interaction ||
                          app.focus().request_focus(editor->interaction, input::FocusModality::keyboard),
                      "native picker selector could not receive focus");
            };
            const auto date_open = [&](runtime::ComponentId owner) {
                std::cout << "picker_native_open=date mode=" << static_cast<int>(dates.snapshot(owner).mode) << '\n'
                          << std::flush;
                reveal(owner);
                focus_selector(dates.snapshot(owner).input);
                press(input::Key::down);
                check(dates.snapshot(owner).visible, "native DatePicker did not open");
            };
            const auto time_open = [&](runtime::ComponentId owner) {
                std::cout << "picker_native_open=time\n" << std::flush;
                reveal(owner);
                focus_selector(times.snapshot(owner).selector);
                press(input::Key::down);
                check(times.snapshot(owner).visible, "native TimePicker did not open");
            };
            const auto range_dates = [&](runtime::ComponentId owner) {
                std::vector<runtime::ComponentId> result;
                for (auto id : dates.mounted()) {
                    if (owns(app, id, owner)) {
                        result.push_back(id);
                    }
                }
                check(result.size() == 2, "native date range endpoints missing");
                return result;
            };
            const auto range_times = [&](runtime::ComponentId owner) {
                std::vector<runtime::ComponentId> result;
                for (auto id : times.mounted()) {
                    if (owns(app, id, owner)) {
                        result.push_back(id);
                    }
                }
                check(result.size() == 2, "native time range endpoints missing");
                return result;
            };
            if (route.component_identity == "ant.component.date-picker") {
                check(dates.mounted().size() == 9 && times.mounted().size() == 3, "native DatePicker examples missing");
                const auto modes = pages.target("gallery.date-picker.modes");
                date_open(modes);
                const auto original = dates.snapshot(modes);
                check(original.mode == DatePickerMode::Date && original.cells.size() == 42, "native day grid missing");
                const auto cell_label = [&](runtime::ComponentId owner) {
                    std::string label;
                    for (const auto& text : app.services().text().mounted_texts()) {
                        if (owns(app, text.component, owner)) {
                            label += app.services().text().scene_service().text_state(text.scene).content().bytes();
                        }
                    }
                    return label;
                };
                const auto sunday = cell_label(original.cells[0]);
                const auto note = replace_text(pages.target("gallery.date-picker.mode-note"), u8"saved date note");
                press(input::Key::escape);
                run(pages.target("gallery.date-picker.locale"));
                run(pages.target("gallery.date-picker.rtl"));
                date_open(modes);
                auto mode = dates.snapshot(modes);
                check(cell_label(mode.cells[0]) != sunday && bounds(mode.cells[0]).x > bounds(mode.cells[1]).x &&
                          inputs.display_snapshot(note.component).text == "saved date note",
                      "native date RTL or retained footer failed");
                for (const auto expected : {DatePickerMode::Week, DatePickerMode::Month, DatePickerMode::Quarter,
                                            DatePickerMode::Year, DatePickerMode::Date}) {
                    press(input::Key::escape);
                    run(pages.target("gallery.date-picker.picker-mode"));
                    date_open(modes);
                    mode = dates.snapshot(modes);
                    const auto cells = expected == DatePickerMode::Quarter                                     ? 4U
                                       : expected == DatePickerMode::Month || expected == DatePickerMode::Year ? 12U
                                                                                                               : 42U;
                    check(mode.mode == expected && mode.cells.size() == cells && mode.input == original.input,
                          "native picker mode did not update its retained real grid");
                }
                activate(labelled(modes, u8"发布日"));
                render();
                check(dates.snapshot(modes).value == DatePickerValue{CivilDate{2026, 12, 1}},
                      "native date preset callback failed");
                date_open(modes);
                activate(labelled(modes, u8"Today"));
                render();
                check(dates.snapshot(modes).value == DatePickerValue{CivilDate{2026, 11, 5}},
                      "native explicit date clock failed");

                run(pages.target("gallery.date-picker.range-authority"));
                const auto multiple = pages.target("gallery.date-picker.multiple");
                const auto accepted = dates.snapshot(multiple).multiple_value;
                date_open(multiple);
                click(dates.snapshot(multiple).cells[15]);
                check(dates.snapshot(multiple).pending_multiple != accepted &&
                          dates.snapshot(multiple).multiple_value == accepted,
                      "native multiple pending value escaped authority");
                activate(labelled(multiple, u8"OK"));
                render();
                check(dates.snapshot(multiple).multiple_value == accepted, "native multiple refusal changed accepted");
                run(pages.target("gallery.date-picker.range-authority"));
                date_open(multiple);
                click(dates.snapshot(multiple).cells[15]);
                activate(labelled(multiple, u8"OK"));
                render();
                check(dates.snapshot(multiple).multiple_value != accepted, "native multiple accepted callback failed");

                const auto range = range_dates(pages.target("gallery.date-picker.range"));
                date_open(range[0]);
                replace_text(dates.snapshot(range[0]).input, u8"12/11/2026");
                press(input::Key::enter);
                check(dates.snapshot(range[1]).visible, "native date range did not activate end");
                replace_text(dates.snapshot(range[1]).input, u8"14/11/2026");
                press(input::Key::enter);
                check(dates.snapshot(range[0]).value == DatePickerValue{CivilDate{2026, 11, 12}} &&
                          dates.snapshot(range[1]).value == DatePickerValue{CivilDate{2026, 11, 14}},
                      "native typed date range did not publish atomically");
                const auto datetime = pages.target("gallery.date-picker.datetime");
                date_open(datetime);
                activate(labelled(datetime, u8"日期时间预设"));
                render();
                check(dates.snapshot(datetime).date_time_value ==
                          DatePickerDateTimeValue{CivilDateTime{CivilDate{2026, 11, 5}, TimeOfDay{12, 0, 0, 125}}},
                      "native date/time preset lost integer milliseconds");
                const auto datetime_range = range_dates(pages.target("gallery.date-picker.datetime-range"));
                const auto old_end = dates.snapshot(datetime_range[1]).date_time_value;
                date_open(datetime_range[1]);
                click(dates.snapshot(datetime_range[1]).cells[15]);
                activate(labelled(datetime_range[1], u8"OK"));
                render();
                check(dates.snapshot(datetime_range[1]).date_time_value != old_end &&
                          dates.snapshot(datetime_range[0]).date_time_value.has_value(),
                      "native date/time range confirmation failed");

                const auto form_picker = dates.mounted()[1];
                const auto form_draft = dates.snapshot(form_picker).draft;
                date_open(form_picker);
                replace_text(dates.snapshot(form_picker).input, u8"unfinished date");
                run(pages.target("gallery.date-picker.reset"));
                check(dates.snapshot(form_picker).draft == form_draft,
                      "native Form String date reset did not restore accepted draft");
                date_open(modes);
                activate(pages.target("gallery.date-picker.child-trigger"));
                render();
                const auto child = replace_text(pages.target("gallery.date-picker.child-editor"), u8"saved child note");
                const auto child_popup = *std::ranges::find_if(app.services().popover().mounted(), [&](auto id) {
                    return owns(app, child.component, app.services().popover().snapshot(id).popup);
                });
                const auto stamp = inputs.sessions().active();
                pages.dispatch(input::CompositionChanged{String{u8"日期候选"}, {}, stamp});
                render();
                press(input::Key::escape);
                check(!inputs.editors().require(stamp.owner).composition().active &&
                          app.services().popover().snapshot(child_popup).geometry.visible &&
                          dates.snapshot(modes).visible,
                      "native date preedit Escape closed its popup");
                press(input::Key::escape);
                check(!app.services().popover().snapshot(child_popup).geometry.visible && dates.snapshot(modes).visible,
                      "native date child popup Escape closed its parent");
                activate(pages.target("gallery.date-picker.child-trigger"));
                render();
                focus_editor(child.component);
                previous_session = inputs.sessions().active();
                pages.dispatch(input::CompositionChanged{String{u8"退场候选"}, {}, previous_session});
                render();
                std::cout << "date_picker_native=passed modes=5 locale=true rtl=true multiple_authority=true "
                             "date_range=true datetime_range=true presets_clock=true form_reset=true "
                             "retained_footer=true child_popup_escape=true\n";
            } else {
                check(times.mounted().size() == 5, "native TimePicker examples missing");
                const auto precise = pages.target("gallery.time-picker.precise");
                time_open(precise);
                auto state = times.snapshot(precise);
                check(state.rows[4].size() == 2 && state.column_clips[4].width > 0 && state.column_clips[3].width > 0 &&
                          state.pending.hour() == 12,
                      "native AM/PM or millisecond columns missing");
                const auto note = replace_text(pages.target("gallery.time-picker.precise-note"), u8"saved time note");
                click(state.rows[4][0]);
                check(times.snapshot(precise).pending.hour() == 0 && times.snapshot(precise).value->hour() == 12,
                      "native AM row lost pending/accepted distinction");
                activate(times.snapshot(precise).confirm);
                render();
                check(times.snapshot(precise).value->hour() == 0, "native AM confirmation failed");
                time_open(precise);
                focus_editor(times.snapshot(precise).selector);
                for (int step = 0; step < 5 && times.snapshot(precise).active_column != 3; ++step) {
                    press(times.snapshot(precise).active_column > 3 ? input::Key::left : input::Key::right);
                }
                press(input::Key::down);
                check(times.snapshot(precise).pending.millisecond() == 250, "native millisecond keyboard step failed");
                activate(times.snapshot(precise).confirm);
                render();
                check(times.snapshot(precise).value->millisecond() == 250, "native millisecond commit failed");
                time_open(precise);
                activate(labelled(precise, u8"Now"));
                render();
                check(times.snapshot(precise).value == TimePickerValue{TimeOfDay{9, 30, 0, 125}},
                      "native explicit Now clock failed");
                time_open(precise);
                activate(labelled(precise, u8"正午"));
                render();
                check(times.snapshot(precise).value == TimePickerValue{TimeOfDay{12}}, "native time preset failed");
                run(pages.target("gallery.time-picker.scroll-select"));
                time_open(precise);
                state = times.snapshot(precise);
                const auto clip = state.column_clips[0];
                events.inject_scroll_at(clip.x + 4, clip.y + 4, -1);
                render();
                check(times.snapshot(precise).pending != state.pending && times.snapshot(precise).value == state.value,
                      "native changeOnScroll failed or bypassed confirmation");
                activate(times.snapshot(precise).confirm);
                render();
                check(times.snapshot(precise).value != state.value, "native scroll selection did not publish");
                run(pages.target("gallery.time-picker.twelve"));
                time_open(precise);
                check(times.snapshot(precise).column_clips[4].width == 0 &&
                          inputs.display_snapshot(note.component).text == "saved time note",
                      "native 24-hour mode or retained precise footer failed");
                press(input::Key::escape);

                const auto range = range_times(pages.target("gallery.time-picker.range"));
                run(pages.target("gallery.time-picker.range-authority"));
                const auto set_range = [&] {
                    time_open(range[0]);
                    replace_text(times.snapshot(range[0]).selector, format_time_of_day(TimeOfDay{10}, "h:mm a"));
                    press(input::Key::enter);
                    check(times.snapshot(range[1]).visible, "native time range did not activate end");
                    replace_text(times.snapshot(range[1]).selector, format_time_of_day(TimeOfDay{19}, "h:mm a"));
                    press(input::Key::enter);
                };
                set_range();
                check(times.snapshot(range[0]).value == TimePickerValue{TimeOfDay{9}} &&
                          times.snapshot(range[1]).value == TimePickerValue{TimeOfDay{18}},
                      "native rejected time range changed accepted values");
                run(pages.target("gallery.time-picker.range-authority"));
                set_range();
                check(times.snapshot(range[0]).value == TimePickerValue{TimeOfDay{10}} &&
                          times.snapshot(range[1]).value == TimePickerValue{TimeOfDay{19}},
                      "native accepted time range did not publish atomically");
                const auto form_picker = times.mounted()[1];
                const auto form_draft = times.snapshot(form_picker).draft;
                time_open(form_picker);
                replace_text(times.snapshot(form_picker).selector, u8"unfinished time");
                run(pages.target("gallery.time-picker.reset"));
                check(times.snapshot(form_picker).draft == form_draft, "native Form String time reset failed");
                time_open(precise);
                focus_editor(note.component);
                previous_session = inputs.sessions().active();
                pages.dispatch(input::CompositionChanged{String{u8"时间候选"}, {}, previous_session});
                render();
                std::cout << "time_picker_native=passed am_pm=true milliseconds=true presets_now=true "
                             "change_on_scroll=true range_authority=true form_reset=true retained_footer=true\n";
            }
            check(inputs.editors().require(previous_session.owner).composition().active,
                  "native picker retirement has no active Core preedit");
            check(pages.request(GalleryPageRoute{GalleryPageKind::introduction}), "native picker retirement rejected");
            static_cast<void>(pages.process());
            render();
            check(dates.mounted().empty() && times.mounted().empty() &&
                      !inputs.editors().find(previous_session.owner) && !inputs.sessions().active().valid() &&
                      !captured(),
                  "native picker retirement retained editor/popup/session/capture");
        }
        if (route.component_identity == "ant.component.cascader") {
            auto& cascader = app.services().cascader();
            check(cascader.mounted().size() == 7, "native Cascader examples absent");
            const auto selector_id = pages.target("gallery.cascader.selector");
            reveal(selector_id);
            activate(cascader.snapshot(selector_id).selector);
            render();
            activate(pages.target("gallery.cascader.child-trigger"));
            render();
            const auto child_editor = replace_text(pages.target("gallery.cascader.child-editor"), u8"child draft");
            const auto child_stamp = inputs.sessions().active();
            pages.dispatch(input::CompositionChanged{String{u8"子候选"}, {}, child_stamp});
            render();
            check(inputs.editors().require(child_stamp.owner).composition().active,
                  "native Cascader child popup has no preedit");
            const auto child_popup = *std::ranges::find_if(app.services().popover().mounted(), [&](auto id) {
                return owns(app, pages.target("gallery.cascader.child-trigger"),
                            app.services().popover().snapshot(id).trigger);
            });
            const auto escape = [&] {
                pages.dispatch(input::KeyboardInputEvent{input::Key::escape, input::KeyAction::down});
                pages.dispatch(input::KeyboardInputEvent{input::Key::escape, input::KeyAction::up});
                render();
            };
            escape();
            check(!inputs.editors().require(child_stamp.owner).composition().active &&
                      app.services().popover().snapshot(child_popup).geometry.visible &&
                      cascader.snapshot(selector_id).open,
                  "native child preedit Escape closed an outer popup");
            escape();
            check(!app.services().popover().snapshot(child_popup).geometry.visible &&
                      cascader.snapshot(selector_id).open &&
                      inputs.display_snapshot(child_editor.component).text == "child draft",
                  "native child popup Escape lost ownership or retained draft");
            escape();
            const auto tags_id = cascader.mounted()[3];
            reveal(tags_id);
            render();
            auto tags = cascader.snapshot(tags_id);
            const auto shown = std::ranges::find_if(tags.tags, [](const auto& tag) { return tag.shown; });
            if (shown == tags.tags.end()) {
                std::cerr << "cascader_tag_measure selector=" << bounds(tags.selector).width
                          << " values=" << tags.value.size() << " tags=" << tags.tags.size();
                for (const auto& tag : tags.tags) {
                    const auto box = bounds(tag.component);
                    std::cerr << " tag=" << box.width << ',' << box.height << ',' << tag.shown;
                }
                std::cerr << '\n';
            }
            check(shown != tags.tags.end(), "native Cascader has no visible selected tag");
            check(std::ranges::any_of(tags.tags, [](const auto& tag) { return !tag.shown; }),
                  "native responsive Cascader tags have no overflow");
            activate(tags.overflow);
            render();
            check(std::ranges::any_of(app.services().popover().mounted(),
                                      [&](auto id) {
                                          return owns(app, id, tags_id) &&
                                                 app.services().popover().snapshot(id).geometry.visible;
                                      }) &&
                      !cascader.snapshot(tags_id).open,
                  "native responsive overflow did not open an independent popup");
            escape();
            activate(shown->remove);
            render();
            check(cascader.snapshot(tags_id).tags.size() == 2 && !cascader.snapshot(tags_id).open,
                  "native tag removal/refusal/open boundary");
            const auto lazy_id = cascader.mounted()[2];
            reveal(lazy_id);
            activate(cascader.snapshot(lazy_id).selector);
            render();
            activate(cascader.snapshot(lazy_id).rows[0].component);
            render();
            check(!cascader.snapshot(lazy_id).error.empty(), "native Cascader local error status absent");
            activate(cascader.snapshot(lazy_id).retry);
            render();
            check(cascader.snapshot(lazy_id).error.empty() && cascader.snapshot(lazy_id).rows.size() == 2,
                  "native Cascader Retry did not complete local children");
            pages.dispatch(input::KeyboardInputEvent{input::Key::escape, input::KeyAction::down});
            pages.dispatch(input::KeyboardInputEvent{input::Key::escape, input::KeyAction::up});
            render();
            const auto panel_id = cascader.mounted()[4];
            reveal(panel_id);
            render();
            auto panel = cascader.snapshot(panel_id);
            const auto editor = std::ranges::find_if(
                inputs.mounted_inputs(), [&](const auto& input) { return input.component == panel.search; });
            check(editor != inputs.mounted_inputs().end(), "native Cascader search editor absent");
            check(app.focus().request_focus(editor->interaction, input::FocusModality::keyboard),
                  "native Cascader search focus");
            pages.dispatch(input::TextCommitted{String{u8"STRASSE"}, inputs.sessions().active()});
            render();
            panel = cascader.snapshot(panel_id);
            check(std::ranges::count_if(panel.rows, [](const auto& r) { return r.shown; }) == 30,
                  "native full Unicode Cascader search");
            const auto r = bounds(panel.rows[0].component);
            events.inject_scroll_at(r.x + 8, r.y + 8, -.125F);
            render();
            check(cascader.snapshot(panel_id).scroll[0] > 0, "native Cascader input-first fractional wheel");
            activate(cascader.snapshot(panel_id).rows[0].component);
            pages.dispatch(input::KeyboardInputEvent{input::Key::end, input::KeyAction::down});
            pages.dispatch(input::KeyboardInputEvent{input::Key::end, input::KeyAction::up});
            render();
            panel = cascader.snapshot(panel_id);
            const auto last_row = bounds(panel.rows.back().component);
            const auto viewport = bounds(panel.rows.back().viewport);
            check(panel.scroll[0] > 100 && last_row.y >= viewport.y &&
                      last_row.y + last_row.height <= viewport.y + viewport.height + .01F,
                  "native Cascader End did not reveal the focused last row");
            check(app.focus().request_focus(editor->interaction, input::FocusModality::keyboard),
                  "native Cascader search refocus");
            const auto stamp = inputs.sessions().active();
            pages.dispatch(input::CompositionChanged{String{u8"候选"}, {}, stamp});
            render();
            check(inputs.editors().require(stamp.owner).composition().active, "native Cascader Core preedit absent");
            check(pages.request(GalleryPageRoute{GalleryPageKind::introduction}),
                  "native Cascader retirement route rejected");
            static_cast<void>(pages.process());
            render();
            check(cascader.mounted().empty() && !inputs.editors().find(stamp.owner) &&
                      !inputs.sessions().active().valid() && !captured(),
                  "native Cascader retirement retained editor/session/capture");
            std::cout << "cascader_native=passed tags=true loader_error_retry=true theme=true unicode_search=true "
                         "responsive_overflow=true child_popup=true focus_reveal=true fractional_scroll=true "
                         "core_preedit=true retired=true\n";
        }
        if (route.component_identity == "ant.component.pagination") {
            auto& paging = app.services().pagination();
            const auto owner = pages.target("gallery.pagination.advanced");
            const auto run = [&](runtime::ComponentId target) {
                reveal(target);
                activate(target);
                render();
            };
            const auto press = [&](input::Key value) {
                pages.dispatch(input::KeyboardInputEvent{value, input::KeyAction::down});
                pages.dispatch(input::KeyboardInputEvent{value, input::KeyAction::up});
                render();
            };
            const auto enter_page = [&](runtime::ComponentId target, String text) {
                reveal(target);
                for (const auto& input : inputs.mounted_inputs()) {
                    if (owns(app, input.component, target) && app.interactions().branch_active(input.interaction)) {
                        check(app.focus().state().focused == input.interaction ||
                                  app.focus().request_focus(input.interaction, input::FocusModality::keyboard),
                              "native Pagination editor focus failed");
                        pages.dispatch(input::KeyboardInputEvent{input::Key::a, input::KeyAction::down,
                                                                 input::KeyModifier::control});
                        pages.dispatch(input::KeyboardInputEvent{input::Key::a, input::KeyAction::up,
                                                                 input::KeyModifier::control});
                        pages.dispatch(input::TextCommitted{std::move(text), inputs.sessions().active()});
                        press(input::Key::enter);
                        return;
                    }
                }
                throw std::runtime_error("native Pagination editor absent");
            };
            resize(1000);
            render();
            reveal(owner);
            check(paging.snapshot(owner).page_size == 20, "native Pagination initial size wrong");
            run(paging.snapshot(owner).size_changer);
            press(input::Key::down);
            press(input::Key::enter);
            check(paging.snapshot(owner).page_size == 50 && paging.snapshot(owner).pages == 5,
                  "native Pagination size changer callback failed");
            enter_page(paging.snapshot(owner).jumper, u8"3");
            check(paging.snapshot(owner).current == 3, "native Pagination jumper callback failed");
            check(std::ranges::any_of(
                      app.services().text().mounted_texts(),
                      [&](const auto& text) {
                          return app.services().text().scene_service().text_state(text.scene).content().bytes() ==
                                 String{u8"分页 / 3 / 50"}.bytes();
                      }),
                  "native Pagination public onChange feedback missing");
            const auto simple = pages.target("gallery.pagination.simple");
            enter_page(paging.snapshot(simple).jumper, u8"4");
            check(paging.snapshot(simple).current == 4 && paging.snapshot(simple).simple,
                  "native Pagination simple editor failed");
            resize(360);
            render();
            check(paging.snapshot(owner).compact, "native Pagination responsive compact mode absent");
            check(pages.request(GalleryPageRoute{GalleryPageKind::introduction}),
                  "native Pagination retirement rejected");
            static_cast<void>(pages.process());
            render();
            check(paging.mounted().empty() && !inputs.sessions().active().valid(),
                  "native Pagination retirement leaked input");
            std::cout << "pagination_native=passed size_callback=true jumper_callback=true simple=true retired=true\n";
        }
        if (route.component_identity == "ant.component.steps") {
            auto& steps = app.services().steps();
            const auto owner = pages.target("gallery.steps.dynamic");
            const auto run = [&](runtime::ComponentId target) {
                reveal(target);
                activate(target);
                render();
            };
            const auto before = steps.snapshot(owner);
            reveal(before.items[0]);
            const auto draft = edit_input(before.items[0]);
            run(pages.target("gallery.steps.reorder"));
            const auto reordered = steps.snapshot(owner);
            check(reordered.keys.front() == String{u8"two"} && reordered.items.back() == before.items.front() &&
                      std::string{inputs.display_snapshot(draft.first).text} == draft.second,
                  "native Steps reorder lost retained editor draft");
            run(reordered.items[2]);
            check(steps.snapshot(owner).current == 2, "native Steps dynamic selection callback failed");
            check(std::ranges::any_of(app.services().text().mounted_texts(),
                                      [&](const auto& text) {
                                          return app.services()
                                                     .text()
                                                     .scene_service()
                                                     .text_state(text.scene)
                                                     .content()
                                                     .bytes()
                                                     .find(String{u8"动态步骤 / 2"}.bytes()) != std::string_view::npos;
                                      }),
                  "native Steps public onChange feedback missing");
            for (const auto name :
                 {"gallery.steps.dot", "gallery.steps.inline", "gallery.steps.navigation", "gallery.steps.panel"}) {
                const auto variant = pages.target(name);
                const auto index = std::string_view{name} == "gallery.steps.dot" ? 1U : 0U;
                const auto row = steps.snapshot(variant).items[index];
                run(row);
                check(steps.snapshot(variant).current == index, "native Steps variant selection failed");
                if (std::string_view{name} == "gallery.steps.inline") {
                    const auto detail_draft = edit_input(row);
                    pages.dispatch(input::KeyboardInputEvent{input::Key::escape, input::KeyAction::down});
                    pages.dispatch(input::KeyboardInputEvent{input::Key::escape, input::KeyAction::up});
                    render();
                    check(!app.components().branch_active(detail_draft.first) && !inputs.sessions().active().valid(),
                          "native Inline Steps Escape retained description/editor session");
                }
                const auto motion_deadline =
                    std::min(deadline, std::chrono::steady_clock::now() + std::chrono::seconds(2));
                while (steps.snapshot(variant).motion < 1 && std::chrono::steady_clock::now() < motion_deadline) {
                    render();
                }
                check(steps.snapshot(variant).motion == 1, "native Steps animation did not finish");
            }
            check(pages.request(GalleryPageRoute{GalleryPageKind::introduction}), "native Steps retirement rejected");
            static_cast<void>(pages.process());
            render();
            check(steps.mounted().empty() && !app.components().contains(draft.first) &&
                      !inputs.sessions().active().valid() && app.services().animations().diagnostics().active == 0,
                  "native Steps retirement leaked editor/motion");
            std::cout << "steps_native=passed variants=true keyed_draft=true inline_boundary=true finite_motion=true "
                         "retired=true\n";
        }
        if (route.component_identity == "ant.component.tabs") {
            auto& tabs = app.services().tabs();
            const auto owner = pages.target("gallery.tabs.editable");
            resize(1000);
            render();
            const auto run = [&](runtime::ComponentId target) {
                reveal(target);
                activate(target);
                render();
            };
            const auto press = [&](input::Key value) {
                pages.dispatch(input::KeyboardInputEvent{value, input::KeyAction::down});
                pages.dispatch(input::KeyboardInputEvent{value, input::KeyAction::up});
                render();
            };
            reveal(owner);
            const auto original = tabs.snapshot(owner);
            check(original.headers.size() == 2 && original.rendered[0] && !original.rendered[1],
                  "native editable Tabs lazy contract absent");
            const auto draft = edit_input(original.panels[0]);
            run(original.headers[1]);
            check(tabs.snapshot(owner).key == String{u8"details"} && tabs.snapshot(owner).rendered[1],
                  "native Tabs lazy panel activation failed");
            const auto retired = edit_input(tabs.snapshot(owner).panels[1]);
            run(original.headers[0]);
            check(!tabs.snapshot(owner).rendered[1] && !app.components().contains(retired.first) &&
                      std::string{inputs.display_snapshot(draft.first).text} == draft.second,
                  "native Tabs destroyOnHidden lost retained draft or kept hidden editor");
            run(pages.target("gallery.tabs.reorder"));
            const auto reordered = tabs.snapshot(owner);
            check(reordered.headers.front() == original.headers.back() &&
                      reordered.headers.back() == original.headers.front() &&
                      std::string{inputs.display_snapshot(draft.first).text} == draft.second,
                  "native Tabs reorder remounted retained content");
            run(reordered.add);
            run(tabs.snapshot(owner).add);
            check(tabs.snapshot(owner).headers.size() == 4, "native Tabs onEdit add callback failed");
            const auto added = tabs.snapshot(owner).headers.back();
            run(added);
            press(input::Key::delete_forward);
            check(tabs.snapshot(owner).headers.size() == 3 && !app.components().contains(added),
                  "native Tabs onEdit remove callback failed");
            resize(480);
            render();
            reveal(owner);
            const auto narrow = tabs.snapshot(owner);
            check(narrow.overflow, "native Tabs narrow overflow absent");
            run(narrow.more);
            bool popup_open = false;
            for (auto id : app.services().dropdown().mounted()) {
                if (owns(app, id, narrow.more) && app.services().dropdown().snapshot(id).geometry.visible) {
                    popup_open = true;
                }
            }
            check(popup_open, "native Tabs overflow Dropdown did not open");
            press(input::Key::end);
            press(input::Key::enter);
            check(tabs.snapshot(owner).key != narrow.key, "native Tabs overflow Menu callback did not change panel");
            for (const auto name : {"gallery.tabs.left", "gallery.tabs.right"}) {
                const auto vertical = pages.target(name);
                run(tabs.snapshot(vertical).headers[1]);
                const auto selected = tabs.snapshot(vertical);
                check(selected.active == 1 && bounds(selected.headers[1]).y > bounds(selected.headers[0]).y,
                      "native Tabs vertical/RTL interaction failed");
            }
            const auto motion_deadline = std::min(deadline, std::chrono::steady_clock::now() + std::chrono::seconds(2));
            while (tabs.snapshot(owner).motion < 1 && std::chrono::steady_clock::now() < motion_deadline) {
                render();
            }
            check(tabs.snapshot(owner).motion == 1, "native Tabs animation did not finish");
            check(pages.request(GalleryPageRoute{GalleryPageKind::introduction}), "native Tabs retirement rejected");
            static_cast<void>(pages.process());
            render();
            check(tabs.mounted().empty() && app.services().dropdown().mounted().empty() &&
                      app.services().menu().mounted().empty() && !app.components().contains(draft.first) &&
                      !inputs.sessions().active().valid() && !captured() &&
                      app.services().animations().diagnostics().active == 0,
                  "native Tabs retirement retained popup/editor/motion");
            std::cout << "tabs_native=passed editable_callbacks=true keyed_draft=true lazy_destroy=true "
                         "overflow_menu=true vertical_rtl=true finite_motion=true retired=true\n";
        }
        if (route.component_identity == "ant.component.anchor") {
            const auto owner = pages.target("gallery.anchor.component");
            auto& anchors = app.services().anchor();
            const auto run = [&](runtime::ComponentId target) {
                reveal(target);
                activate(target);
                render();
            };
            const auto original = anchors.snapshot(owner);
            check(original.keys.size() == 5, "native Anchor has no complete hierarchy");
            app.services().set_motion_preference(animation::MotionPreference::reduced);
            run(original.items[2]);
            check(anchors.snapshot(owner).active == String{u8"api"},
                  "native Anchor target callback did not scroll/highlight");
            const auto& viewport_node =
                app.nodes().require(app.components().root(pages.target("gallery.anchor.viewport")));
            check(!viewport_node.children.empty() &&
                      std::abs(viewport_node.bounds.y - app.nodes().require(viewport_node.children.front()).bounds.y -
                               480) < .01F,
                  "native Anchor callback did not move its real scroll content");
            run(pages.target("gallery.anchor.direction"));
            check(!app.components().branch_active(original.items[1]), "native horizontal Anchor exposed nested item");
            run(pages.target("gallery.anchor.direction"));
            run(pages.target("gallery.anchor.topology"));
            check(anchors.snapshot(owner).keys.size() == 4, "native Anchor removal did not reconcile");
            run(pages.target("gallery.anchor.topology"));
            check(anchors.snapshot(owner).items[0] == original.items[0], "native Anchor remounted unchanged key");
            app.services().set_motion_preference(animation::MotionPreference::normal);
            std::cout << "anchor_native=passed target_callback=true hierarchy=true keyed_topology=true\n";
        }
        if (route.component_identity == "ant.component.masonry") {
            const auto owner = pages.target("gallery.masonry.measured");
            const auto dynamic = pages.target("gallery.masonry.dynamic");
            check(owner.valid() && dynamic.valid(), "native Masonry samples are missing");
            const auto draft = edit_input(owner);
            resize(1000);
            render();
            const auto wide = app.services().masonry().snapshot(owner);
            check(wide.size() == 6 && wide[2].column == 2, "native Masonry missed its three-column layout");
            const auto initial = app.services().masonry().snapshot(dynamic);
            check(initial.size() == 2, "native Masonry dynamic keys are missing");
            std::cout << "masonry_native_action=height\n" << std::flush;
            activate(pages.target("gallery.masonry.resize"));
            render();
            const auto taller = app.services().masonry().snapshot(dynamic);
            check(taller[0].bounds.height > initial[0].bounds.height && taller[0].component == initial[0].component,
                  "native Masonry height action did not remeasure retained content");
            std::cout << "masonry_native_action=insert\n" << std::flush;
            activate(pages.target("gallery.masonry.toggle"));
            render();
            std::cout << "masonry_native_action=reorder\n" << std::flush;
            activate(pages.target("gallery.masonry.reverse"));
            render();
            const auto reordered = app.services().masonry().snapshot(dynamic);
            check(reordered.size() == 3 && std::ranges::any_of(reordered,
                                                               [&](const auto& item) {
                                                                   return item.key == initial[0].key &&
                                                                          item.component == initial[0].component;
                                                               }),
                  "native Masonry insertion/reorder lost keyed identity");
            std::cout << "masonry_native_action=remove\n" << std::flush;
            activate(pages.target("gallery.masonry.toggle"));
            render();
            check(app.services().masonry().snapshot(dynamic).size() == 2, "native Masonry removal failed");
            resize(500);
            render();
            const auto narrow = app.services().masonry().snapshot(owner);
            check(narrow.size() == 6 && narrow[2].column == 0 && narrow[0].component == wide[0].component &&
                      inputs.display_snapshot(draft.first).text == draft.second,
                  "native Masonry narrow reflow lost a key or input draft");
            check(pages.sample_runs("gallery.sample.masonry.layouts") == 1,
                  "native Masonry reflow reran its sample factory");
            previous_session = inputs.sessions().active();
            std::cout << "masonry_native=passed actions=true window_breakpoints=true retained_draft=true\n";
        }
        if (route.component_identity == "ant.component.layout") {
            auto& shell = app.services().shell_layout();
            runtime::ComponentId responsive;
            runtime::ComponentId controlled;
            for (const auto id : shell.mounted()) {
                const auto value = shell.snapshot(id);
                if (value.role == ryn::detail::ShellLayoutRole::Sider) {
                    if (value.zero_width || std::abs(bounds(id).width - 200) < 1) {
                        responsive = id;
                    } else if (std::abs(bounds(id).width - 180) < 1 && value.trigger_visible) {
                        controlled = id;
                    }
                }
            }
            check(responsive.valid() && controlled.valid(), "native responsive/controlled Siders are missing");
            resize(1000);
            render();
            const auto wide = shell.snapshot(responsive);
            check(!wide.broken && !wide.collapsed && std::abs(bounds(responsive).width - 200) < 1,
                  "native Sider did not expand at the shared window lg breakpoint");
            const auto draft = edit_input(wide.body);
            resize(500);
            render();
            const auto narrow = shell.snapshot(responsive);
            check(narrow.broken && narrow.collapsed && narrow.zero_width && narrow.trigger_visible &&
                      bounds(responsive).width == 0 && narrow.body == wide.body,
                  "native zero-width Sider lost its breakpoint, trigger or retained body");
            check(!inputs.sessions().active().valid(), "native collapsed Sider retained its text session");
            key(narrow.interaction, input::Key::enter);
            check(!shell.snapshot(responsive).collapsed && inputs.display_snapshot(draft.first).text == draft.second,
                  "native zero-width trigger failed to expand or preserve the draft");
            key(narrow.interaction, input::Key::space);
            check(shell.snapshot(responsive).collapsed, "native Sider trigger failed to collapse");
            key(narrow.interaction, input::Key::enter);
            check(!shell.snapshot(responsive).collapsed && inputs.display_snapshot(draft.first).text == draft.second,
                  "native Sider trigger roundtrip lost its draft");
            key(shell.snapshot(controlled).interaction, input::Key::enter);
            check(shell.snapshot(controlled).collapsed && std::abs(bounds(controlled).width - 64) < 1,
                  "native controlled Sider request did not update its typed value");
            key(shell.snapshot(controlled).interaction, input::Key::enter);
            check(!shell.snapshot(controlled).collapsed && std::abs(bounds(controlled).width - 180) < 1,
                  "native controlled Sider did not restore its width");
            check(pages.sample_runs("gallery.sample.layout.responsive") == 1 &&
                      pages.sample_runs("gallery.sample.layout.controlled") == 1,
                  "native Sider updates reran their sample factories");
            static_cast<void>(edit_input(wide.body));
            previous_session = inputs.sessions().active();
            std::cout << "layout_native=passed window_breakpoints=true controlled=true zero_trigger=true "
                         "retained_draft=true\n";
        }
        if (route.component_identity == "ant.component.splitter") {
            auto& splitters = app.services().splitter();
            runtime::ComponentId multiple;
            runtime::ComponentId lazy;
            for (const auto id : splitters.mounted()) {
                const auto value = splitters.snapshot(id);
                if (value.panels.size() == 3) {
                    multiple = id;
                }
                if (value.lazy) {
                    lazy = id;
                }
            }
            check(multiple.valid() && lazy.valid(), "native multiple/lazy Splitters are missing");
            const auto initial = splitters.snapshot(multiple);
            const auto draft = edit_input(initial.panels[1].content);
            reveal(initial.bars[0].component);
            const auto seam = bounds(initial.bars[0].component);
            const runtime::Point start{seam.x + seam.width / 2, seam.y + 20};
            pointer(input::PointerAction::down, start);
            check(captured(), "native Splitter drag did not capture pointer");
            pointer(input::PointerAction::move, {start.x + 40, start.y});
            render();
            const auto moved = splitters.snapshot(multiple);
            check(moved.dragging && moved.sizes[0] > initial.sizes[0] && moved.sizes[2] == initial.sizes[2] &&
                      std::abs(moved.sizes[0] + moved.sizes[1] - initial.sizes[0] - initial.sizes[1]) < 1,
                  "native Splitter drag did not preserve adjacent total or unrelated panel");
            pointer(input::PointerAction::up, {start.x + 40, start.y});
            render();
            check(!captured() && !splitters.snapshot(multiple).dragging,
                  "native Splitter release retained pointer capture");
            const float total = std::accumulate(initial.sizes.begin(), initial.sizes.end(), 0.0F);
            key(initial.bars[0].interaction, input::Key::home);
            check(std::abs(splitters.snapshot(multiple).sizes[0] - total * .15F) < 1,
                  "native Splitter Home ignored minimum");
            key(initial.bars[0].interaction, input::Key::end);
            const float maximum = total * .40F;
            check(maximum < initial.sizes[0] + initial.sizes[1] - 96,
                  "native Splitter maximum is masked by neighbor minimum");
            const auto ended = splitters.snapshot(multiple);
            check(std::abs(ended.sizes[0] - maximum) < 1 && ended.sizes[2] == initial.sizes[2] &&
                      std::abs(ended.sizes[0] + ended.sizes[1] - initial.sizes[0] - initial.sizes[1]) < 1,
                  "native Splitter End ignored max/neighbor min or adjacent total");
            key(initial.bars[0].interaction, input::Key::left);
            check(splitters.snapshot(multiple).sizes[0] < ended.sizes[0] &&
                      inputs.display_snapshot(draft.first).text == draft.second,
                  "native Splitter arrow failed or discarded draft");
            const auto lazy_initial = splitters.snapshot(lazy);
            const auto lazy_draft = edit_input(lazy_initial.panels[0].content);
            const auto drag_lazy = [&] {
                reveal(splitters.snapshot(lazy).bars[0].component);
                const auto r = bounds(splitters.snapshot(lazy).bars[0].component);
                const runtime::Point p{r.x + r.width / 2, r.y + 20};
                pointer(input::PointerAction::down, p);
                check(captured(), "native lazy Splitter has no pointer capture");
                pointer(input::PointerAction::move, {p.x + 40, p.y});
                render();
                return runtime::Point{p.x + 40, p.y};
            };
            const auto cancelled_at = drag_lazy();
            const auto preview = splitters.snapshot(lazy);
            check(preview.dragging && preview.sizes == lazy_initial.sizes &&
                      preview.visual_sizes == lazy_initial.visual_sizes && preview.preview_sizes[0] > preview.sizes[0],
                  "native lazy Splitter committed its preview early");
            pointer(input::PointerAction::cancel, cancelled_at);
            render();
            check(!captured() && !splitters.snapshot(lazy).dragging &&
                      splitters.snapshot(lazy).sizes == lazy_initial.sizes,
                  "native lazy Splitter cancellation committed or retained capture");
            const auto released_at = drag_lazy();
            pointer(input::PointerAction::up, released_at);
            render();
            const auto committed = splitters.snapshot(lazy);
            check(!captured() && !committed.dragging && committed.sizes[0] > lazy_initial.sizes[0] &&
                      std::abs(committed.sizes[0] + committed.sizes[1] - lazy_initial.sizes[0] -
                               lazy_initial.sizes[1]) < 1,
                  "native lazy Splitter release failed to commit adjacent sizes");
            key(committed.bars[0].start_interaction, input::Key::enter);
            const auto collapsed = splitters.snapshot(lazy);
            check(collapsed.panels[0].collapsed && collapsed.sizes[0] == 0 &&
                      collapsed.panels[0].content == lazy_initial.panels[0].content &&
                      !inputs.sessions().active().valid(),
                  "native Splitter retained collapse lost content or kept a text session");
            key(collapsed.bars[0].end_interaction, input::Key::enter);
            check(!splitters.snapshot(lazy).panels[0].collapsed &&
                      inputs.display_snapshot(lazy_draft.first).text == lazy_draft.second &&
                      std::abs(splitters.snapshot(lazy).sizes[0] - committed.sizes[0]) < 1,
                  "native Splitter restore lost its size or draft");
            activate(pages.target("gallery.splitter.destroy-hidden"));
            render();
            const auto old_content = splitters.snapshot(lazy).panels[0].content;
            key(splitters.snapshot(lazy).bars[0].start_interaction, input::Key::enter);
            check(splitters.snapshot(lazy).panels[0].destroy_on_hidden && !app.components().contains(old_content) &&
                      !app.components().contains(lazy_draft.first),
                  "native Splitter destroyOnHidden retained its hidden content/editor");
            key(splitters.snapshot(lazy).bars[0].end_interaction, input::Key::enter);
            check(splitters.snapshot(lazy).panels[0].content.valid() &&
                      splitters.snapshot(lazy).panels[0].content != old_content,
                  "native Splitter destroyOnHidden did not rebuild on restore");
            check(pages.sample_runs("gallery.sample.splitter.lazy-collapse") == 1,
                  "native Splitter interactions reran their sample factory");
            // Retire the page with a live lazy gesture and text session to prove cleanup.
            static_cast<void>(edit_input(splitters.snapshot(lazy).panels[0].content));
            previous_session = inputs.sessions().active();
            static_cast<void>(drag_lazy());
            check(captured(), "native Splitter retirement test has no active capture");
            std::cout << "splitter_native=passed drag=true keyboard_constraints=true lazy_cancel_commit=true "
                         "collapse_destroy=true\n";
        }
        if (route.component_identity == "ant.component.float-button") {
            runtime::ComponentId floating;
            for (const auto id : app.services().float_button().mounted()) {
                const auto state = app.services().float_button().snapshot(id);
                if (owns(app, id, root) && app.components().in_window_layer(id) && !state.menu.valid() &&
                    !state.progress_view.valid()) {
                    floating = id;
                }
            }
            check(floating.valid(), "native FloatButton window layer is missing");
            const auto before = app.services().float_button().snapshot(floating).bounds;
            const auto& token = app.components().theme_scope(floating)->snapshot().float_button();
            check(std::abs(before.x + before.width + token.inset_inline - events.viewport.width) < 1 &&
                      std::abs(before.y + before.height + token.inset_block - events.viewport.height) < 1,
                  "native FloatButton did not anchor to the window viewport");
            const auto send = [&](input::PointerAction action, float x, float y) {
                pages.dispatch(input::PointerInputEvent{
                    input::PointerIdentity::mouse(), action,
                    action == input::PointerAction::move ? input::PointerButton::none : input::PointerButton::primary,
                    x, y});
            };
            send(input::PointerAction::down, before.x + before.width / 2, before.y + before.height / 2);
            check(app.pointer().state(input::PointerIdentity::mouse())->capture.has_value(),
                  "native FloatButton drag has no capture");
            send(input::PointerAction::move, before.x + before.width / 2 - 80, before.y + before.height / 2 - 40);
            render();
            const auto dragged = app.services().float_button().snapshot(floating);
            check(dragged.dragging && std::abs(dragged.bounds.x - before.x + 80) < 1 &&
                      std::abs(dragged.bounds.y - before.y + 40) < 1,
                  "native FloatButton drag did not update retained window geometry");
            send(input::PointerAction::up, dragged.bounds.x + dragged.bounds.width / 2,
                 dragged.bounds.y + dragged.bounds.height / 2);
            check(!app.pointer().state(input::PointerIdentity::mouse())->capture &&
                      !app.services().float_button().snapshot(floating).dragging,
                  "native FloatButton release retained capture");
            const auto lane = pages.snapshot().layout.document;
            events.inject_scroll_at(lane.x + lane.width - 8, lane.y + lane.height / 2, -2);
            render();
            check(app.services().float_button().snapshot(floating).bounds == dragged.bounds,
                  "native document scroll moved its FloatButton window layer");
            const auto image = read_frame("float-button-" + std::to_string(i));
            const auto button = std::ranges::find_if(
                app.mounted_buttons(), [&](const auto& candidate) { return owns(app, candidate.component, floating); });
            check(button != app.mounted_buttons().end(), "native FloatButton has no Button input owner");
            check_pixel(
                image.get(),
                {(dragged.bounds.x + dragged.bounds.width / 2) * events.scale, (dragged.bounds.y + 6) * events.scale},
                app.snapshot(button->component).presentation_background);
            check(app.focus().request_focus(button->interaction, input::FocusModality::keyboard),
                  "native FloatButton is not keyboard reachable");
            pages.dispatch(input::KeyboardInputEvent{input::Key::enter, input::KeyAction::down});
            pages.dispatch(input::KeyboardInputEvent{input::Key::enter, input::KeyAction::up});
            render();
            send(input::PointerAction::down, dragged.bounds.x + dragged.bounds.width / 2,
                 dragged.bounds.y + dragged.bounds.height / 2);
            std::cout << "float_button_native=passed anchor=true drag=true capture=true keyboard=true gpu_pixel=true\n";
        }
        if (route.component_identity == "ant.component.calendar") {
            auto& calendars = app.services().calendar();
            const auto owner = pages.target("gallery.calendar.main");
            reveal(owner);
            auto original = calendars.snapshot(owner);
            check(original.cells.size() == 42, "native Calendar month grid absent");
            const auto chosen = std::ranges::find_if(
                original.cells, [](const auto& cell) { return cell.date == CivilDate{2024, 2, 29}; });
            check(chosen != original.cells.end(), "native Calendar leap day missing");
            const auto cell = original.components[std::size_t(chosen - original.cells.begin())];
            reveal(cell);
            const auto draft = edit_input(cell);
            const auto stamp = inputs.sessions().active();
            pages.dispatch(input::CompositionChanged{String{u8"候选"}, {}, stamp});
            reveal(cell);
            activate(cell);
            render();
            check(!inputs.editors().require(stamp.owner).composition().active,
                  "Calendar date focus handoff retained preedit");
            press(input::Key::right);
            press(input::Key::enter);
            check(calendars.snapshot(owner).value == CivilDate{2024, 3, 1} && app.components().contains(draft.first) &&
                      inputs.display_snapshot(draft.first).text == draft.second,
                  "native Calendar keyboard/month overlap lost date or draft");
            const auto run = [&](std::string_view target) {
                reveal(pages.target(target));
                activate(pages.target(target));
                render();
            };
            const auto before_locale = calendars.snapshot(owner).cells.front().date;
            run("gallery.calendar.locale");
            check(calendars.snapshot(owner).cells.front().date != before_locale,
                  "native Calendar locale did not update grid");
            check(has_text(u8"source="), "native Calendar select callback was not delivered");
            run("gallery.calendar.range");
            check(std::ranges::any_of(calendars.snapshot(owner).cells, [](const auto& cell) { return cell.disabled; }),
                  "native Calendar range did not gate actual dates");
            const auto height = bounds(owner).height;
            run("gallery.calendar.mini");
            check(bounds(owner).height < height, "native Calendar mini metrics did not change layout");
            const auto header_action = [&](std::string_view label) {
                const auto header = calendars.snapshot(owner).header;
                for (const auto& button : app.mounted_buttons()) {
                    if (!owns(app, button.component, header)) {
                        continue;
                    }
                    for (const auto& text : app.services().text().mounted_texts()) {
                        if (owns(app, text.component, button.component) &&
                            app.services().text().scene_service().text_state(text.scene).content().bytes() == label) {
                            reveal(button.component);
                            activate(button.component);
                            render();
                            return;
                        }
                    }
                }
                throw std::runtime_error("native Calendar header action missing");
            };
            header_action("Year");
            check(calendars.snapshot(owner).mode == CalendarMode::Year && calendars.snapshot(owner).cells.size() == 12,
                  "native Calendar Year header did not select actual year grid");
            header_action("Month");
            header_action("Today");
            check(calendars.snapshot(owner).value == CivilDate{2024, 2, 29}, "native Calendar Today callback failed");
            const auto current = calendars.snapshot(owner);
            std::vector<runtime::ComponentId> selectors;
            for (const auto id : app.services().select().mounted()) {
                if (owns(app, id, current.header)) {
                    selectors.push_back(id);
                }
            }
            check(selectors.size() == 2, "native Calendar year/month controls absent");
            reveal(selectors.back());
            activate(selectors.back());
            render();
            press(input::Key::down);
            press(input::Key::enter);
            check(calendars.snapshot(owner).value.month() == 3, "native Calendar month Select did not deliver value");
            check(pages.sample_runs("gallery.sample.calendar.schedule") == 1 &&
                      pages.sample_runs("gallery.sample.calendar.custom") == 1,
                  "native Calendar interactions reran sample factories");
            previous_session = inputs.sessions().active();
            std::cout << "calendar_native=passed date_keyboard=true header_year_month_today=true locale_range=true "
                         "mini_metrics=true rich_draft_preedit=true callbacks=true\n";
        }
        if (route.component_identity == "ant.component.carousel") {
            auto& carousels = app.services().carousel();
            const auto owner = pages.target("gallery.carousel.main");
            reveal(owner);
            update_acceptance_theme(pages, [](ThemeConfig& config) { config.seed.motion = true; });
            render();
            const auto original = carousels.snapshot(owner);
            const auto draft = edit_input(original.slides[0]);
            const auto settle = [&] {
                for (int frame = 0; frame < 60 && carousels.snapshot(owner).transitioning; ++frame) {
                    render();
                }
                check(!carousels.snapshot(owner).transitioning, "native Carousel transition did not settle");
            };
            activate(original.arrows[1]);
            render();
            const auto middle = carousels.snapshot(owner);
            check(middle.current == 1 && middle.transitioning && middle.progress > 0 && middle.progress < 1,
                  "native Carousel arrow missed actual ScrollX midpoint");
            check(bounds(middle.slides[0]).x < middle.viewport.x && bounds(middle.slides[1]).x > middle.viewport.x &&
                      !inputs.sessions().active().valid(),
                  "native ScrollX midpoint paint/input cleanup failed");
            settle();
            check(has_text(u8"after 1"), "native Carousel afterChange callback was not delivered");
            activate(carousels.snapshot(owner).dots[0]);
            render();
            settle();
            check(inputs.display_snapshot(draft.first).text == draft.second &&
                      carousels.snapshot(owner).slides == original.slides,
                  "native Carousel roundtrip cloned/lost draft");
            const auto run = [&](std::string_view target) {
                reveal(pages.target(target));
                activate(pages.target(target));
                render();
            };
            run("gallery.carousel.effect");
            reveal(owner);
            activate(carousels.snapshot(owner).arrows[1]);
            render();
            const auto fade = carousels.snapshot(owner);
            const auto a = app.nodes().require(app.components().root(fade.slides[0]));
            const auto b = app.nodes().require(app.components().root(fade.slides[1]));
            check(fade.transitioning && a.bounds == b.bounds && a.paint_opacity > 0 && a.paint_opacity < 1 &&
                      b.paint_opacity > 0 && b.paint_opacity < 1,
                  "native Carousel did not paint real Fade midpoint");
            settle();
            reveal(owner);
            auto viewport = carousels.snapshot(owner).viewport;
            runtime::Point start{viewport.x + viewport.width * .8F, viewport.y + viewport.height - 4};
            pointer(input::PointerAction::down, start);
            pointer(input::PointerAction::move, {start.x - viewport.width / 3, start.y});
            render();
            check(captured() && carousels.snapshot(owner).dragging, "native Carousel drag did not capture");
            pointer(input::PointerAction::cancel, start);
            render();
            check(!captured() && !carousels.snapshot(owner).dragging && carousels.snapshot(owner).current == 1,
                  "native Carousel cancel changed current or retained capture");
            run("gallery.carousel.autoplay");
            pointer(input::PointerAction::move, {0, 0});
            app.focus().clear_focus();
            render();
            const auto before_auto = carousels.snapshot(owner).current;
            for (int frame = 0; frame < 100 && carousels.snapshot(owner).current == before_auto; ++frame) {
                render();
            }
            check(carousels.snapshot(owner).current != before_auto, "native Carousel autoplay deadline did not wake");
            settle();
            run("gallery.carousel.pause");
            check(carousels.snapshot(owner).paused, "native Carousel pause action failed");
            run("gallery.carousel.play");
            pointer(input::PointerAction::move, {0, 0});
            app.focus().clear_focus();
            render();
            check(!carousels.snapshot(owner).paused, "native Carousel play did not restore countdown");
            check(pages.sample_runs("gallery.sample.carousel.panels") == 1 &&
                      pages.sample_runs("gallery.sample.carousel.controlled") == 1,
                  "native Carousel actions reran sample factories");
            update_acceptance_theme(pages, [](ThemeConfig& config) { config.seed.motion = false; });
            check(pages.request(GalleryPageRoute{GalleryPageKind::introduction}), "Carousel retirement route failed");
            pages.process();
            render();
            check(carousels.mounted().empty() && !captured(), "native Carousel page retirement leaked capture/root");
            std::cout
                << "carousel_native=passed arrows_dots=true scroll_fade_midpoints=true draft=true drag_cancel=true "
                   "autoplay_pause_wake=true retirement=true\n";
        }
        if (route.component_identity == "ant.component.qr-code") {
            auto& codes = app.services().qrcode();
            const auto id = pages.target("gallery.qrcode.main");
            reveal(id);
            pointer(input::PointerAction::move, {0, 0});
            render();
            auto view = codes.snapshot(id);
            check(view.data.has_value() && !view.modules.empty(), "Native QRCode has no standard model/modules");
            const auto frame = read_frame("qrcode-native-" + std::to_string(i));
            const auto pitch = view.symbol.width / view.data->size();
            for (int y = 0; y < view.data->size(); ++y) {
                for (int x = 0; x < view.data->size(); ++x) {
                    const auto expected = view.data->module(x, y) ? view.paint.foreground : view.paint.background;
                    check_pixel(frame.get(),
                                {(view.symbol.x + (x + .5F) * pitch) * events.scale,
                                 (view.symbol.y + (y + .5F) * pitch) * events.scale},
                                expected);
                }
            }
            check_pixel(frame.get(),
                        {(view.symbol.x - 2 * pitch) * events.scale, (view.symbol.y + pitch / 2) * events.scale},
                        view.paint.background);
            const auto run = [&](std::string_view name) {
                reveal(pages.target(name));
                activate(pages.target(name));
                render();
            };
            run("gallery.qrcode.expire");
            view = codes.snapshot(id);
            check(view.status == QRCodeStatus::Expired, "Native QR expiration action is static");
            reveal(view.refresh);
            activate(view.refresh);
            render();
            check(codes.snapshot(id).status == QRCodeStatus::Active,
                  "Native QR refresh callback did not accept request");
            run("gallery.qrcode.refuse");
            run("gallery.qrcode.expire");
            view = codes.snapshot(id);
            reveal(view.refresh);
            activate(view.refresh);
            render();
            check(codes.snapshot(id).status == QRCodeStatus::Expired, "Controlled QR refresh refusal was overwritten");
            run("gallery.qrcode.status");
            check(codes.snapshot(id).status == QRCodeStatus::Loading, "Native QR loading action is static");
            run("gallery.qrcode.status");
            check(codes.snapshot(id).status == QRCodeStatus::Scanned, "Native QR scanned action is static");
            run("gallery.qrcode.status");
            run("gallery.qrcode.level");
            check(codes.snapshot(id).data->error_level() == QRCodeErrorLevel::Q,
                  "Native QR EC action did not reencode");
            run("gallery.qrcode.invalid");
            check(!codes.snapshot(id).data && codes.snapshot(id).modules.empty(),
                  "Invalid QR content produced fake modules");
            run("gallery.qrcode.invalid");
            run("gallery.qrcode.logo");
            view = codes.snapshot(id);
            check(view.logo_status == ImageStatus::Ready && view.logo_image && view.paint.logo,
                  "Native QR logo did not use shared real pixels");
            run("gallery.qrcode.export");
            view = codes.snapshot(id);
            const auto png = export_qrcode_png(*view.data, view.paint, 464);
            const auto svg = export_qrcode_svg(*view.data, view.paint);
            const auto path = options.output.parent_path() / ("qrcode-export-" + std::to_string(i));
            {
                std::ofstream file(path.string() + ".png", std::ios::binary);
                file.write(reinterpret_cast<const char*>(png.data()), static_cast<std::streamsize>(png.size()));
                check(bool(file), "Native QR export PNG write failed");
            }
            {
                std::ofstream file(path.string() + ".svg", std::ios::binary);
                file << svg;
                check(bool(file), "Native QR export SVG write failed");
            }
            check(pages.sample_runs("gallery.sample.qr-code.native") == 1, "Native QR props reran sample factory");
            std::cout << "qrcode_native=passed real_gpu_module_centers=true quiet_zone=true controlled_refresh=true "
                         "four_statuses=true ec=true error_recovery=true shared_logo=true png_svg_export=true\n";
        }
        if (route.component_identity == "ant.component.image") {
            auto& images = app.services().image();
            const auto id = pages.target("gallery.image.primary");
            reveal(id);
            auto view = images.snapshot(id);
            check(view.status == ImageStatus::Ready && view.image.has_value(), "Native Image has no decoded source");
            const auto* record = app.services().images().find(*view.image);
            check(record && record->instance.pixels->width() == 3, "Native Image CPU record missing");
            const auto r = record->instance.bounds;
            pointer(input::PointerAction::move, {0, 0});
            render();
            const auto frame = read_frame("image-native-" + std::to_string(i));
            check_pixel(frame.get(), {(r.x + r.width / 6) * events.scale, (r.y + r.height / 4) * events.scale},
                        Color::rgba8(255, 0, 0));
            const auto background = app.components().theme_scope(id)->snapshot().image().colors.background;
            constexpr float alpha = 128.F / 255;
            check_pixel(frame.get(), {(r.x + r.width / 2) * events.scale, (r.y + r.height / 4) * events.scale},
                        Color{background.red() * (1 - alpha), alpha + background.green() * (1 - alpha),
                              background.blue() * (1 - alpha)});
            const auto run = [&](std::string_view name) {
                reveal(pages.target(name));
                activate(pages.target(name));
                render();
            };
            run("gallery.image.preview");
            view = images.snapshot(id);
            check(view.open && app.services().images().live_count() >= 2, "Native Image preview did not open");
            check(app.focus().state().focused == view.canvas_interaction ||
                      app.focus().request_focus(view.canvas_interaction, input::FocusModality::keyboard),
                  "Native Image preview canvas focus failed");
            press(input::Key::up);
            check(images.snapshot(id).transform.scale > 1, "Native Image keyboard zoom failed");
            const auto canvas = images.snapshot(id).canvas_bounds;
            pointer(input::PointerAction::down, {canvas.x + canvas.width / 2, canvas.y + canvas.height / 2});
            pointer(input::PointerAction::move, {canvas.x + canvas.width * 2, canvas.y + canvas.height * 2});
            render();
            check(images.snapshot(id).dragging && captured(), "Native preview pointer capture missing");
            pointer(input::PointerAction::up, {canvas.x + canvas.width * 2, canvas.y + canvas.height * 2});
            render();
            check(!images.snapshot(id).dragging && !captured(), "Native preview release retained capture");
            press(input::Key::escape);
            check(!images.snapshot(id).open, "Native preview Escape did not close");
            run("gallery.image.load");
            check(images.snapshot(id).status == ImageStatus::Loading, "Native Image loading example is static");
            run("gallery.image.deliver");
            check(images.snapshot(id).status == ImageStatus::Ready, "Native Image manual source delivery failed");
            run("gallery.image.load");
            run("gallery.image.failure");
            check(images.snapshot(id).status == ImageStatus::Error, "Native Image error example did not fail");
            run("gallery.image.load");
            run("gallery.image.deliver");
            check(pages.request(GalleryPageRoute{GalleryPageKind::introduction}), "Image retirement route rejected");
            pages.process();
            render();
            check(images.mounted().empty() && app.services().images().live_count() == 0 && !captured(),
                  "Native Image page retirement leaked");
            std::cout << "image_native=passed rgba_pixel=true alpha_pixel=true preview_keyboard=true bounded_drag=true "
                         "source_loading_error=true retirement=true\n";
        }
        if (route.component_identity == "ant.component.upload") {
            auto& uploads = app.services().upload();
            const auto id = pages.target("gallery.upload.primary");
            const auto run = [&](std::string_view name) {
                reveal(pages.target(name));
                activate(pages.target(name));
                render();
            };
            run("gallery.upload.fixture");
            auto view = uploads.snapshot(id);
            check(view.files.size() == 1 && view.files[0].status == UploadFileStatus::Uploading,
                  "Native Upload synthetic request missing");
            const auto uid = view.files[0].uid;
            const auto row = view.rows[0].component;
            reveal(row);
            view = uploads.snapshot(id);
            const auto image = app.services().image().snapshot(view.rows[0].image);
            check(image.image.has_value(), "Native Upload has no actual thumbnail");
            const auto* record = app.services().images().find(*image.image);
            check(record != nullptr, "Native Upload image resource missing");
            const auto rect = record->instance.bounds;
            const auto frame = read_frame("upload-native-" + std::to_string(i));
            const auto background =
                app.components().theme_scope(view.rows[0].image)->snapshot().image().colors.background;
            constexpr float alpha = 128.F / 255;
            check_pixel(frame.get(),
                        {(rect.x + rect.width / 2) * events.scale, (rect.y + rect.height / 4) * events.scale},
                        Color{background.red() * (1 - alpha), alpha + background.green() * (1 - alpha),
                              background.blue() * (1 - alpha)});
            run("gallery.upload.ignore");
            check(uploads.snapshot(id).files.size() == 1, "Native beforeUpload Ignore failed");
            run("gallery.upload.progress");
            check(uploads.snapshot(id).files[0].percent == 20 && uploads.snapshot(id).rows[0].component == row,
                  "Native progress did not retain row");
            run("gallery.upload.failure");
            check(uploads.snapshot(id).files[0].status == UploadFileStatus::Error, "Native failure control failed");
            check(uploads.retry(id, uid), "Native Upload retry failed");
            render();
            run("gallery.upload.success");
            check(uploads.snapshot(id).files[0].status == UploadFileStatus::Done, "Native synthetic success failed");
            reveal(uploads.snapshot(id).rows[0].component);
            check(uploads.preview(id, uid), "Native Upload default preview failed");
            render();
            check(app.services().image().snapshot(uploads.snapshot(id).preview_group).open,
                  "Native Upload shared preview did not open");
            press(input::Key::escape);
            run("gallery.upload.custom");
            const auto content = uploads.snapshot(id).rows[0].content;
            const auto draft = edit_input(content);
            run("gallery.upload.list");
            check(uploads.snapshot(id).rows[0].content == content &&
                      std::string(inputs.display_snapshot(draft.first).text) == draft.second,
                  "Native Upload list mode lost retained draft");
            check(uploads.remove(id, uid), "Native Upload remove request failed");
            run("gallery.upload.remove");
            check(uploads.snapshot(id).files.empty(), "Native remove approval failed");
            run("gallery.upload.fixture");
            check(uploads.snapshot(id).files[0].status == UploadFileStatus::Uploading,
                  "Native cancellation fixture did not start");
            check(uploads.abort(id, uploads.snapshot(id).files[0].uid), "Native transport cancellation failed");
            run("gallery.upload.success");
            check(uploads.snapshot(id).files[0].status == UploadFileStatus::Cancelled,
                  "Late success overrode cancelled native upload");
            check(pages.request(GalleryPageRoute{GalleryPageKind::introduction}), "Upload retirement route rejected");
            pages.process();
            render();
            check(uploads.mounted().empty() && app.services().images().live_count() == 0 && !captured(),
                  "Native Upload page retirement leaked");
            std::cout << "upload_native=passed synthetic_transport_labeled=true actual_thumbnail_pixel=true "
                         "ignore=true progress=true retry=true cancel_stale=true preview=true retained_draft=true "
                         "removal=true retirement=true\n";
        }
        if (route.component_identity == "ant.component.transfer") {
            auto& transfer = app.services().transfer();
            const auto lists = pages.target("gallery.transfer.lists");
            const auto rich = pages.target("gallery.transfer.rich");
            reveal(lists);
            auto before = transfer.snapshot(lists);
            check(transfer.mounted().size() == 3, "native Transfer complete examples missing");
            const auto selected = std::ranges::find_if(app.interactions().declaration_order(), [&](auto hit) {
                return app.interactions().require(hit).component == before.rows[0].checkbox;
            });
            check(selected != app.interactions().declaration_order().end() &&
                      app.focus().request_focus(*selected, input::FocusModality::keyboard),
                  "native Transfer Checkbox cannot focus");
            press(input::Key::space);
            activate(before.move[1]);
            render();
            check(transfer.snapshot(lists).target_keys.size() == 3 && has_text(u8"Moved right: 1"),
                  "native Transfer controlled move callback missing");
            reveal(pages.target("gallery.transfer.accept"));
            activate(pages.target("gallery.transfer.accept"));
            render();
            auto range = transfer.snapshot(lists);
            const auto select_range = [&] {
                reveal(range.rows[4].checkbox);
                click(range.rows[4].checkbox);
                reveal(range.rows[6].checkbox);
                const auto range_rect = bounds(range.rows[6].checkbox);
                for (auto action : {input::PointerAction::down, input::PointerAction::up}) {
                    pages.dispatch(input::PointerInputEvent{
                        input::PointerIdentity::mouse(), action, input::PointerButton::primary,
                        range_rect.x + range_rect.width / 2, range_rect.y + range_rect.height / 2, 0, true});
                }
                render();
                check(transfer.snapshot(lists).selected_keys == TransferKeys{4.0, 5.0, 6.0},
                      "native Transfer pointer Shift metadata did not select keyed range");
            };
            select_range();
            reveal(lists);
            activate(range.move[1]);
            render();
            check(transfer.snapshot(lists).target_keys.size() == 3 && has_text(u8"Transfer move refused"),
                  "native Transfer controlled refusal mutated authority");
            reveal(pages.target("gallery.transfer.accept"));
            activate(pages.target("gallery.transfer.accept"));
            render();
            // Selection is independently controlled; the application accepted
            // onSelectChange clearing it even when it refused targetKeys.
            select_range();
            reveal(lists);
            activate(range.move[1]);
            render();
            check(transfer.snapshot(lists).target_keys.size() == 6 && has_text(u8"Moved right: 3"),
                  "native Transfer controlled acceptance lost range request");
            reveal(pages.target("gallery.transfer.locale"));
            activate(pages.target("gallery.transfer.locale"));
            render();
            check(has_text(u8"Source"), "native Transfer locale did not update retained text");
            reveal(pages.target("gallery.transfer.direction"));
            activate(pages.target("gallery.transfer.direction"));
            render();
            reveal(lists);
            check(transfer.snapshot(lists).sections[0].x > transfer.snapshot(lists).sections[1].x,
                  "native Transfer RTL panel order wrong");
            reveal(pages.target("gallery.transfer.geometry"));
            activate(pages.target("gallery.transfer.geometry"));
            render();
            reveal(lists);
            auto resized = transfer.snapshot(lists);
            check(resized.viewports[0].height == 96 && resized.rows[0].component == before.rows[0].component,
                  "native Transfer geometry remounted rows or stayed stale");
            const auto lane = pages.snapshot().layout.document;
            const auto root_bounds = bounds(lists);
            const auto& root_node = app.nodes().require(app.components().root(lists));
            const auto list = resized.viewports[0];
            const float x = list.x + root_node.translation.x + 6;
            const float y = list.y + root_node.translation.y + 6;
            check(x >= lane.x && x < lane.x + lane.width && y >= lane.y && y < lane.y + lane.height,
                  "native Transfer list scroll target is outside viewport");
            events.inject_scroll_at(x, y, -.25F);
            render();
            check(transfer.snapshot(lists).scroll[0] > 0 && has_text(u8"List scroll:"),
                  "native Transfer fractional scroll callback missing");
            static_cast<void>(root_bounds);
            reveal(pages.target("gallery.transfer.tokens"));
            activate(pages.target("gallery.transfer.tokens"));
            render();
            reveal(lists);
            const auto themed = app.components().theme_scope(lists)->snapshot().transfer();
            check(themed.metrics.item_height == 44 && themed.colors.header == Color::rgba8(255, 241, 184),
                  "native Transfer component Token update did not apply");
            const auto themed_section = transfer.snapshot(lists).sections[0];
            const auto themed_translation = app.nodes().require(app.components().root(lists)).translation;
            const auto transfer_image = read_frame("transfer-token-" + std::to_string(i));
            check_pixel(transfer_image.get(),
                        {(themed_section.x + themed_translation.x + themed_section.width / 2) * events.scale,
                         (themed_section.y + themed_translation.y + 6) * events.scale},
                        themed.colors.header);
            reveal(rich);
            const auto note = replace_text(rich, u8"Transfer retained draft");
            const auto rich_before = transfer.snapshot(rich);
            activate(rich_before.move[1]);
            render();
            check(transfer.snapshot(rich).target_keys == TransferKeys{String{u8"draft"}} &&
                      inputs.display_snapshot(note.component).text == "Transfer retained draft",
                  "native Transfer row move lost rich draft");
            const auto custom = pages.target("gallery.transfer.custom");
            auto& tables = app.services().table();
            const auto table = std::ranges::find_if(
                tables.mounted(), [&](auto id) { return owns(app, id, custom) && !tables.snapshot(id).rows.empty(); });
            check(table != tables.mounted().end(), "native Transfer retained Table missing");
            const auto checkbox = tables.snapshot(*table).rows.front().checkbox;
            reveal(checkbox);
            click(checkbox);
            check(transfer.snapshot(custom).selected_keys.size() == 1,
                  "native Transfer custom Table selection callback missing");
            const auto custom_move = transfer.snapshot(custom).move[1];
            reveal(custom_move);
            activate(custom_move);
            render();
            check(transfer.snapshot(custom).target_keys.size() == 1 && has_text(u8"Custom list moved: 1"),
                  "native Transfer custom action callback missing");
            const auto footer = pages.target("gallery.transfer.custom.footer");
            reveal(footer);
            const auto footer_note = replace_text(footer, u8"Transfer custom footer draft");
            capture_editor(footer);
            previous_session = inputs.sessions().active();
            pages.dispatch(input::CompositionChanged{String{u8"未提交"}, {}, previous_session});
            render();
            check(inputs.editors().require(previous_session.owner).composition().active,
                  "native Transfer preedit missing");
            check(pages.request(GalleryPageRoute{GalleryPageKind::introduction}),
                  "native Transfer retirement rejected");
            static_cast<void>(pages.process());
            render();
            check(transfer.mounted().empty() && !app.components().contains(note.component) &&
                      !app.components().contains(footer_note.component) && !inputs.sessions().active().valid() &&
                      !captured() && !inputs.editors().find(previous_session.owner),
                  "native Transfer retirement retained rich input/preedit/capture");
            std::cout << "transfer_native=passed controlled_move=true controlled_refusal=true shift_metadata=true "
                         "custom_table_action=true locale_rtl_geometry=true rich_draft=true "
                         "preedit_capture_retired=true\n";
        }
        if (route.component_identity == "ant.component.color-picker") {
            auto& colors = app.services().color_picker();
            const auto solid = pages.target("gallery.color-picker.solid");
            const auto gradient = pages.target("gallery.color-picker.gradient");
            const auto open_color = [&](runtime::ComponentId owner) {
                reveal(owner);
                const auto lane = pages.snapshot().layout.document;
                const auto rect = bounds(owner);
                events.inject_scroll_at(lane.x + lane.width - 8, lane.y + lane.height / 2, -(rect.y - lane.y - 8) / 48);
                render();
                activate(owner);
                render();
            };
            const auto labelled_button = [&](runtime::ComponentId owner, String label) {
                for (const auto& button : app.mounted_buttons()) {
                    if (!owns(app, button.component, owner) || !app.interactions().branch_active(button.interaction)) {
                        continue;
                    }
                    for (const auto& text : app.services().text().mounted_texts()) {
                        if (owns(app, text.component, button.component) &&
                            app.services().text().scene_service().text_state(text.scene).content().bytes() ==
                                label.bytes()) {
                            return button.component;
                        }
                    }
                }
                throw std::runtime_error("native ColorPicker labelled action is missing");
            };
            open_color(solid);
            check(colors.snapshot(solid).open, "native ColorPicker did not open");
            const auto note = replace_text(pages.target("gallery.color-picker.note"), u8"retained color draft");
            const auto drag = [&](runtime::ComponentId owner, std::size_t field, float x, float y) {
                const auto rect = bounds(colors.snapshot(owner).fields[field]);
                pointer(input::PointerAction::down, {rect.x + rect.width / 4, rect.y + rect.height / 2});
                render();
                check(captured() && colors.snapshot(owner).dragging, "native color field did not capture");
                pointer(input::PointerAction::move, {rect.x + rect.width * x, rect.y + rect.height * y});
                render();
                pointer(input::PointerAction::up, {rect.x + rect.width * x, rect.y + rect.height * y});
                render();
                check(!captured() && !colors.snapshot(owner).dragging, "native color field retained capture");
            };
            const auto original = colors.snapshot(solid).value;
            drag(solid, 0, .8F, .2F);
            check(colors.snapshot(solid).value != original && has_text(u8"Complete:"), "native SV callback failed");
            drag(solid, 1, .7F, .5F);
            const auto opaque = colors.snapshot(solid).value;
            drag(solid, 2, .25F, .5F);
            check(colors.snapshot(solid).value != opaque, "native hue/alpha callbacks failed");
            click(labelled_button(solid, PickerColor{ColorRgb{22, 119, 255, 1}}.format(ColorFormat::Hex)));
            check(colors.snapshot(solid).value == ColorPickerValue{PickerColor{ColorRgb{22, 119, 255, 1}}},
                  "native ColorPicker preset callback failed");
            for (const auto format : {ColorFormat::Rgb, ColorFormat::Hsb, ColorFormat::Hex}) {
                click(labelled_button(solid, u8"HEX / RGB / HSB"));
                check(colors.snapshot(solid).format == format && has_text(u8"Format callback"),
                      "native ColorPicker format callback failed");
            }
            check(inputs.display_snapshot(note.component).text == "retained color draft",
                  "native custom ColorPicker panel remounted its editor");
            click(labelled_button(solid, u8"Clear"));
            check(colors.snapshot(solid).value.empty() && has_text(u8"Clear callback"),
                  "native ColorPicker clear callback failed");
            press(input::Key::escape);
            open_color(gradient);
            const auto old_gradient = colors.snapshot(gradient).value;
            drag(gradient, 3, .35F, .5F);
            check(colors.snapshot(gradient).value.gradient() && colors.snapshot(gradient).value != old_gradient,
                  "native gradient stop drag failed");
            click(labelled_button(gradient, u8"Add stop"));
            check(colors.snapshot(gradient).value.stops().size() == 3, "native gradient add callback failed");
            click(labelled_button(gradient, u8"Remove"));
            check(colors.snapshot(gradient).value.stops().size() == 2, "native gradient remove callback failed");
            press(input::Key::escape);
            open_color(solid);
            capture_editor(pages.target("gallery.color-picker.note"));
            previous_session = inputs.sessions().active();
            pages.dispatch(input::CompositionChanged{String{u8"未提交"}, {}, previous_session});
            render();
            check(inputs.editors().require(previous_session.owner).composition().active,
                  "native ColorPicker custom editor preedit missing");
            check(pages.request(GalleryPageRoute{GalleryPageKind::introduction}),
                  "native ColorPicker retirement rejected");
            static_cast<void>(pages.process());
            render();
            check(colors.mounted().empty() && !app.components().contains(note.component) &&
                      !inputs.sessions().active().valid() && !captured() &&
                      !inputs.editors().find(previous_session.owner),
                  "native ColorPicker retirement retained editor/preedit/capture");
            std::cout << "color_picker_native=passed sv_hue_alpha_callbacks=true gradient_drag_add_remove=true "
                         "preset_clear=true custom_panel_draft=true preedit_capture_retired=true\n";
        }
        if (route.component_identity == "ant.component.input-number") {
            const auto exact = pages.target("gallery.number.exact");
            const auto formatted = pages.target("gallery.number.formatted");
            reveal(exact);
            replace_text(exact, u8"9007199254740993.00000000000000001");
            press(input::Key::up);
            check(inputs.display_snapshot(exact).text == "9007199254740993.00000000000000002" &&
                      has_text(u8"9007199254740993.00000000000000002"),
                  "native exact decimal step callback failed");
            auto area = inputs.layout_snapshot(exact).viewport;
            auto offset = pages.snapshot().document.offset;
            events.inject_scroll_at(area.x + area.width / 2, area.y + area.height / 2, 1);
            render();
            check(inputs.display_snapshot(exact).text == "9007199254740993.00000000000000003" &&
                      pages.snapshot().document.offset == offset,
                  "native exact decimal wheel lost precision or leaked");
            reveal(formatted);
            const auto editor = replace_text(formatted, u8"12,345.6");
            press(input::Key::enter);
            check(inputs.number_value(formatted) && std::abs(*inputs.number_value(formatted) - 12345.6) < .000001 &&
                      inputs.display_snapshot(formatted).text == "12,345.6" && has_text(u8"Formatted value:"),
                  "native InputNumber formatter/parser callback failed");
            const auto actions = inputs.number_actions(formatted);
            const auto up = bounds(actions[0]);
            const runtime::Point center{up.x + up.width / 2, up.y + up.height / 2};
            const auto before = *inputs.number_value(formatted);
            pointer(input::PointerAction::down, center);
            render();
            check(captured(), "native number hold did not capture");
            const auto hold_deadline = std::chrono::steady_clock::now() + std::chrono::seconds(2);
            while (*inputs.number_value(formatted) < before + .15 && std::chrono::steady_clock::now() < hold_deadline) {
                render();
            }
            check(*inputs.number_value(formatted) > before + .15, "native number hold repeat did not run");
            pointer(input::PointerAction::up, center);
            render();
            const auto released = inputs.number_value(formatted);
            const auto release_deadline = std::chrono::steady_clock::now() + std::chrono::milliseconds(500);
            while (std::chrono::steady_clock::now() < release_deadline) {
                render();
            }
            check(!captured() && inputs.number_value(formatted) == released, "native number release retained repeat");
            area = inputs.layout_snapshot(formatted).viewport;
            offset = pages.snapshot().document.offset;
            events.inject_scroll_at(area.x + area.width / 2, area.y + area.height / 2, -1);
            render();
            check(*inputs.number_value(formatted) < *released && pages.snapshot().document.offset == offset,
                  "native formatted number wheel callback failed");
            focus_editor(formatted);
            const auto stamp = inputs.sessions().active();
            pages.dispatch(input::CompositionChanged{String{u8"77"}, {}, stamp});
            render();
            check(inputs.editors().require(editor.editor).composition().active, "native number preedit missing");
            focus_editor(exact);
            render();
            check(!inputs.editors().require(editor.editor).composition().active, "native number blur retained preedit");
            focus_editor(formatted);
            pointer(input::PointerAction::down, center);
            render();
            check(captured(), "native number retirement has no active hold capture");
            previous_session = inputs.sessions().active();
            check(pages.request(GalleryPageRoute{GalleryPageKind::introduction}), "native number retirement rejected");
            static_cast<void>(pages.process());
            render();
            check(!app.components().contains(exact) && !app.components().contains(formatted) &&
                      !app.components().contains(actions[0]) && !inputs.sessions().active().valid() && !captured() &&
                      !inputs.editors().find(previous_session.owner),
                  "native number retirement retained repeat/editor/capture");
            std::cout << "input_number_native=passed exact_beyond_2pow53=true exact_wheel=true "
                         "formatter_parser_callback=true "
                         "hold_repeat_release=true wheel_input_first=true preedit_blur=true hold_retired=true\n";
        }
        if (route.component_identity == "ant.component.form") {
            auto& forms = app.services().form();
            const auto action = [&](std::string_view target) {
                const auto id = pages.target(target);
                reveal(id);
                activate(id);
                render();
            };
            check(forms.mounted().size() == 4, "native Form workflows missing");
            action("gallery.form.layouts.submit");
            check(has_text(u8"校验失败：1 项"), "native Form required validation did not run");
            action("gallery.form.layouts.reveal");
            const auto name = focus_editor(pages.target("gallery.form.layouts.name"));
            check(app.focus().state().focused == name.interaction,
                  "native Form scroll acknowledgement failed to focus visible field");
            const auto item =
                std::ranges::find_if(forms.items(), [&](auto id) { return owns(app, name.component, id); });
            check(item != forms.items().end(), "native Form automatic control has no Item");
            const auto regions = forms.snapshot(*item);
            check(std::abs(bounds(regions.control).y - bounds(regions.label).y) < 1,
                  "native Form Horizontal geometry is not horizontal");
            action("gallery.form.layouts.toggle");
            check(bounds(regions.control).y > bounds(regions.label).y, "native Form Vertical geometry did not update");
            action("gallery.form.layouts.toggle");
            action("gallery.form.layouts.toggle");
            action("gallery.form.layouts.rtl");
            check(bounds(regions.label).x > bounds(regions.control).x, "native Form RTL columns were not mirrored");
            action("gallery.form.layouts.rtl");
            action("gallery.form.layouts.reveal");
            replace_text(name.component, u8"Native user");
            const auto amount = pages.target("gallery.form.layouts.amount");
            action("gallery.form.layouts.reveal-amount");
            reveal(amount);
            replace_text(amount, u8"9007199254740993.0002");
            press(input::Key::enter);
            check(has_text(u8"name=Native user amount=9007199254740993.0002"),
                  "native Form automatic exact Decimal submit rounded or lost typed values");
            const auto custom = pages.target("gallery.form.layouts.custom");
            action("gallery.form.layouts.reveal-custom");
            reveal(custom);
            replace_text(custom, u8"native custom adapter");
            press(input::Key::enter);
            check(has_text(u8"custom=native custom adapter submitCount=2"),
                  "native Form custom typed adapter did not submit actual input");
            action("gallery.form.layouts.reveal");
            focus_editor(name.component);
            const auto composing = inputs.sessions().active();
            pages.dispatch(input::CompositionChanged{String{u8"候选"}, {}, composing});
            render();
            press(input::Key::enter);
            check(inputs.editors().require(name.editor).composition().active && has_text(u8"submitCount=2") &&
                      !has_text(u8"submitCount=3"),
                  "native Form composition Enter submitted a snapshot");
            action("gallery.form.layouts.reset");
            const auto reset_display = inputs.display_snapshot(name.component);
            check(inputs.editors().require(name.editor).value().empty() && reset_display.placeholder &&
                      reset_display.text == "填写名称" && !inputs.editors().require(name.editor).composition().active,
                  "native Form reset retained typed input/preedit or failed to show its placeholder");
            const auto first = pages.target("gallery.form.list.name.1");
            const auto second = pages.target("gallery.form.list.name.2");
            check(app.components().contains(first) && app.components().contains(second),
                  "native Form initial stable List targets missing");
            reveal(first);
            const auto first_editor = replace_text(first, u8"Ada");
            reveal(second);
            replace_text(second, u8"Grace");
            const auto note = pages.target("gallery.form.list.note.1");
            reveal(note);
            const auto note_editor = replace_text(note, u8"saved\nnote");
            press(input::Key::enter);
            check(inputs.display_snapshot(note).text == "saved\nnote\n",
                  "native Form multiline Enter submitted instead of inserting a newline");
            action("gallery.form.list.add");
            const auto third = pages.target("gallery.form.list.name.3");
            reveal(third);
            replace_text(third, u8"Lin");
            action("gallery.form.list.move.2");
            check(inputs.editors().find(first_editor.editor) && inputs.editors().find(note_editor.editor) &&
                      inputs.display_snapshot(first).text == "Ada" &&
                      inputs.display_snapshot(note).text == "saved\nnote\n",
                  "native Form List move remounted retained drafts");
            action("gallery.form.list.submit");
            check(has_text(u8"people/0/name/Grace") && has_text(u8"people/1/notes/0/text/saved\nnote"),
                  "native Form deep List submitted wrong paths or values");
            const auto retired = focus_editor(second);
            action("gallery.form.list.remove.2");
            check(!app.components().contains(second) && !inputs.editors().find(retired.editor) &&
                      inputs.editors().find(note_editor.editor),
                  "native Form List remove retained stale editor or removed another draft");
            action("gallery.form.list.reset");
            check(inputs.display_snapshot(first).text.empty(), "native Form List reset did not restore initial values");
            const auto async_name = pages.target("gallery.form.async.name");
            const auto confirmation = pages.target("gallery.form.async.confirmation");
            reveal(async_name);
            replace_text(async_name, u8"taken");
            reveal(confirmation);
            replace_text(confirmation, u8"taken");
            action("gallery.form.async.submit");
            check(has_text(u8"正在校验当前快照"), "native Form async submit did not become pending");
            reveal(async_name);
            replace_text(async_name, u8"latest");
            check(has_text(u8"旧快照已取消"), "native Form changed value did not cancel old submit");
            reveal(confirmation);
            replace_text(confirmation, u8"latest");
            action("gallery.form.async.submit");
            action("gallery.form.async.deliver");
            check(has_text(u8"实际异步提交 name=latest confirmation=latest") && !has_text(u8"校验错误"),
                  "native Form stale async result replaced latest typed submission");
            reveal(async_name);
            replace_text(async_name, u8"cancel");
            reveal(confirmation);
            replace_text(confirmation, u8"cancel");
            action("gallery.form.async.submit");
            action("gallery.form.async.reset");
            action("gallery.form.async.deliver");
            check(!has_text(u8"正在校验当前快照") && !has_text(u8"实际异步提交 name=cancel"),
                  "native Form reset published cancelled async work");
            reveal(async_name);
            replace_text(async_name, u8"retired");
            reveal(confirmation);
            replace_text(confirmation, u8"retired");
            action("gallery.form.async.submit");
            reveal(async_name);
            capture_editor(async_name);
            // Revealing a different field may intentionally cancel the old eligibility snapshot.
            // Enter starts the current async submission without moving focus or releasing capture.
            press(input::Key::enter);
            previous_session = inputs.sessions().active();
            pages.dispatch(input::CompositionChanged{String{u8"离页候选"}, {}, previous_session});
            render();
            std::cout << "form_retirement_setup capture=" << captured()
                      << " preedit=" << inputs.editors().require(previous_session.owner).composition().active
                      << " pending=" << has_text(u8"正在校验当前快照") << '\n';
            check(captured() && inputs.editors().require(previous_session.owner).composition().active &&
                      has_text(u8"正在校验当前快照"),
                  "native Form retirement setup lost pending validation, preedit or capture");
            check(pages.request(GalleryPageRoute{GalleryPageKind::introduction}), "native Form retirement rejected");
            static_cast<void>(pages.process());
            render();
            check(forms.mounted().empty() && !inputs.sessions().active().valid() && !captured() &&
                      !inputs.editors().find(previous_session.owner),
                  "native Form retirement retained pending editor, preedit, capture or session");
            std::cout
                << "form_native=passed layouts=true typed_binding=true custom_adapter=true exact_decimal=true "
                   "deep_list_add_move_remove=true retained_drafts=true actual_values=true multiline_enter=true "
                   "core_preedit=true reset=true async_latest_cancel_submit=true scroll_focus=true retired=true\n";
        }
        if (route.component_identity == "ant.component.mentions") {
            auto& mentions = app.services().mentions();
            const auto main = pages.target("gallery.mentions.editor");
            reveal(main);
            const auto native = replace_text(main, u8"消息\n@Ad");
            auto measured = mentions.snapshot(main);
            const auto caret = inputs.layout_snapshot(native.component).caret;
            check(measured.visible && measured.query == String{u8"Ad"} && measured.prefix == String{u8"@"} &&
                      measured.bounds.y >= caret.y,
                  "native Mentions caret query/placement failed");
            const auto note = replace_text(pages.target("gallery.mentions.note"), u8"saved rich note");
            focus_editor(main);
            press(input::Key::escape);
            replace_text(main, u8"消息\n@A");
            check(pages.target("gallery.mentions.note") == note.component &&
                      inputs.display_snapshot(note.component).text == "saved rich note",
                  "native Mentions rich suggestion draft remounted or lost text");
            press(input::Key::enter);
            check(mentions.snapshot(main).value == String{u8"消息\n@Ada "}, "native Mentions inserted text failed");
            const auto authority = pages.target("gallery.mentions.accept");
            reveal(authority);
            activate(authority);
            render();
            reveal(main);
            replace_text(main, u8"@Grace");
            check(mentions.snapshot(main).value == String{u8"消息\n@Ada "},
                  "native Mentions controlled refusal failed");
            reveal(authority);
            activate(authority);
            render();
            reveal(main);
            replace_text(main, u8"@G");
            const auto stamp = inputs.sessions().active();
            pages.dispatch(input::CompositionChanged{String{u8"组合"}, {}, stamp});
            render();
            const auto accepted = mentions.snapshot(main).value;
            press(input::Key::enter);
            check(inputs.editors().require(native.editor).composition().active &&
                      mentions.snapshot(main).value == accepted,
                  "native Mentions composition Enter selected a suggestion");
            pages.dispatch(input::CompositionChanged{String{}, {}, stamp});
            render();
            const auto reset = pages.target("gallery.mentions.reset");
            reveal(reset);
            activate(reset);
            render();
            check(mentions.snapshot(main).value == String{u8"在这里输入 @ 或 #\n"} && !mentions.snapshot(main).visible,
                  "native Mentions Form reset failed");
            const auto long_list = pages.target("gallery.mentions.long");
            reveal(long_list);
            replace_text(long_list, u8"@");
            for (int step = 0; step < 12; ++step) {
                press(input::Key::down);
            }
            const auto window = mentions.snapshot(long_list);
            check(window.visible && window.total_rows == 10000 && window.rows.size() <= 10 && window.scroll > 0,
                  "native Mentions long list realization failed");
            const auto offset = pages.snapshot().document.offset;
            events.inject_scroll_at(window.clip.x + 4, window.clip.y + 4, -2);
            render();
            check(mentions.snapshot(long_list).scroll > window.scroll && pages.snapshot().document.offset == offset,
                  "native Mentions wheel escaped to document");
            press(input::Key::escape);
            const auto async = pages.target("gallery.mentions.async");
            reveal(async);
            replace_text(async, u8"@old");
            replace_text(async, u8"@latest");
            check(mentions.snapshot(async).pending, "native Mentions async request missing");
            click(pages.target("gallery.mentions.deliver"));
            const auto delivered = mentions.snapshot(async);
            check(!delivered.pending && delivered.query == String{u8"latest"} && delivered.total_rows == 1 &&
                      has_text(u8"latest-synthetic") && !has_text(u8"old-synthetic"),
                  "native Mentions stale result applied");
            click(delivered.rows.front());
            check(mentions.snapshot(async).value == String{u8"@latest-synthetic "},
                  "native Mentions async choice failed");
            replace_text(async, u8"@retired");
            capture_editor(async);
            previous_session = inputs.sessions().active();
            check(pages.request(GalleryPageRoute{GalleryPageKind::introduction}), "Mentions retirement rejected");
            static_cast<void>(pages.process());
            render();
            check(mentions.mounted().empty() && !inputs.sessions().active().valid() && !captured() &&
                      !inputs.editors().find(previous_session.owner),
                  "native Mentions retained retired editor/capture/ticket");
            std::cout
                << "mentions_native=passed multiline=true caret_popup=true native_replace=true controlled_refusal=true "
                   "core_composition_enter=true form_reset=true rich_draft=true long_window=true "
                   "input_first_wheel=true "
                   "synthetic_latest=true retired=true\n";
        }
        if (route.component_identity == "ant.component.auto-complete") {
            auto& complete = app.services().auto_complete();
            const auto grouped = pages.target("gallery.auto-complete.virtual");
            reveal(grouped);
            replace_text(grouped, u8"Ada");
            check(complete.snapshot(grouped).total_rows == 2, "native AutoComplete grouped filter failed");
            press(input::Key::enter);
            check(complete.snapshot(grouped).value == String{u8"Ada"}, "native AutoComplete grouped selection failed");
            replace_text(grouped, u8"");
            for (int step = 0; step < 8; ++step) {
                press(input::Key::down);
            }
            auto window = complete.snapshot(grouped);
            std::cout << "auto_complete_window query=" << window.query.bytes()
                      << " active=" << window.active.value_or(999999) << " rows=" << window.rows.size()
                      << " total=" << window.total_rows << " scroll=" << window.scroll << " visible=" << window.visible
                      << '\n';
            check(window.visible && window.active && window.scroll > 0 && window.rows.size() <= 10 &&
                      std::ranges::find(window.row_indices, *window.active) != window.row_indices.end(),
                  "native AutoComplete keyboard reveal is unbounded");
            const auto offset = pages.snapshot().document.offset;
            events.inject_scroll_at(window.clip.x + 4, window.clip.y + 4, -2);
            render();
            check(complete.snapshot(grouped).scroll > window.scroll && complete.snapshot(grouped).rows.size() <= 10 &&
                      pages.snapshot().document.offset == offset,
                  "native AutoComplete wheel escaped its bounded list");
            press(input::Key::escape);

            const auto custom = pages.target("gallery.auto-complete.custom");
            reveal(custom);
            const auto editor = replace_text(custom, u8"多行\n草稿");
            const auto stamp = inputs.sessions().active();
            pages.dispatch(input::CompositionChanged{String{u8"组合"}, {}, stamp});
            render();
            check(inputs.editors().require(editor.editor).composition().active,
                  "native custom TextArea composition missing");
            const auto composing_value = complete.snapshot(custom).value;
            press(input::Key::down);
            check(complete.snapshot(custom).value == composing_value &&
                      inputs.editors().require(editor.editor).composition().active,
                  "native composition navigation changed AutoComplete value");
            pages.dispatch(input::CompositionChanged{String{}, {}, stamp});
            render();
            replace_text(custom, u8"");
            press(input::Key::down);
            check(complete.snapshot(custom).visible && complete.snapshot(custom).backfilled,
                  "native TextArea backfill missing");
            const auto popup_note = pages.target("gallery.auto-complete.popup-note");
            const auto note = replace_text(popup_note, u8"retained native note");
            check(complete.snapshot(custom).visible, "native custom popup focus closed suggestions");
            const auto choice = complete.snapshot(custom).rows.front();
            click(choice);
            check(complete.snapshot(custom).value == String{u8"第一段"} &&
                      app.focus().state().focused == editor.interaction,
                  "native custom TextArea selection/focus return failed");
            press(input::Key::down);
            check(inputs.display_snapshot(note.component).text == "retained native note",
                  "native AutoComplete popup draft was replaced");
            press(input::Key::escape);

            const auto form = pages.target("gallery.auto-complete.form");
            reveal(form);
            replace_text(form, u8"changed");
            const auto reset = pages.target("gallery.auto-complete.reset");
            reveal(reset);
            activate(reset);
            render();
            check(complete.snapshot(form).value.empty() && !complete.snapshot(form).visible,
                  "native AutoComplete Form reset failed");
            const auto controlled = pages.target("gallery.auto-complete.input");
            const auto authority = pages.target("gallery.auto-complete.authority");
            reveal(authority);
            activate(authority);
            render();
            const auto accepted = complete.snapshot(controlled).value;
            reveal(controlled);
            replace_text(controlled, u8"refused");
            check(complete.snapshot(controlled).value == accepted && has_text(u8"拒绝候选="),
                  "native AutoComplete controlled refusal failed");

            const auto async = pages.target("gallery.auto-complete.async");
            reveal(async);
            replace_text(async, u8"old");
            replace_text(async, u8"latest");
            check(complete.snapshot(async).pending, "native AutoComplete async query did not start");
            click(pages.target("gallery.auto-complete.deliver"));
            auto delivered = complete.snapshot(async);
            check(!delivered.pending && delivered.total_rows == 1 && delivered.query == String{u8"latest"} &&
                      has_text(u8"latest · synthetic") && !has_text(u8"old · synthetic"),
                  "native AutoComplete stale/latest owner-thread delivery failed");
            click(delivered.rows.front());
            check(complete.snapshot(async).value == String{u8"latest · synthetic"},
                  "native AutoComplete async selection failed");
            replace_text(async, u8"retired pending");
            capture_editor(async);
            previous_session = inputs.sessions().active();
            check(pages.request(GalleryPageRoute{GalleryPageKind::introduction}), "AutoComplete retirement rejected");
            static_cast<void>(pages.process());
            render();
            check(complete.mounted().empty() && !app.components().contains(note.component) &&
                      !inputs.sessions().active().valid() && !captured() &&
                      !inputs.editors().find(previous_session.owner),
                  "native AutoComplete retirement retained popup/editor/session/capture");
            std::cout << "auto_complete_native=passed grouped_filter=true virtual_keyboard_wheel=true textarea=true "
                         "composition=true backfill=true popup_draft=true form_reset=true controlled_refusal=true "
                         "synthetic_latest_callback=true retired=true\n";
        }
        if (route.component_identity == "ant.component.select") {
            auto& select = app.services().select();
            const auto multiple = pages.target("gallery.select.multiple");
            reveal(multiple);
            replace_text(multiple, u8"服务");
            press(input::Key::down);
            press(input::Key::enter);
            check(select.snapshot(multiple).values ==
                          SelectValues{SelectValue{u8"desktop"}, SelectValue{u8"service"}} &&
                      has_text(u8"已接受多选"),
                  "native grouped Multiple callback failed");
            const auto first_tag = select.snapshot(multiple).tags.front();
            bool removed_tag{};
            for (const auto interaction : app.interactions().declaration_order()) {
                const auto* record = app.interactions().find(interaction);
                if (record && record->eligible && app.interactions().branch_active(interaction) &&
                    owns(app, record->component, first_tag)) {
                    click(record->component);
                    removed_tag = true;
                    break;
                }
            }
            check(removed_tag, "native Multiple tag has no pointer action");
            check(select.snapshot(multiple).values == SelectValues{SelectValue{u8"service"}},
                  "native Multiple tag removal failed");
            const auto authority = pages.target("gallery.select.authority");
            reveal(authority);
            activate(authority);
            render();
            reveal(multiple);
            replace_text(multiple, u8"桌面");
            press(input::Key::down);
            press(input::Key::enter);
            check(select.snapshot(multiple).values == SelectValues{SelectValue{u8"service"}} &&
                      has_text(u8"已拒绝多选"),
                  "native Multiple controlled refusal failed");
            press(input::Key::escape);
            const auto tags = pages.target("gallery.select.tags");
            std::cout << "select_native_step=tags\n" << std::flush;
            reveal(tags);
            replace_text(tags, u8"中文；英文,尾部");
            check(select.snapshot(tags).values == SelectValues{SelectValue{u8"中文"}, SelectValue{u8"英文"}} &&
                      select.snapshot(tags).query == String{u8"尾部"},
                  "native Tags UTF8 tokenization failed");
            press(input::Key::enter);
            check(select.snapshot(tags).values.size() == 3 && has_text(u8"标签数量=3"),
                  "native Tags Enter callback failed");
            activate(select.snapshot(tags).tags.front());
            render();
            check(select.snapshot(tags).values.size() == 2, "native custom tag removal failed");
            press(input::Key::escape);

            const auto virtual_list = pages.target("gallery.select.virtual");
            std::cout << "select_native_step=virtual\n" << std::flush;
            reveal(virtual_list);
            activate(pages.target("gallery.select.prefix"));
            render();
            check(has_text(u8"已执行前缀操作"), "native Select interactive affix callback failed");
            focus_editor(select.snapshot(virtual_list).selector);
            press(input::Key::down);
            press(input::Key::end);
            const auto window = select.snapshot(virtual_list);
            check(window.visible && window.active == 9999 && window.rows.size() <= 10 && window.scroll > 0 &&
                      std::ranges::find(window.row_indices, 9999) != window.row_indices.end(),
                  "native Select End reveal failed");
            const auto offset = pages.snapshot().document.offset;
            const auto wheel_row = bounds(window.rows.back());
            events.inject_scroll_at(wheel_row.x + wheel_row.width / 2, wheel_row.y + wheel_row.height / 2, -2);
            render();
            check(select.snapshot(virtual_list).scroll == window.scroll,
                  "native Select custom footer made bottom wheel jump backwards");
            events.inject_scroll_at(wheel_row.x + wheel_row.width / 2, wheel_row.y + wheel_row.height / 2, 2);
            render();
            check(select.snapshot(virtual_list).scroll < window.scroll &&
                      select.snapshot(virtual_list).rows.size() <= 10 && pages.snapshot().document.offset == offset,
                  "native Select virtual wheel leaked/unbounded");
            const auto note = replace_text(pages.target("gallery.select.popup-note"), u8"saved Select draft");
            check(select.snapshot(virtual_list).visible, "native Select custom popup lost focus ownership");
            press(input::Key::escape);
            focus_editor(select.snapshot(virtual_list).selector);
            press(input::Key::down);
            check(inputs.display_snapshot(note.component).text == "saved Select draft",
                  "native Select lost popup draft");
            press(input::Key::escape);

            const auto async = pages.target("gallery.select.async");
            std::cout << "select_native_step=async\n" << std::flush;
            reveal(async);
            replace_text(async, u8"old");
            replace_text(async, u8"latest");
            check(select.snapshot(async).pending, "native Select source query missing");
            click(pages.target("gallery.select.deliver"));
            const auto delivered = select.snapshot(async);
            check(!delivered.pending && delivered.total_rows == 1 && has_text(u8"latest · synthetic") &&
                      !has_text(u8"old · synthetic"),
                  "native Select stale/latest delivery failed");
            click(delivered.rows.front());
            check(select.snapshot(async).value == SelectSelection{SelectValue{u8"latest · synthetic"}},
                  "native Select async single compatibility callback failed");
            replace_text(async, u8"retired pending");
            capture_editor(select.snapshot(async).selector);
            previous_session = inputs.sessions().active();
            check(pages.request(GalleryPageRoute{GalleryPageKind::introduction}), "native Select retirement rejected");
            static_cast<void>(pages.process());
            render();
            check(std::ranges::none_of(select.mounted(), [&](auto id) { return owns(app, id, root); }) &&
                      !app.components().contains(root) && !app.components().contains(note.component) &&
                      !inputs.sessions().active().valid() && !captured() &&
                      !inputs.editors().find(previous_session.owner),
                  "native Select retirement retained resources");
            std::cout << "select_native=passed grouped_multiple_callback=true tag_removal=true utf8_tokens=true "
                         "controlled_refusal=true interactive_affix=true popup_draft=true virtual_end_wheel=true "
                         "synthetic_latest_callback=true retired=true\n";
        }
        if (route.component_identity == "ant.component.tree") {
            auto& trees = app.services().tree();
            const auto owner = pages.target("gallery.tree.complete");
            reveal(owner);
            auto tree = trees.snapshot(owner);
            key(tree.rows[0].interaction, input::Key::enter);
            check(trees.snapshot(owner).selected == TreeKeys{u8"native-a"}, "native Tree selection callback failed");
            key(tree.rows[0].interaction, input::Key::space);
            check(trees.snapshot(owner).checked == TreeKeys{u8"native-a"}, "native Tree checkbox callback failed");
            reveal(pages.target("gallery.tree.reorder"));
            activate(pages.target("gallery.tree.reorder"));
            render();
            check(trees.snapshot(owner).rows.back().component == tree.rows.front().component,
                  "native Tree reorder lost identity");
            reveal(owner);
            auto reordered = trees.snapshot(owner);
            const auto drag_source = bounds(reordered.rows[0].title);
            const auto drag_target = bounds(reordered.rows[1].component);
            pointer(input::PointerAction::down,
                    {drag_source.x + drag_source.width / 2, drag_source.y + drag_source.height / 2});
            render();
            check(captured(), "native Tree drag did not capture");
            pointer(input::PointerAction::move,
                    {drag_source.x + drag_source.width / 2, drag_target.y + drag_target.height - 1});
            render();
            pointer(input::PointerAction::up,
                    {drag_source.x + drag_source.width / 2, drag_target.y + drag_target.height - 1});
            render();
            check(!captured() && trees.snapshot(owner).rows.front().key == TreeKeys{u8"native-b"}.front(),
                  "native Tree drop callback did not publish application order");

            const auto virtual_tree = pages.target("gallery.tree.virtual");
            reveal(virtual_tree);
            auto virtual_before = trees.snapshot(virtual_tree);
            check(virtual_before.rows.size() == 10000 &&
                      std::ranges::count(virtual_before.rows, true, &ryn::detail::TreeRowSnapshot::realized) <= 10,
                  "native Tree virtual row bound failed");
            reveal(pages.target("gallery.tree.scroll-end"));
            activate(pages.target("gallery.tree.scroll-end"));
            render();
            check(trees.snapshot(virtual_tree).rows.back().realized, "native Tree scrollTo failed");
            const auto async = pages.target("gallery.tree.async");
            reveal(async);
            auto async_before = trees.snapshot(async);
            key(async_before.rows[0].interaction, input::Key::right);
            key(async_before.rows[1].interaction, input::Key::right);
            reveal(pages.target("gallery.tree.deliver"));
            activate(pages.target("gallery.tree.deliver"));
            render();
            check(trees.snapshot(async).rows.size() == 4, "native synthetic Tree loads lost an independent request");
            check(pages.request(GalleryPageRoute{GalleryPageKind::introduction}), "native Tree retirement rejected");
            pages.process();
            render();
            check(trees.mounted().empty() && !captured() && !inputs.sessions().active().valid(),
                  "native Tree retirement leaked input");
            std::cout << "tree_native=passed keyed_reorder=true selection_callback=true checkbox_callback=true "
                         "virtual_10k=true synthetic_async=true retired=true\n";
        }
        if (route.component_identity == "ant.component.tree-select") {
            auto& selectors = app.services().tree_select();
            auto& trees = app.services().tree();
            const auto owner = pages.target("gallery.tree-select.multiple");
            reveal(owner);
            auto selector = selectors.snapshot(owner);
            const auto feedback_seen = [&](String text) {
                return std::ranges::any_of(app.services().text().mounted_texts(), [&](const auto& item) {
                    return app.services().text().scene_service().text_state(item.scene).content().bytes() ==
                           text.bytes();
                });
            };
            activate(pages.target("gallery.tree-select.affix"));
            render();
            check(feedback_seen(u8"已执行 TreeSelect 前缀") && !selectors.snapshot(owner).open,
                  "native TreeSelect affix lost its callback or toggled popup");

            activate(selector.selector);
            render();
            check(selectors.snapshot(owner).open, "native TreeSelect multiple did not open");
            activate(pages.target("gallery.tree-select.footer"));
            render();
            check(feedback_seen(u8"已执行 TreeSelect popup footer") && selectors.snapshot(owner).open,
                  "native TreeSelect rich popup action lost ownership");

            auto rows = trees.snapshot(selector.tree).rows;
            key(rows[1].interaction, input::Key::enter);
            check(selectors.snapshot(owner).values == TreeKeys{u8"target-a"} && selectors.snapshot(owner).open,
                  "native TreeSelect multiple callback failed");
            const auto selected_tags = selectors.snapshot(owner).tags;
            check(!selected_tags.empty(), "native TreeSelect removable tag absent");
            activate(selected_tags.front());
            render();
            check(selectors.snapshot(owner).values.empty(), "native TreeSelect tag remove callback failed");

            replace_text(selector.selector, u8"Beta");
            render();
            auto filtered = trees.snapshot(selector.tree);
            check(!filtered.rows[1].shown && filtered.rows[2].shown, "native TreeSelect search failed");
            press(input::Key::escape);
            const auto strict = pages.target("gallery.tree-select.strict");
            reveal(strict);
            selector = selectors.snapshot(strict);
            activate(selector.selector);
            render();
            rows = trees.snapshot(selector.tree).rows;
            key(rows[1].interaction, input::Key::space);
            check(selectors.snapshot(strict).values == TreeKeys{u8"strict-a"},
                  "native TreeSelect strict check conducted incorrectly");
            press(input::Key::escape);
            const auto virtual_selector = pages.target("gallery.tree-select.virtual");
            reveal(virtual_selector);
            selector = selectors.snapshot(virtual_selector);
            activate(selector.selector);
            render();
            auto window = trees.snapshot(selector.tree);
            check(window.rows.size() == 10000 &&
                      std::ranges::count(window.rows, true, &ryn::detail::TreeRowSnapshot::realized) <= 20,
                  "native TreeSelect virtual row bound failed");
            activate(pages.target("gallery.tree-select.virtual-scroll"));
            render();
            window = trees.snapshot(selector.tree);
            check(window.rows.back().realized && window.scroll > 0, "native TreeSelect virtual scrollTo failed");
            key(window.rows.back().interaction, input::Key::enter);
            check(selectors.snapshot(virtual_selector).value == String{u8"choice-9999"} &&
                      feedback_seen(u8"virtual onChange=choice-9999"),
                  "native TreeSelect virtual selection callback failed");
            activate(selector.selector);
            render();
            activate(pages.target("gallery.tree-select.virtual-reverse"));
            render();
            check(trees.snapshot(selector.tree).rows.front().key == String{u8"choice-9999"} &&
                      selectors.snapshot(virtual_selector).value == String{u8"choice-9999"},
                  "native TreeSelect dynamic topology lost accepted value");
            press(input::Key::escape);
            const auto async = pages.target("gallery.tree-select.async");
            reveal(async);
            selector = selectors.snapshot(async);
            activate(selector.selector);
            render();
            key(trees.snapshot(selector.tree).rows[0].interaction, input::Key::right);
            check(trees.snapshot(selector.tree).rows[0].loading, "native synthetic TreeSelect loader did not start");
            activate(pages.target("gallery.tree-select.async-deliver"));
            render();
            rows = trees.snapshot(selector.tree).rows;
            check(rows.size() == 2 && feedback_seen(u8"loaded=1"), "native synthetic TreeSelect load callback failed");
            key(rows[1].interaction, input::Key::enter);
            check(selectors.snapshot(async).value == String{u8"async-choice-child"} &&
                      selectors.snapshot(async).label == String{u8"Synthetic target"} &&
                      feedback_seen(u8"async onChange=async-choice-child"),
                  "native TreeSelect async child label/value callback failed");
            const auto form_selector = pages.target("gallery.tree-select.form-selector");
            reveal(form_selector);
            const auto form_before = selectors.snapshot(form_selector);
            const auto native_field = std::ranges::find_if(
                inputs.mounted_inputs(), [&](const auto& input) { return input.component == form_before.selector; });
            check(native_field != inputs.mounted_inputs().end(), "native readonly Form selector editor missing");
            const auto form_editor = *native_field;
            check(app.focus().state().focused == form_editor.interaction ||
                      app.focus().request_focus(form_editor.interaction, input::FocusModality::keyboard),
                  "native readonly Form selector focus failed");
            const auto field_area = inputs.layout_snapshot(form_editor.component).viewport;
            pointer(input::PointerAction::down, {field_area.x + 4, field_area.y + field_area.height / 2});
            render();
            check(captured() && !inputs.sessions().active().valid(),
                  "native readonly Form reset setup lost selection capture or started an editor session");
            auto& forms = app.services().form();
            const auto form_owner =
                std::ranges::find_if(forms.mounted(), [&](auto id) { return owns(app, form_selector, id); });
            check(form_owner != forms.mounted().end(), "native TreeSelect Form owner missing");
            const auto control = forms.field_control(*form_owner, FormNamePath{u8"destination"});
            check(control && bool(control->reset), "native typed Form field instance has no reset");
            control->reset();
            render();
            check(selectors.snapshot(form_selector).selector == form_before.selector &&
                      selectors.snapshot(form_selector).value == form_before.value &&
                      selectors.snapshot(form_selector).search.empty() && !selectors.snapshot(form_selector).open &&
                      !captured() && !inputs.editors().require(form_editor.editor).composition().active &&
                      app.focus().state().focused == form_editor.interaction,
                  "native Form field-instance reset lost TreeSelect authority/focus or retained input");
            check(!inputs.sessions().active().valid(), "native readonly Form reset started a text session");
            reveal(pages.target("gallery.tree-select.reset"));
            activate(pages.target("gallery.tree-select.reset"));
            render();
            check(selectors.snapshot(form_selector).value == form_before.value &&
                      app.focus().state().focused == form_editor.interaction && has_text(u8"已重置目标"),
                  "native full same-value Form reset lost accepted TreeSelect value or selector focus");
            reveal(async);
            capture_editor(async);
            previous_session = inputs.sessions().active();
            pages.dispatch(input::CompositionChanged{String{u8"退休候选"}, {}, previous_session});
            render();
            check(captured() && inputs.editors().require(previous_session.owner).composition().active,
                  "native TreeSelect retirement lacks capture/preedit");
            check(pages.request(GalleryPageRoute{GalleryPageKind::introduction}),
                  "native TreeSelect retirement rejected");
            pages.process();
            render();
            check(selectors.mounted().empty() && trees.mounted().empty() && forms.mounted().empty() && !captured() &&
                      !inputs.sessions().active().valid() && !inputs.editors().find(previous_session.owner),
                  "native TreeSelect retirement leaked input");
            std::cout << "tree_select_native=passed multiple_callback=true affix_popup_callback=true "
                         "tag_remove_callback=true controlled_search=true strict_checkbox=true "
                         "rtl=true virtual_10k=true dynamic_topology=true synthetic_async=true typed_form_reset=true "
                         "same_value_form_reset=true selector_focus=true preedit_capture_retired=true retired=true\n";
        }
        if (route.component_identity == "ant.component.space") {
            auto header_route = route;
            header_route.section = GalleryPageSection::usage;
            header_route.sample_identity.clear();
            pages.request(header_route);
            pages.process();
            render();
            const auto entry = pages.target("gallery.page.enter");
            for (const auto id : app.interactions().declaration_order()) {
                const auto* record = app.interactions().find(id);
                if (record && record->focusable && record->eligible && owns(app, record->component, entry)) {
                    static_cast<void>(app.focus().request_focus(id, input::FocusModality::keyboard));
                    break;
                }
            }
            render();
            const auto viewport = pages.target("gallery.shell.document.viewport");
            check(app.focus().state().focused.has_value() && app.focus().state().focus_visible &&
                      owns(app, app.interactions().require(*app.focus().state().focused).component, entry),
                  "Space entry did not receive visible keyboard focus");
            const auto viewport_bounds = app.nodes().require(app.components().root(viewport)).bounds;
            check(std::abs(viewport_bounds.x - snapshot.layout.document.x) < 1 &&
                      std::abs(viewport_bounds.width - snapshot.layout.document.width) < 1,
                  "Space document viewport lost its focus gutters");
            const auto header = read_frame("space-" + std::to_string(i) + "-header");
            bool focus_checked{};
            const auto background = app.components().theme_scope(root)->snapshot().alias().color_background_container;
            for (const auto& button : app.mounted_buttons()) {
                if (!owns(app, button.component, entry)) {
                    continue;
                }
                const auto& effect = app.services().surfaces().focus_effect(button.scene);
                check(effect.material.opacity > 0 && effect.material.color.alpha() > 0,
                      "Space entry has no visible focus decoration");
                auto rect = effect.geometry.shape.rect;
                rect.x += effect.geometry.translation.x;
                rect.y += effect.geometry.translation.y;
                const auto distance = effect.geometry.outline_offset + effect.geometry.outline_width / 2;
                const auto opacity = effect.material.color.alpha() * effect.material.opacity;
                const Color expected{effect.material.color.red() * opacity + background.red() * (1 - opacity),
                                     effect.material.color.green() * opacity + background.green() * (1 - opacity),
                                     effect.material.color.blue() * opacity + background.blue() * (1 - opacity)};
                for (const auto point : {runtime::Point{rect.x - distance, rect.y + rect.height / 2},
                                         runtime::Point{rect.x + rect.width + distance, rect.y + rect.height / 2},
                                         runtime::Point{rect.x + rect.width / 2, rect.y - distance},
                                         runtime::Point{rect.x + rect.width / 2, rect.y + rect.height + distance}}) {
                    check_pixel(header.get(), {point.x * events.scale, point.y * events.scale}, expected);
                }
                focus_checked = true;
            }
            check(focus_checked, "Space page has no focused entry control");
            for (const auto identity :
                 {"gallery.space.compact.small", "gallery.space.compact.middle", "gallery.space.compact.large"}) {
                const auto owner = pages.target(identity);
                const auto mounted = app.mounted_buttons();
                const auto found = std::ranges::find_if(mounted, [&](const auto& button) {
                    return owns(app, button.component, owner) &&
                           app.snapshot(button.component).type == ButtonType::Primary;
                });
                check(found != mounted.end(), "Space Compact primary action is missing");
                const auto component = found->component;
                const auto node = found->node;
                const auto lane = pages.snapshot().layout.document;
                const auto& initial = app.nodes().require(node);
                const auto target_y = initial.bounds.y + initial.translation.y + initial.bounds.height / 2;
                events.inject_scroll_at(lane.x + lane.width - 8, lane.y + lane.height / 2,
                                        -(target_y - lane.y - lane.height / 2) / 48);
                render();
                const auto offset = pages.snapshot().document.offset;
                const auto extent = pages.snapshot().document.maximum_offset;
                events.inject_scroll_at(lane.x + lane.width - 8, lane.y + lane.height / 2, -(extent + 48) / 48);
                render();
                events.inject_scroll_at(lane.x + lane.width - 8, lane.y + lane.height / 2, (extent + 48) / 48);
                render();
                check(pages.snapshot().document.offset == 0, "Space scroll roundtrip did not visit the top");
                events.inject_scroll_at(lane.x + lane.width - 8, lane.y + lane.height / 2, -offset / 48);
                render();
                check(app.components().contains(component) && pages.snapshot().page == root,
                      "Space scroll roundtrip remounted its retained content");
                const auto& current = app.nodes().require(node);
                const auto color = app.snapshot(component).presentation_background;
                const auto image = read_frame("space-" + std::to_string(i) + "-" + std::string{identity});
                const auto center_x =
                    (current.bounds.x + current.translation.x + current.bounds.width / 2) * events.scale;
                const auto center_y =
                    (current.bounds.y + current.translation.y + current.bounds.height / 2) * events.scale;
                for (int delta = -2; delta <= 2; ++delta) {
                    check_pixel(
                        image.get(),
                        {(current.bounds.x + current.translation.x + 2) * events.scale, std::floor(center_y) + delta},
                        color);
                    check_pixel(
                        image.get(),
                        {std::floor(center_x) + delta, (current.bounds.y + current.translation.y + 2) * events.scale},
                        color);
                }
            }
            auto source_route = route;
            source_route.section = GalleryPageSection::sources;
            source_route.sample_identity.clear();
            pages.request(source_route);
            pages.process();
            render();
            const auto source_owner = pages.target("gallery.page.sources.content");
            bool expanded{};
            for (const auto id : app.services().collapse().mounted()) {
                if (!owns(app, id, source_owner)) {
                    continue;
                }
                const auto panel = app.services().collapse().snapshot(id).panels.front();
                if (!panel.expanded) {
                    for (const auto interaction : app.interactions().declaration_order()) {
                        const auto* record = app.interactions().find(interaction);
                        if (record && record->focusable && record->eligible &&
                            owns(app, record->component, panel.header)) {
                            static_cast<void>(app.focus().request_focus(interaction, input::FocusModality::keyboard));
                            pages.dispatch(input::KeyboardInputEvent{input::Key::space, input::KeyAction::down});
                            pages.dispatch(input::KeyboardInputEvent{input::Key::space, input::KeyAction::up});
                            break;
                        }
                    }
                    render();
                }
                expanded = app.services().collapse().snapshot(id).panels.front().expanded;
                break;
            }
            check(expanded, "Space source disclosure did not expand");
            const auto lane = pages.snapshot().layout.document;
            events.inject_scroll_at(lane.x + lane.width - 8, lane.y + lane.height / 2,
                                    -(pages.snapshot().document.maximum_offset + 48) / 48);
            render();
            static_cast<void>(read_frame("space-" + std::to_string(i) + "-sources"));
            std::cout << "space_scroll_visual=passed case=" << i << " compact_sizes=3 focus_sides=4\n";
        }
        previous_root = root;
        previous_route = route;
        ++result.cases;
        write_gallery_page_acceptance(options.output, result);
    }
    check(result.submitted_frames >= result.cases && result.cases == options.pages.size(),
          "finite renderer missed a page");
    check(renderer.counters().quad_draws > 0 && renderer.counters().glyph_draws > 0,
          "native renderer did not issue Quad and Glyph draws");
    check(result.system_fonts, "native default font chain did not resolve any system face");
    // Focus/area coverage is required in the default matrix; an explicitly selected
    // non-input page reports that narrower scope without claiming an input check.
    if (!options.visual && std::ranges::any_of(options.pages, [](const auto& route) {
            return route.component_identity == "ant.component.input";
        })) {
        check(result.host_input_area && result.host_cursor_sync,
              "native input area/cursor synchronization was not exercised");
    }
    result.passed = true;
    write_gallery_page_acceptance(options.output, result);
    std::cout << "page_acceptance_passed=true cases=" << result.cases << " checks=" << result.checks << '\n';
}

} // namespace

int run_gallery_pages(int argc, char** argv) {
    GalleryPageOptions options;
    GalleryPageAcceptanceResult result;
    try {
        std::vector<std::string_view> arguments;
        for (int i = 1; i < argc; ++i) {
            arguments.emplace_back(argv[i]);
        }
        options = gallery_page_options(arguments, RYNUI_GALLERY_SOURCE_ROOT);
        const bool motion_disabled = options.motion_disabled || options.acceptance;
        result.platform =
#ifdef _WIN32
            "windows";
#else
            "linux";
#endif
        result.preset = RYNUI_GALLERY_PRESET;
        result.toolchain = RYNUI_GALLERY_TOOLCHAIN;
        result.configuration = RYNUI_GALLERY_CONFIGURATION;
        write_gallery_page_acceptance(options.output, result);
        ryn::detail::PlatformConfig config;
        config.title = "RynUI · Components";
        config.width = 1440;
        config.height = 900;
        config.hidden = options.visual;
        auto platform = ryn::detail::PlatformState::create(config);
        if (!platform) {
            throw std::runtime_error(platform.error->message);
        }
        if (options.visual &&
            !(SDL_GetWindowFlags(static_cast<SDL_Window*>(platform.state->window())) & SDL_WINDOW_HIDDEN)) {
            throw std::runtime_error("Visual acceptance requires a hidden native window");
        }
        auto font_runtime = font::FontRuntime::create();
        if (!font_runtime) {
            throw std::runtime_error(font_runtime.error.diagnostic);
        }
        auto fonts = std::move(font_runtime.runtime);
        const auto scale = platform.state->window_metrics().display_scale;
        ryn::detail::DefaultFontChainRequest request;
        request.raster = {14, scale};
        auto font_chain = ryn::detail::load_default_ui_font_chain(*fonts, request);
        if (!font_chain) {
            throw std::runtime_error(font_chain.diagnostic);
        }
        runtime::NodeStore nodes;
        layout::LayoutEngine layout(nodes);
        runtime::FrameRequestState frames;
        runtime::DirtyQueues dirty(nodes, &frames);
        text::TextEngine engine(*fonts);
        ryn::detail::TextSceneService scene(*fonts, engine, frames);
        ryn::detail::SdlFileServices native_files(*platform.state);
        ryn::detail::WindowComponentServices services(
            nodes, layout, dirty, scene, ryn::detail::make_default_ui_font_resolver(*fonts, font_chain, scale), frames);
        services.bind_file_services(&native_files, SDL_GetWindowID(static_cast<SDL_Window*>(platform.state->window())));
        ryn::detail::ButtonComponentHost app(services);
        ReferenceSurfaceHost surfaces(app);
        ryn::detail::InputComponentHost inputs(services, *platform.state, *platform.state);
        ryn::detail::SelectionComponentHost selections(services);
        inputs.set_display_scale(scale);
        static_cast<void>(platform.state->set_pointer_display_scale(scale));
        GalleryPagesHost pages(surfaces, inputs, frames, [&platform](const String& value) {
            static_cast<void>(platform.state->write_text(value.view()));
        });
        if (motion_disabled) {
            auto theme = pages.theme();
            theme.seed.motion = false;
            pages.set_theme(theme);
        }
        GalleryPageEvents events(*platform.state, pages, app, inputs, frames, *fonts, font_chain);
        pages.mount(events.viewport);
        result.font_source = font_chain.telemetry_source();
        result.system_fonts = font_chain.uses_system_fonts;
        ryn::detail::SdlSceneRenderer renderer(*platform.state);
        ryn::detail::SceneResources resources(renderer);
        resources.glyphs().set_sparse_upload_coalescing_limit(512 * 1024);
        GalleryPageSubmitter submitter(events, pages, app, inputs, scene, resources, renderer);
        if (options.visual) {
            submitter.offscreen_frame = options.output.parent_path() / "current.bmp";
        }
        runtime::OnDemandFrameLoop loop(frames, events, submitter, app, 10);
        frames.request_frame();
        if (options.acceptance) {
            submitter.trace = true;
            result.backend = renderer.gpu_driver();
            result.shader = renderer.shader_format();
            run_page_acceptance(options, result, events, pages, app, inputs, selections, renderer, submitter, loop,
                                frames);
            return 0;
        }
        while (!events.quit) {
            if (loop.step() == runtime::FrameLoopStep::failed) {
                throw std::runtime_error(submitter.error.empty() ? renderer.last_error() : submitter.error);
            }
        }
        if (!events.error.empty()) {
            throw std::runtime_error(events.error);
        }
        return 0;
    } catch (const std::exception& failure) {
        result.error = failure.what();
        try {
            write_gallery_page_acceptance(options.output, result);
        } catch (...) {
        }
        std::cerr << "gallery_pages_error=" << failure.what() << '\n';
        return 1;
    }
}
} // namespace rynui::example
