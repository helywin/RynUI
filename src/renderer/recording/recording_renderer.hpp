#pragma once

#include "renderer/common/scene_backend.hpp"

#include <memory>
#include <string>
#include <thread>
#include <vector>

namespace ryn::detail {

enum class RecordingFailure { none, begin, create, upload, commit, upload_exception };

struct RecordedDraw final {
    graphics::SceneDrawCommand command;
    std::vector<std::byte> instance_bytes;
    GlyphGpuTextureHandle texture{};
    std::uint64_t epoch{};
};

struct RecordingCounters final {
    std::uint64_t uploads{};
    std::uint64_t uploaded_bytes{};
    std::uint64_t commits{};
    std::uint64_t cancels{};
    std::uint64_t frames{};
    std::uint64_t deferred{};
    std::uint64_t rejected_releases{};
};

// This is a data/ordering backend, not a rasterizer or a GPU performance model.
class RecordingRenderer final : public SceneBackend {
public:
    explicit RecordingRenderer(SceneBackendCapabilities capabilities = baseline_scene_capabilities())
        : capabilities_(capabilities) {}

    ~RecordingRenderer() override;

    [[nodiscard]] std::uint64_t device_epoch() const noexcept override {
        return epoch_;
    }

    [[nodiscard]] SceneBackendCapabilities capabilities() const noexcept override {
        return capabilities_;
    }

    void fail_next(RecordingFailure failure, std::size_t after = 0) noexcept;
    void reset_device();

    void set_surface_available(bool value) noexcept {
        surface_available_ = value;
    }

    [[nodiscard]] const RecordingCounters& counters() const noexcept {
        return counters_;
    }

    [[nodiscard]] std::span<const RecordedDraw> draws() const noexcept {
        return draws_;
    }

    [[nodiscard]] std::size_t live_resources() const noexcept;
    [[nodiscard]] std::span<const std::byte> buffer_bytes(void* handle) const;
    [[nodiscard]] std::span<const std::byte> texture_bytes(void* handle) const;

    bool begin_upload_batch() override;
    bool finish_upload_batch() override;
    void cancel_upload_batch() noexcept override;
    void* create_vertex_buffer(std::size_t size) override;
    void release_buffer(void* handle) noexcept override;
    bool upload(void* handle, std::size_t offset, std::span<const std::byte> bytes) override;
    void* create_glyph_sampler() override;
    void* create_glyph_texture(std::uint32_t width, std::uint32_t height) override;
    void* create_glyph_buffer(std::size_t size) override;
    bool upload_glyph_texture(void* handle, const GlyphTextureUpload& upload) override;
    bool upload_glyph_buffer(void* handle, std::size_t offset, std::span<const std::byte> bytes) override;
    void release_glyph_buffer(void* handle) noexcept override;
    void release_glyph_texture(void* handle) noexcept override;
    void release_glyph_sampler(void* handle) noexcept override;
    void* create_effect_buffer(std::size_t size) override;
    bool upload_effect_buffer(void* handle, std::size_t offset, std::span<const std::byte> bytes) override;
    void release_effect_buffer(void* handle) noexcept override;

    [[nodiscard]] const char* last_error() const noexcept override {
        return error_.c_str();
    }

    [[nodiscard]] const char* glyph_gpu_error() const noexcept override {
        return last_error();
    }

    [[nodiscard]] const char* effect_gpu_error() const noexcept override {
        return last_error();
    }

    void draw_quad(std::uint32_t first, std::uint32_t count) override;
    void draw_glyph(std::uint32_t page, std::uint32_t first, std::uint32_t count) override;
    void draw_rounded_effect(std::uint32_t first, std::uint32_t count) override;
    runtime::FrameSubmissionResult submit_frame(animation::AnimationTime time) override;

private:
    enum class Kind { quad, glyph, effect, texture, sampler };

    struct Resource final {
        Kind kind;
        std::uint64_t epoch;
        bool live{true};
        std::uint32_t width{};
        std::uint32_t height{};
        std::vector<std::byte> bytes;
    };

    struct Upload final {
        Resource* resource;
        std::size_t offset{};
        graphics::GlyphAtlasRect rectangle{};
        std::uint32_t pitch{};
        std::vector<std::byte> bytes;
    };

    void require_owner() const;
    [[nodiscard]] bool inject(RecordingFailure stage);
    void* create(Kind kind, std::size_t size, std::uint32_t width = 0, std::uint32_t height = 0);
    Resource* find(void* handle) const noexcept;
    Resource& require(void* handle, Kind kind) const;
    void release(void* handle, Kind kind) noexcept;
    bool upload_buffer(void* handle, Kind kind, std::size_t offset, std::span<const std::byte> bytes);
    void record(graphics::SceneDrawKind kind, void* handle, Kind resource_kind, std::uint32_t first,
                std::uint32_t count, std::uint32_t limit, std::size_t stride,
                std::uint32_t page = graphics::invalid_glyph_atlas_page, void* texture = nullptr);

    std::thread::id owner_{std::this_thread::get_id()};
    const SceneBackendCapabilities capabilities_;
    std::uint64_t epoch_{1};
    bool batch_{};
    bool surface_available_{true};
    bool drawing_{};
    RecordingFailure failure_{RecordingFailure::none};
    std::size_t failure_after_{};
    std::string error_;
    std::vector<std::unique_ptr<Resource>> resources_;
    std::vector<Upload> pending_;
    std::vector<RecordedDraw> draws_;
    RecordingCounters counters_;
};

} // namespace ryn::detail
