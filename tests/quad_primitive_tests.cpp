#include "renderer/common/quad_gpu_resources.hpp"
#include "renderer/common/rounded_effect_packing.hpp"
#include "graphics/quad_primitive.hpp"

#include <algorithm>
#include <cmath>
#include <cstring>
#include <iostream>
#include <stdexcept>
#include <vector>

namespace {

void require(bool condition, const char* message) {
    if (!condition) {
        throw std::runtime_error(message);
    }
}

class RecordingUploadApi final : public ryn::detail::QuadUploadApi {
public:
    ryn::detail::QuadGpuBufferHandle create_vertex_buffer(std::size_t size) override {
        ++create_calls;
        buffer.assign(size, std::byte{});
        return this;
    }

    void release_buffer(ryn::detail::QuadGpuBufferHandle handle) noexcept override {
        if (handle == this) {
            ++release_calls;
        }
    }

    bool upload(ryn::detail::QuadGpuBufferHandle handle, std::size_t offset,
                std::span<const std::byte> bytes) override {
        if (handle != this || offset + bytes.size() > buffer.size()) {
            error = "invalid fake upload";
            return false;
        }
        if (fail_next_upload) {
            fail_next_upload = false;
            return false;
        }
        ++upload_calls;
        upload_offsets.push_back(offset);
        std::copy(bytes.begin(), bytes.end(), buffer.begin() + offset);
        return true;
    }

    const char* last_error() const noexcept override {
        return error.c_str();
    }

