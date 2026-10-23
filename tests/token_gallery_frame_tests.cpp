#include "renderer/common/quad_gpu_resources.hpp"
#include "component/button_component.hpp"
#include "component/selection_component.hpp"
#include "component/typography_component.hpp"
#include "component/divider_component.hpp"
#include "component/otp_component.hpp"
#include "component/select_component.hpp"
#include "component/card_component.hpp"
#include "component/tabs_component.hpp"
#include "component/modal_component.hpp"
#include "component/popconfirm_component.hpp"
#include "component/tag_badge_component.hpp"
#include "component/feedback_component.hpp"
#include "component/table_component.hpp"
#include "component/pagination_component.hpp"
#include "component/menu_component.hpp"
#include "component/dropdown_component.hpp"
#include "renderer/common/glyph_gpu_resources.hpp"
#include "renderer/common/rounded_effect_gpu_resources.hpp"
#include "runtime/frame_scheduler.hpp"
#include "runtime/animation_frame_deadline.hpp"
#include "runtime/invalidation.hpp"
#include "gallery_document_viewport.hpp"
#include "gallery_layout.hpp"
#include "reference_surface.hpp"
#include "token_gallery_definition.hpp"
#include "support/input_fixture.hpp"

#include <ryn/rynui.hpp>

#include <algorithm>
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

std::size_t drain_animation_frames(ryn::detail::ButtonComponentHost& host, ryn::runtime::OnDemandFrameLoop& loop,
                                   const char* message) {
    const auto submissions = loop.counters().submissions;
    bool reached_idle = false;
    // The 5ms native wait must cover the Button's independent 2000ms fade.
    for (std::size_t step = 0; step < 512; ++step) {
        const auto result = loop.step();
        require(result == ryn::runtime::FrameLoopStep::submitted || result == ryn::runtime::FrameLoopStep::idle,
                message);
        if (host.animations().size() == 0 && result == ryn::runtime::FrameLoopStep::idle) {
            reached_idle = true;
            break;
        }
    }
    require(reached_idle, message);
    return static_cast<std::size_t>(loop.counters().submissions - submissions);
}

void advance_animation_frame(ryn::runtime::OnDemandFrameLoop& loop, const char* message) {
    const auto animation_frames = loop.counters().animation_frames;
    for (std::size_t step = 0; step < 64; ++step) {
        const auto result = loop.step();
        require(result == ryn::runtime::FrameLoopStep::submitted || result == ryn::runtime::FrameLoopStep::idle,
                message);
        if (loop.counters().animation_frames > animation_frames) {
            return;
        }
    }
    throw std::runtime_error(std::string{message} + " submissions=" + std::to_string(loop.counters().submissions) +
                             " idle=" + std::to_string(loop.counters().idle_waits) +
                             " animations=" + std::to_string(loop.counters().animation_frames));
}

bool near(float actual, float expected, float tolerance = 0.001F) {
    return std::fabs(actual - expected) <= tolerance;
}

