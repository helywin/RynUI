#include "renderer/sdl/scene_renderer.hpp"
#include "renderer/sdl/glyph_texture_upload_layout.hpp"
#include "renderer/sdl/shader_resources.hpp"

#include <SDL3/SDL.h>

#include <algorithm>
#include <array>
#include <cstring>
#include <limits>
#include <new>
#include <stdexcept>
#include <vector>
#include <utility>

namespace ryn::detail {
namespace {

struct ShaderSelection {
    SDL_GPUShaderFormat format;
    const char* extension;
    const char* name;
};

[[nodiscard]] ShaderSelection select_shader_format(SDL_GPUDevice* device) {
    const auto formats = SDL_GetGPUShaderFormats(device);
    if ((formats & SDL_GPU_SHADERFORMAT_DXIL) != 0) {
        return {SDL_GPU_SHADERFORMAT_DXIL, "dxil", "DXIL"};
    }
    if ((formats & SDL_GPU_SHADERFORMAT_SPIRV) != 0) {
        return {SDL_GPU_SHADERFORMAT_SPIRV, "spv", "SPIR-V"};
    }
    throw std::runtime_error("SDL GPU device supports neither DXIL nor SPIR-V");
}

[[nodiscard]] std::string sdl_error(const char* fallback) {
    const char* error = SDL_GetError();
    return error != nullptr && error[0] != '\0' ? error : fallback;
}

[[nodiscard]] SDL_GPUShader* create_shader(SDL_GPUDevice* device, std::string_view filename, SDL_GPUShaderFormat format,
                                           SDL_GPUShaderStage stage, Uint32 sampler_count, Uint32 uniform_count = 0,
                                           Uint32 storage_buffer_count = 0,
                                           std::span<const std::byte> override_code = {}) {
    const auto code = override_code.empty() ? builtin_shader_bytes(filename) : override_code;
    SDL_GPUShaderCreateInfo info{};
    info.code_size = code.size();
    info.code = reinterpret_cast<const Uint8*>(code.data());
    info.entrypoint = stage == SDL_GPU_SHADERSTAGE_VERTEX ? "VSMain" : "PSMain";
    info.format = format;
    info.stage = stage;
    info.num_samplers = sampler_count;
    info.num_uniform_buffers = uniform_count;
    info.num_storage_buffers = storage_buffer_count;
    return SDL_CreateGPUShader(device, &info);
}

[[nodiscard]] SDL_GPUColorTargetDescription color_target(SDL_GPUDevice* device, SDL_Window* window) {
    SDL_GPUColorTargetDescription target{};
    target.format = SDL_GetGPUSwapchainTextureFormat(device, window);
    target.blend_state.src_color_blendfactor = SDL_GPU_BLENDFACTOR_SRC_ALPHA;
    target.blend_state.dst_color_blendfactor = SDL_GPU_BLENDFACTOR_ONE_MINUS_SRC_ALPHA;
    target.blend_state.color_blend_op = SDL_GPU_BLENDOP_ADD;
    target.blend_state.src_alpha_blendfactor = SDL_GPU_BLENDFACTOR_ONE;
    target.blend_state.dst_alpha_blendfactor = SDL_GPU_BLENDFACTOR_ONE_MINUS_SRC_ALPHA;
    target.blend_state.alpha_blend_op = SDL_GPU_BLENDOP_ADD;
    target.blend_state.color_write_mask = static_cast<SDL_GPUColorComponentFlags>(
        SDL_GPU_COLORCOMPONENT_R | SDL_GPU_COLORCOMPONENT_G | SDL_GPU_COLORCOMPONENT_B | SDL_GPU_COLORCOMPONENT_A);
    target.blend_state.enable_blend = true;
    target.blend_state.enable_color_write_mask = true;
    return target;
}

[[nodiscard]] SDL_GPUGraphicsPipeline* create_pipeline(SDL_GPUDevice* device, SDL_Window* window, SDL_GPUShader* vertex,
                                                       SDL_GPUShader* fragment,
                                                       std::span<const SDL_GPUVertexBufferDescription> buffers,
                                                       std::span<const SDL_GPUVertexAttribute> attributes,
                                                       SDL_GPUTextureFormat format = SDL_GPU_TEXTUREFORMAT_INVALID,
                                                       int blend = 0) {
    auto target = color_target(device, window);
    if (format != SDL_GPU_TEXTUREFORMAT_INVALID) {
        target.format = format;
    }
    if (blend == 1) {
        target.blend_state.src_color_blendfactor = SDL_GPU_BLENDFACTOR_ONE;
    }
    if (blend == 2) {
        target.blend_state.enable_blend = false;
    }
    SDL_GPUGraphicsPipelineCreateInfo info{};
    info.vertex_shader = vertex;
    info.fragment_shader = fragment;
    info.vertex_input_state.vertex_buffer_descriptions = buffers.data();
    info.vertex_input_state.num_vertex_buffers = static_cast<Uint32>(buffers.size());
    info.vertex_input_state.vertex_attributes = attributes.data();
    info.vertex_input_state.num_vertex_attributes = static_cast<Uint32>(attributes.size());
    info.primitive_type = SDL_GPU_PRIMITIVETYPE_TRIANGLELIST;
    info.rasterizer_state.enable_depth_clip = true;
    info.target_info.color_target_descriptions = &target;
    info.target_info.num_color_targets = 1;
    return SDL_CreateGPUGraphicsPipeline(device, &info);
}

} // namespace

SdlSceneRenderer::SdlSceneRenderer(PlatformState& platform, bool debug_mode)
    : platform_(&platform), binding_(platform, debug_mode) {
    auto* device = static_cast<SDL_GPUDevice*>(binding_.device());
    auto* window = static_cast<SDL_Window*>(platform.window());
    capabilities_ = baseline_scene_capabilities();
    capabilities_.effect_paints = false;
    // Integer geometry/union is still a candidate. Numerical certification and
    // actual platform readback must precede production capability opt-in.
    capabilities_.effect_arc_caps = false;
    capabilities_.cap_geometry_abi_version = cap_geometry_abi_version;
    capabilities_.effect_linear_fields = false;
    capabilities_.effect_conic_fields = false;
    capabilities_.effect_underlays = false;
    capabilities_.effect_paint_abi_version = effect_paint_abi_version;
    capabilities_.r8_sampling = SDL_GPUTextureSupportsFormat(device, SDL_GPU_TEXTUREFORMAT_R8_UNORM,
                                                             SDL_GPU_TEXTURETYPE_2D, SDL_GPU_TEXTUREUSAGE_SAMPLER);
    capabilities_.rgba8_sampling = SDL_GPUTextureSupportsFormat(device, SDL_GPU_TEXTUREFORMAT_R8G8B8A8_UNORM,
                                                                SDL_GPU_TEXTURETYPE_2D, SDL_GPU_TEXTUREUSAGE_SAMPLER);
    capabilities_.scene_composition_version = graphics::scene_composition_version;
    capabilities_.rounded_clip_abi_version = rounded_clip_abi_version;
    const bool composition_format =
        SDL_GPUTextureSupportsFormat(device, SDL_GPU_TEXTUREFORMAT_R8G8B8A8_UNORM, SDL_GPU_TEXTURETYPE_2D,
                                     SDL_GPU_TEXTUREUSAGE_COLOR_TARGET | SDL_GPU_TEXTUREUSAGE_SAMPLER);
    capabilities_.masked_groups = composition_format;
    capabilities_.rounded_clip = composition_format;
    capabilities_.cropped_target_coordinates = composition_format;
    capabilities_.premultiplied_targets = composition_format;
    capabilities_.target_sampling = composition_format;
    capabilities_.maximum_composition_target_width = 16384;
    capabilities_.maximum_composition_target_height = 16384;
    capabilities_.maximum_composition_bytes = 512ULL * 1024 * 1024;
    capabilities_.maximum_composition_frames = 2;
    capabilities_.maximum_composition_depth = 32;
    capabilities_.maximum_composition_groups = 4096;
    capabilities_.maximum_composition_ops = 1048576;
    capabilities_.backdrop_filter_abi_version = backdrop_filter_abi_version;
    capabilities_.gaussian_backdrop = composition_format;
    capabilities_.maximum_gaussian_radius = 256;
    capabilities_.maximum_gaussian_weight_bytes = 16ULL * 1024 * 1024;
    validate_scene_capabilities(capabilities_);
    const auto selection = select_shader_format(device);
    shader_format_ = selection.name;

    auto build_pipeline = [&](const char* name, Uint32 pitch, const auto& attributes, Uint32 fragment_samplers,
                              Uint32 vertex_uniforms = 0) -> void* {
        auto* vertex = create_shader(device, std::string(name) + ".vertex." + selection.extension, selection.format,
                                     SDL_GPU_SHADERSTAGE_VERTEX, 0, vertex_uniforms);
        if (vertex == nullptr) {
            throw std::runtime_error(sdl_error("Failed to create vertex shader"));
        }
        auto* fragment = create_shader(device, std::string(name) + ".fragment." + selection.extension, selection.format,
                                       SDL_GPU_SHADERSTAGE_FRAGMENT, fragment_samplers);
        if (fragment == nullptr) {
            SDL_ReleaseGPUShader(device, vertex);
            throw std::runtime_error(sdl_error("Failed to create fragment shader"));
        }
        const std::array buffers{SDL_GPUVertexBufferDescription{0, pitch, SDL_GPU_VERTEXINPUTRATE_INSTANCE, 0}};
        auto* pipeline = create_pipeline(device, window, vertex, fragment, buffers, attributes);
        SDL_ReleaseGPUShader(device, vertex);
        SDL_ReleaseGPUShader(device, fragment);
        if (pipeline == nullptr) {
            throw std::runtime_error(sdl_error("Failed to create graphics pipeline"));
        }
        return pipeline;
    };

    const std::array quad_attributes{
        SDL_GPUVertexAttribute{0, 0, SDL_GPU_VERTEXELEMENTFORMAT_FLOAT4, 0},
        SDL_GPUVertexAttribute{1, 0, SDL_GPU_VERTEXELEMENTFORMAT_FLOAT4, 16},
        SDL_GPUVertexAttribute{2, 0, SDL_GPU_VERTEXELEMENTFORMAT_FLOAT, 32},
        SDL_GPUVertexAttribute{3, 0, SDL_GPU_VERTEXELEMENTFORMAT_FLOAT, 36},
        SDL_GPUVertexAttribute{4, 0, SDL_GPU_VERTEXELEMENTFORMAT_FLOAT2, 40},
    };
    quad_pipeline_ = build_pipeline("quad", sizeof(QuadGpuInstance), quad_attributes, 0, 1);
    try {
        const std::array glyph_attributes{
            SDL_GPUVertexAttribute{0, 0, SDL_GPU_VERTEXELEMENTFORMAT_FLOAT4, 0},
            SDL_GPUVertexAttribute{1, 0, SDL_GPU_VERTEXELEMENTFORMAT_FLOAT4, 16},
            SDL_GPUVertexAttribute{2, 0, SDL_GPU_VERTEXELEMENTFORMAT_FLOAT4, 32},
            SDL_GPUVertexAttribute{3, 0, SDL_GPU_VERTEXELEMENTFORMAT_FLOAT4, 48},
            SDL_GPUVertexAttribute{4, 0, SDL_GPU_VERTEXELEMENTFORMAT_FLOAT4, 64},
            SDL_GPUVertexAttribute{5, 0, SDL_GPU_VERTEXELEMENTFORMAT_FLOAT4, 80},
        };
        glyph_pipeline_ = build_pipeline("glyph", sizeof(GlyphGpuInstance), glyph_attributes, 1);
        const std::array image_attributes{SDL_GPUVertexAttribute{0, 0, SDL_GPU_VERTEXELEMENTFORMAT_FLOAT4, 0},
                                          SDL_GPUVertexAttribute{1, 0, SDL_GPU_VERTEXELEMENTFORMAT_FLOAT4, 16},
                                          SDL_GPUVertexAttribute{2, 0, SDL_GPU_VERTEXELEMENTFORMAT_FLOAT4, 32},
                                          SDL_GPUVertexAttribute{3, 0, SDL_GPU_VERTEXELEMENTFORMAT_FLOAT4, 48},
                                          SDL_GPUVertexAttribute{4, 0, SDL_GPU_VERTEXELEMENTFORMAT_FLOAT4, 64},
                                          SDL_GPUVertexAttribute{5, 0, SDL_GPU_VERTEXELEMENTFORMAT_FLOAT4, 80},
                                          SDL_GPUVertexAttribute{6, 0, SDL_GPU_VERTEXELEMENTFORMAT_FLOAT4, 96}};
        image_pipeline_ = build_pipeline("image", sizeof(ImageGpuInstance), image_attributes, 1);
        image_sampler_ = create_glyph_sampler();
        if (!image_sampler_) {
            throw std::runtime_error("Failed to create Image sampler");
        }
        const std::array effect_attributes{
            SDL_GPUVertexAttribute{0, 0, SDL_GPU_VERTEXELEMENTFORMAT_FLOAT4, 0},
            SDL_GPUVertexAttribute{1, 0, SDL_GPU_VERTEXELEMENTFORMAT_FLOAT4, 16},
            SDL_GPUVertexAttribute{2, 0, SDL_GPU_VERTEXELEMENTFORMAT_FLOAT4, 32},
            SDL_GPUVertexAttribute{3, 0, SDL_GPU_VERTEXELEMENTFORMAT_FLOAT4, 48},
            SDL_GPUVertexAttribute{4, 0, SDL_GPU_VERTEXELEMENTFORMAT_FLOAT4, 64},
            SDL_GPUVertexAttribute{5, 0, SDL_GPU_VERTEXELEMENTFORMAT_FLOAT4, 80},
            SDL_GPUVertexAttribute{6, 0, SDL_GPU_VERTEXELEMENTFORMAT_FLOAT4, 96},
        };
        effect_pipeline_ =
            build_pipeline("rounded_effect", sizeof(detail::RoundedEffectGpuInstance), effect_attributes, 0);
    } catch (...) {
        if (effect_pipeline_) {
            SDL_ReleaseGPUGraphicsPipeline(device, static_cast<SDL_GPUGraphicsPipeline*>(effect_pipeline_));
            effect_pipeline_ = nullptr;
        }
        if (image_pipeline_) {
            SDL_ReleaseGPUGraphicsPipeline(device, static_cast<SDL_GPUGraphicsPipeline*>(image_pipeline_));
            image_pipeline_ = nullptr;
        }
        if (image_sampler_) {
            release_glyph_sampler(image_sampler_);
            image_sampler_ = nullptr;
        }
        if (glyph_pipeline_ != nullptr) {
            SDL_ReleaseGPUGraphicsPipeline(device, static_cast<SDL_GPUGraphicsPipeline*>(glyph_pipeline_));
            glyph_pipeline_ = nullptr;
        }
        SDL_ReleaseGPUGraphicsPipeline(device, static_cast<SDL_GPUGraphicsPipeline*>(quad_pipeline_));
        quad_pipeline_ = nullptr;
        throw;
    }
}

SdlSceneRenderer::~SdlSceneRenderer() {
    cancel_upload_batch();
    for (const auto& [id, fence] : composition_fences_) {
        (void)id;
        auto* native = static_cast<SDL_GPUFence*>(fence);
        SDL_WaitForGPUFences(static_cast<SDL_GPUDevice*>(binding_.device()), true, &native, 1);
        SDL_ReleaseGPUFence(static_cast<SDL_GPUDevice*>(binding_.device()), native);
    }
    for (auto* pipeline : composition_pipelines_) {
        if (pipeline) {
            SDL_ReleaseGPUGraphicsPipeline(static_cast<SDL_GPUDevice*>(binding_.device()),
                                           static_cast<SDL_GPUGraphicsPipeline*>(pipeline));
        }
    }
    if (image_sampler_) {
        release_glyph_sampler(image_sampler_);
    }
    if (image_pipeline_) {
        SDL_ReleaseGPUGraphicsPipeline(static_cast<SDL_GPUDevice*>(binding_.device()),
                                       static_cast<SDL_GPUGraphicsPipeline*>(image_pipeline_));
    }
    auto* device = static_cast<SDL_GPUDevice*>(binding_.device());
    if (effect_paint_pipeline_ != nullptr) {
        SDL_ReleaseGPUGraphicsPipeline(device, static_cast<SDL_GPUGraphicsPipeline*>(effect_paint_pipeline_));
    }
    if (effect_cap_pipeline_ != nullptr) {
        SDL_ReleaseGPUGraphicsPipeline(device, static_cast<SDL_GPUGraphicsPipeline*>(effect_cap_pipeline_));
    }
    if (effect_pipeline_ != nullptr) {
        SDL_ReleaseGPUGraphicsPipeline(device, static_cast<SDL_GPUGraphicsPipeline*>(effect_pipeline_));
    }
    if (glyph_pipeline_ != nullptr) {
        SDL_ReleaseGPUGraphicsPipeline(device, static_cast<SDL_GPUGraphicsPipeline*>(glyph_pipeline_));
    }
    if (quad_pipeline_ != nullptr) {
        SDL_ReleaseGPUGraphicsPipeline(device, static_cast<SDL_GPUGraphicsPipeline*>(quad_pipeline_));
    }
}

bool SdlSceneRenderer::attach_scene(const SceneAttachment& value) noexcept {
    if (!SceneBackend::attach_scene(value)) {
        quad_buffer_ = nullptr;
        glyph_resources_ = nullptr;
        effect_resources_ = nullptr;
        scene_ = nullptr;
        return false;
    }
    quad_buffer_ = value.quads();
    glyph_resources_ = value.glyphs();
    effect_resources_ = value.effects();
    scene_ = value.scene();
    return true;
}

bool SdlSceneRenderer::resize_window(int width, int height) {
    if (width <= 0 || height <= 0) {
        last_error_ = "Window dimensions must be positive";
        return false;
    }
    if (!SDL_SetWindowSize(static_cast<SDL_Window*>(platform_->window()), width, height)) {
        last_error_ = sdl_error("Failed to resize the window");
        return false;
    }
    return true;
}

detail::QuadGpuBufferHandle SdlSceneRenderer::create_vertex_buffer(std::size_t size) {
    auto* handle = create_glyph_buffer(size);
    if (handle) {
        resources_.at(handle)->kind = ResourceKind::quad;
    }
    return handle;
}

void SdlSceneRenderer::release_buffer(detail::QuadGpuBufferHandle buffer) noexcept {
    release_resource(buffer, ResourceKind::quad);
}

bool SdlSceneRenderer::upload(detail::QuadGpuBufferHandle buffer, std::size_t offset,
                              std::span<const std::byte> bytes) {
    auto* owned = resource(buffer, ResourceKind::quad);
    if (!owned || offset > owned->size || bytes.size() > owned->size - offset) {
        last_error_ = "Foreign, stale or out-of-range Quad buffer";
        return false;
    }
    return upload_buffer(owned->native, offset, bytes, "Quad");
}

const char* SdlSceneRenderer::last_error() const noexcept {
    return last_error_.c_str();
}

GlyphGpuSamplerHandle SdlSceneRenderer::create_glyph_sampler() {
    if (!platform_->is_owner_thread()) {
        return nullptr;
    }
    SDL_GPUSamplerCreateInfo info{};
    info.min_filter = SDL_GPU_FILTER_LINEAR;
    info.mag_filter = SDL_GPU_FILTER_LINEAR;
    info.mipmap_mode = SDL_GPU_SAMPLERMIPMAPMODE_NEAREST;
    info.address_mode_u = SDL_GPU_SAMPLERADDRESSMODE_CLAMP_TO_EDGE;
    info.address_mode_v = SDL_GPU_SAMPLERADDRESSMODE_CLAMP_TO_EDGE;
    info.address_mode_w = SDL_GPU_SAMPLERADDRESSMODE_CLAMP_TO_EDGE;
    auto* sampler = SDL_CreateGPUSampler(static_cast<SDL_GPUDevice*>(binding_.device()), &info);
    if (sampler == nullptr) {
        last_error_ = sdl_error("Failed to create Glyph sampler");
    }
    return track_resource(sampler, ResourceKind::sampler);
}

GlyphGpuTextureHandle SdlSceneRenderer::create_glyph_texture(std::uint32_t width, std::uint32_t height) {
    if (!platform_->is_owner_thread() || !width || !height || width > capabilities_.maximum_texture_width ||
        height > capabilities_.maximum_texture_height) {
        last_error_ = "Glyph texture extent exceeds renderer input limit";
        return nullptr;
    }
    SDL_GPUTextureCreateInfo info{};
    info.type = SDL_GPU_TEXTURETYPE_2D;
    info.format = SDL_GPU_TEXTUREFORMAT_R8_UNORM;
    info.usage = SDL_GPU_TEXTUREUSAGE_SAMPLER;
    info.width = width;
    info.height = height;
    info.layer_count_or_depth = 1;
    info.num_levels = 1;
    info.sample_count = SDL_GPU_SAMPLECOUNT_1;
    auto* texture = SDL_CreateGPUTexture(static_cast<SDL_GPUDevice*>(binding_.device()), &info);
    if (texture == nullptr) {
        last_error_ = sdl_error("Failed to create Glyph atlas texture");
    }
    return track_resource(texture, ResourceKind::texture, 0, width, height);
}

GlyphGpuBufferHandle SdlSceneRenderer::create_glyph_buffer(std::size_t size) {
    if (!platform_->is_owner_thread() || size == 0 || size > std::numeric_limits<Uint32>::max()) {
        last_error_ = "Glyph vertex buffer size is invalid";
        return nullptr;
    }
    const SDL_GPUBufferCreateInfo info{
        SDL_GPU_BUFFERUSAGE_VERTEX,
        static_cast<Uint32>(size),
        0,
    };
    auto* buffer = SDL_CreateGPUBuffer(static_cast<SDL_GPUDevice*>(binding_.device()), &info);
    if (buffer == nullptr) {
        last_error_ = sdl_error("Failed to create Glyph vertex buffer");
    }
    return track_resource(buffer, ResourceKind::glyph, size);
}

bool SdlSceneRenderer::upload_glyph_texture(GlyphGpuTextureHandle texture, const GlyphTextureUpload& upload) {
    const auto* owned = resource(texture, ResourceKind::texture);
    if (!platform_->is_owner_thread() || !owned) {
        last_error_ = "Glyph texture upload violates owner or handle contract";
        return false;
    }
    SdlGlyphTextureLayout layout;
    try {
        validate_glyph_texture_upload(upload, owned->width, owned->height);
        layout = sdl_glyph_texture_layout(upload);
    } catch (const std::exception& error) {
        last_error_ = error.what();
        return false;
    }
    texture = owned->native;
    if (upload_batch_active_) {
        if (!flush_buffer_upload_chunk()) {
            cancel_upload_batch();
            return false;
        }
        if (active_texture_transfer_ == nullptr || !texture_layout_.can_fit(layout.byte_count)) {
            if (!flush_texture_upload_chunk() || !begin_texture_upload_chunk(layout.byte_count)) {
                cancel_upload_batch();
                return false;
            }
        }
        const auto source_offset = texture_layout_.append(texture, upload.rectangle, layout.pixels_per_row,
                                                          layout.rows_per_layer, layout.byte_count);
        pack_sdl_glyph_texture_rows(
            upload, {static_cast<std::byte*>(active_texture_mapped_) + source_offset, layout.byte_count});
        return true;
    }
    auto* device = static_cast<SDL_GPUDevice*>(binding_.device());
    const SDL_GPUTransferBufferCreateInfo transfer_info{
        SDL_GPU_TRANSFERBUFFERUSAGE_UPLOAD,
        layout.byte_count,
        0,
    };
    auto* transfer = SDL_CreateGPUTransferBuffer(device, &transfer_info);
    if (transfer == nullptr) {
        last_error_ = sdl_error("Failed to create Glyph texture transfer buffer");
        return false;
    }
    ++counters_.texture_transfer_creations;
    void* mapped = SDL_MapGPUTransferBuffer(device, transfer, false);
    if (mapped == nullptr) {
        last_error_ = sdl_error("Failed to map Glyph texture transfer buffer");
        SDL_ReleaseGPUTransferBuffer(device, transfer);
        return false;
    }
    ++counters_.texture_transfer_maps;
    pack_sdl_glyph_texture_rows(upload, {static_cast<std::byte*>(mapped), layout.byte_count});
    SDL_UnmapGPUTransferBuffer(device, transfer);

    const SDL_GPUTextureTransferInfo source{
        transfer,
        0,
        layout.pixels_per_row,
        layout.rows_per_layer,
    };
    const auto& rectangle = upload.rectangle;
    const SDL_GPUTextureRegion destination{
        static_cast<SDL_GPUTexture*>(texture), 0, 0, rectangle.x, rectangle.y, 0, rectangle.width, rectangle.height, 1,
    };
    auto* command = SDL_AcquireGPUCommandBuffer(device);
    if (command == nullptr) {
        last_error_ = sdl_error("Failed to acquire Glyph texture upload command buffer");
        SDL_ReleaseGPUTransferBuffer(device, transfer);
        return false;
    }
    auto* pass = SDL_BeginGPUCopyPass(command);
    if (pass == nullptr) {
        last_error_ = sdl_error("Failed to begin Glyph texture copy pass");
        SDL_CancelGPUCommandBuffer(command);
        SDL_ReleaseGPUTransferBuffer(device, transfer);
        return false;
    }
    SDL_UploadToGPUTexture(pass, &source, &destination, false);
    SDL_EndGPUCopyPass(pass);
    if (!SDL_SubmitGPUCommandBuffer(command)) {
        last_error_ = sdl_error("Failed to submit Glyph texture upload");
        SDL_ReleaseGPUTransferBuffer(device, transfer);
        return false;
    }
    SDL_ReleaseGPUTransferBuffer(device, transfer);
    ++counters_.upload_submissions;
    counters_.uploaded_bytes += layout.byte_count;
    return true;
}

bool SdlSceneRenderer::upload_glyph_buffer(GlyphGpuBufferHandle buffer, std::size_t offset,
                                           std::span<const std::byte> bytes) {
    auto* owned = resource(buffer, ResourceKind::glyph);
    if (!owned || offset > owned->size || bytes.size() > owned->size - offset) {
        last_error_ = "Foreign, stale or out-of-range Glyph buffer";
        return false;
    }
    return upload_buffer(owned->native, offset, bytes, "Glyph");
}

void SdlSceneRenderer::release_glyph_buffer(GlyphGpuBufferHandle buffer) noexcept {
    release_resource(buffer, ResourceKind::glyph);
}

void SdlSceneRenderer::release_glyph_texture(GlyphGpuTextureHandle texture) noexcept {
    release_resource(texture, ResourceKind::texture);
}

void SdlSceneRenderer::release_glyph_sampler(GlyphGpuSamplerHandle sampler) noexcept {
    release_resource(sampler, ResourceKind::sampler);
}

const char* SdlSceneRenderer::glyph_gpu_error() const noexcept {
    return last_error();
}

RoundedEffectGpuBufferHandle SdlSceneRenderer::create_effect_buffer(std::size_t size) {
    if (!platform_->is_owner_thread() || size == 0 || size > std::numeric_limits<Uint32>::max()) {
        last_error_ = "Rounded effect vertex buffer size is invalid";
        return nullptr;
    }
    const SDL_GPUBufferCreateInfo info{
        SDL_GPU_BUFFERUSAGE_VERTEX,
        static_cast<Uint32>(size),
        0,
    };
    auto* buffer = SDL_CreateGPUBuffer(static_cast<SDL_GPUDevice*>(binding_.device()), &info);
    if (buffer == nullptr) {
        last_error_ = sdl_error("Failed to create rounded-effect vertex buffer");
    }
    return track_resource(buffer, ResourceKind::effect, size);
}

bool SdlSceneRenderer::upload_effect_buffer(RoundedEffectGpuBufferHandle buffer, std::size_t offset,
                                            std::span<const std::byte> bytes) {
    auto* owned = resource(buffer, ResourceKind::effect);
    if (!owned || offset > owned->size || bytes.size() > owned->size - offset) {
        last_error_ = "Foreign, stale or out-of-range effect buffer";
        return false;
    }
    return upload_buffer(owned->native, offset, bytes, "Rounded effect");
}

void SdlSceneRenderer::release_effect_buffer(RoundedEffectGpuBufferHandle buffer) noexcept {
    release_resource(buffer, ResourceKind::effect);
}

const char* SdlSceneRenderer::effect_gpu_error() const noexcept {
    return last_error();
}

bool SdlSceneRenderer::ensure_effect_paint_pipeline(bool cap_geometry) {
    if (cap_geometry) {
        last_error_ = "Cap shader is excluded from this Round transport stage pending numerical certification";
        return false;
    }
    auto& pipeline = cap_geometry ? effect_cap_pipeline_ : effect_paint_pipeline_;
    if (pipeline) {
        return true;
    }
    auto* device = static_cast<SDL_GPUDevice*>(binding_.device());
    const auto selection = select_shader_format(device);
    const std::string shader = cap_geometry ? "rounded_effect_cap" : "rounded_effect_paint";
    auto* vertex = create_shader(device, shader + ".vertex." + selection.extension, selection.format,
                                 SDL_GPU_SHADERSTAGE_VERTEX, 0);
    if (!vertex) {
        last_error_ = sdl_error("Failed to create effect-paint vertex shader");
        return false;
    }
    SDL_GPUShader* fragment = nullptr;
    SDL_GPUGraphicsPipeline* candidate = nullptr;
    try {
        std::span<const std::byte> linear_test_code;
#if defined(RYNUI_SDL_TEST_HOOKS)
        linear_test_code = linear_test_fragment_;
#endif
        fragment = create_shader(device, shader + ".fragment." + selection.extension, selection.format,
                                 SDL_GPU_SHADERSTAGE_FRAGMENT, 0, 0, 2, linear_test_code);
        if (fragment) {
            const std::array buffers{
                SDL_GPUVertexBufferDescription{0, sizeof(RoundedEffectGpuInstance), SDL_GPU_VERTEXINPUTRATE_INSTANCE,
                                               0},
                SDL_GPUVertexBufferDescription{1, sizeof(EffectPaintGpuLink), SDL_GPU_VERTEXINPUTRATE_INSTANCE, 0},
                SDL_GPUVertexBufferDescription{2, sizeof(CapGeometryGpuV1), SDL_GPU_VERTEXINPUTRATE_INSTANCE, 0}};
            const std::array attributes{SDL_GPUVertexAttribute{0, 0, SDL_GPU_VERTEXELEMENTFORMAT_FLOAT4, 0},
                                        SDL_GPUVertexAttribute{1, 0, SDL_GPU_VERTEXELEMENTFORMAT_FLOAT4, 16},
                                        SDL_GPUVertexAttribute{2, 0, SDL_GPU_VERTEXELEMENTFORMAT_FLOAT4, 32},
                                        SDL_GPUVertexAttribute{3, 0, SDL_GPU_VERTEXELEMENTFORMAT_FLOAT4, 48},
                                        SDL_GPUVertexAttribute{4, 0, SDL_GPU_VERTEXELEMENTFORMAT_FLOAT4, 64},
                                        SDL_GPUVertexAttribute{5, 0, SDL_GPU_VERTEXELEMENTFORMAT_FLOAT4, 80},
                                        SDL_GPUVertexAttribute{6, 0, SDL_GPU_VERTEXELEMENTFORMAT_FLOAT4, 96},
                                        SDL_GPUVertexAttribute{7, 1, SDL_GPU_VERTEXELEMENTFORMAT_UINT4, 0},
                                        SDL_GPUVertexAttribute{8, 2, SDL_GPU_VERTEXELEMENTFORMAT_UINT4, 0},
                                        SDL_GPUVertexAttribute{9, 2, SDL_GPU_VERTEXELEMENTFORMAT_UINT4, 16}};
            candidate = create_pipeline(device, static_cast<SDL_Window*>(platform_->window()), vertex, fragment,
                                        std::span(buffers).first(cap_geometry ? 3 : 2),
                                        std::span(attributes).first(cap_geometry ? 10 : 8));
        }
    } catch (...) {
        if (fragment) {
            SDL_ReleaseGPUShader(device, fragment);
        }
        SDL_ReleaseGPUShader(device, vertex);
        throw;
    }
    SDL_ReleaseGPUShader(device, vertex);
    if (fragment) {
        SDL_ReleaseGPUShader(device, fragment);
    }
    if (!candidate) {
        last_error_ = sdl_error("Failed to create effect-paint pipeline");
        return false;
    }
    pipeline = candidate;
    if (cap_geometry) {
        ++counters_.effect_cap_pipeline_creations;
    } else {
        ++counters_.effect_paint_pipeline_creations;
    }
    return true;
}

SdlSceneRenderer::ResourceKind SdlSceneRenderer::effect_paint_resource_kind(EffectPaintBufferKind kind) {
    switch (kind) {
    case EffectPaintBufferKind::link:
        return ResourceKind::effect_link;
    case EffectPaintBufferKind::field:
        return ResourceKind::effect_field;
    case EffectPaintBufferKind::stop:
        return ResourceKind::effect_stop;
    }
    throw std::invalid_argument("Unknown effect-paint buffer kind");
}

RoundedEffectGpuBufferHandle SdlSceneRenderer::create_effect_paint_buffer(EffectPaintBufferKind kind,
                                                                          std::size_t size) {
    const auto resource_kind = effect_paint_resource_kind(kind);
    const auto stride = effect_paint_buffer_stride(kind);
    if (!platform_->is_owner_thread() || size == 0 || size % stride != 0 || size > capabilities_.maximum_buffer_bytes ||
        size > std::numeric_limits<Uint32>::max()) {
        last_error_ = "Effect-paint buffer violates owner, stride or size contract";
        return nullptr;
    }
    if (!ensure_effect_paint_pipeline()) {
        return nullptr;
    }
    const SDL_GPUBufferCreateInfo info{kind == EffectPaintBufferKind::link ? SDL_GPU_BUFFERUSAGE_VERTEX
                                                                           : SDL_GPU_BUFFERUSAGE_GRAPHICS_STORAGE_READ,
                                       static_cast<Uint32>(size), 0};
    auto* buffer = SDL_CreateGPUBuffer(static_cast<SDL_GPUDevice*>(binding_.device()), &info);
    if (!buffer) {
        last_error_ = sdl_error("Failed to create effect-paint buffer");
    }
    return track_resource(buffer, resource_kind, size);
}

bool SdlSceneRenderer::upload_effect_paint_buffer(EffectPaintBufferKind kind, RoundedEffectGpuBufferHandle buffer,
                                                  std::size_t offset, std::span<const std::byte> bytes) {
    auto* owned = resource(buffer, effect_paint_resource_kind(kind));
    if (!owned || offset > owned->size || bytes.size() > owned->size - offset) {
        last_error_ = "Foreign, stale, wrong-kind or out-of-range effect-paint buffer";
        return false;
    }
#if defined(RYNUI_SDL_TEST_HOOKS)
    if (kind == EffectPaintBufferKind::field && std::exchange(fail_effect_field_upload_, false)) {
        last_error_ = "Test-only effect field upload failure";
        return false;
    }
#endif
    return upload_buffer(owned->native, offset, bytes, "Effect paint");
}

void SdlSceneRenderer::release_effect_paint_buffer(EffectPaintBufferKind kind,
                                                   RoundedEffectGpuBufferHandle buffer) noexcept {
    switch (kind) {
    case EffectPaintBufferKind::link:
        release_resource(buffer, ResourceKind::effect_link);
        break;
    case EffectPaintBufferKind::field:
        release_resource(buffer, ResourceKind::effect_field);
        break;
    case EffectPaintBufferKind::stop:
        release_resource(buffer, ResourceKind::effect_stop);
        break;
    }
}

RoundedEffectGpuBufferHandle SdlSceneRenderer::create_cap_geometry_buffer(std::size_t size) {
    if (!platform_->is_owner_thread() || !size || size % sizeof(CapGeometryGpuV1) ||
        size > capabilities_.maximum_buffer_bytes || size > std::numeric_limits<Uint32>::max() ||
        capabilities_.cap_geometry_abi_version != cap_geometry_abi_version) {
        last_error_ = "Cap Geometry buffer violates owner, ABI, stride or size contract";
        return nullptr;
    }
    if (!ensure_effect_paint_pipeline(true)) {
        return nullptr;
    }
    const SDL_GPUBufferCreateInfo info{SDL_GPU_BUFFERUSAGE_VERTEX, static_cast<Uint32>(size), 0};
    auto* buffer = SDL_CreateGPUBuffer(static_cast<SDL_GPUDevice*>(binding_.device()), &info);
    if (!buffer) {
        last_error_ = sdl_error("Failed to create Cap Geometry buffer");
    }
    return track_resource(buffer, ResourceKind::cap_geometry, size);
}

bool SdlSceneRenderer::upload_cap_geometry_buffer(RoundedEffectGpuBufferHandle buffer, std::size_t offset,
                                                  std::span<const std::byte> bytes) {
    auto* owned = resource(buffer, ResourceKind::cap_geometry);
    if (!owned || offset > owned->size || bytes.size() > owned->size - offset) {
        last_error_ = "Foreign, stale, wrong-kind or out-of-range Cap Geometry buffer";
        return false;
    }
#if defined(RYNUI_SDL_TEST_HOOKS)
    if (std::exchange(fail_cap_geometry_upload_, false)) {
        // Exercise cancellation after the preceding four real copies were
        // recorded, without submitting the common transaction.
        if (!flush_buffer_upload_chunk()) {
            return false;
        }
        last_error_ = "Test-only cap geometry upload failure";
        return false;
    }
#endif
    return upload_buffer(owned->native, offset, bytes, "Cap Geometry");
}

void SdlSceneRenderer::release_cap_geometry_buffer(RoundedEffectGpuBufferHandle buffer) noexcept {
    release_resource(buffer, ResourceKind::cap_geometry);
}

SdlSceneRenderer::Resource::~Resource() {
    release();
}

void SdlSceneRenderer::Resource::release() noexcept {
    if (!native) {
        return;
    }
    auto* gpu = static_cast<SDL_GPUDevice*>(device);
    switch (kind) {
    case ResourceKind::sampler:
        SDL_ReleaseGPUSampler(gpu, static_cast<SDL_GPUSampler*>(native));
        break;
    case ResourceKind::texture:
    case ResourceKind::image_texture:
    case ResourceKind::composition_target:
        SDL_ReleaseGPUTexture(gpu, static_cast<SDL_GPUTexture*>(native));
        break;
    case ResourceKind::quad:
    case ResourceKind::glyph:
    case ResourceKind::effect:
    case ResourceKind::effect_link:
    case ResourceKind::effect_field:
    case ResourceKind::effect_stop:
    case ResourceKind::cap_geometry:
    case ResourceKind::image_buffer:
    case ResourceKind::composition_weights:
        SDL_ReleaseGPUBuffer(gpu, static_cast<SDL_GPUBuffer*>(native));
        break;
    }
    native = nullptr;
}

void* SdlSceneRenderer::track_resource(void* native, ResourceKind kind, std::size_t size, std::uint32_t width,
                                       std::uint32_t height) {
    if (!native) {
        return nullptr;
    }
    Resource pending{native, binding_.device(), kind, size, width, height};
    auto owned = std::make_unique<Resource>();
    owned->device = pending.device;
    owned->kind = kind;
    owned->size = size;
    owned->width = width;
    owned->height = height;
    owned->native = std::exchange(pending.native, nullptr);
    auto* handle = owned.get();
    resources_.emplace(handle, std::move(owned));
    return handle;
}

SdlSceneRenderer::Resource* SdlSceneRenderer::resource(void* handle, ResourceKind kind) const noexcept {
    if (!platform_->is_owner_thread()) {
        return nullptr;
    }
    const auto found = resources_.find(handle);
    if (found == resources_.end() || !found->second->native || found->second->kind != kind ||
        found->second->device != binding_.device()) {
        return nullptr;
    }
    return found->second.get();
}

void SdlSceneRenderer::release_resource(void* handle, ResourceKind kind) noexcept {
    if (!platform_->is_owner_thread()) {
        return;
    }
    if (auto* owned = resource(handle, kind)) {
        owned->release();
    }
}

void* SdlSceneRenderer::native_resource(void* handle, ResourceKind kind) const {
    const auto* owned = resource(handle, kind);
    if (!owned) {
        throw std::invalid_argument("Foreign, stale or wrong-kind SDL GPU resource");
    }
    return owned->native;
}

void SdlSceneRenderer::draw_quad(std::uint32_t first, std::uint32_t count) {
    if (active_render_pass_ == nullptr || quad_buffer_ == nullptr) {
        throw std::logic_error("Quad draw resources are not attached");
    }
    auto* pass = static_cast<SDL_GPURenderPass*>(active_render_pass_);
    SDL_BindGPUGraphicsPipeline(
        pass, static_cast<SDL_GPUGraphicsPipeline*>(composition_active_ ? composition_pipelines_[0] : quad_pipeline_));
    const SDL_GPUBufferBinding binding{
        static_cast<SDL_GPUBuffer*>(native_resource(quad_buffer_, ResourceKind::quad)),
        first * static_cast<Uint32>(sizeof(QuadGpuInstance)),
    };
    SDL_BindGPUVertexBuffers(pass, 0, &binding, 1);
    SDL_DrawGPUPrimitives(pass, detail::quad_vertex_count, count, 0, 0);
    ++counters_.quad_draws;
}

void SdlSceneRenderer::draw_glyph(std::uint32_t atlas_page, std::uint32_t first, std::uint32_t count) {
    if (active_render_pass_ == nullptr || glyph_resources_ == nullptr ||
        glyph_resources_->instance_buffer() == nullptr) {
        throw std::logic_error("Glyph draw resources are not attached");
    }
    auto* pass = static_cast<SDL_GPURenderPass*>(active_render_pass_);
    SDL_BindGPUGraphicsPipeline(
        pass, static_cast<SDL_GPUGraphicsPipeline*>(composition_active_ ? composition_pipelines_[1] : glyph_pipeline_));
    const SDL_GPUBufferBinding vertex_binding{
        static_cast<SDL_GPUBuffer*>(native_resource(glyph_resources_->instance_buffer(), ResourceKind::glyph)),
        first * static_cast<Uint32>(sizeof(GlyphGpuInstance)),
    };
    SDL_BindGPUVertexBuffers(pass, 0, &vertex_binding, 1);
    const SDL_GPUTextureSamplerBinding atlas_binding{
        static_cast<SDL_GPUTexture*>(native_resource(glyph_resources_->texture(atlas_page), ResourceKind::texture)),
        static_cast<SDL_GPUSampler*>(native_resource(glyph_resources_->sampler(), ResourceKind::sampler)),
    };
    SDL_BindGPUFragmentSamplers(pass, 0, &atlas_binding, 1);
    SDL_DrawGPUPrimitives(pass, detail::glyph_vertex_count, count, 0, 0);
    ++counters_.atlas_page_bindings;
    ++counters_.glyph_draws;
}

void SdlSceneRenderer::draw_rounded_effect(std::uint32_t first, std::uint32_t count) {
    if (!platform_->is_owner_thread() || !attached_scene_ready() || active_render_pass_ == nullptr ||
        effect_resources_ == nullptr) {
        throw std::logic_error("Rounded effect draw resources are not attached");
    }
    const auto binding = effect_resources_->draw_binding(first, count);
    const auto checked = [&](void* handle, ResourceKind kind, std::size_t offset, std::size_t elements,
                             std::size_t stride) {
        auto* owned = resource(handle, kind);
        if (!owned || offset > owned->size || elements > (owned->size - offset) / stride ||
            offset > std::numeric_limits<Uint32>::max()) {
            throw std::invalid_argument("Effect draw binding violates typed resource or byte range contract");
        }
        return SDL_GPUBufferBinding{static_cast<SDL_GPUBuffer*>(owned->native), static_cast<Uint32>(offset)};
    };
    const auto base =
        checked(binding.base, ResourceKind::effect, binding.base_byte_offset, count, sizeof(RoundedEffectGpuInstance));
    auto* pass = static_cast<SDL_GPURenderPass*>(active_render_pass_);
    if (binding.link) {
        auto* pipeline = composition_active_ ? composition_pipelines_[4]
                                             : (binding.cap_geometry ? effect_cap_pipeline_ : effect_paint_pipeline_);
        if (composition_active_ && binding.cap_geometry) {
            throw std::invalid_argument("Composition does not enable candidate cap geometry");
        }
        if (!pipeline || !binding.field_count || !binding.stop_count) {
            throw std::logic_error("Effect paint pipeline or storage is not ready");
        }
        const auto link = checked(binding.link, ResourceKind::effect_link, binding.link_byte_offset, count,
                                  sizeof(EffectPaintGpuLink));
        const auto field =
            checked(binding.field, ResourceKind::effect_field, 0, binding.field_count, sizeof(EffectPaintGpuField));
        const auto stop =
            checked(binding.stop, ResourceKind::effect_stop, 0, binding.stop_count, sizeof(EffectPaintGpuStop));
        const std::array storage{field.buffer, stop.buffer};
        SDL_BindGPUGraphicsPipeline(pass, static_cast<SDL_GPUGraphicsPipeline*>(pipeline));
        if (binding.cap_geometry) {
            const auto cap = checked(binding.cap_geometry, ResourceKind::cap_geometry, binding.cap_byte_offset, count,
                                     sizeof(CapGeometryGpuV1));
            const std::array streams{base, link, cap};
            SDL_BindGPUVertexBuffers(pass, 0, streams.data(), static_cast<Uint32>(streams.size()));
        } else {
            const std::array streams{base, link};
            SDL_BindGPUVertexBuffers(pass, 0, streams.data(), static_cast<Uint32>(streams.size()));
        }
        SDL_BindGPUFragmentStorageBuffers(pass, 0, storage.data(), static_cast<Uint32>(storage.size()));
    } else {
        SDL_BindGPUGraphicsPipeline(pass, static_cast<SDL_GPUGraphicsPipeline*>(
                                              composition_active_ ? composition_pipelines_[3] : effect_pipeline_));
        SDL_BindGPUVertexBuffers(pass, 0, &base, 1);
    }
    SDL_DrawGPUPrimitives(pass, detail::rounded_effect_vertex_count, count, 0, 0);
    ++counters_.effect_draws;
    counters_.effect_instances += count;
}

runtime::FrameSubmissionResult SdlSceneRenderer::submit_frame(animation::AnimationTime) {
    if (!platform_->is_owner_thread()) {
        last_error_ = "GPU frame work must run on the Window owner thread";
        return runtime::FrameSubmissionResult::failed;
    }
    if (!attached_scene_ready() || upload_batch_active_) {
        last_error_ = "Ordered Scene is not attached";
        return runtime::FrameSubmissionResult::failed;
    }
    if (attachment().composition()) {
        return submit_composition();
    }
    auto* device = static_cast<SDL_GPUDevice*>(binding_.device());
    auto* command = SDL_AcquireGPUCommandBuffer(device);
    if (command == nullptr) {
        last_error_ = sdl_error("Failed to acquire Scene command buffer");
        return runtime::FrameSubmissionResult::failed;
    }
    ++counters_.command_buffers;
    SDL_GPUTexture* swapchain = nullptr;
    Uint32 width = 0;
    Uint32 height = 0;
    if (!SDL_WaitAndAcquireGPUSwapchainTexture(command, static_cast<SDL_Window*>(platform_->window()), &swapchain,
                                               &width, &height)) {
        last_error_ = sdl_error("Failed to acquire Scene swapchain texture");
        SDL_CancelGPUCommandBuffer(command);
        return runtime::FrameSubmissionResult::failed;
    }
    if (swapchain == nullptr) {
        if (!SDL_SubmitGPUCommandBuffer(command)) {
            last_error_ = sdl_error("Failed to submit deferred Scene frame");
            return runtime::FrameSubmissionResult::failed;
        }
        ++counters_.frame_submissions;
        ++counters_.no_texture_frames;
        return runtime::FrameSubmissionResult::deferred;
    }
    const auto viewport = quad_viewport_uniform(width, height);
    SDL_PushGPUVertexUniformData(command, 0, &viewport, sizeof(viewport));
    SDL_GPUColorTargetInfo target{};
    target.texture = swapchain;
    target.clear_color =
        SDL_FColor{clear_color_.red(), clear_color_.green(), clear_color_.blue(), clear_color_.alpha()};
    target.load_op = SDL_GPU_LOADOP_CLEAR;
    target.store_op = SDL_GPU_STOREOP_STORE;
    auto* pass = SDL_BeginGPURenderPass(command, &target, 1, nullptr);
    if (pass == nullptr) {
        last_error_ = sdl_error("Failed to begin Scene render pass");
        SDL_SubmitGPUCommandBuffer(command);
        return runtime::FrameSubmissionResult::failed;
    }
    active_render_pass_ = pass;
    try {
        draw_ordered_scene(*scene_, *this);
    } catch (const std::exception& error) {
        last_error_ = error.what();
        active_render_pass_ = nullptr;
        SDL_EndGPURenderPass(pass);
        SDL_SubmitGPUCommandBuffer(command);
        return runtime::FrameSubmissionResult::failed;
    }
    active_render_pass_ = nullptr;
    SDL_EndGPURenderPass(pass);
    ++counters_.render_passes;
    if (!SDL_SubmitGPUCommandBuffer(command)) {
        last_error_ = sdl_error("Failed to submit Scene frame command buffer");
        return runtime::FrameSubmissionResult::failed;
    }
    ++counters_.frame_submissions;
    return runtime::FrameSubmissionResult::submitted;
}

bool SdlSceneRenderer::read_frame_rgba(SceneRgbaReadback& output) {
    output = {};
    if (!platform_->is_owner_thread() || !attached_scene_ready() || upload_batch_active_) {
        last_error_ = "Frame export requires an attached scene on the Window owner thread";
        return false;
    }
    auto* device = static_cast<SDL_GPUDevice*>(binding_.device());
    const auto metrics = platform_->window_metrics();
    const auto width = static_cast<Uint32>(metrics.pixel_width);
    const auto height = static_cast<Uint32>(metrics.pixel_height);
    if (attachment().composition() && (attachment().composition()->plan.metrics.pixel_width != width ||
                                       attachment().composition()->plan.metrics.pixel_height != height)) {
        last_error_ = "Composition readback requires matching window metrics";
        return false;
    }
    const auto format = attachment().composition()
                            ? SDL_GPU_TEXTUREFORMAT_R8G8B8A8_UNORM
                            : SDL_GetGPUSwapchainTextureFormat(device, static_cast<SDL_Window*>(platform_->window()));
    if (width == 0 || height == 0 || width > 16384 || height > 16384 ||
        (format != SDL_GPU_TEXTUREFORMAT_R8G8B8A8_UNORM && format != SDL_GPU_TEXTUREFORMAT_B8G8R8A8_UNORM)) {
        last_error_ = "Frame export requires an SDR RGBA8 or BGRA8 target of valid size";
        return false;
    }

    struct ReadbackScope final {
        SDL_GPUDevice* device;
        void*& active;
        SDL_GPUTexture* texture{};
        SDL_GPUTransferBuffer* transfer{};
        SDL_GPUCommandBuffer* command{};
        SDL_GPUFence* fence{};
        SDL_GPURenderPass* render{};
        SDL_GPUCopyPass* copy{};
        bool mapped{};

        ~ReadbackScope() {
            active = nullptr;
            if (render) {
                SDL_EndGPURenderPass(render);
            }
            if (copy) {
                SDL_EndGPUCopyPass(copy);
            }
            if (command) {
                SDL_CancelGPUCommandBuffer(command);
            }
            if (mapped) {
                SDL_UnmapGPUTransferBuffer(device, transfer);
            }
            if (fence) {
                SDL_ReleaseGPUFence(device, fence);
            }
            if (transfer) {
                SDL_ReleaseGPUTransferBuffer(device, transfer);
            }
            if (texture) {
                SDL_ReleaseGPUTexture(device, texture);
            }
        }
    };

    ReadbackScope scope{device, active_render_pass_};
    try {
        SceneRgbaReadback candidate{width, height, width * 4U, {}, {}};
        candidate.pixels.resize(static_cast<std::size_t>(candidate.row_pitch) * height);
        SDL_GPUTextureCreateInfo texture_info{};
        texture_info.type = SDL_GPU_TEXTURETYPE_2D;
        texture_info.format = format;
        texture_info.usage = SDL_GPU_TEXTUREUSAGE_COLOR_TARGET;
        texture_info.width = width;
        texture_info.height = height;
        texture_info.layer_count_or_depth = 1;
        texture_info.num_levels = 1;
        texture_info.sample_count = SDL_GPU_SAMPLECOUNT_1;
        if (!attachment().composition()) {
            scope.texture = SDL_CreateGPUTexture(device, &texture_info);
        }
        const Uint32 pitch = (width * 4U + 255U) & ~255U;
        const SDL_GPUTransferBufferCreateInfo transfer_info{SDL_GPU_TRANSFERBUFFERUSAGE_DOWNLOAD, pitch * height, 0};
        scope.transfer = SDL_CreateGPUTransferBuffer(device, &transfer_info);
        scope.command = SDL_AcquireGPUCommandBuffer(device);
        if ((!scope.texture && !attachment().composition()) || !scope.transfer || !scope.command) {
            last_error_ = sdl_error("Failed to allocate frame export resources");
            return false;
        }
        SDL_GPUTexture* rendered = scope.texture;
        if (attachment().composition()) {
            auto& frame = *attachment().composition();
            if (frame.fence && !composition_fence_complete(frame.fence)) {
                throw std::runtime_error("Composition readback awaits prior frame fence");
            }
            execute_composition(scope.command);
            rendered =
                static_cast<SDL_GPUTexture*>(native_resource(frame.targets[0], ResourceKind::composition_target));
        } else {
            const auto viewport = quad_viewport_uniform(width, height);
            SDL_PushGPUVertexUniformData(scope.command, 0, &viewport, sizeof(viewport));
            SDL_GPUColorTargetInfo target{};
            target.texture = scope.texture;
            target.clear_color = {clear_color_.red(), clear_color_.green(), clear_color_.blue(), clear_color_.alpha()};
            target.load_op = SDL_GPU_LOADOP_CLEAR;
            target.store_op = SDL_GPU_STOREOP_STORE;
            scope.render = SDL_BeginGPURenderPass(scope.command, &target, 1, nullptr);
            if (!scope.render) {
                last_error_ = sdl_error("Failed to begin frame export render pass");
                return false;
            }
            active_render_pass_ = scope.render;
            draw_ordered_scene(*scene_, *this);
            active_render_pass_ = nullptr;
            SDL_EndGPURenderPass(scope.render);
            scope.render = nullptr;
        }
        scope.copy = SDL_BeginGPUCopyPass(scope.command);
        if (!scope.copy) {
            last_error_ = sdl_error("Failed to begin frame export readback");
            return false;
        }
        const SDL_GPUTextureRegion source{rendered, 0, 0, 0, 0, 0, width, height, 1};
        const SDL_GPUTextureTransferInfo destination{scope.transfer, 0, pitch / 4U, height};
        SDL_DownloadFromGPUTexture(scope.copy, &source, &destination);
        SDL_EndGPUCopyPass(scope.copy);
        scope.copy = nullptr;
        scope.fence = SDL_SubmitGPUCommandBufferAndAcquireFence(scope.command);
        scope.command = nullptr;
        if (!scope.fence || !SDL_WaitForGPUFences(device, true, &scope.fence, 1)) {
            last_error_ = sdl_error("Failed to complete frame export readback");
            return false;
        }
        // An owner-local serial names this specific submitted fence. It is not a
        // surface submission counter or the address of an SDL object.
        const auto readback_id = ++next_readback_id_;
        const auto* pixels = static_cast<const std::byte*>(SDL_MapGPUTransferBuffer(device, scope.transfer, false));
        if (!pixels) {
            last_error_ = sdl_error("Failed to map frame RGBA readback");
            return false;
        }
        scope.mapped = true;
        for (Uint32 row = 0; row < height; ++row) {
            auto* destination_row = candidate.pixels.data() + static_cast<std::size_t>(row) * candidate.row_pitch;
            std::memcpy(destination_row, pixels + static_cast<std::size_t>(row) * pitch, candidate.row_pitch);
            if (format == SDL_GPU_TEXTUREFORMAT_B8G8R8A8_UNORM) {
                for (Uint32 column = 0; column < width; ++column) {
                    std::swap(destination_row[column * 4U], destination_row[column * 4U + 2U]);
                }
            }
        }
        candidate.evidence = {readback_id,
                              readback_id,
                              attachment().scene_revision(),
                              owner_id(),
                              device_epoch(),
                              {width, height, metrics.display_scale},
                              attachment().metrics(),
                              true,
                              format == SDL_GPU_TEXTUREFORMAT_B8G8R8A8_UNORM};
        output = std::move(candidate);
        return true;
    } catch (const std::exception& error) {
        last_error_ = error.what();
        return false;
    }
}

bool SdlSceneRenderer::save_frame_bmp(const std::filesystem::path& path, SceneReadbackEvidence* evidence) {
    if (evidence) {
        *evidence = {};
    }
    SceneRgbaReadback frame;
    if (!read_frame_rgba(frame)) {
        return false;
    }
    const auto surface = std::unique_ptr<SDL_Surface, decltype(&SDL_DestroySurface)>(
        SDL_CreateSurfaceFrom(static_cast<int>(frame.width), static_cast<int>(frame.height), SDL_PIXELFORMAT_RGBA32,
                              frame.pixels.data(), static_cast<int>(frame.row_pitch)),
        &SDL_DestroySurface);
    const bool saved = surface && SDL_SaveBMP(surface.get(), path.string().c_str());
    if (!saved) {
        last_error_ = sdl_error("Failed to save exported frame");
    } else if (evidence) {
        *evidence = frame.evidence;
    }
    return saved;
}

SceneDeviceDescription SdlSceneRenderer::device_description() const {
    const auto properties = SDL_GetGPUDeviceProperties(static_cast<SDL_GPUDevice*>(binding_.device()));
    return {SDL_GetStringProperty(properties, SDL_PROP_GPU_DEVICE_NAME_STRING, ""),
            SDL_GetStringProperty(properties, SDL_PROP_GPU_DEVICE_DRIVER_NAME_STRING, ""),
            SDL_GetStringProperty(properties, SDL_PROP_GPU_DEVICE_DRIVER_VERSION_STRING, ""),
            SDL_GetStringProperty(properties, SDL_PROP_GPU_DEVICE_DRIVER_INFO_STRING, "")};
}

const char* SdlSceneRenderer::shader_format() const noexcept {
    return shader_format_.c_str();
}

const SceneRendererCounters& SdlSceneRenderer::counters() const noexcept {
    return counters_;
}

bool SdlSceneRenderer::begin_upload_batch() {
    if (!platform_->is_owner_thread() || upload_batch_active_) {
        last_error_ = "Upload batch violates owner or state contract";
        return false;
    }
    upload_layout_.reset();
    texture_layout_.reset();
    upload_batch_active_ = true;
    return true;
}

bool SdlSceneRenderer::finish_upload_batch() {
    if (!platform_->is_owner_thread() || !upload_batch_active_) {
        last_error_ = "Upload batch finish violates owner or state contract";
        return false;
    }
    if (!flush_buffer_upload_chunk() || !flush_texture_upload_chunk()) {
        cancel_upload_batch();
        return false;
    }
    auto* device = static_cast<SDL_GPUDevice*>(binding_.device());
    if (upload_pass_ != nullptr) {
        SDL_EndGPUCopyPass(static_cast<SDL_GPUCopyPass*>(upload_pass_));
        upload_pass_ = nullptr;
    }
#if defined(RYNUI_SDL_TEST_HOOKS)
    if (std::exchange(fail_upload_finish_, false)) {
        cancel_upload_batch();
        last_error_ = "Test-only upload finish failure before submission";
        return false;
    }
#endif
    bool submitted = true;
    if (upload_command_ != nullptr) {
        submitted = SDL_SubmitGPUCommandBuffer(static_cast<SDL_GPUCommandBuffer*>(upload_command_));
        if (submitted) {
            ++counters_.upload_submissions;
        } else {
            last_error_ = sdl_error("Failed to submit upload batch");
        }
        upload_command_ = nullptr;
    }
    for (void* transfer : upload_transfers_) {
        SDL_ReleaseGPUTransferBuffer(device, static_cast<SDL_GPUTransferBuffer*>(transfer));
    }
    upload_transfers_.clear();
    upload_batch_active_ = false;
    return submitted;
}

#if defined(RYNUI_SDL_TEST_HOOKS)
bool SdlSceneRenderer::fail_next_effect_field_upload_for_test() noexcept {
    if (!platform_->is_owner_thread()) {
        return false;
    }
    fail_effect_field_upload_ = true;
    return true;
}

bool SdlSceneRenderer::fail_next_upload_finish_for_test() noexcept {
    if (!platform_->is_owner_thread()) {
        return false;
    }
    fail_upload_finish_ = true;
    return true;
}

bool SdlSceneRenderer::begin_pending_cap_validation_for_test() noexcept {
    if (!platform_->is_owner_thread() || pending_cap_validation_ || pending_paint_validation_ || upload_batch_active_ ||
        active_render_pass_) {
        return false;
    }
    pending_cap_validation_ = true;
    return true;
}

bool SdlSceneRenderer::begin_pending_paint_validation_for_test() noexcept {
    if (!platform_->is_owner_thread() || pending_paint_validation_ || pending_cap_validation_ || upload_batch_active_ ||
        active_render_pass_) {
        return false;
    }
    pending_paint_validation_ = true;
    return true;
}

bool SdlSceneRenderer::use_linear_test_fragment_for_test(std::span<const std::byte> code) {
    if (!platform_->is_owner_thread() || !pending_paint_validation_ || upload_batch_active_ || active_render_pass_ ||
        effect_paint_pipeline_ || !linear_test_fragment_.empty() || code.empty()) {
        return false;
    }
    linear_test_fragment_.assign(code.begin(), code.end());
    return true;
}

bool SdlSceneRenderer::fail_next_cap_geometry_upload_for_test() noexcept {
    if (!platform_->is_owner_thread() || upload_batch_active_ || active_render_pass_) {
        return false;
    }
    fail_cap_geometry_upload_ = true;
    return true;
}

void SdlSceneRenderer::end_pending_paint_validation_for_test() noexcept {
    if (platform_->is_owner_thread()) {
        pending_paint_validation_ = false;
    }
}

void SdlSceneRenderer::end_pending_cap_validation_for_test() noexcept {
    if (platform_->is_owner_thread()) {
        pending_cap_validation_ = false;
    }
}
#endif

void SdlSceneRenderer::cancel_upload_batch() noexcept {
    auto* device = static_cast<SDL_GPUDevice*>(binding_.device());
    if (active_texture_mapped_ != nullptr) {
        SDL_UnmapGPUTransferBuffer(device, static_cast<SDL_GPUTransferBuffer*>(active_texture_transfer_));
        active_texture_mapped_ = nullptr;
    }
    if (active_texture_transfer_ != nullptr) {
        SDL_ReleaseGPUTransferBuffer(device, static_cast<SDL_GPUTransferBuffer*>(active_texture_transfer_));
        active_texture_transfer_ = nullptr;
    }
    if (active_upload_mapped_ != nullptr) {
        SDL_UnmapGPUTransferBuffer(device, static_cast<SDL_GPUTransferBuffer*>(active_upload_transfer_));
        active_upload_mapped_ = nullptr;
    }
    if (active_upload_transfer_ != nullptr) {
        SDL_ReleaseGPUTransferBuffer(device, static_cast<SDL_GPUTransferBuffer*>(active_upload_transfer_));
        active_upload_transfer_ = nullptr;
    }
    if (upload_pass_ != nullptr) {
        SDL_EndGPUCopyPass(static_cast<SDL_GPUCopyPass*>(upload_pass_));
        upload_pass_ = nullptr;
    }
    if (upload_command_ != nullptr) {
        SDL_CancelGPUCommandBuffer(static_cast<SDL_GPUCommandBuffer*>(upload_command_));
        upload_command_ = nullptr;
    }
    for (void* transfer : upload_transfers_) {
        SDL_ReleaseGPUTransferBuffer(device, static_cast<SDL_GPUTransferBuffer*>(transfer));
    }
    upload_transfers_.clear();
    upload_layout_.reset();
    texture_layout_.reset();
    upload_batch_active_ = false;
}

bool SdlSceneRenderer::begin_buffer_upload_chunk(std::uint32_t minimum_capacity) {
    auto* device = static_cast<SDL_GPUDevice*>(binding_.device());
    const auto capacity = std::max(BufferUploadBatchLayout::default_capacity, minimum_capacity);
    const SDL_GPUTransferBufferCreateInfo transfer_info{
        SDL_GPU_TRANSFERBUFFERUSAGE_UPLOAD,
        capacity,
        0,
    };
    auto* transfer = SDL_CreateGPUTransferBuffer(device, &transfer_info);
    if (transfer == nullptr) {
        last_error_ = sdl_error("Failed to create buffer upload chunk");
        return false;
    }
    ++counters_.buffer_transfer_creations;
    void* mapped = SDL_MapGPUTransferBuffer(device, transfer, false);
    if (mapped == nullptr) {
        last_error_ = sdl_error("Failed to map buffer upload chunk");
        SDL_ReleaseGPUTransferBuffer(device, transfer);
        return false;
    }
    ++counters_.buffer_transfer_maps;
    upload_layout_.reset(capacity);
    active_upload_transfer_ = transfer;
    active_upload_mapped_ = mapped;
    return true;
}

bool SdlSceneRenderer::begin_texture_upload_chunk(std::uint32_t minimum_capacity) {
    auto* device = static_cast<SDL_GPUDevice*>(binding_.device());
    const auto capacity = std::max(TextureUploadBatchLayout::default_capacity, minimum_capacity);
    const SDL_GPUTransferBufferCreateInfo transfer_info{
        SDL_GPU_TRANSFERBUFFERUSAGE_UPLOAD,
        capacity,
        0,
    };
    auto* transfer = SDL_CreateGPUTransferBuffer(device, &transfer_info);
    if (transfer == nullptr) {
        last_error_ = sdl_error("Failed to create Glyph texture upload chunk");
        return false;
    }
    ++counters_.texture_transfer_creations;
    void* mapped = SDL_MapGPUTransferBuffer(device, transfer, false);
    if (mapped == nullptr) {
        last_error_ = sdl_error("Failed to map Glyph texture upload chunk");
        SDL_ReleaseGPUTransferBuffer(device, transfer);
        return false;
    }
    ++counters_.texture_transfer_maps;
    texture_layout_.reset(capacity);
    active_texture_transfer_ = transfer;
    active_texture_mapped_ = mapped;
    return true;
}

bool SdlSceneRenderer::ensure_upload_copy_pass() {
    if (upload_pass_ != nullptr) {
        return true;
    }
    auto* device = static_cast<SDL_GPUDevice*>(binding_.device());
    upload_command_ = SDL_AcquireGPUCommandBuffer(device);
    if (upload_command_ == nullptr) {
        last_error_ = sdl_error("Failed to acquire upload batch command buffer");
        return false;
    }
    upload_pass_ = SDL_BeginGPUCopyPass(static_cast<SDL_GPUCommandBuffer*>(upload_command_));
    if (upload_pass_ == nullptr) {
        last_error_ = sdl_error("Failed to begin upload batch copy pass");
        return false;
    }
    return true;
}

bool SdlSceneRenderer::flush_buffer_upload_chunk() {
    if (active_upload_transfer_ == nullptr) {
        return true;
    }
    auto* device = static_cast<SDL_GPUDevice*>(binding_.device());
    auto* transfer = static_cast<SDL_GPUTransferBuffer*>(active_upload_transfer_);
    SDL_UnmapGPUTransferBuffer(device, transfer);
    active_upload_mapped_ = nullptr;
    if (!ensure_upload_copy_pass()) {
        return false;
    }
    try {
        upload_transfers_.push_back(transfer);
    } catch (const std::bad_alloc&) {
        last_error_ = "Failed to record buffer upload chunk";
        return false;
    }
    active_upload_transfer_ = nullptr;
    for (const auto region : upload_layout_.regions()) {
        const SDL_GPUTransferBufferLocation source{transfer, region.source_offset};
        const SDL_GPUBufferRegion destination{
            static_cast<SDL_GPUBuffer*>(region.target),
            region.target_offset,
            region.byte_count,
        };
        SDL_UploadToGPUBuffer(static_cast<SDL_GPUCopyPass*>(upload_pass_), &source, &destination, false);
        ++counters_.buffer_upload_regions;
        counters_.uploaded_bytes += region.byte_count;
    }
    upload_layout_.reset();
    return true;
}

bool SdlSceneRenderer::flush_texture_upload_chunk() {
    if (active_texture_transfer_ == nullptr) {
        return true;
    }
    auto* device = static_cast<SDL_GPUDevice*>(binding_.device());
    auto* transfer = static_cast<SDL_GPUTransferBuffer*>(active_texture_transfer_);
    SDL_UnmapGPUTransferBuffer(device, transfer);
    active_texture_mapped_ = nullptr;
    if (!ensure_upload_copy_pass()) {
        return false;
    }
    try {
        upload_transfers_.push_back(transfer);
    } catch (const std::bad_alloc&) {
        last_error_ = "Failed to record Glyph texture upload chunk";
        return false;
    }
    active_texture_transfer_ = nullptr;
    for (const auto region : texture_layout_.regions()) {
        const SDL_GPUTextureTransferInfo source{
            transfer,
            region.source_offset,
            region.pixels_per_row,
            region.rows_per_layer,
        };
        const auto& rectangle = region.rectangle;
        const SDL_GPUTextureRegion destination{
            static_cast<SDL_GPUTexture*>(region.target),
            0,
            0,
            rectangle.x,
            rectangle.y,
            0,
            rectangle.width,
            rectangle.height,
            1,
        };
        SDL_UploadToGPUTexture(static_cast<SDL_GPUCopyPass*>(upload_pass_), &source, &destination, false);
        counters_.uploaded_bytes += region.byte_count;
    }
    texture_layout_.reset();
    return true;
}

bool SdlSceneRenderer::upload_buffer(void* buffer, std::size_t offset, std::span<const std::byte> bytes,
                                     const char* label) {
    if (!platform_->is_owner_thread() || buffer == nullptr || bytes.empty() ||
        offset > std::numeric_limits<Uint32>::max() || bytes.size() > std::numeric_limits<Uint32>::max() ||
        bytes.size() > std::numeric_limits<Uint32>::max() - offset) {
        last_error_ = std::string(label) + " buffer upload range is invalid";
        return false;
    }
    if (upload_batch_active_) {
        if (!flush_texture_upload_chunk()) {
            cancel_upload_batch();
            return false;
        }
        if (active_upload_transfer_ == nullptr || !upload_layout_.can_fit(bytes.size())) {
            if (!flush_buffer_upload_chunk() || !begin_buffer_upload_chunk(static_cast<Uint32>(bytes.size()))) {
                cancel_upload_batch();
                return false;
            }
        }
        const auto source_offset =
            upload_layout_.append(buffer, static_cast<Uint32>(offset), static_cast<Uint32>(bytes.size()));
        std::memcpy(static_cast<std::byte*>(active_upload_mapped_) + source_offset, bytes.data(), bytes.size());
        return true;
    }
    auto* device = static_cast<SDL_GPUDevice*>(binding_.device());
    const SDL_GPUTransferBufferCreateInfo transfer_info{
        SDL_GPU_TRANSFERBUFFERUSAGE_UPLOAD,
        static_cast<Uint32>(bytes.size()),
        0,
    };
    auto* transfer = SDL_CreateGPUTransferBuffer(device, &transfer_info);
    if (transfer == nullptr) {
        last_error_ = sdl_error("Failed to create buffer transfer buffer");
        return false;
    }
    ++counters_.buffer_transfer_creations;
    void* mapped = SDL_MapGPUTransferBuffer(device, transfer, false);
    if (mapped == nullptr) {
        last_error_ = sdl_error("Failed to map buffer transfer buffer");
        SDL_ReleaseGPUTransferBuffer(device, transfer);
        return false;
    }
    ++counters_.buffer_transfer_maps;
    std::memcpy(mapped, bytes.data(), bytes.size());
    SDL_UnmapGPUTransferBuffer(device, transfer);
    auto* command = SDL_AcquireGPUCommandBuffer(device);
    if (command == nullptr) {
        last_error_ = sdl_error("Failed to acquire buffer upload command buffer");
        SDL_ReleaseGPUTransferBuffer(device, transfer);
        return false;
    }
    auto* pass = SDL_BeginGPUCopyPass(command);
    if (pass == nullptr) {
        last_error_ = sdl_error("Failed to begin buffer copy pass");
        SDL_CancelGPUCommandBuffer(command);
        SDL_ReleaseGPUTransferBuffer(device, transfer);
        return false;
    }
    const SDL_GPUTransferBufferLocation source{transfer, 0};
    const SDL_GPUBufferRegion destination{
        static_cast<SDL_GPUBuffer*>(buffer),
        static_cast<Uint32>(offset),
        static_cast<Uint32>(bytes.size()),
    };
    SDL_UploadToGPUBuffer(pass, &source, &destination, false);
    ++counters_.buffer_upload_regions;
    SDL_EndGPUCopyPass(pass);
    if (!SDL_SubmitGPUCommandBuffer(command)) {
        last_error_ = sdl_error("Failed to submit buffer upload");
        SDL_ReleaseGPUTransferBuffer(device, transfer);
        return false;
    }
    SDL_ReleaseGPUTransferBuffer(device, transfer);
    ++counters_.upload_submissions;
    counters_.uploaded_bytes += bytes.size();
    return true;
}

} // namespace ryn::detail

