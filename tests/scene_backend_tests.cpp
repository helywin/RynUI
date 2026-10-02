#include "renderer/common/scene_resources.hpp"
#include "renderer/recording/recording_renderer.hpp"
#include "runtime/callback_frame_pump.hpp"
#include "support/input_fixture.hpp"
#include "component/slider_component.hpp"

#include <algorithm>
#include <array>
#include <iostream>
#include <limits>

namespace {
using namespace ryn;
using namespace ryn::detail;

void check(bool value, const char* message) {
    if (!value) {
        throw std::runtime_error(message);
    }
}

template <class F> void rejects(F&& action) {
    bool rejected = false;
    try {
        action();
    } catch (const std::exception&) {
        rejected = true;
    }
    check(rejected, "invalid handle or upload did not reject");
}

bool same(std::span<const std::byte> actual, std::span<const std::byte> expected) {
    return std::ranges::equal(actual, expected);
}

void owned_bytes_and_ranges() {
    RecordingRenderer backend;
    RecordingRenderer other;
    auto* buffer = backend.create_vertex_buffer(8);
    auto* texture = backend.create_glyph_texture(3, 2);
    std::array bytes{std::byte{1}, std::byte{2}, std::byte{3}, std::byte{4},
                     std::byte{5}, std::byte{6}, std::byte{7}, std::byte{8}};
    const auto original = bytes;
    check(backend.begin_upload_batch(), "batch begin failed");
    check(backend.upload(buffer, 0, bytes), "buffer copy failed");
    check(backend.upload_glyph_texture(texture, {0, {0, 0, 3, 2}, 0, 4, bytes}), "texture copy failed");
    bytes.fill(std::byte{99});
    check(backend.finish_upload_batch(), "batch commit failed");
    check(same(backend.buffer_bytes(buffer), original), "upload retained caller memory");
    const std::array texture_expected{std::byte{1}, std::byte{2}, std::byte{3},
                                      std::byte{5}, std::byte{6}, std::byte{7}};
    check(same(backend.texture_bytes(texture), texture_expected),
          "texture rows ignored stride or copied caller memory");
    check(backend.begin_upload_batch(), "range batch begin failed");
    check(!backend.upload(buffer, std::numeric_limits<std::size_t>::max(), bytes), "overflow range accepted");
    check(!backend.upload_glyph_buffer(buffer, 0, bytes), "wrong-kind handle accepted");
    check(!backend.upload_glyph_texture(texture, {0, {2, 1, 3, 2}, 0, 4, bytes}), "texture range accepted");
    check(other.begin_upload_batch(), "foreign batch begin failed");
    check(!other.upload(buffer, 0, bytes), "foreign handle accepted");
    other.cancel_upload_batch();
    backend.cancel_upload_batch();
    backend.reset_device();
    rejects([&] { static_cast<void>(backend.buffer_bytes(buffer)); });
    check(backend.begin_upload_batch(), "reset batch begin failed");
    check(!backend.upload(buffer, 0, bytes), "old epoch upload accepted");
    backend.cancel_upload_batch();
}

void texture_source_lifetime_cancel_and_invalid_ranges() {
    RecordingRenderer backend;
    auto* texture = backend.create_glyph_texture(5, 3);
    std::array<std::byte, 15> initial;
    initial.fill(std::byte{40});
    check(backend.begin_upload_batch(), "texture initial begin failed");
    check(backend.upload_glyph_texture(texture, {0, {0, 0, 5, 3}, 0, 5, initial}), "initial texture upload failed");
    check(backend.finish_upload_batch(), "initial texture commit failed");
    std::array source{std::byte{99}, std::byte{98}, std::byte{1}, std::byte{2}, std::byte{3},
                      std::byte{97}, std::byte{96}, std::byte{4}, std::byte{5}, std::byte{6}};
    const GlyphTextureUpload valid{0, {1, 1, 3, 2}, 2, 5, source};
    check(backend.begin_upload_batch(), "texture range begin failed");
    backend.fail_next(RecordingFailure::upload);
    for (int mode = 0; mode < 7; ++mode) {
        auto invalid = valid;
        switch (mode) {
        case 0:
            invalid.source_offset = std::numeric_limits<std::size_t>::max();
            break;
        case 1:
            invalid.source_row_pitch = 2;
            break;
        case 2:
            invalid.bytes = std::span(source).first(9);
            break;
        case 3:
            invalid.rectangle.width = 0;
            break;
        case 4:
            invalid.rectangle.x = std::numeric_limits<std::uint32_t>::max();
            break;
        case 5:
            invalid.rectangle.y = 2;
            break;
        case 6:
            invalid.source_row_pitch = std::numeric_limits<std::uint32_t>::max();
            break;
        }
        check(!backend.upload_glyph_texture(texture, invalid), "invalid texture source accepted");
    }
    check(!backend.upload_glyph_texture(texture, valid), "invalid source consumed failure injection");
    check(same(backend.texture_bytes(texture), initial), "rejected uploads changed pixels");
    check(backend.upload_glyph_texture(texture, valid), "valid nonzero source failed");
    source.fill(std::byte{70});
    check(backend.finish_upload_batch(), "owned texture commit failed");
    auto expected = initial;
    expected[6] = std::byte{1};
    expected[7] = std::byte{2};
    expected[8] = std::byte{3};
    expected[11] = std::byte{4};
    expected[12] = std::byte{5};
    expected[13] = std::byte{6};
    check(same(backend.texture_bytes(texture), expected), "source lifetime/stride or untouched pixels changed");
    check(backend.begin_upload_batch(), "cancel fixture begin failed");
    check(backend.upload_glyph_texture(texture, {0, {0, 0, 1, 1}, 0, 1, source}), "cancel fixture upload failed");
    backend.cancel_upload_batch();
    check(same(backend.texture_bytes(texture), expected), "cancel committed texture pixels");
    check(backend.begin_upload_batch(), "commit failure begin failed");
    check(backend.upload_glyph_texture(texture, {0, {0, 0, 1, 1}, 0, 1, source}), "commit failure upload failed");
    backend.fail_next(RecordingFailure::commit);
    check(!backend.finish_upload_batch(), "injected commit failure succeeded");
    check(same(backend.texture_bytes(texture), expected), "failed commit changed texture pixels");
    backend.cancel_upload_batch();
}

void required_capabilities_and_scene_limits() {
    for (int mode = 0; mode < 11; ++mode) {
        auto caps = baseline_scene_capabilities();
        switch (mode) {
        case 0:
            ++caps.logical_scene_version;
            break;
        case 1:
            ++caps.packed_abi_version;
            break;
        case 2:
            caps.quads = false;
            break;
        case 3:
            caps.glyphs = false;
            break;
        case 4:
            caps.rounded_effects = false;
            break;
        case 5:
            caps.r8_sampling = false;
            break;
        case 6:
            caps.ordered_draws = false;
            break;
        case 7:
            caps.partial_uploads = false;
            break;
        case 8:
            caps.maximum_buffer_bytes = 0;
            break;
        case 9:
            caps.maximum_texture_width = 0;
            break;
        case 10:
            caps.maximum_texture_height = 0;
            break;
        }
        RecordingRenderer backend(caps);
        backend.fail_next(RecordingFailure::create);
        bool rejected = false;
        try {
            SceneResources resources(backend);
        } catch (const std::invalid_argument& error) {
            rejected = std::string_view(error.what()).find("Renderer") != std::string_view::npos;
        }
        check(rejected && backend.live_resources() == 0, "unsupported capabilities created resources");
        check(backend.create_glyph_sampler() == nullptr, "capability validation consumed create injection");
    }
    for (int mode = 0; mode < 4; ++mode) {
        auto caps = baseline_scene_capabilities();
        caps.maximum_buffer_bytes = 128;
        caps.maximum_texture_width = caps.maximum_texture_height = 4;
        RecordingRenderer backend(caps);
        SceneResources resources(backend);
        graphics::QuadInstanceStore quads;
        graphics::QuadInstanceStore empty_quads;
        graphics::GlyphInstanceStore glyphs;
        graphics::GlyphInstanceStore empty_glyphs;
        graphics::RoundedEffectStore effects;
        graphics::RoundedEffectStore empty_effects;
        graphics::GlyphAtlas atlas({10, 10, 1});
        graphics::GlyphAtlas empty_atlas({4, 4, 1});
        const SceneDeviceMetrics metrics{100, 100, 1};
        graphics::OrderedScene order;
        check(resources.synchronize({&quads, atlas, glyphs, &effects, metrics}),
              "empty atlas config was over-rejected");
        const auto old = resources.attach(order);
        check(backend.valid_attachment(old), "initial empty scene attachment failed");
        if (mode == 0) {
            const std::array<graphics::QuadInstance, 3> values{};
            static_cast<void>(quads.append(values));
        } else if (mode == 1) {
            const std::array<graphics::GlyphInstance, 2> values{};
            static_cast<void>(glyphs.append(values));
        } else if (mode == 2) {
            const auto effect = graphics::make_shadow_effect({{1, 1, 10, 10}, 2},
                                                             {ShadowKind::outer, {}, 2, 0, Color::rgba8(0, 0, 0, 80)});
            static_cast<void>(effects.add(effect));
            static_cast<void>(effects.add(effect));
        } else {
            font::GlyphBitmap bitmap;
            bitmap.width = bitmap.height = bitmap.row_stride = 1;
            bitmap.coverage = {255};
            check(bool(atlas.insert({{0, 1}, 1, 14}, bitmap)), "atlas limit fixture failed");
        }
        const auto uploads = backend.counters().uploads;
        backend.fail_next(RecordingFailure::begin);
        rejects([&] { resources.synchronize({&quads, atlas, glyphs, &effects, metrics}); });
        check(backend.counters().uploads == uploads && !backend.valid_attachment(old),
              "limit preflight uploaded or retained ready attachment");
        check((mode != 0 || !quads.geometry_dirty_ranges().empty()) &&
                  (mode != 1 || !glyphs.geometry_dirty_ranges().empty()) &&
                  (mode != 3 || !atlas.dirty_regions().empty()),
              "preflight lost CPU dirty data");
        check(!resources.synchronize({&empty_quads, empty_atlas, empty_glyphs, &empty_effects, metrics}),
              "over-limit scene consumed begin failure");
        check(resources.synchronize({&empty_quads, empty_atlas, empty_glyphs, &empty_effects, metrics}),
              "corrected scene could not retry");
        check(backend.valid_attachment(resources.attach(order)), "retry attachment failed");
    }
    auto caps = baseline_scene_capabilities();
    for (const auto stride : {sizeof(QuadGpuInstance), sizeof(GlyphGpuInstance), sizeof(RoundedEffectGpuInstance)}) {
        caps.maximum_buffer_bytes = 4 * stride;
        validate_scene_buffer_requirement(caps, 3, stride, true);
        validate_scene_buffer_requirement(caps, 4, stride, false);
        rejects([&] { validate_scene_buffer_requirement(caps, 5, stride, true); });
        rejects(
            [&] { validate_scene_buffer_requirement(caps, std::numeric_limits<std::size_t>::max(), stride, true); });
    }
    // Quad growth doubles an existing arbitrary capacity, not a power of two.
    caps.maximum_buffer_bytes = 8 * sizeof(QuadGpuInstance);
    RecordingRenderer backend(caps);
    SceneResources resources(backend);
    graphics::QuadInstanceStore quads;
    const std::array<graphics::QuadInstance, 6> initial{};
    static_cast<void>(quads.append(initial));
    graphics::GlyphInstanceStore glyphs;
    graphics::GlyphAtlas atlas;
    graphics::RoundedEffectStore effects;
    const SceneDeviceMetrics metrics{100, 100, 1};
    check(resources.synchronize({&quads, atlas, glyphs, &effects, metrics}), "exact initial Quad budget rejected");
    check(resources.quads()->capacity() == 6, "preflight changed initial Quad allocation policy");
    const std::array<graphics::QuadInstance, 1> more{};
    static_cast<void>(quads.append(more));
    backend.fail_next(RecordingFailure::begin);
    rejects([&] { resources.synchronize({&quads, atlas, glyphs, &effects, metrics}); });
    check(!backend.begin_upload_batch(), "Quad growth preflight underestimated doubled capacity");
}

struct Fixture final {
    Signal<String> content{String{u8"共同场景 Hello"}};
    Signal<double> slider_value{25};
    ryn_test::input_component::Fixture ui;
    RecordingRenderer backend;
    SceneResources resources{backend};
    int mounts{};
    SceneDeviceMetrics metrics{320, 240, 1};

