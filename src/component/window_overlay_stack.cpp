#include "component/window_overlay_stack.hpp"
#include "component/window_component_services.hpp"
#include <algorithm>
#include <limits>
#include <thread>
#include <utility>

namespace ryn::detail {
struct WindowOverlayLifetime final {
    WindowOverlayStack* stack{};
    std::thread::id owner{std::this_thread::get_id()};
};

struct WindowOverlayCompanionState final {
    std::weak_ptr<WindowOverlayLifetime> lifetime;
    input::InputRootRef owner;
    std::vector<input::InputRootRef> controls;
    input::InputCompanionLease lease;
    input::InputConstraintAuthority authority;
    bool registered{true};
    bool enabled{};
};

namespace {
bool descendant(WindowComponentServices& services, runtime::ComponentId child, runtime::ComponentId parent) {
    for (auto current = std::optional{child}; current && services.components().contains(*current);
         current = services.components().parent(*current)) {
        if (*current == parent) {
            return true;
        }
    }
    return false;
}

bool can_focus(WindowComponentServices& services, input::InteractionId id) {
    const auto* record = services.interactions().find(id);
    return record && record->eligible && record->focusable && services.interactions().branch_active(id);
}
} // namespace

bool operator==(const WindowOverlayTicket& left, const WindowOverlayTicket& right) noexcept {
    return left.id_ == right.id_ && left.owner_ == right.owner_ && !left.lifetime_.owner_before(right.lifetime_) &&
           !right.lifetime_.owner_before(left.lifetime_);
}

bool WindowOverlayCompanion::bound() const {
    if (!state_) {
        return false;
    }
    const auto life = state_->lifetime.lock();
    if (!life) {
        return false;
    }
    if (life->owner != std::this_thread::get_id()) {
        throw std::logic_error("Overlay companion requires its owner thread");
    }
    return life->stack && state_->registered && state_->owner.valid();
}

std::optional<std::uint64_t> WindowOverlayCompanion::stack_order() const {
    return bound() && state_->enabled ? state_->lifetime.lock()->stack->companion_order() : std::nullopt;
}

bool WindowOverlayCompanion::set_active(bool enabled) {
    return update(enabled, {});
}

bool WindowOverlayCompanion::update(bool enabled, std::span<const input::InputConstraintUpdate> changes) {
    return bound() && state_->lifetime.lock()->stack->update_companion(*state_, enabled, changes);
}

bool WindowOverlayCompanion::release() noexcept {
    if (!state_) {
        return false;
    }
    const auto life = state_->lifetime.lock();
    if (life && life->owner != std::this_thread::get_id()) {
        return false;
    }
    if (!std::exchange(state_->registered, false)) {
        return false;
    }
    state_->enabled = false;
    static_cast<void>(state_->lease.release());
    if (life && life->stack) {
        life->stack->retire_companion(state_.get());
    }
    return true;
}

WindowOverlayStack::WindowOverlayStack(WindowComponentServices& services)
    : services_(&services), lifetime_(std::make_shared<WindowOverlayLifetime>()) {
    lifetime_->stack = this;
}

WindowOverlayStack::~WindowOverlayStack() {
    dispose();
}

WindowOverlayStack::Entry* WindowOverlayStack::find(const WindowOverlayTicket& ticket) noexcept {
    const auto found = std::ranges::find_if(entries_, [&](const auto& entry) { return entry.ticket == ticket; });
    return found == entries_.end() ? nullptr : &*found;
}

WindowOverlayTicket WindowOverlayStack::add(input::InputRootRef owner, input::InputRootRef root,
                                            WindowOverlayKind kind) {
    if (lifetime_->stack != this) {
        throw std::logic_error("Window overlay stack is disposed");
    }
    if (!services_->interactions().owns_input_root(owner) || !services_->interactions().owns_input_root(root) ||
        !descendant(*services_, root.component(), owner.component())) {
        throw std::invalid_argument("Overlay requires its exact live window owner and descendant root");
    }
    if (kind != WindowOverlayKind::Modal && kind != WindowOverlayKind::Drawer &&
        kind != WindowOverlayKind::FullscreenSpin) {
        throw std::invalid_argument("Invalid window overlay kind");
    }
    if (std::ranges::any_of(entries_, [&](const auto& entry) { return entry.root == root; })) {
        throw std::invalid_argument("Overlay root is already registered");
    }
    if (next_id_ == std::numeric_limits<std::uint64_t>::max()) {
        throw std::length_error("Window overlay identity exhausted");
    }
    WindowOverlayTicket ticket;
    ticket.owner_ = owner;
    ticket.lifetime_ = lifetime_;
    ticket.id_ = next_id_ + 1;
    entries_.push_back({ticket, root, kind});
    ++next_id_;
    dirty_ = true;
    return ticket;
}

void WindowOverlayStack::update(const WindowOverlayTicket& ticket, const WindowOverlayState& next) {
    auto* entry = find(ticket);
    if (!entry || !services_->interactions().owns_input_root(ticket.owner_) || !entry->root.valid()) {
        throw std::invalid_argument("Overlay ticket is foreign or retired");
    }
    if (next.input_visible && (!next.paint_visible || !next.layer_priority)) {
        throw std::invalid_argument("An input overlay must have a visible window layer");
    }
    if (next == entry->desired) {
        return;
    }
    for (auto parent = next.parent; parent;) {
        const auto* ancestor = find(*parent);
        if (!ancestor || !ancestor->ticket.owner_.valid() || ancestor->ticket == ticket) {
            throw std::invalid_argument("Overlay parent is foreign, retired or cyclic");
        }
        parent = ancestor->desired.parent;
    }
    if (!entry->desired.input_visible && next.input_visible) {
        const bool resume_drawer = entry->kind == WindowOverlayKind::Drawer && entry->desired.paint_visible &&
                                   next.paint_visible && entry->order != 0;
        if (!resume_drawer && next_order_ == std::numeric_limits<std::uint64_t>::max()) {
            throw std::length_error("Window overlay opening order exhausted");
        }
        entry->trigger = services_->focus().state().focused;
        const Entry* parent_entry = next.parent ? find(*next.parent) : nullptr;
        if (!parent_entry) {
            for (auto owner = services_->components().parent(ticket.owner_.component()); owner;
                 owner = services_->components().parent(*owner)) {
                const auto parent = std::ranges::find_if(entries_, [&](const auto& e) {
                    return e.root.component() == *owner && e.root.valid() && e.desired.layer_priority;
                });
                if (parent != entries_.end()) {
                    parent_entry = &*parent;
                    break;
                }
            }
        }
        if (parent_entry && entry->trigger) {
            const auto* focused = services_->interactions().find(*entry->trigger);
            if (!focused || !descendant(*services_, focused->component, parent_entry->root.component())) {
                entry->trigger = parent_entry->last_focus;
            }
        }
        if (!resume_drawer) {
            entry->order = ++next_order_;
        }
    }
    entry->desired = next;
    dirty_ = true;
}

bool WindowOverlayStack::logical_branch_active(runtime::ComponentId id) const {
    for (auto current = std::optional{id}; current && services_->components().contains(*current);
         current = services_->components().parent(*current)) {
        const auto overlay = std::ranges::find_if(
            entries_, [&](const auto& entry) { return entry.root.component() == *current && entry.root.valid(); });
        if (overlay != entries_.end()) {
            if (!overlay->desired.paint_visible) {
                return false;
            }
        } else if (!services_->components().local_branch_active(*current)) {
            return false;
        }
    }
    return services_->components().contains(id);
}

bool WindowOverlayStack::candidate(const Entry& entry) const {
    if (!entry.ticket.owner_.valid() || !entry.root.valid() || !entry.desired.input_visible ||
        !entry.desired.paint_visible || !entry.desired.layer_priority ||
        !logical_branch_active(entry.ticket.owner_.component()) ||
        services_->nodes().content_opacity(services_->components().root(entry.ticket.owner_.component())) <= 0) {
        return false;
    }
    // Paint-only exiting parents must not admit input through an otherwise live child.
    for (auto current = services_->components().parent(entry.ticket.owner_.component()); current;
         current = services_->components().parent(*current)) {
        const auto ancestor = std::ranges::find_if(
            entries_, [&](const auto& e) { return e.root.component() == *current && e.root.valid(); });
        if (ancestor != entries_.end() && ancestor->desired.layer_priority && !ancestor->desired.input_visible) {
            return false;
        }
    }
    for (auto parent = entry.desired.parent; parent;) {
        const auto ancestor = std::ranges::find_if(entries_, [&](const auto& e) { return e.ticket == *parent; });
        if (ancestor == entries_.end() || !ancestor->ticket.owner_.valid() || !ancestor->desired.input_visible ||
            !logical_branch_active(ancestor->ticket.owner_.component())) {
            return false;
        }
        parent = ancestor->desired.parent;
    }
    return true;
}

bool WindowOverlayStack::companion_permitted(const WindowOverlayCompanionState& entry,
                                             std::optional<runtime::ComponentId> domain, bool modal_winner) const {
    bool permitted = modal_winner && domain && entry.registered && entry.enabled && entry.owner.valid();
    for (const auto& root : entry.controls) {
        permitted = permitted && root.valid() && (!domain || !descendant(*services_, *domain, root.component()));
    }
    return permitted;
}

bool WindowOverlayStack::reconcile(bool quiet) {
    if (!services_->components().active()) {
        return false;
    }
    const auto paint_candidate = [&](const Entry& entry) {
        return entry.ticket.owner_.valid() && entry.root.valid() && entry.desired.paint_visible &&
               entry.desired.layer_priority && logical_branch_active(entry.ticket.owner_.component());
    };
    if (!dirty_ && std::ranges::none_of(entries_, [&](const auto& entry) {
            const bool painted =
                std::ranges::any_of(published_, [&](const auto& view) { return view.root == entry.root.component(); });
            return !entry.ticket.owner_.valid() || !entry.root.valid() || entry.published != candidate(entry) ||
                   painted != paint_candidate(entry);
        })) {
        return false;
    }

    // Every allocation and root/parent check finishes before the registry or painter is changed.
    std::vector<std::size_t> candidates(std::size_t{0});
    std::vector<std::optional<std::size_t>> parents(entries_.size());
    std::vector<WindowOverlayView> next_views(std::size_t{0});
    candidates.reserve(entries_.size());
    next_views.reserve(entries_.size());
    for (std::size_t i = 0; i < entries_.size(); ++i) {
        if (paint_candidate(entries_[i])) {
            candidates.push_back(i);
        }
    }
    for (const auto i : candidates) {
        const auto& entry = entries_[i];
        if (entry.desired.parent) {
            const auto parent =
                std::ranges::find_if(candidates, [&](auto j) { return entries_[j].ticket == *entry.desired.parent; });
            if (parent != candidates.end()) {
                parents[i] = *parent;
            }
        } else {
            for (auto owner = services_->components().parent(entry.ticket.owner_.component()); owner;
                 owner = services_->components().parent(*owner)) {
                const auto parent =
                    std::ranges::find_if(candidates, [&](auto j) { return entries_[j].root.component() == *owner; });
                if (parent != candidates.end()) {
                    parents[i] = *parent;
                    break;
                }
            }
        }
    }
    std::ranges::sort(candidates, [&](auto a, auto b) { return entries_[a].order < entries_[b].order; });
    const auto append_family = [&](const auto& self, std::size_t index, std::size_t depth) -> void {
        if (depth > entries_.size()) {
            throw std::invalid_argument("Cyclic window overlay family");
        }
        const auto& entry = entries_[index];
        next_views.push_back(
            {entry.ticket.owner_.component(), entry.root.component(), entry.kind,
             parents[index] ? std::optional{entries_[*parents[index]].ticket.owner_.component()} : std::nullopt,
             entry.order, next_views.size() + 1});
        for (auto child : candidates) {
            if (parents[child] == index) {
                self(self, child, depth + 1);
            }
        }
    };
    for (auto index : candidates) {
        if (!parents[index]) {
            append_family(append_family, index, 0);
        }
    }
    if (next_views.size() != candidates.size()) {
        throw std::invalid_argument("Window overlay forest has no live root");
    }
    const auto entry_for = [&](runtime::ComponentId owner) -> const Entry& {
        return *std::ranges::find_if(entries_,
                                     [&](const auto& entry) { return entry.ticket.owner_.component() == owner; });
    };
    std::optional<WindowOverlayView> next_top;
    for (auto view = next_views.rbegin(); view != next_views.rend(); ++view) {
        if (candidate(entry_for(view->owner))) {
            next_top = *view;
            break;
        }
    }
    std::optional<runtime::ComponentId> next_domain;
    if (next_top) {
        for (auto view = next_views.rbegin(); view != next_views.rend(); ++view) {
            const auto& entry = entry_for(view->owner);
            if (candidate(entry) && (entry.desired.focus_trap || view->kind == WindowOverlayKind::FullscreenSpin)) {
                next_domain = descendant(*services_, next_top->root, view->root) ? view->root : next_top->root;
                break;
            }
        }
    }
    const auto old_top = top();
    const bool top_changed = (old_top ? std::optional{old_top->owner} : std::nullopt) !=
                             (next_top ? std::optional{next_top->owner} : std::nullopt);
    if (top_changed && top_epoch_ == std::numeric_limits<std::uint64_t>::max()) {
        throw std::length_error("Window overlay top epoch exhausted");
    }
    std::vector<std::pair<std::uint64_t, input::InteractionId>> restores(std::size_t{0});
    if (top_changed) {
        restores.reserve(entries_.size());
        for (const auto& entry : entries_) {
            if (entry.published && !candidate(entry) && entry.desired.restore_focus && entry.trigger) {
                restores.emplace_back(entry.order, *entry.trigger);
            }
        }
        std::ranges::sort(restores, [](const auto& a, const auto& b) { return a.first > b.first; });
    }

    const bool boundary_changed = next_domain != input_domain_ || (next_domain && !input_lease_.valid());
    auto next_lease = input_lease_;
    auto next_authority = input_authority_;
    std::vector<std::pair<std::shared_ptr<WindowOverlayCompanionState>, input::InputCompanionLease>> changes(
        std::size_t{0});
    std::vector<input::InputCompanionLease> acquired(std::size_t{0});
    changes.reserve(companions_.size());
    acquired.reserve(companions_.size());
    bool acquired_boundary{};
    bool published{};
    try {
        if (boundary_changed) {
            if (next_domain) {
                next_lease = services_->create_input_constraint(services_->interactions().input_root(*next_domain),
                                                                input::InputConstraintKind::RestrictToSubtrees);
                acquired_boundary = true;
                next_authority = services_->interactions().input_authority(next_lease);
            } else {
                next_lease = {};
                next_authority = {};
            }
        }
        const bool modal_winner = next_top && next_top->kind != WindowOverlayKind::FullscreenSpin;
        for (const auto& entry : companions_) {
            const bool permitted = companion_permitted(*entry, next_domain, modal_winner);
            if ((permitted && entry->lease.valid() && entry->authority == next_authority) ||
                (!permitted && !entry->lease.valid())) {
                continue;
            }
            input::InputCompanionLease grant;
            if (permitted) {
                grant = services_->create_input_companion(next_authority, entry->owner, entry->controls);
                acquired.push_back(grant);
            }
            changes.emplace_back(entry, grant);
        }
        const auto focus_before = services_->focus().state().focused;
        if (boundary_changed || !changes.empty()) {
            auto batch = services_->interactions().begin_input_constraint_update();
            if (boundary_changed) {
                if (input_lease_.valid()) {
                    batch.deactivate(input_lease_);
                }
                if (next_domain) {
                    batch.replace(next_lease, {{services_->interactions().input_root(*next_domain)}, true});
                }
                batch.boundary_view(next_domain ? std::optional{next_authority} : std::nullopt);
            }
            for (const auto& [entry, grant] : changes) {
                if (entry->lease.valid()) {
                    batch.enable(entry->lease, false);
                }
                if (grant.valid()) {
                    batch.enable(grant);
                }
            }
            static_cast<void>(services_->interactions().publish_input_constraints(batch));
        }
        published = true;
        const auto focus_ticket = services_->focus().focus_request_revision();
        auto old_lease = input_lease_;
        input_lease_ = next_lease;
        input_authority_ = next_authority;
        input_domain_ = next_domain;
        published_.swap(next_views);
        // These setters are non-allocating after the live-root/layer checks above. No callback can
        // observe the new input authority with the old branch or painter ranks.
        for (auto& entry : entries_) {
            if (!entry.root.valid()) {
                continue;
            }
            entry.published = candidate(entry);
            if (focus_before) {
                const auto* focused = services_->interactions().find(*focus_before);
                if (focused && descendant(*services_, focused->component, entry.root.component())) {
                    entry.last_focus = focus_before;
                }
            }
            const auto view = std::ranges::find_if(
                published_, [&](const auto& value) { return value.owner == entry.ticket.owner_.component(); });
            const auto root = entry.root.component();
            static_cast<void>(services_->components().set_window_layer(root, entry.desired.layer_priority));
            static_cast<void>(services_->components().set_window_layer_stack(
                root, view == published_.end() ? std::nullopt : std::optional{view->paint_rank}));
            static_cast<void>(services_->components().set_branch_active(root, entry.desired.paint_visible));
        }
        for (const auto& [entry, grant] : changes) {
            auto old = entry->lease;
            entry->lease = grant;
            entry->authority = next_authority;
            static_cast<void>(old.release());
        }
        if (boundary_changed) {
            static_cast<void>(old_lease.release());
        }
        std::erase_if(entries_, [](const auto& entry) { return !entry.ticket.owner_.valid() || !entry.root.valid(); });
        dirty_ = false;
        services_->mark_scene_structure_dirty();
        if (top_changed) {
            ++top_epoch_;
            focus_epoch_ = top_epoch_;
            focus_ticket_ = focus_ticket;
            focus_pending_ = true;
            restore_target_.reset();
            for (const auto& [order, target] : restores) {
                if (can_focus(*services_, target)) {
                    restore_target_ = target;
                    break;
                }
            }
        }
    } catch (...) {
        dirty_ = true;
        if (!published) {
            for (auto grant : acquired) {
                static_cast<void>(grant.release());
            }
            if (acquired_boundary) {
                static_cast<void>(next_lease.release());
            }
        }
        throw;
    }
    if (!quiet) {
        // This can invoke application code, including disposal. No access to this object afterward.
        static_cast<void>(services_->synchronize_input_constraints());
    }
    return true;
}

std::optional<WindowOverlayView> WindowOverlayStack::top() const noexcept {
    for (auto view = published_.rbegin(); view != published_.rend(); ++view) {
        const auto entry = std::ranges::find_if(
            entries_, [&](const auto& value) { return value.ticket.owner_.component() == view->owner; });
        if (entry != entries_.end() && entry->published) {
            return *view;
        }
    }
    return {};
}

bool WindowOverlayStack::is_top(runtime::ComponentId owner) const noexcept {
    const auto winner = top();
    return winner && winner->owner == owner;
}

std::span<const WindowOverlayView> WindowOverlayStack::ordered() const noexcept {
    return published_;
}

std::optional<runtime::ComponentId> WindowOverlayStack::input_domain() const noexcept {
    return input_domain_;
}

std::optional<runtime::ComponentId> WindowOverlayStack::drawer_parent(runtime::ComponentId owner) const noexcept {
    auto entry = std::ranges::find_if(published_, [&](const auto& view) { return view.owner == owner; });
    while (entry != published_.end() && entry->parent) {
        const auto parent = *entry->parent;
        entry = std::ranges::find_if(published_, [&](const auto& view) { return view.owner == parent; });
        if (entry != published_.end() && entry->kind == WindowOverlayKind::Drawer) {
            return parent;
        }
    }
    return {};
}

void WindowOverlayStack::synchronize_focus() {
    if (!focus_pending_ || focus_epoch_ != top_epoch_ || !services_->focus().state().window_active) {
        return;
    }
    if ((input_domain_ && !services_->interactions().is_boundary_authority(input_authority_)) ||
        (!input_domain_ && services_->interactions().has_input_confinement())) {
        return;
    }
    const auto winner = top();
    const auto focused = services_->focus().state().focused;
    std::optional<input::InteractionId> target;
    if (restore_target_ && can_focus(*services_, *restore_target_)) {
        target = restore_target_;
    } else if (focused && can_focus(*services_, *focused)) {
        // A live focus (including an explicitly authorized Tour control) has no recovery to perform.
        focus_pending_ = false;
        restore_target_.reset();
        return;
    } else if (winner) {
        const auto entry =
            std::ranges::find_if(entries_, [&](const auto& e) { return e.ticket.owner_.component() == winner->owner; });
        if (entry != entries_.end()) {
            std::optional<input::InteractionId> body;
            std::optional<input::InteractionId> primary;
            for (auto id : services_->interactions().declaration_order()) {
                const auto* record = services_->interactions().find(id);
                if (!record || !record->tab_stop || !can_focus(*services_, id) ||
                    !descendant(*services_, record->component, winner->root) || id == entry->desired.fallback_focus) {
                    continue;
                }
                if (!target) {
                    target = id;
                }
                if (!body && entry->desired.body.valid() &&
                    descendant(*services_, record->component, entry->desired.body)) {
                    body = id;
                }
                if (record->component == entry->desired.primary_action) {
                    primary = id;
                }
            }
            target = body ? body : primary ? primary : target;
            if (!target && entry->desired.fallback_focus && can_focus(*services_, *entry->desired.fallback_focus)) {
                target = entry->desired.fallback_focus;
            }
        }
    }
    const auto ticket = focus_ticket_;
    focus_pending_ = false;
    restore_target_.reset();
    // Consume once before focus callbacks; a newer explicit request or top publication wins.
    static_cast<void>(services_->focus().defer_focus_if_current(ticket, target, input::FocusModality::keyboard));
}

void WindowOverlayStack::on_window_active(bool active) {
    if (active && (focus_pending_ || (top() && !services_->focus().state().focused))) {
        focus_pending_ = true;
        focus_epoch_ = top_epoch_;
        focus_ticket_ = services_->focus().focus_request_revision();
    }
}

void WindowOverlayStack::on_destroy() noexcept {
    dirty_ = true;
    try {
        static_cast<void>(reconcile(true));
    } catch (...) {
        services_->request_frame();
    }
}

void WindowOverlayStack::dispose() noexcept {
    lifetime_->stack = nullptr;
    for (auto& companion : companions_) {
        companion->registered = false;
        static_cast<void>(companion->lease.release());
    }
    companions_.clear();
    static_cast<void>(input_lease_.release());
    input_authority_ = {};
    input_domain_.reset();
    entries_.clear();
    published_.clear();
    restore_target_.reset();
    focus_pending_ = false;
    dirty_ = false;
}

std::optional<std::uint64_t> WindowOverlayStack::companion_order() const noexcept {
    const auto winner = top();
    return winner && winner->kind != WindowOverlayKind::FullscreenSpin ? std::optional{winner->paint_rank}
                                                                       : std::nullopt;
}

void WindowOverlayStack::retire_companion(WindowOverlayCompanionState* state) noexcept {
    std::erase_if(companions_, [state](const auto& entry) { return entry.get() == state; });
}

WindowOverlayCompanion WindowOverlayStack::register_tour_controls(input::InputRootRef owner,
                                                                  std::vector<input::InputRootRef> controls,
                                                                  bool active) {
    if (!services_->interactions().owns_input_root(owner) || controls.empty() || controls.size() > 128) {
        throw std::invalid_argument("Tour companion requires its exact window owner and bounded controls");
    }
    for (const auto& root : controls) {
        if (!services_->interactions().owns_input_root(root) ||
            !descendant(*services_, root.component(), owner.component())) {
            throw std::invalid_argument("Tour controls must belong to their registered owner");
        }
    }
    WindowOverlayCompanion result;
    result.state_ = std::make_shared<WindowOverlayCompanionState>();
    result.state_->lifetime = lifetime_;
    result.state_->owner = owner;
    result.state_->controls = std::move(controls);
    result.state_->enabled = active;
    companions_.push_back(result.state_);
    try {
        services_->components().scope(owner.component()).on_cleanup([result]() mutable {
            static_cast<void>(result.release());
        });
        dirty_ = true;
        static_cast<void>(reconcile(false));
    } catch (...) {
        static_cast<void>(result.release());
        throw;
    }
    return result;
}

bool WindowOverlayStack::update_companion(WindowOverlayCompanionState& entry, bool enabled,
                                          std::span<const input::InputConstraintUpdate> changes) {
    if (!entry.registered || !entry.owner.valid()) {
        return false;
    }
    for (const auto& change : changes) {
        if (change.lease.owner() != entry.owner) {
            throw std::invalid_argument("Tour transaction may change only its own leases");
        }
    }
    static_cast<void>(reconcile(true));
    const auto winner = top();
    const bool was_enabled = entry.enabled;
    entry.enabled = enabled;
    const bool permitted =
        companion_permitted(entry, input_domain_, winner && winner->kind != WindowOverlayKind::FullscreenSpin);
    entry.enabled = was_enabled;
    const bool reuse = permitted && entry.lease.valid() && entry.authority == input_authority_;
    if (entry.enabled == enabled && changes.empty() && (reuse || (!permitted && !entry.lease.valid()))) {
        return false;
    }
    input::InputCompanionLease grant = reuse ? entry.lease : input::InputCompanionLease{};
    bool acquired{};
    bool published{};
    try {
        if (permitted && !reuse) {
            grant = services_->create_input_companion(input_authority_, entry.owner, entry.controls);
            acquired = true;
        }
        auto batch = services_->interactions().begin_input_constraint_update();
        for (const auto& change : changes) {
            if (change.spec) {
                batch.replace(change.lease, *change.spec);
            } else {
                batch.deactivate(change.lease);
            }
        }
        if (!reuse && entry.lease.valid()) {
            batch.enable(entry.lease, false);
        }
        if (grant.valid()) {
            batch.enable(grant);
        }
        const bool changed = services_->interactions().publish_input_constraints(batch);
        published = true;
        auto old = entry.lease;
        entry.lease = grant;
        entry.authority = input_authority_;
        entry.enabled = enabled;
        if (!reuse) {
            static_cast<void>(old.release());
        }
        return services_->synchronize_input_constraints() || changed;
    } catch (...) {
        if (acquired && !published) {
            static_cast<void>(grant.release());
        }
        throw;
    }
}
} // namespace ryn::detail
