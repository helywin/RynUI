#include "renderer/common/scene_resources.hpp"
#include "renderer/recording/recording_renderer.hpp"

#include <algorithm>
#include <bit>
#include <cmath>
#include <cstring>
#include <iostream>
#include <stdexcept>
#include <thread>

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
    throw std::runtime_error("invalid cap operation accepted");
}

RoundedEffectInstance arc(ArcCap cap = ArcCap::Butt) {
    return make_arc_effect({20, 20}, 10, 4, 0, 90, cap, Color{1, 0, 0});
}

RoundedEffectInstance painted_round() {
    auto value = arc(ArcCap::Round);
    value.material.paint = EffectPaint{
        {EffectGradientRamp{{0, Color{1, 0, 0}}, {1, Color{0, 0, 1}}}, EffectLinearDomain{{0, 0}, {40, 0}}}, {}};
    return value;
}

template <class T> void same_bytes(std::span<const std::byte> actual, std::span<const T> expected) {
    const auto bytes = std::as_bytes(expected);
    require(actual.size() >= bytes.size() && std::ranges::equal(actual.first(bytes.size()), bytes),
            "cap projection differs from actual buffer bytes");
}

void raw_bits_break_complete_physical_collision() {
    const SceneDeviceMetrics metrics{8, 8, 1.5F};
    auto first = make_arc_effect({-8388603, 0}, 8388608, 4, 0, 90, ArcCap::Butt, Color{1, 0, 0});
    auto second = make_arc_effect({-8388602, 0}, 8388608, 4, 0, 90, ArcCap::Butt, Color{1, 0, 0});
    const auto base0 = pack_effect_paint_base_instance(first, metrics);
    const auto base1 = pack_effect_paint_base_instance(second, metrics);
    EffectPaintPackedTables t0;
    EffectPaintPackedTables t1;
    pack_effect_paint_tables(std::array{first}, metrics, t0);
    pack_effect_paint_tables(std::array{second}, metrics, t1);
    require(base0 == base1 && t0.links == t1.links && t0.fields == t1.fields && t0.stops == t1.stops,
            "fixture no longer exhibits the complete old 192-byte collision");
    const auto c0 = pack_cap_geometry(first, metrics);
    const auto c1 = pack_cap_geometry(second, metrics);
    require(c0.logical_rect_bits ==
                    std::array{std::bit_cast<std::uint32_t>(-16777211.0F), std::bit_cast<std::uint32_t>(-8388608.0F),
                               std::bit_cast<std::uint32_t>(16777216.0F), std::bit_cast<std::uint32_t>(16777216.0F)} &&
                c1.logical_rect_bits[0] == std::bit_cast<std::uint32_t>(-16777210.0F) &&
                c0.stroke_scale_bits ==
                    std::array{std::bit_cast<std::uint32_t>(4.0F), std::bit_cast<std::uint32_t>(1.5F), 0U, 0U} &&
                c0 != c1,
            "raw factors lost the logical inputs or occupied reserved bits");
    // Independent quarter-annulus geometry: at this pixel C0 is inside its
    // stroke by > half AA; C1 is outside by >= half AA. No packed oracle equality.
    const auto p0 = effect_paint_gpu_fragment_reference({5.5F, 0.5F}, base0, t0.links[0], t0.fields, t0.stops, &c0);
    const auto p1 = effect_paint_gpu_fragment_reference({5.5F, 0.5F}, base1, t1.links[0], t1.fields, t1.stops, &c1);
    require(p0[3] == 1 && p1[3] < 0.00001F, "decoded stream did not recover distinct logical coverage");
    rejects([&] {
        static_cast<void>(effect_paint_gpu_fragment_reference({5.5F, 0.5F}, base0, t0.links[0], t0.fields, t0.stops));
    });
    first.geometry.outline_width = std::numeric_limits<float>::denorm_min();
    std::get<RoundedArcPath>(first.geometry.path).sweep_radians = std::numeric_limits<float>::denorm_min();
    const auto tiny = pack_cap_geometry(first, metrics);
    require(tiny.stroke_scale_bits[0] == 1, "positive subnormal stroke was lost before integer vertex fetch");
    validate_cap_geometry(tiny);
    require(pack_cap_geometry(arc(ArcCap::Round), metrics) == CapGeometryGpuV1{}, "Round placeholder is nonzero");
    auto bad = tiny;
    bad.stroke_scale_bits[2] = 1;
    rejects([&] { validate_cap_geometry(bad); });
}

