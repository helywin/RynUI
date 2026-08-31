#include "token_gallery_definition.hpp"
#include "gallery_document_viewport.hpp"
#include "reference_surface.hpp"

#include "component/button_component.hpp"
#include "font/font_runtime.hpp"
#include "graphics/quad_primitive.hpp"
#include "platform/default_font_chain.hpp"
#include "platform/sdl/platform_state.hpp"
#include "renderer/sdl/glyph_gpu_resources.hpp"
#include "renderer/sdl/scene_renderer.hpp"
#include "runtime/animation_frame_deadline.hpp"
#include "runtime/frame_scheduler.hpp"
#include "runtime/invalidation.hpp"
#include "text/text_engine.hpp"
#include "text/text_scene_service.hpp"

#include <algorithm>
#include <array>
#include <chrono>
#include <cstdint>
#include <filesystem>
#include <iostream>
#include <memory>
#include <optional>
#include <stdexcept>
#include <string>
#include <string_view>
#include <utility>
#include <variant>

namespace rynui::example {
namespace {

bool has_argument(int argc, char** argv, std::string_view expected) {
    for (int index = 1; index < argc; ++index) {
        if (argv[index] == expected) {
            return true;
        }
    }
    return false;
}

std::optional<float> acceptance_scale_argument(int argc, char** argv) {
    constexpr std::string_view prefix = "--acceptance-scale=";
    for (int index = 1; index < argc; ++index) {
        const std::string_view argument = argv[index];
        if (!argument.starts_with(prefix)) {
            continue;
        }
        const auto value = argument.substr(prefix.size());
        if (value == "1" || value == "1.0") {
            return 1.0F;
        }
        if (value == "1.25") {
            return 1.25F;
        }
        if (value == "1.5") {
            return 1.5F;
        }
        if (value == "2" || value == "2.0") {
            return 2.0F;
        }
        throw std::invalid_argument(
            "--acceptance-scale must be 1.0, 1.25, 1.5, or 2.0");
    }
    return std::nullopt;
}

std::filesystem::path executable_directory(char* executable) {
    return std::filesystem::absolute(executable).parent_path();
}

class GalleryEvents final : public ryn::runtime::FrameEventSource {
public:
    GalleryEvents(
        ryn::detail::PlatformState& platform,
        ryn::detail::ButtonComponentHost& application,
        ryn::runtime::FrameRequestState& frame_requests,
        GalleryDocumentViewport& document_viewport,
        ryn::runtime::Size& viewport,
        float& render_scale,
        bool fixed_render_scale,
        ryn::font::FontRuntime& fonts,
        const ryn::detail::DefaultFontChainResult& font_chain,
        const std::function<void(float)>& set_viewport_width,
        const std::function<std::optional<GalleryNavigationTarget>()>&
            take_navigation_request) noexcept
        : platform_(&platform),
          application_(&application),
          frame_requests_(&frame_requests),
          document_viewport_(&document_viewport),
          viewport_(&viewport),
          render_scale_(&render_scale),
          fixed_render_scale_(fixed_render_scale),
          fonts_(&fonts),
          font_chain_(&font_chain),
          set_viewport_width_(&set_viewport_width),
          take_navigation_request_(&take_navigation_request),
          started_(std::chrono::steady_clock::now()) {}

    ryn::animation::AnimationTime now() const noexcept override {
        return ryn::animation::AnimationTime::microseconds(
            std::chrono::duration_cast<std::chrono::microseconds>(
                std::chrono::steady_clock::now() - started_).count());
    }

    bool poll_frame_event() noexcept override { return consume(platform_->poll_events()); }
    bool wait_for_frame_event(std::uint32_t timeout_milliseconds) noexcept override {
        return consume(platform_->wait_events(timeout_milliseconds));
    }

