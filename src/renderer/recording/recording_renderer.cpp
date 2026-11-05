#include "renderer/recording/recording_renderer.hpp"

#include <algorithm>
#include <cstring>
#include <limits>
#include <stdexcept>

namespace ryn::detail {

RecordingRenderer::~RecordingRenderer() = default;

void RecordingRenderer::require_owner() const {
    if (owner_ != std::this_thread::get_id()) {
        throw std::logic_error("Recording renderer requires its owner thread");
    }
}

void RecordingRenderer::fail_next(RecordingFailure failure, std::size_t after) noexcept {
    failure_ = failure;
    failure_after_ = after;
}

bool RecordingRenderer::inject(RecordingFailure stage) {
    if (failure_ != stage) {
        return false;
    }
    if (failure_after_) {
        --failure_after_;
        return false;
    }
    failure_ = RecordingFailure::none;
    error_ = "Injected Recording failure";
    return true;
}

void RecordingRenderer::reset_device() {
    require_owner();
    cancel_upload_batch();
    for (auto& resource : resources_) {
        resource->live = false;
        std::vector<std::byte>().swap(resource->bytes);
    }
    ++epoch_;
    draws_.clear();
}

void RecordingRenderer::reset_device(SceneBackendCapabilities capabilities) {
    reset_device();
    capabilities_ = capabilities;
}

bool RecordingRenderer::begin_upload_batch() {
    require_owner();
    if (batch_) {
        error_ = "Recording upload batch already active";
        return false;
    }
    if (inject(RecordingFailure::begin)) {
        return false;
    }
    batch_ = true;
    return true;
}

bool RecordingRenderer::finish_upload_batch() {
    require_owner();
    if (!batch_) {
        error_ = "Recording upload batch absent";
        return false;
    }
    if (inject(RecordingFailure::commit)) {
        return false;
    }
    for (const auto& upload : pending_) {
        if (!upload.resource->live || upload.resource->epoch != epoch_) {
            error_ = "Upload references retired resource";
            return false;
        }
    }
    for (const auto& upload : pending_) {
        auto& target = *upload.resource;
        if (target.kind == Kind::texture || target.kind == Kind::image_texture) {
            for (std::uint32_t row = 0; row < upload.rectangle.height; ++row) {
                const auto bpp = target.kind == Kind::image_texture ? 4U : 1U;
                const auto destination =
                    static_cast<std::size_t>(upload.rectangle.y + row) * target.width + upload.rectangle.x;
                std::memcpy(target.bytes.data() + destination * bpp,
                            upload.bytes.data() + upload.offset + static_cast<std::size_t>(row) * upload.pitch,
                            upload.rectangle.width * bpp);
            }
        } else if (!upload.bytes.empty()) {
            std::memcpy(target.bytes.data() + upload.offset, upload.bytes.data(), upload.bytes.size());
        }
    }
    pending_.clear();
    batch_ = false;
    ++counters_.commits;
    return true;
}

void RecordingRenderer::cancel_upload_batch() noexcept {
    if (batch_) {
        ++counters_.cancels;
    }
    batch_ = false;
    pending_.clear();
}

void* RecordingRenderer::create(Kind kind, std::size_t size, std::uint32_t width, std::uint32_t height) {
    require_owner();
    if (((kind == Kind::texture || kind == Kind::image_texture) &&
         (width > capabilities_.maximum_texture_width || height > capabilities_.maximum_texture_height)) ||
        (kind != Kind::texture && kind != Kind::image_texture && kind != Kind::sampler &&
         (!size || size > capabilities_.maximum_buffer_bytes))) {
        error_ = "Recording resource exceeds renderer input limit";
        return nullptr;
    }
    if (inject(RecordingFailure::create)) {
        return nullptr;
    }
    auto resource = std::make_unique<Resource>();
    resource->kind = kind;
    resource->epoch = epoch_;
    resource->width = width;
    resource->height = height;
    resource->bytes.resize(size);
    auto* handle = resource.get();
    resources_.push_back(std::move(resource));
    return handle;
}

RecordingRenderer::Resource* RecordingRenderer::find(void* handle) const noexcept {
    for (const auto& resource : resources_) {
        if (resource.get() == handle) {
            return resource.get();
        }
    }
    return nullptr;
}

RecordingRenderer::Resource& RecordingRenderer::require(void* handle, Kind kind) const {
    auto* resource = find(handle);
    if (!resource || !resource->live || resource->epoch != epoch_ || resource->kind != kind) {
        throw std::invalid_argument("Foreign, retired, stale epoch or wrong-kind Recording resource");
    }
    return *resource;
}

void RecordingRenderer::release(void* handle, Kind kind) noexcept {
    auto* resource = find(handle);
    if (owner_ != std::this_thread::get_id() || !resource || !resource->live || resource->epoch != epoch_ ||
        resource->kind != kind) {
        ++counters_.rejected_releases;
        return;
    }
    resource->live = false;
    std::vector<std::byte>().swap(resource->bytes);
}

std::size_t RecordingRenderer::live_resources() const noexcept {
    return static_cast<std::size_t>(std::ranges::count_if(resources_, [](const auto& value) { return value->live; }));
}

bool RecordingRenderer::upload_buffer(void* handle, Kind kind, std::size_t offset, std::span<const std::byte> bytes) {
    require_owner();
    try {
        auto& resource = require(handle, kind);
        if (!batch_ || offset > resource.bytes.size() || bytes.size() > resource.bytes.size() - offset) {
            throw std::out_of_range("Recording buffer upload range or batch invalid");
        }
        if (inject(RecordingFailure::upload_exception)) {
            throw std::runtime_error("Injected upload exception");
        }
        if (inject(RecordingFailure::upload)) {
            return false;
        }
        Upload upload{&resource, offset, {}, 0, {bytes.begin(), bytes.end()}};
        pending_.push_back(std::move(upload));
        ++counters_.uploads;
        counters_.uploaded_bytes += bytes.size();
        return true;
    } catch (const std::invalid_argument& error) {
        error_ = error.what();
        return false;
    } catch (const std::out_of_range& error) {
        error_ = error.what();
        return false;
    }
}

void* RecordingRenderer::create_vertex_buffer(std::size_t size) {
    return create(Kind::quad, size);
}

void* RecordingRenderer::create_glyph_buffer(std::size_t size) {
    return create(Kind::glyph, size);
}

void* RecordingRenderer::create_effect_buffer(std::size_t size) {
    return create(Kind::effect, size);
}

void* RecordingRenderer::create_glyph_sampler() {
    return create(Kind::sampler, 0);
}

void* RecordingRenderer::create_glyph_texture(std::uint32_t width, std::uint32_t height) {
    if (!width || !height || std::uint64_t(width) * height > std::numeric_limits<std::size_t>::max()) {
        throw std::length_error("Recording texture extent invalid");
    }
    return create(Kind::texture, static_cast<std::size_t>(width) * height, width, height);
}

void RecordingRenderer::release_buffer(void* handle) noexcept {
    release(handle, Kind::quad);
}

void RecordingRenderer::release_glyph_buffer(void* handle) noexcept {
    release(handle, Kind::glyph);
}

void RecordingRenderer::release_effect_buffer(void* handle) noexcept {
    release(handle, Kind::effect);
}

void RecordingRenderer::release_glyph_sampler(void* handle) noexcept {
    release(handle, Kind::sampler);
}

void RecordingRenderer::release_glyph_texture(void* handle) noexcept {
    release(handle, Kind::texture);
}

bool RecordingRenderer::upload(void* handle, std::size_t offset, std::span<const std::byte> bytes) {
    return upload_buffer(handle, Kind::quad, offset, bytes);
}

bool RecordingRenderer::upload_glyph_buffer(void* handle, std::size_t offset, std::span<const std::byte> bytes) {
    return upload_buffer(handle, Kind::glyph, offset, bytes);
}

bool RecordingRenderer::upload_effect_buffer(void* handle, std::size_t offset, std::span<const std::byte> bytes) {
    return upload_buffer(handle, Kind::effect, offset, bytes);
}

bool RecordingRenderer::upload_glyph_texture(void* handle, const GlyphTextureUpload& source) {
    require_owner();
    try {
        auto& resource = require(handle, Kind::texture);
        const auto rectangle = source.rectangle;
        if (!batch_) {
            throw std::out_of_range("Recording texture upload batch absent");
        }
        validate_glyph_texture_upload(source, resource.width, resource.height);
        if (inject(RecordingFailure::upload_exception)) {
            throw std::runtime_error("Injected upload exception");
        }
        if (inject(RecordingFailure::upload)) {
            return false;
        }
        std::vector<std::byte> pixels(std::size_t(rectangle.width) * rectangle.height);
        for (std::uint32_t row = 0; row < rectangle.height; ++row) {
            std::memcpy(pixels.data() + std::size_t(row) * rectangle.width,
                        source.bytes.data() + source.source_offset + std::size_t(row) * source.source_row_pitch,
                        rectangle.width);
        }
        pending_.push_back({&resource, 0, rectangle, rectangle.width, std::move(pixels)});
        ++counters_.uploads;
        counters_.uploaded_bytes += std::uint64_t(rectangle.width) * rectangle.height;
        return true;
    } catch (const std::invalid_argument& error) {
        error_ = error.what();
        return false;
    } catch (const std::out_of_range& error) {
        error_ = error.what();
        return false;
    }
}

std::span<const std::byte> RecordingRenderer::buffer_bytes(void* handle) const {
    require_owner();
    auto* resource = find(handle);
    if (!resource || resource->kind == Kind::texture || resource->kind == Kind::image_texture ||
        resource->kind == Kind::sampler) {
        throw std::invalid_argument("Recording resource is not a buffer");
    }
    return require(handle, resource->kind).bytes;
}

std::span<const std::byte> RecordingRenderer::texture_bytes(void* handle) const {
    require_owner();
    auto* r = find(handle);
    if (!r || (r->kind != Kind::texture && r->kind != Kind::image_texture)) {
        throw std::invalid_argument("Not a texture");
    }
    return require(handle, r->kind).bytes;
}

void RecordingRenderer::record(graphics::SceneDrawKind kind, void* handle, Kind resource_kind, std::uint32_t first,
                               std::uint32_t count, std::uint32_t limit, std::size_t stride, std::uint32_t page,
                               void* texture) {
    if (!drawing_ || !count || std::uint64_t(first) + count > limit) {
        throw std::out_of_range("Recording draw range or attachment invalid");
    }
    const auto& resource = require(handle, resource_kind);
    const auto offset = static_cast<std::size_t>(first) * stride;
    const auto bytes = static_cast<std::size_t>(count) * stride;
    if (offset > resource.bytes.size() || bytes > resource.bytes.size() - offset) {
        throw std::out_of_range("Recording draw exceeds buffer bytes");
    }
    draws_.push_back({{kind, first, count, page},
                      {resource.bytes.begin() + offset, resource.bytes.begin() + offset + bytes},
                      texture,
                      epoch_});
}

void RecordingRenderer::draw_quad(std::uint32_t first, std::uint32_t count) {
    record(graphics::SceneDrawKind::quad, attachment().quads(), Kind::quad, first, count, attachment().quad_count(),
           sizeof(QuadGpuInstance));
}

void RecordingRenderer::draw_glyph(std::uint32_t page, std::uint32_t first, std::uint32_t count) {
    const auto* glyphs = attachment().glyphs();
    if (!drawing_ || !glyphs) {
        throw std::logic_error("Recording glyph attachment invalid");
    }
    static_cast<void>(require(glyphs->sampler(), Kind::sampler));
    auto* texture = glyphs->texture(page);
    static_cast<void>(require(texture, Kind::texture));
    record(graphics::SceneDrawKind::glyph, glyphs->instance_buffer(), Kind::glyph, first, count,
           attachment().glyph_count(), sizeof(GlyphGpuInstance), page, texture);
}

void RecordingRenderer::draw_rounded_effect(std::uint32_t first, std::uint32_t count) {
    const auto* effects = attachment().effects();
    if (!drawing_ || !effects) {
        throw std::logic_error("Recording effect attachment invalid");
    }
    record(graphics::SceneDrawKind::rounded_effect, effects->buffer(), Kind::effect, first, count,
           effects->instance_count(), sizeof(detail::RoundedEffectGpuInstance));
}

runtime::FrameSubmissionResult RecordingRenderer::submit_frame(animation::AnimationTime) {
    require_owner();
    if (!attached_scene_ready() || batch_) {
        error_ = "Scene is not committed or attachment is stale";
        return runtime::FrameSubmissionResult::failed;
    }
    if (!surface_available_) {
        ++counters_.deferred;
        return runtime::FrameSubmissionResult::deferred;
    }
    draws_.clear();
    drawing_ = true;
    try {
        draw_ordered_scene(*attachment().scene(), *this);
    } catch (const std::exception& error) {
        drawing_ = false;
        draws_.clear();
        error_ = error.what();
        return runtime::FrameSubmissionResult::failed;
    }
    drawing_ = false;
    ++counters_.frames;
    return runtime::FrameSubmissionResult::submitted;
}
} // namespace ryn::detail

