#include "renderer/sdl/glyph_gpu_resources.hpp"

#include <array>
#include <algorithm>
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

struct TextureRecord {
    std::uintptr_t texture{};
    std::uint32_t page{};
    ryn::graphics::GlyphAtlasRect rectangle{};
    std::uint32_t offset{};
    std::uint32_t row_pitch{};
    std::uint32_t rows{};
    std::vector<std::byte> bytes;
};

struct BufferRecord {
    std::uintptr_t buffer{};
    std::size_t offset{};
    std::vector<std::byte> bytes;
};

class RecordingGpuApi final : public ryn::detail::GlyphGpuApi {
public:
    enum class Failure {
        none,
        sampler,
        texture,
        buffer,
        texture_upload,
        buffer_upload,
    };

    explicit RecordingGpuApi(Failure failure = Failure::none) : failure_(failure) {}

    ryn::detail::GlyphGpuSamplerHandle create_glyph_sampler() override {
        events.emplace_back("create_sampler");
        if (failure_ == Failure::sampler) {
            return nullptr;
        }
        sampler_live_ = true;
        return handle(1);
    }

    ryn::detail::GlyphGpuTextureHandle create_glyph_texture(
        std::uint32_t width,
        std::uint32_t height) override {
        events.emplace_back("create_texture");
        require(width == 10 && height == 10, "atlas texture dimensions differ");
        if (failure_ == Failure::texture) {
            return nullptr;
        }
        const auto value = 100 + textures_live_;
        ++textures_live_;
        return handle(value);
    }

    ryn::detail::GlyphGpuBufferHandle create_glyph_buffer(std::size_t size) override {
        events.emplace_back("create_buffer");
        require(size > 0, "zero-sized Glyph buffer was requested");
        if (failure_ == Failure::buffer) {
            return nullptr;
        }
        buffer_live_ = true;
        return handle(2);
    }

    bool upload_glyph_texture(
        ryn::detail::GlyphGpuTextureHandle texture,
        const ryn::detail::GlyphTextureUpload& upload) override {
        events.emplace_back("upload_texture");
        TextureRecord record{
            value(texture), upload.page, upload.rectangle,
            upload.transfer_offset, upload.pixels_per_row, upload.rows_per_layer,
            {upload.bytes.begin(), upload.bytes.end()},
        };
        textures.push_back(std::move(record));
        return failure_ != Failure::texture_upload;
    }

    bool upload_glyph_buffer(
        ryn::detail::GlyphGpuBufferHandle buffer,
        std::size_t offset,
        std::span<const std::byte> bytes) override {
        events.emplace_back("upload_buffer");
        buffers.push_back({value(buffer), offset, {bytes.begin(), bytes.end()}});
        return failure_ != Failure::buffer_upload;
    }

    void release_glyph_buffer(ryn::detail::GlyphGpuBufferHandle) noexcept override {
        events.emplace_back("release_buffer");
        buffer_live_ = false;
    }

    void release_glyph_texture(ryn::detail::GlyphGpuTextureHandle) noexcept override {
        events.emplace_back("release_texture");
        --textures_live_;
    }

    void release_glyph_sampler(ryn::detail::GlyphGpuSamplerHandle) noexcept override {
        events.emplace_back("release_sampler");
        sampler_live_ = false;
    }

    const char* glyph_gpu_error() const noexcept override {
        return "injected Glyph GPU failure";
    }

    [[nodiscard]] bool no_leaks() const noexcept {
        return !sampler_live_ && !buffer_live_ && textures_live_ == 0;
    }

    static void* handle(std::uintptr_t value) {
        return reinterpret_cast<void*>(value);
    }

    static std::uintptr_t value(void* handle_value) {
        return reinterpret_cast<std::uintptr_t>(handle_value);
    }

