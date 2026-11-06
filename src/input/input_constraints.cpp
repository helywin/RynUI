#include "input/input_constraints.hpp"
#include "input/interaction_registry.hpp"
#include <algorithm>
#include <limits>
#include <stdexcept>
#include <thread>
#include <utility>

namespace ryn::input {
namespace {
constexpr std::size_t maximum_leases = 4096;
constexpr std::size_t maximum_roots = 256;

struct ConstraintSlot final {
    std::uint64_t generation{1};
    runtime::ComponentId owner;
    InputConstraintKind kind{};
    InputConstraintSpec spec;
    bool live{};
    bool active{};
};

struct CompanionSlot final {
    std::uint64_t generation{1};
    runtime::ComponentId owner;
    std::uint32_t authority{};
    std::uint64_t authority_generation{};
    std::vector<InputRootRef> roots;
    bool live{};
    bool active{};
};

bool same_window(const std::weak_ptr<InputConstraintState>& a, const std::weak_ptr<InputConstraintState>& b) noexcept {
    return !a.owner_before(b) && !b.owner_before(a);
}
} // namespace

struct InputConstraintState final {
    InteractionRegistry* registry{};
    runtime::ComponentHost* components{};
    std::uint64_t* revision{};
    std::thread::id thread{std::this_thread::get_id()};
    std::vector<ConstraintSlot> constraints;
    std::vector<CompanionSlot> companions;
    std::uint64_t mutation{};
    std::uint64_t publications{};
    std::size_t active_constraints{};

    void check() const {
        if (thread != std::this_thread::get_id() || !registry) {
            throw std::logic_error("Input constraint requires its live window owner thread");
        }
    }

    bool live(runtime::ComponentId id) const {
        for (auto current = std::optional{id}; current; current = components->parent(*current)) {
            if (!components->contains(*current) || !components->scope(*current).active()) {
                return false;
            }
        }
        return true;
    }

    bool descendant(runtime::ComponentId child, runtime::ComponentId root) const {
        for (auto current = std::optional{child}; current && components->contains(*current);
             current = components->parent(*current)) {
            if (*current == root) {
                return true;
            }
        }
        return false;
    }

    bool constraint_live(const ConstraintSlot& slot) const {
        return slot.live && live(slot.owner);
    }

    bool companion_live(const CompanionSlot& slot) const {
        return slot.live && live(slot.owner) && slot.authority < constraints.size() &&
               constraints[slot.authority].generation == slot.authority_generation &&
               constraint_live(constraints[slot.authority]);
    }

    void release_owner(runtime::ComponentId owner) noexcept {
        if (thread != std::this_thread::get_id() || !registry) {
            return;
        }
        bool changed = false;
        bool retired = false;
        for (std::size_t index = 0; index < constraints.size(); ++index) {
            auto& slot = constraints[index];
            if (!slot.live || slot.owner != owner) {
                continue;
            }
            retired = true;
            changed |= slot.active;
            slot.live = false;
            slot.active = false;
            slot.spec.roots.clear();
            for (auto& grant : companions) {
                if (grant.live && grant.authority == index && grant.authority_generation == slot.generation) {
                    changed |= grant.active;
                    grant.live = false;
                    grant.active = false;
                    grant.roots.clear();
                }
            }
        }
        for (auto& grant : companions) {
            if (grant.live && grant.owner == owner) {
                retired = true;
                changed |= grant.active;
                grant.live = false;
                grant.active = false;
                grant.roots.clear();
            }
        }
        if (retired) {
            ++mutation;
            recount();
        }
        if (changed) {
            publish();
        }
    }

    void publish() noexcept {
        ++*revision;
        ++publications;
    }

    void recount() noexcept {
        active_constraints = static_cast<std::size_t>(
            std::count_if(constraints.begin(), constraints.end(), [](const auto& s) { return s.live && s.active; }));
    }

