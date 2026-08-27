#include "graphics/rounded_effect.hpp"

#include <algorithm>
#include <array>
#include <cmath>
#include <limits>
#include <stdexcept>
#include <utility>

namespace ryn::graphics {
namespace {

[[nodiscard]] bool finite(float value) noexcept {
    return std::isfinite(value);
}

void validate_rect(runtime::Rect rect, const char* message) {
    if (!finite(rect.x) || !finite(rect.y) || !finite(rect.width)
            || !finite(rect.height) || rect.width < 0.0F || rect.height < 0.0F) {
        throw std::invalid_argument(message);
    }
}

[[nodiscard]] LogicalRoundedRect translated(
    LogicalRoundedRect shape,
    runtime::Point translation) noexcept {
    shape.rect.x += translation.x;
    shape.rect.y += translation.y;
    return shape;
}

[[nodiscard]] LogicalRoundedRect spread_shape(
    LogicalRoundedRect shape,
    float spread) noexcept {
    shape.rect.x -= spread;
    shape.rect.y -= spread;
    shape.rect.width += 2.0F * spread;
    shape.rect.height += 2.0F * spread;
    if (shape.rect.width <= 0.0F || shape.rect.height <= 0.0F) {
        shape.rect.width = 0.0F;
        shape.rect.height = 0.0F;
        shape.radius = 0.0F;
        return shape;
    }
    shape.radius = std::clamp(
        shape.radius + spread,
        0.0F,
        0.5F * std::min(shape.rect.width, shape.rect.height));
    return shape;
}

[[nodiscard]] float gaussian_edge(float signed_distance, float sigma) noexcept {
    if (sigma <= 0.0F) {
        return signed_distance <= 0.0F ? 1.0F : 0.0F;
    }
    constexpr float inverse_sqrt_two = 0.7071067811865475F;
    return std::clamp(
        0.5F * std::erfc(signed_distance * inverse_sqrt_two / sigma),
        0.0F,
        1.0F);
}

[[nodiscard]] float smoothstep(float edge0, float edge1, float value) noexcept {
    if (edge0 == edge1) {
        return value < edge0 ? 0.0F : 1.0F;
    }
    const float t = std::clamp((value - edge0) / (edge1 - edge0), 0.0F, 1.0F);
    return t * t * (3.0F - 2.0F * t);
}

[[nodiscard]] bool empty(runtime::Rect rect) noexcept {
    return rect.width <= 0.0F || rect.height <= 0.0F;
}

[[nodiscard]] runtime::Rect expand(runtime::Rect rect, float amount) noexcept {
    return {
        rect.x - amount,
        rect.y - amount,
        std::max(0.0F, rect.width + 2.0F * amount),
        std::max(0.0F, rect.height + 2.0F * amount),
    };
}

[[nodiscard]] runtime::Rect translate(
    runtime::Rect rect,
    runtime::Point point) noexcept {
    rect.x += point.x;
    rect.y += point.y;
    return rect;
}

[[nodiscard]] RoundedEffectGeometry geometry_of(
    const RoundedEffectInstance& instance) {
    return instance.geometry;
}

[[nodiscard]] bool drawable(const RoundedEffectInstance& instance) noexcept {
    return instance.material.visible
        && instance.material.opacity > 0.0F
        && instance.material.color.alpha() > 0.0F;
}

void append_command(
    std::vector<SceneDrawCommand>& output,
    SceneDrawCommand command) {
    if (command.instance_count == 0) {
        return;
    }
    if (!output.empty()) {
        auto& previous = output.back();
        if (previous.kind == command.kind
                && previous.atlas_page == command.atlas_page
                && static_cast<std::uint64_t>(previous.first_instance)
                        + previous.instance_count == command.first_instance) {
            const auto merged = static_cast<std::uint64_t>(previous.instance_count)
                + command.instance_count;
            if (merged > std::numeric_limits<std::uint32_t>::max()) {
                throw std::length_error(
                    "Rounded effect Scene command range exceeds uint32_t");
            }
            previous.instance_count = static_cast<std::uint32_t>(merged);
            return;
        }
    }
    output.push_back(command);
}

} // namespace

struct RoundedEffectStore::Slot final {
    std::optional<RoundedEffectInstance> instance;
    std::optional<std::uint32_t> packed_index;
    std::uint32_t generation{1};
};

RoundedEffectStore::RoundedEffectStore() = default;
RoundedEffectStore::~RoundedEffectStore() = default;
RoundedEffectStore::RoundedEffectStore(RoundedEffectStore&&) noexcept = default;
RoundedEffectStore& RoundedEffectStore::operator=(RoundedEffectStore&&) noexcept = default;

RoundedEffectInstance make_shadow_effect(
    LogicalRoundedRect shape,
    const ShadowLayer& layer,
    runtime::Point translation,
    std::optional<EffectClip> ancestor_clip) {
    RoundedEffectInstance instance{
        {
            shape,
            layer.kind == ShadowKind::outer
                ? RoundedEffectKind::outer_shadow
                : RoundedEffectKind::inset_shadow,
            layer.offset,
            layer.blur,
            layer.spread,
            0.0F,
            0.0F,
            translation,
            std::move(ancestor_clip),
        },
        {layer.color, 1.0F, true},
    };
    validate_rounded_effect(instance);
    return instance;
}

RoundedEffectInstance make_outline_effect(
    LogicalRoundedRect shape,
    float width,
    float offset,
    Color color,
    float opacity,
    runtime::Point translation,
    std::optional<EffectClip> ancestor_clip) {
    RoundedEffectInstance instance{
        {
            shape,
            RoundedEffectKind::outline,
            {},
            0.0F,
            0.0F,
            width,
            offset,
            translation,
            std::move(ancestor_clip),
        },
        {color, opacity, true},
    };
    validate_rounded_effect(instance);
    return instance;
}

void validate_rounded_effect(const RoundedEffectInstance& instance) {
    const auto& geometry = instance.geometry;
    validate_rect(geometry.shape.rect, "Rounded effect shape must be finite and non-negative");
    const float maximum_radius =
        0.5F * std::min(geometry.shape.rect.width, geometry.shape.rect.height);
    if (!finite(geometry.shape.radius) || geometry.shape.radius < 0.0F
            || geometry.shape.radius > maximum_radius
            || !finite(geometry.offset.x) || !finite(geometry.offset.y)
            || !finite(geometry.blur) || geometry.blur < 0.0F
            || !finite(geometry.spread)
            || !finite(geometry.outline_width) || geometry.outline_width < 0.0F
            || !finite(geometry.outline_offset) || geometry.outline_offset < 0.0F
            || !finite(geometry.translation.x) || !finite(geometry.translation.y)) {
        throw std::invalid_argument("Rounded effect geometry is invalid");
    }
    switch (geometry.kind) {
    case RoundedEffectKind::outer_shadow:
    case RoundedEffectKind::inset_shadow:
        if (geometry.outline_width != 0.0F || geometry.outline_offset != 0.0F) {
            throw std::invalid_argument("Shadow effect cannot contain outline geometry");
        }
        break;
    case RoundedEffectKind::outline:
        if (geometry.outline_width <= 0.0F || geometry.blur != 0.0F
                || geometry.spread != 0.0F || geometry.offset != LogicalOffset{}) {
            throw std::invalid_argument("Outline effect geometry is invalid");
        }
        break;
    default:
        throw std::invalid_argument("Rounded effect kind is invalid");
    }
    if (!finite(instance.material.opacity) || instance.material.opacity < 0.0F
            || instance.material.opacity > 1.0F) {
        throw std::invalid_argument("Rounded effect opacity must be in [0, 1]");
    }
    if (geometry.ancestor_clip.has_value()) {
        validate_rect(
            geometry.ancestor_clip->bounds,
            "Rounded effect ancestor clip must be finite and non-negative");
    }
}

float rounded_rect_signed_distance(
    runtime::Point point,
    LogicalRoundedRect shape) noexcept {
    if (shape.rect.width <= 0.0F || shape.rect.height <= 0.0F) {
        return std::numeric_limits<float>::infinity();
    }
    const float radius = std::clamp(
        shape.radius,
        0.0F,
        0.5F * std::min(shape.rect.width, shape.rect.height));
    const float center_x = shape.rect.x + shape.rect.width * 0.5F;
    const float center_y = shape.rect.y + shape.rect.height * 0.5F;
    const float half_width = shape.rect.width * 0.5F;
    const float half_height = shape.rect.height * 0.5F;
    const float qx = std::abs(point.x - center_x) - (half_width - radius);
    const float qy = std::abs(point.y - center_y) - (half_height - radius);
    const float outside = std::hypot(std::max(qx, 0.0F), std::max(qy, 0.0F));
    const float inside = std::min(std::max(qx, qy), 0.0F);
    return outside + inside - radius;
}

float rounded_effect_coverage(
    runtime::Point point,
    const RoundedEffectInstance& instance,
    float antialias_width) {
    validate_rounded_effect(instance);
    if (!finite(antialias_width) || antialias_width <= 0.0F) {
        throw std::invalid_argument("Rounded effect antialias width must be positive");
    }
    const auto& geometry = instance.geometry;
    const auto base = translated(geometry.shape, geometry.translation);
    switch (geometry.kind) {
    case RoundedEffectKind::outer_shadow: {
        auto shadow = spread_shape(base, geometry.spread);
        shadow.rect.x += geometry.offset.x;
        shadow.rect.y += geometry.offset.y;
        return gaussian_edge(
            rounded_rect_signed_distance(point, shadow),
            geometry.blur * 0.5F);
    }
    case RoundedEffectKind::inset_shadow: {
        if (rounded_rect_signed_distance(point, base) > 0.0F) {
            return 0.0F;
        }
        auto shifted = base;
        shifted.rect.x += geometry.offset.x;
        shifted.rect.y += geometry.offset.y;
        const float distance_inside = -rounded_rect_signed_distance(point, shifted)
            - geometry.spread;
        return gaussian_edge(distance_inside, geometry.blur * 0.5F);
    }
    case RoundedEffectKind::outline: {
        const float distance = rounded_rect_signed_distance(point, base);
        const float half_aa = antialias_width * 0.5F;
        const float inner = smoothstep(
            geometry.outline_offset - half_aa,
            geometry.outline_offset + half_aa,
            distance);
        const float outer = 1.0F - smoothstep(
            geometry.outline_offset + geometry.outline_width - half_aa,
            geometry.outline_offset + geometry.outline_width + half_aa,
            distance);
        return std::clamp(inner * outer, 0.0F, 1.0F);
    }
    }
    return 0.0F;
}

runtime::Rect rounded_effect_bounds(
    const RoundedEffectInstance& instance,
    float antialias_guard) {
    validate_rounded_effect(instance);
    if (!finite(antialias_guard) || antialias_guard < 0.0F) {
        throw std::invalid_argument("Rounded effect antialias guard must be non-negative");
    }
    const auto& geometry = instance.geometry;
    runtime::Rect bounds = translate(geometry.shape.rect, geometry.translation);
    if (empty(bounds)) {
        return bounds;
    }
    switch (geometry.kind) {
    case RoundedEffectKind::outer_shadow: {
        const auto spread = spread_shape(
            {bounds, geometry.shape.radius}, geometry.spread);
        if (empty(spread.rect)) {
            return spread.rect;
        }
        bounds = spread.rect;
        bounds.x += geometry.offset.x;
        bounds.y += geometry.offset.y;
        bounds = expand(bounds, 1.5F * geometry.blur + antialias_guard);
        break;
    }
    case RoundedEffectKind::inset_shadow:
        break;
    case RoundedEffectKind::outline:
        bounds = expand(
            bounds,
            geometry.outline_offset + geometry.outline_width + antialias_guard);
        break;
    }
    if (geometry.ancestor_clip.has_value()) {
        bounds = intersect_effect_bounds(bounds, geometry.ancestor_clip->bounds);
    }
    return bounds;
}

runtime::Rect intersect_effect_bounds(
    runtime::Rect bounds,
    runtime::Rect clip) noexcept {
    const float left = std::max(bounds.x, clip.x);
    const float top = std::max(bounds.y, clip.y);
    const float right = std::min(bounds.x + bounds.width, clip.x + clip.width);
    const float bottom = std::min(bounds.y + bounds.height, clip.y + clip.height);
    if (right <= left || bottom <= top) {
        return {left, top, 0.0F, 0.0F};
    }
    return {left, top, right - left, bottom - top};
}

void RoundedEffectStore::reserve(std::size_t capacity) {
    const auto slots_before = slots_.capacity();
    const auto order_before = draw_order_.capacity();
    const auto packed_before = packed_instances_.capacity();
    slots_.reserve(capacity);
    free_slots_.reserve(capacity);
    draw_order_.reserve(capacity);
    packed_instances_.reserve(capacity);
    material_dirty_ranges_.reserve(capacity);
    geometry_dirty_ranges_.reserve(capacity);
    note_capacity_growth(slots_before, order_before, packed_before);
}

RoundedEffectId RoundedEffectStore::add(RoundedEffectInstance instance) {
    const std::array values{instance};
    return add_batch(values).front();
}

std::vector<RoundedEffectId> RoundedEffectStore::add_batch(
    std::span<const RoundedEffectInstance> instances) {
    for (const auto& instance : instances) {
        validate_rounded_effect(instance);
    }
    if (instances.empty()) {
        return {};
    }
    if (diagnostics_.live_instances + instances.size()
            > std::numeric_limits<std::uint32_t>::max()) {
        throw std::length_error("RoundedEffectStore exhausted instance identities");
    }

    const auto slots_before = slots_.capacity();
    const auto order_before = draw_order_.capacity();
    const auto packed_before = packed_instances_.capacity();
    const auto new_slots = instances.size() > free_slots_.size()
        ? instances.size() - free_slots_.size()
        : 0;
    slots_.reserve(slots_.size() + new_slots);
    free_slots_.reserve(slots_.size() + new_slots);
    draw_order_.reserve(draw_order_.size() + instances.size());
    packed_instances_.reserve(packed_instances_.size() + instances.size());
    std::vector<RoundedEffectId> result;
    result.reserve(instances.size());
    note_capacity_growth(slots_before, order_before, packed_before);

    for (const auto& instance : instances) {
        std::uint32_t index{};
        if (!free_slots_.empty()) {
            index = free_slots_.back();
            free_slots_.pop_back();
            ++diagnostics_.slot_reuses;
        } else {
            slots_.emplace_back();
            index = static_cast<std::uint32_t>(slots_.size() - 1);
        }
        auto& slot = slots_[index];
        slot.instance = instance;
        slot.packed_index.reset();
        const RoundedEffectId id{index, slot.generation};
        draw_order_.push_back(id);
        result.push_back(id);
    }
    diagnostics_.additions += instances.size();
    diagnostics_.live_instances += instances.size();
    compact_dirty_ = true;
    return result;
}

bool RoundedEffectStore::remove(RoundedEffectId id) {
    auto* slot = find_slot(id);
    if (slot == nullptr) {
        return false;
    }
    free_slots_.reserve(free_slots_.size() + 1);
    slot->instance.reset();
    slot->packed_index.reset();
    advance_generation(*slot);
    free_slots_.push_back(id.index);
    std::erase(draw_order_, id);
    ++diagnostics_.removals;
    --diagnostics_.live_instances;
    compact_dirty_ = true;
    return true;
}

bool RoundedEffectStore::contains(RoundedEffectId id) const noexcept {
    return find_slot(id) != nullptr;
}

const RoundedEffectInstance& RoundedEffectStore::at(RoundedEffectId id) const {
    return *require_slot(id).instance;
}

bool RoundedEffectStore::update_material(
    RoundedEffectId id,
    RoundedEffectMaterial material) {
    auto& slot = require_slot(id);
    auto candidate = *slot.instance;
    candidate.material = material;
    validate_rounded_effect(candidate);
    if (slot.instance->material == material) {
        return false;
    }
    const bool visibility_changed =
        drawable(*slot.instance) != drawable(candidate);
    slot.instance->material = material;
    ++diagnostics_.material_updates;
    if (visibility_changed || !slot.packed_index.has_value()) {
        compact_dirty_ = true;
    } else {
        packed_instances_[*slot.packed_index].material = material;
        mark_dirty(material_dirty_ranges_, {*slot.packed_index, 1});
    }
    return true;
}

bool RoundedEffectStore::update_geometry(
    RoundedEffectId id,
    RoundedEffectGeometry geometry) {
    auto& slot = require_slot(id);
    auto candidate = *slot.instance;
    candidate.geometry = geometry;
    validate_rounded_effect(candidate);
    if (slot.instance->geometry == geometry) {
        return false;
    }
    slot.instance->geometry = std::move(geometry);
    ++diagnostics_.geometry_updates;
    compact_dirty_ = true;
    return true;
}

bool RoundedEffectStore::compact(runtime::Rect window_clip) {
    validate_clip(window_clip);
    if (!compact_dirty_ && compact_clip_ == window_clip) {
        ++diagnostics_.idle_compactions;
        return false;
    }
    const auto packed_before = packed_instances_.capacity();
    packed_instances_.clear();
    diagnostics_.culled_instances = 0;
    for (auto& slot : slots_) {
        slot.packed_index.reset();
    }
    for (const auto id : draw_order_) {
        auto* slot = find_slot(id);
        if (slot == nullptr || !drawable(*slot->instance)) {
            ++diagnostics_.culled_instances;
            continue;
        }
        const auto bounds = intersect_effect_bounds(
            rounded_effect_bounds(*slot->instance),
            window_clip);
        if (empty(bounds)) {
            ++diagnostics_.culled_instances;
            continue;
        }
        if (packed_instances_.size() >= std::numeric_limits<std::uint32_t>::max()) {
            throw std::length_error("Rounded effect packed range exceeds uint32_t");
        }
        slot->packed_index = static_cast<std::uint32_t>(packed_instances_.size());
        packed_instances_.push_back(*slot->instance);
    }
    if (packed_instances_.capacity() != packed_before) {
        ++diagnostics_.capacity_growths;
    }
    material_dirty_ranges_.clear();
    geometry_dirty_ranges_.clear();
    if (!packed_instances_.empty()) {
        geometry_dirty_ranges_.push_back({
            0,
            static_cast<std::uint32_t>(packed_instances_.size()),
        });
    }
    compact_clip_ = window_clip;
    compact_dirty_ = false;
    ++diagnostics_.compactions;
    diagnostics_.packed_instances = packed_instances_.size();
    return true;
}

std::optional<std::uint32_t> RoundedEffectStore::packed_index(
    RoundedEffectId id) const {
    return require_slot(id).packed_index;
}

std::span<const RoundedEffectInstance>
RoundedEffectStore::packed_instances() const noexcept {
    return packed_instances_;
}

std::span<const std::byte> RoundedEffectStore::bytes(
    RoundedEffectInstanceRange range) const {
    const auto end = static_cast<std::uint64_t>(range.first) + range.count;
    if (end > packed_instances_.size()) {
        throw std::out_of_range("Rounded effect byte range is out of bounds");
    }
    return std::as_bytes(std::span(packed_instances_).subspan(range.first, range.count));
}

std::span<const RoundedEffectInstanceRange>
RoundedEffectStore::material_dirty_ranges() const noexcept {
    return material_dirty_ranges_;
}

std::span<const RoundedEffectInstanceRange>
RoundedEffectStore::geometry_dirty_ranges() const noexcept {
    return geometry_dirty_ranges_;
}

void RoundedEffectStore::clear_dirty_ranges() noexcept {
    material_dirty_ranges_.clear();
    geometry_dirty_ranges_.clear();
}

std::size_t RoundedEffectStore::live_count() const noexcept {
    return diagnostics_.live_instances;
}

std::size_t RoundedEffectStore::slot_capacity() const noexcept {
    return slots_.capacity();
}

const RoundedEffectStoreDiagnostics&
RoundedEffectStore::diagnostics() const noexcept {
    return diagnostics_;
}

RoundedEffectStore::Slot* RoundedEffectStore::find_slot(
    RoundedEffectId id) noexcept {
    if (!id.valid() || id.index >= slots_.size()) {
        return nullptr;
    }
    auto& slot = slots_[id.index];
    return slot.generation == id.generation && slot.instance.has_value()
        ? &slot
        : nullptr;
}

const RoundedEffectStore::Slot* RoundedEffectStore::find_slot(
    RoundedEffectId id) const noexcept {
    if (!id.valid() || id.index >= slots_.size()) {
        return nullptr;
    }
    const auto& slot = slots_[id.index];
    return slot.generation == id.generation && slot.instance.has_value()
        ? &slot
        : nullptr;
}

RoundedEffectStore::Slot& RoundedEffectStore::require_slot(RoundedEffectId id) {
    if (auto* slot = find_slot(id)) {
        return *slot;
    }
    throw std::out_of_range("RoundedEffectId is stale or invalid");
}

const RoundedEffectStore::Slot& RoundedEffectStore::require_slot(
    RoundedEffectId id) const {
    if (const auto* slot = find_slot(id)) {
        return *slot;
    }
    throw std::out_of_range("RoundedEffectId is stale or invalid");
}

void RoundedEffectStore::advance_generation(Slot& slot) noexcept {
    ++slot.generation;
    if (slot.generation == 0) {
        slot.generation = 1;
    }
}

void RoundedEffectStore::mark_dirty(
    std::vector<RoundedEffectInstanceRange>& ranges,
    RoundedEffectInstanceRange range) {
    if (range.count == 0) {
        return;
    }
    ranges.push_back(range);
    std::ranges::sort(ranges, {}, &RoundedEffectInstanceRange::first);
    std::size_t merged_count = 0;
    for (const auto candidate : ranges) {
        if (merged_count == 0) {
            ranges[merged_count++] = candidate;
            continue;
        }
        auto& prior = ranges[merged_count - 1];
        const auto prior_end = static_cast<std::uint64_t>(prior.first) + prior.count;
        const auto candidate_end =
            static_cast<std::uint64_t>(candidate.first) + candidate.count;
        if (candidate.first <= prior_end) {
            prior.count = static_cast<std::uint32_t>(
                std::max(prior_end, candidate_end) - prior.first);
        } else {
            ranges[merged_count++] = candidate;
        }
    }
    ranges.resize(merged_count);
}

void RoundedEffectStore::validate_clip(runtime::Rect clip) {
    validate_rect(clip, "Rounded effect window clip must be finite and non-negative");
}

void RoundedEffectStore::note_capacity_growth(
    std::size_t slots_before,
    std::size_t order_before,
    std::size_t packed_before) noexcept {
    if (slots_.capacity() != slots_before
            || draw_order_.capacity() != order_before
            || packed_instances_.capacity() != packed_before) {
        ++diagnostics_.capacity_growths;
    }
}

RoundedEffectPrimitive RoundedEffectScene::append_shadow_list(
    LogicalRoundedRect shape,
    const ShadowList& shadows,
    runtime::Point translation,
    std::optional<EffectClip> ancestor_clip) {
    std::vector<RoundedEffectInstance> pending;
    pending.reserve(shadows.size());
    for (const auto& layer : shadows.layers()) {
        pending.push_back(make_shadow_effect(
            shape,
            layer,
            translation,
            ancestor_clip));
    }
    RoundedEffectPrimitive primitive;
    primitive.before_fill.reserve(pending.size());
    primitive.after_fill.reserve(pending.size());
    const auto ids = store_.add_batch(pending);
    for (std::size_t index = 0; index < ids.size(); ++index) {
        if (pending[index].geometry.kind == RoundedEffectKind::inset_shadow) {
            primitive.after_fill.push_back(ids[index]);
        } else {
            primitive.before_fill.push_back(ids[index]);
        }
    }
    return primitive;
}

RoundedEffectPrimitive RoundedEffectScene::append_outline(
    LogicalRoundedRect shape,
    float width,
    float offset,
    Color color,
    float opacity,
    runtime::Point translation,
    std::optional<EffectClip> ancestor_clip) {
    RoundedEffectPrimitive primitive;
    primitive.before_fill.reserve(1);
    primitive.before_fill.push_back(store_.add(make_outline_effect(
        shape,
        width,
        offset,
        color,
        opacity,
        translation,
        std::move(ancestor_clip))));
    return primitive;
}

bool RoundedEffectScene::remove(const RoundedEffectPrimitive& primitive) {
    bool removed = false;
    for (const auto id : primitive.before_fill) {
        removed = store_.remove(id) || removed;
    }
    for (const auto id : primitive.after_fill) {
        removed = store_.remove(id) || removed;
    }
    return removed;
}

void RoundedEffectScene::compose_surface(
    const RoundedEffectPrimitive& primitive,
    SceneDrawCommand fill,
    std::vector<SceneDrawCommand>& output) const {
    append_effect_commands(primitive.before_fill, output);
    append_command(output, fill);
    append_effect_commands(primitive.after_fill, output);
}

RoundedEffectStore& RoundedEffectScene::store() noexcept {
    return store_;
}

const RoundedEffectStore& RoundedEffectScene::store() const noexcept {
    return store_;
}

void RoundedEffectScene::append_effect_commands(
    std::span<const RoundedEffectId> effects,
    std::vector<SceneDrawCommand>& output) const {
    for (const auto id : effects) {
        if (!store_.contains(id)) {
            continue;
        }
        const auto index = store_.packed_index(id);
        if (!index.has_value()) {
            continue;
        }
        append_command(output, {
            SceneDrawKind::rounded_effect,
            *index,
            1,
            invalid_glyph_atlas_page,
        });
    }
}

} // namespace ryn::graphics