    Fixture() {
        ui.services.mount(Content{[this] {
            ++mounts;
            Slider(SliderProps{}.value(slider_value));
            Input(InputProps{}.defaultValue(u8"编辑状态 kept").layout(LayoutStyle{}.width(dp(180))));
            Button(ButtonProps{}, ButtonContent{[] { Text(TypographyProps{}.content(u8"按钮 Button")); }});
            Text(TypographyProps{}.content(content).underline(true));
            Divider(DividerProps{}.content(u8"分隔 Label"));
        }});
        const auto target = ui.inputs.mounted_inputs().front();
        check(ui.services.focus().request_focus(target.interaction, input::FocusModality::keyboard), "focus failed");
        check(bool(ui.inputs.editors().require(target.editor).select({0, 2})), "editor select failed");
        layout();
    }

    void layout() {
        ui.synchronize(320);
    }

    SceneCpuData data() {
        return {&ui.services.surfaces().instances(), ui.scene.atlas(), ui.scene.glyph_scene().instances(),
                &ui.services.rounded_effects(), metrics};
    }

    void mark_all() {
        data().quads->mark_all_dirty();
        data().glyphs.mark_all_dirty();
        data().atlas.mark_all_pages_dirty();
        resources.effects().invalidate_upload();
    }

    SceneAttachment attachment() {
        return resources.attach(ui.services.scene_composer().ordered_scene());
    }