    Failure failure_;
    bool sampler_live_{};
    bool buffer_live_{};
    std::uintptr_t textures_live_{};
    std::vector<std::string> events;
    std::vector<TextureRecord> textures;
    std::vector<BufferRecord> buffers;
};

ryn::font::GlyphBitmap bitmap(
    std::uint32_t width,
    std::uint32_t height,
    std::uint8_t seed) {
    ryn::font::GlyphBitmap result;
    result.width = width;
    result.height = height;
    result.row_stride = width;
    result.coverage.resize(static_cast<std::size_t>(width) * height);
    for (std::size_t index = 0; index < result.coverage.size(); ++index) {
        result.coverage[index] = static_cast<std::uint8_t>(seed + index);
    }
    return result;
}

ryn::graphics::GlyphAtlasKey key(std::uint32_t glyph) {
    return {{0, 1}, glyph, 14, ryn::font::GlyphRasterMode::grayscale};
}

ryn::graphics::GlyphInstance instance(float marker) {
    ryn::graphics::GlyphInstance result;
    result.position_size[0] = marker;
    return result;
}

void test_aligned_dirty_texture_and_sparse_buffer_uploads() {
    RecordingGpuApi api;
    {
        ryn::detail::GlyphGpuResources resources(api);
        ryn::graphics::GlyphAtlas atlas({10, 10, 2});
        require(static_cast<bool>(atlas.insert(key(1), bitmap(3, 2, 10))),
                "first glyph insert failed");
        require(static_cast<bool>(atlas.insert(key(2), bitmap(3, 2, 30))),
                "second glyph insert failed");
        ryn::graphics::GlyphInstanceStore instances;
        const std::array initial{instance(1.0F), instance(2.0F), instance(3.0F)};
        static_cast<void>(instances.append(initial));

        resources.synchronize(atlas, instances);
        require(api.textures.size() == 2, "dirty atlas rectangles were not uploaded exactly");
        for (const auto& upload : api.textures) {
            require(upload.offset % ryn::detail::glyph_texture_offset_alignment == 0
                        && upload.row_pitch % ryn::detail::glyph_texture_row_alignment == 0,
                    "texture staging alignment contract was not preserved");
            require(upload.row_pitch == 256
                        && upload.bytes.size()
                            == static_cast<std::size_t>(256) * upload.rectangle.height,
                    "texture staging pitch or allocation differs");
            for (std::uint32_t row = 0; row < upload.rectangle.height; ++row) {
                for (std::uint32_t column = upload.rectangle.width;
                        column < upload.row_pitch; ++column) {
                    require(upload.bytes[static_cast<std::size_t>(row) * upload.row_pitch + column]
                                == std::byte{},
                            "texture row padding was not zero initialized");
                }
            }
        }
        require(atlas.dirty_regions().empty(), "successful atlas upload did not clear dirty state");
        require(api.buffers.size() == 1 && api.buffers.front().offset == 0
                    && api.buffers.front().bytes.size() == 3 * sizeof(ryn::graphics::GlyphInstance),
                "initial Glyph instance upload differs");

        const std::size_t texture_uploads = api.textures.size();
        static_cast<void>(instances.update_material(
            {1, 1}, {0.2F, 0.4F, 0.6F, 0.65F}, 1.0F));
        resources.synchronize(atlas, instances);
        require(api.textures.size() == texture_uploads,
                "Material-only update re-uploaded atlas pixels");
        require(api.buffers.size() == 2
                    && api.buffers.back().offset == sizeof(ryn::graphics::GlyphInstance)
                    && api.buffers.back().bytes.size() == sizeof(ryn::graphics::GlyphInstance),
                "Material-only update did not remain a sparse instance upload");
    }
    require(api.no_leaks(), "Glyph resources leaked after normal teardown");
    require(api.events.size() >= 3
                && api.events[api.events.size() - 3] == "release_texture"
                && api.events[api.events.size() - 2] == "release_buffer"
                && api.events.back() == "release_sampler",
            "Glyph resources were not released in reverse dependency order");
}

