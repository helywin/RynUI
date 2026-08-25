#include "renderer/sdl/quad_renderer.hpp"

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

std::vector<Uint8> read_shader(const std::filesystem::path& path) {
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

ShaderSelection select_shader_format(SDL_GPUDevice* device) {
    const auto formats = SDL_GetGPUShaderFormats(device);
    if ((formats & SDL_GPU_SHADERFORMAT_DXIL) != 0) {
        return {SDL_GPU_SHADERFORMAT_DXIL, "dxil", "DXIL"};
    }
    if ((formats & SDL_GPU_SHADERFORMAT_SPIRV) != 0) {
        return {SDL_GPU_SHADERFORMAT_SPIRV, "spv", "SPIR-V"};
    }
    throw std::runtime_error("SDL GPU device supports neither DXIL nor SPIR-V");
}

SDL_GPUShader* create_shader(
    SDL_GPUDevice* device,
    const std::filesystem::path& path,
    const char* entrypoint,
    SDL_GPUShaderFormat format,
    SDL_GPUShaderStage stage) {
    const auto code = read_shader(path);
    SDL_GPUShaderCreateInfo info{};
    info.code_size = code.size();
    info.code = code.data();
    info.entrypoint = entrypoint;
    info.format = format;
    info.stage = stage;
    return SDL_CreateGPUShader(device, &info);
}

std::string sdl_error(const char* fallback) {
    const char* error = SDL_GetError();
    return error != nullptr && error[0] != '\0' ? error : fallback;
}

} // namespace

SdlQuadRenderer::SdlQuadRenderer(
    PlatformState& platform,
    const std::filesystem::path& shader_directory)
    : platform_(&platform) {
    auto* device = static_cast<SDL_GPUDevice*>(platform.gpu_device());
    auto* window = static_cast<SDL_Window*>(platform.window());
    const auto selection = select_shader_format(device);
    shader_format_ = selection.name;

    auto* vertex_shader = create_shader(
        device,
        shader_directory / (std::string("quad.vertex.") + selection.extension),
        "VSMain",
        selection.format,
        SDL_GPU_SHADERSTAGE_VERTEX);
    if (vertex_shader == nullptr) {
        throw std::runtime_error(sdl_error("Failed to create Quad vertex shader"));
    }
    auto* fragment_shader = create_shader(
        device,
        shader_directory / (std::string("quad.fragment.") + selection.extension),
        "PSMain",
        selection.format,
        SDL_GPU_SHADERSTAGE_FRAGMENT);
    if (fragment_shader == nullptr) {
        SDL_ReleaseGPUShader(device, vertex_shader);
        throw std::runtime_error(sdl_error("Failed to create Quad fragment shader"));
    }

    SDL_GPUVertexBufferDescription buffer_description{};
    buffer_description.slot = 0;
    buffer_description.pitch = sizeof(graphics::QuadInstance);
    buffer_description.input_rate = SDL_GPU_VERTEXINPUTRATE_INSTANCE;

    const std::array attributes{
        SDL_GPUVertexAttribute{0, 0, SDL_GPU_VERTEXELEMENTFORMAT_FLOAT4, 0},
        SDL_GPUVertexAttribute{1, 0, SDL_GPU_VERTEXELEMENTFORMAT_FLOAT4, 16},
        SDL_GPUVertexAttribute{2, 0, SDL_GPU_VERTEXELEMENTFORMAT_FLOAT, 32},
        SDL_GPUVertexAttribute{3, 0, SDL_GPU_VERTEXELEMENTFORMAT_FLOAT, 36},
        SDL_GPUVertexAttribute{4, 0, SDL_GPU_VERTEXELEMENTFORMAT_FLOAT2, 40},
    };

    SDL_GPUColorTargetDescription color_target{};
    color_target.format = SDL_GetGPUSwapchainTextureFormat(device, window);
    color_target.blend_state.src_color_blendfactor = SDL_GPU_BLENDFACTOR_SRC_ALPHA;
    color_target.blend_state.dst_color_blendfactor = SDL_GPU_BLENDFACTOR_ONE_MINUS_SRC_ALPHA;
    color_target.blend_state.color_blend_op = SDL_GPU_BLENDOP_ADD;
    color_target.blend_state.src_alpha_blendfactor = SDL_GPU_BLENDFACTOR_ONE;
    color_target.blend_state.dst_alpha_blendfactor = SDL_GPU_BLENDFACTOR_ONE_MINUS_SRC_ALPHA;
    color_target.blend_state.alpha_blend_op = SDL_GPU_BLENDOP_ADD;
    color_target.blend_state.color_write_mask = static_cast<SDL_GPUColorComponentFlags>(
        SDL_GPU_COLORCOMPONENT_R
        | SDL_GPU_COLORCOMPONENT_G
        | SDL_GPU_COLORCOMPONENT_B
        | SDL_GPU_COLORCOMPONENT_A);
    color_target.blend_state.enable_blend = true;
    color_target.blend_state.enable_color_write_mask = true;

    SDL_GPUGraphicsPipelineCreateInfo pipeline_info{};
    pipeline_info.vertex_shader = vertex_shader;
    pipeline_info.fragment_shader = fragment_shader;
    pipeline_info.vertex_input_state.vertex_buffer_descriptions = &buffer_description;
    pipeline_info.vertex_input_state.num_vertex_buffers = 1;
    pipeline_info.vertex_input_state.vertex_attributes = attributes.data();
    pipeline_info.vertex_input_state.num_vertex_attributes =
        static_cast<Uint32>(attributes.size());
    pipeline_info.primitive_type = SDL_GPU_PRIMITIVETYPE_TRIANGLELIST;
    pipeline_info.rasterizer_state.enable_depth_clip = true;
    pipeline_info.target_info.color_target_descriptions = &color_target;
    pipeline_info.target_info.num_color_targets = 1;
    pipeline_ = SDL_CreateGPUGraphicsPipeline(device, &pipeline_info);
    SDL_ReleaseGPUShader(device, vertex_shader);
    SDL_ReleaseGPUShader(device, fragment_shader);
    if (pipeline_ == nullptr) {
        throw std::runtime_error(sdl_error("Failed to create Quad graphics pipeline"));
    }
}

