#include "gallery_pages_runtime.hpp"
#include "gallery_page_acceptance.hpp"
#include "component/select_component.hpp"
#include "component/auto_complete_component.hpp"
#include "component/color_picker_component.hpp"
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
#include "component/grid_component.hpp"
#include "component/shell_layout_component.hpp"
#include "component/splitter_component.hpp"
#include <numeric>
#include "component/masonry_component.hpp"
#include "component/typography_component.hpp"
#include "gallery_pages_host.hpp"
#include "token_gallery_definition.hpp"

#include "component/input_component.hpp"
#include "component/selection_component.hpp"
#include "platform/default_font_chain.hpp"
#include "platform/sdl/platform_state.hpp"
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
                                      std::is_same_v<T, input::ScrollInputEvent>) {
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
            if (!inputs.synchronize_input_area(double(metrics.coordinate_width) / events.viewport.width,
                                               metrics.coordinate_width, metrics.coordinate_height)) {
                throw std::runtime_error("Gallery host input area synchronization failed");
            }
            if (!events.platform.synchronize_pointer_cursor()) {
                throw std::runtime_error("Gallery host cursor synchronization failed");
            }
            ++cursor_syncs;
            if (!resources.synchronize({&app.services().surfaces().instances(),
                                        text_scene.atlas(),
                                        text_scene.glyph_scene().instances(),
                                        &app.rounded_effects(),
                                        {static_cast<std::uint32_t>(metrics.pixel_width),
                                         static_cast<std::uint32_t>(metrics.pixel_height), events.scale}})) {
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

void run_page_acceptance(const GalleryPageOptions& options, GalleryPageAcceptanceResult& result,
                         GalleryPageEvents& events, GalleryPagesHost& pages, ryn::detail::ButtonComponentHost& app,
                         ryn::detail::InputComponentHost& inputs, ryn::detail::SdlSceneRenderer& renderer,
                         GalleryPageSubmitter& submitter, runtime::OnDemandFrameLoop& loop,
                         runtime::FrameRequestState& frames) {
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
                check(bool(submitter.frame_boundary), "Wayland page frame boundary was not armed");
                const auto frame_deadline =
                    std::min(deadline, std::chrono::steady_clock::now() + std::chrono::seconds(5));
                while (!submitter.frame_boundary->done()) {
                    if (std::chrono::steady_clock::now() >= frame_deadline) {
                        throw std::runtime_error("Wayland page frame callback deadline exceeded");
                    }
                    events.consume(events.platform.wait_events(10));
                    if (!events.error.empty() || events.quit) {
                        throw std::runtime_error(events.error.empty() ? "Wayland page window closed" : events.error);
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
                        throw std::runtime_error(events.error.empty() ? "Wayland page window closed" : events.error);
                    }
                }
                ++result.checks;
                write_gallery_page_acceptance(options.output, result);
                std::cout << "page_acceptance_frame_callback\n" << std::flush;
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
        resize(width);
        auto config = pages.theme();
        config.algorithms = i % 3 == 0   ? std::vector{ThemeAlgorithm::Default}
                            : i % 3 == 1 ? std::vector{ThemeAlgorithm::Dark}
                                         : std::vector{ThemeAlgorithm::Dark, ThemeAlgorithm::Compact};
        pages.set_theme(config);
        check(pages.request(route), "finite page route rejected");
        static_cast<void>(pages.process());
        const bool replaced = previous_root.valid() && (route.kind != previous_route.kind ||
                                                        route.component_identity != previous_route.component_identity ||
                                                        route.family != previous_route.family);
        if (replaced) {
            check(!app.components().contains(previous_root), "native page owner survived navigation");
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
    if (std::ranges::any_of(options.pages,
                            [](const auto& route) { return route.component_identity == "ant.component.input"; })) {
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
        auto platform = ryn::detail::PlatformState::create(config);
        if (!platform) {
            throw std::runtime_error(platform.error->message);
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
        ryn::detail::WindowComponentServices services(
            nodes, layout, dirty, scene, ryn::detail::make_default_ui_font_resolver(*fonts, font_chain, scale), frames);
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
        runtime::OnDemandFrameLoop loop(frames, events, submitter, app, 10);
        frames.request_frame();
        if (options.acceptance) {
            submitter.trace = true;
            result.backend = renderer.gpu_driver();
            result.shader = renderer.shader_format();
            run_page_acceptance(options, result, events, pages, app, inputs, renderer, submitter, loop, frames);
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