namespace ryn::detail {
void* RecordingRenderer::create_image_texture(std::uint32_t w, std::uint32_t h) {
    if (!w || !h || std::uint64_t(w) * h > std::numeric_limits<std::size_t>::max() / 4) {
        throw std::length_error("Invalid RGBA extent");
    }
    return create(Kind::image_texture, std::size_t(w) * h * 4, w, h);
}

void* RecordingRenderer::create_image_buffer(std::size_t size) {
    return create(Kind::image_buffer, size);
}

bool RecordingRenderer::upload_image_buffer(void* h, std::size_t offset, std::span<const std::byte> bytes) {
    return upload_buffer(h, Kind::image_buffer, offset, bytes);
}

void RecordingRenderer::release_image_texture(void* h) noexcept {
    release(h, Kind::image_texture);
}

void RecordingRenderer::release_image_buffer(void* h) noexcept {
    release(h, Kind::image_buffer);
}

bool RecordingRenderer::upload_image_texture(void* h, const ImageTextureUpload& u) {
    require_owner();
    auto& r = require(h, Kind::image_texture);
    validate_image_texture_upload(u, r.width, r.height);
    if (!batch_) {
        throw std::logic_error("Image upload batch absent");
    }
    if (inject(RecordingFailure::upload_exception)) {
        throw std::runtime_error("Injected image upload exception");
    }
    if (inject(RecordingFailure::upload)) {
        return false;
    }
    std::vector<std::byte> pixels(std::size_t(u.width) * u.height * 4);
    for (std::uint32_t row = 0; row < u.height; ++row) {
        std::memcpy(pixels.data() + std::size_t(row) * u.width * 4,
                    u.bytes.data() + u.source_offset + std::size_t(row) * u.source_row_pitch, std::size_t(u.width) * 4);
    }
    pending_.push_back({&r, 0, {0, 0, u.width, u.height}, u.width * 4, std::move(pixels)});
    ++counters_.uploads;
    counters_.uploaded_bytes += std::uint64_t(u.width) * u.height * 4;
    return true;
}

void RecordingRenderer::draw_image(std::uint32_t index, std::uint32_t generation) {
    const auto* images = attachment().images();
    if (!images || !images->drawable(index, generation)) {
        throw std::logic_error("Invalid Image attachment");
    }
    auto* texture = images->texture(index);
    static_cast<void>(require(texture, Kind::image_texture));
    record(graphics::SceneDrawKind::image, images->buffer(), Kind::image_buffer, index, 1, index + 1,
           sizeof(ImageGpuInstance), generation, texture);
}
} // namespace ryn::detail
