#include "gallery_pages_runtime.hpp"
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
        pages.dispatch(input::ScrollInputEvent{0, delta, x, y});
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
            if (!resources.synchronize({&app.services().surfaces().instances(),
                                        text_scene.atlas(),
                                        text_scene.glyph_scene().instances(),
                                        &app.rounded_effects(),
                                        {static_cast<std::uint32_t>(metrics.pixel_width),
                                         static_cast<std::uint32_t>(metrics.pixel_height), events.scale}})) {
                throw std::runtime_error(renderer.last_error());
            }
            const runtime::Rect clip{0, 0, events.viewport.width, events.viewport.height};
            static_cast<void>(app.scene_composer().build_visible_scene(app.nodes(), clip, visible));
            if (!renderer.attach_scene(resources.attach(visible))) {
                throw std::runtime_error("Gallery scene attachment failed");
            }
            renderer.set_clear_color(resolve_theme(pages.theme()).alias().color_background_container);
            return renderer.submit_frame(time);
        } catch (const std::exception& failure) {
            error = failure.what();
            return runtime::FrameSubmissionResult::failed;
        }
    }
};
} // namespace

int run_gallery_pages(int argc, char** argv) {
    try {
        bool motion_disabled = false;
        for (int i = 1; i < argc; ++i) {
            const std::string_view argument = argv[i];
            if (argument == "--motion-disabled") {
                motion_disabled = true;
            } else {
                throw std::invalid_argument("Unknown Gallery argument: " + std::string{argument});
            }
        }
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
        ryn::detail::SdlSceneRenderer renderer(*platform.state);
        ryn::detail::SceneResources resources(renderer);
        resources.glyphs().set_sparse_upload_coalescing_limit(512 * 1024);
        GalleryPageSubmitter submitter(events, pages, app, inputs, scene, resources, renderer);
        runtime::OnDemandFrameLoop loop(frames, events, submitter, app, 10);
        frames.request_frame();
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
        std::cerr << "gallery_pages_error=" << failure.what() << '\n';
        return 1;
    }
}
} // namespace rynui::example
