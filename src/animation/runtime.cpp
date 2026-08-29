#include "animation/runtime.hpp"

#include <algorithm>
#include <stdexcept>
#include <utility>

namespace ryn::animation {
namespace {

constexpr AnimationDirtyDomain allowed_dirty_domains =
    AnimationDirtyDomain::material
    | AnimationDirtyDomain::transform
    | AnimationDirtyDomain::geometry
    | AnimationDirtyDomain::animation;

void advance_generation(std::uint32_t& generation) noexcept {
    ++generation;
    if (generation == 0) {
        generation = 1;
    }
}

} // namespace

void AnimationTargetSink::completed(
    AnimationId,
    AnimationTargetId) {}

AnimationRuntime::AnimationRuntime() noexcept
    : owner_thread_(std::this_thread::get_id()) {}

void AnimationRuntime::reserve(
    std::size_t animation_capacity,
    std::size_t scope_capacity,
    std::size_t target_capacity) {
    ensure_owner_thread();
    animation_slots_.reserve(animation_capacity);
    free_animation_slots_.reserve(animation_capacity);
    active_.reserve(animation_capacity);
    tick_snapshot_.reserve(animation_capacity);
    scope_slots_.reserve(scope_capacity);
    free_scope_slots_.reserve(scope_capacity);
    target_slots_.reserve(target_capacity);
    free_target_slots_.reserve(target_capacity);
}

AnimationScopeId AnimationRuntime::create_scope() {
    ensure_owner_thread();
    const auto index = acquire_scope_slot();
    auto& slot = scope_slots_[index];
    const AnimationScopeId id{index, slot.generation};
    slot.record.emplace(ScopeRecord{id});
    ++diagnostics_.scopes;
    return id;
}

bool AnimationRuntime::dispose_scope(AnimationScopeId scope) {
    ensure_owner_thread();
    if (find(scope) == nullptr) {
        ++diagnostics_.stale_operations;
        return false;
    }

    for (std::size_t index = 0; index < target_slots_.size(); ++index) {
        auto& slot = target_slots_[index];
        if (!slot.record.has_value() || slot.record->scope != scope) {
            continue;
        }
        static_cast<void>(unregister_target(slot.record->id));
    }

    auto& slot = scope_slots_[scope.index];
    slot.record.reset();
    advance_generation(slot.generation);
    free_scope_slots_.push_back(scope.index);
    --diagnostics_.scopes;
    return true;
}

AnimationTargetId AnimationRuntime::register_target(
    AnimationScopeId scope,
    AnimationTargetSink& sink,
    AnimationValueKind kind,
    AnimationDirtyDomain dirty_domain) {
    ensure_owner_thread();
    if (find(scope) == nullptr) {
        ++diagnostics_.stale_operations;
        throw std::out_of_range("animation scope is stale");
    }
    validate_target_binding(kind, dirty_domain);
    const auto index = acquire_target_slot();
    auto& slot = target_slots_[index];
    const AnimationTargetId id{index, slot.generation};
    slot.record.emplace(TargetRecord{id, scope, &sink, kind, dirty_domain});
    ++diagnostics_.targets;
    return id;
}

bool AnimationRuntime::unregister_target(AnimationTargetId target) {
    ensure_owner_thread();
    if (find(target) == nullptr) {
        ++diagnostics_.stale_operations;
        return false;
    }

    auto& slot = target_slots_[target.index];
    slot.record.reset();
    advance_generation(slot.generation);
    free_target_slots_.push_back(target.index);
    --diagnostics_.targets;
    cancel_target_animations(target);
    return true;
}

AnimationId AnimationRuntime::play(
    AnimationTargetId target_id,
    AnimationValue from,
    AnimationValue to,
    AnimationSpec spec,
    AnimationTime start_time) {
    ensure_owner_thread();
    const auto* target = find(target_id);
    if (target == nullptr) {
        ++diagnostics_.stale_operations;
        throw std::out_of_range("animation target is stale");
    }
    validate_play_request(*target, from, to, spec, start_time);

    const auto index = acquire_animation_slot();
    auto& slot = animation_slots_[index];
    const AnimationId id{index, slot.generation};
    slot.record.emplace(AnimationRecord{
        id,
        target_id,
        std::move(from),
        std::move(to),
        spec,
        start_time,
        std::nullopt,
        false,
        false,
        false,
    });
    try {
        active_.push_back(id);
    } catch (...) {
        slot.record.reset();
        free_animation_slots_.push_back(index);
        throw;
    }
    ++diagnostics_.created;
    ++diagnostics_.active;

    if (spec.duration == AnimationDuration{}) {
        static_cast<void>(finish(id));
        return id;
    }
    const auto initial = slot.record->from;
    try {
        static_cast<void>(apply_value(id, initial));
    } catch (...) {
        if (find(id) != nullptr) {
            static_cast<void>(remove_animation(id, true));
        }
        throw;
    }
    return id;
}

bool AnimationRuntime::cancel(
    AnimationId animation,
    AnimationTime sample_time) {
    ensure_owner_thread();
    auto* record = find(animation);
    if (record == nullptr) {
        ++diagnostics_.stale_operations;
        return false;
    }
    if (record->in_callback) {
        ++diagnostics_.callback_mutations;
        return remove_animation(animation, true);
    }
    const auto effective_time = observe_time(sample_time);
    const auto current = sample_value(*record, effective_time);
    if (!apply_value(animation, current)) {
        return true;
    }
    return remove_animation(animation, true);
}

bool AnimationRuntime::finish(AnimationId animation) {
    ensure_owner_thread();
    auto* record = find(animation);
    if (record == nullptr) {
        ++diagnostics_.stale_operations;
        return false;
    }
    if (record->completion_counted) {
        return true;
    }
    if (record->in_callback) {
        record->finish_requested = true;
        ++diagnostics_.callback_mutations;
        return true;
    }
    const auto target = record->to;
    if (!apply_value(animation, target)) {
        return true;
    }
    record = find(animation);
    if (record == nullptr) {
        return true;
    }
    record->completion_counted = true;
    ++diagnostics_.completed;
    invoke_completion(animation);
    if (find(animation) != nullptr) {
        static_cast<void>(remove_animation(animation, false));
    }
    return true;
}

bool AnimationRuntime::retarget(
    AnimationId animation,
    AnimationValue to,
    AnimationSpec spec,
    AnimationTime start_time) {
    ensure_owner_thread();
    auto* record = find(animation);
    if (record == nullptr) {
        ++diagnostics_.stale_operations;
        return false;
    }
    validate_animation_value(to);
    const auto* target = find(record->target);
    if (target == nullptr || value_kind(to) != target->value_kind) {
        throw std::invalid_argument(
            "retarget value does not match the animation target kind");
    }

    const auto effective_time = observe_time(start_time);
    static_cast<void>(effective_time + spec.delay + spec.duration);
    AnimationValue current = record->applied.value_or(record->from);
    if (!record->in_callback) {
        current = sample_value(*record, effective_time);
        if (!apply_value(animation, current)) {
            return true;
        }
        record = find(animation);
        if (record == nullptr) {
            return true;
        }
    } else {
        ++diagnostics_.callback_mutations;
    }

    record->from = std::move(current);
    record->to = std::move(to);
    record->spec = spec;
    record->start_time = effective_time;
    ++diagnostics_.retargeted;
    if (spec.duration == AnimationDuration{}) {
        if (record->in_callback) {
            record->finish_requested = true;
        } else {
            static_cast<void>(finish(animation));
        }
    }
    return true;
}

std::size_t AnimationRuntime::tick(AnimationTime sample_time) {
    ensure_owner_thread();
    ++diagnostics_.ticks;
    const auto previous_time = time_cursor_.last();
    const auto observation = time_cursor_.observe(sample_time);
    if (observation.clamped) {
        ++diagnostics_.non_monotonic_timestamps;
        return 0;
    }
    if (!observation.advanced) {
        return 0;
    }
    if (!active_.empty() && previous_time.has_value()) {
        const auto elapsed = observation.effective - *previous_time;
        const auto period = nominal_frame_period_.count_microseconds();
        if (elapsed.count_microseconds() > period) {
            diagnostics_.missed_cadences += static_cast<std::uint64_t>(
                elapsed.count_microseconds() / period - 1);
        }
    }

    tick_snapshot_.assign(active_.begin(), active_.end());
    std::size_t updates = 0;
    for (const auto id : tick_snapshot_) {
        const auto* record = find(id);
        if (record == nullptr) {
            continue;
        }
        const auto interval = sample_animation_interval(
            observation.effective,
            record->start_time,
            record->spec.delay,
            record->spec.duration);
        if (interval.phase == AnimationIntervalPhase::completed) {
            const auto applied_before = diagnostics_.applied_values;
            static_cast<void>(finish(id));
            updates += static_cast<std::size_t>(
                diagnostics_.applied_values - applied_before);
            continue;
        }
        const auto value = interval.phase == AnimationIntervalPhase::delayed
            ? record->from
            : interpolate_animation_value(
                record->from,
                record->to,
                record->spec.easing.sample(interval.progress));
        const auto applied_before = diagnostics_.applied_values;
        static_cast<void>(apply_value(id, value));
        updates += static_cast<std::size_t>(
            diagnostics_.applied_values - applied_before);
    }
    return updates;
}

void AnimationRuntime::set_nominal_frame_period(AnimationDuration period) {
    ensure_owner_thread();
    if (period == AnimationDuration{}) {
        throw std::invalid_argument(
            "nominal animation frame period must be positive");
    }
    nominal_frame_period_ = period;
}

AnimationDuration AnimationRuntime::nominal_frame_period() const noexcept {
    return nominal_frame_period_;
}

std::optional<AnimationTime> AnimationRuntime::next_deadline() const {
    ensure_owner_thread();
    std::optional<AnimationTime> earliest;
    const auto observed = time_cursor_.last();
    const auto period_value = nominal_frame_period_.count_microseconds();
    for (const auto id : active_) {
        const auto* record = find(id);
        if (record == nullptr) {
            continue;
        }
        const auto active_start = record->start_time + record->spec.delay;
        const auto active_end = active_start + record->spec.duration;
        const auto reference = observed.value_or(record->start_time);
        AnimationTime candidate;
        if (reference < active_start) {
            candidate = active_start;
        } else if (reference >= active_end) {
            candidate = reference;
        } else {
            const auto elapsed = reference - active_start;
            const auto step = elapsed.count_microseconds() / period_value + 1;
            const auto duration_value = record->spec.duration.count_microseconds();
            candidate = step > duration_value / period_value
                ? active_end
                : active_start + AnimationDuration::microseconds(step * period_value);
            if (candidate > active_end) {
                candidate = active_end;
            }
        }
        if (!earliest.has_value() || candidate < *earliest) {
            earliest = candidate;
        }
    }
    return earliest;
}

bool AnimationRuntime::contains(AnimationId animation) const {
    ensure_owner_thread();
    return find(animation) != nullptr;
}

bool AnimationRuntime::contains(AnimationScopeId scope) const {
    ensure_owner_thread();
    return find(scope) != nullptr;
}

bool AnimationRuntime::contains(AnimationTargetId target) const {
    ensure_owner_thread();
    return find(target) != nullptr;
}

std::size_t AnimationRuntime::size() const noexcept {
    return diagnostics_.active;
}

const AnimationRuntimeDiagnostics& AnimationRuntime::diagnostics() const noexcept {
    return diagnostics_;
}

AnimationRuntime::ScopeRecord* AnimationRuntime::find(
    AnimationScopeId scope) noexcept {
    if (!scope.valid() || scope.index >= scope_slots_.size()) {
        return nullptr;
    }
    auto& slot = scope_slots_[scope.index];
    return slot.generation == scope.generation && slot.record.has_value()
        ? &*slot.record
        : nullptr;
}

const AnimationRuntime::ScopeRecord* AnimationRuntime::find(
    AnimationScopeId scope) const noexcept {
    if (!scope.valid() || scope.index >= scope_slots_.size()) {
        return nullptr;
    }
    const auto& slot = scope_slots_[scope.index];
    return slot.generation == scope.generation && slot.record.has_value()
        ? &*slot.record
        : nullptr;
}

AnimationRuntime::TargetRecord* AnimationRuntime::find(
    AnimationTargetId target) noexcept {
    if (!target.valid() || target.index >= target_slots_.size()) {
        return nullptr;
    }
    auto& slot = target_slots_[target.index];
    return slot.generation == target.generation && slot.record.has_value()
        ? &*slot.record
        : nullptr;
}

const AnimationRuntime::TargetRecord* AnimationRuntime::find(
    AnimationTargetId target) const noexcept {
    if (!target.valid() || target.index >= target_slots_.size()) {
        return nullptr;
    }
    const auto& slot = target_slots_[target.index];
    return slot.generation == target.generation && slot.record.has_value()
        ? &*slot.record
        : nullptr;
}

AnimationRuntime::AnimationRecord* AnimationRuntime::find(
    AnimationId animation) noexcept {
    if (!animation.valid() || animation.index >= animation_slots_.size()) {
        return nullptr;
    }
    auto& slot = animation_slots_[animation.index];
    return slot.generation == animation.generation && slot.record.has_value()
        ? &*slot.record
        : nullptr;
}

const AnimationRuntime::AnimationRecord* AnimationRuntime::find(
    AnimationId animation) const noexcept {
    if (!animation.valid() || animation.index >= animation_slots_.size()) {
        return nullptr;
    }
    const auto& slot = animation_slots_[animation.index];
    return slot.generation == animation.generation && slot.record.has_value()
        ? &*slot.record
        : nullptr;
}

std::uint32_t AnimationRuntime::acquire_scope_slot() {
    if (!free_scope_slots_.empty()) {
        const auto index = free_scope_slots_.back();
        free_scope_slots_.pop_back();
        return index;
    }
    if (scope_slots_.size() == scope_slots_.capacity()) {
        ++diagnostics_.capacity_growths;
    }
    scope_slots_.emplace_back();
    return static_cast<std::uint32_t>(scope_slots_.size() - 1);
}

std::uint32_t AnimationRuntime::acquire_target_slot() {
    if (!free_target_slots_.empty()) {
        const auto index = free_target_slots_.back();
        free_target_slots_.pop_back();
        return index;
    }
    if (target_slots_.size() == target_slots_.capacity()) {
        ++diagnostics_.capacity_growths;
    }
    target_slots_.emplace_back();
    return static_cast<std::uint32_t>(target_slots_.size() - 1);
}

std::uint32_t AnimationRuntime::acquire_animation_slot() {
    if (!free_animation_slots_.empty()) {
        const auto index = free_animation_slots_.back();
        free_animation_slots_.pop_back();
        return index;
    }
    if (animation_slots_.size() == animation_slots_.capacity()) {
        ++diagnostics_.capacity_growths;
    }
    animation_slots_.emplace_back();
    return static_cast<std::uint32_t>(animation_slots_.size() - 1);
}

void AnimationRuntime::validate_target_binding(
    AnimationValueKind kind,
    AnimationDirtyDomain dirty_domain) const {
    const auto raw = static_cast<std::uint32_t>(dirty_domain);
    const auto allowed = static_cast<std::uint32_t>(allowed_dirty_domains);
    if (raw == 0U || (raw & ~allowed) != 0U) {
        throw std::invalid_argument(
            "animation target dirty domain must be Material, Transform, Geometry, or Animation");
    }
    switch (kind) {
    case AnimationValueKind::scalar:
    case AnimationValueKind::color:
    case AnimationValueKind::point:
    case AnimationValueKind::size:
    case AnimationValueKind::rect:
    case AnimationValueKind::logical_offset:
        return;
    }
    throw std::invalid_argument("animation target value kind is invalid");
}

void AnimationRuntime::validate_play_request(
    const TargetRecord& target,
    const AnimationValue& from,
    const AnimationValue& to,
    const AnimationSpec& spec,
    AnimationTime start_time) const {
    validate_animation_value(from);
    validate_animation_value(to);
    if (value_kind(from) != target.value_kind
            || value_kind(to) != target.value_kind) {
        throw std::invalid_argument(
            "animation endpoints do not match the target value kind");
    }
    static_cast<void>(start_time + spec.delay + spec.duration);
}

AnimationTime AnimationRuntime::observe_time(AnimationTime candidate) noexcept {
    const auto observation = time_cursor_.observe(candidate);
    if (observation.clamped) {
        ++diagnostics_.non_monotonic_timestamps;
    }
    return observation.effective;
}

AnimationValue AnimationRuntime::sample_value(
    const AnimationRecord& record,
    AnimationTime sample_time) const {
    const auto interval = sample_animation_interval(
        sample_time,
        record.start_time,
        record.spec.delay,
        record.spec.duration);
    if (interval.phase == AnimationIntervalPhase::delayed) {
        return record.from;
    }
    if (interval.phase == AnimationIntervalPhase::completed) {
        return record.to;
    }
    return interpolate_animation_value(
        record.from,
        record.to,
        record.spec.easing.sample(interval.progress));
}

bool AnimationRuntime::apply_value(
    AnimationId animation,
    const AnimationValue& value) {
    auto* record = find(animation);
    if (record == nullptr) {
        return false;
    }
    auto* target = find(record->target);
    if (target == nullptr) {
        static_cast<void>(remove_animation(animation, true));
        return false;
    }
    if (record->applied.has_value() && *record->applied == value) {
        return true;
    }

    const auto target_id = target->id;
    auto* sink = target->sink;
    const auto dirty_domain = target->dirty_domain;
    const auto previous = record->applied;
    record->applied = value;
    record->in_callback = true;
    try {
        sink->apply(animation, target_id, value, dirty_domain);
    } catch (...) {
        if (auto* live = find(animation)) {
            live->in_callback = false;
            live->applied = previous;
        }
        throw;
    }
    ++diagnostics_.applied_values;

    record = find(animation);
    if (record == nullptr) {
        return false;
    }
    record->in_callback = false;
    if (record->finish_requested) {
        record->finish_requested = false;
        static_cast<void>(finish(animation));
    }
    return find(animation) != nullptr;
}

void AnimationRuntime::invoke_completion(AnimationId animation) {
    auto* record = find(animation);
    if (record == nullptr) {
        return;
    }
    auto* target = find(record->target);
    if (target == nullptr) {
        return;
    }
    auto* sink = target->sink;
    const auto target_id = target->id;
    record->in_callback = true;
    try {
        sink->completed(animation, target_id);
    } catch (...) {
        if (auto* live = find(animation)) {
            live->in_callback = false;
            static_cast<void>(remove_animation(animation, false));
        }
        throw;
    }
    if (auto* live = find(animation)) {
        live->in_callback = false;
    }
}

bool AnimationRuntime::remove_animation(
    AnimationId animation,
    bool canceled) {
    auto* record = find(animation);
    if (record == nullptr) {
        return false;
    }
    const bool completion_counted = record->completion_counted;
    const auto active = std::find(active_.begin(), active_.end(), animation);
    if (active != active_.end()) {
        active_.erase(active);
    }
    auto& slot = animation_slots_[animation.index];
    slot.record.reset();
    advance_generation(slot.generation);
    free_animation_slots_.push_back(animation.index);
    --diagnostics_.active;
    if (canceled && !completion_counted) {
        ++diagnostics_.canceled;
    }
    return true;
}

void AnimationRuntime::cancel_target_animations(AnimationTargetId target) {
    std::size_t index = 0;
    while (index < active_.size()) {
        const auto animation = active_[index];
        const auto* record = find(animation);
        if (record != nullptr && record->target == target) {
            static_cast<void>(remove_animation(animation, true));
            continue;
        }
        ++index;
    }
}

void AnimationRuntime::ensure_owner_thread() const {
    if (std::this_thread::get_id() != owner_thread_) {
        throw std::logic_error(
            "AnimationRuntime must be used on its owner thread");
    }
}

} // namespace ryn::animation
