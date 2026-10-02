#include "renderer/common/rounded_effect_gpu_resources.hpp"

#include <array>
#include <algorithm>
#include <cstddef>
#include <cstdint>
#include <iostream>
#include <map>
#include <stdexcept>
#include <string>
#include <utility>
#include <vector>

namespace {

void require(bool condition, const char* message) {
    if (!condition) {
        throw std::runtime_error(message);
    }
}

struct Upload final {
    std::uintptr_t buffer{};
    std::size_t offset{};
    std::vector<std::byte> bytes;
};

class RecordingApi final : public ryn::detail::RoundedEffectGpuApi {
public:
    ryn::detail::RoundedEffectGpuBufferHandle create_effect_buffer(std::size_t size) override {
        events.emplace_back("create");
        requested_sizes.push_back(size);
        if (fail_create) {
            return nullptr;
        }
        const auto handle = ++next_handle;
        buffers.emplace(handle, std::vector<std::byte>(size));
        ++live_buffers;
        return reinterpret_cast<void*>(handle);
    }

    bool upload_effect_buffer(ryn::detail::RoundedEffectGpuBufferHandle buffer, std::size_t offset,
                              std::span<const std::byte> bytes) override {
        events.emplace_back("upload");
        uploads.push_back({
            reinterpret_cast<std::uintptr_t>(buffer),
            offset,
            {bytes.begin(), bytes.end()},
        });
        const auto found = buffers.find(reinterpret_cast<std::uintptr_t>(buffer));
        if (found == buffers.end() || offset > found->second.size() || bytes.size() > found->second.size() - offset) {
            return false;
        }
        std::copy(bytes.begin(), bytes.end(), found->second.begin() + offset);
        if (throw_upload) {
            throw std::runtime_error("injected write-then-throw");
        }
        return !fail_upload;
    }

    void release_effect_buffer(ryn::detail::RoundedEffectGpuBufferHandle buffer) noexcept override {
        events.emplace_back("release");
        if (!buffers.erase(reinterpret_cast<std::uintptr_t>(buffer))) {
            released_stale = true;
        }
        live_buffers = buffers.size();
    }

    void reset_device() {
        buffers.clear();
        live_buffers = 0;
    }

    const char* effect_gpu_error() const noexcept override {
        return "injected rounded-effect GPU failure";
    }

