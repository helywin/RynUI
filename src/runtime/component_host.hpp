#pragma once

#include "runtime/node_store.hpp"
#include "theme/theme_runtime.hpp"

#include <ryn/component.hpp>
#include <ryn/prop.hpp>
#include <ryn/reactive.hpp>

#include <array>
#include <cstddef>
#include <cstdint>
#include <functional>
#include <limits>
#include <memory>
#include <optional>
#include <span>
#include <stdexcept>
#include <thread>
#include <typeindex>
#include <utility>
#include <vector>

namespace ryn::detail {

struct SlotContentAccess final {
    template <typename SlotTag>
    [[nodiscard]] static const std::function<void()>& function(
        const SlotContent<SlotTag>& content) noexcept {
        return content.function_;
    }
};

} // namespace ryn::detail

namespace ryn::runtime {

using SemanticForeground = std::array<float, 4>;

struct SemanticTypography final {
    SystemFontFamily font_family{SystemFontFamily::ui_sans};
    std::uint32_t font_weight{400};
    float font_size{14.0F};
    float line_height{22.0F};

    friend constexpr bool operator==(
        SemanticTypography,
        SemanticTypography) = default;
};

struct ComponentId final {
    static constexpr std::uint32_t invalid_index =
        std::numeric_limits<std::uint32_t>::max();

    std::uint32_t index{invalid_index};
    std::uint32_t generation{0};

    [[nodiscard]] constexpr bool valid() const noexcept {
        return index != invalid_index && generation != 0;
    }

    friend constexpr bool operator==(ComponentId, ComponentId) = default;
};

struct SceneFragmentId final {
    static constexpr std::uint32_t invalid_index =
        std::numeric_limits<std::uint32_t>::max();

    std::uint32_t index{invalid_index};
    std::uint32_t generation{0};

    [[nodiscard]] constexpr bool valid() const noexcept {
        return index != invalid_index && generation != 0;
    }

    friend constexpr bool operator==(SceneFragmentId, SceneFragmentId) = default;
};

enum class SceneFragmentPlacement : std::uint8_t {
    before_children,
    after_children,
};

struct SceneFragmentPaintEntry final {
    SceneFragmentId fragment;
    ComponentId component;
    SceneFragmentPlacement placement{SceneFragmentPlacement::before_children};

    friend constexpr bool operator==(
        SceneFragmentPaintEntry,
        SceneFragmentPaintEntry) = default;
};

class ComponentBuildContext;

class ComponentHost final {
public:
    explicit ComponentHost(NodeStore& nodes);
    ComponentHost(const ComponentHost&) = delete;
    ComponentHost& operator=(const ComponentHost&) = delete;
    ComponentHost(ComponentHost&&) = delete;
    ComponentHost& operator=(ComponentHost&&) = delete;
    ~ComponentHost();

    void mount(const Content& content);
    bool destroy(ComponentId id) noexcept;
    void dispose() noexcept;

    [[nodiscard]] bool active() const noexcept;
    [[nodiscard]] bool is_owner_thread() const noexcept;
    [[nodiscard]] bool contains(ComponentId id) const noexcept;
    [[nodiscard]] std::size_t component_count() const noexcept;
    [[nodiscard]] std::size_t mount_runs() const noexcept;
    [[nodiscard]] NodeId root(ComponentId id) const;
    [[nodiscard]] std::optional<ComponentId> parent(ComponentId id) const;
    [[nodiscard]] const std::vector<ComponentId>& children(ComponentId id) const;
    [[nodiscard]] std::span<const ComponentId> root_components() const noexcept;
    [[nodiscard]] std::size_t declaration_order(ComponentId id) const;
    [[nodiscard]] Scope& scope(ComponentId id);
    [[nodiscard]] const std::shared_ptr<theme_runtime::ThemeScope>& theme_scope(
        ComponentId id) const;
    bool remove_scene_fragment(SceneFragmentId id);
    [[nodiscard]] bool contains(SceneFragmentId id) const noexcept;
    [[nodiscard]] std::span<const SceneFragmentPaintEntry> paint_traversal();

    template <typename State>
    [[nodiscard]] State* state(ComponentId id) noexcept {
        return static_cast<State*>(find_state(id, std::type_index(typeid(State))));
    }

    template <typename State>
    [[nodiscard]] const State* state(ComponentId id) const noexcept {
        return static_cast<const State*>(
            find_state(id, std::type_index(typeid(State))));
    }

private:
    friend class ComponentBuildContext;

    struct Record;
    struct Slot;
    struct FragmentRecord;
    struct FragmentSlot;

