#pragma once
#include "renderer/common/glyph_gpu_resources.hpp"
#include "renderer/common/rounded_effect_gpu_resources.hpp"
#include "renderer/common/quad_gpu_resources.hpp"
#include <stdexcept>

namespace ryn_test::input_component {
struct CountingGpu final : ryn::detail::GlyphGpuApi, ryn::detail::RoundedEffectGpuApi, ryn::detail::QuadUploadApi {
    std::uintptr_t identity{};
    std::size_t texture_uploads{};
    std::size_t effect_uploads{};
    std::size_t glyph_uploads{};
    std::size_t quad_uploads{};
    bool check_ranges{};
    ryn::graphics::GlyphInstanceRange selected;

    void* handle() {
        return reinterpret_cast<void*>(++identity);
    }

    void* create_glyph_sampler() override {
        return handle();
    }

    void* create_glyph_texture(std::uint32_t, std::uint32_t) override {
        return handle();
    }

    void* create_glyph_buffer(std::size_t) override {
        return handle();
    }

    bool upload_glyph_texture(void*, const ryn::detail::GlyphTextureUpload&) override {
        ++texture_uploads;
        return true;
    }

    bool upload_glyph_buffer(void*, std::size_t offset, std::span<const std::byte> bytes) override {
        if (check_ranges &&
            (offset < selected.first * sizeof(ryn::detail::GlyphGpuInstance) ||
             offset + bytes.size() > (selected.first + selected.count) * sizeof(ryn::detail::GlyphGpuInstance))) {
            throw std::runtime_error("benchmark glyph upload escaped target selected range");
        }
        ++glyph_uploads;
        return true;
    }

    void release_glyph_buffer(void*) noexcept override {}

    void release_glyph_texture(void*) noexcept override {}

    void release_glyph_sampler(void*) noexcept override {}

    const char* glyph_gpu_error() const noexcept override {
        return "";
    }

    void* create_effect_buffer(std::size_t) override {
        return handle();
    }

    bool upload_effect_buffer(void*, std::size_t, std::span<const std::byte>) override {
        ++effect_uploads;
        return true;
    }

    void release_effect_buffer(void*) noexcept override {}

    const char* effect_gpu_error() const noexcept override {
        return "";
    }

    void* create_vertex_buffer(std::size_t) override {
        return handle();
    }

    void release_buffer(void*) noexcept override {}

    bool upload(void*, std::size_t offset, std::span<const std::byte> bytes) override {
        if (check_ranges && offset + bytes.size() > 3 * sizeof(ryn::detail::QuadGpuInstance)) {
            throw std::runtime_error("benchmark quad upload escaped target Input range");
        }
        ++quad_uploads;
        return true;
    }

    const char* last_error() const noexcept override {
        return "";
    }
};
} // namespace ryn_test::input_component
