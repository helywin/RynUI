#include "renderer/common/scene_resources.hpp"
#include "renderer/recording/recording_renderer.hpp"
#include "support/scaling_benchmark.hpp"

#include <array>
#include <cstring>
#include <iostream>
#include <sstream>
#include <string>
#include <vector>

namespace {

using namespace ryn;
using namespace ryn::detail;
using namespace ryn_test::scaling;

struct UploadRange final {
    std::size_t offset{};
    std::size_t bytes{};
};

// Observe actual callbacks without changing Recording's byte ownership or transaction implementation.
class RangeRecorder final : public SceneBackend {
public:
    RecordingRenderer recording;
    std::vector<UploadRange> uploads;

    SceneBackendCapabilities capabilities() const noexcept override {
        return recording.capabilities();
    }

    std::uint64_t device_epoch() const noexcept override {
        return recording.device_epoch();
    }

    bool begin_upload_batch() override {
        uploads.clear();
        return recording.begin_upload_batch();
    }

    bool finish_upload_batch() override {
        return recording.finish_upload_batch();
    }

    void cancel_upload_batch() noexcept override {
        recording.cancel_upload_batch();
    }

    void* create_vertex_buffer(std::size_t size) override {
        return recording.create_vertex_buffer(size);
    }

    void release_buffer(void* handle) noexcept override {
        recording.release_buffer(handle);
    }

    bool upload(void* handle, std::size_t offset, std::span<const std::byte> bytes) override {
        return recording.upload(handle, offset, bytes);
    }

    void* create_glyph_sampler() override {
        return recording.create_glyph_sampler();
    }

    void* create_glyph_texture(std::uint32_t width, std::uint32_t height) override {
        return recording.create_glyph_texture(width, height);
    }

    void* create_glyph_buffer(std::size_t size) override {
        return recording.create_glyph_buffer(size);
    }

    bool upload_glyph_texture(void* handle, const GlyphTextureUpload& upload) override {
        return recording.upload_glyph_texture(handle, upload);
    }

    bool upload_glyph_buffer(void* handle, std::size_t offset, std::span<const std::byte> bytes) override {
        return recording.upload_glyph_buffer(handle, offset, bytes);
    }

    void release_glyph_buffer(void* handle) noexcept override {
        recording.release_glyph_buffer(handle);
    }

    void release_glyph_texture(void* handle) noexcept override {
        recording.release_glyph_texture(handle);
    }

    void release_glyph_sampler(void* handle) noexcept override {
        recording.release_glyph_sampler(handle);
    }

    void* create_effect_buffer(std::size_t size) override {
        return recording.create_effect_buffer(size);
    }

    bool upload_effect_buffer(void* handle, std::size_t offset, std::span<const std::byte> bytes) override {
        uploads.push_back({offset, bytes.size()});
        return recording.upload_effect_buffer(handle, offset, bytes);
    }

    void release_effect_buffer(void* handle) noexcept override {
        recording.release_effect_buffer(handle);
    }

    const char* last_error() const noexcept override {
        return recording.last_error();
    }

    const char* glyph_gpu_error() const noexcept override {
        return recording.glyph_gpu_error();
    }

    const char* effect_gpu_error() const noexcept override {
        return recording.effect_gpu_error();
    }

    void draw_quad(std::uint32_t, std::uint32_t) override {
        throw std::logic_error("range benchmark must not draw");
    }

    void draw_glyph(std::uint32_t, std::uint32_t, std::uint32_t) override {
        throw std::logic_error("range benchmark must not draw");
    }

    void draw_rounded_effect(std::uint32_t, std::uint32_t) override {
        throw std::logic_error("range benchmark must not draw");
    }