void wide_bounds_enclose_independent_visible_neighbors() {
    for (const bool transpose : {false, true}) {
        auto value = make_arc_effect({-99999968, 0}, 100000000, 4, 0, 90, ArcCap::Butt, Color{1, 0, 0});
        if (transpose) {
            std::swap(value.geometry.shape.rect.x, value.geometry.shape.rect.y);
        }
        const runtime::Rect clip{0, 0, 96, 96};
        const auto endpoints = nonround_arc_draw_bounds(value, clip);
        const auto public_bounds = rounded_effect_bounds(value);
        const auto base = pack_effect_paint_base_instance(value, {96, 96, 1});
        const auto draw_right = (double(base.clip_rect[0]) + base.clip_rect[2] + 1) * 48;
        const auto draw_bottom = (1 - double(base.clip_rect[1]) - base.clip_rect[3]) * 48;
        for (const auto coordinate : {30.5F, 31.5F, 32.5F, 33.5F}) {
            const runtime::Point point =
                transpose ? runtime::Point{31.5F, coordinate} : runtime::Point{coordinate, 31.5F};
            require(rounded_effect_coverage(point, value) > 0.9999F, "quarter-annulus independent interior changed");
            require(point.x >= endpoints.left && point.x < endpoints.right && point.y >= endpoints.top &&
                        point.y < endpoints.bottom && point.x < draw_right && point.y < draw_bottom &&
                        point.x < public_bounds.x + public_bounds.width &&
                        point.y < public_bounds.y + public_bounds.height,
                    "bounds or packed draw lost an independently covered outer neighbor");
        }
        RoundedEffectStore store;
        const auto id = store.add(value);
        const runtime::Rect narrow = transpose ? runtime::Rect{30, 32, 3, 2} : runtime::Rect{32, 30, 2, 3};
        require(store.compact(narrow) && store.packed_index(id) == 0, "store culled visible outer stroke");
        value.geometry.translation = {0.25F, 0.25F};
        require(store.update_geometry(id, value.geometry) && store.packed_index(id) == 0 && !store.compact(narrow),
                "geometry update used lossy LTWH cull path");
        value.geometry.ancestor_clip = EffectClip{1, {0, 0, 32, 32}};
        const auto clipped = pack_effect_paint_base_instance(value, {96, 96, 1});
        require(clipped.clip_bounds == std::array<float, 4>{0, 0, 32, 32}, "bounds inflated actual hard clip");
    }
    const auto square = make_arc_effect({0, 0}, 1, 20, 45, 90, ArcCap::Square, Color{1, 0, 0});
    const auto bounds = nonround_arc_draw_bounds(square, {-32, -32, 64, 64});
    require(bounds.right > 21 / std::sqrt(2.0) && !bounds.empty(), "Square corner envelope is not conservative");
    const auto huge =
        make_arc_effect({-std::ldexp(1.0F, 126), 0}, std::ldexp(1.0F, 126), 4, 0, 90, ArcCap::Butt, Color{1, 0, 0});
    require(!nonround_arc_draw_bounds(huge, {0, 0, 16, 16}).empty(), "wide finite geometry overflowed before clip");
    require(nonround_arc_draw_bounds(square, {0, 0, 0, 16}).empty(), "zero-width clip acquired coverage");
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

    void projection(SceneDeviceMetrics metrics = {100, 100, 1}) {
        std::vector<CapGeometryGpuV1> expected;
        for (const auto& value : effects.packed_instances()) {
            expected.push_back(pack_cap_geometry(value, metrics));
        }
        const auto binding = resources.effects().draw_binding(0, static_cast<std::uint32_t>(expected.size()));
        same_bytes(backend.buffer_bytes(binding.cap_geometry), std::span<const CapGeometryGpuV1>{expected});
        require(backend.buffer_bytes(binding.cap_geometry).size() ==
                    resources.effects().cap_capacity() * sizeof(CapGeometryGpuV1),
                "cap allocation capacity differs from preflight");
    }

    void draw() {
        require(backend.attach_scene(resources.attach(order)) &&
                    backend.submit_frame({}) == runtime::FrameSubmissionResult::submitted,
                "cap attachment/draw failed");
    }
};

