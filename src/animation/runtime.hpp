#pragma once

#include "animation/easing.hpp"
#include "animation/time.hpp"
#include "animation/value.hpp"

#include <cstddef>
#include <cstdint>
#include <limits>
#include <optional>
#include <thread>
#include <vector>

namespace ryn::animation {

struct AnimationId final {
    static constexpr std::uint32_t invalid_index =
        std::numeric_limits<std::uint32_t>::max();

    std::uint32_t index{invalid_index};
    std::uint32_t generation{0};

    [[nodiscard]] constexpr bool valid() const noexcept {
        return index != invalid_index && generation != 0;
    }

    friend constexpr bool operator==(AnimationId, AnimationId) = default;
};

struct AnimationScopeId final {
    static constexpr std::uint32_t invalid_index =
        std::numeric_limits<std::uint32_t>::max();

    std::uint32_t index{invalid_index};
    std::uint32_t generation{0};

    [[nodiscard]] constexpr bool valid() const noexcept {
        return index != invalid_index && generation != 0;
    }

    friend constexpr bool operator==(
        AnimationScopeId,
        AnimationScopeId) = default;
};

struct AnimationTargetId final {
    static constexpr std::uint32_t invalid_index =
        std::numeric_limits<std::uint32_t>::max();

    std::uint32_t index{invalid_index};
    std::uint32_t generation{0};

    [[nodiscard]] constexpr bool valid() const noexcept {
        return index != invalid_index && generation != 0;
    }

    friend constexpr bool operator==(
        AnimationTargetId,
        AnimationTargetId) = default;
};

enum class AnimationDirtyDomain : std::uint32_t {
    none = 0,
    material = 1U << 0U,
    transform = 1U << 1U,
    geometry = 1U << 2U,
    animation = 1U << 3U,
    structure = 1U << 4U,
    measure_layout = 1U << 5U,
};

[[nodiscard]] constexpr AnimationDirtyDomain operator|(
    AnimationDirtyDomain left,
    AnimationDirtyDomain right) noexcept {
    return static_cast<AnimationDirtyDomain>(
        static_cast<std::uint32_t>(left)
        | static_cast<std::uint32_t>(right));
}

[[nodiscard]] constexpr AnimationDirtyDomain operator&(
    AnimationDirtyDomain left,
    AnimationDirtyDomain right) noexcept {
    return static_cast<AnimationDirtyDomain>(
        static_cast<std::uint32_t>(left)
        & static_cast<std::uint32_t>(right));
}

[[nodiscard]] constexpr bool has_any(
    AnimationDirtyDomain value,
    AnimationDirtyDomain mask) noexcept {
    return static_cast<std::uint32_t>(value & mask) != 0U;
}

struct AnimationSpec final {
    AnimationDuration delay;
    AnimationDuration duration;
    Easing easing{Easing::linear()};

    friend constexpr bool operator==(
        const AnimationSpec&,
        const AnimationSpec&) = default;
};

class AnimationTargetSink {
public:
    virtual ~AnimationTargetSink() = default;

    virtual void apply(
        AnimationId animation,
        AnimationTargetId target,
        const AnimationValue& value,
        AnimationDirtyDomain dirty_domain) = 0;

    virtual void completed(
        AnimationId animation,
        AnimationTargetId target);
};

struct AnimationRuntimeDiagnostics final {
    std::uint64_t created{0};
    std::uint64_t completed{0};
    std::uint64_t canceled{0};
    std::uint64_t retargeted{0};
    std::uint64_t stale_operations{0};
    std::uint64_t non_monotonic_timestamps{0};
    std::uint64_t ticks{0};
    std::uint64_t applied_values{0};
    std::uint64_t callback_mutations{0};
    std::uint64_t capacity_growths{0};
    std::size_t active{0};
    std::size_t scopes{0};
    std::size_t targets{0};
};

class AnimationRuntime final {
public:
    AnimationRuntime() noexcept;

    void reserve(
        std::size_t animation_capacity,
        std::size_t scope_capacity,
        std::size_t target_capacity);

    [[nodiscard]] AnimationScopeId create_scope();
    bool dispose_scope(AnimationScopeId scope);

