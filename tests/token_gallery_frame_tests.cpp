#include "component/button_component.hpp"
#include "renderer/sdl/glyph_gpu_resources.hpp"
#include "renderer/sdl/rounded_effect_gpu_resources.hpp"
#include "runtime/frame_scheduler.hpp"
#include "runtime/invalidation.hpp"
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
};

class IdleEvents final : public ryn::runtime::FrameEventSource {
public:
    std::uint64_t now_milliseconds() const noexcept override { return now_; }
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

    ryn::runtime::FrameSubmissionResult submit_frame() override {
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
    ryn::runtime::Size viewport_{1200.0F, 900.0F};
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
}

void require_shadow_order(
    const Fixture& fixture,
    std::size_t button_index,
    const ryn::ShadowList& expected) {
    const auto mounted = fixture.host->mounted_buttons()[button_index];
    const auto ids = fixture.host->button_scene().shadow_effects(mounted.scene);
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

ryn::input::PointerInputEvent pointer_event(
    ryn::input::PointerAction action,
    ryn::runtime::Point point,
    ryn::input::PointerButton button = ryn::input::PointerButton::none) {
    return {
        ryn::input::PointerIdentity::mouse(), action, button, point.x, point.y,
    };
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
    fixture.host->mount(definition.content);
    require(fixture.host->mounted_buttons().size() == definition.stable_test_ids.size(),
            "Token Gallery did not mount one retained cell per stable test id");

    RecordingGpuApi gpu;
    RecordingDrawApi draw;
    IdleEvents events;
    HeadlessSubmitter submitter(
        *fixture.host, fixture.text_scene, fixture.frames, gpu, draw);
    ryn::runtime::OnDemandFrameLoop loop(fixture.frames, events, submitter, 5);
    require(loop.step() == ryn::runtime::FrameLoopStep::submitted,
            "Token Gallery initial wide frame was not submitted");
    require_all_cells_reachable(fixture, {1200.0F, 900.0F});

    const auto initial = definition.telemetry();
    require(initial.content_runs == 1
                && initial.theme_content_runs == definition.stable_test_ids.size() + 1,
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

    const auto focus_target = fixture.host->mounted_buttons()[9];
    const auto hidden_focus = fixture.host->button_scene().focus_effect(focus_target.scene);
    require(hidden_focus.geometry.kind == ryn::graphics::RoundedEffectKind::outline
                && near(hidden_focus.geometry.outline_offset, 1.0F)
                && near(hidden_focus.geometry.outline_width, 3.0F)
                && near(hidden_focus.material.opacity, 0.0F),
            "Token Gallery focus cell does not use a 1px gap and 3px hollow ring");

    definition.set_viewport_width(560.0F);
    submitter.set_viewport({560.0F, 1100.0F});
    require(loop.step() == ryn::runtime::FrameLoopStep::submitted,
            "Token Gallery narrow frame was not submitted");
    require_all_cells_reachable(fixture, {560.0F, 1100.0F});

    definition.set_viewport_width(1200.0F);
    submitter.set_viewport({1200.0F, 900.0F});
    require(loop.step() == ryn::runtime::FrameLoopStep::submitted,
            "Token Gallery wide restoration was not submitted");
    const auto component_count = fixture.host->components().component_count();
    const auto scene_rebuilds = fixture.host->scene_composer().diagnostics().rebuilds;
    const auto effect_uploads = gpu.effect_uploads;
    const auto quad_uploads = gpu.quad_uploads;

    definition.smoke_step(0);
    require(loop.step() == ryn::runtime::FrameLoopStep::submitted,
            "Dark Theme did not submit a Token Gallery frame");
    require(gpu.effect_uploads > effect_uploads && gpu.quad_uploads > quad_uploads,
            "Theme update did not produce local retained uploads");
    definition.smoke_step(1);
    require(loop.step() == ryn::runtime::FrameLoopStep::submitted,
            "Compact Theme did not submit a Token Gallery frame");
    definition.smoke_step(2);
    require(loop.step() == ryn::runtime::FrameLoopStep::submitted,
            "Brand Seed did not submit a Token Gallery frame");
    definition.smoke_step(3);
    require(loop.step() == ryn::runtime::FrameLoopStep::submitted,
            "disabled/loading update did not submit a Token Gallery frame");
    definition.smoke_step(4);
    require(loop.step() == ryn::runtime::FrameLoopStep::submitted,
            "Default Theme restoration did not submit a Token Gallery frame");

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

    const auto hover = fixture.host->mounted_buttons()[7];
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
        test_token_gallery_frame_contract();
    } catch (const std::exception& error) {
        std::cerr << error.what() << '\n';
        return 1;
    }
    return 0;
}