    void verify() {
        const auto cpu = data();
        std::vector<QuadGpuInstance> packed_quads;
        for (const auto& instance : cpu.quads->instances()) {
            packed_quads.push_back(pack_quad_instance(instance, cpu.metrics));
        }
        std::vector<GlyphGpuInstance> packed_glyphs;
        for (const auto& instance : cpu.glyphs.instances()) {
            packed_glyphs.push_back(pack_glyph_instance(instance, cpu.metrics));
        }
        check(resources.quads(), "component scene omitted Quad resources");
        check(
            same(backend.buffer_bytes(resources.quads()->handle()).first(packed_quads.size() * sizeof(QuadGpuInstance)),
                 std::as_bytes(std::span(packed_quads))),
            "Quad bytes differ from component scene");
        check(same(backend.buffer_bytes(resources.glyphs().instance_buffer())
                       .first(packed_glyphs.size() * sizeof(GlyphGpuInstance)),
                   std::as_bytes(std::span(packed_glyphs))),
              "Glyph bytes differ from component scene");
        check(resources.effects().instance_count() > 0, "focused component fixture omitted effects");
        check(
            same(backend.buffer_bytes(resources.effects().buffer()).first(resources.effects().instances().size_bytes()),
                 std::as_bytes(resources.effects().instances())),
            "Effect bytes differ from packed scene");
        for (std::uint32_t page = 0; page < cpu.atlas.page_count(); ++page) {
            check(same(backend.texture_bytes(resources.glyphs().texture(page)),
                       std::as_bytes(cpu.atlas.page_bytes(page))),
                  "Atlas pixels differ from CPU page");
        }
        check(backend.attach_scene(attachment()), "shared scene attachment rejected");
        check(backend.submit_frame(animation::AnimationTime::microseconds(0)) ==
                  runtime::FrameSubmissionResult::submitted,
              "ordered frame submission failed");
        const auto commands = ui.services.scene_composer().ordered_scene().commands();
        check(backend.draws().size() == commands.size(), "ordered draw count differs");
        bool quad = false;
        bool glyph = false;
        bool effect = false;
        for (std::size_t index = 0; index < commands.size(); ++index) {
            check(backend.draws()[index].command == commands[index], "draw order changed");
            check(!backend.draws()[index].instance_bytes.empty(), "draw did not consume real bytes");
            quad |= commands[index].kind == graphics::SceneDrawKind::quad;
            glyph |= commands[index].kind == graphics::SceneDrawKind::glyph;
            effect |= commands[index].kind == graphics::SceneDrawKind::rounded_effect;
        }
        check(quad && glyph && effect, "fixture did not cover all draw kinds");
    }
};

class SurfaceEvents final : public runtime::FrameEventSource {
public:
    animation::AnimationTime now() const noexcept override {
        return {};
    }