    bool release_constraint(std::uint32_t index, std::uint64_t generation) noexcept {
        if (thread != std::this_thread::get_id() || !registry || index >= constraints.size()) {
            return false;
        }
        auto& slot = constraints[index];
        if (!slot.live || slot.generation != generation) {
            return false;
        }
        bool changed = slot.active;
        slot.live = false;
        slot.active = false;
        slot.spec.roots.clear();
        for (auto& grant : companions) {
            if (grant.live && grant.authority == index && grant.authority_generation == generation) {
                changed |= grant.active;
                grant.live = false;
                grant.active = false;
                grant.roots.clear();
            }
        }
        ++mutation;
        recount();
        if (changed) {
            publish();
        }
        return true;
    }

    bool release_companion(std::uint32_t index, std::uint64_t generation) noexcept {
        if (thread != std::this_thread::get_id() || !registry || index >= companions.size()) {
            return false;
        }
        auto& slot = companions[index];
        if (!slot.live || slot.generation != generation) {
            return false;
        }
        const bool changed = slot.active;
        slot.live = false;
        slot.active = false;
        slot.roots.clear();
        ++mutation;
        if (changed) {
            publish();
        }
        return true;
    }
};

bool operator==(const InputRootRef& a, const InputRootRef& b) noexcept {
    return a.component_ == b.component_ && same_window(a.window_, b.window_);
}

bool InputRootRef::valid() const {
    const auto state = window_.lock();
    if (!state) {
        return false;
    }
    state->check();
    return state->live(component_);
}

bool operator==(const InputConstraintLease& a, const InputConstraintLease& b) noexcept {
    return a.index_ == b.index_ && a.generation_ == b.generation_ && same_window(a.window_, b.window_);
}

bool InputConstraintLease::valid() const {
    const auto state = window_.lock();
    if (!state) {
        return false;
    }
    state->check();
    return index_ < state->constraints.size() && state->constraints[index_].generation == generation_ &&
           state->constraint_live(state->constraints[index_]);
}

bool InputConstraintLease::release() noexcept {
    const auto state = window_.lock();
    if (state && state->thread != std::this_thread::get_id()) {
        return false;
    }
    const auto generation = std::exchange(generation_, 0);
    return state && state->release_constraint(index_, generation);
}

bool InputCompanionLease::valid() const {
    const auto state = window_.lock();
    if (!state) {
        return false;
    }
    state->check();
    return index_ < state->companions.size() && state->companions[index_].generation == generation_ &&
           state->companion_live(state->companions[index_]);
}

bool InputCompanionLease::release() noexcept {
    const auto state = window_.lock();
    if (state && state->thread != std::this_thread::get_id()) {
        return false;
    }
    const auto generation = std::exchange(generation_, 0);
    return state && state->release_companion(index_, generation);
}

bool InputConstraintAuthority::valid() const {
    if (!lease_.valid()) {
        return false;
    }
    const auto state = lease_.window_.lock();
    return state->constraints[lease_.index_].kind == InputConstraintKind::RestrictToSubtrees;
}

bool operator==(const InputConstraintAuthority& a, const InputConstraintAuthority& b) noexcept {
    return a.lease_ == b.lease_;
}

struct InputConstraintBatch::Impl final {
    std::weak_ptr<InputConstraintState> window;
    std::uint64_t mutation{};
    std::vector<ConstraintSlot> constraints;
    std::vector<CompanionSlot> companions;
    bool committed{};
    std::optional<InputConstraintAuthority> boundary_view;
    bool edit_boundary_view{};
    std::vector<bool> constraint_edits;
    std::vector<bool> companion_edits;