namespace ryn::detail {
void* SdlSceneRenderer::create_image_texture(std::uint32_t w, std::uint32_t h) {
    if (!platform_->is_owner_thread() || !w || !h || w > capabilities_.maximum_texture_width ||
        h > capabilities_.maximum_texture_height || !capabilities_.rgba8_sampling) {
        return nullptr;
    }
    SDL_GPUTextureCreateInfo info{};
    info.type = SDL_GPU_TEXTURETYPE_2D;
    info.format = SDL_GPU_TEXTUREFORMAT_R8G8B8A8_UNORM;
    info.usage = SDL_GPU_TEXTUREUSAGE_SAMPLER;
    info.width = w;
    info.height = h;
    info.layer_count_or_depth = 1;
    info.num_levels = 1;
    info.sample_count = SDL_GPU_SAMPLECOUNT_1;
    return track_resource(SDL_CreateGPUTexture(static_cast<SDL_GPUDevice*>(binding_.device()), &info),
                          ResourceKind::image_texture, 0, w, h);
}

void* SdlSceneRenderer::create_image_buffer(std::size_t size) {
    auto* h = create_glyph_buffer(size);
    if (h) {
        resources_.at(h)->kind = ResourceKind::image_buffer;
    }
    return h;
}

bool SdlSceneRenderer::upload_image_buffer(void* h, std::size_t offset, std::span<const std::byte> bytes) {
    const auto* r = resource(h, ResourceKind::image_buffer);
    if (!r || offset > r->size || bytes.size() > r->size - offset) {
        return false;
    }
    return upload_buffer(r->native, offset, bytes, "Image");
}

void SdlSceneRenderer::release_image_buffer(void* h) noexcept {
    release_resource(h, ResourceKind::image_buffer);
}

void SdlSceneRenderer::release_image_texture(void* h) noexcept {
    release_resource(h, ResourceKind::image_texture);
}

bool SdlSceneRenderer::upload_image_texture(void* h, const ImageTextureUpload& u) {
    const auto* r = resource(h, ResourceKind::image_texture);
    if (!platform_->is_owner_thread() || !r || !upload_batch_active_) {
        return false;
    }
    try {
        validate_image_texture_upload(u, r->width, r->height);
        const auto pitch = (std::uint64_t(u.width) * 4 + 255) / 256 * 256;
        if (pitch * u.height > std::numeric_limits<Uint32>::max()) {
            throw std::length_error("Image transfer exceeds SDL uint32 limit");
        }
        const auto count = static_cast<Uint32>(pitch * u.height);
        if (!flush_buffer_upload_chunk()) {
            return false;
        }
        if (!active_texture_transfer_ || !texture_layout_.can_fit(count)) {
            if (!flush_texture_upload_chunk() || !begin_texture_upload_chunk(count)) {
                return false;
            }
        }
        const auto offset = texture_layout_.append(r->native, {0, 0, u.width, u.height}, static_cast<Uint32>(pitch / 4),
                                                   u.height, count);
        auto* target = static_cast<std::byte*>(active_texture_mapped_) + offset;
        std::memset(target, 0, count);
        for (std::uint32_t row = 0; row < u.height; ++row) {
            std::memcpy(target + std::size_t(row) * pitch,
                        u.bytes.data() + u.source_offset + std::size_t(row) * u.source_row_pitch,
                        std::size_t(u.width) * 4);
        }
        return true;
    } catch (const std::exception& error) {
        last_error_ = error.what();
        return false;
    }
}

void SdlSceneRenderer::draw_image(std::uint32_t index, std::uint32_t generation) {
    const auto* images = attachment().images();
    if (!active_render_pass_ || !images || !images->drawable(index, generation)) {
        throw std::logic_error("Invalid SDL Image draw");
    }
    auto* pass = static_cast<SDL_GPURenderPass*>(active_render_pass_);
    SDL_BindGPUGraphicsPipeline(
        pass, static_cast<SDL_GPUGraphicsPipeline*>(composition_active_ ? composition_pipelines_[2] : image_pipeline_));
    const SDL_GPUBufferBinding vertex{
        static_cast<SDL_GPUBuffer*>(native_resource(images->buffer(), ResourceKind::image_buffer)),
        index * static_cast<Uint32>(sizeof(ImageGpuInstance))};
    SDL_BindGPUVertexBuffers(pass, 0, &vertex, 1);
    const SDL_GPUTextureSamplerBinding texture{
        static_cast<SDL_GPUTexture*>(native_resource(images->texture(index), ResourceKind::image_texture)),
        static_cast<SDL_GPUSampler*>(native_resource(image_sampler_, ResourceKind::sampler))};
    SDL_BindGPUFragmentSamplers(pass, 0, &texture, 1);
    SDL_DrawGPUPrimitives(pass, 6, 1, 0, 0);
}
} // namespace ryn::detail

