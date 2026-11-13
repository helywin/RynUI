#include "renderer/recording/recording_renderer.hpp"

#include <algorithm>
#include <cstring>
#include <limits>
#include <stdexcept>

namespace ryn::detail {

RecordingRenderer::~RecordingRenderer() = default;

void* RecordingRenderer::create_composition_target(CompositionTargetSpec spec) {
    return create(Kind::composition_target, std::size_t(spec.width) * spec.height * 4, spec.width, spec.height);
}

void RecordingRenderer::release_composition_target(void* target) noexcept {
    release(target, Kind::composition_target);
}

void* RecordingRenderer::create_composition_weights(std::size_t size) {
    return create(Kind::composition_weights, size);
}

bool RecordingRenderer::upload_composition_weights(void* handle, std::span<const std::byte> bytes) {
    return upload_buffer(handle, Kind::composition_weights, 0, bytes);
}

void RecordingRenderer::release_composition_weights(void* handle) noexcept {
    release(handle, Kind::composition_weights);
}

bool RecordingRenderer::composition_fence_complete(std::uint64_t fence) {
    require_owner();
    return fence <= completed_composition_fence_;
}

void RecordingRenderer::wait_composition_fence(std::uint64_t fence) noexcept {
    completed_composition_fence_ = std::max(completed_composition_fence_, fence);
}

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
    if (inject(RecordingFailure::create_exception)) {
        throw std::runtime_error("Injected create exception");
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

RecordingRenderer::Kind RecordingRenderer::paint_kind(EffectPaintBufferKind kind) noexcept {
    switch (kind) {
    case EffectPaintBufferKind::link:
        return Kind::effect_link;
    case EffectPaintBufferKind::field:
        return Kind::effect_field;
    case EffectPaintBufferKind::stop:
        return Kind::effect_stop;
    }
    return Kind::effect_link;
}

void* RecordingRenderer::create_effect_paint_buffer(EffectPaintBufferKind kind, std::size_t size) {
    if (capabilities_.effect_paint_abi_version != effect_paint_abi_version) {
        error_ = "Recording Effect paint ABI unsupported";
        return nullptr;
    }
    const auto stride = effect_paint_buffer_stride(kind);
    if (!stride || !size || size % stride) {
        error_ = "Recording Effect paint buffer stride is invalid";
        return nullptr;
    }
    return create(paint_kind(kind), size);
}

bool RecordingRenderer::upload_effect_paint_buffer(EffectPaintBufferKind kind, void* handle, std::size_t offset,
                                                   std::span<const std::byte> bytes) {
    return upload_buffer(handle, paint_kind(kind), offset, bytes);
}

void RecordingRenderer::release_effect_paint_buffer(EffectPaintBufferKind kind, void* handle) noexcept {
    release(handle, paint_kind(kind));
}

void* RecordingRenderer::create_cap_geometry_buffer(std::size_t size) {
    if (!capabilities_.effect_arc_caps || capabilities_.cap_geometry_abi_version != cap_geometry_abi_version) {
        error_ = "Recording Cap Geometry ABI unsupported";
        return nullptr;
    }
    return create(Kind::cap_geometry, size);
}

bool RecordingRenderer::upload_cap_geometry_buffer(void* handle, std::size_t offset, std::span<const std::byte> bytes) {
    return upload_buffer(handle, Kind::cap_geometry, offset, bytes);
}

void RecordingRenderer::release_cap_geometry_buffer(void* handle) noexcept {
    release(handle, Kind::cap_geometry);
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
                      epoch_,
                      {},
                      {},
                      {},
                      {},
                      {},
                      {},
                      {}});
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
    auto& draw = draws_.back();
    draw.effect_binding = effects->draw_binding(first, count);
    const auto binding = draw.effect_binding;
    if (!binding.link) {
        return;
    }
    const auto& links = require(binding.link, Kind::effect_link).bytes;
    const auto& fields = require(binding.field, Kind::effect_field).bytes;
    const auto& stops = require(binding.stop, Kind::effect_stop).bytes;
    const auto link_size = std::size_t(count) * sizeof(EffectPaintGpuLink);
    if (!binding.field_count || !binding.stop_count || binding.link_byte_offset > links.size() ||
        link_size > links.size() - binding.link_byte_offset ||
        std::size_t(binding.field_count) > fields.size() / sizeof(EffectPaintGpuField) ||
        std::size_t(binding.stop_count) > stops.size() / sizeof(EffectPaintGpuStop)) {
        throw std::out_of_range("Recording Effect paint binding exceeds table bytes");
    }
    draw.effect_link_bytes.assign(links.begin() + binding.link_byte_offset,
                                  links.begin() + binding.link_byte_offset + link_size);
    if (binding.cap_geometry) {
        const auto& caps = require(binding.cap_geometry, Kind::cap_geometry).bytes;
        const auto cap_size = std::size_t(count) * sizeof(CapGeometryGpuV1);
        if (binding.cap_byte_offset > caps.size() || cap_size > caps.size() - binding.cap_byte_offset) {
            throw std::out_of_range("Recording Cap Geometry binding exceeds buffer bytes");
        }
        draw.cap_geometry_bytes.assign(caps.begin() + binding.cap_byte_offset,
                                       caps.begin() + binding.cap_byte_offset + cap_size);
    }
    std::vector<EffectPaintGpuField> packed_fields(binding.field_count);
    std::vector<EffectPaintGpuStop> packed_stops(binding.stop_count);
    std::memcpy(packed_fields.data(), fields.data(), packed_fields.size() * sizeof(EffectPaintGpuField));
    std::memcpy(packed_stops.data(), stops.data(), packed_stops.size() * sizeof(EffectPaintGpuStop));
    const auto consume = [](std::uint32_t index, std::size_t stride, const auto& source, auto& indices, auto& bytes) {
        if (std::ranges::find(indices, index) == indices.end()) {
            indices.push_back(index);
            const auto offset = std::size_t(index) * stride;
            bytes.insert(bytes.end(), source.begin() + offset, source.begin() + offset + stride);
        }
    };
    for (std::uint32_t index = 0; index < count; ++index) {
        EffectPaintGpuLink link;
        std::memcpy(&link, draw.effect_link_bytes.data() + std::size_t(index) * sizeof(link), sizeof(link));
        validate_effect_paint_link(link, packed_fields, packed_stops);
        if (link.arc_cap && !binding.cap_geometry) {
            throw std::invalid_argument("Recording non-round link lacks Cap Geometry stream");
        }
        if (binding.cap_geometry) {
            CapGeometryGpuV1 cap;
            std::memcpy(&cap, draw.cap_geometry_bytes.data() + std::size_t(index) * sizeof(cap), sizeof(cap));
            if (link.arc_cap) {
                validate_cap_geometry(cap);
            } else if (cap != CapGeometryGpuV1{}) {
                throw std::invalid_argument("Recording non-cap row has nonzero Cap Geometry bytes");
            }
        }
        for (const auto field : {link.foreground_field, link.underlay_field}) {
            if (field == effect_paint_no_field) {
                continue;
            }
            consume(field, sizeof(EffectPaintGpuField), fields, draw.effect_field_indices, draw.effect_field_bytes);
            const auto& packed = packed_fields[field];
            for (std::uint32_t stop = 0; stop < packed.stop_count; ++stop) {
                consume(packed.first_stop + stop, sizeof(EffectPaintGpuStop), stops, draw.effect_stop_indices,
                        draw.effect_stop_bytes);
            }
        }
    }
}

