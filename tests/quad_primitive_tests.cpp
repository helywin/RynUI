#include "graphics/quad_primitive.hpp"

#include <algorithm>
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

class RecordingUploadApi final : public ryn::graphics::QuadUploadApi {
public:
    ryn::graphics::QuadGpuBufferHandle create_vertex_buffer(std::size_t size) override {
        ++create_calls;
        buffer.assign(size, std::byte{});
        return this;
    }

    void release_buffer(ryn::graphics::QuadGpuBufferHandle handle) noexcept override {
        if (handle == this) {
            ++release_calls;
        }
    }

    bool upload(
        ryn::graphics::QuadGpuBufferHandle handle,
        std::size_t offset,
        std::span<const std::byte> bytes) override {
        if (handle != this || offset + bytes.size() > buffer.size()) {
            error = "invalid fake upload";
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
};

void test_instance_layout_matches_shader_contract() {
    using ryn::graphics::QuadAttributeBinding;
    using ryn::graphics::QuadAttributeFormat;
    constexpr std::array expected{
        QuadAttributeBinding{0, QuadAttributeFormat::float4, 0},
        QuadAttributeBinding{1, QuadAttributeFormat::float4, 16},
        QuadAttributeBinding{2, QuadAttributeFormat::float1, 32},
        QuadAttributeBinding{3, QuadAttributeFormat::float1, 36},
        QuadAttributeBinding{4, QuadAttributeFormat::float2, 40},
    };

    require(sizeof(ryn::graphics::QuadInstance) == 48, "Quad instance stride changed");
    require(ryn::graphics::quad_attribute_bindings == expected,
            "Quad attribute metadata does not match the shader contract");
    require(ryn::graphics::quad_vertex_count == 6, "Quad vertex count is incorrect");
}

void test_initial_upload_preserves_instance_bytes() {
    ryn::graphics::QuadInstanceStore store;
    const auto first = store.add(
        {0, 1},
        {
            {-0.8F, 0.6F, 0.5F, -0.4F},
            {0.1F, 0.3F, 0.9F, 1.0F},
            0.75F,
            0.1F,
            {0.0F, 0.0F},
        });
    const auto second = store.add(
        {1, 1},
        {
            {0.1F, 0.4F, 0.6F, -0.5F},
            {0.9F, 0.3F, 0.2F, 1.0F},
            1.0F,
            0.2F,
            {0.05F, -0.02F},
        });
    require(first.instance_index == 0 && second.instance_index == 1,
            "Quad instance indices are unstable");

    RecordingUploadApi api;
    {
        ryn::graphics::QuadGpuBuffer gpu_buffer(api, store);
        const auto expected = std::as_bytes(store.instances());

        require(api.create_calls == 1, "initial upload created the wrong number of buffers");
        require(api.upload_calls == 1 && api.upload_offsets == std::vector<std::size_t>({0}),
                "initial upload did not write one full range at offset zero");
        require(api.buffer == std::vector<std::byte>(expected.begin(), expected.end()),
                "initial GPU buffer bytes differ from the CPU instance store");
        require(gpu_buffer.capacity() == 2, "GPU buffer capacity is incorrect");
        require(gpu_buffer.counters().initial_uploads == 1
                    && gpu_buffer.counters().range_uploads == 0
                    && gpu_buffer.counters().uploaded_bytes == expected.size(),
                "initial upload counters are incorrect");
    }
    require(api.release_calls == 1, "Quad GPU buffer was not released exactly once");
}

ryn::graphics::QuadInstance instance(float value) {
    return {
        {value, value + 0.1F, 0.2F, -0.2F},
        {value, 0.3F, 0.4F, 1.0F},
        1.0F,
        0.1F,
        {0.0F, 0.0F},
    };
}

void test_sparse_dirty_ranges_and_compaction() {
    ryn::graphics::QuadInstanceStore store;
    const std::array initial{
        instance(0.0F),
        instance(0.1F),
        instance(0.2F),
        instance(0.3F),
        instance(0.4F),
    };
    require(store.append(initial) == ryn::graphics::QuadInstanceRange{0, 5},
            "Quad append returned the wrong range");
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
    require(store.material_dirty_ranges().size() == 2
                && store.material_dirty_ranges()[0]
                    == ryn::graphics::QuadInstanceRange{1, 2}
                && store.material_dirty_ranges()[1]
                    == ryn::graphics::QuadInstanceRange{4, 1},
            "sparse Quad Material updates expanded their dirty ranges");

    std::array<ryn::graphics::QuadGeometry, 3> geometry;
    for (std::size_t index = 0; index < geometry.size(); ++index) {
        geometry[index] = {
            {0.2F + static_cast<float>(index), 0.3F, 0.4F, -0.5F},
            0.2F,
            {0.01F, -0.01F},
        };
    }
    static_cast<void>(store.update_geometry({2, 3}, geometry));
    require(store.geometry_dirty_ranges().size() == 1
                && store.geometry_dirty_ranges().front()
                    == ryn::graphics::QuadInstanceRange{2, 3},
            "Quad Geometry update did not retain its exact dirty range");
    require(store.bytes({1, 2}).size()
                == 2 * sizeof(ryn::graphics::QuadInstance),
            "Quad dirty byte range has the wrong size");

    store.clear_dirty_ranges();
    const std::array replacement{instance(0.75F)};
    require(store.replace({1, 2}, replacement)
                == ryn::graphics::QuadInstanceRange{1, 1}
                && store.size() == 4,
            "Quad replace did not compact the instance store");
    require(store.geometry_dirty_ranges().size() == 1
                && store.geometry_dirty_ranges().front()
                    == ryn::graphics::QuadInstanceRange{1, 3},
            "Quad compaction did not dirty the shifted suffix only");
}

void test_gpu_buffer_growth_and_sparse_synchronization() {
    ryn::graphics::QuadInstanceStore store;
    const std::array initial{instance(0.0F), instance(0.1F)};
    static_cast<void>(store.append(initial));
    RecordingUploadApi api;
    ryn::graphics::QuadGpuBuffer gpu(api, store);

    const std::array appended{instance(0.2F)};
    static_cast<void>(store.append(appended));
    gpu.synchronize(store);
    require(gpu.capacity() == 4
                && gpu.counters().buffer_reallocations == 1
                && api.create_calls == 2,
            "Quad GPU buffer did not grow with reusable capacity");

    const std::array material{
        ryn::graphics::QuadMaterial{{0.2F, 0.8F, 0.4F, 1.0F}, 0.5F},
    };
    static_cast<void>(store.update_material({1, 1}, material));
    gpu.synchronize(store);
    require(api.upload_offsets.back() == sizeof(ryn::graphics::QuadInstance)
                && gpu.counters().range_uploads == 2,
            "Quad GPU synchronization expanded a sparse Material upload");
}

} // namespace

int main() {
    try {
        test_instance_layout_matches_shader_contract();
        test_initial_upload_preserves_instance_bytes();
        test_sparse_dirty_ranges_and_compaction();
        test_gpu_buffer_growth_and_sparse_synchronization();
    } catch (const std::exception& error) {
        std::cerr << error.what() << '\n';
        return 1;
    }
    return 0;
}
