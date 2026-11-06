#include "renderer/common/scene_resources.hpp"
#include "renderer/recording/recording_renderer.hpp"
#include "graphics/color_field.hpp"

#include <algorithm>
#include <array>
#include <cstring>
#include <iostream>
#include <numbers>
#include <stdexcept>

namespace {
using namespace ryn;
using namespace ryn::graphics;
using namespace ryn::detail;

void require(bool value, const char* message) {
    if (!value) {
        throw std::runtime_error(message);
    }
}

template <class F> void rejects(F&& action) {
    try {
        action();
    } catch (const std::exception&) {
        return;
    }
    throw std::runtime_error("invalid paint resource operation accepted");
}

EffectPaintField field(Color color) {
    return {EffectGradientRamp{{0, color}, {1, color}}, EffectLinearDomain{{10, 10}, {30, 10}}};
}

RoundedEffectInstance solid() {
    return make_shadow_effect({{10, 10, 20, 20}, 2}, {ShadowKind::outer, {}, 0, 0, Color{0.2F, 0.4F, 0.6F, 0.5F}});
}

RoundedEffectInstance painted(Color color = Color{1, 0, 0, 0.5F}, bool underlay = false) {
    auto value = solid();
    value.material.paint = EffectPaint{field(color), std::nullopt};
    if (underlay) {
        value.material.paint->underlay = field(Color{0, 0, 1, 0.5F});
    }
    return value;
}

struct Fixture final {
    RecordingRenderer backend;
    GlyphAtlas atlas;
    GlyphInstanceStore glyphs;
    RoundedEffectStore effects;
    ImageStore images;
    SceneResources resources;
    OrderedScene order;

    explicit Fixture(SceneBackendCapabilities caps = recording_scene_capabilities())
        : backend(caps), resources(backend) {}

    bool sync(SceneDeviceMetrics metrics = {100, 100, 1}) {
        return resources.synchronize({nullptr, atlas, glyphs, &effects, metrics, &images});
    }