    runtime::FrameSubmissionResult submit_frame(animation::AnimationTime) override {
        throw std::logic_error("range benchmark must not present");
    }
};

enum class Mode { material, geometry, mixed, overlap };

const char* name(Mode mode) {
    switch (mode) {
    case Mode::material:
        return "material";
    case Mode::geometry:
        return "geometry";
    case Mode::mixed:
        return "mixed";
    case Mode::overlap:
        return "overlap";
    }
    throw std::logic_error("unknown mode");
}

bool changes_geometry(Mode mode, std::size_t rank) {
    return mode == Mode::geometry || mode == Mode::overlap || (mode == Mode::mixed && rank % 2 == 0);
}

bool changes_material(Mode mode, std::size_t rank) {
    return mode == Mode::material || mode == Mode::overlap || (mode == Mode::mixed && rank % 2 != 0);
}

class Fixture final {
public:
    Fixture(std::size_t count, std::size_t range_count) : resources(backend) {
        require(range_count > 1 && range_count * 2 <= count, "ranges must remain separated by untouched instances");
        backend.uploads.reserve(range_count);
        initial.reserve(count);
        for (std::size_t index = 0; index < count; ++index) {
            const float x = 16.0F + static_cast<float>(index % 100) * 16.0F;
            const float y = 16.0F + static_cast<float>(index / 100) * 16.0F;
            initial.push_back(graphics::make_shadow_effect(
                {{x, y, 8, 8}, 2}, {ShadowKind::outer, {}, 2, 0, Color::rgba8(20, 30, 40, 180)}));
        }
        ids = effects.add_batch(initial);
        for (std::size_t rank = 0; rank < range_count; ++rank) {
            selected.push_back(rank * count / range_count);
        }
    }

    bool synchronize(SceneUploadTiming* timing = nullptr) {
        return resources.synchronize({nullptr, atlas, glyphs, &effects, metrics}, timing);
    }

    void update(Mode mode, bool alternate) {
        // 73 is coprime to both 256 and 1000; this visits every selected rank once.
        for (std::size_t step = 0; step < selected.size(); ++step) {
            const auto rank = (step * 73 + 19) % selected.size();
            const auto index = selected[rank];
            if (changes_geometry(mode, rank)) {
                auto geometry = effects.at(ids[index]).geometry;
                geometry.translation.x = alternate ? 0.25F : 0.75F;
                require(effects.update_geometry(ids[index], geometry), "geometry measurement became an equal update");
            }
            if (changes_material(mode, rank)) {
                auto material = effects.at(ids[index]).material;
                material.opacity = alternate ? 0.5F : 0.75F;
                require(effects.update_material(ids[index], material), "material measurement became an equal update");
            }
        }
    }

    void verify_dirty(Mode mode) const {
        const auto check = [&](std::span<const graphics::RoundedEffectInstanceRange> ranges, bool geometry) {
            std::size_t found{};
            for (std::size_t rank = 0; rank < selected.size(); ++rank) {
                if (geometry ? changes_geometry(mode, rank) : changes_material(mode, rank)) {
                    require(found < ranges.size() && ranges[found].first == selected[rank] && ranges[found].count == 1,
                            "source dirty ranges were merged, missing or not discrete");
                    ++found;
                }
            }
            require(found == ranges.size(), "source dirty ranges include unrelated instances");
        };
        check(effects.geometry_dirty_ranges(), true);
        check(effects.material_dirty_ranges(), false);
    }

    std::uint64_t verify_packed(Mode mode, bool alternate, bool changed) {
        require(effects.live_count() == ids.size() && effects.packed_instances().size() == ids.size(),
                "measurement changed live/packed population");
        const auto packed = resources.effects().instances();
        require(packed.size() == ids.size(), "GPU packed population differs");
        const auto bytes = backend.recording.buffer_bytes(resources.effects().buffer());
        require(bytes.size() >= packed.size_bytes(), "Recording buffer is shorter than live data");
        require(std::ranges::equal(bytes.first(packed.size_bytes()), std::as_bytes(packed)),
                "committed bytes differ from packed CPU staging");
        std::size_t selected_rank{};
        for (std::size_t index = 0; index < ids.size(); ++index) {
            auto expected = initial[index];
            const bool selected_here = selected_rank < selected.size() && selected[selected_rank] == index;
            if (changed && selected_here) {
                if (changes_geometry(mode, selected_rank)) {
                    expected.geometry.translation.x = alternate ? 0.25F : 0.75F;
                }
                if (changes_material(mode, selected_rank)) {
                    expected.material.opacity = alternate ? 0.5F : 0.75F;
                }
            }
            require(effects.at(ids[index]) == expected && effects.packed_instances()[index] == expected &&
                        effects.packed_index(ids[index]) == index,
                    "effect identity, logical value or untouched neighbor changed");
            const auto reference = pack_rounded_effect_instance(expected, metrics);
            require(packed[index] == reference, "packed effect differs from expected logical input");
            // Independently check the two fields changed by the workload, beyond the packing reference.
            require(packed[index].shape_rect[0] == expected.geometry.shape.rect.x + expected.geometry.translation.x &&
                        packed[index].material_params[0] == expected.material.opacity,
                    "changed geometry/material was not represented in packed output");
            selected_rank += selected_here ? 1 : 0;
        }
        require(effects.geometry_dirty_ranges().empty() && effects.material_dirty_ranges().empty(),
                "successful transaction retained source dirty ranges");
        std::uint64_t hash = 14695981039346656037ULL;
        for (const auto byte : bytes.first(packed.size_bytes())) {
            hash = (hash ^ std::to_integer<std::uint8_t>(byte)) * 1099511628211ULL;
        }
        return hash;
    }