namespace ryn::detail {
bool SdlSceneRenderer::ensure_composition_pipelines() {
    if (composition_pipelines_[0] && (composition_pipelines_[4] || !capabilities().effect_paints)) {
        return true;
    }
    auto* device = static_cast<SDL_GPUDevice*>(binding_.device());
    auto* window = static_cast<SDL_Window*>(platform_->window());
    const auto selection = select_shader_format(device);
    auto pending = composition_pipelines_;
    std::array<bool, 8> created{};
    try {
        const std::array names{"quad_composition",
                               "glyph_composition",
                               "image_composition",
                               "rounded_effect_composition",
                               "rounded_effect_paint_composition",
                               "masked_group"};
        for (std::size_t index = 0; index < names.size(); ++index) {
            if (pending[index] || (index == 4 && !capabilities().effect_paints)) {
                continue;
            }
            std::vector<SDL_GPUVertexAttribute> attributes;
            std::vector<SDL_GPUVertexBufferDescription> buffers;
            if (index == 0) {
                attributes = {{0, 0, SDL_GPU_VERTEXELEMENTFORMAT_FLOAT4, 0},
                              {1, 0, SDL_GPU_VERTEXELEMENTFORMAT_FLOAT4, 16},
                              {2, 0, SDL_GPU_VERTEXELEMENTFORMAT_FLOAT, 32},
                              {3, 0, SDL_GPU_VERTEXELEMENTFORMAT_FLOAT, 36},
                              {4, 0, SDL_GPU_VERTEXELEMENTFORMAT_FLOAT2, 40}};
                buffers.push_back({0, 48, SDL_GPU_VERTEXINPUTRATE_INSTANCE, 0});
            } else if (index < 5) {
                const Uint32 count = index == 1 ? 6 : 7;
                for (Uint32 i = 0; i < count; ++i) {
                    attributes.push_back({i, 0, SDL_GPU_VERTEXELEMENTFORMAT_FLOAT4, i * 16});
                }
                buffers.push_back({0, index == 1 ? 96U : 112U, SDL_GPU_VERTEXINPUTRATE_INSTANCE, 0});
                if (index == 4) {
                    attributes.push_back({7, 1, SDL_GPU_VERTEXELEMENTFORMAT_UINT4, 0});
                    buffers.push_back({1, 16, SDL_GPU_VERTEXINPUTRATE_INSTANCE, 0});
                }
            }
            auto* vertex = create_shader(device, std::string(names[index]) + ".vertex." + selection.extension,
                                         selection.format, SDL_GPU_SHADERSTAGE_VERTEX, 0, index == 5 ? 0 : 1);
            if (!vertex) {
                throw std::runtime_error(sdl_error("Composition vertex shader"));
            }
            auto* fragment =
                create_shader(device, std::string(names[index]) + ".fragment." + selection.extension, selection.format,
                              SDL_GPU_SHADERSTAGE_FRAGMENT, (index == 1 || index == 2 || index == 5) ? 1 : 0,
                              index == 5   ? 2
                              : index == 0 ? 2
                              : index == 3 ? 3
                              : index == 4 ? 1
                                           : 0,
                              index == 4 ? 2 : 0);
            if (!fragment) {
                SDL_ReleaseGPUShader(device, vertex);
                throw std::runtime_error(sdl_error("Composition fragment shader"));
            }
            pending[index] = create_pipeline(device, window, vertex, fragment, buffers, attributes,
                                             SDL_GPU_TEXTUREFORMAT_R8G8B8A8_UNORM, index == 5 ? 1 : 0);
            SDL_ReleaseGPUShader(device, vertex);
            SDL_ReleaseGPUShader(device, fragment);
            if (!pending[index]) {
                throw std::runtime_error(sdl_error("Composition pipeline"));
            }
            created[index] = true;
        }
        composition_pipelines_ = pending;
        return true;
    } catch (const std::exception& e) {
        for (std::size_t i = 0; i < pending.size(); ++i) {
            if (created[i]) {
                SDL_ReleaseGPUGraphicsPipeline(device, static_cast<SDL_GPUGraphicsPipeline*>(pending[i]));
            }
        }
        last_error_ = e.what();
        return false;
    }
}

void* SdlSceneRenderer::create_composition_target(CompositionTargetSpec spec) {
    if (!platform_->is_owner_thread() || !spec.width || !spec.height ||
        spec.width > capabilities_.maximum_composition_target_width ||
        spec.height > capabilities_.maximum_composition_target_height || !ensure_composition_pipelines()) {
        return nullptr;
    }
    SDL_GPUTextureCreateInfo info{};
    info.type = SDL_GPU_TEXTURETYPE_2D;
    info.format = SDL_GPU_TEXTUREFORMAT_R8G8B8A8_UNORM;
    info.usage = SDL_GPU_TEXTUREUSAGE_COLOR_TARGET | SDL_GPU_TEXTUREUSAGE_SAMPLER;
    info.width = spec.width;
    info.height = spec.height;
    info.layer_count_or_depth = 1;
    info.num_levels = 1;
    info.sample_count = SDL_GPU_SAMPLECOUNT_1;
    return track_resource(SDL_CreateGPUTexture(static_cast<SDL_GPUDevice*>(binding_.device()), &info),
                          ResourceKind::composition_target, std::size_t(spec.width) * spec.height * 4, spec.width,
                          spec.height);
}

void SdlSceneRenderer::release_composition_target(void* handle) noexcept {
    release_resource(handle, ResourceKind::composition_target);
}

void* SdlSceneRenderer::create_composition_weights(std::size_t size) {
    if (!platform_->is_owner_thread() || !size || size % sizeof(GaussianWeightV1) ||
        size > capabilities_.maximum_gaussian_weight_bytes || size > std::numeric_limits<Uint32>::max() ||
        !ensure_backdrop_pipelines()) {
        return nullptr;
    }
    const SDL_GPUBufferCreateInfo info{SDL_GPU_BUFFERUSAGE_GRAPHICS_STORAGE_READ, static_cast<Uint32>(size), 0};
    return track_resource(SDL_CreateGPUBuffer(static_cast<SDL_GPUDevice*>(binding_.device()), &info),
                          ResourceKind::composition_weights, size);
}

bool SdlSceneRenderer::upload_composition_weights(void* handle, std::span<const std::byte> bytes) {
    auto* owned = resource(handle, ResourceKind::composition_weights);
    if (!owned || bytes.size() != owned->size) {
        last_error_ = "Composition weights violate owner/kind/range";
        return false;
    }
    return upload_buffer(owned->native, 0, bytes, "Composition weights");
}

void SdlSceneRenderer::release_composition_weights(void* handle) noexcept {
    release_resource(handle, ResourceKind::composition_weights);
}

std::uint64_t SdlSceneRenderer::store_composition_fence(void* fence) {
    if (!fence) {
        throw std::runtime_error(sdl_error("Composition submit failed"));
    }
    const auto id = ++next_composition_fence_;
    try {
        composition_fences_.emplace(id, fence);
    } catch (...) {
        auto* native = static_cast<SDL_GPUFence*>(fence);
        SDL_WaitForGPUFences(static_cast<SDL_GPUDevice*>(binding_.device()), true, &native, 1);
        SDL_ReleaseGPUFence(static_cast<SDL_GPUDevice*>(binding_.device()), native);
        throw;
    }
    return id;
}

bool SdlSceneRenderer::composition_fence_complete(std::uint64_t id) {
    if (!platform_->is_owner_thread()) {
        return false;
    }
    if (id <= completed_composition_fence_) {
        return true;
    }
    const auto found = composition_fences_.find(id);
    if (found == composition_fences_.end()) {
        return false;
    }
    auto* device = static_cast<SDL_GPUDevice*>(binding_.device());
    if (!SDL_QueryGPUFence(device, static_cast<SDL_GPUFence*>(found->second))) {
        return false;
    }
    SDL_ReleaseGPUFence(device, static_cast<SDL_GPUFence*>(found->second));
    composition_fences_.erase(found);
    completed_composition_fence_ = id;
    return true;
}

void SdlSceneRenderer::wait_composition_fence(std::uint64_t id) noexcept {
    const auto found = composition_fences_.find(id);
    if (found == composition_fences_.end()) {
        return;
    }
    auto* device = static_cast<SDL_GPUDevice*>(binding_.device());
    auto* fence = static_cast<SDL_GPUFence*>(found->second);
    SDL_WaitForGPUFences(device, true, &fence, 1);
    SDL_ReleaseGPUFence(device, fence);
    composition_fences_.erase(found);
    completed_composition_fence_ = std::max(completed_composition_fence_, id);
}

void SdlSceneRenderer::trim_idle_composition() noexcept {
    auto* device = static_cast<SDL_GPUDevice*>(binding_.device());
    for (auto*& pipeline : composition_pipelines_) {
        if (pipeline) {
            SDL_ReleaseGPUGraphicsPipeline(device, static_cast<SDL_GPUGraphicsPipeline*>(pipeline));
            pipeline = nullptr;
        }
    }
    for (const auto& [id, value] : composition_fences_) {
        (void)id;
        auto* fence = static_cast<SDL_GPUFence*>(value);
        SDL_WaitForGPUFences(device, true, &fence, 1);
        SDL_ReleaseGPUFence(device, fence);
    }
    composition_fences_.clear();
}

void SdlSceneRenderer::execute_composition(void* command_handle) {
    const auto& frame = *attachment().composition();
    const auto& plan = frame.plan;
    auto* command = static_cast<SDL_GPUCommandBuffer*>(command_handle);

    struct Target {
        std::uint32_t index;
        CompositionCrop crop;
    };

    std::vector<Target> stack{{0, {0, 0, plan.metrics.pixel_width, plan.metrics.pixel_height}}};
    auto* pass = static_cast<SDL_GPURenderPass*>(nullptr);
    auto end = [&] {
        if (pass) {
            SDL_EndGPURenderPass(pass);
            pass = nullptr;
            active_render_pass_ = nullptr;
        }
    };
    auto begin = [&](bool clear) {
        SDL_GPUColorTargetInfo target{};
        target.texture = static_cast<SDL_GPUTexture*>(
            native_resource(frame.targets[stack.back().index], ResourceKind::composition_target));
        target.load_op = clear ? SDL_GPU_LOADOP_CLEAR : SDL_GPU_LOADOP_LOAD;
        target.store_op = SDL_GPU_STOREOP_STORE;
        if (stack.back().index == 0) {
            const float a = clear_color_.alpha();
            target.clear_color = {clear_color_.red() * a, clear_color_.green() * a, clear_color_.blue() * a, a};
        }
        pass = SDL_BeginGPURenderPass(command, &target, 1, nullptr);
        if (!pass) {
            throw std::runtime_error(sdl_error("Composition render pass"));
        }
        active_render_pass_ = pass;
        ++counters_.render_passes;
        const auto viewport = composition_viewport(plan.metrics, stack.back().crop);
        SDL_PushGPUVertexUniformData(command, 0, &viewport, sizeof(viewport));
        SDL_PushGPUFragmentUniformData(command, 0, &viewport, sizeof(viewport));
    };
    composition_active_ = true;
    try {
        begin(true);
        for (const auto op : plan.program.operations) {
            if (op.kind == graphics::SceneCompositionOpKind::BeginMaskedGroup) {
                end();
                const auto& request = plan.requests[op.request];
                stack.push_back({request.target, request.crop});
                begin(true);
            } else if (op.kind == graphics::SceneCompositionOpKind::EndMaskedGroup) {
                end();
                const auto child = stack.back();
                stack.pop_back();
                begin(false);
                const auto& clip = plan.requests[op.request].clip;
                SDL_PushGPUFragmentUniformData(command, 1, &clip, sizeof(clip));
                SDL_BindGPUGraphicsPipeline(pass, static_cast<SDL_GPUGraphicsPipeline*>(composition_pipelines_[5]));
                const SDL_GPUTextureSamplerBinding sample{
                    static_cast<SDL_GPUTexture*>(
                        native_resource(frame.targets[child.index], ResourceKind::composition_target)),
                    static_cast<SDL_GPUSampler*>(native_resource(image_sampler_, ResourceKind::sampler))};
                SDL_BindGPUFragmentSamplers(pass, 0, &sample, 1);
                SDL_DrawGPUPrimitives(pass, 3, 1, 0, 0);
            } else if (op.kind == graphics::SceneCompositionOpKind::DrawRange) {
                const DrawCoverageControlV1 control{{static_cast<std::uint32_t>(op.coverage), 0, 0, 0}};
                SDL_PushGPUFragmentUniformData(command, 1, &control, sizeof(control));
                RoundedClipUniformV1 owner{};
                if (stack.size() > 1) {
                    const auto found = std::find_if(plan.requests.begin(), plan.requests.end(),
                                                    [&](const auto& r) { return r.target == stack.back().index; });
                    owner = found->clip;
                }
                SDL_PushGPUFragmentUniformData(command, 2, &owner, sizeof(owner));
                graphics::OrderedScene range;
                for (std::uint32_t i = op.first; i < op.first + op.count; ++i) {
                    range.append_command(scene_->commands()[i]);
                }
                draw_ordered_scene(range, *this);
            } else {
                end();
                execute_backdrop(command, frame, op.request);
                begin(false);
            }
        }
        end();
        composition_active_ = false;
    } catch (...) {
        end();
        composition_active_ = false;
        throw;
    }
}

runtime::FrameSubmissionResult SdlSceneRenderer::submit_composition() {
    auto& frame = *attachment().composition();
    if (frame.fence && !composition_fence_complete(frame.fence)) {
        return runtime::FrameSubmissionResult::deferred;
    }
    auto* device = static_cast<SDL_GPUDevice*>(binding_.device());
    auto* command = SDL_AcquireGPUCommandBuffer(device);
    if (!command) {
        last_error_ = sdl_error("Composition command buffer");
        return runtime::FrameSubmissionResult::failed;
    }
    bool acquired = false;
    try {
        execute_composition(command);
        auto* resolved_root =
            static_cast<SDL_GPUTexture*>(native_resource(frame.targets[0], ResourceKind::composition_target));
        SDL_GPUTexture* swapchain = nullptr;
        Uint32 width = 0;
        Uint32 height = 0;
        if (!SDL_WaitAndAcquireGPUSwapchainTexture(command, static_cast<SDL_Window*>(platform_->window()), &swapchain,
                                                   &width, &height)) {
            throw std::runtime_error(sdl_error("Composition acquire swapchain"));
        }
        acquired = swapchain != nullptr;
        if (swapchain) {
            // All fallible rendering happened before swapchain acquisition.
            SDL_GPUBlitInfo blit{};
            blit.source = {resolved_root, 0, 0, 0, 0, frame.plan.metrics.pixel_width, frame.plan.metrics.pixel_height};
            blit.destination = {swapchain, 0, 0, 0, 0, width, height};
            blit.load_op = SDL_GPU_LOADOP_DONT_CARE;
            blit.filter = SDL_GPU_FILTER_NEAREST;
            SDL_BlitGPUTexture(command, &blit);
        }
        auto* fence = SDL_SubmitGPUCommandBufferAndAcquireFence(command);
        command = nullptr;
        frame.fence = store_composition_fence(fence);
        ++counters_.frame_submissions;
        return swapchain ? runtime::FrameSubmissionResult::submitted : runtime::FrameSubmissionResult::deferred;
    } catch (const std::exception& e) {
        last_error_ = e.what();
        frame.uploaded = false;
        if (command) {
            if (acquired) {
                SDL_SubmitGPUCommandBuffer(command);
            } else {
                SDL_CancelGPUCommandBuffer(command);
            }
        }
        return runtime::FrameSubmissionResult::failed;
    }
}
} // namespace ryn::detail

