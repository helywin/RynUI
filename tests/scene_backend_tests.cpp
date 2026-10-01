#include "renderer/common/scene_resources.hpp"
#include "renderer/recording/recording_renderer.hpp"
#include "support/input_fixture.hpp"

#include <algorithm>
#include <array>
#include <iostream>
#include <limits>

namespace {
using namespace ryn;
using namespace ryn::detail;

void check(bool value, const char *message) {
    if (!value)
        throw std::runtime_error(message);
}
template <class F> void rejects(F &&action) {
    bool rejected = false;
    try {
        action();
    } catch (const std::exception &) {
        rejected = true;
    }
    check(rejected, "invalid handle or upload did not reject");
}

bool same(std::span<const std::byte> actual, std::span<const std::byte> expected) {
    return std::ranges::equal(actual, expected);
}

void owned_bytes_and_ranges() {
    RecordingRenderer backend, other;
    auto *buffer = backend.create_vertex_buffer(8);
    auto *texture = backend.create_glyph_texture(3, 2);
    std::array bytes{std::byte{1}, std::byte{2}, std::byte{3}, std::byte{4},
                     std::byte{5}, std::byte{6}, std::byte{7}, std::byte{8}};
    const auto original = bytes;
    check(backend.begin_upload_batch(), "batch begin failed");
    check(backend.upload(buffer, 0, bytes), "buffer copy failed");
    check(backend.upload_glyph_texture(texture, {0, {0, 0, 3, 2}, 0, 4, 2, bytes}),
          "texture copy failed");
    bytes.fill(std::byte{99});
    check(backend.finish_upload_batch(), "batch commit failed");
    check(same(backend.buffer_bytes(buffer), original), "upload retained caller memory");
    const std::array texture_expected{std::byte{1}, std::byte{2}, std::byte{3},
                                      std::byte{5}, std::byte{6}, std::byte{7}};
    check(same(backend.texture_bytes(texture), texture_expected),
          "texture rows ignored stride or copied caller memory");
    check(backend.begin_upload_batch(), "range batch begin failed");
    check(!backend.upload(buffer, std::numeric_limits<std::size_t>::max(), bytes),
          "overflow range accepted");
    check(!backend.upload_glyph_buffer(buffer, 0, bytes), "wrong-kind handle accepted");
    check(!backend.upload_glyph_texture(texture, {0, {2, 1, 3, 2}, 0, 4, 2, bytes}),
          "texture range accepted");
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

struct Fixture final {
    Signal<String> content{String{u8"共同场景 Hello"}};
    ryn_test::input_component::Fixture ui;
    RecordingRenderer backend;
    SceneResources resources{backend};
    int mounts{};

    Fixture() {
        ui.services.mount(Content{[this] {
            ++mounts;
            Input(
                InputProps{}.defaultValue(u8"编辑状态 kept").layout(LayoutStyle{}.width(dp(180))));
            Button(ButtonProps{},
                   ButtonContent{[] { Text(TypographyProps{}.content(u8"按钮 Button")); }});
            Text(TypographyProps{}.content(content).underline(true));
            Divider(DividerProps{}.content(u8"分隔 Label"));
        }});
        const auto target = ui.inputs.mounted_inputs().front();
        check(ui.services.focus().request_focus(target.interaction, input::FocusModality::keyboard),
              "focus failed");
        check(bool(ui.inputs.editors().require(target.editor).select({0, 2})),
              "editor select failed");
        layout();
    }
    void layout() { ui.synchronize(320); }
    SceneCpuData data() {
        return {&ui.services.surfaces().instances(),
                ui.scene.atlas(),
                ui.scene.glyph_scene().instances(),
                &ui.services.rounded_effects(),
                {320, 240, 1}};
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
        check(resources.quads(), "component scene omitted Quad resources");
        check(same(backend.buffer_bytes(resources.quads()->handle())
                       .first(cpu.quads->instances().size_bytes()),
                   std::as_bytes(cpu.quads->instances())),
              "Quad bytes differ from component scene");
        check(same(backend.buffer_bytes(resources.glyphs().instance_buffer())
                       .first(cpu.glyphs.instances().size_bytes()),
                   std::as_bytes(cpu.glyphs.instances())),
              "Glyph bytes differ from component scene");
        check(resources.effects().instance_count() > 0,
              "focused component fixture omitted effects");
        check(same(backend.buffer_bytes(resources.effects().buffer())
                       .first(resources.effects().instances().size_bytes()),
                   std::as_bytes(resources.effects().instances())),
              "Effect bytes differ from packed scene");
        for (std::uint32_t page = 0; page < cpu.atlas.page_count(); ++page)
            check(same(backend.texture_bytes(resources.glyphs().texture(page)),
                       std::as_bytes(cpu.atlas.page_bytes(page))),
                  "Atlas pixels differ from CPU page");
        check(backend.attach_scene(attachment()), "shared scene attachment rejected");
        check(backend.submit_frame(animation::AnimationTime::microseconds(0)) ==
                  runtime::FrameSubmissionResult::submitted,
              "ordered frame submission failed");
        const auto commands = ui.services.scene_composer().ordered_scene().commands();
        check(backend.draws().size() == commands.size(), "ordered draw count differs");
        bool quad = false, glyph = false, effect = false;
        for (std::size_t index = 0; index < commands.size(); ++index) {
            check(backend.draws()[index].command == commands[index], "draw order changed");
            check(!backend.draws()[index].instance_bytes.empty(),
                  "draw did not consume real bytes");
            quad |= commands[index].kind == graphics::SceneDrawKind::quad;
            glyph |= commands[index].kind == graphics::SceneDrawKind::glyph;
            effect |= commands[index].kind == graphics::SceneDrawKind::rounded_effect;
        }
        check(quad && glyph && effect, "fixture did not cover all draw kinds");
    }
};

void real_scene_transaction_and_epoch() {
    Fixture fixture;
    check(fixture.resources.synchronize(fixture.data()), "initial transaction failed");
    fixture.verify();
    auto uploads = fixture.backend.counters().uploads;
    check(fixture.resources.synchronize(fixture.data()), "idle transaction failed");
    check(fixture.backend.counters().uploads == uploads, "idle reuploaded resources");
    // Material changes use the existing reactive component path and a bounded range.
    auto &quads = *fixture.data().quads;
    const std::array material{graphics::QuadMaterial{{1, 0, 0, 1}, 0.75F}};
    check(quads.update_material({0, 1}, material) == 1, "material update failed");
    const auto bytes_before = fixture.backend.counters().uploaded_bytes;
    check(fixture.resources.synchronize(fixture.data()), "partial transaction failed");
    check(fixture.backend.counters().uploaded_bytes - bytes_before ==
              sizeof(graphics::QuadInstance),
          "local material update uploaded unrelated resources");
    fixture.verify();
    for (const auto failure : {RecordingFailure::begin, RecordingFailure::upload,
                               RecordingFailure::commit, RecordingFailure::upload_exception}) {
        fixture.content.set(String{u8"改变后的共同组件 Update"});
        fixture.layout();
        fixture.mark_all();
        const auto previous = fixture.attachment();
        fixture.backend.fail_next(failure, failure == RecordingFailure::upload ? 1 : 0);
        bool success = false;
        try {
            success = fixture.resources.synchronize(fixture.data());
        } catch (const std::runtime_error &) {
        }
        check(!success, "injected failure did not fail");
        check(!fixture.backend.valid_attachment(previous),
              "failed upload left old attachment presentable");
        check(fixture.backend.submit_frame(animation::AnimationTime{}) ==
                  runtime::FrameSubmissionResult::failed,
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
    check(fixture.backend.counters().rejected_releases == 0,
          "old resources released through new epoch");
    check(fixture.mounts == 1 &&
              fixture.ui.inputs.mounted_inputs().front().component == target.component &&
              fixture.ui.inputs.editors().require(target.editor).value() == editor_before,
          "epoch reconstruction remounted component or lost editor state");
    const std::vector<graphics::QuadInstance> growth(fixture.resources.quads()->capacity() + 1);
    static_cast<void>(quads.append(growth));
    fixture.backend.fail_next(RecordingFailure::create);
    rejects([&] { fixture.resources.synchronize(fixture.data()); });
    check(!fixture.backend.valid_attachment(fixture.attachment()),
          "growth failure left attachment ready");
    check(fixture.resources.synchronize(fixture.data()), "growth retry failed");
    fixture.verify();
    auto retired = fixture.attachment();
    fixture.resources.retire();
    check(!fixture.backend.attach_scene(retired), "retired resources remained attachable");
    check(fixture.resources.synchronize(fixture.data()),
          "explicit resource retirement did not rebuild");
    fixture.verify();
}

void deferred_surface_retains_uploads() {
    Fixture fixture;
    check(fixture.resources.synchronize(fixture.data()), "surface initial sync failed");
    check(fixture.backend.attach_scene(fixture.attachment()), "surface attach failed");
    auto uploads = fixture.backend.counters().uploads;
    fixture.backend.set_surface_available(false);
    check(fixture.backend.submit_frame(animation::AnimationTime{}) ==
              runtime::FrameSubmissionResult::deferred,
          "unavailable surface did not defer");
    check(fixture.resources.synchronize(fixture.data()), "deferred idle sync failed");
    check(fixture.backend.counters().uploads == uploads, "accepted data reuploaded while deferred");
    fixture.content.set(String{u8"挂起期间的最新内容 Latest"});
    fixture.layout();
    check(fixture.resources.synchronize(fixture.data()), "latest deferred data failed");
    fixture.backend.set_surface_available(true);
    fixture.verify();
}

void upload_exceptions_release_temporary_resources() {
    RecordingRenderer backend;
    graphics::QuadInstanceStore quads;
    const std::array quad{graphics::QuadInstance{}};
    static_cast<void>(quads.append(quad));
    check(backend.begin_upload_batch(), "Quad initial begin failed");
    backend.fail_next(RecordingFailure::upload_exception);
    rejects([&] { graphics::QuadGpuBuffer failed{backend, quads}; });
    backend.cancel_upload_batch();
    check(backend.live_resources() == 0, "Quad constructor leaked temporary buffer");
    {
        check(backend.begin_upload_batch(), "Quad retry begin failed");
        graphics::QuadGpuBuffer buffer{backend, quads};
        check(backend.finish_upload_batch(), "Quad retry commit failed");
        const std::vector<graphics::QuadInstance> growth(buffer.capacity() + 1);
        static_cast<void>(quads.append(growth));
        check(backend.begin_upload_batch(), "Quad growth begin failed");
        backend.fail_next(RecordingFailure::upload_exception);
        rejects([&] { buffer.synchronize(quads); });
        backend.cancel_upload_batch();
        check(backend.live_resources() == 1, "Quad growth leaked or retired old buffer");
        check(backend.begin_upload_batch(), "Quad growth retry begin failed");
        buffer.synchronize(quads);
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
        rejects([&] { resources.synchronize(atlas, glyphs); });
        backend.cancel_upload_batch();
        check(backend.live_resources() == 1, "Glyph initial upload leaked buffer");
        check(backend.begin_upload_batch(), "Glyph retry begin failed");
        resources.synchronize(atlas, glyphs);
        check(backend.finish_upload_batch(), "Glyph retry commit failed");
        const std::vector<graphics::GlyphInstance> growth(resources.instance_capacity() + 1);
        static_cast<void>(glyphs.append(growth));
        check(backend.begin_upload_batch(), "Glyph growth begin failed");
        backend.fail_next(RecordingFailure::upload_exception);
        rejects([&] { resources.synchronize(atlas, glyphs); });
        backend.cancel_upload_batch();
        check(backend.live_resources() == 2, "Glyph growth leaked or retired old buffer");
        check(backend.begin_upload_batch(), "Glyph growth retry begin failed");
        resources.synchronize(atlas, glyphs);
        check(backend.finish_upload_batch(), "Glyph growth retry commit failed");
        check(backend.live_resources() == 2, "Glyph growth retry retained obsolete buffer");
    }
    check(backend.live_resources() == 0, "Glyph destruction leaked");
    {
        RoundedEffectGpuResources resources{backend};
        graphics::RoundedEffectStore effects;
        const auto effect = graphics::make_shadow_effect(
            {{1, 1, 10, 10}, 2}, {ShadowKind::outer, {}, 2, 0, Color::rgba8(0, 0, 0, 80)});
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
        real_scene_transaction_and_epoch();
        deferred_surface_retains_uploads();
        upload_exceptions_release_temporary_resources();
    } catch (const std::exception &error) {
        std::cerr << error.what() << '\n';
        return 1;
    }
    std::cout
        << "Shared component scene, owned bytes, retry, ordered draw and device epoch passed\n";
}