class RecordingGpuApi final : public ryn::detail::QuadUploadApi,
                              public ryn::detail::GlyphGpuApi,
                              public ryn::detail::RoundedEffectGpuApi {
public:
    void* create_vertex_buffer(std::size_t) override {
        return handle(next_++);
    }

    void release_buffer(void*) noexcept override {}

    bool upload(void*, std::size_t, std::span<const std::byte> bytes) override {
        ++quad_uploads;
        quad_bytes += bytes.size();
        return true;
    }

    const char* last_error() const noexcept override {
        return "";
    }

    void* create_glyph_sampler() override {
        return handle(next_++);
    }

    void* create_glyph_texture(std::uint32_t, std::uint32_t) override {
        return handle(next_++);
    }

    void* create_glyph_buffer(std::size_t) override {
        return handle(next_++);
    }

    bool upload_glyph_texture(void*, const ryn::detail::GlyphTextureUpload& upload) override {
        ++glyph_texture_uploads;
        glyph_bytes += std::uint64_t(upload.rectangle.width) * upload.rectangle.height;
        return true;
    }

    bool upload_glyph_buffer(void*, std::size_t, std::span<const std::byte> bytes) override {
        ++glyph_buffer_uploads;
        glyph_bytes += bytes.size();
        return true;
    }

    void release_glyph_buffer(void*) noexcept override {}

    void release_glyph_texture(void*) noexcept override {}

    void release_glyph_sampler(void*) noexcept override {}

    const char* glyph_gpu_error() const noexcept override {
        return "";
    }

    void* create_effect_buffer(std::size_t) override {
        return handle(next_++);
    }

    bool upload_effect_buffer(void*, std::size_t, std::span<const std::byte> bytes) override {
        ++effect_uploads;
        effect_bytes += bytes.size();
        return true;
    }

    void release_effect_buffer(void*) noexcept override {}

    const char* effect_gpu_error() const noexcept override {
        return "";
    }

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
        : fonts(create_runtime()), engine(*fonts), layout(nodes), dirty(nodes, &frames),
          text_scene(*fonts, engine, frames) {
        const auto latin = fonts->load_font_file(RYNUI_VALIDATION_LATIN_FONT, 0, 14);
        const auto cjk = fonts->load_font_file(RYNUI_VALIDATION_CJK_FONT, 0, 14);
        require(latin && cjk, "Token Gallery fonts failed to load");
        host = std::make_unique<ryn::detail::ButtonComponentHost>(
            nodes, layout, dirty, text_scene, std::vector<ryn::font::FontIdentity>{latin.font, cjk.font}, frames);
        surfaces = std::make_unique<rynui::example::ReferenceSurfaceHost>(*host);
        inputs = std::make_unique<ryn::detail::InputComponentHost>(host->services(), platform, platform);
        selections = std::make_unique<ryn::detail::SelectionComponentHost>(host->services());
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
    ryn_test::input_component::Platform platform;
    std::unique_ptr<ryn::detail::InputComponentHost> inputs;
    std::unique_ptr<ryn::detail::SelectionComponentHost> selections;
};

ryn::detail::MountedButtonComponent button_with_label(Fixture& f, std::string_view prefix) {
    for (const auto& text : f.host->text().mounted_texts()) {
        if (!f.text_scene.text_state(text.scene).content().bytes().starts_with(prefix)) {
            continue;
        }
        for (auto current = std::optional{text.component}; current; current = f.host->components().parent(*current)) {
            for (const auto& button : f.host->mounted_buttons()) {
                if (button.component == *current) {
                    return button;
                }
            }
        }
    }
    throw std::runtime_error("Gallery button label absent: " + std::string{prefix});
}

class IdleEvents final : public ryn::runtime::FrameEventSource {
public:
    ryn::animation::AnimationTime now() const noexcept override {
        return ryn::animation::AnimationTime::microseconds(static_cast<std::int64_t>(now_) * 1000);
    }

    bool poll_frame_event() noexcept override {
        return false;
    }

    bool wait_for_frame_event(std::uint32_t timeout) noexcept override {
        now_ += timeout;
        return false;
    }

private:
    std::uint64_t now_{};
};

class HeadlessSubmitter final : public ryn::runtime::FrameSubmitter {
public:
    HeadlessSubmitter(ryn::detail::ButtonComponentHost& host, ryn::detail::TextSceneService& text_scene,
                      ryn::runtime::FrameRequestState& frames, RecordingGpuApi& gpu, RecordingDrawApi& draw) noexcept
        : host_(&host), text_scene_(&text_scene), frames_(&frames), gpu_(&gpu), glyphs_(gpu), effects_(gpu),
          draw_(&draw) {}

    void set_viewport(ryn::runtime::Size viewport) {
        viewport_ = viewport;
        frames_->request_frame();
    }

    ryn::runtime::FrameSubmissionResult submit_frame(ryn::animation::AnimationTime frame_time) override {
        static_cast<void>(host_->tick_animations(frame_time));
        if (!host_->layout_and_synchronize(viewport_, {0.0F, 0.0F, viewport_.width, viewport_.height}, {24.0F, 20.0F},
                                           0.0F)) {
            return ryn::runtime::FrameSubmissionResult::failed;
        }
        if (quads_ == nullptr) {
            quads_ = std::make_unique<ryn::detail::QuadGpuBuffer>(*gpu_, host_->button_scene().instances(),
                                                                  ryn::detail::SceneDeviceMetrics{1200, 40000, 1});
        } else {
            quads_->synchronize(host_->button_scene().instances(), {1200, 40000, 1});
        }
        glyphs_.synchronize(text_scene_->atlas(), text_scene_->glyph_scene().instances(), {1200, 40000, 1});
        effects_.synchronize(host_->rounded_effects(), {
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
    ryn::runtime::Size viewport_{1200.0F, 40000.0F};
    std::unique_ptr<ryn::detail::QuadGpuBuffer> quads_;
};

void require_all_cells_reachable(const Fixture& fixture, ryn::runtime::Size viewport) {
    for (const auto& mounted : fixture.host->mounted_buttons()) {
        if (!fixture.host->components().branch_active(mounted.component)) {
            continue;
        }
        const auto bounds = fixture.nodes.require(mounted.node).bounds;
        require(bounds.width > 0.0F && bounds.height > 0.0F, "Token Gallery produced an empty cell");
        require(bounds.x >= 23.75F && bounds.x + bounds.width <= viewport.width - 23.75F,
                "Token Gallery cell escaped the horizontal viewport");
        require(bounds.y >= 19.75F && bounds.y + bounds.height <= viewport.height - 19.75F,
                "Token Gallery cell escaped the vertical viewport");
    }
    std::size_t surface_index = 0;
    for (const auto& mounted : fixture.surfaces->mounted_surfaces()) {
        if (fixture.surfaces->snapshot(mounted.component).role != rynui::example::ReferenceSurfaceRole::reference) {
            ++surface_index;
            continue;
        }
        const auto bounds = fixture.nodes.require(mounted.node).bounds;
        if (bounds.width <= 0.0F || bounds.height <= 0.0F) {
            throw std::runtime_error("Token Gallery produced an empty reference surface " +
                                     std::to_string(surface_index) + " " + std::to_string(bounds.width) + "x" +
                                     std::to_string(bounds.height));
        }
        require(bounds.x >= 23.75F && bounds.x + bounds.width <= viewport.width - 23.75F,
                "Token Gallery reference surface escaped the horizontal viewport");
        require(bounds.y >= 19.75F && bounds.y + bounds.height <= viewport.height - 19.75F,
                "Token Gallery reference surface escaped the vertical viewport");
        ++surface_index;
    }
}

void require_shadow_order(const Fixture& fixture, std::size_t surface_index, const ryn::ShadowList& expected) {
    const auto mounted = fixture.surfaces->mounted_surfaces()[surface_index];
    const auto snapshot = fixture.surfaces->snapshot(mounted.component);
    const auto ids = fixture.host->button_scene().shadow_effects(snapshot.scene);
    require(ids.size() == expected.size(), "Gallery shadow cell lost a typed layer");
    for (std::size_t index = 0; index < expected.size(); ++index) {
        const auto& actual = fixture.host->rounded_effects().at(ids[index]);
        const auto kind = expected[index].kind == ryn::ShadowKind::outer
                              ? ryn::graphics::RoundedEffectKind::outer_shadow
                              : ryn::graphics::RoundedEffectKind::inset_shadow;
        require(actual.geometry.kind == kind && actual.geometry.offset == expected[index].offset &&
                    near(actual.geometry.blur, expected[index].blur) &&
                    near(actual.geometry.spread, expected[index].spread) &&
                    actual.material.color == expected[index].color,
                "Gallery shadow layer order or geometry drifted from the catalog");
    }
}

void test_acceptance_scale_viewports_use_physical_pixels() {
    const auto one = rynui::example::token_gallery_logical_viewport(1280, 900, 1.0F);
    const auto one_and_quarter = rynui::example::token_gallery_logical_viewport(1280, 900, 1.25F);
    const auto one_and_half = rynui::example::token_gallery_logical_viewport(1280, 900, 1.5F);
    const auto two = rynui::example::token_gallery_logical_viewport(1280, 900, 2.0F);
    require(near(one.width, 1280.0F) && near(one.height, 900.0F) && near(one_and_quarter.width, 1024.0F) &&
                near(one_and_quarter.height, 720.0F) && near(one_and_half.width, 853.3333F, 0.01F) &&
                near(one_and_half.height, 600.0F) && near(two.width, 640.0F) && near(two.height, 450.0F),
            "Token Gallery acceptance scales did not derive logical extents from pixels");
    try {
        static_cast<void>(rynui::example::token_gallery_logical_viewport(1280, 900, 0.0F));
        throw std::runtime_error("Token Gallery accepted an invalid render scale");
    } catch (const std::invalid_argument&) {
    }

    require(near(rynui::example::token_gallery_pointer_to_render_logical(300.0F, 1.0F, 1.5F), 200.0F) &&
                near(rynui::example::token_gallery_pointer_to_render_logical(300.0F, 1.5F, 1.5F), 300.0F) &&
                near(rynui::example::token_gallery_pointer_to_render_logical(300.0F, 2.0F, 1.0F), 600.0F),
            "Token Gallery host/render scale mapping drifted from the visual viewport");
}

void test_motion_acceptance_control_updates_theme_without_content_rerun() {
    auto definition = rynui::example::make_token_gallery_definition();
    definition.set_motion_enabled(false);
    auto disabled = definition.telemetry();
    definition.set_motion_enabled(true);
    const auto restored = definition.telemetry();
    require(disabled.motion_updates == 1 && disabled.theme_updates == 1 && restored.motion_updates == 2 &&
                restored.theme_updates == 2 && restored.content_runs == 0,
            "Token Gallery motion acceptance control reran or skipped Theme state");
}

void test_small_acceptance_viewport_survives_theme_transitions() {
    auto definition = rynui::example::make_token_gallery_definition();
    definition.set_viewport_width(640.0F);
    Fixture fixture;
    fixture.surfaces->mount(definition.content, fixture.inputs.get());
    RecordingGpuApi gpu;
    RecordingDrawApi draw;
    IdleEvents events;
    HeadlessSubmitter submitter(*fixture.host, fixture.text_scene, fixture.frames, gpu, draw);
    submitter.set_viewport({640.0F, 450.0F});
    auto& animation_deadlines = *fixture.host;
    ryn::runtime::OnDemandFrameLoop loop(fixture.frames, events, submitter, animation_deadlines, 5);
    require(loop.step() == ryn::runtime::FrameLoopStep::submitted,
            "Token Gallery 200% acceptance viewport did not submit its initial frame");
    for (std::size_t step = 0; step < 5; ++step) {
        definition.smoke_step(step);
        require(loop.step() == ryn::runtime::FrameLoopStep::submitted,
                "Token Gallery 200% acceptance viewport lost a theme or state frame");
        if (step < 3) {
            advance_animation_frame(loop, "Token Gallery 200% acceptance animation frame did not advance");
        } else {
            static_cast<void>(
                drain_animation_frames(*fixture.host, loop, "Token Gallery 200% acceptance animations did not settle"));
        }
    }
}

void test_document_viewport_scrolls_long_content_without_remount() {
    using rynui::example::GalleryDocumentSectionKind;
    using rynui::example::GalleryDocumentViewport;

    auto definition = rynui::example::make_token_gallery_definition();
    definition.set_viewport_width(1200.0F);
    Fixture fixture;
    fixture.surfaces->mount(definition.content, fixture.inputs.get());
    const auto roots = fixture.host->components().root_components();
    require(roots.size() == 1, "Token Gallery document did not preserve one retained root");
    const auto root = fixture.host->components().root(roots.front());
    const auto body = fixture.nodes.require(root).children.front();
    const auto navigation_root = fixture.nodes.require(body).children[0];
    const auto document_root = fixture.nodes.require(body).children[1];
    const auto header_root = fixture.nodes.require(root).children.back();
    constexpr ryn::runtime::Size viewport{1200.0F, 900.0F};
    constexpr ryn::runtime::Rect clip{16.0F, 12.0F, 1168.0F, 876.0F};
    constexpr ryn::runtime::Point origin{24.0F, 20.0F};

    GalleryDocumentViewport document;
    require(document.apply_subtree_translation(document_root, fixture.nodes, fixture.dirty) &&
                fixture.host->layout_and_synchronize(viewport, clip, origin, 0.0F, true),
            "Token Gallery long document initial layout failed");
    const auto& document_node = fixture.nodes.require(document_root);
    require(document_node.bounds.height > clip.height, "Token Gallery document was truncated to the window height");
    ryn::graphics::OrderedScene visible_scene;
    const auto source_commands = fixture.host->scene_composer().ordered_scene().commands().size();
    const auto top_visibility = fixture.host->scene_composer().build_visible_scene(fixture.nodes, clip, visible_scene);
    require(top_visibility.fragments_visible > 0 &&
                top_visibility.fragments_visible < top_visibility.fragments_considered &&
                !visible_scene.commands().empty() && visible_scene.commands().size() < source_commands,
            "Gallery initial viewport still drew offscreen document fragments");

    constexpr std::array<std::size_t, 6> section_surface_indices{0, 5, 6, 11, 51, 125};
    std::array<float, 6> anchors{};
    const auto surfaces = fixture.surfaces->mounted_surfaces();
    for (std::size_t index = 0; index < anchors.size(); ++index) {
        anchors[index] =
            fixture.nodes.require(surfaces[section_surface_indices[index]].node).bounds.y - document_node.bounds.y;
    }
    document.replace_anchors(anchors);
    document.set_extents(clip.height, document_node.bounds.height);
    const auto live = document.anchor(GalleryDocumentSectionKind::live_samples);
    require(live.has_value() && document.jump_to(*live), "Token Gallery live-sample anchor was not reachable");

    const auto mounted_before = fixture.host->components().mount_runs();
    const auto component_count = fixture.host->components().component_count();
    const auto content_runs = definition.telemetry().content_runs;
    const auto ordered_rebuilds_before_scroll = fixture.text_scene.counters().ordered_scene_rebuilds;
    std::uint64_t text_rebuilds_before_scroll = 0;
    std::vector<bool> realized_before_scroll;
    std::vector<std::uint64_t> rebuilds_before_scroll;
    std::vector<std::uint64_t> geometry_before_scroll;
    std::vector<std::uint64_t> geometry_rebuilds_before_scroll;
    for (const auto& text : fixture.host->text().mounted_texts()) {
        const auto& counters = fixture.text_scene.record_counters(text.scene);
        text_rebuilds_before_scroll += counters.instance_rebuilds;
        geometry_before_scroll.push_back(counters.geometry_updates);
        geometry_rebuilds_before_scroll.push_back(counters.geometry_rebuilds);
        realized_before_scroll.push_back(fixture.text_scene.primitive(text.scene).instances.count > 0);
        rebuilds_before_scroll.push_back(counters.instance_rebuilds);
    }
    require(document.apply_subtree_translation(document_root, fixture.nodes, fixture.dirty) &&
                fixture.host->layout_and_synchronize(viewport, clip, origin, 0.0F, true),
            "Token Gallery live-sample scroll synchronization failed");
    require(fixture.nodes.require(navigation_root).translation == ryn::runtime::Point{} &&
                fixture.nodes.require(header_root).translation == ryn::runtime::Point{} &&
                fixture.nodes.require(document_root).translation.y == -document.snapshot().offset,
            "document scroll moved the independent sidebar or site header");
    const auto bottom_visibility =
        fixture.host->scene_composer().build_visible_scene(fixture.nodes, clip, visible_scene);
    require(bottom_visibility.fragments_visible > 0 &&
                bottom_visibility.fragments_visible < bottom_visibility.fragments_considered &&
                !visible_scene.commands().empty(),
            "Gallery scroll failed to cull offscreen fragments");
    std::uint64_t text_rebuilds_after_scroll = 0;
    std::size_t newly_realized = 0;
    std::size_t text_index = 0;
    for (const auto& text : fixture.host->text().mounted_texts()) {
        const auto& counters = fixture.text_scene.record_counters(text.scene);
        text_rebuilds_after_scroll += counters.instance_rebuilds;
        if (realized_before_scroll[text_index] || fixture.text_scene.primitive(text.scene).instances.count > 0) {
            require(counters.geometry_rebuilds == geometry_rebuilds_before_scroll[text_index],
                    "Gallery long jump rebuilt existing glyph geometry");
        }
        bool has_content_clip = false;
        for (std::optional<ryn::runtime::NodeId> id = fixture.host->components().root(text.component); id;
             id = fixture.nodes.require(*id).parent) {
            has_content_clip = has_content_clip || fixture.nodes.require(*id).clip_content;
        }
        const auto range = fixture.text_scene.primitive(text.scene).instances;
        bool coverage_culled = true;
        for (std::uint32_t index = range.first; index < range.first + range.count; ++index) {
            const auto glyph_clip = fixture.text_scene.glyph_scene().instances().at(index).clip_bounds;
            coverage_culled = coverage_culled && (glyph_clip[2] <= glyph_clip[0] || glyph_clip[3] <= glyph_clip[1]);
        }
        require(has_content_clip || counters.geometry_updates == geometry_before_scroll[text_index] || coverage_culled,
                "Gallery scroll updated glyph geometry without an inherited moving content clip");
        if (realized_before_scroll[text_index]) {
            require(counters.instance_rebuilds == rebuilds_before_scroll[text_index],
                    "Gallery scroll rebuilt already-realized glyph instances");
        } else if (fixture.text_scene.primitive(text.scene).instances.count > 0) {
            ++newly_realized;
        }
        ++text_index;
    }
    require(newly_realized > 0 && text_rebuilds_after_scroll > text_rebuilds_before_scroll,
            "Gallery scroll did not realize newly visible text");
    require(fixture.text_scene.counters().ordered_scene_rebuilds == ordered_rebuilds_before_scroll + 1,
            "Gallery long jump rebuilt text draw order more than once");

    const auto live_button = fixture.host->mounted_buttons()[definition.navigation_control_count];
    const auto& live_node = fixture.nodes.require(live_button.node);
    const ryn::runtime::Point live_point{
        live_node.bounds.x + live_node.translation.x + live_node.bounds.width * 0.5F,
        live_node.bounds.y + live_node.translation.y + live_node.bounds.height * 0.5F,
    };
    const auto hit = fixture.host->hit_test().hit_test(live_point);
    require(hit.has_value() && *hit == live_button.interaction,
            "Token Gallery scrolled HitTest did not follow subtree translation");
    require(fixture.host->components().mount_runs() == mounted_before &&
                fixture.host->components().component_count() == component_count &&
                definition.telemetry().content_runs == content_runs,
            "Token Gallery scroll remounted or reran document content");

    require(document.scroll_to(0.0F) &&
                document.apply_subtree_translation(document_root, fixture.nodes, fixture.dirty) &&
                fixture.host->layout_and_synchronize(viewport, clip, origin, 0.0F, true),
            "Token Gallery top-anchor restoration failed");
    const auto top_hit = fixture.host->hit_test().hit_test(live_point);
    require(!top_hit.has_value() || *top_hit != live_button.interaction,
            "offscreen Token Gallery control retained stale HitTest geometry");
}

ryn::input::PointerInputEvent pointer_event(ryn::input::PointerAction action, ryn::runtime::Point point,
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
    fixture.surfaces->mount(definition.content, fixture.inputs.get());
    constexpr ryn::runtime::Size viewport{1200.0F, 40000.0F};
    constexpr ryn::runtime::Rect clip{0.0F, 0.0F, 1200.0F, 40000.0F};
    require(fixture.host->layout_and_synchronize(viewport, clip, {24.0F, 20.0F}, 0.0F, true),
            "Gallery navigation fixture did not layout");

    const auto components = fixture.host->components().component_count();
    const auto mount_runs = fixture.host->components().mount_runs();
    const auto live_component = fixture.host->mounted_buttons()[definition.navigation_control_count].component;
    const auto click = [&](std::size_t index) {
        const auto button = fixture.host->mounted_buttons()[index];
        const auto& node = fixture.nodes.require(button.node);
        const ryn::runtime::Point point{
            node.bounds.x + node.translation.x + node.bounds.width * 0.5F,
            node.bounds.y + node.translation.y + node.bounds.height * 0.5F,
        };
        fixture.host->pointer().dispatch(pointer_event(ryn::input::PointerAction::move, point));
        fixture.host->pointer().dispatch(
            pointer_event(ryn::input::PointerAction::down, point, ryn::input::PointerButton::primary));
        fixture.host->pointer().dispatch(
            pointer_event(ryn::input::PointerAction::up, point, ryn::input::PointerButton::primary));
    };

    constexpr std::size_t first_category_control = 6;
    click(first_category_control);
    const auto request = definition.take_navigation_request();
    require(request.has_value() && request->kind == GalleryNavigationTargetKind::category &&
                request->category == AntDesignGalleryCategory::general,
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

    constexpr std::size_t first_filter_control = 13 + 73;
    constexpr std::size_t partial_filter_control = first_filter_control + 2;
    click(partial_filter_control);
    require(fixture.host->layout_and_synchronize(viewport, clip, {24.0F, 20.0F}, 0.0F, true),
            "Gallery partial filter did not synchronize");
    std::size_t visible = 0;
    std::size_t hidden = 0;
    const auto surfaces = fixture.surfaces->mounted_surfaces();
    for (std::size_t index = 52; index < 125; ++index) {
        const auto snapshot = fixture.surfaces->snapshot(surfaces[index].component);
        const auto& node = fixture.nodes.require(surfaces[index].node);
        if (snapshot.visible) {
            ++visible;
            require(node.bounds.width > 0.0F && node.bounds.height > 0.0F, "visible Gallery filter entry collapsed");
        } else {
            ++hidden;
            require(node.bounds.width == 0.0F && node.bounds.height == 0.0F,
                    "hidden Gallery filter entry retained layout extent");
        }
    }
    const auto expected_partial = std::ranges::count_if(ant_design_reference_entries(), [](const auto& entry) {
        return entry.support_status == GallerySupportStatus::partial;
    });
    require(visible == static_cast<std::size_t>(expected_partial) &&
                hidden == ant_design_reference_entries().size() - visible,
            "Gallery partial filter did not match the support catalog");
    const auto hidden_surface = surfaces[53].component;
    std::size_t hidden_texts = 0;
    for (const auto& text : fixture.host->text().mounted_texts()) {
        auto current = std::optional<ryn::runtime::ComponentId>{text.component};
        while (current.has_value() && *current != hidden_surface) {
            current = fixture.host->components().parent(*current);
        }
        if (current.has_value()) {
            ++hidden_texts;
            require(near(fixture.text_scene.text_state(text.scene).material().opacity, 0.0F),
                    "hidden Gallery entry retained visible glyph material");
        }
    }
    require(hidden_texts > 0, "Gallery hidden-text contract did not inspect a descendant");
    require(fixture.host->components().component_count() == components &&
                fixture.host->components().mount_runs() == mount_runs &&
                fixture.host->components().contains(live_component) && definition.telemetry().content_runs == 1 &&
                definition.telemetry().reference_content_runs == 126 &&
                definition.telemetry().navigation_requests == 2 && definition.telemetry().filter_updates == 1,
            "Gallery navigation/filter remounted content or disturbed its live sibling");

    const auto hidden_entry = ant_design_reference_entries()[1];
    require(hidden_entry.support_status != GallerySupportStatus::partial,
            "Gallery filtered-entry fixture no longer selects a hidden component");
    click(8);
    const auto component_request = definition.take_navigation_request();
    require(component_request.has_value() && component_request->kind == GalleryNavigationTargetKind::component &&
                component_request->component_identity == hidden_entry.identity,
            "Gallery component child did not emit its stable navigation identity");
    require(fixture.host->layout_and_synchronize(viewport, clip, {24.0F, 20.0F}, 0.0F, true),
            "Gallery hidden component navigation did not restore layout");
    require(fixture.surfaces->snapshot(surfaces[53].component).visible,
            "Gallery child navigation did not restore the hidden component");

    click(first_filter_control);
    require(fixture.host->layout_and_synchronize(viewport, clip, {24.0F, 20.0F}, 0.0F, true),
            "Gallery all-status filter did not synchronize");
    for (std::size_t index = 52; index < 125; ++index) {
        require(fixture.surfaces->snapshot(surfaces[index].component).visible,
                "Gallery all-status filter did not restore an entry");
    }
    for (const auto& text : fixture.host->text().mounted_texts()) {
        auto current = std::optional<ryn::runtime::ComponentId>{text.component};
        while (current.has_value() && *current != hidden_surface) {
            current = fixture.host->components().parent(*current);
        }
        if (current.has_value()) {
            require(near(fixture.text_scene.text_state(text.scene).material().opacity, 1.0F),
                    "Gallery all-status filter did not restore glyph material");
        }
    }
}

void test_responsive_navigation_and_document_reflow_preserves_identity() {
    auto definition = rynui::example::make_token_gallery_definition();
    definition.set_viewport_width(1200.0F);
    Fixture fixture;
    fixture.surfaces->mount(definition.content, fixture.inputs.get());
    const auto root_component = fixture.host->components().root_components().front();
    const auto root = fixture.host->components().root(root_component);
    const auto components = fixture.host->components().component_count();
    const auto first_surface = fixture.surfaces->mounted_surfaces().front().component;
    const auto first_control = fixture.host->mounted_buttons().front().component;

    require(fixture.host->layout_and_synchronize({1200.0F, 40000.0F}, {0.0F, 0.0F, 1200.0F, 40000.0F}, {24.0F, 20.0F},
                                                 0.0F, true),
            "wide responsive Gallery layout failed");
    const auto root_children = fixture.nodes.require(root).children;
    require(root_children.size() == 6, "responsive Gallery duplicated body, bars, or header roots");
    const auto body = root_children[0];
    const auto header = root_children[5];
    const auto wide_children = fixture.nodes.require(body).children;
    require(wide_children.size() == 2, "responsive Gallery duplicated navigation or document roots");
    const auto wide_navigation = fixture.nodes.require(wide_children[0]).bounds;
    const auto wide_document = fixture.nodes.require(wide_children[1]).bounds;
    const auto wide_header = fixture.nodes.require(header).bounds;
    require(wide_navigation.x < wide_document.x && near(wide_navigation.y, wide_document.y) &&
                near(wide_navigation.width, 216.0F) && wide_document.width > 800.0F &&
                wide_header.y < wide_navigation.y && near(wide_header.height, 56.0F),
            "wide Gallery did not produce a header above both columns");
    require(fixture.nodes.require(wide_children[0]).children.size() == 97,
            "Gallery navigation did not include every document and catalog entry");
    const auto metrics = rynui::example::gallery_layout_metrics({1200.0F, 900.0F});
    require(near(wide_document.x, metrics.document_lane.x) && near(wide_document.y, metrics.document_lane.y) &&
                near(wide_document.width, metrics.document_width) &&
                metrics.document_track.x - (wide_document.x + wide_document.width) >= 16.0F,
            "Gallery layout and scrollbar geometry disagree or overlap document content");
    for (const auto child : fixture.nodes.require(wide_children[0]).children) {
        const auto& node = fixture.nodes.require(child);
        require(node.bounds.x + node.bounds.width <= metrics.navigation_track.x + 0.25F,
                "Gallery navigation row overlaps its scrollbar");
    }
    std::size_t heading_count = 0;
    for (const auto& surface : fixture.surfaces->mounted_surfaces()) {
        if (fixture.surfaces->snapshot(surface.component).role ==
            rynui::example::ReferenceSurfaceRole::document_heading) {
            ++heading_count;
        }
    }
    require(heading_count == 6, "Gallery lost its semantic document headings");
    for (const auto& text : fixture.host->text().mounted_texts()) {
        auto parent = fixture.nodes.require(fixture.text_scene.node(text.scene)).parent;
        while (parent && *parent != wide_children[0]) {
            parent = fixture.nodes.require(*parent).parent;
        }
        if (parent) {
            require(fixture.text_scene.text_state(text.scene).measurement().lines.size() <= 1,
                    "Gallery navigation label wraps into the following row");
        }
    }
    definition.set_motion_enabled(false);
    require(definition.set_current_section(rynui::example::GalleryDocumentSectionKind::design_values) &&
                !definition.set_current_section(rynui::example::GalleryDocumentSectionKind::design_values),
            "Gallery active section failed to update or remained dirty at rest");
    const auto buttons = fixture.host->mounted_buttons();
    require(near(fixture.host->snapshot(buttons[0].component).presentation_background.alpha(), 0.0F) &&
                near(fixture.host->snapshot(buttons[2].component).presentation_background.alpha(), 0.08F),
            "Gallery active section did not move the rendered navigation highlight");

    definition.set_viewport_width(560.0F);
    require(fixture.host->layout_and_synchronize({560.0F, 40000.0F}, {0.0F, 0.0F, 560.0F, 40000.0F}, {24.0F, 20.0F},
                                                 0.0F, true),
            "narrow responsive Gallery layout failed");
    const auto narrow_children = fixture.nodes.require(body).children;
    const auto narrow_navigation = fixture.nodes.require(narrow_children[0]).bounds;
    const auto narrow_document = fixture.nodes.require(narrow_children[1]).bounds;
    require(near(narrow_navigation.x, narrow_document.x) && narrow_navigation.y < narrow_document.y &&
                near(narrow_navigation.width, 512.0F) && near(narrow_document.width, 488.0F),
            "narrow Gallery did not stack wrapped navigation above the document");
    rynui::example::GalleryDocumentViewport shared_scroll;
    const float narrow_extent = fixture.nodes.require(body).bounds.height;
    require(shared_scroll.set_extents(600.0F, narrow_extent) &&
                shared_scroll.scroll_to(shared_scroll.snapshot().maximum_offset) &&
                shared_scroll.apply_subtree_translation(body, fixture.nodes, fixture.dirty) &&
                fixture.host->layout_and_synchronize({560.0F, 40000.0F}, {0.0F, 0.0F, 560.0F, 40000.0F}, {24.0F, 20.0F},
                                                     0.0F, true) &&
                fixture.nodes.require(narrow_children[0]).translation.y == -shared_scroll.snapshot().offset &&
                fixture.nodes.require(narrow_children[1]).translation.y == -shared_scroll.snapshot().offset &&
                fixture.nodes.require(header).translation == ryn::runtime::Point{},
            "narrow Gallery did not keep both columns in one scroll region");
    rynui::example::GalleryScrollRange short_navigation;
    short_navigation.set_extents(600.0F, 100.0F);
    const auto navigation_bar =
        rynui::example::gallery_scrollbar_geometry({526.0F, 88.0F, 8.0F, 600.0F}, short_navigation.snapshot());
    const auto document_bar = rynui::example::gallery_scrollbar_geometry(
        {526.0F, 88.0F, 8.0F, 600.0F},
        {600.0F, narrow_extent, shared_scroll.snapshot().maximum_offset, shared_scroll.snapshot().offset});
    require(definition.set_scrollbars(navigation_bar, document_bar, true),
            "narrow Gallery scrollbar presentation did not update");
    const auto chrome = fixture.surfaces->mounted_surfaces();
    require(!fixture.surfaces->snapshot(chrome[126].component).visible &&
                !fixture.surfaces->snapshot(chrome[127].component).visible &&
                fixture.surfaces->snapshot(chrome[128].component).visible &&
                fixture.surfaces->snapshot(chrome[129].component).visible,
            "narrow Gallery did not keep exactly one visible scrollbar");
    require(shared_scroll.scroll_to(0.0F) &&
                shared_scroll.apply_subtree_translation(body, fixture.nodes, fixture.dirty),
            "narrow Gallery could not restore its shared scroll origin");
    for (std::size_t index = 52; index < 125; ++index) {
        const auto& node = fixture.nodes.require(fixture.surfaces->mounted_surfaces()[index].node);
        require(node.bounds.width > 0.0F && node.bounds.x >= narrow_document.x &&
                    node.bounds.x + node.bounds.width <= narrow_document.x + narrow_document.width + 0.25F,
                "narrow Gallery made a catalog entry horizontally unreachable");
    }

    definition.set_viewport_width(1200.0F);
    require(fixture.host->layout_and_synchronize({1200.0F, 40000.0F}, {0.0F, 0.0F, 1200.0F, 40000.0F}, {24.0F, 20.0F},
                                                 0.0F, true),
            "wide Gallery restoration failed");
    const auto restored_navigation = fixture.nodes.require(wide_children[0]).bounds;
    const auto restored_document = fixture.nodes.require(wide_children[1]).bounds;
    require(restored_navigation.x < restored_document.x && fixture.host->components().component_count() == components &&
                fixture.host->components().contains(first_surface) &&
                fixture.host->components().contains(first_control) && definition.telemetry().content_runs == 1,
            "wide/narrow Gallery reflow rebuilt retained identities");
}

void test_token_gallery_frame_contract() {
    auto palette = rynui::example::make_token_gallery_definition();
    require(palette.background_color() == ryn::Color::rgba8(255, 255, 255),
            "Token Gallery default clear color is not white");
    palette.smoke_step(0);
    require(palette.background_color() == ryn::Color::rgba8(20, 20, 20),
            "Token Gallery dark clear color did not follow Theme");
    palette.smoke_step(1);
    require(palette.background_color() == ryn::Color::rgba8(255, 255, 255),
            "Token Gallery compact clear color did not restore Theme");
    auto definition = rynui::example::make_token_gallery_definition();
    require(definition.stable_test_ids.size() == 251, "Token Gallery stable test-id inventory is incomplete");
    for (const auto id : definition.stable_test_ids) {
        if (id.starts_with("ant.")) {
            if (ryn::find_ant_design_token(id) == nullptr) {
                throw std::runtime_error("Token Gallery test id is not backed by the locked catalog: " +
                                         std::string(id));
            }
        }
    }

    Fixture fixture;
    definition.set_viewport_width(1200.0F);
    fixture.surfaces->mount(definition.content, fixture.inputs.get());
    require(fixture.host->mounted_buttons().size() == definition.navigation_control_count + 204,
            "Token Gallery live sample count drifted");
    require(fixture.surfaces->mounted_surfaces().size() == 131 &&
                fixture.surfaces->snapshot(fixture.surfaces->mounted_surfaces().back().component).role ==
                    rynui::example::ReferenceSurfaceRole::site_header,
            "Token Gallery document and header surface count drifted");
    require(fixture.selections->mounted().size() == 45 && fixture.selections->checkbox_groups().size() == 2 &&
                fixture.selections->radio_groups().size() == 4,
            "Token Gallery selection and Group samples did not mount");
    // Headers/arrows and hidden panel controls stay retained; only the active branches paint.
    if (fixture.host->interactions().size() != definition.navigation_control_count + 782) {
        throw std::runtime_error(
            "Token Gallery interaction inventory: actual=" + std::to_string(fixture.host->interactions().size()) +
            " navigation=" + std::to_string(definition.navigation_control_count));
    }
    require(fixture.host->services().typography().mounted().size() == 4 &&
                fixture.host->services().divider().mounted().size() == 11,
            "Gallery Typography actions or Divider examples absent");

    RecordingGpuApi gpu;
    RecordingDrawApi draw;
    IdleEvents events;
    HeadlessSubmitter submitter(*fixture.host, fixture.text_scene, fixture.frames, gpu, draw);
    auto& animation_deadlines = *fixture.host;
    ryn::runtime::OnDemandFrameLoop loop(fixture.frames, events, submitter, animation_deadlines, 5);
    require(loop.step() == ryn::runtime::FrameLoopStep::submitted,
            "Token Gallery initial wide frame was not submitted");
    require_all_cells_reachable(fixture, {1200.0F, 40000.0F});
    // Closed popup subtrees stay out of the visible scene; selectors/actions remain.
    if (fixture.host->scene_composer().interaction_order().size() != definition.navigation_control_count + 464) {
        throw std::runtime_error("Token Gallery visible action inventory drifted: actual=" +
                                 std::to_string(fixture.host->scene_composer().interaction_order().size()) +
                                 " navigation=" + std::to_string(definition.navigation_control_count));
    }

    const auto initial = definition.telemetry();
    require(initial.content_runs == 1 && initial.theme_content_runs == 67 && initial.document_sections == 6 &&
                initial.component_entries == 73 && initial.reference_surfaces == 126 &&
                initial.reference_content_runs == 126 && initial.live_samples == 269,
            "Token Gallery Theme content did not mount exactly once");
    require(gpu.quad_uploads == 1 && gpu.glyph_buffer_uploads == 1 && gpu.effect_uploads == 1 && draw.quad_draws > 0 &&
                draw.glyph_draws > 0 && draw.effect_draws > 0,
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

    const auto focus_target = fixture.host->mounted_buttons()[definition.navigation_control_count + 9];
    const auto hidden_focus = fixture.host->button_scene().focus_effect(focus_target.scene);
    require(hidden_focus.geometry.kind == ryn::graphics::RoundedEffectKind::outline &&
                near(hidden_focus.geometry.outline_offset, 1.0F) && near(hidden_focus.geometry.outline_width, 3.0F) &&
                near(hidden_focus.material.opacity, 0.0F),
            "Token Gallery focus cell does not use a 1px gap and 3px hollow ring");

    definition.set_viewport_width(560.0F);
    submitter.set_viewport({560.0F, 40000.0F});
    require(loop.step() == ryn::runtime::FrameLoopStep::submitted, "Token Gallery narrow frame was not submitted");
    require_all_cells_reachable(fixture, {560.0F, 40000.0F});

    definition.set_viewport_width(1200.0F);
    submitter.set_viewport({1200.0F, 40000.0F});
    require(loop.step() == ryn::runtime::FrameLoopStep::submitted, "Token Gallery wide restoration was not submitted");
    const auto component_count = fixture.host->components().component_count();
    const std::vector<ryn::detail::MountedButtonComponent> retained_buttons{fixture.host->mounted_buttons().begin(),
                                                                            fixture.host->mounted_buttons().end()};
    const auto effect_uploads = gpu.effect_uploads;
    const auto quad_uploads = gpu.quad_uploads;

    definition.smoke_step(0);
    require(loop.step() == ryn::runtime::FrameLoopStep::submitted, "Dark Theme did not submit a Token Gallery frame");
    advance_animation_frame(loop, "Dark Theme did not submit an animation deadline frame");
    require(gpu.effect_uploads > effect_uploads && gpu.quad_uploads > quad_uploads,
            "Theme update did not produce local retained uploads");
    definition.smoke_step(1);
    require(loop.step() == ryn::runtime::FrameLoopStep::submitted,
            "Compact Theme did not submit a Token Gallery frame");
    advance_animation_frame(loop, "Compact Theme did not submit an animation deadline frame");
    definition.smoke_step(2);
    require(loop.step() == ryn::runtime::FrameLoopStep::submitted, "Brand Seed did not submit a Token Gallery frame");
    advance_animation_frame(loop, "Brand Seed did not submit an animation deadline frame");
    definition.smoke_step(3);
    require(loop.step() == ryn::runtime::FrameLoopStep::submitted,
            "disabled/loading update did not submit a Token Gallery frame");
    static_cast<void>(drain_animation_frames(*fixture.host, loop,
                                             "disabled/loading animations did not settle through frame deadlines"));
    definition.smoke_step(4);
    require(loop.step() == ryn::runtime::FrameLoopStep::submitted,
            "Default Theme restoration did not submit a Token Gallery frame");
    static_cast<void>(
        drain_animation_frames(*fixture.host, loop, "Default Theme animations did not settle through frame deadlines"));

    fixture.host->focus().dispatch({
        ryn::input::Key::tab,
        ryn::input::KeyAction::down,
        ryn::input::KeyModifier::none,
        false,
    });
    require(loop.step() == ryn::runtime::FrameLoopStep::submitted, "keyboard focus-visible transition did not submit");
    const auto focused = fixture.host->mounted_buttons().front();
    const auto visible_focus = fixture.host->button_scene().focus_effect(focused.scene);
    require(near(visible_focus.material.opacity, 1.0F) && near(visible_focus.geometry.outline_offset, 1.0F) &&
                near(visible_focus.geometry.outline_width, 3.0F),
            "keyboard focus-visible transition lost the hollow outline");

    const auto hover = fixture.host->mounted_buttons()[definition.navigation_control_count + 7];
    const auto bounds = fixture.nodes.require(hover.node).bounds;
    const ryn::runtime::Point inside{
        bounds.x + bounds.width * 0.5F,
        bounds.y + bounds.height * 0.5F,
    };
    fixture.host->pointer().dispatch(pointer_event(ryn::input::PointerAction::move, inside));
    require(loop.step() == ryn::runtime::FrameLoopStep::submitted, "Gallery hover state did not submit");
    fixture.host->pointer().dispatch(
        pointer_event(ryn::input::PointerAction::down, inside, ryn::input::PointerButton::primary));
    require(loop.step() == ryn::runtime::FrameLoopStep::submitted, "Gallery active state did not submit");
    fixture.host->pointer().dispatch(
        pointer_event(ryn::input::PointerAction::up, inside, ryn::input::PointerButton::primary));
    require(loop.step() == ryn::runtime::FrameLoopStep::submitted, "Gallery active release did not submit");

    static_cast<void>(drain_animation_frames(
        *fixture.host, loop, "Token Gallery interaction animations did not settle through frame deadlines"));

    const auto final = definition.telemetry();
    require(final.content_runs == initial.content_runs && final.theme_content_runs == initial.theme_content_runs &&
                fixture.host->components().component_count() == component_count &&
                std::ranges::equal(retained_buttons, fixture.host->mounted_buttons(),
                                   [](const auto& before, const auto& after) {
                                       return before.component == after.component && before.node == after.node &&
                                              before.scene == after.scene && before.interaction == after.interaction &&
                                              before.fragment == after.fragment;
                                   }) &&
                final.theme_updates == 4 && final.brand_updates == 1 && final.state_updates == 2 &&
                final.viewport_updates >= 4,
            "Token Gallery updates reran content or replaced retained component/scene owners");

    const auto submissions = loop.counters().submissions;
    const auto settled_rebuilds = fixture.host->scene_composer().diagnostics().rebuilds;
    for (int index = 0; index < 40; ++index) {
        require(loop.step() == ryn::runtime::FrameLoopStep::idle, "Idle Token Gallery continued submitting frames");
    }
    require(loop.counters().submissions == submissions && loop.counters().idle_waits >= 40 &&
                fixture.host->scene_composer().diagnostics().rebuilds == settled_rebuilds,
            "Token Gallery idle acceptance counters drifted");
}

void test_live_input_samples() {
    auto definition = rynui::example::make_token_gallery_definition();
    definition.set_viewport_width(1200.0F);
    Fixture fixture;
    fixture.host->set_motion_preference(ryn::animation::MotionPreference::reduced);
    fixture.surfaces->mount(definition.content, fixture.inputs.get());
    RecordingGpuApi gpu;
    RecordingDrawApi draw;
    IdleEvents events;
    HeadlessSubmitter submitter(*fixture.host, fixture.text_scene, fixture.frames, gpu, draw);
    ryn::runtime::OnDemandFrameLoop loop(fixture.frames, events, submitter, *fixture.host, 5);
    require(loop.step() == ryn::runtime::FrameLoopStep::submitted, "Input Gallery initial frame failed");
    for (int i = 0; i < 4; ++i) {
        const auto step = loop.step();
        if (step == ryn::runtime::FrameLoopStep::idle) {
            break;
        }
        require(step == ryn::runtime::FrameLoopStep::submitted, "Input Gallery warmup frame failed");
    }
    require(fixture.inputs->mounted_inputs().size() == 131 &&
                fixture.host->services().select().mounted().size() == 20 &&
                fixture.host->services().card().mounted().size() == 7 &&
                fixture.host->services().tabs().mounted().size() == 14 &&
                fixture.host->services().modal().mounted().size() == 13,
            "Gallery Input/Search/Password/Typography/TextArea/OTP/Select samples absent");
    const auto samples = fixture.inputs->mounted_inputs();
    const auto variants = std::array{ryn::InputVariant::Outlined, ryn::InputVariant::Filled,
                                     ryn::InputVariant::Borderless, ryn::InputVariant::Underlined};
    for (std::size_t index = 0; index < variants.size(); ++index) {
        require(fixture.inputs->variant(samples[15 + index].component) == variants[index],
                "Gallery four Input variants did not mount");
    }
    require(fixture.inputs->count_value(samples[19].component) == 4 &&
                fixture.inputs->status(samples[19].component) == ryn::InputStatus::Error &&
                fixture.inputs->count_value(samples[20].component) == 5 &&
                fixture.inputs->variant(samples[25].component) == ryn::InputVariant::Filled &&
                fixture.inputs->variant(samples[26].component) == ryn::InputVariant::Underlined,
            "Gallery statistics or connected Search feature sample differs");
    for (std::size_t index = 27; index < 35; ++index) {
        require(fixture.inputs->editors().require(samples[index].editor).mode() ==
                        ryn::input::TextEditMode::MultiLine &&
                    fixture.inputs->caret_map(samples[index].component).line_count() >= 2,
                "Gallery TextArea samples did not mount multiline editor/carets");
    }
    for (std::size_t index = 0; index < variants.size(); ++index) {
        require(fixture.inputs->variant(samples[27 + index].component) == variants[index],
                "Gallery TextArea variant sample differs");
    }
    const auto controlled = fixture.inputs->mounted_inputs()[0];
    require(fixture.host->services().otp().mounted().size() == 10, "Gallery OTP group inventory differs");
    for (std::size_t index = 35; index < 81; ++index) {
        require(fixture.inputs->editors().require(samples[index].editor).mode() == ryn::input::TextEditMode::SingleLine,
                "Gallery OTP cell was not a retained single-line editor");
    }
    require(fixture.inputs->editors().require(samples[82].editor).mode() == ryn::input::TextEditMode::MultiLine &&
                fixture.inputs->editors().require(samples[81].editor).value() == "A אבג 12 مرحبا" &&
                fixture.inputs->editors().require(samples[83].editor).value() == "אבג مرحبا 👩‍💻",
            "Gallery bidi Input/TextArea/Password samples missing");
    const auto uncontrolled = fixture.inputs->mounted_inputs()[1];
    const auto controlled_layers = fixture.inputs->text_layers(controlled.component);
    const auto uncontrolled_layers = fixture.inputs->text_layers(uncontrolled.component);
    const auto initial = definition.telemetry();
    const auto components = fixture.host->components().component_count();
    const auto text_scenes = fixture.text_scene.size();
    const auto quads = fixture.host->button_scene().instances().size();
    const auto effects = fixture.host->rounded_effects().live_count();
    require(fixture.host->focus().request_focus(controlled.interaction, ryn::input::FocusModality::keyboard),
            "Gallery Input focus failed");
    const auto stamp = fixture.inputs->sessions().active();
    require(bool(fixture.inputs->dispatch(ryn::input::CompositionChanged{ryn::String{u8"ni"}, {2, 0}, stamp})),
            "Gallery preedit failed");
    require(definition.telemetry().input_changes == 0, "Gallery preedit emitted change");
    require(bool(fixture.inputs->dispatch(ryn::input::TextCommitted{ryn::String{u8"你🙂"}, stamp})),
            "Gallery controlled commit failed");
    require(fixture.inputs->editors().require(controlled.editor).value() == ryn::String{u8"你🙂"}.bytes() &&
                definition.telemetry().input_changes == 1,
            "Gallery controlled echo failed");
    require(loop.step() == ryn::runtime::FrameLoopStep::submitted, "Gallery Input editing frame failed");
    require(fixture.inputs->synchronize_input_area(1, 1200, 40000) && fixture.platform.areas > 0,
            "Gallery Input area failed");
    fixture.host->focus().dispatch({ryn::input::Key::enter, ryn::input::KeyAction::down});
    require(definition.telemetry().input_submits == 1, "Gallery Input submit callback absent");
    require(fixture.host->focus().request_focus(uncontrolled.interaction, ryn::input::FocusModality::keyboard),
            "Gallery uncontrolled focus failed");
    fixture.host->focus().dispatch({ryn::input::Key::a, ryn::input::KeyAction::down, ryn::input::KeyModifier::control});
    fixture.platform.clipboard = ryn::String{u8"Latin\n中文"};
    fixture.host->focus().dispatch({ryn::input::Key::v, ryn::input::KeyAction::down, ryn::input::KeyModifier::control});
    require(fixture.inputs->editors().require(uncontrolled.editor).value() == ryn::String{u8"Latin中文"}.bytes() &&
                definition.telemetry().input_changes == 2,
            "Gallery uncontrolled paste failed");
    require(loop.step() == ryn::runtime::FrameLoopStep::submitted, "Gallery uncontrolled frame failed");
    const auto search_input = fixture.inputs->mounted_inputs()[2];
    const auto button_for_input = [&](std::size_t index) {
        const auto parent = fixture.host->components().parent(fixture.inputs->mounted_inputs()[index].component);
        const auto buttons = fixture.host->mounted_buttons();
        const auto found = std::find_if(buttons.begin(), buttons.end(), [&](const auto& button) {
            return fixture.host->components().parent(button.component) == parent;
        });
        require(found != buttons.end(), "Gallery Search sibling button is absent");
        return *found;
    };
    const auto search_button = button_for_input(2);
    const auto second_search_button = button_for_input(3);
    require(fixture.host->components().parent(search_input.component) ==
                    fixture.host->components().parent(search_button.component) &&
                &fixture.inputs->editors() == &fixture.host->services().text_edit()->editors(),
            "Gallery Search did not reuse Input/Button or window editor ownership");
    require(fixture.host->focus().request_focus(search_input.interaction, ryn::input::FocusModality::keyboard),
            "Gallery Search Input focus failed");
    require(bool(fixture.inputs->dispatch(
                ryn::input::TextCommitted{ryn::String{u8"搜索"}, fixture.inputs->sessions().active()})),
            "Gallery Search commit failed");
    fixture.host->focus().dispatch({ryn::input::Key::enter, ryn::input::KeyAction::down});
    require(definition.telemetry().search_submits == 1, "Gallery Search Enter did not submit");
    fixture.host->focus().dispatch({ryn::input::Key::tab, ryn::input::KeyAction::down});
    require(fixture.host->focus().state().focused == search_button.interaction,
            "Gallery Search Tab did not reach its Button");
    fixture.host->focus().dispatch({ryn::input::Key::space, ryn::input::KeyAction::down});
    fixture.host->focus().dispatch({ryn::input::Key::space, ryn::input::KeyAction::up});
    require(definition.telemetry().search_submits == 2, "Gallery Search Button did not submit");
    require(fixture.host->focus().request_focus(second_search_button.interaction, ryn::input::FocusModality::keyboard),
            "Gallery second Search Button focus failed");
    fixture.host->focus().dispatch({ryn::input::Key::enter, ryn::input::KeyAction::down});
    require(definition.telemetry().search_submits == 3, "Gallery second Search Button did not submit");
    require(loop.step() == ryn::runtime::FrameLoopStep::submitted, "Gallery Search frame did not submit");
    fixture.inputs->set_window_active(false);
    require(loop.step() == ryn::runtime::FrameLoopStep::submitted, "Gallery blur frame failed");
    const auto final = definition.telemetry();
    require(final.content_runs == initial.content_runs, "Gallery Input reran root content");
    require(final.theme_content_runs == initial.theme_content_runs, "Gallery Input reran Theme content");
    require(fixture.host->components().component_count() == components, "Gallery Input changed component topology");
    require(fixture.inputs->mounted_inputs()[0].component == controlled.component &&
                fixture.inputs->mounted_inputs()[0].editor == controlled.editor &&
                fixture.inputs->mounted_inputs()[1].component == uncontrolled.component &&
                fixture.inputs->mounted_inputs()[1].editor == uncontrolled.editor,
            "Gallery Input changed owner identity");
    require(fixture.inputs->text_layers(controlled.component).base == controlled_layers.base &&
                fixture.inputs->text_layers(controlled.component).selected == controlled_layers.selected &&
                fixture.inputs->text_layers(controlled.component).placeholder == controlled_layers.placeholder &&
                fixture.inputs->text_layers(uncontrolled.component).base == uncontrolled_layers.base &&
                fixture.inputs->text_layers(uncontrolled.component).selected == uncontrolled_layers.selected &&
                fixture.inputs->text_layers(uncontrolled.component).placeholder == uncontrolled_layers.placeholder,
            "Gallery Input changed retained text identity");
    require(fixture.text_scene.size() == text_scenes && fixture.host->button_scene().instances().size() == quads &&
                fixture.host->rounded_effects().live_count() == effects,
            "Gallery Input changed retained scene capacity");
    const auto submissions = loop.counters().submissions;
    for (int i = 0; i < 4; ++i) {
        require(loop.step() == ryn::runtime::FrameLoopStep::idle, "Gallery Input did not idle");
    }
    require(loop.counters().submissions == submissions && fixture.platform.starts == fixture.platform.stops,
            "Gallery Input retained submits/session");
}

void test_select_popup_after_document_scroll() {
    auto definition = rynui::example::make_token_gallery_definition();
    definition.set_viewport_width(1200.0F);
    Fixture fixture;
    fixture.host->set_motion_preference(ryn::animation::MotionPreference::reduced);
    fixture.surfaces->mount(definition.content, fixture.inputs.get());
    const auto synchronize = [&] {
        require(fixture.host->layout_and_synchronize({1200, 900}, {0, 0, 1200, 900}, {24, 20}, 0, true),
                "Gallery Select synchronization failed");
    };
    synchronize();
    auto& selects = fixture.host->services().select();
    const auto id = selects.mounted().front();
    const auto initial = selects.snapshot(id);
    const auto selector_node = fixture.host->components().root(initial.selector);
    const auto root = fixture.host->components().root(fixture.host->components().root_components().front());
    const auto body = fixture.nodes.require(root).children.front();
    const auto document = fixture.nodes.require(body).children[1];
    rynui::example::GalleryScrollTranslation scroll;
    require(scroll.apply(document, fixture.nodes.require(selector_node).bounds.y - 160, fixture.nodes, fixture.dirty)
                .changed,
            "Gallery Select document scroll failed");
    synchronize();
    const auto telemetry = definition.telemetry();
    const auto mounts = fixture.host->components().mount_runs();
    const auto click = [&](ryn::runtime::ComponentId component) {
        const auto& node = fixture.nodes.require(fixture.host->components().root(component));
        const ryn::runtime::Point point{node.bounds.x + node.translation.x + node.bounds.width / 2,
                                        node.bounds.y + node.translation.y + node.bounds.height / 2};
        fixture.host->pointer().dispatch(
            pointer_event(ryn::input::PointerAction::down, point, ryn::input::PointerButton::primary));
        fixture.host->pointer().dispatch(
            pointer_event(ryn::input::PointerAction::up, point, ryn::input::PointerButton::primary));
        synchronize();
    };
    click(initial.selector);
    const auto opened = selects.snapshot(id);
    const auto& selector = fixture.nodes.require(selector_node);
    require(opened.visible && opened.popup.y >= selector.bounds.y + selector.translation.y + selector.bounds.height &&
                opened.popup.y < 240 && opened.rows == initial.rows,
            "Gallery Select popup did not follow the translated selector");
    require(fixture.nodes.require(fixture.host->components().root(opened.rows[1])).translation == ryn::runtime::Point{},
            "Gallery document scroll translated a window-layer option twice");
    click(opened.rows[1]);
    const auto selected = selects.snapshot(id);
    require(!selected.visible && selected.value == ryn::SelectSelection{u8"service"} &&
                selected.selector == initial.selector && selected.rows == initial.rows &&
                definition.telemetry().activations == telemetry.activations + 1 &&
                definition.telemetry().content_runs == telemetry.content_runs &&
                fixture.host->components().mount_runs() == mounts,
            "Gallery translated option did not select through its controlled binding without remount");
}

void test_card_gallery_loading_restores_editor() {
    auto definition = rynui::example::make_token_gallery_definition();
    definition.set_viewport_width(1200.0F);
    Fixture fixture;
    fixture.host->set_motion_preference(ryn::animation::MotionPreference::reduced);
    fixture.surfaces->mount(definition.content, fixture.inputs.get());
    RecordingGpuApi gpu;
    RecordingDrawApi draw;
    IdleEvents events;
    HeadlessSubmitter submitter(*fixture.host, fixture.text_scene, fixture.frames, gpu, draw);
    ryn::runtime::OnDemandFrameLoop loop(fixture.frames, events, submitter, *fixture.host, 5);
    require(loop.step() == ryn::runtime::FrameLoopStep::submitted, "Card Gallery initial frame failed");
    auto& cards = fixture.host->services().card();
    require(cards.mounted().size() == 7, "Card Gallery samples are missing");
    const auto account = cards.mounted()[1];
    const auto before = cards.snapshot(account);
    const auto input = fixture.inputs->mounted_inputs()[100];
    require(fixture.host->focus().request_focus(input.interaction, ryn::input::FocusModality::keyboard),
            "Card Gallery editor focus failed");
    static_cast<void>(fixture.inputs->editors().require(input.editor).select_all());
    require(bool(fixture.inputs->dispatch(ryn::input::TextCommitted{ryn::String{u8"保留账户编辑 / Saved draft"},
                                                                    fixture.inputs->sessions().active()})),
            "Card Gallery editor input failed");
    require(loop.step() == ryn::runtime::FrameLoopStep::submitted, "Card Gallery edit frame failed");
    const auto telemetry = definition.telemetry();
    const auto components = fixture.host->components().component_count();
    const auto toggle = button_with_label(fixture, "gallery.card.toggle-loading");
    const auto click_toggle = [&] {
        const auto& node = fixture.nodes.require(toggle.node);
        const ryn::runtime::Point point{node.bounds.x + node.bounds.width / 2, node.bounds.y + node.bounds.height / 2};
        fixture.host->pointer().dispatch(
            pointer_event(ryn::input::PointerAction::down, point, ryn::input::PointerButton::primary));
        fixture.host->pointer().dispatch(
            pointer_event(ryn::input::PointerAction::up, point, ryn::input::PointerButton::primary));
        require(loop.step() == ryn::runtime::FrameLoopStep::submitted, "Card Gallery loading frame failed");
    };
    click_toggle();
    require(cards.snapshot(account).loading && !fixture.host->components().branch_active(before.body) &&
                !fixture.inputs->sessions().active().valid(),
            "Card Gallery loading retained its editor session");
    click_toggle();
    require(!cards.snapshot(account).loading && cards.snapshot(account).body == before.body &&
                fixture.inputs->mounted_inputs()[100].editor == input.editor &&
                fixture.inputs->editors().require(input.editor).value() ==
                    ryn::String{u8"保留账户编辑 / Saved draft"}.bytes() &&
                fixture.host->components().component_count() == components &&
                definition.telemetry().content_runs == telemetry.content_runs &&
                definition.telemetry().theme_content_runs == telemetry.theme_content_runs &&
                definition.telemetry().activations == telemetry.activations + 2,
            "Card Gallery loading remounted content or lost its draft");
    static_cast<void>(drain_animation_frames(*fixture.host, loop, "Card Gallery did not settle idle"));
}

void test_tabs_gallery_retains_scrolled_panel() {
    using namespace ryn;
    auto definition = rynui::example::make_token_gallery_definition();
    definition.set_viewport_width(1200);
    Fixture f;
    f.host->set_motion_preference(animation::MotionPreference::reduced);
    f.surfaces->mount(definition.content, f.inputs.get());
    const auto synchronize = [&] {
        require(f.host->layout_and_synchronize({1200, 900}, {0, 0, 1200, 900}, {24, 20}, 0, true),
                "Tabs Gallery synchronization failed");
    };
    synchronize();
    auto& tabs = f.host->services().tabs();
    const auto id = tabs.mounted().front();
    const auto original = tabs.snapshot(id);
    const auto editor = f.inputs->mounted_inputs()[101];
    const auto select = f.host->services().select().mounted()[16];
    const auto root = f.host->components().root(f.host->components().root_components().front());
    const auto document = f.nodes.require(f.nodes.require(root).children.front()).children[1];
    rynui::example::GalleryScrollTranslation scroll;
    static_cast<void>(
        scroll.apply(document, f.nodes.require(f.host->components().root(id)).bounds.y - 120, f.nodes, f.dirty));
    synchronize();
    const auto click = [&](runtime::ComponentId component) {
        const auto& node = f.nodes.require(f.host->components().root(component));
        const runtime::Point point{node.bounds.x + node.translation.x + node.bounds.width / 2,
                                   node.bounds.y + node.translation.y + node.bounds.height / 2};
        f.host->pointer().dispatch(pointer_event(input::PointerAction::down, point, input::PointerButton::primary));
        f.host->pointer().dispatch(pointer_event(input::PointerAction::up, point, input::PointerButton::primary));
        synchronize();
    };
    require(f.host->focus().request_focus(editor.interaction, input::FocusModality::keyboard),
            "Tabs Gallery focus failed");
    static_cast<void>(f.inputs->editors().require(editor.editor).select_all());
    require(bool(f.inputs->dispatch(input::TextCommitted{String{u8"跨标签保留的草稿"}, f.inputs->sessions().active()})),
            "Tabs Gallery edit failed");
    synchronize();
    click(f.host->services().select().snapshot(select).selector);
    require(f.host->services().select().snapshot(select).visible, "Tabs Gallery nested Select did not open");
    const auto telemetry = definition.telemetry();
    const auto mounts = f.host->components().mount_runs();
    click(original.headers[1]);
    require(tabs.snapshot(id).active == 1 && !f.host->services().select().snapshot(select).visible &&
                !f.inputs->sessions().active().valid(),
            "Tabs Gallery switch retained old popup/session");
    click(original.headers[0]);
    require(tabs.snapshot(id).active == 0 && tabs.snapshot(id).panels == original.panels &&
                f.inputs->mounted_inputs()[101].editor == editor.editor &&
                f.inputs->editors().require(editor.editor).value() == String{u8"跨标签保留的草稿"}.bytes() &&
                f.host->components().mount_runs() == mounts &&
                definition.telemetry().activations == telemetry.activations + 2 &&
                definition.telemetry().content_runs == telemetry.content_runs &&
                definition.telemetry().theme_content_runs == telemetry.theme_content_runs,
            "Tabs Gallery controlled binding remounted or lost the draft");
}

void test_modal_gallery_after_document_scroll() {
    using namespace ryn;
    auto definition = rynui::example::make_token_gallery_definition();
    definition.set_viewport_width(1200);
    definition.set_motion_enabled(false);
    Fixture f;
    f.host->set_motion_preference(animation::MotionPreference::reduced);
    f.surfaces->mount(definition.content, f.inputs.get());
    auto& services = f.host->services();
    const auto sync = [&] {
        for (int frame = 0; frame < 3; ++frame) {
            require(f.host->layout_and_synchronize({1200, 900}, {0, 0, 1200, 900}, {24, 20}, 0, true),
                    "Modal Gallery layout failed");
        }
    };
    const auto click = [&](runtime::ComponentId component) {
        const auto& node = f.nodes.require(services.components().root(component));
        const runtime::Point point{node.bounds.x + node.translation.x + node.bounds.width / 2,
                                   node.bounds.y + node.translation.y + node.bounds.height / 2};
        for (auto action : {input::PointerAction::down, input::PointerAction::up}) {
            f.host->pointer().dispatch(pointer_event(action, point, input::PointerButton::primary));
        }
        sync();
    };
    const auto key = [&](input::Key value) {
        f.host->focus().dispatch({value, input::KeyAction::down});
        f.host->focus().dispatch({value, input::KeyAction::up});
        sync();
    };
    sync();
    const auto trigger = button_with_label(f, "gallery.modal.retained-edit");
    const auto root = services.components().root(services.components().root_components().front());
    const auto document = f.nodes.require(f.nodes.require(root).children.front()).children[1];
    rynui::example::GalleryScrollTranslation scroll;
    const float offset = f.nodes.require(trigger.node).bounds.y - 120;
    require(scroll.apply(document, offset, f.nodes, f.dirty).changed, "Modal Gallery scroll absent");
    sync();
    const auto count = services.components().component_count();
    const auto telemetry = definition.telemetry();
    const auto id = services.modal().mounted()[1];
    const auto closed = services.modal().snapshot(id);
    require(!closed.visible && !f.inputs->sessions().active().valid(), "closed Gallery Modal started input");
    require(f.host->focus().request_focus(trigger.interaction, input::FocusModality::keyboard),
            "Gallery Modal trigger focus failed");
    click(trigger.component);
    auto shown = services.modal().snapshot(id);
    require(shown.visible && shown.topmost && near(shown.bounds.y, 100) && shown.bounds.x >= 0 &&
                f.nodes.require(services.components().root(shown.overlay)).translation == runtime::Point{},
            "Gallery document translated the Modal window layer");
    const auto editor = f.inputs->mounted_inputs()[104];
    require(f.host->focus().state().focused == editor.interaction, "Gallery Modal initial editor focus failed");
    static_cast<void>(f.inputs->editors().require(editor.editor).select_all());
    const String draft{u8"保留对话框草稿 / Saved modal draft"};
    require(bool(f.inputs->dispatch(input::TextCommitted{draft, f.inputs->sessions().active()})),
            "Gallery Modal draft edit failed");
    sync();
    const auto select = services.select().mounted()[17];
    click(services.select().snapshot(select).selector);
    require(services.select().snapshot(select).visible, "Gallery Modal child Select did not open");
    key(input::Key::escape);
    require(!services.select().snapshot(select).visible && services.modal().snapshot(id).visible,
            "Gallery Modal child Escape closed the dialog");
    require(f.inputs->dispatch(input::ScrollInputEvent{0, -1, 2, 2}) &&
                near(f.nodes.require(document).translation.y, -offset),
            "Gallery Modal background wheel leaked to document");
    key(input::Key::escape);
    require(!services.modal().snapshot(id).visible && !f.inputs->sessions().active().valid() &&
                f.host->focus().state().focused == trigger.interaction,
            "Gallery Modal close did not restore trigger and input");
    click(trigger.component);
    require(services.modal().snapshot(id).body == closed.body &&
                f.inputs->mounted_inputs()[104].editor == editor.editor &&
                f.inputs->editors().require(editor.editor).value() == draft.bytes(),
            "Gallery Modal reopen lost retained editor identity or draft");
    click(services.modal().snapshot(id).ok);
    const auto nested_trigger = button_with_label(f, "gallery.modal.nested");
    require(scroll.apply(document, f.nodes.require(nested_trigger.node).bounds.y - 120, f.nodes, f.dirty).changed,
            "nested Gallery Modal scroll absent");
    sync();
    click(nested_trigger.component);
    const auto parent = services.modal().mounted()[9];
    const auto child = services.modal().mounted()[10];
    click(button_with_label(f, "打开下一层").component);
    require(services.modal().snapshot(child).topmost && services.modal().snapshot(parent).visible &&
                !services.modal().snapshot(parent).topmost,
            "Gallery nested Modal did not become the input boundary");
    key(input::Key::escape);
    require(!services.modal().snapshot(child).visible && services.modal().snapshot(parent).topmost,
            "Gallery nested Modal Escape escaped parent");
    key(input::Key::escape);
    require(!services.modal().visible() && services.components().component_count() == count &&
                definition.telemetry().content_runs == telemetry.content_runs &&
                definition.telemetry().theme_content_runs == telemetry.theme_content_runs,
            "Gallery Modal interaction remounted content or left an open layer");
}

void test_common_component_gallery_after_document_scroll() {
    using namespace ryn;
    auto definition = rynui::example::make_token_gallery_definition();
    definition.set_viewport_width(1200);
    definition.set_motion_enabled(false);
    Fixture f;
    f.host->set_motion_preference(animation::MotionPreference::reduced);
    f.surfaces->mount(definition.content, f.inputs.get());
    auto& services = f.host->services();
    const auto sync = [&] {
        for (int frame = 0; frame < 3; ++frame) {
            require(f.host->layout_and_synchronize({1200, 900}, {0, 0, 1200, 900}, {24, 20}, 0, true),
                    "common Gallery layout failed");
        }
    };
    sync();
    const auto root = services.components().root(services.components().root_components().front());
    const auto document = f.nodes.require(f.nodes.require(root).children.front()).children[1];
    rynui::example::GalleryScrollTranslation scroll;
    const auto reveal = [&](runtime::ComponentId id) {
        const auto& node = f.nodes.require(services.components().root(id));
        scroll.apply(document, node.bounds.y - 120, f.nodes, f.dirty);
        sync();
    };
    const auto click = [&](runtime::ComponentId id) {
        const auto& node = f.nodes.require(services.components().root(id));
        const runtime::Point point{node.bounds.x + node.translation.x + node.bounds.width / 2,
                                   node.bounds.y + node.translation.y + node.bounds.height / 2};
        for (auto action : {input::PointerAction::down, input::PointerAction::up}) {
            f.host->pointer().dispatch(pointer_event(action, point, input::PointerButton::primary));
        }
        sync();
    };
    const auto page_button = [&](std::string_view label) {
        const auto button = button_with_label(f, label);
        reveal(button.component);
        click(button.component);
    };
    const auto escape = [&] {
        f.host->focus().dispatch({input::Key::escape, input::KeyAction::down});
        f.host->focus().dispatch({input::Key::escape, input::KeyAction::up});
        sync();
    };
    const auto editor = [&](std::string_view prefix) {
        for (const auto& mounted : f.inputs->mounted_inputs()) {
            if (f.inputs->editors().require(mounted.editor).value().starts_with(prefix)) {
                return mounted;
            }
        }
        throw std::runtime_error("common Gallery editor absent");
    };
    const auto count = services.components().component_count();
    const auto telemetry = definition.telemetry();
    auto& tags = services.tag_badge();
    require(tags.tags().size() >= 4 && tags.badges().size() == 5, "Gallery Tag/Badge samples absent");
    const auto closable = tags.tags()[2], checkable = tags.tags()[3];
    const auto tag_content = tags.tag_snapshot(closable).content;
    reveal(checkable);
    click(checkable);
    require(tags.tag_snapshot(checkable).checked, "Gallery CheckableTag write-back failed");
    reveal(closable);
    click(tags.tag_snapshot(closable).close);
    require(!tags.tag_snapshot(closable).visible, "Gallery Tag close write-back failed");
    page_button("gallery.tag.restore");
    require(tags.tag_snapshot(closable).visible && tags.tag_snapshot(closable).content == tag_content,
            "Gallery Tag restoration lost retained content");
    page_button("gallery.badge.increment");
    require(tags.badge_snapshot(tags.badges()[0]).display_count == String{u8"4"}, "Gallery Badge increment failed");
    page_button("gallery.badge.clear");
    require(!tags.badge_snapshot(tags.badges()[0]).shown, "Gallery Badge zero did not hide");
    for (auto index : {3, 4}) {
        const auto indicator = tags.badge_snapshot(tags.badges()[index]).indicator;
        require(f.nodes.require(services.components().root(indicator)).bounds.width > 0,
                "Gallery dot/status Badge has no actual surface width");
    }

    const auto drawer_it =
        std::ranges::find_if(services.modal().mounted(), [&](auto id) { return services.modal().snapshot(id).drawer; });
    require(drawer_it != services.modal().mounted().end(), "Gallery Drawer sample absent");
    const auto drawer = *drawer_it;
    const auto draft_editor = editor("gallery.drawer.draft");
    const auto trigger = button_with_label(f, "gallery.drawer.right");
    reveal(trigger.component);
    require(f.host->focus().request_focus(trigger.interaction, input::FocusModality::keyboard),
            "Gallery Drawer trigger focus failed");
    click(trigger.component);
    require(services.modal().snapshot(drawer).visible && near(services.modal().snapshot(drawer).bounds.x, 780),
            "Gallery right Drawer did not dock to the viewport");
    static_cast<void>(f.inputs->editors().require(draft_editor.editor).select_all());
    const String draft{u8"保留抽屉草稿 / Saved drawer draft"};
    require(bool(f.inputs->dispatch(input::TextCommitted{draft, f.inputs->sessions().active()})),
            "Gallery Drawer draft edit failed");
    sync();
    const auto offset = f.nodes.require(document).translation;
    require(f.inputs->dispatch(input::ScrollInputEvent{0, -1, 2, 2}) && f.nodes.require(document).translation == offset,
            "Gallery Drawer background wheel escaped the input boundary");
    click(button_with_label(f, "gallery.drawer.confirm").component);
    const auto child_popup = services.popconfirm().mounted()[0];
    require(services.popconfirm().snapshot(child_popup).geometry.visible, "Gallery Drawer Popconfirm did not open");
    click(services.popconfirm().snapshot(child_popup).confirm);
    require(!services.popconfirm().snapshot(child_popup).geometry.visible && services.modal().snapshot(drawer).visible,
            "Gallery Drawer confirmation closed the Drawer");
    escape();
    require(!services.modal().snapshot(drawer).visible && f.host->focus().state().focused == trigger.interaction,
            "Gallery Drawer close did not restore its trigger");
    for (const auto& [label, expected] :
         std::vector<std::pair<std::string_view, runtime::Rect>>{{"gallery.drawer.left", {0, 0, 420, 900}},
                                                                 {"gallery.drawer.top", {0, 0, 1200, 340}},
                                                                 {"gallery.drawer.bottom", {0, 560, 1200, 340}}}) {
        page_button(label);
        const auto shown = services.modal().snapshot(drawer);
        require(shown.visible && shown.bounds == expected &&
                    f.inputs->editors().require(draft_editor.editor).value() == draft.bytes(),
                "Gallery Drawer edge switch lost docking or retained draft");
        escape();
    }

    page_button("gallery.popconfirm.controlled");
    const auto popup = services.popconfirm().mounted()[1];
    require(services.popconfirm().snapshot(popup).geometry.visible, "Gallery controlled Popconfirm did not open");
    click(services.popconfirm().snapshot(popup).confirm);
    require(services.popconfirm().snapshot(popup).geometry.visible &&
                f.host->snapshot(services.popconfirm().snapshot(popup).confirm).loading,
            "Gallery Popconfirm loading did not retain the popup");
    click(button_with_label(f, "gallery.popconfirm.finish").component);
    require(!services.popconfirm().snapshot(popup).geometry.visible, "Gallery Popconfirm finish did not close");
    page_button("gallery.popconfirm.single");
    const auto single = services.popconfirm().mounted()[2];
    require(services.popconfirm().snapshot(single).geometry.visible &&
                !services.components().branch_active(services.popconfirm().snapshot(single).cancel),
            "Gallery single-action Popconfirm kept Cancel active");
    click(services.popconfirm().snapshot(single).confirm);

    auto& feedback = services.feedback();
    require(feedback.alerts().size() == 3 && feedback.spins().size() == 1 && feedback.progress().size() == 3,
            "Gallery feedback samples absent");
    const auto alert = feedback.alerts()[0];
    reveal(alert);
    click(feedback.alert_snapshot(alert).close);
    require(!feedback.alert_snapshot(alert).visible, "Gallery Alert close write-back failed");
    page_button("gallery.alert.restore");
    require(feedback.alert_snapshot(alert).visible, "Gallery Alert restore failed");
    const auto retained = editor("gallery.spin.draft");
    page_button("gallery.spin.toggle");
    const auto spin = feedback.spins()[0];
    require(feedback.spin_snapshot(spin).spinning &&
                !f.host->focus().request_focus(retained.interaction, input::FocusModality::keyboard),
            "Gallery Spin did not shield retained editing");
    page_button("gallery.spin.toggle");
    require(!feedback.spin_snapshot(spin).spinning &&
                f.inputs->editors().require(retained.editor).value().starts_with("gallery.spin.draft"),
            "Gallery Spin recovery lost its draft");
    page_button("gallery.progress.advance");
    page_button("gallery.progress.status");
    for (auto id : feedback.progress()) {
        const auto value = feedback.progress_snapshot(id);
        require(value.percent == 60 && value.status == ProgressStatus::Exception,
                "Gallery Progress controls did not update all three types");
    }
    require(services.components().component_count() == count &&
                definition.telemetry().content_runs == telemetry.content_runs &&
                definition.telemetry().theme_content_runs == telemetry.theme_content_runs,
            "common Gallery interactions remounted content");
}

void test_data_and_selection_gallery_after_document_scroll() {
    using namespace ryn;
    auto definition = rynui::example::make_token_gallery_definition();
    definition.set_viewport_width(1200);
    definition.set_motion_enabled(false);
    Fixture f;
    f.host->set_motion_preference(animation::MotionPreference::reduced);
    f.surfaces->mount(definition.content, f.inputs.get());
    auto& services = f.host->services();
    const auto sync = [&] {
        for (int frame = 0; frame < 3; ++frame) {
            require(f.host->layout_and_synchronize({1200, 900}, {0, 0, 1200, 900}, {24, 20}, 0, true),
                    "data Gallery layout failed");
        }
    };
    sync();
    const auto root = services.components().root(services.components().root_components().front());
    const auto document = f.nodes.require(f.nodes.require(root).children.front()).children[1];
    rynui::example::GalleryScrollTranslation scroll;
    const auto reveal = [&](runtime::ComponentId id) {
        scroll.apply(document, f.nodes.require(services.components().root(id)).bounds.y - 120, f.nodes, f.dirty);
        sync();
    };
    const auto click = [&](runtime::ComponentId id, input::PointerButton button = input::PointerButton::primary) {
        const auto& node = f.nodes.require(services.components().root(id));
        const runtime::Point point{node.bounds.x + node.translation.x + node.bounds.width / 2,
                                   node.bounds.y + node.translation.y + node.bounds.height / 2};
        for (auto action : {input::PointerAction::down, input::PointerAction::up}) {
            f.host->pointer().dispatch(pointer_event(action, point, button));
        }
        sync();
    };
    const auto page_button = [&](std::string_view label) {
        const auto id = button_with_label(f, label).component;
        reveal(id);
        click(id);
    };
    const auto key = [&](input::Key value) {
        f.host->focus().dispatch({value, input::KeyAction::down});
        f.host->focus().dispatch({value, input::KeyAction::up});
        sync();
    };
    const auto descendant = [&](runtime::ComponentId child, runtime::ComponentId ancestor) {
        for (auto current = std::optional{child}; current; current = services.components().parent(*current)) {
            if (*current == ancestor) {
                return true;
            }
        }
        return false;
    };
    const auto editor_in = [&](runtime::ComponentId ancestor) {
        for (const auto& editor : f.inputs->mounted_inputs()) {
            if (descendant(editor.component, ancestor)) {
                return editor;
            }
        }
        throw std::runtime_error("data Gallery editor absent");
    };
    const auto labelled_editor = [&](std::string_view label) {
        for (const auto& text : f.host->text().mounted_texts()) {
            if (f.text_scene.text_state(text.scene).content().bytes().starts_with(label)) {
                const auto parent = services.components().parent(text.component);
                require(parent.has_value(), "numeric Gallery caption parent absent");
                return editor_in(*parent);
            }
        }
        throw std::runtime_error("numeric Gallery caption absent");
    };
    auto& tables = services.table();
    auto& pagination = services.pagination();
    require(tables.mounted().size() == 1 && pagination.mounted().size() == 1 && services.menu().mounted().size() == 2 &&
                services.dropdown().mounted().size() == 1,
            "data Gallery examples absent");
    const auto table = tables.mounted().front();
    const auto row = [&](String key) {
        const auto snapshot = tables.snapshot(table);
        const auto found = std::ranges::find(snapshot.rows, key, &detail::TableRowSnapshot::key);
        require(found != snapshot.rows.end(), "data Gallery row absent");
        return *found;
    };
    const auto original_row = row(u8"record-1");
    const auto original_editor = editor_in(original_row.component);
    const auto telemetry = definition.telemetry();
    reveal(table);
    require(tables.snapshot(table).visible_keys ==
                std::vector<String>{u8"record-1", u8"record-2", u8"record-3", u8"record-4"},
            "data Gallery initial page incorrect");
    require(f.host->focus().request_focus(original_editor.interaction, input::FocusModality::keyboard),
            "table Gallery cell editor focus failed");
    key(input::Key::up);
    require(f.inputs->number_value(original_editor.component) == 4 &&
                f.inputs->editors().require(original_editor.editor).value() == "4",
            "table Gallery cell did not write its numeric value back");
    page_button("gallery.table.update");
    bool updated_text = false;
    for (const auto& text : f.host->text().mounted_texts()) {
        if (descendant(text.component, original_row.component) &&
            f.text_scene.text_state(text.scene).content().bytes() == String{u8"已更新任务"}.bytes()) {
            updated_text = true;
        }
    }
    require(updated_text && row(u8"record-1").component == original_row.component &&
                editor_in(original_row.component).editor == original_editor.editor,
            "table Gallery external update remounted the row or missed text");
    reveal(table);
    const auto sort = tables.snapshot(table).sort_buttons[1];
    click(sort);
    require(tables.snapshot(table).sort == TableSort{u8"quantity", TableSortOrder::Ascend},
            "table Gallery ascending sort write-back failed");
    click(sort);
    require(tables.snapshot(table).sort.order == TableSortOrder::Descend &&
                tables.snapshot(table).visible_keys.front() == String{u8"record-12"},
            "table Gallery descending sort did not reorder rows");
    click(sort);
    require(tables.snapshot(table).sort.order == TableSortOrder::None, "table Gallery sort reset failed");
    click(original_row.checkbox);
    require(f.selections->snapshot(original_row.checkbox).checked, "table Gallery selection write-back failed");
    const auto pager = pagination.mounted().front();
    const auto page = [&](std::size_t target) {
        reveal(pager);
        const auto snapshot = pagination.snapshot(pager);
        const auto found = std::ranges::find(snapshot.targets, target);
        require(found != snapshot.targets.end(), "Gallery pagination target absent");
        click(snapshot.items[static_cast<std::size_t>(found - snapshot.targets.begin())]);
    };
    page(2);
    require(tables.snapshot(table).current == 2 && tables.snapshot(table).visible_keys.front() == String{u8"record-5"},
            "Gallery Pagination did not update Table");
    page(1);
    require(editor_in(original_row.component).editor == original_editor.editor &&
                f.inputs->number_value(original_editor.component) == 4 &&
                f.selections->snapshot(original_row.checkbox).checked,
            "Gallery pagination lost retained edit/selection");
    page_button("gallery.table.loading");
    require(tables.snapshot(table).loading, "Gallery Table loading toggle failed");
    reveal(table);
    click(sort);
    require(tables.snapshot(table).sort.order == TableSortOrder::None, "loading Table accepted sort");
    page_button("gallery.table.loading");
    page_button("gallery.table.remove");
    require(tables.snapshot(table).rows.size() == 11, "Gallery Table removal failed");
    page_button("gallery.table.reverse");
    require(tables.snapshot(table).visible_keys.front() == String{u8"record-11"}, "Gallery source reorder failed");
    page_button("gallery.table.restore");
    require(tables.snapshot(table).rows.size() == 12 && row(u8"record-1").component == original_row.component &&
                editor_in(original_row.component).editor == original_editor.editor &&
                f.inputs->number_value(original_editor.component) == 3,
            "Gallery data restoration remounted existing keys");
    reveal(table);
    click(original_row.checkbox);
    require(f.selections->snapshot(original_row.checkbox).checked, "Gallery command selection setup failed");

    auto& menus = services.menu();
    const auto menu = menus.mounted().front();
    const auto initial_menu = menus.snapshot(menu);
    reveal(menu);
    click(initial_menu.rows[1]);
    require(menus.snapshot(menu).selected_keys == MenuKeys{u8"tasks"}, "Gallery Menu selection failed");
    page_button("gallery.menu.label");
    bool updated_label = false;
    for (const auto& text : f.host->text().mounted_texts()) {
        updated_label |= descendant(text.component, initial_menu.rows[1]) &&
                         f.text_scene.text_state(text.scene).content().bytes() == String{u8"任务 / 已更新"}.bytes();
    }
    require(updated_label && menus.snapshot(menu).rows == initial_menu.rows,
            "Gallery menu label update missed text or remounted rows");
    reveal(menu);
    click(initial_menu.rows[2]);
    require(menus.snapshot(menu).open_keys.empty(), "Gallery inline submenu close write-back failed");
    click(initial_menu.rows[2]);
    require(menus.snapshot(menu).open_keys == MenuKeys{u8"tools"}, "Gallery inline submenu reopen failed");
    page_button("gallery.menu.disabled");
    reveal(menu);
    click(initial_menu.rows.front());
    require(menus.snapshot(menu).selected_keys == MenuKeys{u8"tasks"}, "disabled Gallery Menu accepted activation");
    page_button("gallery.menu.disabled");
    reveal(menu);
    menus.focus_menu(menu);
    key(input::Key::home);
    key(input::Key::enter);
    require(menus.snapshot(menu).selected_keys == MenuKeys{u8"overview"}, "Gallery Menu keyboard navigation failed");
    const auto dropdown = services.dropdown().mounted().front();
    page_button("gallery.dropdown.actions");
    auto popup = services.dropdown().snapshot(dropdown);
    require(popup.geometry.visible && popup.geometry.bounds.y < 900, "translated Gallery Dropdown did not open");
    const auto command_rows = menus.snapshot(popup.menu).rows;
    click(command_rows[1]);
    require(!services.dropdown().snapshot(dropdown).geometry.visible &&
                !f.selections->snapshot(original_row.checkbox).checked,
            "Gallery Dropdown command did not clear selection and close");
    reveal(popup.trigger);
    click(popup.trigger, input::PointerButton::secondary);
    require(services.dropdown().snapshot(dropdown).geometry.visible, "Gallery context Dropdown did not open");
    key(input::Key::escape);
    require(!services.dropdown().snapshot(dropdown).geometry.visible, "Gallery context Dropdown Escape failed");

    const auto number = labelled_editor("gallery.number.controlled");
    const auto disabled = labelled_editor("gallery.number.disabled");
    const auto readonly = labelled_editor("gallery.number.readonly");
    page_button("gallery.number.focus");
    require(f.host->focus().state().focused == number.interaction, "Gallery InputRef focus failed");
    key(input::Key::up);
    require(f.inputs->number_value(number.component) == 1.3, "Gallery controlled decimal step failed");
    static_cast<void>(f.inputs->editors().require(number.editor).select_all());
    require(bool(f.inputs->dispatch(input::TextCommitted{String{u8"9.75"}, f.inputs->sessions().active()})),
            "Gallery decimal input failed");
    key(input::Key::enter);
    require(f.inputs->number_value(number.component) == 9.75, "Gallery decimal submission failed");
    require(!f.host->focus().request_focus(disabled.interaction, input::FocusModality::keyboard),
            "disabled Gallery number accepted focus");
    page_button("gallery.number.toggle-disabled");
    reveal(disabled.component);
    require(f.host->focus().request_focus(disabled.interaction, input::FocusModality::keyboard),
            "Gallery number did not become enabled");
    key(input::Key::up);
    require(f.inputs->number_value(disabled.component) == 6, "Gallery enabled number did not step");
    require(f.host->focus().request_focus(readonly.interaction, input::FocusModality::keyboard),
            "Gallery read-only number could not focus");
    key(input::Key::up);
    require(f.inputs->number_value(readonly.component) == 10 &&
                definition.telemetry().content_runs == telemetry.content_runs &&
                definition.telemetry().theme_content_runs == telemetry.theme_content_runs,
            "Gallery read-only number changed or content remounted");
}

void print_gallery_inventory() {
    auto definition = rynui::example::make_token_gallery_definition();
    definition.set_viewport_width(1200);
    definition.set_motion_enabled(false);
    Fixture f;
    f.surfaces->mount(definition.content, f.inputs.get());
    require(f.host->layout_and_synchronize({1200, 40000}, {0, 0, 1200, 40000}, {24, 20}), "inventory layout failed");
    const auto telemetry = definition.telemetry();
    std::cout << "stable_ids=" << definition.stable_test_ids.size()
              << " navigation=" << definition.navigation_control_count
              << " buttons=" << f.host->mounted_buttons().size() << " interactions=" << f.host->interactions().size()
              << " visible_interactions=" << f.host->scene_composer().interaction_order().size()
              << " live_samples=" << telemetry.live_samples << " theme_content=" << telemetry.theme_content_runs
              << " inputs=" << f.inputs->mounted_inputs().size()
              << " selects=" << f.host->services().select().mounted().size()
              << " modal_drawer=" << f.host->services().modal().mounted().size()
              << " selection=" << f.selections->mounted().size()
              << " menu=" << f.host->services().menu().mounted().size()
              << " dropdown=" << f.host->services().dropdown().mounted().size() << '\n';
}
} // namespace

int main(int argc, char** argv) {
    try {
        if (argc == 2 && std::string_view{argv[1]} == "--inventory-only") {
            print_gallery_inventory();
            return 0;
        }
        if (argc == 2 && std::string_view{argv[1]} == "--common-only") {
            test_common_component_gallery_after_document_scroll();
            return 0;
        }
        if (argc == 2 && std::string_view{argv[1]} == "--data-only") {
            test_data_and_selection_gallery_after_document_scroll();
            return 0;
        }
        if (argc == 2 && std::string_view{argv[1]} == "--modal-only") {
            test_modal_gallery_after_document_scroll();
            return 0;
        }
        test_acceptance_scale_viewports_use_physical_pixels();
        test_motion_acceptance_control_updates_theme_without_content_rerun();
        test_small_acceptance_viewport_survives_theme_transitions();
        test_document_viewport_scrolls_long_content_without_remount();
        test_navigation_and_filter_controls_preserve_catalog_identity();
        test_responsive_navigation_and_document_reflow_preserves_identity();
        test_token_gallery_frame_contract();
        test_live_input_samples();
        test_select_popup_after_document_scroll();
        test_card_gallery_loading_restores_editor();
        test_tabs_gallery_retains_scrolled_panel();
        test_modal_gallery_after_document_scroll();
        test_common_component_gallery_after_document_scroll();
        test_data_and_selection_gallery_after_document_scroll();
    } catch (const std::exception& error) {
        std::cerr << error.what() << '\n';
        return 1;
    }
    return 0;
}
