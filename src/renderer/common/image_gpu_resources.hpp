#pragma once
#include "graphics/image_scene.hpp"
#include "renderer/common/scene_packing.hpp"
#include <optional>
#include <vector>

namespace ryn::detail {
inline constexpr std::uint32_t image_packed_abi_version = 2;

// Separate Image ABI: seven float4 records; existing Quad/Glyph/Effect ABI is unchanged.
struct alignas(16) ImageGpuInstance final {
    std::array<float, 4> position_size;
    std::array<float, 4> uv_rect;
    std::array<float, 4> clip_bounds;
    std::array<float, 4> tint;
    std::array<float, 4> translation_opacity;
    std::array<float, 4> rotation_basis;
    std::array<float, 4> circle_clip;
};

static_assert(sizeof(ImageGpuInstance) == 112);
static_assert(offsetof(ImageGpuInstance, rotation_basis) == 80);
static_assert(offsetof(ImageGpuInstance, circle_clip) == 96);
[[nodiscard]] ImageGpuInstance pack_image_instance(const graphics::ImageInstance&, SceneDeviceMetrics);

struct ImageTextureUpload final {
    std::span<const std::byte> bytes;
    std::size_t source_offset{};
    std::size_t source_row_pitch{};
    std::uint32_t width{};
    std::uint32_t height{};
};

void validate_image_texture_upload(const ImageTextureUpload&, std::uint32_t width, std::uint32_t height);

class ImageGpuApi {
public:
    virtual ~ImageGpuApi() = default;
    virtual void* create_image_texture(std::uint32_t width, std::uint32_t height) = 0;
    virtual void* create_image_buffer(std::size_t size) = 0;
    virtual bool upload_image_texture(void*, const ImageTextureUpload&) = 0;
    virtual bool upload_image_buffer(void*, std::size_t offset, std::span<const std::byte>) = 0;
    virtual void release_image_texture(void*) noexcept = 0;
    virtual void release_image_buffer(void*) noexcept = 0;
};

class ImageGpuResources final {
public:
    explicit ImageGpuResources(ImageGpuApi& api) : api_(&api) {}

    ~ImageGpuResources();
    void synchronize(const graphics::ImageStore* store, SceneDeviceMetrics metrics);
    static void preflight(const graphics::ImageStore* store, std::uint64_t maximum_buffer_bytes,
                          std::uint32_t maximum_width, std::uint32_t maximum_height);
    void abandon_device() noexcept;

    void invalidate_upload() noexcept {
        retry_ = true;
        textures_invalid_ = true;
    }

    [[nodiscard]] void* buffer() const noexcept {
        return buffer_;
    }

    [[nodiscard]] void* texture(std::uint32_t index) const noexcept;
    [[nodiscard]] bool drawable(std::uint32_t index, std::uint32_t generation) const noexcept;

private:
    struct TextureLease final {
        ImageGpuApi* api{};
        void* texture{};
        std::shared_ptr<const ImagePixels> pixels;
        bool uploaded{};

        ~TextureLease() {
            if (texture) {
                api->release_image_texture(texture);
            }
        }
    };

    struct Entry final {
        std::shared_ptr<TextureLease> lease;
        std::uint32_t generation{};
        std::uint64_t revision{};
    };

    std::vector<std::weak_ptr<TextureLease>> leases_;
    bool textures_invalid_{};

    ImageGpuApi* api_;
    void* buffer_{};
    std::size_t capacity_{};
    std::vector<Entry> entries_;
    std::optional<SceneDeviceMetrics> metrics_;
    bool retry_{true};
};
} // namespace ryn::detail
