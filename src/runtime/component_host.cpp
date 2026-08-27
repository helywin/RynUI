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
    std::shared_ptr<void> state;
    std::type_index state_type{typeid(void)};
    std::vector<std::function<void()>> resource_cleanups;
    std::size_t declaration_order{0};

    Record(
        std::optional<ComponentId> component_parent,
        NodeId component_root,
        std::shared_ptr<void> component_state,
        std::type_index component_state_type,
        std::size_t order)
        : parent(component_parent),
          root(component_root),
          state(std::move(component_state)),
          state_type(component_state_type),
          declaration_order(order) {}
};

struct ComponentHost::Slot final {
    std::unique_ptr<Record> record;
    std::uint32_t generation{1};
};

ComponentHost::ComponentHost(NodeStore& nodes) noexcept
    : nodes_(&nodes), owner_thread_(std::this_thread::get_id()) {}

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
    ComponentBuildContext context(*this, std::nullopt);
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
    }
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
        return;
    }
    dispose_records(ids, roots);
}

bool ComponentHost::active() const noexcept {
    return active_;
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

std::size_t ComponentHost::declaration_order(ComponentId id) const {
    return require_record(id).declaration_order;
}

Scope& ComponentHost::scope(ComponentId id) {
    return require_record(id).scope;
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
            next_declaration_order_++);
        const ComponentId id{slot_index, slots_[slot_index].generation};
        slots_[slot_index].record = std::move(record);
        if (parent_record != nullptr) {
            parent_record->children.push_back(id);
        }
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

void ComponentHost::add_resource_cleanup(
    ComponentId id,
    std::function<void()> cleanup) {
    ensure_owner_thread();
    require_record(id).resource_cleanups.push_back(std::move(cleanup));
}

void ComponentHost::mount_slot(
    ComponentId parent,
    const std::function<void()>& content) {
    ensure_owner_thread();
    static_cast<void>(require_record(parent));
    if (!mounting_ || active_build_context == nullptr) {
        throw std::logic_error("Typed slots can only run during Host mount");
    }

    ComponentBuildContext context(*this, parent);
    ActiveBuildContextGuard guard(context);
    content();
}

void ComponentHost::ensure_owner_thread() const {
    if (std::this_thread::get_id() != owner_thread_) {
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

void ComponentHost::release_slot(ComponentId id) noexcept {
    if (id.index >= slots_.size()) {
        return;
    }
    auto& slot = slots_[id.index];
    if (slot.generation != id.generation || slot.record == nullptr) {
        return;
    }
    slot.record.reset();
    advance_generation(slot);
    try {
        free_slots_.push_back(id.index);
    } catch (...) {
    }
    --live_components_;
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

ComponentBuildContext::ComponentBuildContext(
    ComponentHost& host,
    std::optional<ComponentId> parent) noexcept
    : host_(&host), parent_(parent) {}

void ComponentBuildContext::on_resource_cleanup(
    ComponentId id,
    std::function<void()> cleanup) {
    host_->add_resource_cleanup(id, std::move(cleanup));
}

Scope& ComponentBuildContext::scope(ComponentId id) {
    return host_->scope(id);
}

NodeId ComponentBuildContext::root(ComponentId id) const {
    return host_->root(id);
}

ComponentBuildContext& require_component_build_context() {
    if (active_build_context == nullptr) {
        throw std::logic_error(
            "A component can only be declared inside active Host content");
    }
    return *active_build_context;
}

} // namespace ryn::runtime
