#include "component/text_component.hpp"
#include "renderer/sdl/glyph_gpu_resources.hpp"
#include "runtime/frame_scheduler.hpp"
#include "runtime/invalidation.hpp"

#include <ryn/rynui.hpp>

#include <cstddef>
#include <cstdint>
#include <exception>
#include <iostream>
#include <memory>
#include <span>
#include <stdexcept>
#include <utility>
#include <vector>

namespace {

void require(bool condition, const char* message) {
    if (!condition) {
        throw std::runtime_error(message);
    }
}

class RecordingGpuApi final : public ryn::detail::GlyphGpuApi {
public:
    void* create_glyph_sampler() override { return handle(next_++); }
    void* create_glyph_texture(std::uint32_t, std::uint32_t) override {
        return handle(next_++);
    }
    void* create_glyph_buffer(std::size_t) override { return handle(next_++); }

    bool upload_glyph_texture(
        void*,
        const ryn::detail::GlyphTextureUpload&) override {
        ++texture_uploads;
        return true;
    }

    bool upload_glyph_buffer(
        void*,
        std::size_t,
        std::span<const std::byte>) override {
        ++buffer_uploads;
        return true;
    }

    void release_glyph_buffer(void*) noexcept override {}
    void release_glyph_texture(void*) noexcept override {}
    void release_glyph_sampler(void*) noexcept override {}
    const char* glyph_gpu_error() const noexcept override { return ""; }

    static void* handle(std::uintptr_t value) {
        return reinterpret_cast<void*>(value);
    }

    std::uintptr_t next_{1};
    std::uint64_t texture_uploads{};
    std::uint64_t buffer_uploads{};
};

class RecordingDrawApi final : public ryn::detail::SceneDrawApi {
public:
    void draw_quad(std::uint32_t, std::uint32_t) override {
        ++quad_draws;
    }

    void draw_glyph(
        std::uint32_t,
        std::uint32_t,
        std::uint32_t count) override {
        ++glyph_draws;
        glyph_instances += count;
    }

    void draw_rounded_effect(std::uint32_t, std::uint32_t) override {
        ++effect_draws;
    }

    std::uint64_t quad_draws{};
    std::uint64_t glyph_draws{};
    std::uint64_t glyph_instances{};
    std::uint64_t effect_draws{};
};

class ControlledEvents final : public ryn::runtime::FrameEventSource {
public:
    ryn::animation::AnimationTime now() const noexcept override {
        return ryn::animation::AnimationTime::microseconds(
            static_cast<std::int64_t>(now_) * 1000);
    }

    bool poll_frame_event() noexcept override { return false; }

    bool wait_for_frame_event(std::uint32_t timeout) noexcept override {
        now_ += timeout;
        return std::exchange(wake_on_wait, false);
    }

    bool wake_on_wait{};

private:
    std::uint64_t now_{};
};

struct Fixture final {
    Fixture()
        : fonts(create_runtime()),
          engine(*fonts),
          layout(nodes),
          dirty(nodes, &requests),
          scene(*fonts, engine, requests) {
        const auto latin = fonts->load_font_file(
            RYNUI_VALIDATION_LATIN_FONT, 0, 14);
        const auto cjk = fonts->load_font_file(
            RYNUI_VALIDATION_CJK_FONT, 0, 14);
        require(latin && cjk, "Text component frame fonts failed to load");
        host = std::make_unique<ryn::detail::TextComponentHost>(
            nodes,
            layout,
            dirty,
            scene,
            std::vector<ryn::font::FontIdentity>{latin.font, cjk.font});
    }

    static std::unique_ptr<ryn::font::FontRuntime> create_runtime() {
        auto created = ryn::font::FontRuntime::create();
        require(static_cast<bool>(created), "Font Runtime initialization failed");
        return std::move(created.runtime);
    }

    std::unique_ptr<ryn::font::FontRuntime> fonts;
    ryn::text::TextEngine engine;
    ryn::runtime::FrameRequestState requests;
    ryn::runtime::NodeStore nodes;
    ryn::layout::LayoutEngine layout;
    ryn::runtime::DirtyQueues dirty;
    ryn::detail::TextSceneService scene;
    std::unique_ptr<ryn::detail::TextComponentHost> host;
};

class ComponentSubmitter final : public ryn::runtime::FrameSubmitter {
public:
    ComponentSubmitter(
        ryn::detail::TextComponentHost& host,
        ryn::detail::TextSceneService& scene,
        ryn::detail::GlyphGpuResources& resources,
        RecordingDrawApi& draw,
        ryn::runtime::Size& viewport) noexcept
        : host_(&host),
          scene_(&scene),
          resources_(&resources),
          draw_(&draw),
          viewport_(&viewport) {}

