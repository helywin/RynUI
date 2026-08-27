#pragma once

#include "runtime/component_host.hpp"
#include "runtime/geometry.hpp"
#include "runtime/node_store.hpp"

#include <cstddef>
#include <cstdint>
#include <functional>
#include <limits>
#include <memory>
#include <optional>
#include <span>
#include <thread>
#include <vector>

namespace ryn::input {

struct InteractionId final {
    static constexpr std::uint32_t invalid_index =
        std::numeric_limits<std::uint32_t>::max();

    std::uint32_t index{invalid_index};
    std::uint32_t generation{0};

    [[nodiscard]] constexpr bool valid() const noexcept {
        return index != invalid_index && generation != 0;
    }

    friend constexpr bool operator==(InteractionId, InteractionId) = default;
};

class PointerDispatchContext;
using PointerEventHandler = std::function<void(PointerDispatchContext&)>;

struct InteractionHandlers final {
    PointerEventHandler capture;
    PointerEventHandler target;
    PointerEventHandler bubble;
};

struct InteractionRegistration final {
    runtime::ComponentId component;
    runtime::NodeId node;
    std::optional<InteractionId> parent;
    bool eligible{true};
    bool focusable{false};
    InteractionHandlers handlers;
};

struct InteractionRecord final {
    InteractionId id;
    runtime::ComponentId component;
    runtime::NodeId node;
    std::optional<InteractionId> parent;
    bool eligible{true};
    bool focusable{false};
    std::shared_ptr<const InteractionHandlers> handlers;
    std::size_t declaration_order{0};
};

class InteractionRegistry final {
public:
    InteractionRegistry(
        runtime::ComponentHost& components,
        runtime::NodeStore& nodes) noexcept;

    void reserve(std::size_t capacity);
    [[nodiscard]] InteractionId create(InteractionRegistration registration);
    bool remove(InteractionId id);
    bool set_eligible(InteractionId id, bool eligible);
    bool set_focusable(InteractionId id, bool focusable);
    bool set_handlers(InteractionId id, InteractionHandlers handlers);

    [[nodiscard]] InteractionRecord* find(InteractionId id);
    [[nodiscard]] const InteractionRecord* find(InteractionId id) const;
    [[nodiscard]] InteractionRecord& require(InteractionId id);
    [[nodiscard]] const InteractionRecord& require(InteractionId id) const;
    [[nodiscard]] bool contains(InteractionId id) const;
    [[nodiscard]] bool is_owner_thread() const noexcept;
    [[nodiscard]] std::size_t size() const;
    [[nodiscard]] std::span<const InteractionId> declaration_order() const;

private:
    struct Slot final {
        std::optional<InteractionRecord> record;
        std::uint32_t generation{1};
    };

    void ensure_owner_thread() const;
    void validate_registration(const InteractionRegistration& registration) const;
    [[nodiscard]] bool node_belongs_to_component(
        runtime::NodeId node,
        runtime::ComponentId component) const;
    [[nodiscard]] bool component_is_same_or_ancestor(
        runtime::ComponentId ancestor,
        runtime::ComponentId component) const;
    [[nodiscard]] InteractionRecord* find_slot_record(InteractionId id) noexcept;
    [[nodiscard]] const InteractionRecord* find_slot_record(
        InteractionId id) const noexcept;
    [[nodiscard]] bool associations_are_live(const InteractionRecord& record) const;
    [[nodiscard]] std::uint32_t acquire_slot();
    static void advance_generation(Slot& slot) noexcept;

    runtime::ComponentHost* components_;
    runtime::NodeStore* nodes_;
    std::thread::id owner_thread_;
    std::vector<Slot> slots_;
    std::vector<std::uint32_t> free_slots_;
    std::vector<InteractionId> declaration_order_;
    std::size_t live_records_{0};
    std::size_t next_declaration_order_{0};
};

struct HitTestPaintEntry final {
    InteractionId interaction;
    std::optional<runtime::Rect> clip;
};

struct HitTestRecord final {
    InteractionId interaction;
    runtime::NodeId node;
    std::optional<InteractionId> parent;
    runtime::Rect translated_bounds;
    runtime::Rect effective_clip;
    std::optional<runtime::Rect> source_clip;
    std::size_t depth{0};
    std::size_t paint_order{0};
    bool effective_eligible{false};
    bool has_committed_layout{false};
};

struct HitTestDiagnostics final {
    std::uint64_t snapshot_rebuilds{0};
    std::uint64_t records_refreshed{0};
    std::uint64_t queries{0};
    std::uint64_t records_examined{0};
    std::uint64_t hits{0};
    std::uint64_t stale_skips{0};

    friend bool operator==(const HitTestDiagnostics&, const HitTestDiagnostics&) = default;
};

class HitTestSnapshot final {
public:
    HitTestSnapshot(
        InteractionRegistry& registry,
        runtime::NodeStore& nodes) noexcept;

    void reserve(std::size_t capacity);
    void rebuild(
        std::span<const HitTestPaintEntry> paint_entries,
        runtime::Rect window_clip);
    std::size_t refresh(std::span<const runtime::NodeId> dirty_nodes);
    std::size_t refresh_interaction(InteractionId interaction);
    [[nodiscard]] std::optional<InteractionId> hit_test(runtime::Point point);

    [[nodiscard]] std::span<const HitTestRecord> records() const noexcept;
    [[nodiscard]] std::size_t capacity() const noexcept;
    [[nodiscard]] const HitTestDiagnostics& diagnostics() const noexcept;

private:
    [[nodiscard]] HitTestRecord make_record(
        const HitTestPaintEntry& entry,
        std::size_t paint_order,
        std::span<const HitTestRecord> preceding,
        runtime::Rect window_clip) const;
    bool refresh_record(std::size_t index);
    [[nodiscard]] const HitTestRecord* find_preceding(
        InteractionId id,
        std::span<const HitTestRecord> records) const noexcept;
    [[nodiscard]] bool snapshot_descends_from(
        const HitTestRecord& record,
        InteractionId ancestor) const noexcept;
    [[nodiscard]] bool node_descends_from(
        runtime::NodeId node,
        runtime::NodeId ancestor) const noexcept;

    InteractionRegistry* registry_;
    runtime::NodeStore* nodes_;
    runtime::Rect window_clip_;
    std::vector<HitTestRecord> records_;
    HitTestDiagnostics diagnostics_;
};

} // namespace ryn::input