SdlQuadRenderer::~SdlQuadRenderer() {
    if (pipeline_ != nullptr) {
        SDL_ReleaseGPUGraphicsPipeline(
            static_cast<SDL_GPUDevice*>(platform_->gpu_device()),
            static_cast<SDL_GPUGraphicsPipeline*>(pipeline_));
    }
}

void SdlQuadRenderer::attach_scene(
    graphics::QuadGpuBuffer& buffer,
    std::uint32_t instance_count) {
    if (instance_count == 0 || instance_count > buffer.capacity()) {
        throw std::invalid_argument("Quad scene instance count exceeds the GPU buffer");
    }
    scene_buffer_ = buffer.handle();
    instance_count_ = instance_count;
}

graphics::QuadGpuBufferHandle SdlQuadRenderer::create_vertex_buffer(std::size_t size) {
    if (size == 0 || size > std::numeric_limits<Uint32>::max()) {
        last_error_ = "Quad vertex buffer size is invalid";
        return nullptr;
    }
    SDL_GPUBufferCreateInfo info{};
    info.usage = SDL_GPU_BUFFERUSAGE_VERTEX;
    info.size = static_cast<Uint32>(size);
    auto* buffer = SDL_CreateGPUBuffer(
        static_cast<SDL_GPUDevice*>(platform_->gpu_device()),
        &info);
    if (buffer == nullptr) {
        last_error_ = sdl_error("Failed to create Quad vertex buffer");
    }
    return buffer;
}

void SdlQuadRenderer::release_buffer(graphics::QuadGpuBufferHandle buffer) noexcept {
    SDL_ReleaseGPUBuffer(
        static_cast<SDL_GPUDevice*>(platform_->gpu_device()),
        static_cast<SDL_GPUBuffer*>(buffer));
}