    ryn::runtime::FrameSubmissionResult submit_frame(
        ryn::animation::AnimationTime) override {
        if (!host_->layout_and_synchronize(
                *viewport_,
                {0.0F, 0.0F, viewport_->width, viewport_->height},
                {16.0F, 20.0F},
                6.0F)) {
            return ryn::runtime::FrameSubmissionResult::failed;
        }
        resources_->synchronize(
            scene_->atlas(),
            scene_->glyph_scene().instances());
        ryn::detail::draw_ordered_scene(scene_->ordered_scene(), *draw_);
        return ryn::runtime::FrameSubmissionResult::submitted;
    }

private:
    ryn::detail::TextComponentHost* host_;
    ryn::detail::TextSceneService* scene_;
    ryn::detail::GlyphGpuResources* resources_;
    RecordingDrawApi* draw_;
    ryn::runtime::Size* viewport_;
};

void require_idle(
    ryn::runtime::OnDemandFrameLoop& loop,
    std::uint64_t submissions,
    const char* message) {
    require(loop.step() == ryn::runtime::FrameLoopStep::idle
                && loop.counters().submissions == submissions,
            message);
}

void test_public_text_demo_updates_stay_minimal_and_idle() {
    Fixture fixture;
    ryn::Signal<ryn::String> content{
        ryn::String{u8"RynUI Device Monitor / 设备监控"}};
    ryn::Signal<ryn::TextTone> tone{ryn::TextTone::Secondary};
    ryn::Signal<ryn::LogicalLength> width{ryn::dp(520.0F)};
    ryn::Signal<ryn::LogicalLength> margin{ryn::dp(8.0F)};
    fixture.host->mount(ryn::Content{[&] {
        ryn::Text(
            ryn::TextProps{}
                .content(content)
                .layout(
                    ryn::LayoutStyle{}
                        .max_width(width)
                        .margin_bottom(margin)));
        ryn::Text(
            ryn::TextProps{}
                .content(u8"Secondary shared RynUI 中文")
                .tone(tone));
        ryn::Text(
            ryn::TextProps{}
                .content(u8"Disabled 设备离线")
                .tone(ryn::TextTone::Disabled));
        ryn::Text(
            ryn::TextProps{}
                .content(u8"Shared RynUI 中文")
                .tone(ryn::TextTone::Secondary));
    }});

    require(fixture.host->components().mount_runs() == 1
                && fixture.host->mounted_texts().size() == 4,
            "public Text demo did not mount four stable components once");

    RecordingGpuApi gpu;
    ryn::detail::GlyphGpuResources resources(gpu);
    RecordingDrawApi draw;
    ControlledEvents events;
    ryn::runtime::Size viewport{640.0F, 360.0F};
    ComponentSubmitter submitter(
        *fixture.host, fixture.scene, resources, draw, viewport);
    ryn::runtime::OnDemandFrameLoop loop(
        fixture.requests, events, submitter, 5);

    require(loop.step() == ryn::runtime::FrameLoopStep::submitted,
            "initial public Text frame was not submitted");
    const auto target = fixture.host->mounted_texts()[0];
    const auto tone_target = fixture.host->mounted_texts()[1];
    const auto sibling = fixture.host->mounted_texts()[2];
    require(fixture.scene.text_state(target.scene).counters().shape_count == 1
                && fixture.scene.text_state(target.scene).counters().measure_count == 1
                && fixture.scene.text_state(sibling.scene).counters().shape_count == 1
                && gpu.texture_uploads > 0
                && gpu.buffer_uploads == 1
                && draw.glyph_draws > 0
                && fixture.scene.atlas().entry_count()
                    < fixture.scene.glyph_scene().instances().size(),
            "initial Text component frame missed shape/measure/shared atlas/draw work");
    require_idle(loop, 1, "stable mounted Text components continued submitting");

    const auto content_before = fixture.scene.text_state(target.scene).counters();
    const auto content_record_before = fixture.scene.record_counters(target.scene);
    const auto sibling_before = fixture.scene.text_state(sibling.scene).counters();
    const auto atlas_before_content = gpu.texture_uploads;
    content.set(ryn::String{u8"RynUI 内容更新：温度正常界"});
    require(loop.step() == ryn::runtime::FrameLoopStep::submitted,
            "content Prop did not wake the frame loop");
    require(fixture.host->components().mount_runs() == 1
                && fixture.scene.text_state(target.scene).counters().shape_count
                    == content_before.shape_count + 1
                && fixture.scene.text_state(target.scene).counters().measure_count
                    == content_before.measure_count + 1
                && fixture.scene.record_counters(target.scene).instance_rebuilds
                    == content_record_before.instance_rebuilds + 1
                && fixture.scene.text_state(sibling.scene).counters().shape_count
                    == sibling_before.shape_count
                && gpu.texture_uploads > atlas_before_content,
            "content Prop escaped target shape/measure/atlas/instance invalidation");
    require_idle(loop, 2, "content update did not return to idle");

    const auto tone_before = fixture.scene.text_state(tone_target.scene).counters();
    const auto tone_record_before = fixture.scene.record_counters(tone_target.scene);
    const auto atlas_before_tone = gpu.texture_uploads;
    const auto buffers_before_tone = gpu.buffer_uploads;
    tone.set(ryn::TextTone::Primary);
    require(loop.step() == ryn::runtime::FrameLoopStep::submitted,
            "tone Prop did not wake the frame loop");
    require(fixture.scene.text_state(tone_target.scene).counters().shape_count
                    == tone_before.shape_count
                && fixture.scene.text_state(tone_target.scene).counters().measure_count
                    == tone_before.measure_count
                && fixture.scene.record_counters(tone_target.scene).material_updates
                    == tone_record_before.material_updates + 1
                && gpu.texture_uploads == atlas_before_tone
                && gpu.buffer_uploads == buffers_before_tone + 1,
            "tone Prop escaped Material-only upload invalidation");
    require_idle(loop, 3, "tone update did not return to idle");

    const auto shape_before_width =
        fixture.scene.text_state(target.scene).counters().shape_count;
    const auto measure_before_width =
        fixture.scene.text_state(target.scene).counters().measure_count;
    const auto atlas_before_width = gpu.texture_uploads;
    width.set(ryn::dp(120.0F));
    require(loop.step() == ryn::runtime::FrameLoopStep::submitted,
            "width Prop did not wake the frame loop");
    require(fixture.scene.text_state(target.scene).counters().shape_count
                    == shape_before_width
                && fixture.scene.text_state(target.scene).counters().measure_count
                    == measure_before_width + 1
                && gpu.texture_uploads == atlas_before_width,
            "width Prop reshaped Text or dirtied the atlas");
    require_idle(loop, 4, "width update did not return to idle");

    std::uint64_t shape_before_margin = 0;
    std::uint64_t measure_before_margin = 0;
    std::uint64_t geometry_before_margin = 0;
    for (const auto& mounted : fixture.host->mounted_texts()) {
        shape_before_margin +=
            fixture.scene.text_state(mounted.scene).counters().shape_count;
        measure_before_margin +=
            fixture.scene.text_state(mounted.scene).counters().measure_count;
        geometry_before_margin +=
            fixture.scene.record_counters(mounted.scene).geometry_updates;
    }
    margin.set(ryn::dp(24.0F));
    require(loop.step() == ryn::runtime::FrameLoopStep::submitted,
            "margin Prop did not wake the frame loop");
    std::uint64_t shape_after_margin = 0;
    std::uint64_t measure_after_margin = 0;
    std::uint64_t geometry_after_margin = 0;
    for (const auto& mounted : fixture.host->mounted_texts()) {
        shape_after_margin +=
            fixture.scene.text_state(mounted.scene).counters().shape_count;
        measure_after_margin +=
            fixture.scene.text_state(mounted.scene).counters().measure_count;
        geometry_after_margin +=
            fixture.scene.record_counters(mounted.scene).geometry_updates;
    }
    require(shape_after_margin == shape_before_margin
                && measure_after_margin == measure_before_margin
                && geometry_after_margin > geometry_before_margin,
            "margin Prop reshaped/remeasured Text or missed placement geometry");
    require_idle(loop, 5, "margin update did not return to idle");

    const auto shapes_before_resize = shape_after_margin;
    const auto draws_before_resize = draw.glyph_draws;
    viewport = {480.0F, 320.0F};
    events.wake_on_wait = true;
    require(loop.step() == ryn::runtime::FrameLoopStep::submitted
                && loop.counters().event_wakes == 1,
            "resize event did not wake and submit through the controlled clock");
    std::uint64_t shapes_after_resize = 0;
    for (const auto& mounted : fixture.host->mounted_texts()) {
        shapes_after_resize +=
            fixture.scene.text_state(mounted.scene).counters().shape_count;
    }
    require(shapes_after_resize == shapes_before_resize
                && draw.glyph_draws > draws_before_resize,
            "resize event reshaped content or failed to draw the retained scene");

    const auto stable_submissions = loop.counters().submissions;
    content.set(ryn::String{u8"RynUI 内容更新：温度正常界"});
    tone.set(ryn::TextTone::Primary);
    width.set(ryn::dp(120.0F));
    margin.set(ryn::dp(24.0F));
    require_idle(loop, stable_submissions,
            "equal Prop writes requested another frame");
    require(events.now_milliseconds() >= 30
                && loop.counters().idle_waits >= 6,
            "controlled clock did not record idle waits");
}

} // namespace

int main() {
    try {
        test_public_text_demo_updates_stay_minimal_and_idle();
    } catch (const std::exception& error) {
        std::cerr << error.what() << '\n';
        return 1;
    }
    return 0;
}
