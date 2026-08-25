#pragma once

#include "runtime/node_store.hpp"

#include <array>
#include <cstddef>
#include <cstdint>
#include <span>
#include <string>
#include <vector>

namespace ryn::graphics {

struct alignas(16) QuadInstance {
    std::array<float, 4> clip_rect{};
    std::array<float, 4> color{1.0F, 1.0F, 1.0F, 1.0F};
    float opacity{1.0F};
    float corner_radius{0.0F};
    std::array<float, 2> translation{};

    friend constexpr bool operator==(const QuadInstance&, const QuadInstance&) = default;
};

static_assert(sizeof(QuadInstance) == 48);
static_assert(offsetof(QuadInstance, clip_rect) == 0);
static_assert(offsetof(QuadInstance, color) == 16);
static_assert(offsetof(QuadInstance, opacity) == 32);
static_assert(offsetof(QuadInstance, corner_radius) == 36);
static_assert(offsetof(QuadInstance, translation) == 40);

enum class QuadAttributeFormat {
    float1,
    float2,
    float4,
};

struct QuadAttributeBinding {
    std::uint32_t location;
    QuadAttributeFormat format;
    std::uint32_t offset;

    friend constexpr bool operator==(QuadAttributeBinding, QuadAttributeBinding) = default;
};

inline constexpr std::array<QuadAttributeBinding, 5> quad_attribute_bindings{{
    {0, QuadAttributeFormat::float4, 0},
    {1, QuadAttributeFormat::float4, 16},
    {2, QuadAttributeFormat::float1, 32},
    {3, QuadAttributeFormat::float1, 36},
    {4, QuadAttributeFormat::float2, 40},
}};

inline constexpr std::uint32_t quad_vertex_count = 6;

struct QuadPrimitive {
    runtime::NodeId node;
    std::uint32_t instance_index{0};
};

class QuadInstanceStore final {
public:
    [[nodiscard]] QuadPrimitive add(runtime::NodeId node, QuadInstance instance);
    [[nodiscard]] const QuadInstance& at(std::uint32_t index) const;
    [[nodiscard]] QuadInstance& at(std::uint32_t index);
    [[nodiscard]] std::span<const QuadInstance> instances() const noexcept;
    [[nodiscard]] std::size_t size() const noexcept;
    [[nodiscard]] std::span<const std::byte> bytes(
        std::uint32_t first,
        std::uint32_t count) const;

private:
    std::vector<QuadInstance> instances_;
};

using QuadGpuBufferHandle = void*;

class QuadUploadApi {
public:
    virtual ~QuadUploadApi() = default;

    virtual QuadGpuBufferHandle create_vertex_buffer(std::size_t size) = 0;
    virtual void release_buffer(QuadGpuBufferHandle buffer) noexcept = 0;
    virtual bool upload(
        QuadGpuBufferHandle buffer,
        std::size_t offset,
        std::span<const std::byte> bytes) = 0;
    [[nodiscard]] virtual const char* last_error() const noexcept = 0;
};

struct QuadUploadCounters {
    std::uint64_t initial_uploads{0};
    std::uint64_t range_uploads{0};
    std::uint64_t uploaded_bytes{0};
};

class QuadGpuBuffer final {
public:
    QuadGpuBuffer(QuadUploadApi& api, const QuadInstanceStore& store);
    QuadGpuBuffer(const QuadGpuBuffer&) = delete;
    QuadGpuBuffer& operator=(const QuadGpuBuffer&) = delete;
    QuadGpuBuffer(QuadGpuBuffer&&) = delete;
    QuadGpuBuffer& operator=(QuadGpuBuffer&&) = delete;
    ~QuadGpuBuffer();

    void upload_range(
        const QuadInstanceStore& store,
        std::uint32_t first,
        std::uint32_t count);

    [[nodiscard]] QuadGpuBufferHandle handle() const noexcept;
    [[nodiscard]] std::uint32_t capacity() const noexcept;
    [[nodiscard]] const QuadUploadCounters& counters() const noexcept;

private:
    QuadUploadApi* api_;
    QuadGpuBufferHandle handle_{nullptr};
    std::uint32_t capacity_{0};
    QuadUploadCounters counters_;
};

} // namespace ryn::graphics