    bool poll_frame_event() noexcept override {
        return false;
    }

    bool wait_for_frame_event(std::uint32_t) noexcept override {
        ++waits;
        return false;
    }

    int waits{};
};

void real_scene_transaction_and_epoch() {
    Fixture fixture;
    check(fixture.resources.synchronize(fixture.data()), "initial transaction failed");
    fixture.verify();
    auto uploads = fixture.backend.counters().uploads;
    check(fixture.resources.synchronize(fixture.data()), "idle transaction failed");
    check(fixture.backend.counters().uploads == uploads, "idle reuploaded resources");
    // Material changes use the existing reactive component path and a bounded range.
    auto& quads = *fixture.data().quads;
    const std::array material{graphics::QuadMaterial{{1, 0, 0, 1}, 0.75F}};
    check(quads.update_material({0, 1}, material) == 1, "material update failed");
    const auto bytes_before = fixture.backend.counters().uploaded_bytes;
    check(fixture.resources.synchronize(fixture.data()), "partial transaction failed");
    check(fixture.backend.counters().uploaded_bytes - bytes_before == sizeof(detail::QuadGpuInstance),
          "local material update uploaded unrelated resources");
    fixture.verify();
    for (const auto failure : {RecordingFailure::begin, RecordingFailure::upload, RecordingFailure::commit,
                               RecordingFailure::upload_exception}) {
        fixture.content.set(String{u8"改变后的共同组件 Update"});
        fixture.layout();
        fixture.mark_all();
        const auto previous = fixture.attachment();
        fixture.backend.fail_next(failure, failure == RecordingFailure::upload ? 1 : 0);
        bool success = false;
        try {
            success = fixture.resources.synchronize(fixture.data());
        } catch (const std::runtime_error&) {
        }
        check(!success, "injected failure did not fail");
        check(!fixture.backend.valid_attachment(previous), "failed upload left old attachment presentable");
        check(fixture.backend.submit_frame(animation::AnimationTime{}) == runtime::FrameSubmissionResult::failed,
              "failed transaction presented partial data");
        check(!fixture.data().quads->geometry_dirty_ranges().empty() &&
                  !fixture.data().glyphs.geometry_dirty_ranges().empty() &&
                  !fixture.data().atlas.dirty_regions().empty(),
              "failure lost CPU retry data");
        check(fixture.resources.synchronize(fixture.data()), "retry failed");
        fixture.verify();
        uploads = fixture.backend.counters().uploads;
        check(fixture.resources.synchronize(fixture.data()), "post retry idle failed");
        check(fixture.backend.counters().uploads == uploads, "post retry idle uploaded");
    }
    const auto target = fixture.ui.inputs.mounted_inputs().front();
    const auto editor_before = fixture.ui.inputs.editors().require(target.editor).value();
    const auto old = fixture.attachment();
    const auto old_buffer = fixture.resources.quads()->handle();
    RecordingRenderer foreign;
    check(!foreign.attach_scene(old), "foreign backend accepted scene attachment");
    fixture.backend.reset_device();
    check(!fixture.backend.attach_scene(old), "stale epoch attachment accepted");
    check(fixture.resources.synchronize(fixture.data()), "epoch reconstruction failed");
    fixture.verify();
    check(fixture.resources.quads()->handle() != old_buffer, "reset reused stale handle address");
    check(fixture.backend.counters().rejected_releases == 0, "old resources released through new epoch");
    check(fixture.mounts == 1 && fixture.ui.inputs.mounted_inputs().front().component == target.component &&
              fixture.ui.inputs.editors().require(target.editor).value() == editor_before,
          "epoch reconstruction remounted component or lost editor state");
    const std::vector<graphics::QuadInstance> growth(fixture.resources.quads()->capacity() + 1);
    static_cast<void>(quads.append(growth));
    fixture.backend.fail_next(RecordingFailure::create);
    rejects([&] { fixture.resources.synchronize(fixture.data()); });
    check(!fixture.backend.valid_attachment(fixture.attachment()), "growth failure left attachment ready");
    check(fixture.resources.synchronize(fixture.data()), "growth retry failed");
    fixture.verify();
    auto retired = fixture.attachment();
    fixture.resources.retire();
    check(!fixture.backend.attach_scene(retired), "retired resources remained attachable");
    check(fixture.resources.synchronize(fixture.data()), "explicit resource retirement did not rebuild");
    fixture.verify();
}

void slider_scene_locality_and_retry() {
    Fixture fixture;
    check(fixture.resources.synchronize(fixture.data()), "Slider scene initial sync failed");
    fixture.verify();
    const auto mounted = fixture.ui.services.slider().mounted().front();
    const auto surface = mounted.surface;
    const auto before = fixture.backend.counters().uploaded_bytes;
    fixture.slider_value.set(75);
    fixture.layout();
    check(fixture.resources.synchronize(fixture.data()), "Slider local sync failed");
    check(fixture.backend.counters().uploaded_bytes - before < fixture.data().quads->instances().size_bytes(),
          "Slider local geometry uploaded full scene");
    fixture.verify();
    fixture.slider_value.set(65);
    fixture.layout();
    const auto attachment = fixture.attachment();
    fixture.backend.fail_next(RecordingFailure::commit);
    check(!fixture.resources.synchronize(fixture.data()), "Slider commit injection succeeded");
    check(!fixture.backend.valid_attachment(attachment), "failed Slider upload retained old attachment");
    check(fixture.resources.synchronize(fixture.data()), "Slider retry failed");
    fixture.verify();
    check(fixture.ui.services.slider().mounted().front().surface == surface && fixture.mounts == 1,
          "Slider value rebuilt component or surface");
    const auto uploads = fixture.backend.counters().uploads;
    fixture.layout();
    check(fixture.resources.synchronize(fixture.data()), "Slider idle sync failed");
    check(fixture.backend.counters().uploads == uploads, "idle Slider uploaded resources");
    fixture.backend.reset_device();
    check(fixture.resources.synchronize(fixture.data()), "Slider epoch recovery failed");
    fixture.verify();
}

void logical_resize_and_recovery() {
    Fixture fixture;
    check(fixture.resources.synchronize(fixture.data()), "logical scene initial sync failed");
    const auto cpu = fixture.data();
    const std::vector<graphics::QuadInstance> quads(cpu.quads->instances().begin(), cpu.quads->instances().end());
    const std::vector<graphics::GlyphInstance> glyphs(cpu.glyphs.instances().begin(), cpu.glyphs.instances().end());
    const auto rasterizations = fixture.ui.fonts->counters().rasterizations;
    const auto quad_handle = fixture.resources.quads()->handle();
    const auto glyph_handle = fixture.resources.glyphs().instance_buffer();
    auto upload_quads = fixture.resources.quads()->counters().uploaded_bytes;
    auto upload_glyphs = fixture.resources.glyphs().counters().buffer_uploaded_bytes;
    const auto texture_uploads = fixture.resources.glyphs().counters().texture_uploads;
    fixture.metrics = {640, 480, 1};
    check(cpu.quads->geometry_dirty_ranges().empty() && cpu.glyphs.geometry_dirty_ranges().empty(),
          "resize fixture retained dirty CPU geometry");
    check(fixture.resources.synchronize(fixture.data()), "resize without CPU dirties failed");
    check(fixture.resources.quads()->handle() == quad_handle &&
              fixture.resources.glyphs().instance_buffer() == glyph_handle,
          "resize recreated buffers without growth");
    check(fixture.resources.quads()->counters().uploaded_bytes - upload_quads ==
                  quads.size() * sizeof(QuadGpuInstance) &&
              fixture.resources.glyphs().counters().buffer_uploaded_bytes - upload_glyphs ==
                  glyphs.size() * sizeof(GlyphGpuInstance),
          "resize failed to upload every packed instance");
    check(fixture.resources.glyphs().counters().texture_uploads == texture_uploads,
          "projection-only resize uploaded atlas");
    fixture.verify();
    for (auto failure : {RecordingFailure::upload_exception, RecordingFailure::commit}) {
        fixture.metrics.pixel_width += 160;
        const auto prior = fixture.attachment();
        fixture.backend.fail_next(failure, failure == RecordingFailure::upload_exception ? 1 : 0);
        bool accepted = false;
        try {
            accepted = fixture.resources.synchronize(fixture.data());
        } catch (const std::runtime_error&) {
        }
        check(!accepted && !fixture.backend.valid_attachment(prior), "failed resize published a scene");
        check(fixture.resources.synchronize(fixture.data()), "resize retry failed");
        fixture.verify();
    }
    for (auto metrics :
         {SceneDeviceMetrics{0, 480, 1}, SceneDeviceMetrics{400, 200, std::numeric_limits<float>::quiet_NaN()},
          SceneDeviceMetrics{400, 200, std::numeric_limits<float>::infinity()},
          SceneDeviceMetrics{400, 200, std::numeric_limits<float>::denorm_min()}}) {
        const auto committed = fixture.attachment();
        const auto uploads = fixture.backend.counters().uploads;
        auto invalid = fixture.data();
        invalid.metrics = metrics;
        fixture.backend.fail_next(RecordingFailure::begin);
        rejects([&] { fixture.resources.synchronize(invalid); });
        check(!fixture.backend.valid_attachment(committed) && fixture.backend.counters().uploads == uploads,
              "invalid metrics uploaded or left a valid attachment");
        check(!fixture.resources.synchronize(fixture.data()),
              "invalid metrics consumed the begin-upload failure injection");
        check(fixture.resources.synchronize(fixture.data()), "valid sync after invalid metrics failed");
    }
    fixture.backend.reset_device();
    check(fixture.resources.synchronize(fixture.data()), "logical scene device reset failed");
    fixture.verify();
    check(same(std::as_bytes(cpu.quads->instances()), std::as_bytes(std::span(quads))) &&
              same(std::as_bytes(cpu.glyphs.instances()), std::as_bytes(std::span(glyphs))) &&
              fixture.ui.fonts->counters().rasterizations == rasterizations && fixture.mounts == 1,
          "packing/resize/recovery rewrote logical scene or remounted/rasterized components");
}

void deferred_surface_retains_uploads() {
    Fixture fixture;
    check(fixture.resources.synchronize(fixture.data()), "surface initial sync failed");
    check(fixture.backend.attach_scene(fixture.attachment()), "surface attach failed");
    auto uploads = fixture.backend.counters().uploads;
    runtime::FrameRequestState requests;
    SurfaceEvents events;
    runtime::OnDemandFrameLoop loop(requests, events, fixture.backend);
    fixture.backend.set_surface_available(false);
    requests.request_frame();
    check(loop.tick() == runtime::FrameLoopStep::deferred, "unavailable surface did not defer");
    check(loop.pending_presentation_revision() == 1, "surface defer lost presentation revision");
    for (int i = 0; i < 50; ++i) {
        check(loop.tick() == runtime::FrameLoopStep::idle, "surface defer spun");
    }
    check(fixture.resources.synchronize(fixture.data()), "deferred idle sync failed");
    check(fixture.backend.counters().uploads == uploads, "accepted data reuploaded while deferred");
    fixture.content.set(String{u8"挂起期间的最新内容 Latest"});
    fixture.layout();
    check(fixture.resources.synchronize(fixture.data()), "latest deferred data failed");
    check(fixture.backend.attach_scene(fixture.attachment()), "latest deferred attachment failed");
    requests.request_frame();
    check(loop.tick() == runtime::FrameLoopStep::deferred && loop.pending_presentation_revision() == 2,
          "latest surface content did not supersede deferred revision");
    uploads = fixture.backend.counters().uploads;
    fixture.backend.set_surface_available(true);
    requests.request_frame();
    check(loop.tick() == runtime::FrameLoopStep::submitted && !loop.pending_presentation_revision() &&
              fixture.backend.counters().uploads == uploads && events.waits == 0,
          "surface resume lost content, reuploaded accepted data, or waited");
    fixture.verify();
}

void real_component_animation_uses_future_callback() {
    Fixture fixture;

    struct Deadline final : runtime::FrameDeadlineSource {
        explicit Deadline(Fixture& value) : fixture(&value) {}

        std::optional<animation::AnimationTime> next_deadline() const override {
            return fixture->ui.services.next_frame_deadline();
        }

        Fixture* fixture;
    } deadlines{fixture};

    struct Submitter final : runtime::FrameSubmitter {
        explicit Submitter(Fixture& value) : fixture(&value) {}

        runtime::FrameSubmissionResult submit_frame(animation::AnimationTime time) override {
            static_cast<void>(fixture->ui.services.tick_animations(time));
            fixture->layout();
            if (!fixture->resources.synchronize(fixture->data()) ||
                !fixture->backend.attach_scene(fixture->attachment())) {
                return runtime::FrameSubmissionResult::failed;
            }
            return fixture->backend.submit_frame(time);
        }

        Fixture* fixture;
    } submitter{fixture};

    struct Host final : runtime::FrameCallbackHost {
        void replace_callback(std::optional<animation::AnimationTime> value,
                              runtime::FrameCallback cb) noexcept override {
            deadline = value;
            callback = cb;
            scheduled = true;
        }

        void cancel_callback() noexcept override {
            scheduled = false;
        }

        std::optional<animation::AnimationTime> deadline;
        runtime::FrameCallback callback;
        bool scheduled{};
    } host;

    SurfaceEvents events;
    auto& requests = fixture.ui.frames;
    runtime::OnDemandFrameLoop loop(requests, events, submitter, deadlines);
    runtime::CallbackFramePump pump(requests, loop, host);
    fixture.ui.buttons.set_motion_preference(animation::MotionPreference::normal);
    const auto button = fixture.ui.buttons.mounted_buttons().front();
    check(fixture.ui.services.focus().request_focus(button.interaction, input::FocusModality::keyboard),
          "real Input blur transition did not start");
    const auto rect = fixture.ui.nodes.require(button.node).bounds;
    input::PointerInputEvent hover{};
    hover.pointer = input::PointerIdentity::mouse();
    hover.action = input::PointerAction::move;
    hover.x = rect.x + rect.width / 2;
    hover.y = rect.y + rect.height / 2;
    fixture.ui.services.pointer().dispatch(hover);
    check(fixture.ui.services.animations().size() > 0, "real Button hover did not animate");
    check(host.callback.run(animation::AnimationTime::microseconds(100000)) == runtime::FrameLoopStep::submitted,
          "real component animation did not submit");
    check(!requests.pending() && host.scheduled && host.deadline &&
              *host.deadline > animation::AnimationTime::microseconds(100000),
          "real Button/Input invalidation scheduled immediate animation loop");
    fixture.backend.set_surface_available(false);
    check(host.callback.run(*host.deadline) == runtime::FrameLoopStep::deferred && !requests.pending() && host.deadline,
          "real deferred animation self-requested immediate retry");
}

void upload_exceptions_release_temporary_resources() {
    RecordingRenderer backend;
    graphics::QuadInstanceStore quads;
    const std::array quad{graphics::QuadInstance{}};
    static_cast<void>(quads.append(quad));
    check(backend.begin_upload_batch(), "Quad initial begin failed");
    backend.fail_next(RecordingFailure::upload_exception);
    rejects([&] { detail::QuadGpuBuffer failed{backend, quads, {100, 100, 1}}; });
    backend.cancel_upload_batch();
    check(backend.live_resources() == 0, "Quad constructor leaked temporary buffer");
    {
        check(backend.begin_upload_batch(), "Quad retry begin failed");
        detail::QuadGpuBuffer buffer{backend, quads, {100, 100, 1}};
        check(backend.finish_upload_batch(), "Quad retry commit failed");
        const std::vector<graphics::QuadInstance> growth(buffer.capacity() + 1);
        static_cast<void>(quads.append(growth));
        check(backend.begin_upload_batch(), "Quad growth begin failed");
        backend.fail_next(RecordingFailure::upload_exception);
        rejects([&] { buffer.synchronize(quads, {100, 100, 1}); });
        backend.cancel_upload_batch();
        check(backend.live_resources() == 1, "Quad growth leaked or retired old buffer");
        check(backend.begin_upload_batch(), "Quad growth retry begin failed");
        buffer.synchronize(quads, {100, 100, 1});
        check(backend.finish_upload_batch(), "Quad growth retry commit failed");
        check(backend.live_resources() == 1, "Quad growth retry retained obsolete buffer");
    }
    check(backend.live_resources() == 0, "Quad destruction leaked");
    {
        GlyphGpuResources resources{backend};
        graphics::GlyphAtlas atlas;
        graphics::GlyphInstanceStore glyphs;
        const std::array glyph{graphics::GlyphInstance{}};
        static_cast<void>(glyphs.append(glyph));
        check(backend.begin_upload_batch(), "Glyph initial begin failed");
        backend.fail_next(RecordingFailure::upload_exception);
        rejects([&] { resources.synchronize(atlas, glyphs, {100, 100, 1}); });
        backend.cancel_upload_batch();
        check(backend.live_resources() == 1, "Glyph initial upload leaked buffer");
        check(backend.begin_upload_batch(), "Glyph retry begin failed");
        resources.synchronize(atlas, glyphs, {100, 100, 1});
        check(backend.finish_upload_batch(), "Glyph retry commit failed");
        const std::vector<graphics::GlyphInstance> growth(resources.instance_capacity() + 1);
        static_cast<void>(glyphs.append(growth));
        check(backend.begin_upload_batch(), "Glyph growth begin failed");
        backend.fail_next(RecordingFailure::upload_exception);
        rejects([&] { resources.synchronize(atlas, glyphs, {100, 100, 1}); });
        backend.cancel_upload_batch();
        check(backend.live_resources() == 2, "Glyph growth leaked or retired old buffer");
        check(backend.begin_upload_batch(), "Glyph growth retry begin failed");
        resources.synchronize(atlas, glyphs, {100, 100, 1});
        check(backend.finish_upload_batch(), "Glyph growth retry commit failed");
        check(backend.live_resources() == 2, "Glyph growth retry retained obsolete buffer");
    }
    check(backend.live_resources() == 0, "Glyph destruction leaked");
    {
        RoundedEffectGpuResources resources{backend};
        graphics::RoundedEffectStore effects;
        const auto effect =
            graphics::make_shadow_effect({{1, 1, 10, 10}, 2}, {ShadowKind::outer, {}, 2, 0, Color::rgba8(0, 0, 0, 80)});
        static_cast<void>(effects.add(effect));
        check(backend.begin_upload_batch(), "Effect initial begin failed");
        backend.fail_next(RecordingFailure::upload_exception);
        rejects([&] { resources.synchronize(effects, {320, 240, 1}); });
        backend.cancel_upload_batch();
        check(backend.live_resources() == 0, "Effect initial upload leaked buffer");
        check(backend.begin_upload_batch(), "Effect retry begin failed");
        resources.synchronize(effects, {320, 240, 1});
        check(backend.finish_upload_batch(), "Effect retry commit failed");
        const std::vector<graphics::RoundedEffectInstance> growth(resources.capacity() + 1, effect);
        static_cast<void>(effects.add_batch(growth));
        check(backend.begin_upload_batch(), "Effect growth begin failed");
        backend.fail_next(RecordingFailure::upload_exception);
        rejects([&] { resources.synchronize(effects, {320, 240, 1}); });
        backend.cancel_upload_batch();
        check(backend.live_resources() == 1, "Effect growth leaked or retired old buffer");
        check(backend.begin_upload_batch(), "Effect growth retry begin failed");
        resources.synchronize(effects, {320, 240, 1});
        check(backend.finish_upload_batch(), "Effect growth retry commit failed");
        check(backend.live_resources() == 1, "Effect growth retry retained obsolete buffer");
    }
    check(backend.live_resources() == 0, "Effect destruction leaked");
}
} // namespace

int main() {
    try {
        owned_bytes_and_ranges();
        texture_source_lifetime_cancel_and_invalid_ranges();
        required_capabilities_and_scene_limits();
        real_scene_transaction_and_epoch();
        slider_scene_locality_and_retry();
        logical_resize_and_recovery();
        deferred_surface_retains_uploads();
        upload_exceptions_release_temporary_resources();
        real_component_animation_uses_future_callback();
    } catch (const std::exception& error) {
        std::cerr << error.what() << '\n';
        return 1;
    }
    std::cout << "Shared component scene, owned bytes, retry, ordered draw and device epoch passed\n";
}
