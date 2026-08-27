#include "runtime/component_host.hpp"

#include <algorithm>
#include <stdexcept>
#include <utility>

namespace ryn::runtime {
namespace {

thread_local ComponentBuildContext* active_build_context = nullptr;

class ActiveBuildContextGuard final {
public:
    explicit ActiveBuildContextGuard(ComponentBuildContext& context) noexcept
        : previous_(active_build_context) {
        active_build_context = &context;
    }

    ActiveBuildContextGuard(const ActiveBuildContextGuard&) = delete;
    ActiveBuildContextGuard& operator=(const ActiveBuildContextGuard&) = delete;

    ~ActiveBuildContextGuard() {
        active_build_context = previous_;
    }

private:
    ComponentBuildContext* previous_;
};

} // namespace

struct ComponentHost::Record final {
    std::optional<ComponentId> parent;
    std::vector<ComponentId> children;
    NodeId root;
    Scope scope;
    std::shared_ptr<theme_runtime::ThemeScope> theme_scope;
    std::shared_ptr<void> state;
    std::type_index state_type{typeid(void)};
    std::vector<std::function<void()>> resource_cleanups;
    std::vector<SceneFragmentId> before_children_fragments;
    std::vector<SceneFragmentId> after_children_fragments;
    std::size_t declaration_order{0};

