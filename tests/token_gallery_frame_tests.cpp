#include "component/button_component.hpp"
#include "renderer/sdl/glyph_gpu_resources.hpp"
#include "renderer/sdl/rounded_effect_gpu_resources.hpp"
#include "runtime/frame_scheduler.hpp"
#include "runtime/animation_frame_deadline.hpp"
#include "runtime/invalidation.hpp"
#include "gallery_document_viewport.hpp"
#include "reference_surface.hpp"
#include "token_gallery_definition.hpp"

#include <ryn/rynui.hpp>

#include <cmath>
#include <cstddef>
#include <cstdint>
#include <exception>
#include <iostream>
#include <memory>
#include <span>
#include <stdexcept>
#include <string>
#include <string_view>
#include <utility>
#include <vector>

namespace {

void require(bool condition, const char* message) {
    if (!condition) {
        throw std::runtime_error(message);
    }
}

std::size_t drain_animation_frames(
    ryn::detail::ButtonComponentHost& host,
    ryn::runtime::OnDemandFrameLoop& loop,
    const char* message) {
    const auto submissions = loop.counters().submissions;
    bool reached_idle = false;
    for (std::size_t step = 0; step < 256; ++step) {
        const auto result = loop.step();
        require(
            result == ryn::runtime::FrameLoopStep::submitted
                || result == ryn::runtime::FrameLoopStep::idle,
            message);
        if (host.animations().size() == 0
            && result == ryn::runtime::FrameLoopStep::idle) {
            reached_idle = true;
            break;
        }
    }
    require(reached_idle, message);
    return static_cast<std::size_t>(loop.counters().submissions - submissions);
}

void advance_animation_frame(
    ryn::runtime::OnDemandFrameLoop& loop,
    const char* message) {
    const auto animation_frames = loop.counters().animation_frames;
    for (std::size_t step = 0; step < 64; ++step) {
        const auto result = loop.step();
        require(
            result == ryn::runtime::FrameLoopStep::submitted
                || result == ryn::runtime::FrameLoopStep::idle,
            message);
        if (loop.counters().animation_frames > animation_frames) {
            return;
        }
    }
    require(false, message);
}

bool near(float actual, float expected, float tolerance = 0.001F) {
    return std::fabs(actual - expected) <= tolerance;
}

class RecordingGpuApi final : public ryn::graphics::QuadUploadApi,
                              public ryn::detail::GlyphGpuApi,
                              public ryn::detail::RoundedEffectGpuApi {
public:
    void* create_vertex_buffer(std::size_t) override { return handle(next_++); }
    void release_buffer(void*) noexcept override {}
    bool upload(void*, std::size_t, std::span<const std::byte> bytes) override {
        ++quad_uploads;
        quad_bytes += bytes.size();
        return true;
    }
    const char* last_error() const noexcept override { return ""; }

    void* create_glyph_sampler() override { return handle(next_++); }
    void* create_glyph_texture(std::uint32_t, std::uint32_t) override {
        return handle(next_++);
    }
    void* create_glyph_buffer(std::size_t) override { return handle(next_++); }
    bool upload_glyph_texture(
        void*, const ryn::detail::GlyphTextureUpload& upload) override {
        ++glyph_texture_uploads;
        glyph_bytes += upload.bytes.size();
        return true;
    }
    bool upload_glyph_buffer(
        void*, std::size_t, std::span<const std::byte> bytes) override {
        ++glyph_buffer_uploads;
        glyph_bytes += bytes.size();
        return true;
    }
    void release_glyph_buffer(void*) noexcept override {}
    void release_glyph_texture(void*) noexcept override {}
    void release_glyph_sampler(void*) noexcept override {}
    const char* glyph_gpu_error() const noexcept override { return ""; }

    void* create_effect_buffer(std::size_t) override { return handle(next_++); }
    bool upload_effect_buffer(
        void*, std::size_t, std::span<const std::byte> bytes) override {
        ++effect_uploads;
        effect_bytes += bytes.size();
        return true;
    }
    void release_effect_buffer(void*) noexcept override {}
    const char* effect_gpu_error() const noexcept override { return ""; }

    static void* handle(std::uintptr_t value) {
        return reinterpret_cast<void*>(value);
    }

    std::uintptr_t next_{1};
    std::uint64_t quad_uploads{};
    std::uint64_t quad_bytes{};
    std::uint64_t glyph_texture_uploads{};
    std::uint64_t glyph_buffer_uploads{};
    std::uint64_t glyph_bytes{};
    std::uint64_t effect_uploads{};
    std::uint64_t effect_bytes{};
};

class RecordingDrawApi final : public ryn::detail::SceneDrawApi {
public:
    void draw_quad(std::uint32_t, std::uint32_t count) override {
        ++quad_draws;
        quad_instances += count;
    }
    void draw_glyph(std::uint32_t, std::uint32_t, std::uint32_t count) override {
        ++glyph_draws;
        glyph_instances += count;
    }
    void draw_rounded_effect(std::uint32_t, std::uint32_t count) override {
        ++effect_draws;
        effect_instances += count;
    }