void mixed_stream_remap_actual_bytes_and_lifetime() {
    Fixture f;
    const auto skipped = f.effects.add(arc(ArcCap::Round));
    const auto a = f.effects.add(arc());
    const auto b = f.effects.add(painted_round());
    require(f.effects.remove(skipped), "slot removal failed");
    const auto reused = f.effects.add(arc(ArcCap::Square));
    require(reused.index == skipped.index && f.sync() && f.effects.packed_index(reused) == 2,
            "cap slot reused store index");
    f.projection();
    f.order.append_command({SceneDrawKind::rounded_effect, 1, 2, invalid_glyph_atlas_page});
    f.draw();
    const auto& draw = f.backend.draws()[0];
    require(draw.effect_binding.base_byte_offset == 112 && draw.effect_binding.link_byte_offset == 16 &&
                draw.effect_binding.cap_byte_offset == 32 && draw.cap_geometry_bytes.size() == 64,
            "nonzero first cap stream offsets disagree");
    CapGeometryGpuV1 zero;
    std::memcpy(&zero, draw.cap_geometry_bytes.data(), sizeof(zero));
    require(zero == CapGeometryGpuV1{}, "Round paint row acquired cap geometry");
    const auto binding = f.resources.effects().draw_binding(0, 3);
    auto replacement = pack_cap_geometry(arc(), {100, 100, 1});
    replacement.stroke_scale_bits[0] = std::bit_cast<std::uint32_t>(8.0F);
    require(
        f.backend.begin_upload_batch() &&
            f.backend.upload_cap_geometry_buffer(binding.cap_geometry, 64, std::as_bytes(std::span{&replacement, 1})) &&
            f.backend.finish_upload_batch(),
        "actual cap replacement upload failed");
    f.draw();
    same_bytes(std::span<const std::byte>{f.backend.draws()[0].cap_geometry_bytes}.subspan(32),
               std::span<const CapGeometryGpuV1>{&replacement, 1});
    auto hidden = f.effects.at(a).material;
    hidden.visible = false;
    require(f.effects.update_material(a, hidden) && f.sync() && f.effects.packed_index(reused) == 1,
            "hidden cap remap failed");
    f.projection();
    auto culled = f.effects.at(reused).geometry;
    culled.translation = {1000, 0};
    require(f.effects.update_geometry(reused, culled) && f.sync() &&
                !f.resources.effects().draw_binding(0, 1).cap_geometry && f.resources.effects().cap_capacity() == 0,
            "Round paint kept third stream after last visible cap cull");
    require(f.effects.update_geometry(reused, arc().geometry) && f.sync(), "cap restore failed");
    f.projection();
    const auto uploads = f.backend.counters().uploads;
    require(f.sync() && f.backend.counters().uploads == uploads, "idle cap scene uploaded bytes");
    require(f.effects.remove(b) && f.effects.remove(a) && f.sync(), "cap scene shrink failed");
    const auto old = f.resources.attach(f.order);
    auto caps = recording_scene_capabilities();
    caps.maximum_buffer_bytes = 128;
    f.backend.reset_device(caps);
    require(!f.backend.valid_attachment(old) && f.sync() && f.resources.effects().cap_capacity() == 1 &&
                f.backend.counters().rejected_releases == 0,
            "cap epoch inherited capacity or released stale handle");
    f.projection();
    f.resources.retire();
    require(f.backend.live_resources() == 0, "retire leaked cap buffer");
    require(f.sync(), "retired cap resources could not rebuild");
}

void fractional_scale_cull_preserves_one_physical_pixel_aa() {
    Fixture f;
    auto value = make_arc_effect({-0.1F, 18}, 4, 1, 0, 90, ArcCap::Butt, Color{1, 0, 0});
    value.geometry.ancestor_clip = EffectClip{1, {6, 0, 26, 32}};
    const auto id = f.effects.add(value);
    const SceneDeviceMetrics metrics{8, 8, 0.25F};
    require(f.sync(metrics) && f.effects.packed_index(id) == 0,
            "sub-unit DPI fixed logical AA guard culled visible coverage");
    f.projection(metrics);
    const auto& resources = f.resources.effects();
    const auto& tables = resources.paint_tables();
    const auto color = effect_paint_gpu_fragment_reference({1.5F, 4.5F}, resources.instances()[0], tables.links[0],
                                                           tables.fields, tables.stops, &resources.cap_geometry()[0]);
    require(std::abs(color[3] - 0.028F) < 0.0001F, "sub-unit DPI AA is not one physical pixel");
    value.geometry.translation.y = 0.25F;
    require(f.effects.update_geometry(id, value.geometry) && f.effects.packed_index(id) == 0 && f.sync(metrics),
            "geometry update lost the active compact AA guard");
}