    [[nodiscard]] bool quit_requested() const noexcept { return quit_requested_; }
    [[nodiscard]] const std::string& last_error() const noexcept { return last_error_; }
    [[nodiscard]] std::uint64_t scroll_events() const noexcept {
        return scroll_events_;
    }

private:
    bool consume(const ryn::detail::PlatformEvents& events) noexcept {
        application_->set_animation_time(now());
        quit_requested_ = quit_requested_ || events.quit_requested;
        try {
            for (const auto& event : events.input.events()) {
                std::visit([this](const auto& value) { dispatch(value); }, event);
            }
            if (const auto request = (*take_navigation_request_)()) {
                const auto anchor = request->kind
                        == GalleryNavigationTargetKind::section
                    ? document_viewport_->anchor(request->section)
                    : document_viewport_->category_anchor(request->category);
                if (anchor.has_value() && document_viewport_->jump_to(*anchor)) {
                    frame_requests_->request_frame();
                }
            }
        } catch (const std::exception& error) {
            last_error_ = error.what();
            quit_requested_ = true;
            return true;
        }
        return frame_requests_->pending()
            || events.redraw_requested
            || (events.frame_requested && events.input.empty());
    }

    void dispatch(const ryn::input::PointerInputEvent& event) {
        auto mapped = event;
        const float host_scale = platform_->display_scale();
        mapped.x = token_gallery_pointer_to_render_logical(
            event.x, host_scale, *render_scale_);
        mapped.y = token_gallery_pointer_to_render_logical(
            event.y, host_scale, *render_scale_);
        application_->pointer().dispatch(mapped);
    }

    void dispatch(const ryn::input::ScrollInputEvent& event) {
        const float ticks = event.delta_y != 0.0F
            ? -event.delta_y : -event.delta_x;
        if (document_viewport_->scroll_by(ticks * 48.0F)) {
            frame_requests_->request_frame();
        }
        ++scroll_events_;
    }

    void dispatch(const ryn::input::KeyboardInputEvent& event) {
        application_->focus().dispatch(event);
    }

    void dispatch(const ryn::input::WindowInputEvent& event) {
        switch (event.action) {
        case ryn::input::WindowInputAction::focus_gained:
            application_->set_window_active(true);
            return;
        case ryn::input::WindowInputAction::focus_lost:
            application_->set_window_active(false);
            return;
        case ryn::input::WindowInputAction::resized:
            if (event.width > 0 && event.height > 0) {
                const auto metrics = platform_->window_metrics();
                const float next_render_scale = fixed_render_scale_
                    ? *render_scale_
                    : metrics.display_scale;
                if (std::abs(next_render_scale - *render_scale_) > 0.0001F) {
                    auto resolver = ryn::detail::make_default_ui_font_resolver(
                        *fonts_, *font_chain_, next_render_scale);
                    static_cast<void>(
                        application_->text().set_font_resolver(std::move(resolver)));
                    *render_scale_ = next_render_scale;
                }
                const auto logical = token_gallery_logical_viewport(
                    metrics.pixel_width,
                    metrics.pixel_height,
                    *render_scale_);
                *viewport_ = {logical.width, logical.height};
                (*set_viewport_width_)(viewport_->width);
                frame_requests_->request_frame();
            }
            return;
        case ryn::input::WindowInputAction::invalid:
            throw std::invalid_argument("Invalid normalized Window input event");
        }
    }