    void draw() {
        require(backend.attach_scene(resources.attach(order)), "paint attachment rejected");
        require(backend.submit_frame({}) == runtime::FrameSubmissionResult::submitted, "paint draw rejected");
    }
};

template <class T> void same_bytes(std::span<const std::byte> bytes, std::span<const T> expected) {
    const auto packed = std::as_bytes(expected);
    require(bytes.size() >= packed.size() && std::ranges::equal(bytes.first(packed.size()), packed),
            "Recording buffer bytes differ from packed projection");
}

void projection(Fixture& f, SceneDeviceMetrics metrics = {100, 100, 1}) {
    const auto source = f.effects.packed_instances();
    std::vector<RoundedEffectGpuInstance> base;
    for (const auto& instance : source) {
        base.push_back(pack_effect_paint_base_instance(instance, metrics));
    }
    const auto binding = f.resources.effects().draw_binding(0, static_cast<std::uint32_t>(source.size()));
    same_bytes(f.backend.buffer_bytes(binding.base), std::span<const RoundedEffectGpuInstance>{base});
    EffectPaintPackedTables tables;
    pack_effect_paint_tables(source, metrics, tables);
    if (tables.links.empty()) {
        require(!binding.link && !binding.field && !binding.stop, "old solid scene retained paint tables");
        return;
    }
    same_bytes(f.backend.buffer_bytes(binding.link), std::span<const EffectPaintGpuLink>{tables.links});
    same_bytes(f.backend.buffer_bytes(binding.field), std::span<const EffectPaintGpuField>{tables.fields});
    same_bytes(f.backend.buffer_bytes(binding.stop), std::span<const EffectPaintGpuStop>{tables.stops});
    const auto caps = f.resources.effects().paint_capacities();
    require(f.backend.buffer_bytes(binding.link).size() == std::size_t(caps[0]) * sizeof(EffectPaintGpuLink) &&
                f.backend.buffer_bytes(binding.field).size() == std::size_t(caps[1]) * sizeof(EffectPaintGpuField) &&
                f.backend.buffer_bytes(binding.stop).size() == std::size_t(caps[2]) * sizeof(EffectPaintGpuStop),
            "table allocation used count instead of declared capacity");
}

void mixed_nonzero_draws_and_updates() {
    Fixture f;
    const auto ids = f.effects.add_batch(
        std::array{solid(), painted(Color{1, 0, 0, 0.5F}, true), solid(), painted(Color{0, 1, 0, 0.5F})});
    f.order.append_command({SceneDrawKind::rounded_effect, 1, 2, invalid_glyph_atlas_page});
    f.order.append_command({SceneDrawKind::rounded_effect, 0, 1, invalid_glyph_atlas_page});
    f.order.append_command({SceneDrawKind::rounded_effect, 3, 1, invalid_glyph_atlas_page});
    require(f.sync(), "mixed paint scene failed");
    projection(f);
    f.draw();
    const auto draws = f.backend.draws();
    require(draws.size() == 3 && draws[0].command.first_instance == 1 &&
                draws[0].effect_binding.base_byte_offset == 112 && draws[0].effect_binding.link_byte_offset == 16 &&
                draws[0].instance_bytes.size() == 224 && draws[0].effect_link_bytes.size() == 32 &&
                draws[0].effect_field_indices == std::vector<std::uint32_t>{0, 1} &&
                draws[0].effect_stop_indices == std::vector<std::uint32_t>{0, 1, 2, 3} &&
                draws[1].effect_field_bytes.empty() && draws[2].effect_field_indices == std::vector<std::uint32_t>{2} &&
                draws[2].effect_stop_indices == std::vector<std::uint32_t>{4, 5},
            "nonzero first, mixed solid/paint or multi-draw table indices differ");
    const auto& tables = f.resources.effects().paint_tables();
    same_bytes(std::span<const std::byte>{draws[0].effect_field_bytes},
               std::span<const EffectPaintGpuField>{tables.fields}.first(2));
    same_bytes(std::span<const std::byte>{draws[2].effect_stop_bytes},
               std::span<const EffectPaintGpuStop>{tables.stops}.subspan(4, 2));
    auto material = f.effects.at(ids[1]).material;
    material.paint = EffectPaint{field(Color{0, 1, 0, 0.25F}), std::nullopt};
    require(f.effects.update_material(ids[1], material), "paint material fixture did not dirty");
    const auto uploads = f.backend.counters().uploads;
    require(f.sync() && f.backend.counters().uploads == uploads + 4,
            "base cleared material dirty before paint tables consumed it");
    projection(f);
    const auto idle = f.backend.counters().uploads;
    require(f.sync() && f.backend.counters().uploads == idle, "idle paint scene uploaded bytes");
}

void remap_hidden_cull_delete_and_slot_reuse() {
    Fixture f;
    auto hidden = solid();
    hidden.material.visible = false;
    const auto skipped = f.effects.add(hidden);
    const auto first = f.effects.add(painted());
    const auto middle = f.effects.add(solid());
    const auto last = f.effects.add(painted(Color{0, 1, 0}));
    require(f.effects.remove(skipped), "hidden slot removal failed");
    const auto reused = f.effects.add(painted(Color{0, 0, 1}));
    require(reused.index == skipped.index && reused.generation != skipped.generation && f.sync(),
            "slot reuse fixture failed");
    require(f.effects.packed_index(first) == 0 && f.effects.packed_index(reused) == 3,
            "packed order used Store ID.index");
    projection(f);
    auto material = f.effects.at(first).material;
    material.visible = false;
    require(f.effects.update_material(first, material) && f.sync() && f.effects.packed_index(middle) == 0 &&
                f.effects.packed_index(last) == 1 && f.effects.packed_index(reused) == 2,
            "hidden effect did not remap all tables with base");
    projection(f);
    auto geometry = f.effects.at(last).geometry;
    geometry.translation = {1000, 0};
    require(f.effects.update_geometry(last, geometry) && f.sync() && !f.effects.packed_index(last) &&
                f.effects.packed_index(reused) == 1,
            "culled effect retained a table index");
    projection(f);
    require(f.effects.remove(middle), "solid removal failed");
    const auto appended = f.effects.add(painted(Color{1, 1, 0}));
    require(appended.index == middle.index && f.sync() && f.effects.packed_index(reused) == 0 &&
                f.effects.packed_index(appended) == 1,
            "deleted/reused slot did not preserve ordered mapping");
    projection(f);
}

void failures_retry_without_new_dirty() {
    for (const auto failure : {RecordingFailure::create, RecordingFailure::create_exception, RecordingFailure::upload,
                               RecordingFailure::upload_exception}) {
        for (std::size_t table = 0; table < 4; ++table) {
            Fixture f;
            require(f.sync(), "empty priming sync failed");
            const auto live = f.backend.live_resources();
            static_cast<void>(f.effects.add(painted(Color{1, 0, 0, 0.5F}, true)));
            f.order.append_command({SceneDrawKind::rounded_effect, 0, 1, invalid_glyph_atlas_page});
            const auto old = f.resources.attach(f.order);
            f.backend.fail_next(failure, table);
            rejects([&] { static_cast<void>(f.sync()); });
            require(!f.backend.valid_attachment(old) && !f.backend.valid_attachment(f.resources.attach(f.order)) &&
                        f.backend.live_resources() == live,
                    "failed base/link/field/stop create/upload retained attachment or leaked candidate buffers");
            f.effects.clear_dirty_ranges();
            const auto uploads = f.backend.counters().uploads;
            require(f.sync() && f.backend.counters().uploads == uploads + 4,
                    "failed paint projection did not retry all four buffers without CPU dirty");
            projection(f);
            f.draw();
        }
    }
    for (const auto failure : {RecordingFailure::begin, RecordingFailure::commit, RecordingFailure::upload,
                               RecordingFailure::upload_exception}) {
        const auto trials =
            failure == RecordingFailure::upload || failure == RecordingFailure::upload_exception ? 4 : 1;
        for (int table = 0; table < trials; ++table) {
            Fixture f;
            static_cast<void>(f.effects.add(painted()));
            require(f.sync(), "existing paint fixture failed");
            const auto old = f.resources.attach(f.order);
            require(f.effects.geometry_dirty_ranges().empty() && f.effects.material_dirty_ranges().empty(),
                    "paint fixture has unexpected dirty data");
            f.backend.fail_next(failure, static_cast<std::size_t>(table));
            bool failed{};
            try {
                failed = !f.sync({200, 200, 2});
            } catch (const std::exception&) {
                failed = true;
            }
            require(failed && !f.backend.valid_attachment(old), "paint failure retained old attachment");
            const auto uploads = f.backend.counters().uploads;
            require(f.sync() && f.backend.counters().uploads == uploads + 4,
                    "return to original metrics skipped complete paint retry after begin/commit/upload failure");
            projection(f);
            const auto idle = f.backend.counters().uploads;
            require(f.sync() && f.backend.counters().uploads == idle, "retry left paint permanently dirty");
        }
    }
}

void later_image_failure_retries_effects() {
    for (const auto failure : {RecordingFailure::upload, RecordingFailure::upload_exception}) {
        for (std::size_t image_upload = 0; image_upload < 2; ++image_upload) {
            Fixture f;
            static_cast<void>(f.effects.add(painted()));
            f.order.append_command({SceneDrawKind::rounded_effect, 0, 1, invalid_glyph_atlas_page});
            require(f.sync(), "image failure paint fixture failed");
            const auto old = f.resources.attach(f.order);
            const std::array bytes{std::byte{255}, std::byte{0}, std::byte{0}, std::byte{128}};
            const auto image =
                f.images.create({ImagePixels::copy(1, 1, 4, bytes), {0, 0, 4, 4}, {0, 0, 100, 100}, {0, 0}});
            f.order.append_command({SceneDrawKind::image, image.index, 1, image.generation});
            f.backend.fail_next(failure, 4 + image_upload);
            rejects([&] { static_cast<void>(f.sync({200, 200, 2})); });
            require(!f.backend.valid_attachment(old) && f.effects.geometry_dirty_ranges().empty() &&
                        f.effects.material_dirty_ranges().empty(),
                    "later Image failure fixture did not exercise already-cleared Effect dirty data");
            const auto uploads = f.resources.effects().counters().buffer_uploads;
            require(f.sync() && f.resources.effects().counters().buffer_uploads == uploads + 4,
                    "later Image failure did not invalidate base and all paint tables");
            projection(f);
            f.draw();
        }
    }
}

void old_solid_cap_placeholders_epoch_and_retire() {
    Fixture f;
    const auto id = f.effects.add(solid());
    f.order.append_command({SceneDrawKind::rounded_effect, 0, 1, invalid_glyph_atlas_page});
    require(f.sync(), "old solid sync failed");
    projection(f);
    const auto old_resources = f.backend.live_resources();
    require(f.resources.effects().paint_capacities() == std::array<std::uint32_t, 3>{},
            "old solid allocated paint capacity");
    const auto arc = make_arc_effect({20, 20}, 10, 4, 0, 90, ArcCap::Square, Color{1, 0, 0});
    require(f.effects.update_geometry(id, arc.geometry) && f.sync(), "cap-only sync failed");
    projection(f);
    require(f.backend.live_resources() == old_resources + 3 &&
                f.resources.effects().paint_capacities() == std::array<std::uint32_t, 3>{1, 1, 1} &&
                f.resources.effects().paint_tables().fields == std::vector<EffectPaintGpuField>(1) &&
                f.resources.effects().paint_tables().stops == std::vector<EffectPaintGpuStop>(1),
            "cap-only pipeline omitted minimum zero storage placeholders");
    f.draw();
    require(f.backend.draws()[0].effect_binding.field_count == 1 &&
                f.backend.draws()[0].effect_binding.stop_count == 1 && f.backend.draws()[0].effect_field_bytes.empty(),
            "cap-only draw omitted bound placeholders or referenced the dummy field");
    require(f.effects.update_geometry(id, solid().geometry) && f.sync() && f.backend.live_resources() == old_resources,
            "return to old solid retained new resources");
    std::vector<RoundedEffectId> added;
    for (int index = 0; index < 8; ++index) {
        added.push_back(f.effects.add(painted(Color{1, 0, 0}, true)));
    }
    require(f.sync(), "table growth failed");
    require(f.resources.effects().paint_capacities()[0] == 16 && f.resources.effects().paint_capacities()[2] == 32,
            "table growth was not power-of-two capacity");
    for (const auto remove : added) {
        require(f.effects.remove(remove), "epoch source removal failed");
    }
    require(f.effects.update_material(id, painted().material) && f.sync(), "pre-epoch remaining paint fixture failed");
    const auto old = f.resources.attach(f.order);
    auto caps = recording_scene_capabilities();
    caps.maximum_buffer_bytes = 128;
    f.backend.reset_device(caps);
    require(!f.backend.valid_attachment(old) && f.sync() && f.resources.effects().capacity() == 1 &&
                f.resources.effects().paint_capacities() == std::array<std::uint32_t, 3>{1, 1, 2} &&
                f.backend.counters().rejected_releases == 0,
            "epoch rebuild inherited old capacity or released stale handles through new device");
    projection(f);
    f.resources.retire();
    require(f.backend.live_resources() == 0 && !f.backend.valid_attachment(f.resources.attach(f.order)),
            "retire leaked paint tables or kept an attachment ready");
    require(f.sync(), "retired resources could not rebuild");
}

void raw_owned_bytes_foreign_owner_and_kind() {
    for (const auto kind : {EffectPaintBufferKind::link, EffectPaintBufferKind::field, EffectPaintBufferKind::stop}) {
        RecordingRenderer backend;
        RecordingRenderer other;
        auto* buffer = backend.create_effect_paint_buffer(kind, 32);
        std::array<std::byte, 32> source;
        source.fill(std::byte{42});
        const auto expected = source;
        require(backend.begin_upload_batch() && backend.upload_effect_paint_buffer(kind, buffer, 0, source),
                "paint raw upload failed");
        source.fill(std::byte{99});
        require(backend.finish_upload_batch() && std::ranges::equal(backend.buffer_bytes(buffer), expected),
                "paint upload borrowed caller bytes after successful return");
        require(other.begin_upload_batch() && !other.upload_effect_paint_buffer(kind, buffer, 0, source),
                "foreign owner paint upload accepted");
        other.release_effect_paint_buffer(kind, buffer);
        other.cancel_upload_batch();
        require(backend.begin_upload_batch() && !backend.upload_effect_buffer(buffer, 0, source) &&
                    !backend.upload_effect_paint_buffer(kind, buffer, std::numeric_limits<std::size_t>::max(), source),
                "wrong kind or overflow paint upload accepted");
        backend.cancel_upload_batch();
        backend.reset_device();
        require(backend.begin_upload_batch() && !backend.upload_effect_paint_buffer(kind, buffer, 0, source),
                "old epoch paint upload accepted");
        backend.cancel_upload_batch();
    }
    Fixture f;
    static_cast<void>(f.effects.add(painted()));
    require(f.sync(), "foreign attachment fixture failed");
    RecordingRenderer other;
    require(!other.attach_scene(f.resources.attach(f.order)), "foreign paint attachment accepted");
}

void preflight_precedes_begin() {
    for (int mode = 0; mode < 10; ++mode) {
        auto caps = recording_scene_capabilities();
        auto value = painted(Color{1, 0, 0, 0.5F}, true);
        if (mode == 0) {
            caps.effect_paints = false;
        } else if (mode == 1) {
            caps.effect_arc_caps = false;
            value = make_arc_effect({20, 20}, 10, 4, 0, 90, ArcCap::Butt, Color{1, 0, 0});
        } else if (mode == 2) {
            caps.effect_paint_abi_version = 2;
        } else if (mode == 3) {
            caps.effect_linear_fields = false;
        } else if (mode == 4) {
            caps.effect_conic_fields = false;
            value.material.paint->foreground =
                EffectPaintField{EffectGradientRamp{{0, Color{1, 0, 0}}, {1, Color{0, 0, 1}}},
                                 EffectConicDomain{{20, 20}, 0, 2 * std::numbers::pi_v<float>}};
        } else if (mode == 5) {
            caps.effect_underlays = false;
        } else if (mode == 6) {
            std::vector<EffectGradientStop> stops(257, {0, Color{1, 0, 0}});
            value.material.paint->foreground =
                EffectPaintField{EffectGradientRamp{stops}, EffectLinearDomain{{10, 10}, {30, 10}}};
            caps.maximum_buffer_bytes = 512; // base fits; the rounded stop capacity does not.
        } else if (mode == 7) {
            value.geometry.translation = {1e20F, 0};
        } else {
            value.material.paint->foreground =
                EffectPaintField{EffectGradientRamp{{0, Color{1, 0, 0}}, {1, Color{0, 0, 1}}},
                                 EffectLinearDomain{{0, 0}, {mode == 8 ? 1e-30F : 1e30F, 0}}};
        }
        Fixture f{caps};
        require(f.sync(), "preflight priming failed");
        value.material.visible = false;
        const auto id = f.effects.add(value);
        f.backend.fail_next(RecordingFailure::begin);
        const auto uploads = f.backend.counters().uploads;
        const auto cancels = f.backend.counters().cancels;
        const auto live = f.backend.live_resources();
        rejects([&] { static_cast<void>(f.sync()); });
        require(f.backend.counters().uploads == uploads && f.backend.counters().cancels == cancels &&
                    f.backend.live_resources() == live,
                "semantic/limit/domain rejection occurred after begin");
        require(f.effects.remove(id), "preflight rejected source did not retire");
        require(!f.sync() && f.sync(), "preflight consumed pending begin failure or broke retry");
    }
}

void supported_capability_subsets_and_actual_draw_bytes() {
    auto caps = recording_scene_capabilities();
    caps.effect_paints = false;
    caps.effect_linear_fields = false;
    caps.effect_conic_fields = false;
    caps.effect_underlays = false;
    Fixture cap_only{caps};
    static_cast<void>(cap_only.effects.add(make_arc_effect({20, 20}, 10, 4, 0, 90, ArcCap::Butt, Color{1, 0, 0})));
    require(cap_only.sync(), "cap-only support incorrectly required field/paint support");
    caps = recording_scene_capabilities();
    caps.effect_arc_caps = false;
    caps.effect_conic_fields = false;
    caps.effect_underlays = false;
    Fixture linear_only{caps};
    static_cast<void>(linear_only.effects.add(painted()));
    linear_only.order.append_command({SceneDrawKind::rounded_effect, 0, 1, invalid_glyph_atlas_page});
    require(linear_only.sync(), "single Linear support incorrectly required conic/underlay/cap support");
    const auto binding = linear_only.resources.effects().draw_binding(0, 1);
    const EffectPaintGpuStop replacement{{0, 0, 0, 0}, {0, 0.5F, 0, 0.5F}};
    require(linear_only.backend.begin_upload_batch() &&
                linear_only.backend.upload_effect_paint_buffer(EffectPaintBufferKind::stop, binding.stop, 0,
                                                               std::as_bytes(std::span{&replacement, 1})) &&
                linear_only.backend.finish_upload_batch(),
            "actual draw byte fixture upload failed");
    linear_only.draw();
    require(std::ranges::equal(std::span{linear_only.backend.draws()[0].effect_stop_bytes}.first(sizeof(replacement)),
                               std::as_bytes(std::span{&replacement, 1})),
            "Recording used CPU staging instead of actual consumed paint bytes");
    const EffectPaintGpuLink invalid{effect_paint_no_field - 1, effect_paint_no_field, 0, 0};
    require(linear_only.backend.begin_upload_batch() &&
                linear_only.backend.upload_effect_paint_buffer(EffectPaintBufferKind::link, binding.link, 0,
                                                               std::as_bytes(std::span{&invalid, 1})) &&
                linear_only.backend.finish_upload_batch(),
            "invalid actual link fixture upload failed");
    require(linear_only.backend.submit_frame({}) == runtime::FrameSubmissionResult::failed &&
                linear_only.backend.draws().empty(),
            "Recording did not validate actual draw field indices");
}

void growth_create_failures_preserve_old_resources() {
    for (const auto failure : {RecordingFailure::create, RecordingFailure::create_exception}) {
        for (std::size_t table = 0; table < 4; ++table) {
            Fixture f;
            static_cast<void>(f.effects.add(painted()));
            require(f.sync(), "growth failure initial fixture failed");
            const auto live = f.backend.live_resources();
            static_cast<void>(f.effects.add(painted(Color{0, 1, 0}, true)));
            static_cast<void>(f.effects.add(painted(Color{0, 0, 1}, true)));
            f.backend.fail_next(failure, table);
            rejects([&] { static_cast<void>(f.sync()); });
            require(f.backend.live_resources() == live && f.backend.counters().rejected_releases == 0,
                    "growth create failure released old resource or leaked new capacity");
            f.effects.clear_dirty_ranges();
            require(f.sync(), "growth failure retry failed without new dirty");
            projection(f);
        }
    }
}

void color_picker_solid_field_and_paint_share_transaction() {
    Fixture f;
    auto source = sampled_color_field(
        {10, 10, 40, 20}, {}, 8, 4, [](float, float) { return Color{0.2F, 0.4F, 0.6F, 0.5F}; }, {1, {0, 0, 100, 100}});
    source.push_back(painted());
    static_cast<void>(f.effects.add_batch(source));
    f.order.append_command(
        {SceneDrawKind::rounded_effect, 0, static_cast<std::uint32_t>(source.size()), invalid_glyph_atlas_page});
    require(f.sync(), "ColorPicker solidEffects and paint shared transaction failed");
    projection(f);
    f.draw();
    require(f.backend.draws().size() == 1 &&
                f.backend.draws()[0].effect_link_bytes.size() == source.size() * sizeof(EffectPaintGpuLink) &&
                f.backend.draws()[0].effect_field_indices == std::vector<std::uint32_t>{0},
            "solidEffects/paint shared draw lost link correspondence or created fields for solid tiles");
}
} // namespace

int main() {
    try {
        mixed_nonzero_draws_and_updates();
        remap_hidden_cull_delete_and_slot_reuse();
        failures_retry_without_new_dirty();
        later_image_failure_retries_effects();
        old_solid_cap_placeholders_epoch_and_retire();
        raw_owned_bytes_foreign_owner_and_kind();
        preflight_precedes_begin();
        supported_capability_subsets_and_actual_draw_bytes();
        growth_create_failures_preserve_old_resources();
        color_picker_solid_field_and_paint_share_transaction();
    } catch (const std::exception& error) {
        std::cerr << error.what() << '\n';
        return 1;
    }
}