    [[nodiscard]] AnimationTargetId register_target(
        AnimationScopeId scope,
        AnimationTargetSink& sink,
        AnimationValueKind value_kind,
        AnimationDirtyDomain dirty_domain);
    bool unregister_target(AnimationTargetId target);

    [[nodiscard]] AnimationId play(
        AnimationTargetId target,
        AnimationValue from,
        AnimationValue to,
        AnimationSpec spec,
        AnimationTime start_time);
    bool cancel(AnimationId animation, AnimationTime sample_time);
    bool finish(AnimationId animation);
    bool retarget(
        AnimationId animation,
        AnimationValue to,
        AnimationSpec spec,
        AnimationTime start_time);
    [[nodiscard]] std::size_t tick(AnimationTime sample_time);

    [[nodiscard]] bool contains(AnimationId animation) const;
    [[nodiscard]] bool contains(AnimationScopeId scope) const;
    [[nodiscard]] bool contains(AnimationTargetId target) const;
    [[nodiscard]] std::size_t size() const noexcept;
    [[nodiscard]] const AnimationRuntimeDiagnostics& diagnostics() const noexcept;

private:
    struct ScopeRecord final {
        AnimationScopeId id;
    };

    struct ScopeSlot final {
        std::optional<ScopeRecord> record;
        std::uint32_t generation{1};
    };

    struct TargetRecord final {
        AnimationTargetId id;
        AnimationScopeId scope;
        AnimationTargetSink* sink{nullptr};
        AnimationValueKind value_kind{AnimationValueKind::scalar};
        AnimationDirtyDomain dirty_domain{AnimationDirtyDomain::none};
    };

    struct TargetSlot final {
        std::optional<TargetRecord> record;
        std::uint32_t generation{1};
    };

    struct AnimationRecord final {
        AnimationId id;
        AnimationTargetId target;
        AnimationValue from;
        AnimationValue to;
        AnimationSpec spec;
        AnimationTime start_time;
        std::optional<AnimationValue> applied;
        bool in_callback{false};
        bool finish_requested{false};
        bool completion_counted{false};
    };

    struct AnimationSlot final {
        std::optional<AnimationRecord> record;
        std::uint32_t generation{1};
    };

    [[nodiscard]] ScopeRecord* find(AnimationScopeId scope) noexcept;
    [[nodiscard]] const ScopeRecord* find(AnimationScopeId scope) const noexcept;
    [[nodiscard]] TargetRecord* find(AnimationTargetId target) noexcept;
    [[nodiscard]] const TargetRecord* find(AnimationTargetId target) const noexcept;
    [[nodiscard]] AnimationRecord* find(AnimationId animation) noexcept;
    [[nodiscard]] const AnimationRecord* find(AnimationId animation) const noexcept;

    [[nodiscard]] std::uint32_t acquire_scope_slot();
    [[nodiscard]] std::uint32_t acquire_target_slot();
    [[nodiscard]] std::uint32_t acquire_animation_slot();
    void validate_target_binding(
        AnimationValueKind kind,
        AnimationDirtyDomain dirty_domain) const;
    void validate_play_request(
        const TargetRecord& target,
        const AnimationValue& from,
        const AnimationValue& to,
        const AnimationSpec& spec,
        AnimationTime start_time) const;

    [[nodiscard]] AnimationTime observe_time(AnimationTime candidate) noexcept;
    [[nodiscard]] AnimationValue sample_value(
        const AnimationRecord& record,
        AnimationTime sample_time) const;
    [[nodiscard]] bool apply_value(
        AnimationId animation,
        const AnimationValue& value);
    void invoke_completion(AnimationId animation);
    bool remove_animation(AnimationId animation, bool canceled);
    void cancel_target_animations(AnimationTargetId target);
    void ensure_owner_thread() const;

    std::thread::id owner_thread_;
    MonotonicTimeCursor time_cursor_;
    std::vector<ScopeSlot> scope_slots_;
    std::vector<std::uint32_t> free_scope_slots_;
    std::vector<TargetSlot> target_slots_;
    std::vector<std::uint32_t> free_target_slots_;
    std::vector<AnimationSlot> animation_slots_;
    std::vector<std::uint32_t> free_animation_slots_;
    std::vector<AnimationId> active_;
    std::vector<AnimationId> tick_snapshot_;
    AnimationRuntimeDiagnostics diagnostics_;
};

} // namespace ryn::animation
