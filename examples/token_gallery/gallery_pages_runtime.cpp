#include "gallery_pages_runtime.hpp"
#include "gallery_page_acceptance.hpp"
#include "component/select_component.hpp"
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

namespace rynui::example {
namespace {
using namespace ryn;

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
                return;
            }
            events.consume(events.platform.wait_events(10));
        }
        throw std::runtime_error("Gallery page acceptance deadline exceeded");
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
        if (route.component_identity == "ant.component.select") {
            const auto mounted = app.services().select().mounted();
            const auto selected = std::ranges::find_if(mounted, [&](const auto id) { return owns(app, id, root); });
            check(selected != mounted.end(), "native Select sample is missing");
            const auto selector = app.services().select().snapshot(*selected).selector;
            for (const auto id : app.interactions().declaration_order()) {
                const auto* record = app.interactions().find(id);
                if (record && record->focusable && record->eligible && app.interactions().branch_active(id) &&
                    owns(app, record->component, selector)) {
                    static_cast<void>(app.focus().request_focus(id, input::FocusModality::keyboard));
                    if (app.focus().state().focused != id) {
                        continue;
                    }
                    pages.dispatch(input::KeyboardInputEvent{input::Key::enter, input::KeyAction::down});
                    pages.dispatch(input::KeyboardInputEvent{input::Key::enter, input::KeyAction::up});
                    break;
                }
            }
            render();
            const auto popup = app.services().select().snapshot(*selected);
            check(popup.visible, "native Select popup did not open");
            const auto offset = pages.snapshot().document.offset;
            events.inject_scroll_at(popup.popup.x + popup.popup.width - 4, popup.popup.y + 4, -0.25F);
            check(pages.snapshot().document.offset == offset, "native popup wheel leaked to document");
            render();
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