    std::uint64_t quad_draws{};
    std::uint64_t quad_instances{};
    std::uint64_t glyph_draws{};
    std::uint64_t glyph_instances{};
    std::uint64_t effect_draws{};
    std::uint64_t effect_instances{};
};

struct Fixture final {
    Fixture()
        : fonts(create_runtime()),
          engine(*fonts),
          layout(nodes),
          dirty(nodes, &frames),
          text_scene(*fonts, engine, frames) {
        const auto latin = fonts->load_font_file(RYNUI_VALIDATION_LATIN_FONT, 0, 14);
        const auto cjk = fonts->load_font_file(RYNUI_VALIDATION_CJK_FONT, 0, 14);
        require(latin && cjk, "Token Gallery fonts failed to load");
        host = std::make_unique<ryn::detail::ButtonComponentHost>(
            nodes,
            layout,
            dirty,
            text_scene,
            std::vector<ryn::font::FontIdentity>{latin.font, cjk.font},
            frames);
        surfaces = std::make_unique<rynui::example::ReferenceSurfaceHost>(*host);
    }

    static std::unique_ptr<ryn::font::FontRuntime> create_runtime() {
        auto created = ryn::font::FontRuntime::create();
        require(static_cast<bool>(created), "Font Runtime initialization failed");
        return std::move(created.runtime);
    }

    std::unique_ptr<ryn::font::FontRuntime> fonts;
    ryn::text::TextEngine engine;
    ryn::runtime::FrameRequestState frames;
    ryn::runtime::NodeStore nodes;
    ryn::layout::LayoutEngine layout;
    ryn::runtime::DirtyQueues dirty;
    ryn::detail::TextSceneService text_scene;
    std::unique_ptr<ryn::detail::ButtonComponentHost> host;
    std::unique_ptr<rynui::example::ReferenceSurfaceHost> surfaces;
};

class IdleEvents final : public ryn::runtime::FrameEventSource {
public:
    ryn::animation::AnimationTime now() const noexcept override {
        return ryn::animation::AnimationTime::microseconds(
            static_cast<std::int64_t>(now_) * 1000);
    }
    bool poll_frame_event() noexcept override { return false; }
    bool wait_for_frame_event(std::uint32_t timeout) noexcept override {
        now_ += timeout;
        return false;
    }

private:
    std::uint64_t now_{};
};

class HeadlessSubmitter final : public ryn::runtime::FrameSubmitter {
public:
    HeadlessSubmitter(
        ryn::detail::ButtonComponentHost& host,
        ryn::detail::TextSceneService& text_scene,
        ryn::runtime::FrameRequestState& frames,
        RecordingGpuApi& gpu,
        RecordingDrawApi& draw) noexcept
        : host_(&host),
          text_scene_(&text_scene),
          frames_(&frames),
          gpu_(&gpu),
          glyphs_(gpu),
          effects_(gpu),
          draw_(&draw) {}

    void set_viewport(ryn::runtime::Size viewport) {
        viewport_ = viewport;
        frames_->request_frame();
    }