bool SdlQuadRenderer::upload(
    graphics::QuadGpuBufferHandle buffer,
    std::size_t offset,
    std::span<const std::byte> bytes) {
    if (!platform_->is_owner_thread()) {
        last_error_ = "GPU uploads must run on the Window owner thread";
        return false;
    }
    if (buffer == nullptr || bytes.empty()
            || offset > std::numeric_limits<Uint32>::max()
            || bytes.size() > std::numeric_limits<Uint32>::max()) {
        last_error_ = "Quad upload range is invalid";
        return false;
    }

    auto* device = static_cast<SDL_GPUDevice*>(platform_->gpu_device());
    SDL_GPUTransferBufferCreateInfo transfer_info{};
    transfer_info.usage = SDL_GPU_TRANSFERBUFFERUSAGE_UPLOAD;
    transfer_info.size = static_cast<Uint32>(bytes.size());
    auto* transfer = SDL_CreateGPUTransferBuffer(device, &transfer_info);
    if (transfer == nullptr) {
        last_error_ = sdl_error("Failed to create Quad transfer buffer");
        return false;
    }

    void* mapped = SDL_MapGPUTransferBuffer(device, transfer, false);
    if (mapped == nullptr) {
        last_error_ = sdl_error("Failed to map Quad transfer buffer");
        SDL_ReleaseGPUTransferBuffer(device, transfer);
        return false;
    }
    std::memcpy(mapped, bytes.data(), bytes.size());
    SDL_UnmapGPUTransferBuffer(device, transfer);

    auto* command = SDL_AcquireGPUCommandBuffer(device);
    if (command == nullptr) {
        last_error_ = sdl_error("Failed to acquire Quad upload command buffer");
        SDL_ReleaseGPUTransferBuffer(device, transfer);
        return false;
    }
    auto* copy_pass = SDL_BeginGPUCopyPass(command);
    if (copy_pass == nullptr) {
        last_error_ = sdl_error("Failed to begin Quad copy pass");
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
    SDL_UploadToGPUBuffer(copy_pass, &source, &destination, false);
    SDL_EndGPUCopyPass(copy_pass);
    if (!SDL_SubmitGPUCommandBuffer(command)) {
        last_error_ = sdl_error("Failed to submit Quad upload command buffer");
        SDL_ReleaseGPUTransferBuffer(device, transfer);
        return false;
    }
    SDL_ReleaseGPUTransferBuffer(device, transfer);
    ++counters_.upload_submissions;
    counters_.uploaded_bytes += bytes.size();
    return true;
}

const char* SdlQuadRenderer::last_error() const noexcept {
    return last_error_.c_str();
}

runtime::FrameSubmissionResult SdlQuadRenderer::submit_frame() {
    if (!platform_->is_owner_thread()) {
        last_error_ = "GPU frame work must run on the Window owner thread";
        return runtime::FrameSubmissionResult::failed;
    }
    if (scene_buffer_ == nullptr || instance_count_ == 0) {
        last_error_ = "Quad scene is not attached";
        return runtime::FrameSubmissionResult::failed;
    }

    auto* device = static_cast<SDL_GPUDevice*>(platform_->gpu_device());
    auto* command = SDL_AcquireGPUCommandBuffer(device);
    if (command == nullptr) {
        last_error_ = sdl_error("Failed to acquire Quad frame command buffer");
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
        last_error_ = sdl_error("Failed to acquire Quad swapchain texture");
        SDL_CancelGPUCommandBuffer(command);
        return runtime::FrameSubmissionResult::failed;
    }
    if (swapchain == nullptr) {
        if (!SDL_SubmitGPUCommandBuffer(command)) {
            last_error_ = sdl_error("Failed to submit deferred Quad frame");
            return runtime::FrameSubmissionResult::failed;
        }
        ++counters_.frame_submissions;
        ++counters_.no_texture_frames;
        return runtime::FrameSubmissionResult::deferred;
    }

    SDL_GPUColorTargetInfo target{};
    target.texture = swapchain;
    target.clear_color = SDL_FColor{0.035F, 0.050F, 0.085F, 1.0F};
    target.load_op = SDL_GPU_LOADOP_CLEAR;
    target.store_op = SDL_GPU_STOREOP_STORE;
    auto* pass = SDL_BeginGPURenderPass(command, &target, 1, nullptr);
    if (pass == nullptr) {
        last_error_ = sdl_error("Failed to begin Quad render pass");
        SDL_SubmitGPUCommandBuffer(command);
        return runtime::FrameSubmissionResult::failed;
    }
    ++counters_.render_passes;

    SDL_BindGPUGraphicsPipeline(pass, static_cast<SDL_GPUGraphicsPipeline*>(pipeline_));
    const SDL_GPUBufferBinding binding{
        static_cast<SDL_GPUBuffer*>(scene_buffer_),
        0,
    };
    SDL_BindGPUVertexBuffers(pass, 0, &binding, 1);
    SDL_DrawGPUPrimitives(
        pass,
        graphics::quad_vertex_count,
        instance_count_,
        0,
        0);
    ++counters_.draw_calls;
    SDL_EndGPURenderPass(pass);

    if (!SDL_SubmitGPUCommandBuffer(command)) {
        last_error_ = sdl_error("Failed to submit Quad frame command buffer");
        return runtime::FrameSubmissionResult::failed;
    }
    ++counters_.frame_submissions;
    return runtime::FrameSubmissionResult::submitted;
}

const char* SdlQuadRenderer::shader_format() const noexcept {
    return shader_format_.c_str();
}

const QuadRendererCounters& SdlQuadRenderer::counters() const noexcept {
    return counters_;
}

} // namespace ryn::detail
