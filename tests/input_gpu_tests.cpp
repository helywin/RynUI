#include "component/input_component.hpp"
#include "renderer/sdl/glyph_gpu_resources.hpp"
#include "renderer/sdl/rounded_effect_gpu_resources.hpp"

#include <ryn/rynui.hpp>
#include <algorithm>
#include <cmath>
#include <iostream>
#include <memory>
#include <stdexcept>
#include <vector>

namespace {
using namespace ryn;
void require(bool condition, const char* message) { if(!condition) throw std::runtime_error(message); }
struct Platform final : input::TextInputPlatform, input::TextClipboard {
    bool start(input::TextInputSessionStamp, const input::TextInputProperties&) noexcept override { return true; }
    bool stop() noexcept override { return true; }
    bool cancel() noexcept override { return true; }
    bool set_area(const input::WindowTextInputArea&) noexcept override { return true; }
    input::ClipboardReadResult read_text() override { return {input::ClipboardError::no_text, {}}; }
    input::ClipboardError write_text(StringView) override { return input::ClipboardError::none; }
    input::ClipboardAvailability has_text() const noexcept override { return {}; }
};
struct Gpu final : detail::GlyphGpuApi, detail::RoundedEffectGpuApi, graphics::QuadUploadApi, detail::SceneDrawApi {
    struct Upload { std::size_t offset, bytes; };
    std::vector<Upload> glyph_uploads, quad_uploads, effect_uploads;
    std::vector<graphics::SceneDrawCommand> draws;
    std::size_t textures{};
    bool fail_glyph{};
    void* handle() { return reinterpret_cast<void*>(++identity); }
    std::uintptr_t identity{};
    void* create_glyph_sampler() override { return handle(); }
    void* create_glyph_texture(std::uint32_t, std::uint32_t) override { return handle(); }
    void* create_glyph_buffer(std::size_t) override { return handle(); }
    bool upload_glyph_texture(void*, const detail::GlyphTextureUpload&) override { ++textures; return true; }
    bool upload_glyph_buffer(void*, std::size_t offset, std::span<const std::byte> bytes) override {
        glyph_uploads.push_back({offset, bytes.size()}); return !fail_glyph;
    }
    void release_glyph_buffer(void*) noexcept override {}
    void release_glyph_texture(void*) noexcept override {}
    void release_glyph_sampler(void*) noexcept override {}
    const char* glyph_gpu_error() const noexcept override { return "injected glyph upload failure"; }
    void* create_effect_buffer(std::size_t) override { return handle(); }
    bool upload_effect_buffer(void*, std::size_t offset, std::span<const std::byte> bytes) override {
        effect_uploads.push_back({offset, bytes.size()}); return true;
    }
    void release_effect_buffer(void*) noexcept override {}
    const char* effect_gpu_error() const noexcept override { return ""; }
    void* create_vertex_buffer(std::size_t) override { return handle(); }
    void release_buffer(void*) noexcept override {}
    bool upload(void*, std::size_t offset, std::span<const std::byte> bytes) override {
        quad_uploads.push_back({offset, bytes.size()}); return true;
    }
    const char* last_error() const noexcept override { return ""; }
    void draw_quad(std::uint32_t first, std::uint32_t count) override { draws.push_back({graphics::SceneDrawKind::quad, first, count}); }
    void draw_glyph(std::uint32_t page, std::uint32_t first, std::uint32_t count) override { draws.push_back({graphics::SceneDrawKind::glyph, first, count, page}); }
    void draw_rounded_effect(std::uint32_t first, std::uint32_t count) override { draws.push_back({graphics::SceneDrawKind::rounded_effect, first, count}); }
    void clear() { glyph_uploads.clear(); quad_uploads.clear(); effect_uploads.clear(); draws.clear(); }
};
struct Events final : runtime::FrameEventSource {
    animation::AnimationTime now() const noexcept override { return animation::AnimationTime::microseconds(0); }
    bool poll_frame_event() noexcept override { return false; }
    bool wait_for_frame_event(std::uint32_t) noexcept override { return false; }
};
struct Fixture final : runtime::FrameSubmitter {
    runtime::NodeStore nodes;
    runtime::FrameRequestState requests;
    runtime::DirtyQueues dirty{nodes, &requests};
    layout::LayoutEngine layout{nodes};
    std::unique_ptr<font::FontRuntime> fonts{std::move(font::FontRuntime::create().runtime)};
    text::TextEngine engine{*fonts};
    detail::TextSceneService scene{*fonts, engine, requests};
    float scale;
    std::vector<font::FontIdentity> chain;
    detail::ButtonComponentHost host{nodes, layout, dirty, scene,
        [this](SystemFontFamily, std::uint32_t, std::uint32_t) { return chain; }, requests};
    Platform platform;
    detail::InputComponentHost inputs{host, platform, platform};
    Gpu api;
    std::unique_ptr<graphics::QuadGpuBuffer> quads;
    detail::GlyphGpuResources glyphs{api};
    detail::RoundedEffectGpuResources effects{api};
    bool defer{};
    static std::vector<font::FontIdentity> load_chain(font::FontRuntime& fonts, float scale) {
        std::vector<font::FontIdentity> result;
        for(const auto path : {RYNUI_VALIDATION_LATIN_FONT, RYNUI_VALIDATION_CJK_FONT}) {
            auto font = fonts.load_font_file(path, 0, font::FontRasterConfig{14, scale});
            require(bool(font), "GPU fixture font load failed"); result.push_back(font.font);
        }
        return result;
    }
    explicit Fixture(float scale_value) : scale(scale_value), chain(load_chain(*fonts, scale)) {
        inputs.set_display_scale(scale);
    }
    runtime::FrameSubmissionResult submit_frame(animation::AnimationTime) override {
        require(host.layout_and_synchronize({320, 240}, {0, 0, 320, 240}, {10, 10}), "GPU fixture layout failed");
        if(!quads) quads = std::make_unique<graphics::QuadGpuBuffer>(api, host.button_scene().instances());
        quads->synchronize(host.button_scene().instances());
        glyphs.synchronize(scene.atlas(), scene.glyph_scene().instances());
        effects.synchronize(host.rounded_effects(), {
            static_cast<std::uint32_t>(320 * scale), static_cast<std::uint32_t>(240 * scale), scale});
        if(defer) return runtime::FrameSubmissionResult::deferred;
        detail::draw_ordered_scene(host.scene_composer().ordered_scene(), api);
        return runtime::FrameSubmissionResult::submitted;
    }
};

void run(float scale) {
    Fixture f{scale};
    f.inputs.mount(Content{[&] {
        Input(InputProps{}.defaultValue(u8"Input selection 中文").layout(LayoutStyle{}.width(dp(180))));
        Input(InputProps{}.defaultValue(u8"unrelated"));
    }});
    Events events;
    runtime::OnDemandFrameLoop loop{f.requests, events, f};
    require(loop.step() == runtime::FrameLoopStep::submitted, "initial Input GPU frame failed");
    require(f.effects.instance_count() == 2 * detail::input_effect_layer_count && !f.api.glyph_uploads.empty()
        && !f.api.quad_uploads.empty() && !f.api.effect_uploads.empty(), "Input layer GPU buffers incomplete");
    require(std::ranges::equal(f.api.draws, f.host.scene_composer().ordered_scene().commands()), "GPU draw order differs from retained scene");
    const auto shadow = f.effects.instances()[detail::input_shadow_layer_capacity - 1];
    const auto focus = f.effects.instances()[detail::input_focus_layer];
    const auto& shape = f.host.rounded_effects().packed_instances()[detail::input_shadow_layer_capacity - 1].geometry.shape.rect;
    require(graphics::rounded_effect_gpu_coverage_reference(
        {(shape.x + shape.width + 1) * scale, (shape.y + shape.height / 2) * scale}, shadow) > 0.9F,
        "scaled active shadow footprint was clipped");
    const float outline_x = shape.x + shape.width + focus.effect_params[2] / scale
        + focus.effect_params[1] / scale / 2;
    require(graphics::rounded_effect_gpu_coverage_reference(
        {outline_x * scale, (shape.y + shape.height / 2) * scale}, focus) > 0.9F,
        "scaled focus footprint was clipped");
    const auto target = f.inputs.mounted_inputs().front();
    require(f.host.focus().request_focus(target.interaction, input::FocusModality::keyboard), "GPU Input focus failed");
    require(bool(f.inputs.editors().require(target.editor).select({0, 2})), "GPU selection failed");
    f.requests.request_frame();
    require(loop.step() == runtime::FrameLoopStep::submitted, "selection GPU frame failed");
    f.api.clear();
    const auto textures = f.api.textures;
    const auto selected = f.inputs.text_layers(target.component).selected;
    const auto selected_range = f.scene.primitive(selected).instances;
    require(bool(f.inputs.editors().require(target.editor).select({0, 3})), "selection extension failed");
    f.requests.request_frame();
    f.defer = true;
    require(loop.step() == runtime::FrameLoopStep::deferred && f.requests.pending(), "deferred frame lost request");
    require(f.api.effect_uploads.empty() && f.api.textures == textures && !f.api.quad_uploads.empty()
        && !f.api.glyph_uploads.empty() && f.api.draws.empty(), "selection update touched unrelated effect/atlas or drew deferred frame");
    for(const auto upload : f.api.glyph_uploads) require(upload.offset >= selected_range.first * sizeof(graphics::GlyphInstance)
        && upload.offset + upload.bytes <= (selected_range.first + selected_range.count) * sizeof(graphics::GlyphInstance),
        "selection upload escaped selected glyph range");
    f.api.clear(); f.defer = false;
    require(loop.step() == runtime::FrameLoopStep::submitted && !f.api.draws.empty()
        && f.api.glyph_uploads.empty() && f.api.quad_uploads.empty() && f.api.effect_uploads.empty(),
        "deferred retry repeated uploads or lost draws");
    require(bool(f.inputs.editors().require(target.editor).select({0, 4})), "failure-path selection failed");
    f.requests.request_frame(); f.api.fail_glyph = true;
    bool failed{};
    try { static_cast<void>(f.submit_frame(animation::AnimationTime::microseconds(0))); }
    catch(const std::runtime_error&) { failed = true; }
    require(failed && !f.scene.glyph_scene().instances().geometry_dirty_ranges().empty(), "failed upload discarded dirty range");
    f.api.fail_glyph = false;
    require(f.submit_frame(animation::AnimationTime::microseconds(0)) == runtime::FrameSubmissionResult::submitted,
        "failed upload did not recover");
}
}
int main() {
    try { for(const auto scale : {1.0F, 1.25F, 1.5F, 2.0F}) run(scale); }
    catch(const std::exception& error) { std::cerr << error.what() << '\n'; return 1; }
}
