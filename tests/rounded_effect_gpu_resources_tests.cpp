#include "renderer/sdl/rounded_effect_gpu_resources.hpp"

#include <array>
#include <cstddef>
#include <cstdint>
#include <iostream>
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
    ryn::detail::RoundedEffectGpuBufferHandle create_effect_buffer(
        std::size_t size) override {
        events.emplace_back("create");
        requested_sizes.push_back(size);
        if (fail_create) return nullptr;
        const auto handle = ++next_handle;
        ++live_buffers;
        return reinterpret_cast<void*>(handle);
    }

    bool upload_effect_buffer(
        ryn::detail::RoundedEffectGpuBufferHandle buffer,
        std::size_t offset,
        std::span<const std::byte> bytes) override {
        events.emplace_back("upload");
        uploads.push_back({
            reinterpret_cast<std::uintptr_t>(buffer),
            offset,
            {bytes.begin(), bytes.end()},
        });
        return !fail_upload;
    }

    void release_effect_buffer(
        ryn::detail::RoundedEffectGpuBufferHandle) noexcept override {
        events.emplace_back("release");
        --live_buffers;
    }

    const char* effect_gpu_error() const noexcept override {
        return "injected rounded-effect GPU failure";
    }

    bool fail_create{};
    bool fail_upload{};
    std::uintptr_t next_handle{};
    std::size_t live_buffers{};
    std::vector<std::size_t> requested_sizes;
    std::vector<Upload> uploads;
    std::vector<std::string> events;
};

ryn::graphics::RoundedEffectInstance effect(float x, std::uint8_t marker) {
    return ryn::graphics::make_shadow_effect(
        {{x, 10.0F, 10.0F, 10.0F}, 2.0F},
        {ryn::ShadowKind::outer, {}, 2.0F, 0.0F,
         ryn::Color::rgba8(marker, marker, marker, 80)});
}

void test_full_partial_metrics_growth_and_idle_uploads() {
    RecordingApi api;
    {
        ryn::detail::RoundedEffectGpuResources resources(api);
        ryn::graphics::RoundedEffectStore store;
        const std::array initial{effect(10.0F, 1), effect(30.0F, 2), effect(50.0F, 3)};
        const auto ids = store.add_batch(initial);
        resources.synchronize(store, {100, 100, 1.0F});
        require(resources.capacity() == 4 && resources.instance_count() == 3
                    && api.requested_sizes.size() == 1
                    && api.requested_sizes.front()
                        == 4 * sizeof(ryn::graphics::RoundedEffectGpuInstance)
                    && api.uploads.size() == 1 && api.uploads.front().offset == 0
                    && api.uploads.front().bytes.size()
                        == 3 * sizeof(ryn::graphics::RoundedEffectGpuInstance),
                "initial rounded-effect GPU allocation or full upload differs");

        auto material = store.at(ids[1]).material;
        material.opacity = 0.5F;
        require(store.update_material(ids[1], material),
                "GPU resource fixture material did not change");
        resources.synchronize(store, {100, 100, 1.0F});
        require(api.uploads.size() == 2
                    && api.uploads.back().offset
                        == sizeof(ryn::graphics::RoundedEffectGpuInstance)
                    && api.uploads.back().bytes.size()
                        == sizeof(ryn::graphics::RoundedEffectGpuInstance)
                    && resources.counters().partial_uploads == 1,
                "material-only effect update did not remain a partial upload");

        resources.synchronize(store, {200, 200, 2.0F});
        require(api.uploads.size() == 3 && api.uploads.back().offset == 0
                    && api.uploads.back().bytes.size()
                        == 3 * sizeof(ryn::graphics::RoundedEffectGpuInstance)
                    && resources.instances()[1].shape_rect[0] == 60.0F,
                "display-scale change did not force a full device-geometry upload");
        resources.synchronize(store, {200, 200, 2.0F});
        require(api.uploads.size() == 3
                    && resources.counters().idle_synchronizations == 1,
                "idle effect synchronization uploaded unchanged data");

        static_cast<void>(store.add(effect(70.0F, 4)));
        static_cast<void>(store.add(effect(80.0F, 5)));
        resources.synchronize(store, {200, 200, 2.0F});
        require(resources.capacity() == 8 && resources.instance_count() == 5
                    && api.requested_sizes.size() == 2
                    && api.live_buffers == 1
                    && resources.counters().buffer_reallocations == 2,
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
        require(resources.buffer() == nullptr && resources.instance_count() == 0
                    && empty_api.events.empty()
                    && resources.counters().zero_effect_synchronizations == 1,
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
            require(failed && resources.buffer() == nullptr
                        && !store.geometry_dirty_ranges().empty(),
                    "failed effect upload discarded dirty state or published a buffer");
        }
        require(api.live_buffers == 0,
                "failed rounded-effect GPU setup leaked a candidate buffer");
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
        require(failed && resources.buffer() != nullptr
                    && !store.material_dirty_ranges().empty() && api.live_buffers == 1,
                "partial upload failure lost the live buffer or retry range");
    }
    require(api.live_buffers == 0,
            "rounded-effect GPU buffer leaked after partial upload failure");
}

} // namespace

int main() {
    try {
        test_full_partial_metrics_growth_and_idle_uploads();
        test_zero_effect_and_failure_paths_preserve_dirty_state();
    } catch (const std::exception& error) {
        std::cerr << error.what() << '\n';
        return 1;
    }
    return 0;
}