    std::vector<std::byte> buffer;
    std::vector<std::size_t> upload_offsets;
    std::string error;
    int create_calls{0};
    int upload_calls{0};
    int release_calls{0};
    bool fail_next_upload{};
};

void test_instance_layout_matches_shader_contract() {
    using ryn::detail::QuadAttributeBinding;
    using ryn::detail::QuadAttributeFormat;
    constexpr std::array expected{
        QuadAttributeBinding{0, QuadAttributeFormat::float4, 0},
        QuadAttributeBinding{1, QuadAttributeFormat::float4, 16},
        QuadAttributeBinding{2, QuadAttributeFormat::float1, 32},
        QuadAttributeBinding{3, QuadAttributeFormat::float1, 36},
        QuadAttributeBinding{4, QuadAttributeFormat::float2, 40},
    };

    require(sizeof(ryn::detail::QuadGpuInstance) == 48, "Quad instance stride changed");
    require(ryn::detail::quad_attribute_bindings == expected,
            "Quad attribute metadata does not match the shader contract");
    require(ryn::detail::quad_vertex_count == 6, "Quad vertex count is incorrect");
    require(sizeof(ryn::detail::QuadViewportUniform) == 16, "Quad vertex uniform must occupy one constant slot");
}

std::array<float, 4> over(std::array<float, 4> source, std::array<float, 4> destination) {
    return {source[0] * source[3] + destination[0] * (1 - source[3]),
            source[1] * source[3] + destination[1] * (1 - source[3]),
            source[2] * source[3] + destination[2] * (1 - source[3]), source[3] + destination[3] * (1 - source[3])};
}

void test_shape_raster_guard_and_composited_edges() {
    using namespace ryn;
    constexpr std::array scales{1.0F, 1.25F, 1.5F, 2.0F};
    constexpr std::array phases{0.0F, 0.25F, 0.5F, 0.75F};
    constexpr std::array radii{0.0F, 4.0F, 8.0F};
    constexpr std::array corners{runtime::Point{0, 0}, runtime::Point{1, 0}, runtime::Point{1, 1},
                                 runtime::Point{0, 0}, runtime::Point{1, 1}, runtime::Point{0, 1}};
    bool saw_outside_coverage{};
    const auto viewport = detail::quad_viewport_uniform(160, 120);
    for (const auto scale : scales) {
        const detail::SceneDeviceMetrics metrics{160, 120, scale};
        for (const auto phase_x : phases) {
            for (const auto phase_y : phases) {
                for (const auto radius : radii) {
                    for (const auto alpha : {1.0F, 0.65F}) {
                        const auto x = 20 + phase_x;
                        const auto y = 25 + phase_y;
                        const auto extent = 16 * scale;
                        // Split position and motion so the guard cannot hide a
                        // dropped translation or accidentally resize the shape.
                        const graphics::QuadInstance outer{{(x - 1.125F) / scale, (y + 0.375F) / scale, 16, 16},
                                                           {0.05F, 0.4F, 1, alpha},
                                                           0.8F,
                                                           radius,
                                                           {1.125F / scale, -0.375F / scale}};
                        auto inner = outer;
                        inner.bounds[0] += 1;
                        inner.bounds[1] += 1;
                        inner.bounds[2] -= 2;
                        inner.bounds[3] -= 2;
                        inner.corner_radius = std::max(0.0F, radius - 1);
                        inner.color = {1, 1, 1, alpha};
                        const auto packed = detail::pack_quad_instance(outer, metrics);
                        const auto packed_inner = detail::pack_quad_instance(inner, metrics);
                        for (const auto corner : corners) {
                            const auto vertex = detail::quad_gpu_vertex_reference(packed, corner, viewport);
                            const auto pixel_x = (vertex.position_ndc.x + 1) * 80;
                            const auto pixel_y = (1 - vertex.position_ndc.y) * 60;
                            const auto expected_x = x + corner.x * extent + (corner.x * 2 - 1);
                            const auto expected_y = y + corner.y * extent + (corner.y * 2 - 1);
                            require(std::abs(pixel_x - expected_x) < 0.0001F &&
                                        std::abs(pixel_y - expected_y) < 0.0001F,
                                    "Quad triangles did not include the full physical AA guard");
                            require(std::abs(vertex.pixel_extent.width - extent) < 0.0001F &&
                                        std::abs(vertex.pixel_extent.height - extent) < 0.0001F &&
                                        std::abs(vertex.centered_pixels.x -
                                                 ((corner.x - 0.5F) * extent + corner.x * 2 - 1)) < 0.0001F &&
                                        std::abs(vertex.centered_pixels.y -
                                                 ((corner.y - 0.5F) * extent + corner.y * 2 - 1)) < 0.0001F,
                                    "Quad draw guard changed the shape extent or local SDF coordinates");
                        }
                        const auto effect = [&](const graphics::QuadInstance& q) {
                            auto fill = graphics::make_shadow_effect(
                                {{q.bounds[0], q.bounds[1], q.bounds[2], q.bounds[3]}, q.corner_radius},
                                ShadowLayer{
                                    ShadowKind::outer, {}, 0, 0, Color{q.color[0], q.color[1], q.color[2], q.color[3]}},
                                {q.translation[0], q.translation[1]});
                            fill.material.opacity = q.opacity;
                            return detail::pack_rounded_effect_instance(fill, metrics);
                        };
                        const auto expected_outer = effect(outer);
                        const auto expected_inner = effect(inner);
                        for (auto py = 23; py <= static_cast<int>(y + extent) + 2; ++py) {
                            for (auto px = 18; px <= static_cast<int>(x + extent) + 2; ++px) {
                                const runtime::Point sample{px + 0.5F, py + 0.5F};
                                const auto actual_outer = detail::quad_gpu_fragment_reference(sample, packed, viewport);
                                const auto actual_inner =
                                    detail::quad_gpu_fragment_reference(sample, packed_inner, viewport);
                                // Existing clipped-effect coverage is an independent
                                // one-physical-pixel oracle. Sampling also applies
                                // the VS triangle bounds, not only the ideal SDF.
                                const auto reference_outer =
                                    detail::rounded_effect_gpu_fragment_reference(sample, expected_outer);
                                const auto reference_inner =
                                    detail::rounded_effect_gpu_fragment_reference(sample, expected_inner);
                                constexpr std::array<float, 4> background{0.9F, 0.95F, 0.85F, 1};
                                const auto actual = over(actual_inner, over(actual_outer, background));
                                const auto expected = over(reference_inner, over(reference_outer, background));
                                for (std::size_t channel = 0; channel < actual.size(); ++channel) {
                                    require(std::abs(actual[channel] - expected[channel]) < 0.0002F,
                                            "Quad/source-over border differs from common physical AA coverage");
                                }
                                if ((sample.x < x || sample.x > x + extent || sample.y < y || sample.y > y + extent) &&
                                    reference_outer[3] > 0.001F) {
                                    require(actual_outer[3] > 0.001F,
                                            "nonzero outer AA coverage lies outside submitted Quad triangles");
                                    saw_outside_coverage = true;
                                }
                            }
                        }
                    }
                }
            }
        }
    }
    require(saw_outside_coverage, "raster regression did not exercise coverage outside nominal shape bounds");
}

void test_zero_area_and_viewport_raster_clipping() {
    using namespace ryn;
    const auto viewport = detail::quad_viewport_uniform(100, 80);
    for (const auto size : {runtime::Size{0, 16}, runtime::Size{16, 0}, runtime::Size{0, 0}}) {
        const graphics::QuadInstance shape{{20, 20, size.width, size.height}, {1, 0, 0, 1}, 1, 4, {}};
        const auto packed = detail::pack_quad_instance(shape, {100, 80, 1});
        const auto first = detail::quad_gpu_vertex_reference(packed, {0, 0}, viewport);
        const auto last = detail::quad_gpu_vertex_reference(packed, {1, 1}, viewport);
        require((size.width == 0 && first.position_ndc.x == last.position_ndc.x) ||
                    (size.height == 0 && first.position_ndc.y == last.position_ndc.y),
                "zero-area Quad expanded into visible triangles");
        require(detail::quad_gpu_fragment_reference({20.25F, 20.25F}, packed, viewport)[3] == 0,
                "zero-area Quad gained AA coverage");
    }
    const auto packed = detail::pack_quad_instance({{-2, -2, 16, 16}, {1, 0, 0, 1}, 1, 0, {}}, {100, 80, 1});
    require(detail::quad_gpu_fragment_reference({-0.5F, 4.5F}, packed, viewport)[3] == 0 &&
                detail::quad_gpu_fragment_reference({4.5F, -0.5F}, packed, viewport)[3] == 0 &&
                detail::quad_gpu_fragment_reference({0.5F, 0.5F}, packed, viewport)[3] == 1,
            "Quad AA guard escaped the physical viewport");
    bool rejected{};
    try {
        static_cast<void>(detail::quad_viewport_uniform(0, 80));
    } catch (const std::invalid_argument&) {
        rejected = true;
    }
    require(rejected, "Quad vertex viewport accepted zero physical width");
}

void test_initial_upload_preserves_instance_bytes() {
    ryn::graphics::QuadInstanceStore store;
    const auto first = store.add({0, 1}, {
                                             {10.0F, 20.0F, 25.0F, 20.0F},
                                             {0.1F, 0.3F, 0.9F, 1.0F},
                                             0.75F,
                                             2.0F,
                                             {0.0F, 0.0F},
                                         });
    const auto second = store.add({1, 1}, {
                                              {55.0F, 30.0F, 30.0F, 25.0F},
                                              {0.9F, 0.3F, 0.2F, 1.0F},
                                              1.0F,
                                              5.0F,
                                              {2.5F, 1.0F},
                                          });
    require(first.instance_index == 0 && second.instance_index == 1, "Quad instance indices are unstable");

    RecordingUploadApi api;
    {
        ryn::detail::QuadGpuBuffer gpu_buffer(api, store, {100, 100, 1});
        const std::array packed{
            ryn::detail::QuadGpuInstance{{-0.8F, 0.6F, 0.5F, -0.4F}, {0.1F, 0.3F, 0.9F, 1}, 0.75F, 0.1F, {0, 0}},
            ryn::detail::QuadGpuInstance{{0.1F, 0.4F, 0.6F, -0.5F}, {0.9F, 0.3F, 0.2F, 1}, 1, 0.2F, {0.05F, -0.02F}}};
        // Compare floats below; different arithmetic order can change the last bit.
        std::array<ryn::detail::QuadGpuInstance, 2> actual;
        std::memcpy(actual.data(), api.buffer.data(), api.buffer.size());
        for (std::size_t i = 0; i < actual.size(); ++i) {
            for (std::size_t j = 0; j < 4; ++j) {
                require(std::abs(actual[i].clip_rect[j] - packed[i].clip_rect[j]) < 0.00001F,
                        "logical Quad bounds did not pack to expected NDC");
            }
            require(actual[i].color == packed[i].color && actual[i].opacity == packed[i].opacity &&
                        std::abs(actual[i].corner_radius - packed[i].corner_radius) < 0.00001F,
                    "Quad upload lost material or radius");
            for (std::size_t j = 0; j < 2; ++j) {
                require(std::abs(actual[i].translation[j] - packed[i].translation[j]) < 0.00001F,
                        "Quad upload lost translation");
            }
        }
        const auto expected = std::as_bytes(std::span(packed));

        require(api.create_calls == 1, "initial upload created the wrong number of buffers");
        require(api.upload_calls == 1 && api.upload_offsets == std::vector<std::size_t>({0}),
                "initial upload did not write one full range at offset zero");
        require(api.buffer.size() == expected.size(), "initial GPU buffer has incorrect packed size");
        require(gpu_buffer.capacity() == 2, "GPU buffer capacity is incorrect");
        require(gpu_buffer.counters().initial_uploads == 1 && gpu_buffer.counters().range_uploads == 0 &&
                    gpu_buffer.counters().uploaded_bytes == expected.size(),
                "initial upload counters are incorrect");
    }
    require(api.release_calls == 1, "Quad GPU buffer was not released exactly once");
}

ryn::graphics::QuadInstance instance(float value) {
    return {
        {value, value + 0.1F, 0.2F, 0.2F}, {value, 0.3F, 0.4F, 1.0F}, 1.0F, 0.1F, {0.0F, 0.0F},
    };
}

void test_full_retry_after_batch_submit_failure() {
    ryn::graphics::QuadInstanceStore store;
    const std::array initial{instance(0.1F), instance(0.2F)};
    static_cast<void>(store.append(initial));
    RecordingUploadApi api;
    ryn::detail::QuadGpuBuffer gpu(api, store, {100, 100, 1});
    require(store.geometry_dirty_ranges().empty(), "initial Quad upload did not optimistically clear dirty ranges");
    api.buffer.assign(api.buffer.size(), std::byte{});
    store.mark_all_dirty();
    require(store.geometry_dirty_ranges().size() == 1 &&
                store.geometry_dirty_ranges().front() == ryn::graphics::QuadInstanceRange{0, 2},
            "failed batch did not restore the full Quad upload range");
    gpu.synchronize(store, {100, 100, 1});
    const std::array packed{ryn::detail::pack_quad_instance(store.at(0), {100, 100, 1}),
                            ryn::detail::pack_quad_instance(store.at(1), {100, 100, 1})};
    const auto expected = std::as_bytes(std::span(packed));
    require(api.buffer == std::vector<std::byte>(expected.begin(), expected.end()) && api.upload_offsets.back() == 0,
            "Quad retry did not restore the complete GPU buffer");
}

void test_sparse_dirty_ranges_and_compaction() {
    ryn::graphics::QuadInstanceStore store;
    const std::array initial{
        instance(0.0F), instance(0.1F), instance(0.2F), instance(0.3F), instance(0.4F),
    };
    require(store.append(initial) == ryn::graphics::QuadInstanceRange{0, 5}, "Quad append returned the wrong range");
    store.clear_dirty_ranges();

    const std::array first_materials{
        ryn::graphics::QuadMaterial{{0.9F, 0.1F, 0.2F, 1.0F}, 0.7F},
        ryn::graphics::QuadMaterial{{0.8F, 0.2F, 0.3F, 1.0F}, 0.6F},
    };
    static_cast<void>(store.update_material({1, 2}, first_materials));
    const std::array last_material{
        ryn::graphics::QuadMaterial{{0.7F, 0.3F, 0.4F, 1.0F}, 0.5F},
    };
    static_cast<void>(store.update_material({4, 1}, last_material));
    require(store.material_dirty_ranges().size() == 2 &&
                store.material_dirty_ranges()[0] == ryn::graphics::QuadInstanceRange{1, 2} &&
                store.material_dirty_ranges()[1] == ryn::graphics::QuadInstanceRange{4, 1},
            "sparse Quad Material updates expanded their dirty ranges");

    std::array<ryn::graphics::QuadGeometry, 3> geometry;
    for (std::size_t index = 0; index < geometry.size(); ++index) {
        geometry[index] = {
            {0.2F + static_cast<float>(index), 0.3F, 0.4F, 0.5F},
            0.2F,
            {0.01F, -0.01F},
        };
    }
    static_cast<void>(store.update_geometry({2, 3}, geometry));
    require(store.geometry_dirty_ranges().size() == 1 &&
                store.geometry_dirty_ranges().front() == ryn::graphics::QuadInstanceRange{2, 3},
            "Quad Geometry update did not retain its exact dirty range");
    require(store.bytes({1, 2}).size() == 2 * sizeof(ryn::graphics::QuadInstance),
            "Quad dirty byte range has the wrong size");

    store.clear_dirty_ranges();
    const std::array replacement{instance(0.75F)};
    require(store.replace({1, 2}, replacement) == ryn::graphics::QuadInstanceRange{1, 1} && store.size() == 4,
            "Quad replace did not compact the instance store");
    require(store.geometry_dirty_ranges().size() == 1 &&
                store.geometry_dirty_ranges().front() == ryn::graphics::QuadInstanceRange{1, 3},
            "Quad compaction did not dirty the shifted suffix only");
}

void test_gpu_buffer_growth_and_sparse_synchronization() {
    ryn::graphics::QuadInstanceStore store;
    const std::array initial{instance(0.0F), instance(0.1F)};
    static_cast<void>(store.append(initial));
    RecordingUploadApi api;
    ryn::detail::QuadGpuBuffer gpu(api, store, {100, 100, 1});

    const std::array appended{instance(0.2F)};
    static_cast<void>(store.append(appended));
    gpu.synchronize(store, {100, 100, 1});
    require(gpu.capacity() == 4 && gpu.counters().buffer_reallocations == 1 && api.create_calls == 2,
            "Quad GPU buffer did not grow with reusable capacity");

    const std::array material{
        ryn::graphics::QuadMaterial{{0.2F, 0.8F, 0.4F, 1.0F}, 0.5F},
    };
    static_cast<void>(store.update_material({1, 1}, material));
    gpu.synchronize(store, {100, 100, 1});
    require(api.upload_offsets.back() == sizeof(ryn::detail::QuadGpuInstance) && gpu.counters().range_uploads == 2,
            "Quad GPU synchronization expanded a sparse Material upload");
}

void test_out_of_order_dirty_planning_and_shifted_suffix() {
    ryn::graphics::QuadInstanceStore store;
    std::array<ryn::graphics::QuadInstance, 8> initial{};
    static_cast<void>(store.append(initial));
    store.clear_dirty_ranges();
    const std::array material{ryn::graphics::QuadMaterial{{0.1F, 0.2F, 0.3F, 1.0F}, 0.5F}};
    static_cast<void>(store.update_material({5, 1}, material));
    static_cast<void>(store.update_material({2, 1}, material));
    static_cast<void>(store.update_material({1, 1}, material));
    const auto first_plan = store.material_dirty_ranges();
    require(first_plan.size() == 2 && first_plan[0] == ryn::graphics::QuadInstanceRange{1, 2} &&
                first_plan[1] == ryn::graphics::QuadInstanceRange{5, 1},
            "Quad dirty plan did not sort and merge adjacent ranges once");
    const auto second_plan = store.material_dirty_ranges();
    require(second_plan.data() == first_plan.data() && second_plan.size() == 2,
            "unchanged Quad dirty plan was rebuilt");

    const std::array overlapping{
        ryn::graphics::QuadMaterial{{0.4F, 0.3F, 0.2F, 1.0F}, 0.8F},
        ryn::graphics::QuadMaterial{{0.4F, 0.3F, 0.2F, 1.0F}, 0.8F},
        ryn::graphics::QuadMaterial{{0.4F, 0.3F, 0.2F, 1.0F}, 0.8F},
    };
    static_cast<void>(store.update_material({2, 3}, overlapping));
    require(store.material_dirty_ranges().size() == 1 &&
                store.material_dirty_ranges().front() == ryn::graphics::QuadInstanceRange{1, 5},
            "overlapping Quad ranges were not merged into the exact union");

    const std::array replacement{instance(0.7F), instance(0.8F)};
    static_cast<void>(store.replace({2, 1}, replacement));
    const auto geometry = store.geometry_dirty_ranges();
    require(store.size() == 9 && geometry.size() == 1 && geometry.front() == ryn::graphics::QuadInstanceRange{2, 7} &&
                store.material_dirty_ranges().size() == 1 &&
                store.material_dirty_ranges().front() == ryn::graphics::QuadInstanceRange{1, 1},
            "Quad shifted suffix left stale or missing dirty ranges");
}

void test_bounded_sparse_upload_coalescing() {
    using namespace ryn;
    graphics::QuadInstanceStore store;
    std::array<graphics::QuadInstance, 128> initial{};
    static_cast<void>(store.append(initial));
    RecordingUploadApi api;
    detail::QuadGpuBuffer gpu(api, store, {100, 100, 1});
    gpu.set_sparse_upload_coalescing_limit(16 * sizeof(detail::QuadGpuInstance));
    const auto update = [&](std::uint32_t a, std::uint32_t b, float opacity) {
        const std::array material{graphics::QuadMaterial{{0.1F, 0.3F, 0.8F, 1}, opacity}};
        static_cast<void>(store.update_material({a, 1}, material));
        static_cast<void>(store.update_material({b, 1}, material));
    };
    update(0, 5, 0.5F);
    auto uploads = api.upload_calls;
    gpu.synchronize(store, {100, 100, 1});
    require(api.upload_calls == uploads + 1 && api.upload_offsets.back() == 0,
            "nearby Quad changes were not uploaded together");
    const auto verify_bytes = [&] {
        for (std::size_t index = 0; index < store.size(); ++index) {
            const auto packed = detail::pack_quad_instance(store.at(index), {100, 100, 1});
            require(std::memcmp(api.buffer.data() + index * sizeof(packed), &packed, sizeof(packed)) == 0,
                    "coalesced Quad upload changed clean or dirty instance bytes");
        }
    };
    verify_bytes();
    update(0, 127, 0.7F);
    uploads = api.upload_calls;
    gpu.synchronize(store, {100, 100, 1});
    require(api.upload_calls == uploads + 2, "distant Quad changes ignored the coalescing bound");
    gpu.set_sparse_upload_coalescing_limit(4 * sizeof(detail::QuadGpuInstance));
    update(0, 5, 0.8F);
    uploads = api.upload_calls;
    gpu.synchronize(store, {100, 100, 1});
    require(api.upload_calls == uploads + 2, "Quad coalescing exceeded its byte limit");
    gpu.set_sparse_upload_coalescing_limit(16 * sizeof(detail::QuadGpuInstance));
    update(0, 5, 0.9F);
    api.fail_next_upload = true;
    bool failed{};
    try {
        gpu.synchronize(store, {100, 100, 1});
    } catch (const std::runtime_error&) {
        failed = true;
    }
    require(failed && !store.material_dirty_ranges().empty(), "failed merged Quad upload lost dirty state");
    gpu.synchronize(store, {100, 100, 1});
    verify_bytes();
}

} // namespace

int main() {
    try {
        test_instance_layout_matches_shader_contract();
        test_shape_raster_guard_and_composited_edges();
        test_zero_area_and_viewport_raster_clipping();
        test_initial_upload_preserves_instance_bytes();
        test_full_retry_after_batch_submit_failure();
        test_sparse_dirty_ranges_and_compaction();
        test_gpu_buffer_growth_and_sparse_synchronization();
        test_out_of_order_dirty_planning_and_shifted_suffix();
        test_bounded_sparse_upload_coalescing();
    } catch (const std::exception& error) {
        std::cerr << error.what() << '\n';
        return 1;
    }
    return 0;
}