runtime::FrameSubmissionResult RecordingRenderer::submit_frame(animation::AnimationTime) {
    require_owner();
    if (!attached_scene_ready() || batch_) {
        error_ = "Scene is not committed or attachment is stale";
        return runtime::FrameSubmissionResult::failed;
    }
    if (attachment().composition() && attachment().composition()->fence &&
        !composition_fence_complete(attachment().composition()->fence)) {
        ++counters_.deferred;
        return runtime::FrameSubmissionResult::deferred;
    }
    if (!surface_available_) {
        ++counters_.deferred;
        return runtime::FrameSubmissionResult::deferred;
    }
    draws_.clear();
    composition_records_.clear();
    drawing_ = true;
    try {
        if (const auto& frame = attachment().composition()) {
            const auto& plan = frame->plan;
            std::vector<std::uint32_t> stack{0};
            for (const auto op : plan.program.operations) {
                CompositionRecord record{op, frame->targets[stack.back()]};
                switch (op.kind) {
                case graphics::SceneCompositionOpKind::DrawRange: {
                    graphics::OrderedScene range;
                    for (std::uint32_t i = op.first; i < op.first + op.count; ++i) {
                        range.append_command(attachment().scene()->commands()[i]);
                    }
                    draw_ordered_scene(range, *this);
                    break;
                }
                case graphics::SceneCompositionOpKind::BeginMaskedGroup:
                    stack.push_back(plan.requests[op.request].target);
                    record.target = frame->targets[stack.back()];
                    break;
                case graphics::SceneCompositionOpKind::EndMaskedGroup:
                    record.source = frame->targets[stack.back()];
                    stack.pop_back();
                    record.target = frame->targets[stack.back()];
                    break;
                case graphics::SceneCompositionOpKind::Backdrop:
                    record.source = frame->targets[plan.snapshot_target];
                    record.filtered = frame->targets[plan.filtered_target];
                    if (record.target == record.source || record.target == record.filtered ||
                        record.source == record.filtered) {
                        throw std::logic_error("Backdrop read/write alias");
                    }
                    if (frame->weights) {
                        (void)require(frame->weights, Kind::composition_weights);
                    }
                    break;
                }
                (void)require(record.target, Kind::composition_target);
                composition_records_.push_back(record);
            }
            frame->fence = ++composition_fence_;
            if (auto_complete_frames_) {
                completed_composition_fence_ = composition_fence_;
            }
        } else {
            draw_ordered_scene(*attachment().scene(), *this);
        }
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