    Record(
        std::optional<ComponentId> component_parent,
        NodeId component_root,
        std::shared_ptr<void> component_state,
        std::type_index component_state_type,
        std::size_t order,
        std::shared_ptr<theme_runtime::ThemeScope> component_theme_scope)
        : parent(component_parent),
          root(component_root),
          state(std::move(component_state)),
          state_type(component_state_type),
          declaration_order(order),
          theme_scope(std::move(component_theme_scope)) {}
};

struct ComponentHost::Slot final {
    std::unique_ptr<Record> record;
    std::uint32_t generation{1};
};

struct ComponentHost::FragmentRecord final {
    ComponentId component;
    SceneFragmentPlacement placement{SceneFragmentPlacement::before_children};
};

struct ComponentHost::FragmentSlot final {
    std::optional<FragmentRecord> record;
    std::uint32_t generation{1};
};

ComponentHost::ComponentHost(NodeStore& nodes)
    : nodes_(&nodes),
      owner_thread_(std::this_thread::get_id()),
      default_theme_scope_(theme_runtime::ThemeScope::create_default()) {}

ComponentHost::~ComponentHost() {
    dispose();
}

void ComponentHost::mount(const Content& content) {
    ensure_owner_thread();
    if (!active_) {
        throw std::logic_error("Cannot mount content into a disposed ComponentHost");
    }
    if (mounting_) {
        throw std::logic_error("ComponentHost content mount cannot reenter");
    }
    if (mounted_) {
        throw std::logic_error("ComponentHost content can only mount once");
    }

    mounting_ = true;
    ++mount_runs_;
    ComponentBuildContext context(
        *this,
        std::nullopt,
        std::nullopt,
        default_theme_scope_);
    try {
        ActiveBuildContextGuard guard(context);
        detail::SlotContentAccess::function(content)();
        mounting_ = false;
        mounted_ = true;
    } catch (...) {
        mounting_ = false;
        dispose();
        throw;
    }
}

bool ComponentHost::destroy(ComponentId id) noexcept {
    if (std::this_thread::get_id() != owner_thread_) {
        return false;
    }
    auto* record = find_record(id);
    if (!active_ || record == nullptr) {
        return false;
    }

    std::vector<ComponentId> ids;
    try {
        collect_subtree(id, ids);
    } catch (...) {
        return false;
    }
    const std::vector<NodeId> roots{record->root};
    if (record->parent.has_value()) {
        if (auto* parent_record = find_record(*record->parent)) {
            std::erase(parent_record->children, id);
        }
    } else {
        std::erase(root_components_, id);
    }
    paint_traversal_dirty_ = true;
    dispose_records(ids, roots);
    return true;
}

void ComponentHost::dispose() noexcept {
    if (!active_) {
        return;
    }
    active_ = false;
    mounting_ = false;

    std::vector<ComponentId> ids;
    std::vector<NodeId> roots;
    try {
        ids.reserve(live_components_);
        roots.reserve(live_components_);
        for (std::uint32_t index = 0; index < slots_.size(); ++index) {
            const auto& slot = slots_[index];
            if (slot.record == nullptr) {
                continue;
            }
            const ComponentId id{index, slot.generation};
            ids.push_back(id);
            if (!slot.record->parent.has_value()) {
                roots.push_back(slot.record->root);
            }
        }
    } catch (...) {
        for (auto& slot : slots_) {
            if (slot.record != nullptr) {
                slot.record->scope.dispose();
            }
        }
        host_scope_.dispose();
        return;
    }
    dispose_records(ids, roots);
    host_scope_.dispose();
}

bool ComponentHost::active() const noexcept {
    return active_;
}

bool ComponentHost::is_owner_thread() const noexcept {
    return std::this_thread::get_id() == owner_thread_;
}

bool ComponentHost::contains(ComponentId id) const noexcept {
    return find_record(id) != nullptr;
}

std::size_t ComponentHost::component_count() const noexcept {
    return live_components_;
}

std::size_t ComponentHost::mount_runs() const noexcept {
    return mount_runs_;
}

NodeId ComponentHost::root(ComponentId id) const {
    return require_record(id).root;
}

std::optional<ComponentId> ComponentHost::parent(ComponentId id) const {
    return require_record(id).parent;
}

const std::vector<ComponentId>& ComponentHost::children(ComponentId id) const {
    return require_record(id).children;
}

std::span<const ComponentId> ComponentHost::root_components() const noexcept {
    return root_components_;
}

std::size_t ComponentHost::declaration_order(ComponentId id) const {
    return require_record(id).declaration_order;
}

Scope& ComponentHost::scope(ComponentId id) {
    return require_record(id).scope;
}

const std::shared_ptr<theme_runtime::ThemeScope>& ComponentHost::theme_scope(
    ComponentId id) const {
    return require_record(id).theme_scope;
}

bool ComponentHost::remove_scene_fragment(SceneFragmentId id) {
    ensure_owner_thread();
    if (!id.valid() || id.index >= fragment_slots_.size()) {
        return false;
    }
    auto& slot = fragment_slots_[id.index];
    if (slot.generation != id.generation || !slot.record.has_value()) {
        return false;
    }
    if (auto* component = find_record(slot.record->component)) {
        auto& fragments = slot.record->placement
                == SceneFragmentPlacement::before_children
            ? component->before_children_fragments
            : component->after_children_fragments;
        std::erase(fragments, id);
    }
    slot.record.reset();
    advance_generation(slot);
    free_fragment_slots_.push_back(id.index);
    paint_traversal_dirty_ = true;
    return true;
}

bool ComponentHost::contains(SceneFragmentId id) const noexcept {
    if (!id.valid() || id.index >= fragment_slots_.size()) {
        return false;
    }
    const auto& slot = fragment_slots_[id.index];
    return slot.generation == id.generation && slot.record.has_value()
        && contains(slot.record->component);
}

std::span<const SceneFragmentPaintEntry> ComponentHost::paint_traversal() {
    ensure_owner_thread();
    if (!paint_traversal_dirty_) {
        return paint_traversal_;
    }
    paint_traversal_.clear();
    for (const auto root : root_components_) {
        if (contains(root)) {
            append_paint_subtree(root);
        }
    }
    paint_traversal_dirty_ = false;
    return paint_traversal_;
}

ComponentId ComponentHost::create_record(
    std::optional<ComponentId> parent,
    std::shared_ptr<void> state,
    std::type_index state_type) {
    ensure_owner_thread();
    if (!active_ || !mounting_ || active_build_context == nullptr) {
        throw std::logic_error("Components can only be declared during Host mount");
    }

    Record* parent_record = nullptr;
    if (parent.has_value()) {
        parent_record = &require_record(*parent);
    }
    const NodeId node = parent_record == nullptr
        ? nodes_->create_root()
        : nodes_->create_child(parent_record->root);

    std::uint32_t slot_index = ComponentId::invalid_index;
    try {
        slot_index = acquire_slot();
        auto record = std::make_unique<Record>(
            parent,
            node,
            std::move(state),
            state_type,
            next_declaration_order_++,
            active_build_context->theme_scope());
        const ComponentId id{slot_index, slots_[slot_index].generation};
        slots_[slot_index].record = std::move(record);
        if (parent_record != nullptr) {
            parent_record->children.push_back(id);
        } else {
            root_components_.push_back(id);
        }
        paint_traversal_dirty_ = true;
        ++live_components_;
        return id;
    } catch (...) {
        static_cast<void>(nodes_->destroy(node));
        if (slot_index != ComponentId::invalid_index) {
            const ComponentId id{slot_index, slots_[slot_index].generation};
            if (slots_[slot_index].record != nullptr) {
                slots_[slot_index].record.reset();
            }
            try {
                free_slots_.push_back(slot_index);
            } catch (...) {
            }
            static_cast<void>(id);
        }
        throw;
    }
}

SceneFragmentId ComponentHost::register_scene_fragment(
    ComponentId component,
    SceneFragmentPlacement placement) {
    ensure_owner_thread();
    auto& owner = require_record(component);
    const auto slot_index = acquire_fragment_slot();
    auto& slot = fragment_slots_[slot_index];
    const SceneFragmentId id{slot_index, slot.generation};
    try {
        slot.record.emplace(FragmentRecord{component, placement});
        auto& fragments = placement == SceneFragmentPlacement::before_children
            ? owner.before_children_fragments
            : owner.after_children_fragments;
        fragments.push_back(id);
    } catch (...) {
        slot.record.reset();
        try {
            free_fragment_slots_.push_back(slot_index);
        } catch (...) {
        }
        throw;
    }
    paint_traversal_dirty_ = true;
    return id;
}

void ComponentHost::add_resource_cleanup(
    ComponentId id,
    std::function<void()> cleanup) {
    ensure_owner_thread();
    require_record(id).resource_cleanups.push_back(std::move(cleanup));
}

void ComponentHost::mount_slot(
    ComponentId parent,
    const std::function<void()>& content,
    std::optional<Prop<SemanticForeground>> semantic_foreground,
    std::shared_ptr<theme_runtime::ThemeScope> theme_scope) {
    ensure_owner_thread();
    static_cast<void>(require_record(parent));
    if (!mounting_ || active_build_context == nullptr) {
        throw std::logic_error("Typed slots can only run during Host mount");
    }

    ComponentBuildContext context(
        *this,
        parent,
        std::move(semantic_foreground),
        std::move(theme_scope));
    ActiveBuildContextGuard guard(context);
    content();
}

void ComponentHost::mount_transparent_slot(
    std::optional<ComponentId> parent,
    const std::function<void()>& content,
    std::optional<Prop<SemanticForeground>> semantic_foreground,
    std::shared_ptr<theme_runtime::ThemeScope> theme_scope) {
    ensure_owner_thread();
    if (!mounting_ || active_build_context == nullptr) {
        throw std::logic_error("Transparent slots can only run during Host mount");
    }
    if (!theme_scope) {
        throw std::invalid_argument("Transparent slot requires a Theme scope");
    }
    if (parent.has_value()) {
        static_cast<void>(require_record(*parent));
    }

    ComponentBuildContext context(
        *this,
        parent,
        std::move(semantic_foreground),
        std::move(theme_scope));
    ActiveBuildContextGuard guard(context);
    content();
}

void ComponentHost::ensure_owner_thread() const {
    if (!is_owner_thread()) {
        throw std::logic_error("ComponentHost can only be used on its owner thread");
    }
}

ComponentHost::Record* ComponentHost::find_record(ComponentId id) noexcept {
    if (!id.valid() || id.index >= slots_.size()) {
        return nullptr;
    }
    auto& slot = slots_[id.index];
    if (slot.generation != id.generation) {
        return nullptr;
    }
    return slot.record.get();
}

const ComponentHost::Record* ComponentHost::find_record(
    ComponentId id) const noexcept {
    if (!id.valid() || id.index >= slots_.size()) {
        return nullptr;
    }
    const auto& slot = slots_[id.index];
    if (slot.generation != id.generation) {
        return nullptr;
    }
    return slot.record.get();
}

ComponentHost::Record& ComponentHost::require_record(ComponentId id) {
    if (auto* record = find_record(id)) {
        return *record;
    }
    throw std::out_of_range("ComponentId is stale or invalid");
}

const ComponentHost::Record& ComponentHost::require_record(ComponentId id) const {
    if (const auto* record = find_record(id)) {
        return *record;
    }
    throw std::out_of_range("ComponentId is stale or invalid");
}

void* ComponentHost::find_state(ComponentId id, std::type_index type) noexcept {
    auto* record = find_record(id);
    if (record == nullptr || record->state_type != type) {
        return nullptr;
    }
    return record->state.get();
}

const void* ComponentHost::find_state(
    ComponentId id,
    std::type_index type) const noexcept {
    const auto* record = find_record(id);
    if (record == nullptr || record->state_type != type) {
        return nullptr;
    }
    return record->state.get();
}

std::uint32_t ComponentHost::acquire_slot() {
    if (!free_slots_.empty()) {
        const auto index = free_slots_.back();
        free_slots_.pop_back();
        return index;
    }
    if (slots_.size() >= ComponentId::invalid_index) {
        throw std::length_error("ComponentHost exhausted ComponentId indices");
    }
    slots_.emplace_back();
    return static_cast<std::uint32_t>(slots_.size() - 1);
}

std::uint32_t ComponentHost::acquire_fragment_slot() {
    if (!free_fragment_slots_.empty()) {
        const auto index = free_fragment_slots_.back();
        free_fragment_slots_.pop_back();
        return index;
    }
    if (fragment_slots_.size() >= SceneFragmentId::invalid_index) {
        throw std::length_error("ComponentHost exhausted SceneFragmentId indices");
    }
    fragment_slots_.emplace_back();
    return static_cast<std::uint32_t>(fragment_slots_.size() - 1);
}

void ComponentHost::release_slot(ComponentId id) noexcept {
    if (id.index >= slots_.size()) {
        return;
    }
    auto& slot = slots_[id.index];
    if (slot.generation != id.generation || slot.record == nullptr) {
        return;
    }
    release_component_fragments(*slot.record);
    std::erase(root_components_, id);
    slot.record.reset();
    advance_generation(slot);
    try {
        free_slots_.push_back(id.index);
    } catch (...) {
    }
    --live_components_;
    paint_traversal_dirty_ = true;
}

void ComponentHost::release_component_fragments(Record& record) noexcept {
    const auto release = [&](const std::vector<SceneFragmentId>& fragments) {
        for (const auto id : fragments) {
            if (!id.valid() || id.index >= fragment_slots_.size()) {
                continue;
            }
            auto& slot = fragment_slots_[id.index];
            if (slot.generation != id.generation || !slot.record.has_value()) {
                continue;
            }
            slot.record.reset();
            advance_generation(slot);
            try {
                free_fragment_slots_.push_back(id.index);
            } catch (...) {
            }
        }
    };
    release(record.before_children_fragments);
    release(record.after_children_fragments);
    record.before_children_fragments.clear();
    record.after_children_fragments.clear();
}

void ComponentHost::append_paint_subtree(ComponentId id) {
    const auto& record = require_record(id);
    for (const auto fragment : record.before_children_fragments) {
        if (contains(fragment)) {
            paint_traversal_.push_back({
                fragment,
                id,
                SceneFragmentPlacement::before_children,
            });
        }
    }
    for (const auto child : record.children) {
        if (contains(child)) {
            append_paint_subtree(child);
        }
    }
    for (const auto fragment : record.after_children_fragments) {
        if (contains(fragment)) {
            paint_traversal_.push_back({
                fragment,
                id,
                SceneFragmentPlacement::after_children,
            });
        }
    }
}

void ComponentHost::collect_subtree(
    ComponentId id,
    std::vector<ComponentId>& ids) const {
    const auto& record = require_record(id);
    ids.push_back(id);
    for (const auto child : record.children) {
        collect_subtree(child, ids);
    }
}

void ComponentHost::dispose_records(
    const std::vector<ComponentId>& ids,
    const std::vector<NodeId>& roots) noexcept {
    for (const auto id : ids) {
        if (auto* record = find_record(id)) {
            record->scope.dispose();
        }
    }

    for (auto iterator = ids.rbegin(); iterator != ids.rend(); ++iterator) {
        if (auto* record = find_record(*iterator)) {
            for (auto cleanup = record->resource_cleanups.rbegin();
                 cleanup != record->resource_cleanups.rend();
                 ++cleanup) {
                try {
                    (*cleanup)();
                } catch (...) {
                }
            }
            record->resource_cleanups.clear();
        }
    }

    for (auto iterator = ids.rbegin(); iterator != ids.rend(); ++iterator) {
        if (auto* record = find_record(*iterator)) {
            record->state.reset();
        }
    }

    for (auto iterator = roots.rbegin(); iterator != roots.rend(); ++iterator) {
        static_cast<void>(nodes_->destroy(*iterator));
    }

    for (auto iterator = ids.rbegin(); iterator != ids.rend(); ++iterator) {
        release_slot(*iterator);
    }
}

void ComponentHost::advance_generation(Slot& slot) noexcept {
    ++slot.generation;
    if (slot.generation == 0) {
        slot.generation = 1;
    }
}

void ComponentHost::advance_generation(FragmentSlot& slot) noexcept {
    ++slot.generation;
    if (slot.generation == 0) {
        slot.generation = 1;
    }
}

ComponentBuildContext::ComponentBuildContext(
    ComponentHost& host,
    std::optional<ComponentId> parent,
    std::optional<Prop<SemanticForeground>> semantic_foreground,
    std::shared_ptr<theme_runtime::ThemeScope> theme_scope) noexcept
    : host_(&host),
      parent_(parent),
      semantic_foreground_(std::move(semantic_foreground)),
      theme_scope_(std::move(theme_scope)) {}

void ComponentBuildContext::on_resource_cleanup(
    ComponentId id,
    std::function<void()> cleanup) {
    host_->add_resource_cleanup(id, std::move(cleanup));
}

SceneFragmentId ComponentBuildContext::register_scene_fragment(
    ComponentId id,
    SceneFragmentPlacement placement) {
    return host_->register_scene_fragment(id, placement);
}

Scope& ComponentBuildContext::scope(ComponentId id) {
    return host_->scope(id);
}

Scope& ComponentBuildContext::lifetime_scope() {
    return parent_.has_value() ? host_->scope(*parent_) : host_->host_scope_;
}

const std::shared_ptr<theme_runtime::ThemeScope>&
ComponentBuildContext::theme_scope() const noexcept {
    return theme_scope_;
}

NodeId ComponentBuildContext::root(ComponentId id) const {
    return host_->root(id);
}

const std::optional<Prop<SemanticForeground>>&
ComponentBuildContext::semantic_foreground() const noexcept {
    return semantic_foreground_;
}

ComponentBuildContext& require_component_build_context() {
    if (active_build_context == nullptr) {
        throw std::logic_error(
            "A component can only be declared inside active Host content");
    }
    return *active_build_context;
}

} // namespace ryn::runtime