void test_failure_paths_keep_dirty_state_and_release_resources() {
    for (const auto failure : {
             RecordingGpuApi::Failure::sampler,
             RecordingGpuApi::Failure::texture,
             RecordingGpuApi::Failure::buffer,
             RecordingGpuApi::Failure::texture_upload,
             RecordingGpuApi::Failure::buffer_upload}) {
        RecordingGpuApi api(failure);
        ryn::graphics::GlyphAtlas atlas({10, 10, 1});
        require(static_cast<bool>(atlas.insert(key(8), bitmap(2, 2, 4))),
                "failure fixture insert failed");
        ryn::graphics::GlyphInstanceStore instances;
        const std::array initial{instance(8.0F)};
        static_cast<void>(instances.append(initial));
        bool failed = false;
        try {
            ryn::detail::GlyphGpuResources resources(api);
            resources.synchronize(atlas, instances);
        } catch (const std::runtime_error&) {
            failed = true;
        }
        require(failed, "injected Glyph GPU failure unexpectedly succeeded");
        require(api.no_leaks(), "injected Glyph GPU failure leaked resources");
        require(!atlas.dirty_regions().empty(),
                "failed Glyph GPU upload discarded atlas dirty state");
    }
}

class RecordingDrawApi final : public ryn::detail::SceneDrawApi {
public:
    void draw_quad(std::uint32_t first, std::uint32_t count) override {
        calls.push_back({ryn::graphics::SceneDrawKind::quad, first, count,
                         ryn::graphics::invalid_glyph_atlas_page});
    }

    void draw_glyph(
        std::uint32_t page,
        std::uint32_t first,
        std::uint32_t count) override {
        calls.push_back({ryn::graphics::SceneDrawKind::glyph, first, count, page});
    }

    void draw_rounded_effect(
        std::uint32_t first,
        std::uint32_t count) override {
        calls.push_back({
            ryn::graphics::SceneDrawKind::rounded_effect,
            first,
            count,
            ryn::graphics::invalid_glyph_atlas_page,
        });
    }

    std::vector<ryn::graphics::SceneDrawCommand> calls;
};

void test_recording_backend_preserves_order_and_page_switches() {
    ryn::graphics::OrderedScene scene;
    scene.append_quad(0, 1);
    scene.append_glyph({0, {0, 2}});
    scene.append_glyph({1, {2, 1}});
    scene.append_quad(4, 1);
    scene.append_command({
        ryn::graphics::SceneDrawKind::rounded_effect,
        8,
        2,
        ryn::graphics::invalid_glyph_atlas_page,
    });
    scene.append_glyph({0, {3, 1}});
    RecordingDrawApi api;
    ryn::detail::draw_ordered_scene(scene, api);
    require(std::ranges::equal(api.calls, scene.commands()),
            "recording renderer changed Scene command order or atlas page binding");
}

void test_zero_effect_scene_does_not_dispatch_effect_pipeline() {
    ryn::graphics::OrderedScene scene;
    scene.append_quad(0, 1);
    scene.append_glyph({0, {0, 1}});
    RecordingDrawApi api;
    ryn::detail::draw_ordered_scene(scene, api);
    require(std::ranges::none_of(api.calls, [](const auto& command) {
        return command.kind == ryn::graphics::SceneDrawKind::rounded_effect;
    }), "zero-effect Scene dispatched the rounded-effect pipeline");
}

} // namespace

int main() {
    try {
        test_aligned_dirty_texture_and_sparse_buffer_uploads();
        test_failure_paths_keep_dirty_state_and_release_resources();
        test_recording_backend_preserves_order_and_page_switches();
        test_zero_effect_scene_does_not_dispatch_effect_pipeline();
    } catch (const std::exception& error) {
        std::cerr << error.what() << '\n';
        return 1;
    }
    return 0;
}