namespace ryn::detail {
bool SdlSceneRenderer::ensure_backdrop_pipelines() {
    if (composition_pipelines_[6] && composition_pipelines_[7]) {
        return true;
    }
    auto* device = static_cast<SDL_GPUDevice*>(binding_.device());
    auto* window = static_cast<SDL_Window*>(platform_->window());
    const auto selection = select_shader_format(device);
    std::array<void*, 2> pending{};
    try {
        for (unsigned i = 0; i < 2; ++i) {
            const std::string name = i == 0 ? "backdrop_filter" : "backdrop_composite";
            auto* vertex = create_shader(device, name + ".vertex." + selection.extension, selection.format,
                                         SDL_GPU_SHADERSTAGE_VERTEX, 0);
            if (!vertex) {
                throw std::runtime_error(sdl_error("Backdrop vertex shader"));
            }
            auto* fragment = create_shader(device, name + ".fragment." + selection.extension, selection.format,
                                           SDL_GPU_SHADERSTAGE_FRAGMENT, i == 0 ? 1 : 2, 2, i == 0 ? 1 : 0);
            if (!fragment) {
                SDL_ReleaseGPUShader(device, vertex);
                throw std::runtime_error(sdl_error("Backdrop fragment shader"));
            }
            pending[i] =
                create_pipeline(device, window, vertex, fragment, {}, {}, SDL_GPU_TEXTUREFORMAT_R8G8B8A8_UNORM, 2);
            SDL_ReleaseGPUShader(device, vertex);
            SDL_ReleaseGPUShader(device, fragment);
            if (!pending[i]) {
                throw std::runtime_error(sdl_error("Backdrop pipeline"));
            }
        }
        composition_pipelines_[6] = pending[0];
        composition_pipelines_[7] = pending[1];
        return true;
    } catch (const std::exception& e) {
        for (auto* p : pending) {
            if (p) {
                SDL_ReleaseGPUGraphicsPipeline(device, static_cast<SDL_GPUGraphicsPipeline*>(p));
            }
        }
        last_error_ = e.what();
        return false;
    }
}

void SdlSceneRenderer::execute_backdrop(void* handle, const CompositionFrame& frame, std::uint32_t index) {
    auto* command = static_cast<SDL_GPUCommandBuffer*>(handle);
    const auto& plan = frame.plan;
    const auto& request = plan.requests[index];
    const auto texture = [&](std::uint32_t target) {
        return static_cast<SDL_GPUTexture*>(native_resource(frame.targets[target], ResourceKind::composition_target));
    };
    auto* root = texture(0);
    auto* background = texture(plan.snapshot_target);
    auto* horizontal = texture(plan.horizontal_target);
    auto* filtered = texture(plan.filtered_target);
    if (root == background || root == horizontal || root == filtered || background == horizontal ||
        background == filtered || horizontal == filtered) {
        throw std::logic_error("Backdrop texture read/write alias");
    }
    // Snapshot exactly the completed prefix; no dialog/tint/later draw exists yet.
    SDL_GPUBlitInfo copy{};
    copy.source = {root, 0, 0, 0, 0, plan.metrics.pixel_width, plan.metrics.pixel_height};
    copy.destination = {background, 0, 0, 0, 0, plan.metrics.pixel_width, plan.metrics.pixel_height};
    copy.load_op = SDL_GPU_LOADOP_DONT_CARE;
    copy.filter = SDL_GPU_FILTER_NEAREST;
    SDL_BlitGPUTexture(command, &copy);
    const auto viewport = composition_viewport(plan.metrics, request.source_crop);
    SDL_PushGPUFragmentUniformData(command, 0, &viewport, sizeof(viewport));
    auto* sampler = static_cast<SDL_GPUSampler*>(native_resource(image_sampler_, ResourceKind::sampler));
    const auto run = [&](SDL_GPUTexture* destination, SDL_GPUTexture* source, unsigned axis) {
        auto* weights = static_cast<SDL_GPUBuffer*>(native_resource(frame.weights, ResourceKind::composition_weights));
        SDL_GPUColorTargetInfo target{};
        target.texture = destination;
        target.load_op = SDL_GPU_LOADOP_DONT_CARE;
        target.store_op = SDL_GPU_STOREOP_STORE;
        auto* pass = SDL_BeginGPURenderPass(command, &target, 1, nullptr);
        if (!pass) {
            throw std::runtime_error(sdl_error("Gaussian pass"));
        }
        SDL_BindGPUGraphicsPipeline(pass, static_cast<SDL_GPUGraphicsPipeline*>(composition_pipelines_[6]));
        const GaussianFilterUniformV1 filter{
            request.clip.source_origin_extent,
            {axis, request.weight_count - 1, request.first_weight, request.weight_count}};
        SDL_PushGPUFragmentUniformData(command, 1, &filter, sizeof(filter));
        const SDL_GPUTextureSamplerBinding sample{source, sampler};
        SDL_BindGPUFragmentSamplers(pass, 0, &sample, 1);
        SDL_BindGPUFragmentStorageBuffers(pass, 0, &weights, 1);
        SDL_DrawGPUPrimitives(pass, 3, 1, 0, 0);
        SDL_EndGPURenderPass(pass);
        ++counters_.render_passes;
    };
    run(horizontal, background, 0);
    run(filtered, horizontal, 1);
    SDL_GPUColorTargetInfo target{};
    target.texture = root;
    target.load_op = SDL_GPU_LOADOP_LOAD;
    target.store_op = SDL_GPU_STOREOP_STORE;
    auto* pass = SDL_BeginGPURenderPass(command, &target, 1, nullptr);
    if (!pass) {
        throw std::runtime_error(sdl_error("Backdrop replacement pass"));
    }
    SDL_BindGPUGraphicsPipeline(pass, static_cast<SDL_GPUGraphicsPipeline*>(composition_pipelines_[7]));
    SDL_PushGPUFragmentUniformData(command, 1, &request.backdrop, sizeof(request.backdrop));
    const std::array samples{SDL_GPUTextureSamplerBinding{background, sampler},
                             SDL_GPUTextureSamplerBinding{filtered, sampler}};
    SDL_BindGPUFragmentSamplers(pass, 0, samples.data(), 2);
    SDL_DrawGPUPrimitives(pass, 3, 1, 0, 0);
    SDL_EndGPURenderPass(pass);
    ++counters_.render_passes;
}
} // namespace ryn::detail
