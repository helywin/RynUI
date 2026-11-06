#pragma once
#include <ryn/picker_common.hpp>
#include "component/window_component_services.hpp"
#include "component/input_component.hpp"
#include "component/picker_candidate.hpp"
#include "runtime/prop_connection.hpp"
#include <ryn/reactive.hpp>
#include <set>
#include <stdexcept>

namespace ryn::detail {
template <typename F, typename... Args>
void picker_callback(WindowComponentServices& services, runtime::ComponentId id, const F& fn, Args&&... args) {
    try {
        fn(std::forward<Args>(args)...);
    } catch (...) {
        static_cast<void>(services.retire_failed_component(id));
        throw;
    }
}

template <typename T> void validate_range_presets(const std::vector<PickerRangePreset<T>>& presets) {
    std::set<std::string> keys;
    for (const auto& p : presets) {
        if (p.key.empty() || !p.value || !keys.insert(std::string{p.key.bytes()}).second) {
            throw std::invalid_argument("Range presets require unique keys and value callbacks");
        }
    }
}

// Composite selectors keep the public authority distinct from incomplete endpoint drafts.
template <typename T> struct PickerRangeState final {
    using Value = PickerRange<T>;
    Signal<Value> accepted{Value{}};
    Signal<Value> pending{Value{}};
    using Semantics = std::array<PickerCandidateSemantics, 2>;
    Semantics pending_semantics;
    Semantics accepted_semantics;
    std::uint64_t revision{};

    struct Echo final {
        Value value;
        Semantics semantics;
        std::uint64_t revision;
    };

    std::optional<Echo> echo;
    Signal<bool> open{false};
    Signal<PickerEndpoint> endpoint{PickerEndpoint::Start};
    PickerRangePolicy policy;
    Signal<PickerRangePolicy> policy_source{PickerRangePolicy{}};
    bool controlled{};
    bool controlled_open{};
    bool controlled_endpoint{};
    bool alive{true};
    bool resetting{};
    std::vector<runtime::ComponentId> inputs;
    std::vector<InputRef> references;
    std::vector<runtime::ComponentId> owners;
    std::function<void()> retire;
    std::function<bool()> focus_owned;
    std::function<bool(T, PickerEndpoint, const Value&, const PickerCandidateSemantics&)> disabled;
    std::function<void()> end_editors;
    std::function<void(bool)> sync_semantics;
    std::function<void(Value)> on_change;
    std::function<void(Value, PickerEndpoint)> on_calendar;
    std::function<void(bool)> on_open;
    std::function<void(PickerEndpoint)> on_endpoint;

    [[nodiscard]] bool current(std::uint64_t ticket, const PickerCandidate& source = {}) const {
        return alive && revision == ticket && source.active();
    }

    void invalidate() {
        ++revision;
    }

    bool restore_pending() {
        const auto value = accepted.get();
        pending_semantics = accepted_semantics;
        const auto ticket = ++revision;
        if (sync_semantics) {
            sync_semantics(false);
        }
        pending.set(value);
        if (current(ticket) && sync_semantics) {
            sync_semantics(true);
        }
        return current(ticket);
    }

    void reconcile_authority(Value value) {
        const bool echoed = echo && echo->revision == revision && echo->value == value;
        accepted_semantics = echoed ? echo->semantics : Semantics{};
        pending_semantics = accepted_semantics;
        if (!echoed) {
            ++revision;
        }
        const auto ticket = revision;
        accepted.set(value);
        if (!current(ticket)) {
            return;
        }
        if (sync_semantics) {
            sync_semantics(false);
        }
        pending.set(value);
        if (current(ticket) && sync_semantics) {
            sync_semantics(true);
        }
        if (!echoed && current(ticket) && end_editors) {
            end_editors();
        }
    }

    [[nodiscard]] bool valid(const Value& value, const Semantics& semantics, std::uint64_t ticket,
                             const PickerCandidate& source = {}, bool allow_partial = false) const {
        if (!value.start && !value.end) {
            return true;
        }
        if (!allow_partial && ((!value.start && !policy.allow_empty[0]) || (!value.end && !policy.allow_empty[1]))) {
            return false;
        }
        return current(ticket, source) &&
               (!value.start || !disabled || !disabled(*value.start, PickerEndpoint::Start, value, semantics[0])) &&
               current(ticket, source) &&
               (!value.end || !disabled || !disabled(*value.end, PickerEndpoint::End, value, semantics[1])) &&
               current(ticket, source);
    }

    static void validate_endpoint(PickerEndpoint next) {
        if (next != PickerEndpoint::Start && next != PickerEndpoint::End) {
            throw std::invalid_argument("Invalid picker range endpoint");
        }
    }

    void focus_active() {
        const auto index = static_cast<std::size_t>(endpoint.get());
        if (alive && index < references.size() && !policy.disabled[index]) {
            static_cast<void>(references[index].focus({InputFocusCursor::All}));
        }
    }

    void reconcile_endpoint(PickerEndpoint next) {
        validate_endpoint(next);
        ++revision;
        const auto ticket = revision;
        const bool changed = endpoint.get() != next;
        endpoint.set(next);
        if (current(ticket) && changed && open.get()) {
            focus_active();
        }
    }

    PickerRequestDisposition activate(PickerEndpoint next, const PickerCandidate& source = {}) try {
        validate_endpoint(next);
        const bool changed = endpoint.get() != next;
        if (!alive || !source.active() || policy.disabled[static_cast<std::size_t>(next)]) {
            return PickerRequestDisposition::Superseded;
        }
        const auto ticket = ++revision;
        if (!controlled_endpoint) {
            endpoint.set(next);
        }
        if (!current(ticket, source)) {
            return PickerRequestDisposition::Superseded;
        }
        if (on_endpoint) {
            on_endpoint(next);
        }
        if (current(ticket, source) && changed && endpoint.get() == next && open.get()) {
            focus_active();
        }
        return current(ticket, source) ? PickerRequestDisposition::Consumed : PickerRequestDisposition::Superseded;
    } catch (...) {

        alive = false;
        const auto cleanup = retire;
        if (cleanup) {
            cleanup();
        }
        throw;
    }

    PickerRequestDisposition request_open(bool next, const PickerCandidate& source = {}) try {
        if (!alive || !source.active()) {
            return PickerRequestDisposition::Superseded;
        }
        const bool restore = !next && focus_owned && focus_owned();
        auto ticket = ++revision;
        if (!controlled_open) {
            open.set(next);
        }
        if (!current(ticket, source)) {
            return PickerRequestDisposition::Superseded;
        }
        if (!next) {
            pending_semantics = accepted_semantics;
            if (sync_semantics) {
                sync_semantics(false);
            }
            pending.set(accepted.get());
            if (current(ticket, source) && sync_semantics) {
                sync_semantics(true);
            }
            if (!current(ticket, source)) {
                return PickerRequestDisposition::Superseded;
            }
        }
        if (on_open) {
            on_open(next);
        }
        if (current(ticket, source) && restore && !open.get()) {
            focus_active();
        }
        return current(ticket, source) ? PickerRequestDisposition::Consumed : PickerRequestDisposition::Superseded;
    } catch (...) {

        alive = false;
        const auto cleanup = retire;
        if (cleanup) {
            cleanup();
        }
        throw;
    }

    PickerRequestDisposition commit(const PickerCandidate& source = {}) try {
        if (!alive || !source.active()) {
            return PickerRequestDisposition::Superseded;
        }
        auto next = pending.get();
        auto semantics = pending_semantics;
        const auto admission = revision;
        if (policy.order && next.start && next.end && *next.start > *next.end) {
            if (policy.disabled[0] || policy.disabled[1]) {
                return PickerRequestDisposition::Rejected;
            }
            std::swap(next.start, next.end);
            std::swap(semantics[0], semantics[1]);
        }
        if (!valid(next, semantics, admission, source)) {
            return current(admission, source) ? PickerRequestDisposition::Rejected
                                              : PickerRequestDisposition::Superseded;
        }
        const auto before = accepted.get();
        const auto ticket = ++revision;
        if (!controlled) {
            accepted_semantics = semantics;
            accepted.set(next);
        }
        if (!current(ticket, source)) {
            return PickerRequestDisposition::Superseded;
        }
        echo = Echo{next, semantics, ticket};
        if (before != next && on_change) {
            on_change(next);
        }
        echo.reset();
        if (current(ticket, source)) {
            return request_open(false, source);
        }
        return PickerRequestDisposition::Superseded;
    } catch (...) {

        alive = false;
        const auto cleanup = retire;
        if (cleanup) {
            cleanup();
        }
        throw;
    }

    void preset(const PickerRangePreset<T>& preset, const PickerCandidate& source = {}) try {
        if (!source.active()) {
            return;
        }
        const auto admission = revision;
        const auto value = preset.value();
        if (!current(admission, source)) {
            return;
        }
        const Semantics semantics{};
        if (!valid(value, semantics, admission, source, true)) {
            return;
        }
        pending_semantics = semantics;
        const auto ticket = ++revision;
        if (sync_semantics) {
            sync_semantics(false);
        }
        pending.set(value);
        if (current(ticket, source) && sync_semantics) {
            sync_semantics(true);
        }
        if (!current(ticket, source)) {
            return;
        }
        if (on_calendar) {
            on_calendar(value, PickerEndpoint::End);
        }
        if (current(ticket, source)) {
            static_cast<void>(commit(source));
        }
    } catch (...) {

        alive = false;
        const auto cleanup = retire;
        if (cleanup) {
            cleanup();
        }
        throw;
    }

    void clear(const PickerCandidate& source = {}) {
        if (!alive || !source.active()) {
            return;
        }
        auto next = pending.get();
        auto semantics = pending_semantics;
        if (!policy.disabled[0]) {
            next.start.reset();
            semantics[0] = {};
        }
        if (!policy.disabled[1]) {
            next.end.reset();
            semantics[1] = {};
        }
        pending_semantics = semantics;
        const auto ticket = ++revision;
        if (sync_semantics) {
            sync_semantics(false);
        }
        pending.set(next);
        if (current(ticket, source) && sync_semantics) {
            sync_semantics(true);
        }
        if (current(ticket, source)) {
            static_cast<void>(commit(source));
        }
    }

    PickerRequestDisposition choose(PickerEndpoint which, std::optional<T> value,
                                    const PickerCandidate& source = {}) try {
        validate_endpoint(which);
        if (!alive || !source.active()) {
            return PickerRequestDisposition::Superseded;
        }
        if (policy.disabled[static_cast<std::size_t>(which)]) {
            return PickerRequestDisposition::Rejected;
        }
        const auto admission = revision;
        auto next = pending.get();
        auto semantics = pending_semantics;
        const auto index = static_cast<std::size_t>(which);
        (which == PickerEndpoint::Start ? next.start : next.end) = value;
        semantics[index] = value ? source.semantics : PickerCandidateSemantics{};
        const bool rejected = value && disabled && disabled(*value, which, next, semantics[index]);
        if (!current(admission, source)) {
            return PickerRequestDisposition::Superseded;
        }
        if (rejected) {
            return PickerRequestDisposition::Rejected;
        }
        pending_semantics = semantics;
        const auto ticket = ++revision;
        if (sync_semantics) {
            sync_semantics(false);
        }
        pending.set(next);
        if (current(ticket, source) && sync_semantics) {
            sync_semantics(true);
        }
        if (!current(ticket, source)) {
            return PickerRequestDisposition::Consumed;
        }
        if (on_calendar) {
            on_calendar(next, which);
        }
        if (!current(ticket, source)) {
            return PickerRequestDisposition::Consumed;
        }
        if (which == PickerEndpoint::Start && !policy.disabled[1]) {
            static_cast<void>(activate(PickerEndpoint::End, source));
        } else {
            static_cast<void>(commit(source));
        }
        // Admission consumed this endpoint. Later supersession only stops the range tail;
        // the child still checks its own editor session before finishing that request.
        return PickerRequestDisposition::Consumed;
    } catch (...) {

        alive = false;
        const auto cleanup = retire;
        if (cleanup) {
            cleanup();
        }
        throw;
    }
};

template <typename T, typename Reset>
void connect_range_reset(WindowComponentServices& services, const std::shared_ptr<PickerRangeState<T>>& state,
                         Reset reset_models) {
    state->focus_owned = [&services, weak = std::weak_ptr{state}] {
        const auto live = weak.lock();
        const auto focused = services.focus().state().focused;
        const auto* record = focused ? services.interactions().find(*focused) : nullptr;
        if (!live || !live->alive || !record) {
            return false;
        }
        for (auto current = std::optional{record->component}; current && services.components().contains(*current);
             current = services.components().parent(*current)) {
            if (std::find(live->owners.begin(), live->owners.end(), *current) != live->owners.end()) {
                return true;
            }
        }
        return false;
    };
    state->end_editors = [weak = std::weak_ptr{state}, reset_models] {
        const auto state = weak.lock();
        if (!state || !state->alive || state->resetting) {
            return;
        }
        state->resetting = true;
        const auto ticket = state->revision;
        try {
            reset_models();
            const auto references = state->references;
            for (const auto& reference : references) {
                if (!state->current(ticket)) {
                    break;
                }
                const auto reset = capture_form_input(reference).second;
                if (reset) {
                    reset();
                }
            }
            state->resetting = false;
        } catch (...) {
            state->resetting = false;
            const auto retire = state->retire;
            if (retire) {
                retire();
            }
            throw;
        }
    };
    for (auto input : state->inputs) {
        services.input_runtime()->configure_form_reset(input, [state] {
            if (!state->alive || state->resetting) {
                return;
            }
            try {
                const bool restored = state->restore_pending();
                if (restored && state->end_editors) {
                    state->end_editors();
                }
            } catch (...) {
                const auto retire = state->retire;
                if (retire) {
                    retire();
                }
                throw;
            }
        });
    }
}

template <typename T, typename P>
void connect_range_business(Scope& scope, const std::shared_ptr<PickerRangeState<T>>& state, const Prop<P>& prop) {
    connect_prop(scope, prop, [state](const P& value) {
        state->invalidate();
        if constexpr (std::is_same_v<P, bool>) {
            if (value) {
                const bool restored = state->restore_pending();
                if (restored && state->end_editors) {
                    state->end_editors();
                }
            }
        }
    });
}

} // namespace ryn::detail