    std::size_t verify_uploads(bool full) const {
        require(backend.uploads.size() == (full ? 1 : selected.size()), "unexpected final upload range count");
        std::size_t total{};
        for (std::size_t index = 0; index < backend.uploads.size(); ++index) {
            const auto& upload = backend.uploads[index];
            const auto first = full ? 0 : selected[index];
            const auto count = full ? ids.size() : 1;
            require(upload.offset == first * sizeof(RoundedEffectGpuInstance) &&
                        upload.bytes == count * sizeof(RoundedEffectGpuInstance),
                    "actual upload range differs from changed instances");
            total += upload.bytes;
        }
        return total;
    }

    std::string upload_ranges() const {
        std::ostringstream output;
        for (const auto& upload : backend.uploads) {
            if (output.tellp() > 0) {
                output << ';';
            }
            output << upload.offset / sizeof(RoundedEffectGpuInstance) << ':'
                   << upload.bytes / sizeof(RoundedEffectGpuInstance);
        }
        return output.str();
    }

    RangeRecorder backend;
    graphics::GlyphAtlas atlas;
    graphics::GlyphInstanceStore glyphs;
    graphics::RoundedEffectStore effects;
    std::vector<graphics::RoundedEffectInstance> initial;
    std::vector<graphics::RoundedEffectId> ids;
    std::vector<std::size_t> selected;
    const SceneDeviceMetrics metrics{2048, 2048, 1.0F};
    SceneResources resources;
};

struct Sample final {
    Measurement update;
    Measurement sync;
    std::int64_t effect_sync_ns{};
    std::int64_t commit_ns{};
    std::uint64_t geometry_updates{};
    std::uint64_t material_updates{};
    std::size_t geometry_ranges{};
    std::size_t material_ranges{};
    std::size_t upload_ranges{};
    std::size_t uploaded_bytes{};
    std::uint64_t packed_hash{};
    std::string ranges;
};

Sample synchronize_sample(Fixture& fixture, Mode mode, bool alternate, bool cold) {
    Sample result;
    const auto store_before = fixture.effects.diagnostics();
    if (!cold) {
        result.update = measure([&] {
            fixture.update(mode, alternate);
            // Include first dirty-span normalization even if the store later defers it to the getters.
            result.geometry_ranges = fixture.effects.geometry_dirty_ranges().size();
            result.material_ranges = fixture.effects.material_dirty_ranges().size();
        });
        fixture.verify_dirty(mode);
    } else {
        result.geometry_ranges = fixture.effects.geometry_dirty_ranges().size();
        result.material_ranges = fixture.effects.material_dirty_ranges().size();
    }
    result.geometry_updates = fixture.effects.diagnostics().geometry_updates - store_before.geometry_updates;
    result.material_updates = fixture.effects.diagnostics().material_updates - store_before.material_updates;
    const auto gpu_before = fixture.resources.effects().counters();
    const auto recording_before = fixture.backend.recording.counters();
    SceneUploadTiming timing;
    result.sync = measure([&] { require(fixture.synchronize(&timing), "SceneResources transaction failed"); });
    result.effect_sync_ns = nanoseconds(timing.effects - timing.glyphs);
    result.commit_ns = nanoseconds(timing.committed - timing.effects);
    result.uploaded_bytes = fixture.verify_uploads(cold);
    result.upload_ranges = fixture.backend.uploads.size();
    result.ranges = fixture.upload_ranges();
    result.packed_hash = fixture.verify_packed(mode, alternate, !cold);
    const auto& gpu_after = fixture.resources.effects().counters();
    const auto& recording_after = fixture.backend.recording.counters();
    require(gpu_after.buffer_uploads - gpu_before.buffer_uploads == result.upload_ranges &&
                gpu_after.uploaded_bytes - gpu_before.uploaded_bytes == result.uploaded_bytes &&
                recording_after.uploads - recording_before.uploads == result.upload_ranges &&
                recording_after.uploaded_bytes - recording_before.uploaded_bytes == result.uploaded_bytes &&
                recording_after.commits == recording_before.commits + 1,
            "resource/Recording counters disagree with observed uploads");
    if (!cold) {
        require(gpu_after.full_uploads == gpu_before.full_uploads &&
                    gpu_after.buffer_reallocations == gpu_before.buffer_reallocations &&
                    fixture.effects.diagnostics().compactions == store_before.compactions,
                "sparse update unexpectedly rebuilt storage or performed a full upload");
    }
    return result;
}

void emit(std::size_t round, std::size_t count, std::size_t ranges, Mode mode, const char* phase, const char* kind,
          std::size_t index, std::size_t samples, const Sample& value, std::int64_t update_p95, std::int64_t sync_p95,
          std::int64_t effect_p95, std::int64_t commit_p95) {
    std::cout << "1," << round << ',' << count << ',' << ranges << ',' << name(mode) << ',' << phase << ',' << kind
              << ',' << index << ',' << samples << ',' << value.geometry_updates << ',' << value.material_updates << ','
              << value.geometry_ranges << ',' << value.material_ranges << ',' << value.upload_ranges << ','
              << value.uploaded_bytes << ',' << value.packed_hash << ',' << value.update.elapsed_ns << ',' << update_p95
              << ',' << value.sync.elapsed_ns << ',' << sync_p95 << ',' << value.effect_sync_ns << ',' << effect_p95
              << ',' << value.commit_ns << ',' << commit_p95 << ','
              << static_cast<double>(value.sync.elapsed_ns) / static_cast<double>(samples) << ','
              << value.update.allocations << ',' << value.update.requested_bytes << ',' << value.update.largest_request
              << ',' << value.sync.allocations << ',' << value.sync.requested_bytes << ',' << value.sync.largest_request
              << ",,," << value.ranges << '\n';
}

void emit_samples(std::size_t round, std::size_t count, std::size_t ranges, Mode mode, const char* phase,
                  std::span<const Sample> samples) {
    if (samples.empty()) {
        return;
    }
    Sample total;
    std::vector<std::int64_t> update_times;
    std::vector<std::int64_t> sync_times;
    std::vector<std::int64_t> effect_times;
    std::vector<std::int64_t> commit_times;
    for (std::size_t index = 0; index < samples.size(); ++index) {
        const auto& value = samples[index];
        emit(round, count, ranges, mode, phase, "sample", index + 1, 1, value, value.update.elapsed_ns,
             value.sync.elapsed_ns, value.effect_sync_ns, value.commit_ns);
        accumulate(total.update, value.update);
        accumulate(total.sync, value.sync);
        total.effect_sync_ns += value.effect_sync_ns;
        total.commit_ns += value.commit_ns;
        total.geometry_updates += value.geometry_updates;
        total.material_updates += value.material_updates;
        total.geometry_ranges += value.geometry_ranges;
        total.material_ranges += value.material_ranges;
        total.upload_ranges += value.upload_ranges;
        total.uploaded_bytes += value.uploaded_bytes;
        total.packed_hash = value.packed_hash;
        update_times.push_back(value.update.elapsed_ns);
        sync_times.push_back(value.sync.elapsed_ns);
        effect_times.push_back(value.effect_sync_ns);
        commit_times.push_back(value.commit_ns);
    }
    emit(round, count, ranges, mode, phase, "summary", 0, samples.size(), total, p95(std::move(update_times)),
         p95(std::move(sync_times)), p95(std::move(effect_times)), p95(std::move(commit_times)));
}

void verify_retry_and_epoch() {
    Fixture fixture(1000, 256);
    require(fixture.synchronize(), "retry fixture initial sync failed");
    graphics::OrderedScene scene;
    bool alternate = true;
    for (const auto failure : {RecordingFailure::upload, RecordingFailure::commit}) {
        const auto old_attachment = fixture.resources.attach(scene);
        require(fixture.backend.valid_attachment(old_attachment), "initial attachment is invalid");
        const auto old = fixture.backend.recording.buffer_bytes(fixture.resources.effects().buffer());
        const std::vector<std::byte> old_bytes(old.begin(), old.end());
        fixture.update(Mode::overlap, alternate);
        fixture.verify_dirty(Mode::overlap);
        fixture.backend.recording.fail_next(failure, failure == RecordingFailure::upload ? 3 : 0);
        bool failed{};
        try {
            failed = !fixture.synchronize();
        } catch (const std::runtime_error&) {
            failed = true;
        }
        require(failed && !fixture.backend.valid_attachment(old_attachment) &&
                    !fixture.backend.valid_attachment(fixture.resources.attach(scene)),
                "failed transaction published a usable attachment");
        require(
            std::ranges::equal(old_bytes, fixture.backend.recording.buffer_bytes(fixture.resources.effects().buffer())),
            "cancelled Recording transaction changed committed bytes");
        require(fixture.synchronize(), "failed sparse transaction could not retry");
        static_cast<void>(fixture.verify_uploads(true));
        static_cast<void>(fixture.verify_packed(Mode::overlap, alternate, true));
        alternate = !alternate;
    }
    const auto old_attachment = fixture.resources.attach(scene);
    fixture.backend.recording.reset_device();
    require(!fixture.backend.valid_attachment(old_attachment), "old epoch attachment remained valid");
    require(fixture.synchronize(), "new epoch synchronization failed");
    static_cast<void>(fixture.verify_uploads(true));
    static_cast<void>(fixture.verify_packed(Mode::overlap, !alternate, true));
    require(fixture.backend.valid_attachment(fixture.resources.attach(scene)) &&
                fixture.backend.recording.counters().rejected_releases == 0,
            "new epoch attachment or stale resource release differs");
    fixture.resources.retire();
    require(fixture.backend.recording.live_resources() == 0, "resource retirement leaked Recording objects");
}

void run_case(const Options& config, std::size_t round, std::size_t count, std::size_t ranges, Mode mode) {
    Fixture fixture(count, ranges);
    const auto cold = synchronize_sample(fixture, mode, false, true);
    emit_samples(round, count, ranges, mode, "cold_sync", std::span(&cold, 1));
    bool alternate = true;
    for (const bool hot : {false, true}) {
        const auto iterations = hot ? config.iterations : config.warmup;
        std::vector<Sample> samples(iterations);
        for (auto& sample : samples) {
            sample = synchronize_sample(fixture, mode, alternate, false);
            alternate = !alternate;
        }
        emit_samples(round, count, ranges, mode, hot ? "hot" : "warmup", samples);
    }
}

} // namespace