    std::shared_ptr<InputConstraintState> state() const {
        const auto s = window.lock();
        if (!s) {
            throw std::logic_error("Input constraint window retired");
        }
        s->check();
        if (committed || s->mutation != mutation) {
            throw std::logic_error("Input constraint transaction is stale");
        }
        return s;
    }
};

InputConstraintBatch::InputConstraintBatch(std::unique_ptr<Impl> impl) : impl_(std::move(impl)) {}

InputConstraintBatch::InputConstraintBatch(InputConstraintBatch&&) noexcept = default;
InputConstraintBatch& InputConstraintBatch::operator=(InputConstraintBatch&&) noexcept = default;
InputConstraintBatch::~InputConstraintBatch() = default;

void InputConstraintBatch::replace(const InputConstraintLease& lease, InputConstraintSpec spec) {
    const auto state = impl_->state();
    if (!same_window(lease.window_, impl_->window) || lease.index_ >= impl_->constraints.size()) {
        throw std::invalid_argument("Foreign input constraint lease");
    }
    auto& slot = impl_->constraints[lease.index_];
    if (!slot.live || slot.generation != lease.generation_ || !state->live(slot.owner)) {
        throw std::invalid_argument("Retired input constraint owner");
    }
    if (spec.roots.size() > maximum_roots) {
        throw std::length_error("Input constraint root limit exceeded");
    }
    for (const auto& root : spec.roots) {
        if (!same_window(root.window_, impl_->window) || !state->live(root.component_)) {
            throw std::invalid_argument("Input constraint requires live roots from its window");
        }
    }
    std::ranges::sort(spec.roots, [](const auto& a, const auto& b) {
        return a.component_.index < b.component_.index ||
               (a.component_.index == b.component_.index && a.component_.generation < b.component_.generation);
    });
    spec.roots.erase(std::unique(spec.roots.begin(), spec.roots.end()), spec.roots.end());
    slot.spec = std::move(spec);
    slot.active = true;
    impl_->constraint_edits[lease.index_] = true;
}

void InputConstraintBatch::deactivate(const InputConstraintLease& lease) {
    static_cast<void>(impl_->state());
    if (!same_window(lease.window_, impl_->window) || lease.index_ >= impl_->constraints.size()) {
        throw std::invalid_argument("Foreign input constraint lease");
    }
    auto& slot = impl_->constraints[lease.index_];
    if (!slot.live || slot.generation != lease.generation_) {
        throw std::invalid_argument("Retired input constraint lease");
    }
    slot.active = false;
    impl_->constraint_edits[lease.index_] = true;
}

void InputConstraintBatch::enable(const InputCompanionLease& lease, bool active) {
    const auto state = impl_->state();
    if (!same_window(lease.window_, impl_->window) || lease.index_ >= impl_->companions.size()) {
        throw std::invalid_argument("Foreign input companion lease");
    }
    auto& slot = impl_->companions[lease.index_];
    if (!slot.live || slot.generation != lease.generation_ || !state->live(slot.owner) ||
        slot.authority >= impl_->constraints.size()) {
        throw std::invalid_argument("Retired input companion");
    }
    const auto& authority = impl_->constraints[slot.authority];
    if (!authority.live || authority.generation != slot.authority_generation || !state->live(authority.owner)) {
        throw std::invalid_argument("Retired input authority");
    }
    slot.active = active;
    impl_->companion_edits[lease.index_] = true;
}

bool InputConstraintBatch::commit() {
    const auto state = impl_->state();
    bool changed = false;
    for (std::size_t i = 0; i < impl_->constraints.size(); ++i) {
        const auto& next = impl_->constraints[i];
        const auto& old = state->constraints[i];
        if (impl_->constraint_edits[i] && next.active &&
            (!state->live(next.owner) ||
             std::ranges::any_of(next.spec.roots, [&](const auto& r) { return !r.valid(); }))) {
            throw std::invalid_argument("Input constraint root retired before publication");
        }
        changed |= next.active != old.active || (next.active && next.spec != old.spec);
    }
    for (std::size_t i = 0; i < impl_->companions.size(); ++i) {
        const auto& next = impl_->companions[i];
        const auto& old = state->companions[i];
        if (impl_->companion_edits[i] && next.active &&
            (!state->companion_live(next) ||
             std::ranges::any_of(next.roots, [&](const auto& r) { return !r.valid(); }))) {
            throw std::invalid_argument("Input companion retired before publication");
        }
        changed |= next.active != old.active;
    }
    changed |= impl_->edit_boundary_view &&
               state->registry->boundary_authority_ != impl_->boundary_view.value_or(InputConstraintAuthority{});
    impl_->committed = true;
    if (!changed) {
        return false;
    }
    state->constraints.swap(impl_->constraints);
    state->companions.swap(impl_->companions);
    if (impl_->edit_boundary_view) {
        state->registry->boundary_authority_ = impl_->boundary_view.value_or(InputConstraintAuthority{});
    }
    ++state->mutation;
    state->recount();
    state->publish();
    return true;
}

InputRootRef InteractionRegistry::input_root(runtime::ComponentId component) {
    ensure_owner_thread();
    if (!components_->contains(component) || !components_->scope(component).active()) {
        throw std::invalid_argument("Input root requires a live component scope");
    }
    if (!constraints_) {
        constraints_ = std::make_shared<InputConstraintState>();
        constraints_->registry = this;
        constraints_->components = components_;
        constraints_->revision = &input_boundary_revision_;
    }
    InputRootRef root;
    root.window_ = constraints_;
    root.component_ = component;
    return root;
}

InputConstraintLease InteractionRegistry::create_input_constraint(const InputRootRef& owner, InputConstraintKind kind) {
    ensure_owner_thread();
    if (!constraints_ || owner.window_.lock() != constraints_ || !owner.valid()) {
        throw std::invalid_argument("Input owner requires the exact source window");
    }
    if (kind != InputConstraintKind::BlockSubtrees && kind != InputConstraintKind::RestrictToSubtrees) {
        throw std::invalid_argument("Invalid input constraint kind");
    }
    auto& slots = constraints_->constraints;
    std::size_t index{};
    while (index < slots.size() &&
           (slots[index].live || slots[index].generation == std::numeric_limits<std::uint64_t>::max())) {
        ++index;
    }
    if (index == slots.size()) {
        if (index >= maximum_leases) {
            throw std::length_error("Input constraint lease limit exceeded");
        }
        slots.emplace_back();
    } else {
        ++slots[index].generation;
    }
    auto& slot = slots[index];
    slot.live = true;
    slot.active = false;
    slot.owner = owner.component_;
    slot.kind = kind;
    slot.spec = {};
    ++constraints_->mutation;
    InputConstraintLease result;
    result.window_ = constraints_;
    result.index_ = static_cast<std::uint32_t>(index);
    result.generation_ = slot.generation;
    return result;
}

InputConstraintAuthority InteractionRegistry::input_authority(const InputConstraintLease& lease) const {
    ensure_owner_thread();
    if (!constraints_ || lease.window_.lock() != constraints_ || !lease.valid() ||
        constraints_->constraints[lease.index_].kind != InputConstraintKind::RestrictToSubtrees) {
        throw std::invalid_argument("Input authority requires its owned restriction");
    }
    InputConstraintAuthority result;
    result.lease_ = lease;
    return result;
}

InputCompanionLease InteractionRegistry::create_input_companion(const InputConstraintAuthority& authority,
                                                                const InputRootRef& owner,
                                                                std::span<const InputRootRef> controls) {
    ensure_owner_thread();
    if (!constraints_ || authority.lease_.window_.lock() != constraints_ || !authority.valid() ||
        owner.window_.lock() != constraints_ || !owner.valid()) {
        throw std::invalid_argument("Input companion requires its exact authority/window/owner");
    }
    if (controls.empty() || controls.size() > maximum_roots) {
        throw std::invalid_argument("Input companion requires bounded owned controls");
    }
    std::vector<InputRootRef> roots(controls.begin(), controls.end());
    for (const auto& root : roots) {
        if (root.window_.lock() != constraints_ || !root.valid() ||
            !constraints_->descendant(root.component_, owner.component_)) {
            throw std::invalid_argument("Input companion controls must belong to their grantee");
        }
    }
    auto& slots = constraints_->companions;
    std::size_t index{};
    while (index < slots.size() &&
           (slots[index].live || slots[index].generation == std::numeric_limits<std::uint64_t>::max())) {
        ++index;
    }
    if (index == slots.size()) {
        if (index >= maximum_leases) {
            throw std::length_error("Input companion lease limit exceeded");
        }
        slots.emplace_back();
    } else {
        ++slots[index].generation;
    }
    auto& slot = slots[index];
    slot.owner = owner.component_;
    slot.authority = authority.lease_.index_;
    slot.authority_generation = authority.lease_.generation_;
    slot.roots = std::move(roots);
    slot.active = false;
    slot.live = true;
    ++constraints_->mutation;
    InputCompanionLease result;
    result.window_ = constraints_;
    result.index_ = static_cast<std::uint32_t>(index);
    result.generation_ = slot.generation;
    return result;
}

InputConstraintBatch InteractionRegistry::begin_input_constraint_update() {
    ensure_owner_thread();
    if (!constraints_) {
        throw std::logic_error("Create an inactive owned constraint before editing it");
    }
    auto impl = std::make_unique<InputConstraintBatch::Impl>();
    impl->window = constraints_;
    impl->mutation = constraints_->mutation;
    impl->constraints = constraints_->constraints;
    impl->companions = constraints_->companions;
    impl->constraint_edits.resize(impl->constraints.size());
    impl->companion_edits.resize(impl->companions.size());
    return InputConstraintBatch{std::move(impl)};
}

bool InteractionRegistry::owned_input_active(runtime::ComponentId component,
                                             const InputConstraintLease* excluding) const {
    if (!constraints_ || constraints_->active_constraints == 0) {
        return true;
    }
    const auto contains = [&](const auto& roots) {
        return std::ranges::any_of(roots, [&](const auto& root) {
            return constraints_->live(root.component_) && constraints_->descendant(component, root.component_);
        });
    };
    for (std::size_t i = 0; i < constraints_->constraints.size(); ++i) {
        const auto& slot = constraints_->constraints[i];
        if (!slot.active || !constraints_->constraint_live(slot) ||
            (excluding && excluding->index_ == i && excluding->generation_ == slot.generation)) {
            continue;
        }
        const bool own = contains(slot.spec.roots);
        if (slot.kind == InputConstraintKind::BlockSubtrees) {
            if (own) {
                return false;
            }
            continue;
        }
        if (own) {
            continue;
        }
        bool companion = false;
        for (const auto& grant : constraints_->companions) {
            if (grant.active && grant.authority == i && grant.authority_generation == slot.generation &&
                constraints_->companion_live(grant) && contains(grant.roots)) {
                companion = true;
                break;
            }
        }
        if (!companion) {
            return false;
        }
    }
    return true;
}

bool InteractionRegistry::has_input_confinement() const {
    ensure_owner_thread();
    if (input_boundary_) {
        return true;
    }
    if (!constraints_ || constraints_->active_constraints == 0) {
        return false;
    }
    return std::ranges::any_of(constraints_->constraints, [&](const auto& s) {
        return s.active && s.kind == InputConstraintKind::RestrictToSubtrees && constraints_->constraint_live(s);
    });
}

bool InteractionRegistry::consumes_unhandled_scroll() const {
    ensure_owner_thread();
    if (input_boundary_) {
        return true;
    }
    if (!constraints_ || constraints_->active_constraints == 0) {
        return false;
    }
    return std::ranges::any_of(constraints_->constraints, [&](const auto& s) {
        return s.active && s.spec.consume_unhandled_scroll && constraints_->constraint_live(s);
    });
}

bool InteractionRegistry::subtree_intersects_input_domain(runtime::ComponentId root,
                                                          const InputConstraintLease* excluding) const {
    ensure_owner_thread();
    if (excluding && (!constraints_ || excluding->window_.lock() != constraints_ || !excluding->valid())) {
        throw std::invalid_argument("Geometry exclusion requires its exact live owned lease");
    }
    if (!components_->contains(root) || !components_->scope(root).active()) {
        return false;
    }
    const auto candidate = [&](runtime::ComponentId c) {
        return component_is_same_or_ancestor(root, c) && component_active_impl(c, excluding);
    };
    if (candidate(root)) {
        return true;
    }
    if (input_boundary_ && candidate(*input_boundary_)) {
        return true;
    }
    if (constraints_ && constraints_->active_constraints) {
        for (const auto& slot : constraints_->constraints) {
            if (slot.active && slot.kind == InputConstraintKind::RestrictToSubtrees &&
                constraints_->constraint_live(slot)) {
                for (const auto& ref : slot.spec.roots) {
                    if (candidate(ref.component_)) {
                        return true;
                    }
                }
            }
        }
        for (const auto& grant : constraints_->companions) {
            if (grant.active && constraints_->companion_live(grant) &&
                constraints_->constraints[grant.authority].active) {
                for (const auto& ref : grant.roots) {
                    if (candidate(ref.component_)) {
                        return true;
                    }
                }
            }
        }
    }
    return false;
}

InputConstraintDiagnostics InteractionRegistry::input_constraint_diagnostics() const {
    ensure_owner_thread();
    InputConstraintDiagnostics result;
    if (!constraints_) {
        return result;
    }
    result.publications = constraints_->publications;
    for (const auto& s : constraints_->constraints) {
        result.leases += s.live;
        result.active_leases += s.live && s.active;
    }
    for (const auto& s : constraints_->companions) {
        result.companions += s.live;
        result.active_companions += s.live && s.active;
    }
    return result;
}
} // namespace ryn::input