    ryn::detail::PlatformState* platform_;
    ryn::detail::ButtonComponentHost* application_;
    ryn::runtime::FrameRequestState* frame_requests_;
    GalleryDocumentViewport* document_viewport_;
    ryn::runtime::Size* viewport_;
    float* render_scale_;
    bool fixed_render_scale_{};
    ryn::font::FontRuntime* fonts_;
    const ryn::detail::DefaultFontChainResult* font_chain_;
    const std::function<void(float)>* set_viewport_width_;
    const std::function<std::optional<GalleryNavigationTarget>()>*
        take_navigation_request_;
    std::chrono::steady_clock::time_point started_;
    bool quit_requested_{};
    std::uint64_t scroll_events_{};
    std::string last_error_;
};

class GallerySubmitter final : public ryn::runtime::FrameSubmitter {
public:
    GallerySubmitter(
        ryn::detail::PlatformState& platform,
        ryn::detail::ButtonComponentHost& application,
        ryn::detail::TextSceneService& text_scene,
        ryn::detail::GlyphGpuResources& glyph_resources,
        ryn::detail::SdlSceneRenderer& renderer,
        ReferenceSurfaceHost& reference_surfaces,
        GalleryDocumentViewport& document_viewport,
        ryn::runtime::NodeId document_root,
        ryn::runtime::Size& viewport,
        float& render_scale) noexcept
        : platform_(&platform),
          application_(&application),
          text_scene_(&text_scene),
          glyph_resources_(&glyph_resources),
          renderer_(&renderer),
          effect_resources_(renderer),
          reference_surfaces_(&reference_surfaces),
          document_viewport_(&document_viewport),
          document_root_(document_root),
          viewport_(&viewport),
          render_scale_(&render_scale) {}

    ryn::runtime::FrameSubmissionResult submit_frame(
        ryn::animation::AnimationTime frame_time) override {
        try {
            static_cast<void>(application_->tick_animations(frame_time));
            const ryn::runtime::Rect clip{
                16.0F,
                12.0F,
                std::max(0.0F, viewport_->width - 32.0F),
                std::max(0.0F, viewport_->height - 24.0F),
            };
            if (!document_viewport_->apply_subtree_translation(
                    document_root_, application_->nodes(), application_->dirty())) {
                last_error_ = "Token Gallery document root is stale";
                return ryn::runtime::FrameSubmissionResult::failed;
            }
            if (!application_->layout_and_synchronize(
                    *viewport_, clip, {24.0F, 20.0F}, 0.0F, true)) {
                last_error_ = "Token Gallery layout or scene sync failed";
                return ryn::runtime::FrameSubmissionResult::failed;
            }
            const auto mounted_surfaces = reference_surfaces_->mounted_surfaces();
            constexpr std::array<std::size_t, 6> section_surface_indices{
                0, 5, 6, 11, 51, 124};
            if (mounted_surfaces.size() <= section_surface_indices.back()) {
                last_error_ = "Token Gallery section surface inventory is incomplete";
                return ryn::runtime::FrameSubmissionResult::failed;
            }
            const auto& root = application_->nodes().require(document_root_);
            std::array<float, 6> anchors{};
            for (std::size_t index = 0; index < anchors.size(); ++index) {
                const auto& section = application_->nodes().require(
                    mounted_surfaces[section_surface_indices[index]].node);
                anchors[index] = std::max(0.0F, section.bounds.y - root.bounds.y);
            }
            const bool had_section_anchors = document_viewport_->anchor(
                GalleryDocumentSectionKind::header_source).has_value();
            const auto resize_anchor =
                document_viewport_->capture_resize_anchor();
            bool anchors_changed =
                document_viewport_->replace_anchors(anchors);
            constexpr std::array<std::size_t, 7> category_surface_indices{
                52, 56, 63, 70, 88, 108, 119};
            std::array<float, 7> category_anchors{};
            for (std::size_t index = 0; index < category_anchors.size(); ++index) {
                const auto& category = application_->nodes().require(
                    mounted_surfaces[category_surface_indices[index]].node);
                category_anchors[index] =
                    std::max(0.0F, category.bounds.y - root.bounds.y);
            }
            anchors_changed = document_viewport_->replace_category_anchors(
                category_anchors) || anchors_changed;
            static_cast<void>(document_viewport_->set_extents(
                clip.height, root.bounds.height));
            if (anchors_changed && had_section_anchors
                    && document_viewport_->snapshot().offset > 0.0F) {
                static_cast<void>(
                    document_viewport_->restore_resize_anchor(resize_anchor));
            }
            static_cast<void>(document_viewport_->apply_subtree_translation(
                document_root_, application_->nodes(), application_->dirty()));
            if (quad_buffer_ == nullptr) {
                quad_buffer_ = std::make_unique<ryn::graphics::QuadGpuBuffer>(
                    *renderer_, application_->button_scene().instances());
            } else {
                application_->button_scene().synchronize_gpu(*quad_buffer_);
            }
            glyph_resources_->synchronize(
                text_scene_->atlas(), text_scene_->glyph_scene().instances());
            const auto metrics = platform_->window_metrics();
            effect_resources_.synchronize(
                application_->rounded_effects(),
                {
                    static_cast<std::uint32_t>(metrics.pixel_width),
                    static_cast<std::uint32_t>(metrics.pixel_height),
                    *render_scale_,
                });
            renderer_->attach_scene(
                quad_buffer_->handle(),
                *glyph_resources_,
                application_->scene_composer().ordered_scene(),
                &effect_resources_);
            const auto result = renderer_->submit_frame(frame_time);
            if (result == ryn::runtime::FrameSubmissionResult::failed) {
                last_error_ = renderer_->last_error();
            }
            return result;
        } catch (const std::exception& error) {
            last_error_ = error.what();
            return ryn::runtime::FrameSubmissionResult::failed;
        }
    }

