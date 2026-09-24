#include "token_gallery_definition.hpp"
#include "gallery_document_viewport.hpp"
#include "reference_surface.hpp"

#include "component/button_component.hpp"
#include "component/input_component.hpp"
#include "component/selection_component.hpp"
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
        ryn::detail::InputComponentHost& inputs,
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
          inputs_(&inputs),
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

    void dispatch(const ryn::input::TextCommitted& event) { static_cast<void>(inputs_->dispatch(event)); }
    void dispatch(const ryn::input::CompositionChanged& event) { static_cast<void>(inputs_->dispatch(event)); }
    void dispatch(const ryn::input::CandidatesChanged& event) { static_cast<void>(inputs_->dispatch(event)); }
    void dispatch(const ryn::input::ClipboardChanged&) {}

    void dispatch(const ryn::input::KeyboardInputEvent& event) {
        application_->focus().dispatch(event);
    }

    void dispatch(const ryn::input::WindowInputEvent& event) {
        switch (event.action) {
        case ryn::input::WindowInputAction::focus_gained:
            application_->set_window_active(true);
            inputs_->set_window_active(true);
            return;
        case ryn::input::WindowInputAction::focus_lost:
            application_->set_window_active(false);
            inputs_->set_window_active(false);
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
                    inputs_->set_display_scale(next_render_scale);
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
    ryn::detail::InputComponentHost* inputs_;
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
        ryn::detail::InputComponentHost& inputs,
        ryn::detail::TextSceneService& text_scene,
        ryn::detail::GlyphGpuResources& glyph_resources,
        ryn::detail::SdlSceneRenderer& renderer,
        const std::function<ryn::Color()>& background_color,
        ReferenceSurfaceHost& reference_surfaces,
        GalleryDocumentViewport& document_viewport,
        ryn::runtime::NodeId document_root,
        ryn::runtime::Size& viewport,
        float& render_scale) noexcept
        : platform_(&platform),
          application_(&application),
          inputs_(&inputs),
          text_scene_(&text_scene),
          glyph_resources_(&glyph_resources),
          renderer_(&renderer),
          background_color_(&background_color),
          effect_resources_(renderer),
          reference_surfaces_(&reference_surfaces),
          document_viewport_(&document_viewport),
          document_root_(document_root),
          viewport_(&viewport),
          render_scale_(&render_scale) {}

    ryn::runtime::FrameSubmissionResult submit_frame(
        ryn::animation::AnimationTime frame_time) override {
        try {
            const auto frame_started = std::chrono::steady_clock::now();
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
            const auto document_translated = std::chrono::steady_clock::now();
            if (!application_->layout_and_synchronize(
                    *viewport_, clip, {24.0F, 20.0F}, 0.0F, true)) {
                last_error_ = "Token Gallery layout or scene sync failed";
                return ryn::runtime::FrameSubmissionResult::failed;
            }
            const auto layout_synchronized = std::chrono::steady_clock::now();
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
            const float applied_offset = document_viewport_->snapshot().offset;
            static_cast<void>(document_viewport_->set_extents(
                clip.height, root.bounds.height));
            if (anchors_changed && had_section_anchors
                    && document_viewport_->snapshot().offset > 0.0F) {
                static_cast<void>(
                    document_viewport_->restore_resize_anchor(resize_anchor));
            }
            // The first sync already flushed ordinary wheel translation. Only
            // reconcile geometry again when layout changed the clamped offset.
            if (document_viewport_->snapshot().offset != applied_offset) {
                if (!document_viewport_->apply_subtree_translation(
                        document_root_, application_->nodes(), application_->dirty())
                        || !application_->layout_and_synchronize(
                            *viewport_, clip, {24.0F, 20.0F}, 0.0F, true)) {
                    last_error_ = "Token Gallery final scroll sync failed";
                    return ryn::runtime::FrameSubmissionResult::failed;
                }
                ++reconciliation_syncs_;
            }
            const auto metrics = platform_->window_metrics();
            if (!inputs_->synchronize_input_area(
                    double(metrics.coordinate_width) / viewport_->width,
                    metrics.coordinate_width, metrics.coordinate_height)) {
                last_error_ = "Token Gallery text input area update failed";
                return ryn::runtime::FrameSubmissionResult::failed;
            }
            const auto scene_synchronized = std::chrono::steady_clock::now();
            const bool batch_uploads = quad_buffer_ != nullptr
                && text_scene_->atlas().dirty_regions().empty();
            if (batch_uploads && !renderer_->begin_buffer_upload_batch()) {
                last_error_ = renderer_->last_error();
                return ryn::runtime::FrameSubmissionResult::failed;
            }
            auto quad_synchronized = scene_synchronized;
            auto glyph_synchronized = scene_synchronized;
            auto effect_synchronized = scene_synchronized;
            try {
                if (quad_buffer_ == nullptr) {
                    quad_buffer_ = std::make_unique<ryn::graphics::QuadGpuBuffer>(
                        *renderer_, application_->services().surfaces().instances());
                } else {
                    application_->services().surfaces().synchronize_gpu(*quad_buffer_);
                }
                quad_synchronized = std::chrono::steady_clock::now();
                glyph_resources_->synchronize(
                    text_scene_->atlas(), text_scene_->glyph_scene().instances());
                glyph_synchronized = std::chrono::steady_clock::now();
                effect_resources_.synchronize(
                    application_->rounded_effects(),
                    {
                        static_cast<std::uint32_t>(metrics.pixel_width),
                        static_cast<std::uint32_t>(metrics.pixel_height),
                        *render_scale_,
                    });
                effect_synchronized = std::chrono::steady_clock::now();
            } catch (...) {
                if (batch_uploads) {
                    renderer_->cancel_buffer_upload_batch();
                }
                throw;
            }
            if (batch_uploads && !renderer_->finish_buffer_upload_batch()) {
                last_error_ = renderer_->last_error();
                return ryn::runtime::FrameSubmissionResult::failed;
            }
            const auto resources_synchronized = std::chrono::steady_clock::now();
            last_visible_scene_ = application_->scene_composer().build_visible_scene(
                application_->nodes(), clip, visible_scene_);
            const auto scene_culled = std::chrono::steady_clock::now();
            renderer_->attach_scene(
                quad_buffer_->handle(),
                *glyph_resources_,
                visible_scene_,
                &effect_resources_);
            renderer_->set_clear_color((*background_color_)());
            const auto result = renderer_->submit_frame(frame_time);
            const auto frame_finished = std::chrono::steady_clock::now();
            const auto microseconds = [](auto start, auto end) {
                return static_cast<std::int64_t>(
                    std::chrono::duration_cast<std::chrono::microseconds>(end - start)
                        .count());
            };
            const auto elapsed = microseconds(frame_started, frame_finished);
            total_scene_sync_microseconds_ +=
                microseconds(frame_started, scene_synchronized);
            total_translation_microseconds_ +=
                microseconds(frame_started, document_translated);
            total_layout_microseconds_ +=
                microseconds(document_translated, layout_synchronized);
            total_anchor_input_microseconds_ +=
                microseconds(layout_synchronized, scene_synchronized);
            total_resource_sync_microseconds_ +=
                microseconds(scene_synchronized, resources_synchronized);
            total_quad_sync_microseconds_ +=
                microseconds(scene_synchronized, quad_synchronized);
            total_glyph_sync_microseconds_ +=
                microseconds(quad_synchronized, glyph_synchronized);
            total_effect_sync_microseconds_ +=
                microseconds(glyph_synchronized, effect_synchronized);
            total_upload_finish_microseconds_ +=
                microseconds(effect_synchronized, resources_synchronized);
            total_cull_microseconds_ +=
                microseconds(resources_synchronized, scene_culled);
            total_submit_microseconds_ +=
                microseconds(scene_culled, frame_finished);
            total_frame_microseconds_ += elapsed;
            max_frame_microseconds_ = std::max(max_frame_microseconds_, elapsed);
            if (sampled_frames_ < frame_samples_.size()) {
                frame_samples_[sampled_frames_++] = elapsed;
            }
            ++timed_frames_;
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
    [[nodiscard]] std::uint64_t reconciliation_syncs() const noexcept {
        return reconciliation_syncs_;
    }
    [[nodiscard]] ryn::component::VisibleSceneStats last_visible_scene() const noexcept {
        return last_visible_scene_;
    }
    [[nodiscard]] std::int64_t average_frame_microseconds() const noexcept {
        return timed_frames_ == 0 ? 0 : total_frame_microseconds_ / timed_frames_;
    }
    [[nodiscard]] std::int64_t max_frame_microseconds() const noexcept {
        return max_frame_microseconds_;
    }
    [[nodiscard]] std::array<std::int64_t, 4> average_phase_microseconds() const noexcept {
        if (timed_frames_ == 0) {
            return {};
        }
        return {
            total_scene_sync_microseconds_ / timed_frames_,
            total_resource_sync_microseconds_ / timed_frames_,
            total_cull_microseconds_ / timed_frames_,
            total_submit_microseconds_ / timed_frames_,
        };
    }
    [[nodiscard]] std::array<std::int64_t, 7> average_detail_microseconds() const noexcept {
        if (timed_frames_ == 0) {
            return {};
        }
        return {
            total_translation_microseconds_ / timed_frames_,
            total_layout_microseconds_ / timed_frames_,
            total_anchor_input_microseconds_ / timed_frames_,
            total_quad_sync_microseconds_ / timed_frames_,
            total_glyph_sync_microseconds_ / timed_frames_,
            total_effect_sync_microseconds_ / timed_frames_,
            total_upload_finish_microseconds_ / timed_frames_,
        };
    }
    [[nodiscard]] std::int64_t p95_frame_microseconds() const {
        if (sampled_frames_ == 0) {
            return 0;
        }
        auto samples = frame_samples_;
        const auto rank = (sampled_frames_ * 95 + 99) / 100 - 1;
        std::nth_element(
            samples.begin(), samples.begin() + rank,
            samples.begin() + sampled_frames_);
        return samples[rank];
    }
    void reset_frame_timings() noexcept {
        total_frame_microseconds_ = 0;
        total_scene_sync_microseconds_ = 0;
        total_translation_microseconds_ = 0;
        total_layout_microseconds_ = 0;
        total_anchor_input_microseconds_ = 0;
        total_resource_sync_microseconds_ = 0;
        total_quad_sync_microseconds_ = 0;
        total_glyph_sync_microseconds_ = 0;
        total_effect_sync_microseconds_ = 0;
        total_upload_finish_microseconds_ = 0;
        total_cull_microseconds_ = 0;
        total_submit_microseconds_ = 0;
        max_frame_microseconds_ = 0;
        timed_frames_ = 0;
        sampled_frames_ = 0;
    }
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
    ryn::detail::InputComponentHost* inputs_;
    ryn::detail::TextSceneService* text_scene_;
    ryn::detail::GlyphGpuResources* glyph_resources_;
    ryn::detail::SdlSceneRenderer* renderer_;
    const std::function<ryn::Color()>* background_color_;
    ryn::detail::RoundedEffectGpuResources effect_resources_;
    ReferenceSurfaceHost* reference_surfaces_;
    GalleryDocumentViewport* document_viewport_;
    ryn::runtime::NodeId document_root_;
    ryn::runtime::Size* viewport_;
    float* render_scale_;
    std::unique_ptr<ryn::graphics::QuadGpuBuffer> quad_buffer_;
    ryn::graphics::OrderedScene visible_scene_;
    ryn::component::VisibleSceneStats last_visible_scene_;
    std::uint64_t reconciliation_syncs_{};
    std::int64_t total_frame_microseconds_{};
    std::int64_t total_scene_sync_microseconds_{};
    std::int64_t total_translation_microseconds_{};
    std::int64_t total_layout_microseconds_{};
    std::int64_t total_anchor_input_microseconds_{};
    std::int64_t total_resource_sync_microseconds_{};
    std::int64_t total_quad_sync_microseconds_{};
    std::int64_t total_glyph_sync_microseconds_{};
    std::int64_t total_effect_sync_microseconds_{};
    std::int64_t total_upload_finish_microseconds_{};
    std::int64_t total_cull_microseconds_{};
    std::int64_t total_submit_microseconds_{};
    std::int64_t max_frame_microseconds_{};
    std::int64_t timed_frames_{};
    std::array<std::int64_t, 4096> frame_samples_{};
    std::size_t sampled_frames_{};
    std::string last_error_;
};

} // namespace

int run_token_gallery(int argc, char** argv, TokenGalleryDefinition definition) {
    try {
        const bool animation_acceptance =
            has_argument(argc, argv, "--animation-acceptance");
        const bool input_acceptance =
            has_argument(argc, argv, "--input-acceptance");
        const bool selection_acceptance =
            has_argument(argc, argv, "--selection-acceptance");
        const bool selection_dark = has_argument(argc, argv, "--selection-theme=dark");
        const bool selection_compact = has_argument(argc, argv, "--selection-theme=compact");
        const bool search_acceptance =
            has_argument(argc, argv, "--search-acceptance");
        const bool password_acceptance =
            has_argument(argc, argv, "--password-acceptance");
        const bool clear_acceptance =
            has_argument(argc, argv, "--input-clear-acceptance");
        const bool scroll_acceptance =
            has_argument(argc, argv, "--scroll-acceptance");
        const bool motion_disabled = has_argument(argc, argv, "--motion-disabled");
        const bool reduced_motion = has_argument(argc, argv, "--reduced-motion");
        const int acceptance_modes = static_cast<int>(animation_acceptance)
            + static_cast<int>(input_acceptance)
            + static_cast<int>(selection_acceptance)
            + static_cast<int>(search_acceptance)
            + static_cast<int>(password_acceptance)
            + static_cast<int>(clear_acceptance)
            + static_cast<int>(scroll_acceptance)
            + static_cast<int>(motion_disabled)
            + static_cast<int>(reduced_motion);
        if (acceptance_modes > 1) {
            throw std::invalid_argument(
                "--motion-disabled, --reduced-motion, --animation-acceptance, and "
                "--input-acceptance, --selection-acceptance, --search-acceptance, --password-acceptance, --input-clear-acceptance, "
                "and --scroll-acceptance "
                "are mutually exclusive");
        }
        if ((selection_dark && selection_compact)
            || ((selection_dark || selection_compact) && !selection_acceptance
                && !password_acceptance && !clear_acceptance)) {
            throw std::invalid_argument(
                "selection theme requires one selection acceptance mode");
        }
        const bool smoke_mode = has_argument(argc, argv, "--smoke")
            || animation_acceptance || input_acceptance || selection_acceptance
            || search_acceptance || password_acceptance || clear_acceptance;
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
        if (selection_dark) definition.smoke_step(0);
        if (selection_compact) definition.smoke_step(1);
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
        ryn::detail::WindowComponentServices services(
            nodes, layout, dirty, text_scene, std::move(font_resolver), frame_requests);
        ryn::detail::ButtonComponentHost application(services);
        ReferenceSurfaceHost reference_surfaces(application);
        ryn::detail::InputComponentHost inputs(services, platform, platform);
        ryn::detail::SelectionComponentHost selections(services);
        inputs.set_display_scale(render_scale);
        if (reduced_motion) {
            application.set_motion_preference(
                ryn::animation::MotionPreference::reduced);
        }
        reference_surfaces.mount(definition.content, &inputs);
        const auto roots = application.components().root_components();
        if (roots.size() != 1) {
            throw std::logic_error(
                "Token Gallery document requires exactly one retained root");
        }
        const auto document_root = application.components().root(roots.front());
        GalleryDocumentViewport document_viewport;

        ryn::detail::SdlSceneRenderer renderer(platform, executable / "shaders");
        ryn::detail::GlyphGpuResources glyph_resources(renderer);
        glyph_resources.set_sparse_upload_coalescing_limit(512 * 1024);
        GallerySubmitter submitter(
            platform,
            application,
            inputs,
            text_scene,
            glyph_resources,
            renderer,
            definition.background_color,
            reference_surfaces,
            document_viewport,
            document_root,
            viewport,
            render_scale);
        GalleryEvents events(
            platform,
            application,
            inputs,
            frame_requests,
            document_viewport,
            viewport,
            render_scale,
            acceptance_scale.has_value(),
            *fonts,
            font_chain,
            definition.set_viewport_width,
            definition.take_navigation_request);
        auto& animation_deadlines = application;
        ryn::runtime::OnDemandFrameLoop loop(
            frame_requests, events, submitter, animation_deadlines, 10);

        std::size_t smoke_stage = 0;
        std::size_t scroll_stage = 0;
        std::uint64_t scroll_started_milliseconds = 0;
        std::uint64_t scroll_finished_milliseconds = 0;
        std::uint64_t rasterizations_before_scroll = 0;
        std::size_t automated_input_events = 0;
        bool input_latin = false;
        bool input_selection = false;
        bool input_clipboard = false;
        bool input_undo = false;
        bool input_redo = false;
        bool input_theme_status = false;
        bool input_caret_active = false;
        bool input_caret_idle = false;
        bool selection_keyboard = false;
        bool selection_pointer = false;
        bool selection_blocked = false;
        bool selection_scroll = false;
        bool search_keyboard = false;
        bool search_pointer = false;
        bool search_blocked = false;
        bool search_text = false;
        bool search_scroll = false;
        bool password_scroll = false;
        bool password_hidden = false;
        bool password_pointer = false;
        bool password_keyboard = false;
        bool password_disabled = false;
        bool clear_scroll = false;
        bool clear_pointer = false;
        bool clear_keyboard = false;
        bool clear_disabled = false;
        std::uint64_t input_initial_theme_identity = 0;
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
        const auto dispatch_input_acceptance = [&](std::size_t stage) {
            const auto mounted = inputs.mounted_inputs();
            if (mounted.size() < 2) {
                throw std::logic_error(
                    "input acceptance requires the controlled and uncontrolled Gallery Inputs");
            }
            const auto controlled = mounted.front();
            auto& editor = inputs.editors().require(controlled.editor);
            const auto key = [&](ryn::input::Key value,
                                 ryn::input::KeyModifier modifier = ryn::input::KeyModifier::none) {
                application.focus().dispatch({
                    value,
                    ryn::input::KeyAction::down,
                    modifier,
                    false,
                });
                ++automated_input_events;
            };
            switch (stage) {
            case 0:
                input_initial_theme_identity = application.components()
                    .theme_scope(controlled.component)->snapshot().identity();
                application.set_motion_preference(
                    ryn::animation::MotionPreference::reduced);
                definition.smoke_step(0);
                if (const auto live = document_viewport.anchor(
                        GalleryDocumentSectionKind::live_samples)) {
                    static_cast<void>(document_viewport.jump_to(*live));
                    frame_requests.request_frame();
                }
                break;
            case 1:
                application.set_motion_preference(
                    ryn::animation::MotionPreference::normal);
                if (!application.focus().request_focus(
                        controlled.interaction, ryn::input::FocusModality::keyboard)) {
                    throw std::logic_error("input acceptance could not focus the controlled Input");
                }
                input_caret_active = inputs.sessions().active().valid()
                    && inputs.next_caret_deadline().has_value();
                break;
            case 2: {
                const auto result = inputs.dispatch(ryn::input::TextCommitted{
                    ryn::String{u8"RynUI"}, inputs.sessions().active()});
                input_latin = static_cast<bool>(result)
                    && editor.value() == std::string_view{"RynUI"}
                    && inputs.status(controlled.component) == ryn::InputStatus::Warning;
                ++automated_input_events;
                break;
            }
            case 3: {
                key(ryn::input::Key::a, ryn::input::KeyModifier::control);
                const auto selection = editor.selection();
                input_selection = std::min(selection.anchor, selection.caret) == 0
                    && std::max(selection.anchor, selection.caret)
                        == editor.value().size();
                break;
            }
            case 4:
                key(ryn::input::Key::c, ryn::input::KeyModifier::control);
                break;
            case 5:
                key(ryn::input::Key::x, ryn::input::KeyModifier::control);
                input_clipboard = editor.value().empty();
                break;
            case 6:
                key(ryn::input::Key::v, ryn::input::KeyModifier::control);
                input_clipboard = input_clipboard
                    && editor.value() == std::string_view{"RynUI"};
                editor.break_history_merge();
                break;
            case 7:
                static_cast<void>(inputs.dispatch(ryn::input::TextCommitted{
                    ryn::String{u8"X"}, inputs.sessions().active()}));
                ++automated_input_events;
                break;
            case 8:
                key(ryn::input::Key::z, ryn::input::KeyModifier::control);
                input_undo = editor.value() == std::string_view{"RynUI"};
                break;
            case 9:
                key(ryn::input::Key::y, ryn::input::KeyModifier::control);
                input_redo = editor.value() == std::string_view{"RynUIX"};
                break;
            case 10:
                input_theme_status = inputs.status(controlled.component)
                        == ryn::InputStatus::Warning
                    && application.components().theme_scope(controlled.component)
                           ->snapshot().identity() != input_initial_theme_identity;
                break;
            case 11:
                key(ryn::input::Key::enter);
                input_caret_active = input_caret_active
                    && inputs.next_caret_deadline().has_value();
                break;
            case 12:
                static_cast<void>(application.focus().clear_focus());
                input_caret_idle = !inputs.sessions().active().valid()
                    && !inputs.next_caret_deadline().has_value();
                break;
            default:
                throw std::out_of_range("unknown Input acceptance stage");
            }
            std::cout << "input_acceptance_stage=" << stage << '\n' << std::flush;
        };
        const auto dispatch_selection_acceptance = [&](std::size_t stage) {
            const auto mounted = selections.mounted();
            if (mounted.size() != 12)
                throw std::logic_error("selection acceptance requires twelve Gallery controls");
            const auto key = [&](ryn::input::Key value, ryn::input::KeyAction action) {
                application.focus().dispatch({value, action,
                    ryn::input::KeyModifier::none, false});
                ++automated_input_events;
            };
            switch (stage) {
            case 0:
                selection_scroll = document_viewport.scroll_to(
                    document_viewport.snapshot().maximum_offset);
                frame_requests.request_frame();
                break;
            case 1:
                if (!application.focus().request_focus(mounted[0].interaction,
                        ryn::input::FocusModality::keyboard))
                    throw std::logic_error("selection acceptance could not focus Switch");
                key(ryn::input::Key::enter, ryn::input::KeyAction::down);
                key(ryn::input::Key::enter, ryn::input::KeyAction::up);
                selection_keyboard = !selections.snapshot(mounted[0].component).checked;
                key(ryn::input::Key::space, ryn::input::KeyAction::down);
                key(ryn::input::Key::space, ryn::input::KeyAction::up);
                selection_keyboard = selection_keyboard
                    && selections.snapshot(mounted[0].component).checked;
                key(ryn::input::Key::tab, ryn::input::KeyAction::down);
                selection_keyboard = selection_keyboard
                    && application.focus().state().focused == mounted[1].interaction;
                break;
            case 2:
                if (!application.focus().request_focus(mounted[4].interaction,
                        ryn::input::FocusModality::keyboard))
                    throw std::logic_error("selection acceptance could not focus Checkbox");
                key(ryn::input::Key::enter, ryn::input::KeyAction::down);
                key(ryn::input::Key::enter, ryn::input::KeyAction::up);
                selection_keyboard = selection_keyboard
                    && !selections.snapshot(mounted[4].component).checked;
                key(ryn::input::Key::space, ryn::input::KeyAction::down);
                key(ryn::input::Key::space, ryn::input::KeyAction::up);
                selection_keyboard = selection_keyboard
                    && selections.snapshot(mounted[4].component).checked;
                key(ryn::input::Key::tab, ryn::input::KeyAction::down);
                selection_keyboard = selection_keyboard
                    && application.focus().state().focused == mounted[5].interaction;
                break;
            case 3: {
                const auto& node = nodes.require(mounted[1].node);
                const ryn::runtime::Point center{
                    node.bounds.x + node.translation.x + node.bounds.width / 2.0F,
                    node.bounds.y + node.translation.y + node.bounds.height / 2.0F};
                application.pointer().dispatch({ryn::input::PointerIdentity::mouse(),
                    ryn::input::PointerAction::down, ryn::input::PointerButton::primary,
                    center.x, center.y});
                application.pointer().dispatch({ryn::input::PointerIdentity::mouse(),
                    ryn::input::PointerAction::up, ryn::input::PointerButton::primary,
                    center.x, center.y});
                automated_input_events += 2;
                selection_pointer = !selections.snapshot(mounted[1].component).checked;
                break;
            }
            case 4: {
                const auto click = [&](const ryn::detail::MountedSelectionComponent& item) {
                    const auto& node = nodes.require(item.node);
                    const ryn::runtime::Point center{
                        node.bounds.x + node.translation.x + node.bounds.width / 2.0F,
                        node.bounds.y + node.translation.y + node.bounds.height / 2.0F};
                    application.pointer().dispatch({ryn::input::PointerIdentity::mouse(),
                        ryn::input::PointerAction::down, ryn::input::PointerButton::primary,
                        center.x, center.y});
                    application.pointer().dispatch({ryn::input::PointerIdentity::mouse(),
                        ryn::input::PointerAction::up, ryn::input::PointerButton::primary,
                        center.x, center.y});
                    automated_input_events += 2;
                };
                click(mounted[2]);
                click(mounted[3]);
                click(mounted[7]);
                selection_blocked = !selections.snapshot(mounted[2].component).checked
                    && !selections.snapshot(mounted[3].component).checked
                    && !selections.snapshot(mounted[7].component).checked;
                if (!application.focus().request_focus(mounted[3].interaction,
                        ryn::input::FocusModality::keyboard))
                    throw std::logic_error("selection acceptance could not focus loading Switch");
                key(ryn::input::Key::space, ryn::input::KeyAction::down);
                key(ryn::input::Key::space, ryn::input::KeyAction::up);
                selection_blocked = selection_blocked
                    && !selections.snapshot(mounted[3].component).checked;
                if (!application.focus().request_focus(mounted[9].interaction,
                        ryn::input::FocusModality::keyboard))
                    throw std::logic_error("selection acceptance could not focus RadioGroup");
                key(ryn::input::Key::tab, ryn::input::KeyAction::down);
                selection_keyboard = selection_keyboard
                    && application.focus().state().focused == mounted[10].interaction;
                key(ryn::input::Key::enter, ryn::input::KeyAction::down);
                key(ryn::input::Key::enter, ryn::input::KeyAction::up);
                selection_keyboard = selection_keyboard
                    && selections.snapshot(mounted[9].component).checked
                    && !selections.snapshot(mounted[10].component).checked;
                key(ryn::input::Key::space, ryn::input::KeyAction::down);
                key(ryn::input::Key::space, ryn::input::KeyAction::up);
                selection_keyboard = selection_keyboard
                    && !selections.snapshot(mounted[9].component).checked
                    && selections.snapshot(mounted[10].component).checked;
                click(mounted[9]);
                selection_pointer = selection_pointer
                    && selections.snapshot(mounted[9].component).checked
                    && !selections.snapshot(mounted[10].component).checked;
                click(mounted[11]);
                click(mounted[8]);
                selection_blocked = selection_blocked
                    && !selections.snapshot(mounted[11].component).checked
                    && selections.snapshot(mounted[8].component).checked;
                break;
            }
            default:
                throw std::out_of_range("unknown selection acceptance stage");
            }
            std::cout << "selection_acceptance_stage=" << stage << '\n' << std::flush;
        };
        const auto dispatch_search_acceptance = [&](std::size_t stage) {
            const auto mounted_inputs = inputs.mounted_inputs();
            const auto mounted_buttons = application.mounted_buttons();
            if (mounted_inputs.size() != 9 || mounted_buttons.size() < 5)
                throw std::logic_error("search acceptance requires five Gallery Search cells");
            const auto& field = mounted_inputs[2];
            const auto& first_button = mounted_buttons[mounted_buttons.size() - 5];
            const auto& large_button = mounted_buttons[mounted_buttons.size() - 4];
            const auto& loading_button = mounted_buttons[mounted_buttons.size() - 2];
            const auto& disabled_button = mounted_buttons[mounted_buttons.size() - 1];
            const auto key = [&](ryn::input::Key value, ryn::input::KeyAction action) {
                application.focus().dispatch({value, action,
                    ryn::input::KeyModifier::none, false});
                ++automated_input_events;
            };
            const auto click = [&](const ryn::detail::MountedButtonComponent& item) {
                const auto& node = nodes.require(item.node);
                const ryn::runtime::Point center{
                    node.bounds.x + node.translation.x + node.bounds.width / 2.0F,
                    node.bounds.y + node.translation.y + node.bounds.height / 2.0F};
                application.pointer().dispatch({ryn::input::PointerIdentity::mouse(),
                    ryn::input::PointerAction::down, ryn::input::PointerButton::primary,
                    center.x, center.y});
                application.pointer().dispatch({ryn::input::PointerIdentity::mouse(),
                    ryn::input::PointerAction::up, ryn::input::PointerButton::primary,
                    center.x, center.y});
                automated_input_events += 2;
            };
            switch (stage) {
            case 0: {
                const auto& node = nodes.require(field.node);
                search_scroll = document_viewport.scroll_to(
                    node.bounds.y - document_viewport.snapshot().viewport_extent / 2.0F);
                frame_requests.request_frame();
                break;
            }
            case 1: {
                if (!application.focus().request_focus(field.interaction,
                        ryn::input::FocusModality::keyboard))
                    throw std::logic_error("search acceptance could not focus Search Input");
                const auto stamp = inputs.sessions().active();
                const auto result = inputs.dispatch(ryn::input::TextCommitted{
                    ryn::String{u8"RynUI 中文"}, stamp});
                ++automated_input_events;
                search_text = static_cast<bool>(result)
                    && inputs.editors().require(field.editor).value()
                        == ryn::String{u8"RynUI 中文"}.bytes()
                    && definition.telemetry().search_submits == 0;
                break;
            }
            case 2:
                key(ryn::input::Key::enter, ryn::input::KeyAction::down);
                key(ryn::input::Key::enter, ryn::input::KeyAction::up);
                search_keyboard = definition.telemetry().search_submits == 1;
                key(ryn::input::Key::tab, ryn::input::KeyAction::down);
                search_keyboard = search_keyboard
                    && application.focus().state().focused == first_button.interaction;
                key(ryn::input::Key::space, ryn::input::KeyAction::down);
                key(ryn::input::Key::space, ryn::input::KeyAction::up);
                search_keyboard = search_keyboard
                    && definition.telemetry().search_submits == 2;
                break;
            case 3:
                click(large_button);
                search_pointer = definition.telemetry().search_submits == 3;
                break;
            case 4:
                click(loading_button);
                click(disabled_button);
                search_blocked = definition.telemetry().search_submits == 3;
                break;
            case 5:
                if (!application.focus().request_focus(field.interaction,
                        ryn::input::FocusModality::keyboard))
                    throw std::logic_error("search acceptance could not restore Search focus");
                break;
            default:
                throw std::out_of_range("unknown Search acceptance stage");
            }
            std::cout << "search_acceptance_stage=" << stage << '\n' << std::flush;
        };
        const auto dispatch_clear_acceptance = [&](std::size_t stage) {
            const auto mounted = inputs.mounted_inputs();
            if(mounted.size() != 9) throw std::logic_error("Input clear acceptance requires Gallery Input cells");
            const auto& field = mounted[1];
            const auto action = [&] {
                for(const auto id : application.interactions().declaration_order()) {
                    const auto* record = application.interactions().find(id);
                    if(record && record->parent == field.interaction && id != field.interaction)
                        return id;
                }
                throw std::logic_error("Input clear action absent");
            }();
            const auto click = [&] {
                const auto& node = nodes.require(application.interactions().require(action).node);
                const float x = node.bounds.x + node.translation.x + node.bounds.width / 2.0F;
                const float y = node.bounds.y + node.translation.y + node.bounds.height / 2.0F;
                application.pointer().dispatch({ryn::input::PointerIdentity::mouse(),
                    ryn::input::PointerAction::down, ryn::input::PointerButton::primary, x, y});
                application.pointer().dispatch({ryn::input::PointerIdentity::mouse(),
                    ryn::input::PointerAction::up, ryn::input::PointerButton::primary, x, y});
                automated_input_events += 2;
            };
            switch(stage) {
            case 0: {
                const auto& node = nodes.require(field.node);
                clear_scroll = document_viewport.scroll_to(
                    node.bounds.y - document_viewport.snapshot().viewport_extent / 2.0F);
                frame_requests.request_frame();
                break;
            }
            case 1: {
                if(!application.focus().request_focus(field.interaction,
                    ryn::input::FocusModality::keyboard))
                    throw std::logic_error("Input clear acceptance could not focus field");
                const auto stamp = inputs.sessions().active();
                click();
                clear_pointer = inputs.editors().require(field.editor).value().empty()
                    && application.focus().state().focused == field.interaction
                    && inputs.sessions().active() == stamp
                    && !application.interactions().require(action).eligible;
                break;
            }
            case 2: {
                const auto stamp = inputs.sessions().active();
                const auto result = inputs.dispatch(ryn::input::TextCommitted{
                    ryn::String{u8"重写"}, stamp});
                ++automated_input_events;
                if(!application.focus().request_focus(action, ryn::input::FocusModality::keyboard))
                    throw std::logic_error("Input clear acceptance could not focus action");
                application.focus().dispatch({ryn::input::Key::space, ryn::input::KeyAction::down,
                    ryn::input::KeyModifier::none, false});
                application.focus().dispatch({ryn::input::Key::space, ryn::input::KeyAction::up,
                    ryn::input::KeyModifier::none, false});
                automated_input_events += 2;
                clear_keyboard = static_cast<bool>(result)
                    && inputs.editors().require(field.editor).value().empty()
                    && !application.interactions().require(action).eligible;
                break;
            }
            case 3: {
                if(!application.focus().request_focus(field.interaction,
                    ryn::input::FocusModality::keyboard))
                    throw std::logic_error("Input clear acceptance could not restore field focus");
                const auto stamp = inputs.sessions().active();
                if(!inputs.dispatch(ryn::input::TextCommitted{ryn::String{u8"禁用"}, stamp}))
                    throw std::logic_error("Input clear acceptance could not seed disabled field");
                ++automated_input_events;
                definition.set_clear_disabled(true);
                break;
            }
            case 4: {
                const auto before = std::string{inputs.editors().require(field.editor).value()};
                click();
                clear_disabled = !application.interactions().require(action).eligible
                    && inputs.editors().require(field.editor).value() == before;
                break;
            }
            case 5:
                definition.set_clear_disabled(false);
                if(!application.focus().request_focus(field.interaction,
                    ryn::input::FocusModality::keyboard))
                    throw std::logic_error("Input clear acceptance could not refocus field");
                break;
            default:
                throw std::out_of_range("unknown Input clear acceptance stage");
            }
            std::cout << "input_clear_acceptance_stage=" << stage << '\n' << std::flush;
        };
        const auto dispatch_password_acceptance = [&](std::size_t stage) {
            const auto mounted = inputs.mounted_inputs();
            if(mounted.size() != 9) throw std::logic_error("Password acceptance requires Gallery Password cells");
            const auto& field = mounted[7];
            const auto& disabled = mounted[8];
            const auto toggle = [&](ryn::input::InteractionId parent) {
                for(const auto id : application.interactions().declaration_order()) {
                    const auto* record = application.interactions().find(id);
                    if(record && record->parent == parent && id != parent) return id;
                }
                throw std::logic_error("Password toggle interaction absent");
            };
            const auto click = [&](ryn::input::InteractionId id) {
                const auto& node = nodes.require(application.interactions().require(id).node);
                const float x = node.bounds.x + node.translation.x + node.bounds.width / 2.0F;
                const float y = node.bounds.y + node.translation.y + node.bounds.height / 2.0F;
                application.pointer().dispatch({ryn::input::PointerIdentity::mouse(),
                    ryn::input::PointerAction::down, ryn::input::PointerButton::primary, x, y});
                application.pointer().dispatch({ryn::input::PointerIdentity::mouse(),
                    ryn::input::PointerAction::up, ryn::input::PointerButton::primary, x, y});
                automated_input_events += 2;
            };
            switch(stage) {
            case 0: {
                const auto& node = nodes.require(field.node);
                password_scroll = document_viewport.scroll_to(
                    node.bounds.y - document_viewport.snapshot().viewport_extent / 2.0F);
                frame_requests.request_frame();
                break;
            }
            case 1: {
                if(!application.focus().request_focus(field.interaction,
                    ryn::input::FocusModality::keyboard))
                    throw std::logic_error("Password acceptance could not focus field");
                const auto display = inputs.display_snapshot(field.component);
                const auto scene_text = inputs.text_scene(field.component);
                password_hidden = display.text.find("RynUI") == std::string_view::npos
                    && text_scene.text_state(scene_text).content().bytes().find("RynUI") == std::string_view::npos
                    && inputs.sessions().active().owner == field.editor;
                break;
            }
            case 2: {
                const auto stamp = inputs.sessions().active();
                const auto result = inputs.dispatch(ryn::input::CompositionChanged{
                    ryn::String{u8"ni"}, {2, 0}, stamp});
                ++automated_input_events;
                click(toggle(field.interaction));
                password_pointer = static_cast<bool>(result)
                    && application.focus().state().focused == field.interaction
                    && inputs.editors().require(field.editor).composition().active
                    && inputs.sessions().active() == stamp
                    && inputs.display_snapshot(field.component).text.find("ni") != std::string_view::npos;
                break;
            }
            case 3: {
                const auto stamp = inputs.sessions().active();
                const auto result = inputs.dispatch(ryn::input::TextCommitted{
                    ryn::String{u8"你"}, stamp});
                ++automated_input_events;
                const auto toggler = toggle(field.interaction);
                if(!application.focus().request_focus(toggler, ryn::input::FocusModality::keyboard))
                    throw std::logic_error("Password acceptance could not focus toggle");
                application.focus().dispatch({ryn::input::Key::space, ryn::input::KeyAction::down,
                    ryn::input::KeyModifier::none, false});
                application.focus().dispatch({ryn::input::Key::space, ryn::input::KeyAction::up,
                    ryn::input::KeyModifier::none, false});
                automated_input_events += 2;
                password_keyboard = static_cast<bool>(result)
                    && inputs.display_snapshot(field.component).text.find("RynUI") == std::string_view::npos;
                break;
            }
            case 4: {
                const auto before = inputs.display_snapshot(disabled.component).text;
                click(toggle(disabled.interaction));
                password_disabled = !application.interactions().require(toggle(disabled.interaction)).eligible
                    && inputs.display_snapshot(disabled.component).text == before;
                break;
            }
            case 5:
                if(!application.focus().request_focus(field.interaction,
                    ryn::input::FocusModality::keyboard))
                    throw std::logic_error("Password acceptance could not restore field focus");
                break;
            default:
                throw std::out_of_range("unknown Password acceptance stage");
            }
            std::cout << "password_acceptance_stage=" << stage << '\n' << std::flush;
        };
        while (!events.quit_requested()) {
            application.set_animation_time(events.now());
            const auto elapsed = events.now_milliseconds();
            if (scroll_acceptance && scroll_stage < 240
                    && elapsed >= 250 + 4 * scroll_stage) {
                if (scroll_stage == 0) {
                    rasterizations_before_scroll = fonts->counters().rasterizations;
                    submitter.reset_frame_timings();
                    scroll_started_milliseconds = elapsed;
                }
                const bool changed = scroll_stage == 239
                    ? document_viewport.scroll_to(
                        document_viewport.snapshot().maximum_offset)
                    : document_viewport.scroll_by(48.0F);
                if (changed) {
                    frame_requests.request_frame();
                }
                ++scroll_stage;
            }
            const std::size_t smoke_stage_count = search_acceptance || password_acceptance || clear_acceptance
                ? 6 : selection_acceptance ? 5 : input_acceptance ? 13
                : animation_acceptance ? 16 : 5;
            if (smoke_mode && smoke_stage < smoke_stage_count
                    && elapsed >= 250 * (smoke_stage + 1)) {
                if (clear_acceptance) {
                    dispatch_clear_acceptance(smoke_stage);
                } else if (password_acceptance) {
                    dispatch_password_acceptance(smoke_stage);
                } else if (search_acceptance) {
                    dispatch_search_acceptance(smoke_stage);
                } else if (selection_acceptance) {
                    dispatch_selection_acceptance(smoke_stage);
                } else if (input_acceptance) {
                    dispatch_input_acceptance(smoke_stage);
                } else if (!animation_acceptance || smoke_stage >= 11) {
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
            if (scroll_acceptance && scroll_stage == 240
                    && scroll_finished_milliseconds == 0) {
                scroll_finished_milliseconds = events.now_milliseconds();
            }
            if (!events.last_error().empty()) {
                std::cerr << "input_error=" << events.last_error() << '\n';
                return 4;
            }
            if (step == ryn::runtime::FrameLoopStep::failed) {
                std::cerr << "frame_error=" << submitter.last_error() << '\n';
                return 5;
            }
            const auto completion_time = search_acceptance || password_acceptance || clear_acceptance
                ? 4'000U : selection_acceptance ? 5'000U : input_acceptance ? 4'000U
                : animation_acceptance ? 4'700U : 1'700U;
            const bool acceptance_complete = search_acceptance || selection_acceptance
                || password_acceptance || clear_acceptance
                ? true : input_acceptance
                ? input_caret_idle && !inputs.next_caret_deadline().has_value()
                : loop.counters().idle_waits >= 20;
            if (smoke_mode && smoke_stage == smoke_stage_count
                    && elapsed >= completion_time && acceptance_complete) {
                break;
            }
            if (scroll_acceptance && scroll_stage == 240
                    && elapsed >= 1'800) {
                break;
            }
        }

        const auto telemetry = definition.telemetry();
        const auto platform_diagnostics = platform.event_diagnostics();
        const auto pointer_diagnostics = application.pointer().diagnostics();
        const auto focus_diagnostics = application.focus().diagnostics();
        const auto scene = application.scene_composer().diagnostics();
        const auto retained_surfaces = application.services().surfaces().diagnostics();
        const auto quad = submitter.quad_uploads();
        const auto glyph = glyph_resources.counters();
        const auto effect = submitter.effect_uploads();
        const auto render = renderer.counters();
        const auto phase_times = submitter.average_phase_microseconds();
        const auto detail_times = submitter.average_detail_microseconds();
        const auto font_counters = fonts->counters();
        const auto frames = loop.counters();
        const auto metrics = platform.window_metrics();
        const auto document = document_viewport.snapshot();
        const auto document_diagnostics = document_viewport.diagnostics();
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

        const auto expected_stages = search_acceptance || password_acceptance || clear_acceptance
            ? 6U : selection_acceptance ? 5U : input_acceptance ? 13U
            : animation_acceptance ? 16U : 5U;
        const auto expected_theme_updates = search_acceptance
            ? 0U : password_acceptance || selection_acceptance || clear_acceptance
            ? (selection_dark || selection_compact ? 1U : 0U) : input_acceptance
            ? 1U : animation_acceptance ? 6U
            : motion_disabled ? 5U : 4U;
        const auto expected_motion_updates = search_acceptance || password_acceptance || clear_acceptance
            ? 0U : selection_acceptance
            ? 0U : input_acceptance
            ? 0U : animation_acceptance ? 2U
            : motion_disabled ? 1U : 0U;
        const bool scroll_failed = scroll_acceptance
            && (scroll_stage != 240 || document.maximum_offset <= 0.0F
                || document.offset != document.maximum_offset
                || document_diagnostics.translation_passes < 2
                || submitter.reconciliation_syncs() != 0
                || submitter.last_visible_scene().fragments_visible
                    >= submitter.last_visible_scene().fragments_considered
                || telemetry.content_runs != 1
                || render.frame_submissions < 2);
        const bool smoke_failed = scroll_failed || (smoke_mode
            && (smoke_stage != expected_stages || telemetry.content_runs != 1
                || telemetry.theme_updates != expected_theme_updates
                || telemetry.motion_updates != expected_motion_updates
                || (!input_acceptance && !selection_acceptance && !search_acceptance
                    && !password_acceptance && !clear_acceptance
                    && (telemetry.brand_updates != 1 || telemetry.state_updates != 2))
                || (search_acceptance
                    && (!search_scroll || !search_text || !search_keyboard
                        || !search_pointer || !search_blocked
                        || telemetry.search_submits != 3
                        || telemetry.live_samples != 33))
                || (selection_acceptance
                    && (!selection_scroll || !selection_keyboard || !selection_pointer
                        || !selection_blocked || automated_input_events != 31
                        || telemetry.live_samples != 33))
                || (password_acceptance
                    && (!password_scroll || !password_hidden || !password_pointer
                        || !password_keyboard || !password_disabled
                        || telemetry.live_samples != 33))
                || (clear_acceptance
                    && (!clear_scroll || !clear_pointer || !clear_keyboard || !clear_disabled
                        || telemetry.input_changes < 3 || telemetry.live_samples != 33))
                || (input_acceptance
                    && (!input_latin || !input_selection || !input_clipboard
                        || !input_undo || !input_redo || !input_theme_status
                        || !input_caret_active || !input_caret_idle
                        || telemetry.input_changes < 6
                        || telemetry.input_submits != 1
                        || inputs.next_caret_deadline().has_value()))
                || (animation_acceptance
                    && (automated_input_events != 7
                        || pointer_diagnostics.input_events < 4
                        || pointer_diagnostics.hover_enters == 0
                        || pointer_diagnostics.hover_leaves == 0
                        || pointer_diagnostics.captures_started != 1
                        || pointer_diagnostics.captures_released != 1
                        || focus_diagnostics.keyboard_events != 3
                        || focus_diagnostics.traversals == 0
                        || focus_diagnostics.activations != 1))));

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
            << " input_changes=" << telemetry.input_changes
            << " input_submits=" << telemetry.input_submits
            << " search_submits=" << telemetry.search_submits
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
            << " document_extent_updates="
            << document_diagnostics.extent_updates
            << " document_scroll_updates="
            << document_diagnostics.scroll_updates
            << " document_anchor_updates="
            << document_diagnostics.anchor_updates
            << " document_navigation_jumps="
            << document_diagnostics.navigation_jumps
            << " document_translation_passes="
            << document_diagnostics.translation_passes
            << " document_translated_nodes="
            << document_diagnostics.translated_nodes
            << " document_reconciliation_syncs="
            << submitter.reconciliation_syncs()
            << " draw_fragments_considered="
            << submitter.last_visible_scene().fragments_considered
            << " draw_fragments_visible="
            << submitter.last_visible_scene().fragments_visible
            << " frame_average_us=" << submitter.average_frame_microseconds()
            << " frame_max_us=" << submitter.max_frame_microseconds()
            << " frame_p95_us=" << submitter.p95_frame_microseconds()
            << " frame_scene_sync_us=" << phase_times[0]
            << " frame_translation_us=" << detail_times[0]
            << " frame_layout_us=" << detail_times[1]
            << " frame_anchor_input_us=" << detail_times[2]
            << " frame_resource_sync_us=" << phase_times[1]
            << " frame_quad_sync_us=" << detail_times[3]
            << " frame_glyph_sync_us=" << detail_times[4]
            << " frame_effect_sync_us=" << detail_times[5]
            << " frame_upload_finish_us=" << detail_times[6]
            << " frame_cull_us=" << phase_times[2]
            << " frame_submit_us=" << phase_times[3]
            << " font_rasterizations=" << font_counters.rasterizations
            << " scroll_rasterizations="
            << (font_counters.rasterizations - rasterizations_before_scroll)
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
            << " button_material_updates=" << retained_surfaces.material_updates
            << " button_geometry_updates=" << retained_surfaces.geometry_updates
            << " quad_uploads=" << quad.initial_uploads + quad.range_uploads
            << " quad_uploaded_bytes=" << quad.uploaded_bytes
            << " glyph_uploads=" << glyph.texture_uploads + glyph.buffer_uploads
            << " glyph_buffer_uploads=" << glyph.buffer_uploads
            << " glyph_buffer_coalesces=" << glyph.buffer_upload_coalesces
            << " glyph_buffer_capacity=" << glyph_resources.instance_capacity()
            << " glyph_uploaded_bytes="
            << glyph.texture_uploaded_bytes + glyph.buffer_uploaded_bytes
            << " effect_uploads=" << effect.buffer_uploads
            << " effect_uploaded_bytes=" << effect.uploaded_bytes
            << " gpu_upload_submissions=" << render.upload_submissions
            << " quad_draws=" << render.quad_draws
            << " glyph_draws=" << render.glyph_draws
            << " effect_draws=" << render.effect_draws
            << " submits=" << render.frame_submissions
            << " idle_waits=" << frames.idle_waits
            << " animation_frames=" << frames.animation_frames
            << " idle_after_animation=" << frames.idle_after_animation
            << " animation_acceptance=" << (animation_acceptance ? "true" : "false")
            << " input_acceptance=" << (input_acceptance ? "true" : "false")
            << " selection_acceptance=" << (selection_acceptance ? "true" : "false")
            << " selection_theme=" << (selection_dark ? "dark"
                : selection_compact ? "compact" : "default")
            << " search_acceptance=" << (search_acceptance ? "true" : "false")
            << " search_keyboard=" << (search_keyboard ? "true" : "false")
            << " search_pointer=" << (search_pointer ? "true" : "false")
            << " search_blocked=" << (search_blocked ? "true" : "false")
            << " search_text=" << (search_text ? "true" : "false")
            << " search_scroll=" << (search_scroll ? "true" : "false")
            << " password_acceptance=" << (password_acceptance ? "true" : "false")
            << " password_hidden=" << (password_hidden ? "true" : "false")
            << " password_pointer=" << (password_pointer ? "true" : "false")
            << " password_keyboard=" << (password_keyboard ? "true" : "false")
            << " password_disabled=" << (password_disabled ? "true" : "false")
            << " password_scroll=" << (password_scroll ? "true" : "false")
            << " input_clear_acceptance=" << (clear_acceptance ? "true" : "false")
            << " input_clear_scroll=" << (clear_scroll ? "true" : "false")
            << " input_clear_pointer=" << (clear_pointer ? "true" : "false")
            << " input_clear_keyboard=" << (clear_keyboard ? "true" : "false")
            << " input_clear_disabled=" << (clear_disabled ? "true" : "false")
            << " selection_keyboard=" << (selection_keyboard ? "true" : "false")
            << " selection_pointer=" << (selection_pointer ? "true" : "false")
            << " selection_blocked=" << (selection_blocked ? "true" : "false")
            << " scroll_acceptance=" << (scroll_acceptance ? "true" : "false")
            << " scroll_acceptance_steps=" << scroll_stage
            << " scroll_sequence_ms="
            << (scroll_finished_milliseconds - scroll_started_milliseconds)
            << " input_latin=" << (input_latin ? "passed" : "not-run")
            << " input_selection=" << (input_selection ? "passed" : "not-run")
            << " input_clipboard=" << (input_clipboard ? "passed" : "not-run")
            << " input_undo=" << (input_undo ? "passed" : "not-run")
            << " input_redo=" << (input_redo ? "passed" : "not-run")
            << " input_theme_status=" << (input_theme_status ? "passed" : "not-run")
            << " input_caret_idle=" << (input_caret_idle ? "passed" : "not-run")
            << " automated_input_events=" << automated_input_events
            << " motion_mode=" << (motion_disabled
                    ? "theme-disabled"
                    : reduced_motion ? "reduced" : "normal")
            << " exit_code=" << (smoke_failed ? 6 : 0) << '\n';
        return smoke_failed ? 6 : 0;
    } catch (const std::exception& error) {
        std::cerr << "fatal_error=" << error.what() << '\n';
        return 7;
    }
}

} // namespace rynui::example