    ryn::runtime::FrameSubmissionResult submit_frame(
        ryn::animation::AnimationTime frame_time) override {
        static_cast<void>(host_->tick_animations(frame_time));
        if (!host_->layout_and_synchronize(
                viewport_,
                {0.0F, 0.0F, viewport_.width, viewport_.height},
                {24.0F, 20.0F},
                0.0F)) {
            return ryn::runtime::FrameSubmissionResult::failed;
        }
        if (quads_ == nullptr) {
            quads_ = std::make_unique<ryn::graphics::QuadGpuBuffer>(
                *gpu_, host_->button_scene().instances());
        } else {
            host_->button_scene().synchronize_gpu(*quads_);
        }
        glyphs_.synchronize(
            text_scene_->atlas(), text_scene_->glyph_scene().instances());
        effects_.synchronize(
            host_->rounded_effects(),
            {
                static_cast<std::uint32_t>(viewport_.width),
                static_cast<std::uint32_t>(viewport_.height),
                1.0F,
            });
        ryn::detail::draw_ordered_scene(host_->scene_composer().ordered_scene(), *draw_);
        return ryn::runtime::FrameSubmissionResult::submitted;
    }

private:
    ryn::detail::ButtonComponentHost* host_;
    ryn::detail::TextSceneService* text_scene_;
    ryn::runtime::FrameRequestState* frames_;
    RecordingGpuApi* gpu_;
    ryn::detail::GlyphGpuResources glyphs_;
    ryn::detail::RoundedEffectGpuResources effects_;
    RecordingDrawApi* draw_;
    ryn::runtime::Size viewport_{1200.0F, 30000.0F};
    std::unique_ptr<ryn::graphics::QuadGpuBuffer> quads_;
};

void require_all_cells_reachable(
    const Fixture& fixture,
    ryn::runtime::Size viewport) {
    for (const auto& mounted : fixture.host->mounted_buttons()) {
        const auto bounds = fixture.nodes.require(mounted.node).bounds;
        require(bounds.width > 0.0F && bounds.height > 0.0F,
                "Token Gallery produced an empty cell");
        require(bounds.x >= 23.75F && bounds.x + bounds.width <= viewport.width - 23.75F,
                "Token Gallery cell escaped the horizontal viewport");
        require(bounds.y >= 19.75F && bounds.y + bounds.height <= viewport.height - 19.75F,
                "Token Gallery cell escaped the vertical viewport");
    }
    for (const auto& mounted : fixture.surfaces->mounted_surfaces()) {
        const auto bounds = fixture.nodes.require(mounted.node).bounds;
        require(bounds.width > 0.0F && bounds.height > 0.0F,
                "Token Gallery produced an empty reference surface");
        require(bounds.x >= 23.75F && bounds.x + bounds.width <= viewport.width - 23.75F,
                "Token Gallery reference surface escaped the horizontal viewport");
        require(bounds.y >= 19.75F && bounds.y + bounds.height <= viewport.height - 19.75F,
                "Token Gallery reference surface escaped the vertical viewport");
    }
}

void require_shadow_order(
    const Fixture& fixture,
    std::size_t surface_index,
    const ryn::ShadowList& expected) {
    const auto mounted = fixture.surfaces->mounted_surfaces()[surface_index];
    const auto snapshot = fixture.surfaces->snapshot(mounted.component);
    const auto ids = fixture.host->button_scene().shadow_effects(snapshot.scene);
    require(ids.size() == expected.size(), "Gallery shadow cell lost a typed layer");
    for (std::size_t index = 0; index < expected.size(); ++index) {
        const auto& actual = fixture.host->rounded_effects().at(ids[index]);
        const auto kind = expected[index].kind == ryn::ShadowKind::outer
            ? ryn::graphics::RoundedEffectKind::outer_shadow
            : ryn::graphics::RoundedEffectKind::inset_shadow;
        require(actual.geometry.kind == kind
                    && actual.geometry.offset == expected[index].offset
                    && near(actual.geometry.blur, expected[index].blur)
                    && near(actual.geometry.spread, expected[index].spread)
                    && actual.material.color == expected[index].color,
                "Gallery shadow layer order or geometry drifted from the catalog");
    }
}

void test_acceptance_scale_viewports_use_physical_pixels() {
    const auto one = rynui::example::token_gallery_logical_viewport(1280, 900, 1.0F);
    const auto one_and_quarter =
        rynui::example::token_gallery_logical_viewport(1280, 900, 1.25F);
    const auto one_and_half =
        rynui::example::token_gallery_logical_viewport(1280, 900, 1.5F);
    const auto two = rynui::example::token_gallery_logical_viewport(1280, 900, 2.0F);
    require(near(one.width, 1280.0F) && near(one.height, 900.0F)
                && near(one_and_quarter.width, 1024.0F)
                && near(one_and_quarter.height, 720.0F)
                && near(one_and_half.width, 853.3333F, 0.01F)
                && near(one_and_half.height, 600.0F)
                && near(two.width, 640.0F) && near(two.height, 450.0F),
            "Token Gallery acceptance scales did not derive logical extents from pixels");
    try {
        static_cast<void>(
            rynui::example::token_gallery_logical_viewport(1280, 900, 0.0F));
        throw std::runtime_error("Token Gallery accepted an invalid render scale");
    } catch (const std::invalid_argument&) {
    }

    require(near(rynui::example::token_gallery_pointer_to_render_logical(
                     300.0F, 1.0F, 1.5F),
                200.0F)
                && near(rynui::example::token_gallery_pointer_to_render_logical(
                            300.0F, 1.5F, 1.5F),
                    300.0F)
                && near(rynui::example::token_gallery_pointer_to_render_logical(
                            300.0F, 2.0F, 1.0F),
                    600.0F),
            "Token Gallery host/render scale mapping drifted from the visual viewport");
}

void test_motion_acceptance_control_updates_theme_without_content_rerun() {
    auto definition = rynui::example::make_token_gallery_definition();
    definition.set_motion_enabled(false);
    auto disabled = definition.telemetry();
    definition.set_motion_enabled(true);
    const auto restored = definition.telemetry();
    require(disabled.motion_updates == 1
                && disabled.theme_updates == 1
                && restored.motion_updates == 2
                && restored.theme_updates == 2
                && restored.content_runs == 0,
            "Token Gallery motion acceptance control reran or skipped Theme state");
}

void test_small_acceptance_viewport_survives_theme_transitions() {
    auto definition = rynui::example::make_token_gallery_definition();
    definition.set_viewport_width(640.0F);
    Fixture fixture;
    fixture.surfaces->mount(definition.content);
    RecordingGpuApi gpu;
    RecordingDrawApi draw;
    IdleEvents events;
    HeadlessSubmitter submitter(
        *fixture.host, fixture.text_scene, fixture.frames, gpu, draw);
    submitter.set_viewport({640.0F, 450.0F});
    ryn::runtime::AnimationFrameDeadlineSource animation_deadlines(
        fixture.host->animations());
    ryn::runtime::OnDemandFrameLoop loop(
        fixture.frames, events, submitter, animation_deadlines, 5);
    require(loop.step() == ryn::runtime::FrameLoopStep::submitted,
            "Token Gallery 200% acceptance viewport did not submit its initial frame");
    for (std::size_t step = 0; step < 5; ++step) {
        definition.smoke_step(step);
        require(loop.step() == ryn::runtime::FrameLoopStep::submitted,
                "Token Gallery 200% acceptance viewport lost a theme or state frame");
        if (step < 3) {
            advance_animation_frame(
                loop,
                "Token Gallery 200% acceptance animation frame did not advance");
        } else {
            static_cast<void>(drain_animation_frames(
                *fixture.host,
                loop,
                "Token Gallery 200% acceptance animations did not settle"));
        }
    }
}

void test_document_viewport_scrolls_long_content_without_remount() {
    using rynui::example::GalleryDocumentSectionKind;
    using rynui::example::GalleryDocumentViewport;

    auto definition = rynui::example::make_token_gallery_definition();
    definition.set_viewport_width(1200.0F);
    Fixture fixture;
    fixture.surfaces->mount(definition.content);
    const auto roots = fixture.host->components().root_components();
    require(roots.size() == 1,
            "Token Gallery document did not preserve one retained root");
    const auto root = fixture.host->components().root(roots.front());
    constexpr ryn::runtime::Size viewport{1200.0F, 900.0F};
    constexpr ryn::runtime::Rect clip{16.0F, 12.0F, 1168.0F, 876.0F};
    constexpr ryn::runtime::Point origin{24.0F, 20.0F};

    GalleryDocumentViewport document;
    require(document.apply_subtree_translation(
                root, fixture.nodes, fixture.dirty)
                && fixture.host->layout_and_synchronize(
                    viewport, clip, origin, 0.0F, true),
            "Token Gallery long document initial layout failed");
    const auto& root_node = fixture.nodes.require(root);
    require(root_node.bounds.height > clip.height,
            "Token Gallery document was truncated to the window height");

    constexpr std::array<std::size_t, 6> section_surface_indices{
        0, 5, 6, 11, 51, 124};
    std::array<float, 6> anchors{};
    const auto surfaces = fixture.surfaces->mounted_surfaces();
    for (std::size_t index = 0; index < anchors.size(); ++index) {
        anchors[index] = fixture.nodes.require(
            surfaces[section_surface_indices[index]].node).bounds.y
            - root_node.bounds.y;
    }
    document.replace_anchors(anchors);
    document.set_extents(clip.height, root_node.bounds.height);
    const auto live = document.anchor(GalleryDocumentSectionKind::live_samples);
    require(live.has_value() && document.jump_to(*live),
            "Token Gallery live-sample anchor was not reachable");

    const auto mounted_before = fixture.host->components().mount_runs();
    const auto component_count = fixture.host->components().component_count();
    const auto content_runs = definition.telemetry().content_runs;
    require(document.apply_subtree_translation(
                root, fixture.nodes, fixture.dirty)
                && fixture.host->layout_and_synchronize(
                    viewport, clip, origin, 0.0F, true),
            "Token Gallery live-sample scroll synchronization failed");

    const auto live_button = fixture.host->mounted_buttons()[
        definition.navigation_control_count];
    const auto& live_node = fixture.nodes.require(live_button.node);
    const ryn::runtime::Point live_point{
        live_node.bounds.x + live_node.translation.x + live_node.bounds.width * 0.5F,
        live_node.bounds.y + live_node.translation.y + live_node.bounds.height * 0.5F,
    };
    const auto hit = fixture.host->hit_test().hit_test(live_point);
    require(hit.has_value() && *hit == live_button.interaction,
            "Token Gallery scrolled HitTest did not follow subtree translation");
    require(fixture.host->components().mount_runs() == mounted_before
                && fixture.host->components().component_count() == component_count
                && definition.telemetry().content_runs == content_runs,
            "Token Gallery scroll remounted or reran document content");

    require(document.scroll_to(0.0F)
                && document.apply_subtree_translation(
                    root, fixture.nodes, fixture.dirty)
                && fixture.host->layout_and_synchronize(
                    viewport, clip, origin, 0.0F, true),
            "Token Gallery top-anchor restoration failed");
    const auto top_hit = fixture.host->hit_test().hit_test(live_point);
    require(!top_hit.has_value() || *top_hit != live_button.interaction,
            "offscreen Token Gallery control retained stale HitTest geometry");
}

ryn::input::PointerInputEvent pointer_event(
    ryn::input::PointerAction action,
    ryn::runtime::Point point,
    ryn::input::PointerButton button = ryn::input::PointerButton::none) {
    return {
        ryn::input::PointerIdentity::mouse(), action, button, point.x, point.y,
    };
}

void test_navigation_and_filter_controls_preserve_catalog_identity() {
    using namespace rynui::example;
    auto definition = make_token_gallery_definition();
    definition.set_viewport_width(1200.0F);
    Fixture fixture;
    fixture.surfaces->mount(definition.content);
    constexpr ryn::runtime::Size viewport{1200.0F, 30000.0F};
    constexpr ryn::runtime::Rect clip{0.0F, 0.0F, 1200.0F, 30000.0F};
    require(fixture.host->layout_and_synchronize(
                viewport, clip, {24.0F, 20.0F}, 0.0F, true),
            "Gallery navigation fixture did not layout");

    const auto components = fixture.host->components().component_count();
    const auto mount_runs = fixture.host->components().mount_runs();
    const auto live_component = fixture.host->mounted_buttons()[
        definition.navigation_control_count].component;
    const auto click = [&](std::size_t index) {
        const auto button = fixture.host->mounted_buttons()[index];
        const auto& node = fixture.nodes.require(button.node);
        const ryn::runtime::Point point{
            node.bounds.x + node.translation.x + node.bounds.width * 0.5F,
            node.bounds.y + node.translation.y + node.bounds.height * 0.5F,
        };
        fixture.host->pointer().dispatch(
            pointer_event(ryn::input::PointerAction::move, point));
        fixture.host->pointer().dispatch(pointer_event(
            ryn::input::PointerAction::down,
            point,
            ryn::input::PointerButton::primary));
        fixture.host->pointer().dispatch(pointer_event(
            ryn::input::PointerAction::up,
            point,
            ryn::input::PointerButton::primary));
    };

    constexpr std::size_t first_category_control = 6;
    click(first_category_control);
    const auto request = definition.take_navigation_request();
    require(request.has_value()
                && request->kind == GalleryNavigationTargetKind::category
                && request->category == AntDesignGalleryCategory::general,
            "Gallery category control did not emit its typed navigation target");
    fixture.host->focus().dispatch({
        ryn::input::Key::tab,
        ryn::input::KeyAction::down,
        ryn::input::KeyModifier::none,
        false,
    });
    fixture.host->focus().dispatch({
        ryn::input::Key::enter,
        ryn::input::KeyAction::down,
        ryn::input::KeyModifier::none,
        false,
    });
    fixture.host->focus().dispatch({
        ryn::input::Key::enter,
        ryn::input::KeyAction::up,
        ryn::input::KeyModifier::none,
        false,
    });
    require(definition.take_navigation_request().has_value(),
            "Gallery keyboard activation did not emit a navigation target");

    constexpr std::size_t first_filter_control = 13;
    constexpr std::size_t partial_filter_control = first_filter_control + 2;
    click(partial_filter_control);
    require(fixture.host->layout_and_synchronize(
                viewport, clip, {24.0F, 20.0F}, 0.0F, true),
            "Gallery partial filter did not synchronize");
    std::size_t visible = 0;
    std::size_t hidden = 0;
    const auto surfaces = fixture.surfaces->mounted_surfaces();
    for (std::size_t index = 52; index < 124; ++index) {
        const auto snapshot = fixture.surfaces->snapshot(surfaces[index].component);
        const auto& node = fixture.nodes.require(surfaces[index].node);
        if (snapshot.visible) {
            ++visible;
            require(node.bounds.width > 0.0F && node.bounds.height > 0.0F,
                    "visible Gallery filter entry collapsed");
        } else {
            ++hidden;
            require(node.bounds.width == 0.0F && node.bounds.height == 0.0F,
                    "hidden Gallery filter entry retained layout extent");
        }
    }
    require(visible == 5 && hidden == 67,
            "Gallery partial filter did not match the support catalog");
    require(fixture.host->components().component_count() == components
                && fixture.host->components().mount_runs() == mount_runs
                && fixture.host->components().contains(live_component)
                && definition.telemetry().content_runs == 1
                && definition.telemetry().reference_content_runs == 125
                && definition.telemetry().navigation_requests == 2
                && definition.telemetry().filter_updates == 1,
            "Gallery navigation/filter remounted content or disturbed its live sibling");

    click(first_filter_control);
    require(fixture.host->layout_and_synchronize(
                viewport, clip, {24.0F, 20.0F}, 0.0F, true),
            "Gallery all-status filter did not synchronize");
    for (std::size_t index = 52; index < 124; ++index) {
        require(fixture.surfaces->snapshot(surfaces[index].component).visible,
                "Gallery all-status filter did not restore an entry");
    }
}

void test_token_gallery_frame_contract() {
    auto definition = rynui::example::make_token_gallery_definition();
    require(definition.stable_test_ids.size() == 51,
            "Token Gallery stable test-id inventory is incomplete");
    for (const auto id : definition.stable_test_ids) {
        if (id.starts_with("ant.")) {
            if (ryn::find_ant_design_token(id) == nullptr) {
                throw std::runtime_error(
                    "Token Gallery test id is not backed by the locked catalog: "
                    + std::string(id));
            }
        }
    }

    Fixture fixture;
    definition.set_viewport_width(1200.0F);
    fixture.surfaces->mount(definition.content);
    require(fixture.host->mounted_buttons().size()
                == definition.navigation_control_count + 12,
            "Token Gallery live sample count drifted");
    require(fixture.surfaces->mounted_surfaces().size() == 125,
            "Token Gallery document reference surface count drifted");
    require(fixture.host->interactions().size()
                == definition.navigation_control_count + 12,
            "Token Gallery documentation entered the interaction registry");

    RecordingGpuApi gpu;
    RecordingDrawApi draw;
    IdleEvents events;
    HeadlessSubmitter submitter(
        *fixture.host, fixture.text_scene, fixture.frames, gpu, draw);
    ryn::runtime::AnimationFrameDeadlineSource animation_deadlines(
        fixture.host->animations());
    ryn::runtime::OnDemandFrameLoop loop(
        fixture.frames, events, submitter, animation_deadlines, 5);
    require(loop.step() == ryn::runtime::FrameLoopStep::submitted,
            "Token Gallery initial wide frame was not submitted");
    require_all_cells_reachable(fixture, {1200.0F, 30000.0F});
    require(fixture.host->scene_composer().interaction_order().size()
                == definition.navigation_control_count + 12,
            "Token Gallery reference content entered scene interaction order");

    const auto initial = definition.telemetry();
    require(initial.content_runs == 1
                && initial.theme_content_runs == definition.stable_test_ids.size() + 1
                && initial.document_sections == 6
                && initial.component_entries == 72
                && initial.reference_surfaces == 125
                && initial.reference_content_runs == 125
                && initial.live_samples == 12,
            "Token Gallery Theme content did not mount exactly once");
    require(gpu.quad_uploads == 1 && gpu.glyph_buffer_uploads == 1
                && gpu.effect_uploads == 1 && draw.quad_draws > 0
                && draw.glyph_draws > 0 && draw.effect_draws > 0,
            "Token Gallery initial retained upload/draw contract was incomplete");

    const auto& shadows = ryn::ant_design_default_shadows();
    constexpr std::size_t first_shadow_cell = 34;
    require_shadow_order(fixture, first_shadow_cell + 0, shadows.box_shadow_tertiary);
    require_shadow_order(fixture, first_shadow_cell + 1, shadows.box_shadow_secondary);
    require_shadow_order(fixture, first_shadow_cell + 2, shadows.box_shadow);
    require_shadow_order(fixture, first_shadow_cell + 3, shadows.button_default);
    require_shadow_order(fixture, first_shadow_cell + 4, shadows.button_primary);
    require_shadow_order(fixture, first_shadow_cell + 5, shadows.button_danger);
    require_shadow_order(fixture, first_shadow_cell + 6, shadows.drawer_left);
    require_shadow_order(fixture, first_shadow_cell + 7, shadows.drawer_right);
    require_shadow_order(fixture, first_shadow_cell + 8, shadows.drawer_up);
    require_shadow_order(fixture, first_shadow_cell + 9, shadows.drawer_down);
    require_shadow_order(fixture, first_shadow_cell + 10, shadows.popover_arrow);
    require_shadow_order(fixture, first_shadow_cell + 11, shadows.popover_drop);
    require_shadow_order(fixture, first_shadow_cell + 12, shadows.card);
    require_shadow_order(fixture, first_shadow_cell + 13, shadows.tabs_overflow_left);
    require_shadow_order(fixture, first_shadow_cell + 14, shadows.tabs_overflow_right);
    require_shadow_order(fixture, first_shadow_cell + 15, shadows.tabs_overflow_top);
    require_shadow_order(fixture, first_shadow_cell + 16, shadows.tabs_overflow_bottom);

    const auto focus_target = fixture.host->mounted_buttons()[
        definition.navigation_control_count + 9];
    const auto hidden_focus = fixture.host->button_scene().focus_effect(focus_target.scene);
    require(hidden_focus.geometry.kind == ryn::graphics::RoundedEffectKind::outline
                && near(hidden_focus.geometry.outline_offset, 1.0F)
                && near(hidden_focus.geometry.outline_width, 3.0F)
                && near(hidden_focus.material.opacity, 0.0F),
            "Token Gallery focus cell does not use a 1px gap and 3px hollow ring");

    definition.set_viewport_width(560.0F);
    submitter.set_viewport({560.0F, 30000.0F});
    require(loop.step() == ryn::runtime::FrameLoopStep::submitted,
            "Token Gallery narrow frame was not submitted");
    require_all_cells_reachable(fixture, {560.0F, 30000.0F});

    definition.set_viewport_width(1200.0F);
    submitter.set_viewport({1200.0F, 30000.0F});
    require(loop.step() == ryn::runtime::FrameLoopStep::submitted,
            "Token Gallery wide restoration was not submitted");
    const auto component_count = fixture.host->components().component_count();
    const auto scene_rebuilds = fixture.host->scene_composer().diagnostics().rebuilds;
    const auto effect_uploads = gpu.effect_uploads;
    const auto quad_uploads = gpu.quad_uploads;

    definition.smoke_step(0);
    require(loop.step() == ryn::runtime::FrameLoopStep::submitted,
            "Dark Theme did not submit a Token Gallery frame");
    advance_animation_frame(
        loop,
        "Dark Theme did not submit an animation deadline frame");
    require(gpu.effect_uploads > effect_uploads && gpu.quad_uploads > quad_uploads,
            "Theme update did not produce local retained uploads");
    definition.smoke_step(1);
    require(loop.step() == ryn::runtime::FrameLoopStep::submitted,
            "Compact Theme did not submit a Token Gallery frame");
    advance_animation_frame(
        loop,
        "Compact Theme did not submit an animation deadline frame");
    definition.smoke_step(2);
    require(loop.step() == ryn::runtime::FrameLoopStep::submitted,
            "Brand Seed did not submit a Token Gallery frame");
    advance_animation_frame(
        loop,
        "Brand Seed did not submit an animation deadline frame");
    definition.smoke_step(3);
    require(loop.step() == ryn::runtime::FrameLoopStep::submitted,
            "disabled/loading update did not submit a Token Gallery frame");
    static_cast<void>(drain_animation_frames(
        *fixture.host,
        loop,
        "disabled/loading animations did not settle through frame deadlines"));
    definition.smoke_step(4);
    require(loop.step() == ryn::runtime::FrameLoopStep::submitted,
            "Default Theme restoration did not submit a Token Gallery frame");
    static_cast<void>(drain_animation_frames(
        *fixture.host,
        loop,
        "Default Theme animations did not settle through frame deadlines"));

    fixture.host->focus().dispatch({
        ryn::input::Key::tab,
        ryn::input::KeyAction::down,
        ryn::input::KeyModifier::none,
        false,
    });
    require(loop.step() == ryn::runtime::FrameLoopStep::submitted,
            "keyboard focus-visible transition did not submit");
    const auto focused = fixture.host->mounted_buttons().front();
    const auto visible_focus = fixture.host->button_scene().focus_effect(focused.scene);
    require(near(visible_focus.material.opacity, 1.0F)
                && near(visible_focus.geometry.outline_offset, 1.0F)
                && near(visible_focus.geometry.outline_width, 3.0F),
            "keyboard focus-visible transition lost the hollow outline");

    const auto hover = fixture.host->mounted_buttons()[
        definition.navigation_control_count + 7];
    const auto bounds = fixture.nodes.require(hover.node).bounds;
    const ryn::runtime::Point inside{
        bounds.x + bounds.width * 0.5F,
        bounds.y + bounds.height * 0.5F,
    };
    fixture.host->pointer().dispatch(
        pointer_event(ryn::input::PointerAction::move, inside));
    require(loop.step() == ryn::runtime::FrameLoopStep::submitted,
            "Gallery hover state did not submit");
    fixture.host->pointer().dispatch(pointer_event(
        ryn::input::PointerAction::down,
        inside,
        ryn::input::PointerButton::primary));
    require(loop.step() == ryn::runtime::FrameLoopStep::submitted,
            "Gallery active state did not submit");
    fixture.host->pointer().dispatch(pointer_event(
        ryn::input::PointerAction::up,
        inside,
        ryn::input::PointerButton::primary));
    require(loop.step() == ryn::runtime::FrameLoopStep::submitted,
            "Gallery active release did not submit");

    static_cast<void>(drain_animation_frames(
        *fixture.host,
        loop,
        "Token Gallery interaction animations did not settle through frame deadlines"));

    const auto final = definition.telemetry();
    require(final.content_runs == initial.content_runs
                && final.theme_content_runs == initial.theme_content_runs
                && fixture.host->components().component_count() == component_count
                && fixture.host->scene_composer().diagnostics().rebuilds == scene_rebuilds
                && final.theme_updates == 4
                && final.brand_updates == 1
                && final.state_updates == 2
                && final.viewport_updates >= 4,
            "Token Gallery updates reran content or rebuilt retained topology");

    const auto submissions = loop.counters().submissions;
    for (int index = 0; index < 40; ++index) {
        require(loop.step() == ryn::runtime::FrameLoopStep::idle,
                "Idle Token Gallery continued submitting frames");
    }
    require(loop.counters().submissions == submissions
                && loop.counters().idle_waits >= 40,
            "Token Gallery idle acceptance counters drifted");
}

} // namespace

int main() {
    try {
        test_acceptance_scale_viewports_use_physical_pixels();
        test_motion_acceptance_control_updates_theme_without_content_rerun();
        test_small_acceptance_viewport_survives_theme_transitions();
        test_document_viewport_scrolls_long_content_without_remount();
        test_navigation_and_filter_controls_preserve_catalog_identity();
        test_token_gallery_frame_contract();
    } catch (const std::exception& error) {
        std::cerr << error.what() << '\n';
        return 1;
    }
    return 0;
}