int main(int argc, char** argv) {
    try {
        const auto config = options(argc, argv);
        if (config.help) {
            std::cout << "--iterations N --warmup N --rounds N --smoke\n"
                         "Defaults: 8 hot iterations, 2 warmup iterations, 1 round. CSV on stdout.\n";
            return 0;
        }
        std::cerr << "effect_range: iterations=" << config.iterations << " warmup=" << config.warmup
                  << " rounds=" << config.rounds << " smoke=" << config.smoke
                  << "; planning_ns/sort_comparisons unavailable; effect_sync includes packing and Recording enqueue\n";
        std::cout << "schema,round,effects,requested_ranges,mode,phase,record,sample,samples,geometry_updates,"
                     "material_updates,geometry_input_ranges,material_input_ranges,upload_ranges,uploaded_bytes,"
                     "packed_hash,update_total_ns,update_p95_ns,sync_total_ns,sync_p95_ns,effect_sync_total_ns,"
                     "effect_sync_p95_ns,commit_total_ns,commit_p95_ns,sync_mean_ns,update_allocations,"
                     "update_requested_bytes,update_largest_request,sync_allocations,sync_requested_bytes,"
                     "sync_largest_request,planning_ns,sort_comparisons,actual_ranges_first_count\n";
        for (std::size_t round = 1; round <= config.rounds; ++round) {
            for (const auto [count, ranges] :
                 std::array<std::array<std::size_t, 2>, 3>{{{1000, 256}, {10000, 256}, {10000, 1000}}}) {
                if (config.smoke && count != 1000) {
                    continue;
                }
                for (const auto mode : {Mode::material, Mode::geometry, Mode::mixed, Mode::overlap}) {
                    run_case(config, round, count, ranges, mode);
                }
            }
        }
        verify_retry_and_epoch();
        std::cerr << "effect_range: packed/range/transaction/retry/epoch checks passed\n";
    } catch (const std::exception& error) {
        ryn_test::allocation::end();
        std::cerr << error.what() << '\n';
        return 1;
    }
    return 0;
}
