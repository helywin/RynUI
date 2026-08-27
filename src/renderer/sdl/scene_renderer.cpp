#include "renderer/sdl/scene_renderer.hpp"

#include <SDL3/SDL.h>

#include <array>
#include <cstring>
#include <fstream>
#include <limits>
#include <stdexcept>
#include <vector>

namespace ryn::detail {
namespace {

struct ShaderSelection {
    SDL_GPUShaderFormat format;
    const char* extension;
    const char* name;
};

[[nodiscard]] std::vector<Uint8> read_shader(const std::filesystem::path& path) {
    std::ifstream stream(path, std::ios::binary | std::ios::ate);
    if (!stream) {
        throw std::runtime_error("Unable to open shader: " + path.string());
    }
    const auto size = stream.tellg();
    if (size <= 0) {
        throw std::runtime_error("Shader is empty: " + path.string());
    }
    std::vector<Uint8> bytes(static_cast<std::size_t>(size));
    stream.seekg(0);
    stream.read(
        reinterpret_cast<char*>(bytes.data()),
        static_cast<std::streamsize>(size));
    if (!stream) {
        throw std::runtime_error("Unable to read shader: " + path.string());
    }
    return bytes;
}

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

[[nodiscard]] SDL_GPUShader* create_shader(
    SDL_GPUDevice* device,
    const std::filesystem::path& path,
    SDL_GPUShaderFormat format,
    SDL_GPUShaderStage stage,
    Uint32 sampler_count) {
    const auto code = read_shader(path);
    SDL_GPUShaderCreateInfo info{};
    info.code_size = code.size();
    info.code = code.data();
    info.entrypoint = stage == SDL_GPU_SHADERSTAGE_VERTEX ? "VSMain" : "PSMain";
    info.format = format;
    info.stage = stage;
    info.num_samplers = sampler_count;
    return SDL_CreateGPUShader(device, &info);
}

[[nodiscard]] SDL_GPUColorTargetDescription color_target(
    SDL_GPUDevice* device,
    SDL_Window* window) {
    SDL_GPUColorTargetDescription target{};
    target.format = SDL_GetGPUSwapchainTextureFormat(device, window);
    target.blend_state.src_color_blendfactor = SDL_GPU_BLENDFACTOR_SRC_ALPHA;
    target.blend_state.dst_color_blendfactor = SDL_GPU_BLENDFACTOR_ONE_MINUS_SRC_ALPHA;
    target.blend_state.color_blend_op = SDL_GPU_BLENDOP_ADD;
    target.blend_state.src_alpha_blendfactor = SDL_GPU_BLENDFACTOR_ONE;
    target.blend_state.dst_alpha_blendfactor = SDL_GPU_BLENDFACTOR_ONE_MINUS_SRC_ALPHA;
    target.blend_state.alpha_blend_op = SDL_GPU_BLENDOP_ADD;
    target.blend_state.color_write_mask = static_cast<SDL_GPUColorComponentFlags>(
        SDL_GPU_COLORCOMPONENT_R | SDL_GPU_COLORCOMPONENT_G
        | SDL_GPU_COLORCOMPONENT_B | SDL_GPU_COLORCOMPONENT_A);
    target.blend_state.enable_blend = true;
    target.blend_state.enable_color_write_mask = true;
    return target;
}

template <std::size_t AttributeCount>
[[nodiscard]] SDL_GPUGraphicsPipeline* create_pipeline(
    SDL_GPUDevice* device,
    SDL_Window* window,
    SDL_GPUShader* vertex,
    SDL_GPUShader* fragment,
    Uint32 pitch,
    const std::array<SDL_GPUVertexAttribute, AttributeCount>& attributes) {
    const SDL_GPUVertexBufferDescription buffer_description{
        0,
        pitch,
        SDL_GPU_VERTEXINPUTRATE_INSTANCE,
        0,
    };
    const auto target = color_target(device, window);
    SDL_GPUGraphicsPipelineCreateInfo info{};
    info.vertex_shader = vertex;
    info.fragment_shader = fragment;
    info.vertex_input_state.vertex_buffer_descriptions = &buffer_description;
    info.vertex_input_state.num_vertex_buffers = 1;
    info.vertex_input_state.vertex_attributes = attributes.data();
    info.vertex_input_state.num_vertex_attributes = static_cast<Uint32>(attributes.size());
    info.primitive_type = SDL_GPU_PRIMITIVETYPE_TRIANGLELIST;
    info.rasterizer_state.enable_depth_clip = true;
    info.target_info.color_target_descriptions = &target;
    info.target_info.num_color_targets = 1;
    return SDL_CreateGPUGraphicsPipeline(device, &info);
}

} // namespace

SdlSceneRenderer::SdlSceneRenderer(
    PlatformState& platform,
    const std::filesystem::path& shader_directory)
    : platform_(&platform) {
    auto* device = static_cast<SDL_GPUDevice*>(platform.gpu_device());
    auto* window = static_cast<SDL_Window*>(platform.window());
    const auto selection = select_shader_format(device);
    shader_format_ = selection.name;

    auto build_pipeline = [&](
        const char* name,
        Uint32 pitch,
        const auto& attributes,
        Uint32 fragment_samplers) -> void* {
        auto* vertex = create_shader(
            device,
            shader_directory / (std::string(name) + ".vertex." + selection.extension),
            selection.format,
            SDL_GPU_SHADERSTAGE_VERTEX,
            0);
        if (vertex == nullptr) {
            throw std::runtime_error(sdl_error("Failed to create vertex shader"));
        }
        auto* fragment = create_shader(
            device,
            shader_directory / (std::string(name) + ".fragment." + selection.extension),
            selection.format,
            SDL_GPU_SHADERSTAGE_FRAGMENT,
            fragment_samplers);
        if (fragment == nullptr) {
            SDL_ReleaseGPUShader(device, vertex);
            throw std::runtime_error(sdl_error("Failed to create fragment shader"));
        }
        auto* pipeline = create_pipeline(
            device, window, vertex, fragment, pitch, attributes);
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
    quad_pipeline_ = build_pipeline(
        "quad", sizeof(graphics::QuadInstance), quad_attributes, 0);
    try {
        const std::array glyph_attributes{
            SDL_GPUVertexAttribute{0, 0, SDL_GPU_VERTEXELEMENTFORMAT_FLOAT4, 0},
            SDL_GPUVertexAttribute{1, 0, SDL_GPU_VERTEXELEMENTFORMAT_FLOAT4, 16},
            SDL_GPUVertexAttribute{2, 0, SDL_GPU_VERTEXELEMENTFORMAT_FLOAT4, 32},
            SDL_GPUVertexAttribute{3, 0, SDL_GPU_VERTEXELEMENTFORMAT_FLOAT4, 48},
            SDL_GPUVertexAttribute{4, 0, SDL_GPU_VERTEXELEMENTFORMAT_FLOAT4, 64},
        };
        glyph_pipeline_ = build_pipeline(
            "glyph", sizeof(graphics::GlyphInstance), glyph_attributes, 1);
    } catch (...) {
        SDL_ReleaseGPUGraphicsPipeline(
            device,
            static_cast<SDL_GPUGraphicsPipeline*>(quad_pipeline_));
        quad_pipeline_ = nullptr;
        throw;
    }
}

SdlSceneRenderer::~SdlSceneRenderer() {
    auto* device = static_cast<SDL_GPUDevice*>(platform_->gpu_device());
    if (glyph_pipeline_ != nullptr) {
        SDL_ReleaseGPUGraphicsPipeline(
            device, static_cast<SDL_GPUGraphicsPipeline*>(glyph_pipeline_));
    }
    if (quad_pipeline_ != nullptr) {
        SDL_ReleaseGPUGraphicsPipeline(
            device, static_cast<SDL_GPUGraphicsPipeline*>(quad_pipeline_));
    }
}

void SdlSceneRenderer::attach_scene(
    graphics::QuadGpuBufferHandle quad_buffer,
    GlyphGpuResources& glyph_resources,
    const graphics::OrderedScene& scene) {
    quad_buffer_ = quad_buffer;
    glyph_resources_ = &glyph_resources;
    scene_ = &scene;
}

bool SdlSceneRenderer::resize_window(int width, int height) {
    if (width <= 0 || height <= 0) {
        last_error_ = "Window dimensions must be positive";
        return false;
    }
    if (!SDL_SetWindowSize(
            static_cast<SDL_Window*>(platform_->window()), width, height)) {
        last_error_ = sdl_error("Failed to resize the window");
        return false;
    }
    return true;
}

graphics::QuadGpuBufferHandle SdlSceneRenderer::create_vertex_buffer(std::size_t size) {
    return create_glyph_buffer(size);
}

void SdlSceneRenderer::release_buffer(graphics::QuadGpuBufferHandle buffer) noexcept {
    release_glyph_buffer(buffer);
}

bool SdlSceneRenderer::upload(
    graphics::QuadGpuBufferHandle buffer,
    std::size_t offset,
    std::span<const std::byte> bytes) {
    return upload_buffer(buffer, offset, bytes, "Quad");
}

const char* SdlSceneRenderer::last_error() const noexcept {
    return last_error_.c_str();
}

GlyphGpuSamplerHandle SdlSceneRenderer::create_glyph_sampler() {
    SDL_GPUSamplerCreateInfo info{};
    info.min_filter = SDL_GPU_FILTER_LINEAR;
    info.mag_filter = SDL_GPU_FILTER_LINEAR;
    info.mipmap_mode = SDL_GPU_SAMPLERMIPMAPMODE_NEAREST;
    info.address_mode_u = SDL_GPU_SAMPLERADDRESSMODE_CLAMP_TO_EDGE;
    info.address_mode_v = SDL_GPU_SAMPLERADDRESSMODE_CLAMP_TO_EDGE;
    info.address_mode_w = SDL_GPU_SAMPLERADDRESSMODE_CLAMP_TO_EDGE;
    auto* sampler = SDL_CreateGPUSampler(
        static_cast<SDL_GPUDevice*>(platform_->gpu_device()), &info);
    if (sampler == nullptr) {
        last_error_ = sdl_error("Failed to create Glyph sampler");
    }
    return sampler;
}

GlyphGpuTextureHandle SdlSceneRenderer::create_glyph_texture(
    std::uint32_t width,
    std::uint32_t height) {
    SDL_GPUTextureCreateInfo info{};
    info.type = SDL_GPU_TEXTURETYPE_2D;
    info.format = SDL_GPU_TEXTUREFORMAT_R8_UNORM;
    info.usage = SDL_GPU_TEXTUREUSAGE_SAMPLER;
    info.width = width;
    info.height = height;
    info.layer_count_or_depth = 1;
    info.num_levels = 1;
    info.sample_count = SDL_GPU_SAMPLECOUNT_1;
    auto* texture = SDL_CreateGPUTexture(
        static_cast<SDL_GPUDevice*>(platform_->gpu_device()), &info);
    if (texture == nullptr) {
        last_error_ = sdl_error("Failed to create Glyph atlas texture");
    }
    return texture;
}

GlyphGpuBufferHandle SdlSceneRenderer::create_glyph_buffer(std::size_t size) {
    if (size == 0 || size > std::numeric_limits<Uint32>::max()) {
        last_error_ = "Glyph vertex buffer size is invalid";
        return nullptr;
    }
    const SDL_GPUBufferCreateInfo info{
        SDL_GPU_BUFFERUSAGE_VERTEX,
        static_cast<Uint32>(size),
        0,
    };
    auto* buffer = SDL_CreateGPUBuffer(
        static_cast<SDL_GPUDevice*>(platform_->gpu_device()), &info);
    if (buffer == nullptr) {
        last_error_ = sdl_error("Failed to create Glyph vertex buffer");
    }
    return buffer;
}

bool SdlSceneRenderer::upload_glyph_texture(
    GlyphGpuTextureHandle texture,
    const GlyphTextureUpload& upload) {
    if (!platform_->is_owner_thread() || texture == nullptr || upload.bytes.empty()
            || upload.transfer_offset % glyph_texture_offset_alignment != 0
            || upload.pixels_per_row % glyph_texture_row_alignment != 0) {
        last_error_ = "Glyph texture upload violates owner, handle, or alignment contract";
        return false;
    }
    auto* device = static_cast<SDL_GPUDevice*>(platform_->gpu_device());
    if (upload.bytes.size() > std::numeric_limits<Uint32>::max()) {
        last_error_ = "Glyph texture transfer buffer exceeds uint32_t";
        return false;
    }
    const SDL_GPUTransferBufferCreateInfo transfer_info{
        SDL_GPU_TRANSFERBUFFERUSAGE_UPLOAD,
        static_cast<Uint32>(upload.bytes.size()),
        0,
    };
    auto* transfer = SDL_CreateGPUTransferBuffer(device, &transfer_info);
    if (transfer == nullptr) {
        last_error_ = sdl_error("Failed to create Glyph texture transfer buffer");
        return false;
    }
    void* mapped = SDL_MapGPUTransferBuffer(device, transfer, false);
    if (mapped == nullptr) {
        last_error_ = sdl_error("Failed to map Glyph texture transfer buffer");
        SDL_ReleaseGPUTransferBuffer(device, transfer);
        return false;
    }
    std::memcpy(mapped, upload.bytes.data(), upload.bytes.size());
    SDL_UnmapGPUTransferBuffer(device, transfer);

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
    const SDL_GPUTextureTransferInfo source{
        transfer,
        upload.transfer_offset,
        upload.pixels_per_row,
        upload.rows_per_layer,
    };
    const auto& rectangle = upload.rectangle;
    const SDL_GPUTextureRegion destination{
        static_cast<SDL_GPUTexture*>(texture),
        0,
        0,
        rectangle.x,
        rectangle.y,
        0,
        rectangle.width,
        rectangle.height,
        1,
    };
    SDL_UploadToGPUTexture(pass, &source, &destination, false);
    SDL_EndGPUCopyPass(pass);
    if (!SDL_SubmitGPUCommandBuffer(command)) {
        last_error_ = sdl_error("Failed to submit Glyph texture upload");
        SDL_ReleaseGPUTransferBuffer(device, transfer);
        return false;
    }
    SDL_ReleaseGPUTransferBuffer(device, transfer);
    ++counters_.upload_submissions;
    counters_.uploaded_bytes += upload.bytes.size();
    return true;
}

bool SdlSceneRenderer::upload_glyph_buffer(
    GlyphGpuBufferHandle buffer,
    std::size_t offset,
    std::span<const std::byte> bytes) {
    return upload_buffer(buffer, offset, bytes, "Glyph");
}

void SdlSceneRenderer::release_glyph_buffer(GlyphGpuBufferHandle buffer) noexcept {
    SDL_ReleaseGPUBuffer(
        static_cast<SDL_GPUDevice*>(platform_->gpu_device()),
        static_cast<SDL_GPUBuffer*>(buffer));
}

void SdlSceneRenderer::release_glyph_texture(GlyphGpuTextureHandle texture) noexcept {
    SDL_ReleaseGPUTexture(
        static_cast<SDL_GPUDevice*>(platform_->gpu_device()),
        static_cast<SDL_GPUTexture*>(texture));
}

void SdlSceneRenderer::release_glyph_sampler(GlyphGpuSamplerHandle sampler) noexcept {
    SDL_ReleaseGPUSampler(
        static_cast<SDL_GPUDevice*>(platform_->gpu_device()),
        static_cast<SDL_GPUSampler*>(sampler));
}

const char* SdlSceneRenderer::glyph_gpu_error() const noexcept {
    return last_error();
}

void SdlSceneRenderer::draw_quad(std::uint32_t first, std::uint32_t count) {
    if (active_render_pass_ == nullptr || quad_buffer_ == nullptr) {
        throw std::logic_error("Quad draw resources are not attached");
    }
    auto* pass = static_cast<SDL_GPURenderPass*>(active_render_pass_);
    SDL_BindGPUGraphicsPipeline(
        pass, static_cast<SDL_GPUGraphicsPipeline*>(quad_pipeline_));
    const SDL_GPUBufferBinding binding{
        static_cast<SDL_GPUBuffer*>(quad_buffer_),
        first * static_cast<Uint32>(sizeof(graphics::QuadInstance)),
    };
    SDL_BindGPUVertexBuffers(pass, 0, &binding, 1);
    SDL_DrawGPUPrimitives(pass, graphics::quad_vertex_count, count, 0, 0);
    ++counters_.quad_draws;
}

void SdlSceneRenderer::draw_glyph(
    std::uint32_t atlas_page,
    std::uint32_t first,
    std::uint32_t count) {
    if (active_render_pass_ == nullptr || glyph_resources_ == nullptr
            || glyph_resources_->instance_buffer() == nullptr) {
        throw std::logic_error("Glyph draw resources are not attached");
    }
    auto* pass = static_cast<SDL_GPURenderPass*>(active_render_pass_);
    SDL_BindGPUGraphicsPipeline(
        pass, static_cast<SDL_GPUGraphicsPipeline*>(glyph_pipeline_));
    const SDL_GPUBufferBinding vertex_binding{
        static_cast<SDL_GPUBuffer*>(glyph_resources_->instance_buffer()),
        first * static_cast<Uint32>(sizeof(graphics::GlyphInstance)),
    };
    SDL_BindGPUVertexBuffers(pass, 0, &vertex_binding, 1);
    const SDL_GPUTextureSamplerBinding atlas_binding{
        static_cast<SDL_GPUTexture*>(glyph_resources_->texture(atlas_page)),
        static_cast<SDL_GPUSampler*>(glyph_resources_->sampler()),
    };
    SDL_BindGPUFragmentSamplers(pass, 0, &atlas_binding, 1);
    SDL_DrawGPUPrimitives(pass, graphics::glyph_vertex_count, count, 0, 0);
    ++counters_.atlas_page_bindings;
    ++counters_.glyph_draws;
}

runtime::FrameSubmissionResult SdlSceneRenderer::submit_frame() {
    if (!platform_->is_owner_thread()) {
        last_error_ = "GPU frame work must run on the Window owner thread";
        return runtime::FrameSubmissionResult::failed;
    }
    if (scene_ == nullptr || glyph_resources_ == nullptr) {
        last_error_ = "Ordered Scene is not attached";
        return runtime::FrameSubmissionResult::failed;
    }
    auto* device = static_cast<SDL_GPUDevice*>(platform_->gpu_device());
    auto* command = SDL_AcquireGPUCommandBuffer(device);
    if (command == nullptr) {
        last_error_ = sdl_error("Failed to acquire Scene command buffer");
        return runtime::FrameSubmissionResult::failed;
    }
    ++counters_.command_buffers;
    SDL_GPUTexture* swapchain = nullptr;
    Uint32 width = 0;
    Uint32 height = 0;
    if (!SDL_WaitAndAcquireGPUSwapchainTexture(
            command,
            static_cast<SDL_Window*>(platform_->window()),
            &swapchain,
            &width,
            &height)) {
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
    SDL_GPUColorTargetInfo target{};
    target.texture = swapchain;
    target.clear_color = SDL_FColor{1.0F, 1.0F, 1.0F, 1.0F};
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

const char* SdlSceneRenderer::shader_format() const noexcept {
    return shader_format_.c_str();
}

const SceneRendererCounters& SdlSceneRenderer::counters() const noexcept {
    return counters_;
}

bool SdlSceneRenderer::upload_buffer(
    void* buffer,
    std::size_t offset,
    std::span<const std::byte> bytes,
    const char* label) {
    if (!platform_->is_owner_thread() || buffer == nullptr || bytes.empty()
            || offset > std::numeric_limits<Uint32>::max()
            || bytes.size() > std::numeric_limits<Uint32>::max()) {
        last_error_ = std::string(label) + " buffer upload range is invalid";
        return false;
    }
    auto* device = static_cast<SDL_GPUDevice*>(platform_->gpu_device());
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
    void* mapped = SDL_MapGPUTransferBuffer(device, transfer, false);
    if (mapped == nullptr) {
        last_error_ = sdl_error("Failed to map buffer transfer buffer");
        SDL_ReleaseGPUTransferBuffer(device, transfer);
        return false;
    }
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