void all_five_buffers_retry_atomically() {
    for (const auto failure : {RecordingFailure::create, RecordingFailure::create_exception, RecordingFailure::upload,
                               RecordingFailure::upload_exception}) {
        for (std::size_t stage = 0; stage < 5; ++stage) {
            Fixture f;
            require(f.sync(), "failure priming failed");
            const auto live = f.backend.live_resources();
            static_cast<void>(f.effects.add(arc()));
            f.backend.fail_next(failure, stage);
            rejects([&] { static_cast<void>(f.sync()); });
            require(f.backend.live_resources() == live && !f.backend.valid_attachment(f.resources.attach(f.order)),
                    "failed cap transaction leaked candidate or retained attachment");
            f.effects.clear_dirty_ranges();
            const auto uploads = f.backend.counters().uploads;
            require(f.sync() && f.backend.counters().uploads == uploads + 5,
                    "cap retry skipped a buffer without new dirty");
            f.projection();
        }
    }
    for (const auto failure : {RecordingFailure::begin, RecordingFailure::commit, RecordingFailure::upload,
                               RecordingFailure::upload_exception}) {
        Fixture f;
        static_cast<void>(f.effects.add(arc()));
        require(f.sync(), "existing cap fixture failed");
        const auto old = f.resources.attach(f.order);
        f.backend.fail_next(
            failure, failure == RecordingFailure::upload || failure == RecordingFailure::upload_exception ? 4 : 0);
        bool failed{};
        try {
            failed = !f.sync({200, 200, 2});
        } catch (const std::exception&) {
            failed = true;
        }
        require(failed && !f.backend.valid_attachment(old), "existing cap failure retained old attachment");
        f.effects.clear_dirty_ranges();
        const auto uploads = f.backend.counters().uploads;
        require(f.sync() && f.backend.counters().uploads == uploads + 5,
                "cap retry to original metrics skipped stream");
        f.projection();
    }
    Fixture f;
    static_cast<void>(f.effects.add(arc()));
    require(f.sync(), "later image fixture failed");
    const std::array pixel{std::byte{255}, std::byte{0}, std::byte{0}, std::byte{255}};
    static_cast<void>(f.images.create({ImagePixels::copy(1, 1, 4, pixel), {0, 0, 4, 4}, {0, 0, 100, 100}, {0, 0}}));
    f.backend.fail_next(RecordingFailure::upload, 5);
    rejects([&] { static_cast<void>(f.sync({200, 200, 2})); });
    const auto count = f.resources.effects().counters().buffer_uploads;
    require(f.effects.geometry_dirty_ranges().empty() && f.sync() &&
                f.resources.effects().counters().buffer_uploads == count + 5,
            "later Image failure omitted cap invalidation");
    f.projection();
}

void capability_and_owned_raw_buffer_contract() {
    for (const auto version : {0U, 2U}) {
        auto caps = recording_scene_capabilities();
        caps.cap_geometry_abi_version = version;
        Fixture f{caps};
        require(f.sync(), "ABI priming failed");
        auto value = arc();
        value.material.visible = false;
        const auto id = f.effects.add(value);
        f.backend.fail_next(RecordingFailure::begin);
        rejects([&] { static_cast<void>(f.sync()); });
        require(f.effects.remove(id) && !f.sync() && f.sync(), "cap ABI preflight did not precede begin");
        static_cast<void>(f.effects.add(painted_round()));
        require(f.sync() && !f.resources.effects().draw_binding(0, 1).cap_geometry,
                "Round paint incorrectly required Cap Geometry ABI");
    }
    RecordingRenderer backend;
    RecordingRenderer other;
    auto* buffer = backend.create_cap_geometry_buffer(32);
    std::array<std::byte, 32> source;
    source.fill(std::byte{42});
    const auto expected = source;
    require(backend.begin_upload_batch() && backend.upload_cap_geometry_buffer(buffer, 0, source),
            "raw cap upload failed");
    source.fill(std::byte{99});
    require(backend.finish_upload_batch() && std::ranges::equal(backend.buffer_bytes(buffer), expected),
            "cap source not owned");
    require(other.begin_upload_batch() && !other.upload_cap_geometry_buffer(buffer, 0, source),
            "foreign cap owner accepted");
    other.cancel_upload_batch();
    require(backend.begin_upload_batch() && !backend.upload_effect_buffer(buffer, 0, source) &&
                !backend.upload_cap_geometry_buffer(buffer, 1, source),
            "wrong kind or cap range accepted");
    backend.cancel_upload_batch();
    bool owner_rejected{};
    std::thread thread([&] {
        try {
            static_cast<void>(backend.upload_cap_geometry_buffer(buffer, 0, source));
        } catch (const std::logic_error&) {
            owner_rejected = true;
        }
    });
    thread.join();
    require(owner_rejected, "cap upload accepted off-thread");
    backend.release_cap_geometry_buffer(buffer);
    require(backend.begin_upload_batch() && !backend.upload_cap_geometry_buffer(buffer, 0, source),
            "tombstone cap accepted");
    backend.cancel_upload_batch();
    buffer = backend.create_cap_geometry_buffer(32);
    backend.reset_device();
    require(backend.begin_upload_batch() && !backend.upload_cap_geometry_buffer(buffer, 0, source),
            "old epoch cap accepted");
    backend.cancel_upload_batch();
}
} // namespace

int main() {
    try {
        raw_bits_break_complete_physical_collision();
        wide_bounds_enclose_independent_visible_neighbors();
        mixed_stream_remap_actual_bytes_and_lifetime();
        fractional_scale_cull_preserves_one_physical_pixel_aa();
        all_five_buffers_retry_atomically();
        capability_and_owned_raw_buffer_contract();
    } catch (const std::exception& error) {
        std::cerr << error.what() << '\n';
        return 1;
    }
}