    bool fail_create{};
    bool fail_upload{};
    bool throw_upload{};
    bool released_stale{};
    std::uintptr_t next_handle{};
    std::size_t live_buffers{};
    std::vector<std::size_t> requested_sizes;
    std::vector<Upload> uploads;
    std::vector<std::string> events;
    std::map<std::uintptr_t, std::vector<std::byte>> buffers;
};

ryn::graphics::RoundedEffectInstance effect(float x, std::uint8_t marker) {
    return ryn::graphics::make_shadow_effect(
        {{x, 10.0F, 10.0F, 10.0F}, 2.0F},
        {ryn::ShadowKind::outer, {}, 2.0F, 0.0F, ryn::Color::rgba8(marker, marker, marker, 80)});
}

void test_full_partial_metrics_growth_and_idle_uploads() {
    RecordingApi api;
    {
        ryn::detail::RoundedEffectGpuResources resources(api);
        ryn::graphics::RoundedEffectStore store;
        const std::array initial{effect(10.0F, 1), effect(30.0F, 2), effect(50.0F, 3)};
        const auto ids = store.add_batch(initial);
        resources.synchronize(store, {100, 100, 1.0F});
        require(resources.capacity() == 4 && resources.instance_count() == 3 && api.requested_sizes.size() == 1 &&
                    api.requested_sizes.front() == 4 * sizeof(ryn::detail::RoundedEffectGpuInstance) &&
                    api.uploads.size() == 1 && api.uploads.front().offset == 0 &&
                    api.uploads.front().bytes.size() == 3 * sizeof(ryn::detail::RoundedEffectGpuInstance),
                "initial rounded-effect GPU allocation or full upload differs");

        auto material = store.at(ids[1]).material;
        material.opacity = 0.5F;
        require(store.update_material(ids[1], material), "GPU resource fixture material did not change");
        resources.synchronize(store, {100, 100, 1.0F});
        require(api.uploads.size() == 2 && api.uploads.back().offset == sizeof(ryn::detail::RoundedEffectGpuInstance) &&
                    api.uploads.back().bytes.size() == sizeof(ryn::detail::RoundedEffectGpuInstance) &&
                    resources.counters().partial_uploads == 1,
                "material-only effect update did not remain a partial upload");

        resources.synchronize(store, {200, 200, 2.0F});
        require(api.uploads.size() == 3 && api.uploads.back().offset == 0 &&
                    api.uploads.back().bytes.size() == 3 * sizeof(ryn::detail::RoundedEffectGpuInstance) &&
                    resources.instances()[1].shape_rect[0] == 60.0F,
                "display-scale change did not force a full device-geometry upload");
        resources.synchronize(store, {200, 200, 2.0F});
        require(api.uploads.size() == 3 && resources.counters().idle_synchronizations == 1,
                "idle effect synchronization uploaded unchanged data");

        static_cast<void>(store.add(effect(70.0F, 4)));
        static_cast<void>(store.add(effect(80.0F, 5)));
        resources.synchronize(store, {200, 200, 2.0F});
        require(resources.capacity() == 8 && resources.instance_count() == 5 && api.requested_sizes.size() == 2 &&
                    api.live_buffers == 1 && resources.counters().buffer_reallocations == 2,
                "rounded-effect GPU growth did not replace and release its old buffer");
    }
    require(api.live_buffers == 0 && !api.events.empty() && api.events.back() == "release",
            "rounded-effect GPU resource teardown leaked its buffer");
}

void test_zero_effect_and_failure_paths_preserve_dirty_state() {
    RecordingApi empty_api;
    {
        ryn::detail::RoundedEffectGpuResources resources(empty_api);
        ryn::graphics::RoundedEffectStore empty;
        resources.synchronize(empty, {100, 100, 1.0F});
        require(resources.buffer() == nullptr && resources.instance_count() == 0 && empty_api.events.empty() &&
                    resources.counters().zero_effect_synchronizations == 1,
                "zero-effect synchronization created or uploaded a GPU buffer");
    }

    for (const bool fail_create : {true, false}) {
        RecordingApi api;
        api.fail_create = fail_create;
        api.fail_upload = !fail_create;
        ryn::graphics::RoundedEffectStore store;
        static_cast<void>(store.add(effect(10.0F, 1)));
        bool failed = false;
        {
            ryn::detail::RoundedEffectGpuResources resources(api);
            try {
                resources.synchronize(store, {100, 100, 1.0F});
            } catch (const std::runtime_error&) {
                failed = true;
            }
            require(failed && resources.buffer() == nullptr && !store.geometry_dirty_ranges().empty(),
                    "failed effect upload discarded dirty state or published a buffer");
        }
        require(api.live_buffers == 0, "failed rounded-effect GPU setup leaked a candidate buffer");
    }

    RecordingApi api;
    ryn::graphics::RoundedEffectStore store;
    const auto id = store.add(effect(10.0F, 1));
    {
        ryn::detail::RoundedEffectGpuResources resources(api);
        resources.synchronize(store, {100, 100, 1.0F});
        auto material = store.at(id).material;
        material.opacity = 0.5F;
        static_cast<void>(store.update_material(id, material));
        api.fail_upload = true;
        bool failed = false;
        try {
            resources.synchronize(store, {100, 100, 1.0F});
        } catch (const std::runtime_error&) {
            failed = true;
        }
        require(failed && resources.buffer() != nullptr && !store.material_dirty_ranges().empty() &&
                    api.live_buffers == 1,
                "partial upload failure lost the live buffer or retry range");
    }
    require(api.live_buffers == 0, "rounded-effect GPU buffer leaked after partial upload failure");
}

void test_full_retry_after_batch_submit_failure() {
    RecordingApi api;
    ryn::graphics::RoundedEffectStore store;
    const std::array initial{effect(10.0F, 1), effect(30.0F, 2)};
    static_cast<void>(store.add_batch(initial));
    ryn::detail::RoundedEffectGpuResources resources(api);
    resources.synchronize(store, {100, 100, 1.0F});
    require(store.geometry_dirty_ranges().empty() && api.uploads.size() == 1,
            "effect fixture did not clear optimistic upload state");
    resources.invalidate_upload();
    resources.synchronize(store, {100, 100, 1.0F});
    require(api.uploads.size() == 2 && api.uploads.back().offset == 0 &&
                api.uploads.back().bytes.size() == 2 * sizeof(ryn::detail::RoundedEffectGpuInstance) &&
                resources.counters().full_uploads == 2,
            "failed batch did not force a complete effect retry");
}

void test_failed_projection_returns_to_original_metrics_without_cpu_dirty() {
    for (const bool throws : {false, true}) {
        RecordingApi api;
        ryn::graphics::RoundedEffectStore store;
        const auto initial = std::array{effect(10, 1), effect(30, 2)};
        const auto ids = store.add_batch(initial);
        ryn::detail::RoundedEffectGpuResources resources(api);
        resources.synchronize(store, {100, 100, 1});
        const auto handle = reinterpret_cast<std::uintptr_t>(resources.buffer());
        const auto original_bytes = api.buffers.at(handle);
        api.fail_upload = !throws;
        api.throw_upload = throws;
        bool failed = false;
        try {
            resources.synchronize(store, {200, 200, 2});
        } catch (const std::runtime_error&) {
            failed = true;
        }
        require(failed && api.buffers.at(handle) != original_bytes,
                "failure fixture did not contaminate the uploaded projection");
        require(store.geometry_dirty_ranges().empty() && store.material_dirty_ranges().empty(),
                "projection-only update unexpectedly dirtied CPU effects");
        const auto uploads = api.uploads.size();
        api.fail_upload = api.throw_upload = false;
        resources.synchronize(store, {100, 100, 1});
        require(api.uploads.size() == uploads + 1 && api.uploads.back().offset == 0 &&
                    api.uploads.back().bytes.size() == original_bytes.size() &&
                    api.buffers.at(handle) == original_bytes,
                "returning to original metrics skipped complete projection recovery");
        require(store.at(ids[0]) == initial[0] && store.at(ids[1]) == initial[1],
                "projection recovery changed CPU effect identity or material");
    }
}

void test_abandon_recreates_buffer_without_releasing_old_epoch() {
    RecordingApi api;
    ryn::graphics::RoundedEffectStore store;
    const auto initial = std::array{effect(10, 1), effect(30, 2)};
    const auto ids = store.add_batch(initial);
    {
        ryn::detail::RoundedEffectGpuResources resources(api);
        resources.synchronize(store, {100, 100, 1});
        const auto old_handle = resources.buffer();
        const auto staging = resources.instances().data();
        const auto original_bytes = api.buffers.at(reinterpret_cast<std::uintptr_t>(old_handle));
        const auto events = api.events.size();
        api.reset_device();
        resources.abandon_device();
        require(api.events.size() == events && !resources.buffer() && !resources.capacity() &&
                    !resources.instance_count() && resources.instances().empty(),
                "abandon released a stale handle or retained old epoch capacity");
        resources.synchronize(store, {100, 100, 1});
        require(resources.buffer() && resources.buffer() != old_handle && api.requested_sizes.size() == 2 &&
                    api.live_buffers == 1 && resources.instances().data() == staging &&
                    api.buffers.at(reinterpret_cast<std::uintptr_t>(resources.buffer())) == original_bytes,
                "abandoned effects did not recreate/upload a buffer and reuse staging");
        require(store.at(ids[0]) == initial[0] && store.at(ids[1]) == initial[1],
                "device epoch replacement lost CPU effects");
    }
    require(api.live_buffers == 0 && !api.released_stale,
            "effect epoch teardown released a stale handle or leaked the new buffer");
}

void test_fractional_dpi_clip_edges_preserve_draw_indices() {
    RecordingApi api;
    ryn::graphics::RoundedEffectStore store;
    auto at_window_edge = effect(-13.9F, 1);
    auto at_ancestor_edge = effect(20.0F, 2);
    at_ancestor_edge.geometry.ancestor_clip = ryn::graphics::EffectClip{1, {33.9F, 0.0F, 60.0F, 100.0F}};
    const auto ids = store.add_batch(std::array{at_window_edge, at_ancestor_edge, effect(50.0F, 3)});
    ryn::detail::RoundedEffectGpuResources resources(api);
    for (const float scale : {1.0F, 1.25F, 1.5F, 2.0F, 1.0F}) {
        for (const float translation : {0.0F, 0.5F, 1.0F, 0.5F, 0.0F}) {
            for (std::size_t index = 0; index < 2; ++index) {
                auto geometry = store.at(ids[index]).geometry;
                geometry.translation.x = translation;
                static_cast<void>(store.update_geometry(ids[index], geometry));
            }
            resources.synchronize(
                store, {static_cast<std::uint32_t>(100.0F * scale), static_cast<std::uint32_t>(100.0F * scale), scale});
            require(resources.instance_count() == 3 && store.packed_index(ids[0]) == 0 &&
                        store.packed_index(ids[1]) == 1 && store.packed_index(ids[2]) == 2 &&
                        resources.instances()[2].shape_rect[0] == 50.0F * scale,
                    "device clipping changed retained effect draw indices");
            for (std::size_t index = 0; index < 2; ++index) {
                const auto& packed = resources.instances()[index];
                if (scale > 1.0F && translation == 0.0F) {
                    require(packed.clip_rect == std::array<float, 4>{} && packed.material_params[0] == 0.0F,
                            "fully device-clipped effect was not a transparent degenerate quad");
                } else if (translation == 1.0F || scale == 1.0F) {
                    require(packed.clip_rect[2] > 0.0F && packed.clip_rect[3] < 0.0F,
                            "effect did not reappear after moving inside the physical clip");
                }
            }
        }
    }
}

} // namespace

int main() {
    try {
        test_full_partial_metrics_growth_and_idle_uploads();
        test_zero_effect_and_failure_paths_preserve_dirty_state();
        test_full_retry_after_batch_submit_failure();
        test_failed_projection_returns_to_original_metrics_without_cpu_dirty();
        test_abandon_recreates_buffer_without_releasing_old_epoch();
        test_fractional_dpi_clip_edges_preserve_draw_indices();
    } catch (const std::exception& error) {
        std::cerr << error.what() << '\n';
        return 1;
    }
    return 0;
}