namespace ryn::input {
bool InteractionRegistry::publish_input_constraints(InputConstraintBatch& batch) {
    ensure_owner_thread();
    if (!batch.impl_ || batch.impl_->window.lock() != constraints_) {
        throw std::invalid_argument("Foreign input constraint transaction");
    }
    return batch.commit();
}

bool InteractionRegistry::owns_input_root(const InputRootRef& root) const {
    ensure_owner_thread();
    return constraints_ && root.window_.lock() == constraints_ && root.valid();
}

bool InteractionRegistry::release_input_constraint(InputConstraintLease& lease) noexcept {
    return is_owner_thread() && constraints_ && lease.window_.lock() == constraints_ && lease.release();
}

bool InteractionRegistry::release_input_companion(InputCompanionLease& lease) noexcept {
    return is_owner_thread() && constraints_ && lease.window_.lock() == constraints_ && lease.release();
}
} // namespace ryn::input

namespace ryn::input {
void InputConstraintBatch::boundary_view(std::optional<InputConstraintAuthority> authority) {
    const auto state = impl_->state();
    if (authority && (!same_window(authority->lease_.window_, impl_->window) || !authority->valid())) {
        throw std::invalid_argument("Foreign or retired input authority view");
    }
    impl_->boundary_view = std::move(authority);
    impl_->edit_boundary_view = true;
}

std::optional<runtime::ComponentId> InteractionRegistry::input_boundary() const {
    ensure_owner_thread();
    if (input_boundary_) {
        return input_boundary_;
    }
    if (constraints_ && boundary_authority_.valid()) {
        const auto& slot = constraints_->constraints[boundary_authority_.lease_.index_];
        if (slot.active && slot.spec.roots.size() == 1 && slot.spec.roots.front().valid()) {
            return slot.spec.roots.front().component();
        }
    }
    return {};
}

bool InteractionRegistry::is_boundary_authority(const InputConstraintAuthority& authority) const {
    ensure_owner_thread();
    return authority == boundary_authority_ && authority.valid() &&
           constraints_->constraints[authority.lease_.index_].active;
}

bool InteractionRegistry::component_lifetime_active(runtime::ComponentId component) const {
    ensure_owner_thread();
    return component_scope_active(component);
}
} // namespace ryn::input

namespace ryn::input {
std::function<void()> InteractionRegistry::input_owner_cleanup(const InputRootRef& owner) const {
    ensure_owner_thread();
    if (!owns_input_root(owner)) {
        throw std::invalid_argument("Input cleanup requires its exact live window owner");
    }
    return [window = owner.window_, component = owner.component_]() noexcept {
        if (const auto state = window.lock()) {
            state->release_owner(component);
        }
    };
}
} // namespace ryn::input

namespace ryn::input {
InputRootRef InputConstraintLease::owner() const {
    if (!valid()) {
        throw std::invalid_argument("Input owner query requires a live lease");
    }
    const auto state = window_.lock();
    InputRootRef result;
    result.window_ = window_;
    result.component_ = state->constraints[index_].owner;
    return result;
}
} // namespace ryn::input