    [[nodiscard]] const std::string& last_error() const noexcept { return last_error_; }
    [[nodiscard]] const ryn::graphics::QuadUploadCounters& quad_uploads() const {
        if (quad_buffer_ == nullptr) {
            throw std::logic_error("Token Gallery Quad buffer was not created");
        }
        return quad_buffer_->counters();
    }
    [[nodiscard]] const ryn::detail::RoundedEffectGpuResourceCounters&
        effect_uploads() const noexcept {
        return effect_resources_.counters();
    }

private:
    ryn::detail::PlatformState* platform_;
    ryn::detail::ButtonComponentHost* application_;
    ryn::detail::TextSceneService* text_scene_;
    ryn::detail::GlyphGpuResources* glyph_resources_;
    ryn::detail::SdlSceneRenderer* renderer_;
    ryn::detail::RoundedEffectGpuResources effect_resources_;
    ReferenceSurfaceHost* reference_surfaces_;
    GalleryDocumentViewport* document_viewport_;
    ryn::runtime::NodeId document_root_;
    ryn::runtime::Size* viewport_;
    float* render_scale_;
    std::unique_ptr<ryn::graphics::QuadGpuBuffer> quad_buffer_;
    std::string last_error_;
};

} // namespace

int run_token_gallery(int argc, char** argv, TokenGalleryDefinition definition) {
    try {
        const bool animation_acceptance =
            has_argument(argc, argv, "--animation-acceptance");
        const bool motion_disabled = has_argument(argc, argv, "--motion-disabled");
        const bool reduced_motion = has_argument(argc, argv, "--reduced-motion");
        if ((motion_disabled && reduced_motion)
                || (animation_acceptance && (motion_disabled || reduced_motion))) {
            throw std::invalid_argument(
                "--motion-disabled, --reduced-motion, and --animation-acceptance "
                "are mutually exclusive");
        }
        const bool smoke_mode = has_argument(argc, argv, "--smoke")
            || animation_acceptance;
        const auto acceptance_scale = acceptance_scale_argument(argc, argv);
        const auto executable = executable_directory(argv[0]);
        constexpr ryn::runtime::Size requested_window{1280.0F, 900.0F};
        ryn::runtime::Size viewport = requested_window;

        ryn::detail::PlatformConfig platform_config;
        platform_config.title = motion_disabled
            ? "RynUI Token Gallery [Theme Motion Disabled]"
            : reduced_motion
                ? "RynUI Token Gallery [Reduced Motion]"
                : "RynUI Ant Design Token Gallery";
        platform_config.width = static_cast<int>(requested_window.width);
        platform_config.height = static_cast<int>(requested_window.height);
#if !defined(NDEBUG)
        platform_config.gpu_debug = true;
#endif
        auto platform_result = ryn::detail::PlatformState::create(platform_config);
        if (!platform_result) {
            std::cerr << "platform_error=" << platform_result.error->message << '\n';
            return 1;
        }
        auto& platform = *platform_result.state;
        if (motion_disabled) {
            definition.set_motion_enabled(false);
        }
        const auto initial_metrics = platform.window_metrics();
        float render_scale = acceptance_scale.value_or(initial_metrics.display_scale);
        const auto logical_viewport = token_gallery_logical_viewport(
            initial_metrics.pixel_width,
            initial_metrics.pixel_height,
            render_scale);
        viewport = {logical_viewport.width, logical_viewport.height};
        if (viewport.width <= 0.0F || viewport.height <= 0.0F) {
            std::cerr << "platform_error=window metrics did not provide a logical viewport\n";
            return 1;
        }
        definition.set_viewport_width(viewport.width);

        auto font_result = ryn::font::FontRuntime::create();
        if (!font_result) {
            std::cerr << "font_error=" << font_result.error.diagnostic << '\n';
            return 2;
        }
        auto fonts = std::move(font_result.runtime);
        ryn::detail::DefaultFontChainRequest font_request;
        font_request.raster = {14, render_scale};
        font_request.fallback_latin = executable / "fonts/latin.ttf";
        font_request.fallback_cjk = executable / "fonts/cjk.otf";
        const auto font_chain = ryn::detail::load_default_ui_font_chain(*fonts, font_request);
        if (!font_chain) {
            std::cerr << "font_error=" << font_chain.diagnostic << '\n';
            return 3;
        }
        auto font_resolver = ryn::detail::make_default_ui_font_resolver(
            *fonts, font_chain, render_scale);

        ryn::runtime::NodeStore nodes;
        ryn::layout::LayoutEngine layout(nodes);
        ryn::runtime::FrameRequestState frame_requests;
        ryn::runtime::DirtyQueues dirty(nodes, &frame_requests);
        ryn::text::TextEngine text_engine(*fonts);
        ryn::detail::TextSceneService text_scene(*fonts, text_engine, frame_requests);
        ryn::detail::ButtonComponentHost application(
            nodes, layout, dirty, text_scene, std::move(font_resolver), frame_requests);
        ReferenceSurfaceHost reference_surfaces(application);
        if (reduced_motion) {
            application.set_motion_preference(
                ryn::animation::MotionPreference::reduced);
        }
        reference_surfaces.mount(definition.content);
        const auto roots = application.components().root_components();
        if (roots.size() != 1) {
            throw std::logic_error(
                "Token Gallery document requires exactly one retained root");
        }
        const auto document_root = application.components().root(roots.front());
        GalleryDocumentViewport document_viewport;

        ryn::detail::SdlSceneRenderer renderer(platform, executable / "shaders");
        ryn::detail::GlyphGpuResources glyph_resources(renderer);
        GallerySubmitter submitter(
            platform,
            application,
            text_scene,
            glyph_resources,
            renderer,
            reference_surfaces,
            document_viewport,
            document_root,
            viewport,
            render_scale);
        GalleryEvents events(
            platform,
            application,
            frame_requests,
            document_viewport,
            viewport,
            render_scale,
            acceptance_scale.has_value(),
            *fonts,
            font_chain,
            definition.set_viewport_width,
            definition.take_navigation_request);
        ryn::runtime::AnimationFrameDeadlineSource animation_deadlines(
            application.animations());
        ryn::runtime::OnDemandFrameLoop loop(
            frame_requests, events, submitter, animation_deadlines, 10);

        std::size_t smoke_stage = 0;
        std::size_t automated_input_events = 0;
        const auto dispatch_acceptance_input = [&](std::size_t stage) {
            const auto mounted = application.mounted_buttons();
            if (mounted.size() <= definition.navigation_control_count + 7) {
                throw std::logic_error(
                    "animation acceptance requires the interactive Gallery cells");
            }
            const auto& hover_node = nodes.require(
                mounted[definition.navigation_control_count + 7].node);
            const auto bounds = hover_node.bounds;
            const ryn::runtime::Point inside{
                bounds.x + hover_node.translation.x + 0.5F * bounds.width,
                bounds.y + hover_node.translation.y + 0.5F * bounds.height,
            };
            switch (stage) {
            case 0:
                application.pointer().dispatch({
                    ryn::input::PointerIdentity::mouse(),
                    ryn::input::PointerAction::move,
                    ryn::input::PointerButton::none,
                    inside.x,
                    inside.y,
                });
                break;
            case 1:
                application.pointer().dispatch({
                    ryn::input::PointerIdentity::mouse(),
                    ryn::input::PointerAction::down,
                    ryn::input::PointerButton::primary,
                    inside.x,
                    inside.y,
                });
                break;
            case 2:
                application.pointer().dispatch({
                    ryn::input::PointerIdentity::mouse(),
                    ryn::input::PointerAction::up,
                    ryn::input::PointerButton::primary,
                    inside.x,
                    inside.y,
                });
                break;
            case 3:
                application.pointer().dispatch({
                    ryn::input::PointerIdentity::mouse(),
                    ryn::input::PointerAction::move,
                    ryn::input::PointerButton::none,
                    -32.0F,
                    -32.0F,
                });
                break;
            case 4:
                application.focus().dispatch({
                    ryn::input::Key::tab,
                    ryn::input::KeyAction::down,
                    ryn::input::KeyModifier::none,
                    false,
                });
                break;
            case 5:
                application.focus().dispatch({
                    ryn::input::Key::enter,
                    ryn::input::KeyAction::down,
                    ryn::input::KeyModifier::none,
                    false,
                });
                break;
            case 6:
                application.focus().dispatch({
                    ryn::input::Key::enter,
                    ryn::input::KeyAction::up,
                    ryn::input::KeyModifier::none,
                    false,
                });
                break;
            default:
                throw std::out_of_range("unknown animation acceptance input stage");
            }
            ++automated_input_events;
        };
        while (!events.quit_requested()) {
            application.set_animation_time(events.now());
            const auto elapsed = events.now_milliseconds();
            const std::size_t smoke_stage_count = animation_acceptance ? 16 : 5;
            if (smoke_mode && smoke_stage < smoke_stage_count
                    && elapsed >= 250 * (smoke_stage + 1)) {
                if (!animation_acceptance || smoke_stage >= 11) {
                    definition.smoke_step(
                        animation_acceptance ? smoke_stage - 11 : smoke_stage);
                } else if (smoke_stage == 0) {
                    definition.set_motion_enabled(false);
                } else if (smoke_stage == 1) {
                    definition.set_motion_enabled(true);
                } else if (smoke_stage == 2) {
                    application.set_motion_preference(
                        ryn::animation::MotionPreference::reduced);
                } else if (smoke_stage == 3) {
                    application.set_motion_preference(
                        ryn::animation::MotionPreference::normal);
                    if (const auto live = document_viewport.anchor(
                            GalleryDocumentSectionKind::live_samples)) {
                        static_cast<void>(document_viewport.jump_to(*live));
                        frame_requests.request_frame();
                    }
                } else {
                    dispatch_acceptance_input(smoke_stage - 4);
                }
                ++smoke_stage;
            }
            const auto step = loop.step();
            if (!events.last_error().empty()) {
                std::cerr << "input_error=" << events.last_error() << '\n';
                return 4;
            }
            if (step == ryn::runtime::FrameLoopStep::failed) {
                std::cerr << "frame_error=" << submitter.last_error() << '\n';
                return 5;
            }
            const auto completion_time = animation_acceptance ? 4'700U : 1'700U;
            if (smoke_mode && smoke_stage == smoke_stage_count
                    && elapsed >= completion_time
                    && loop.counters().idle_waits >= 20) {
                break;
            }
        }

        const auto telemetry = definition.telemetry();
        const auto platform_diagnostics = platform.event_diagnostics();
        const auto pointer_diagnostics = application.pointer().diagnostics();
        const auto focus_diagnostics = application.focus().diagnostics();
        const auto scene = application.scene_composer().diagnostics();
        const auto button_scene = application.button_scene().diagnostics();
        const auto quad = submitter.quad_uploads();
        const auto glyph = glyph_resources.counters();
        const auto effect = submitter.effect_uploads();
        const auto render = renderer.counters();
        const auto frames = loop.counters();
        const auto metrics = platform.window_metrics();
        const auto document = document_viewport.snapshot();
        std::uint64_t outer_layers = 0;
        std::uint64_t inset_layers = 0;
        std::uint64_t focus_layers = 0;
        for (const auto& instance : application.rounded_effects().packed_instances()) {
            switch (instance.geometry.kind) {
            case ryn::graphics::RoundedEffectKind::outer_shadow:
                ++outer_layers;
                break;
            case ryn::graphics::RoundedEffectKind::inset_shadow:
                ++inset_layers;
                break;
            case ryn::graphics::RoundedEffectKind::outline:
                ++focus_layers;
                break;
            }
        }
        std::uint64_t layout_passes = 0;
        for (const auto& mounted : application.mounted_buttons()) {
            layout_passes += nodes.require(mounted.node).place_count;
        }
        for (const auto& mounted : application.text().mounted_texts()) {
            layout_passes += nodes.require(text_scene.node(mounted.scene)).place_count;
        }
        for (const auto& mounted : reference_surfaces.mounted_surfaces()) {
            layout_passes += nodes.require(mounted.node).place_count;
        }

        std::cout
            << "catalog_hash=" << RYNUI_TOKEN_CATALOG_HASH
            << " gpu_driver=" << platform.gpu_driver()
            << " shader_format=" << renderer.shader_format()
            << " display_scale=" << render_scale
            << " host_display_scale=" << platform.display_scale()
            << " scale_source=" << (acceptance_scale.has_value() ? "acceptance" : "window")
            << " pixel_density=" << metrics.pixel_density
            << " window_system=" << RYNUI_WINDOW_SYSTEM
            << " window_size=" << metrics.coordinate_width << 'x' << metrics.coordinate_height
            << " pixel_size=" << metrics.pixel_width << 'x' << metrics.pixel_height
            << " viewport=" << viewport.width << 'x' << viewport.height
            << " font_source=" << font_chain.telemetry_source()
            << " font_families=" << font_chain.telemetry_families()
            << " font_rendering=" << font_chain.telemetry_rendering()
            << " stable_test_ids=" << definition.stable_test_ids.size()
            << " snapshot_identity=" << telemetry.snapshot_identity
            << " snapshot=" << telemetry.snapshot_diagnostic
            << " content_runs=" << telemetry.content_runs
            << " theme_content_runs=" << telemetry.theme_content_runs
            << " theme_updates=" << telemetry.theme_updates
            << " brand_updates=" << telemetry.brand_updates
            << " motion_updates=" << telemetry.motion_updates
            << " viewport_updates=" << telemetry.viewport_updates
            << " state_updates=" << telemetry.state_updates
            << " activations=" << telemetry.activations
            << " document_sections=" << telemetry.document_sections
            << " component_entries=" << telemetry.component_entries
            << " reference_surfaces=" << telemetry.reference_surfaces
            << " reference_content_runs=" << telemetry.reference_content_runs
            << " live_samples=" << telemetry.live_samples
            << " navigation_controls=" << definition.navigation_control_count
            << " navigation_requests=" << telemetry.navigation_requests
            << " filter_updates=" << telemetry.filter_updates
            << " reference_interactions=0"
            << " document_content_extent=" << document.content_extent
            << " document_viewport_extent=" << document.viewport_extent
            << " document_offset=" << document.offset
            << " document_maximum_offset=" << document.maximum_offset
            << " document_section="
            << gallery_document_sections()[static_cast<std::size_t>(
                document.current_section)].identity
            << " document_anchor_generation=" << document.anchor_generation
            << " scroll_events=" << events.scroll_events()
            << " input_events=" << platform_diagnostics.normalized_input_events
            << " pointer_input_events=" << pointer_diagnostics.input_events
            << " pointer_routes=" << pointer_diagnostics.routes_dispatched
            << " hover_enters=" << pointer_diagnostics.hover_enters
            << " hover_leaves=" << pointer_diagnostics.hover_leaves
            << " captures_started=" << pointer_diagnostics.captures_started
            << " captures_released=" << pointer_diagnostics.captures_released
            << " keyboard_events=" << focus_diagnostics.keyboard_events
            << " focus_traversals=" << focus_diagnostics.traversals
            << " focus_changes=" << focus_diagnostics.focus_changes
            << " keyboard_activations=" << focus_diagnostics.activations
            << " component_count=" << application.components().component_count()
            << " layout_passes=" << layout_passes
            << " scene_rebuilds=" << scene.rebuilds
            << " effect_layers=" << application.rounded_effects().live_count()
            << " outer_layers=" << outer_layers
            << " inset_layers=" << inset_layers
            << " focus_layers=" << focus_layers
            << " button_material_updates=" << button_scene.material_updates
            << " button_geometry_updates=" << button_scene.geometry_updates
            << " quad_uploads=" << quad.initial_uploads + quad.range_uploads
            << " quad_uploaded_bytes=" << quad.uploaded_bytes
            << " glyph_uploads=" << glyph.texture_uploads + glyph.buffer_uploads
            << " glyph_uploaded_bytes="
            << glyph.texture_uploaded_bytes + glyph.buffer_uploaded_bytes
            << " effect_uploads=" << effect.buffer_uploads
            << " effect_uploaded_bytes=" << effect.uploaded_bytes
            << " quad_draws=" << render.quad_draws
            << " glyph_draws=" << render.glyph_draws
            << " effect_draws=" << render.effect_draws
            << " submits=" << render.frame_submissions
            << " idle_waits=" << frames.idle_waits
            << " animation_frames=" << frames.animation_frames
            << " idle_after_animation=" << frames.idle_after_animation
            << " animation_acceptance=" << (animation_acceptance ? "true" : "false")
            << " automated_input_events=" << automated_input_events
            << " motion_mode=" << (motion_disabled
                    ? "theme-disabled"
                    : reduced_motion ? "reduced" : "normal")
            << " exit_code=0\n";

        const auto expected_stages = animation_acceptance ? 16U : 5U;
        const auto expected_theme_updates = animation_acceptance
            ? 6U
            : motion_disabled ? 5U : 4U;
        const auto expected_motion_updates = animation_acceptance
            ? 2U
            : motion_disabled ? 1U : 0U;
        return smoke_mode
                && (smoke_stage != expected_stages || telemetry.content_runs != 1
                    || telemetry.theme_updates != expected_theme_updates
                    || telemetry.motion_updates != expected_motion_updates
                    || telemetry.brand_updates != 1 || telemetry.state_updates != 2
                    || (animation_acceptance
                        && (automated_input_events != 7
                            || pointer_diagnostics.input_events < 4
                            || pointer_diagnostics.hover_enters == 0
                            || pointer_diagnostics.hover_leaves == 0
                            || pointer_diagnostics.captures_started != 1
                            || pointer_diagnostics.captures_released != 1
                            || focus_diagnostics.keyboard_events != 3
                            || focus_diagnostics.traversals == 0
                            || focus_diagnostics.activations != 1)))
            ? 6
            : 0;
    } catch (const std::exception& error) {
        std::cerr << "fatal_error=" << error.what() << '\n';
        return 7;
    }
}

} // namespace rynui::example