    [[nodiscard]] ComponentId create_record(
        std::optional<ComponentId> parent,
        std::shared_ptr<void> state,
        std::type_index state_type);
    void add_resource_cleanup(ComponentId id, std::function<void()> cleanup);
    [[nodiscard]] SceneFragmentId register_scene_fragment(
        ComponentId component,
        SceneFragmentPlacement placement);
    void mount_slot(
        ComponentId parent,
        const std::function<void()>& content,
        std::optional<Prop<SemanticForeground>> semantic_foreground,
        std::optional<Prop<SemanticTypography>> semantic_typography,
        std::shared_ptr<theme_runtime::ThemeScope> theme_scope);
    void mount_transparent_slot(
        std::optional<ComponentId> parent,
        const std::function<void()>& content,
        std::optional<Prop<SemanticForeground>> semantic_foreground,
        std::optional<Prop<SemanticTypography>> semantic_typography,
        std::shared_ptr<theme_runtime::ThemeScope> theme_scope);
    void ensure_owner_thread() const;
    [[nodiscard]] Record* find_record(ComponentId id) noexcept;
    [[nodiscard]] const Record* find_record(ComponentId id) const noexcept;
    [[nodiscard]] Record& require_record(ComponentId id);
    [[nodiscard]] const Record& require_record(ComponentId id) const;
    [[nodiscard]] void* find_state(ComponentId id, std::type_index type) noexcept;
    [[nodiscard]] const void* find_state(
        ComponentId id,
        std::type_index type) const noexcept;
    [[nodiscard]] std::uint32_t acquire_slot();
    [[nodiscard]] std::uint32_t acquire_fragment_slot();
    void release_slot(ComponentId id) noexcept;
    void release_component_fragments(Record& record) noexcept;
    void append_paint_subtree(ComponentId id);
    void collect_subtree(ComponentId id, std::vector<ComponentId>& ids) const;
    void dispose_records(
        const std::vector<ComponentId>& ids,
        const std::vector<NodeId>& roots) noexcept;
    static void advance_generation(Slot& slot) noexcept;
    static void advance_generation(FragmentSlot& slot) noexcept;

    NodeStore* nodes_;
    std::thread::id owner_thread_;
    Scope host_scope_;
    std::shared_ptr<theme_runtime::ThemeScope> default_theme_scope_;
    std::vector<Slot> slots_;
    std::vector<std::uint32_t> free_slots_;
    std::vector<ComponentId> root_components_;
    std::vector<FragmentSlot> fragment_slots_;
    std::vector<std::uint32_t> free_fragment_slots_;
    std::vector<SceneFragmentPaintEntry> paint_traversal_;
    std::size_t live_components_{0};
    std::size_t next_declaration_order_{0};
    std::size_t mount_runs_{0};
    bool active_{true};
    bool mounting_{false};
    bool mounted_{false};
    bool paint_traversal_dirty_{true};
};

class ComponentBuildContext final {
public:
    template <typename State, typename... Arguments>
    [[nodiscard]] ComponentId mount_component(Arguments&&... arguments) {
        auto state = std::make_shared<State>(
            std::forward<Arguments>(arguments)...);
        return host_->create_record(
            parent_,
            std::move(state),
            std::type_index(typeid(State)));
    }

    template <typename SlotTag>
    void mount_slot(ComponentId parent, const SlotContent<SlotTag>& content) {
        host_->mount_slot(
            parent,
            detail::SlotContentAccess::function(content),
            semantic_foreground_,
            semantic_typography_,
            theme_scope_);
    }

    template <typename SlotTag>
    void mount_slot_with_semantic_foreground(
        ComponentId parent,
        const SlotContent<SlotTag>& content,
        Prop<SemanticForeground> foreground) {
        host_->mount_slot(
            parent,
            detail::SlotContentAccess::function(content),
            std::move(foreground),
            semantic_typography_,
            theme_scope_);
    }

    template <typename SlotTag>
    void mount_slot_with_semantic_text_style(
        ComponentId parent,
        const SlotContent<SlotTag>& content,
        Prop<SemanticForeground> foreground,
        Prop<SemanticTypography> typography) {
        host_->mount_slot(
            parent,
            detail::SlotContentAccess::function(content),
            std::move(foreground),
            std::move(typography),
            theme_scope_);
    }

    template <typename SlotTag>
    void mount_slot_with_theme_scope(
        const SlotContent<SlotTag>& content,
        std::shared_ptr<theme_runtime::ThemeScope> theme_scope) {
        host_->mount_transparent_slot(
            parent_,
            detail::SlotContentAccess::function(content),
            semantic_foreground_,
            semantic_typography_,
            std::move(theme_scope));
    }

    void on_resource_cleanup(ComponentId id, std::function<void()> cleanup);
    [[nodiscard]] SceneFragmentId register_scene_fragment(
        ComponentId id,
        SceneFragmentPlacement placement);
    [[nodiscard]] Scope& scope(ComponentId id);
    [[nodiscard]] Scope& lifetime_scope();
    [[nodiscard]] const std::shared_ptr<theme_runtime::ThemeScope>&
    theme_scope() const noexcept;
    [[nodiscard]] NodeId root(ComponentId id) const;
    [[nodiscard]] const std::optional<Prop<SemanticForeground>>&
    semantic_foreground() const noexcept;
    [[nodiscard]] const std::optional<Prop<SemanticTypography>>&
    semantic_typography() const noexcept;

    template <typename State>
    [[nodiscard]] State& state(ComponentId id) {
        auto* value = host_->state<State>(id);
        if (value == nullptr) {
            throw std::out_of_range("Component state type does not match");
        }
        return *value;
    }

private:
    friend class ComponentHost;

    ComponentBuildContext(
        ComponentHost& host,
        std::optional<ComponentId> parent,
        std::optional<Prop<SemanticForeground>> semantic_foreground,
        std::optional<Prop<SemanticTypography>> semantic_typography,
        std::shared_ptr<theme_runtime::ThemeScope> theme_scope) noexcept;

    ComponentHost* host_;
    std::optional<ComponentId> parent_;
    std::optional<Prop<SemanticForeground>> semantic_foreground_;
    std::optional<Prop<SemanticTypography>> semantic_typography_;
    std::shared_ptr<theme_runtime::ThemeScope> theme_scope_;
};

[[nodiscard]] ComponentBuildContext& require_component_build_context();

} // namespace ryn::runtime
