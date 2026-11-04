#pragma once
#include <ryn/picker_common.hpp>
#include "component/window_component_services.hpp"
#include "component/input_component.hpp"
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
    std::function<bool(T, PickerEndpoint, const Value&)> disabled;
    std::function<void(Value)> on_change;
    std::function<void(Value, PickerEndpoint)> on_calendar;
    std::function<void(bool)> on_open;
    std::function<void(PickerEndpoint)> on_endpoint;

    [[nodiscard]] bool valid(const Value& value) const {
        if (!value.start && !value.end) {
            return true;
        }
        if ((!value.start && !policy.allow_empty[0]) || (!value.end && !policy.allow_empty[1])) {
            return false;
        }
        return (!value.start || !disabled || !disabled(*value.start, PickerEndpoint::Start, value)) &&
               (!value.end || !disabled || !disabled(*value.end, PickerEndpoint::End, value));
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
        const bool changed = endpoint.get() != next;
        endpoint.set(next);
        if (changed && open.get()) {
            focus_active();
        }
    }

    void activate(PickerEndpoint next) try {
        validate_endpoint(next);
        const bool changed = endpoint.get() != next;
        if (!alive || policy.disabled[static_cast<std::size_t>(next)]) {
            return;
        }
        if (!controlled_endpoint) {
            endpoint.set(next);
        }
        if (on_endpoint) {
            on_endpoint(next);
        }
        if (alive && changed && endpoint.get() == next && open.get()) {
            focus_active();
        }
    } catch (...) {

        alive = false;
        const auto cleanup = retire;
        if (cleanup) {
            cleanup();
        }
        throw;
    }

    void request_open(bool next) try {
        if (!alive) {
            return;
        }
        const bool restore = !next && focus_owned && focus_owned();
        if (!controlled_open) {
            open.set(next);
        }
        if (!next) {
            pending.set(accepted.get());
        }
        if (on_open) {
            on_open(next);
        }
        if (alive && restore && !open.get()) {
            focus_active();
        }
    } catch (...) {

        alive = false;
        const auto cleanup = retire;
        if (cleanup) {
            cleanup();
        }
        throw;
    }

    bool commit() try {
        if (!alive) {
            return false;
        }
        auto next = pending.get();
        if (policy.order && next.start && next.end && *next.start > *next.end) {
            if (policy.disabled[0] || policy.disabled[1]) {
                return false;
            }
            std::swap(next.start, next.end);
        }
        if (!valid(next) || !alive) {
            return false;
        }
        const auto before = accepted.get();
        if (!controlled) {
            accepted.set(next);
        }
        if (before != next && on_change) {
            on_change(next);
        }
        if (alive) {
            request_open(false);
        }
        return true;
    } catch (...) {

        alive = false;
        const auto cleanup = retire;
        if (cleanup) {
            cleanup();
        }
        throw;
    }

    void preset(const PickerRangePreset<T>& preset) try {
        const auto value = preset.value();
        if (!alive) {
            return;
        }
        pending.set(value);
        if (on_calendar) {
            on_calendar(value, PickerEndpoint::End);
        }
        if (alive) {
            static_cast<void>(commit());
        }
    } catch (...) {

        alive = false;
        const auto cleanup = retire;
        if (cleanup) {
            cleanup();
        }
        throw;
    }

    void clear() {
        auto next = pending.get();
        if (!policy.disabled[0]) {
            next.start.reset();
        }
        if (!policy.disabled[1]) {
            next.end.reset();
        }
        pending.set(next);
        static_cast<void>(commit());
    }

    void choose(PickerEndpoint which, std::optional<T> value) try {
        validate_endpoint(which);
        if (!alive || policy.disabled[static_cast<std::size_t>(which)]) {
            return;
        }
        auto next = pending.get();
        (which == PickerEndpoint::Start ? next.start : next.end) = value;
        if (value && disabled && disabled(*value, which, next)) {
            return;
        }
        if (!alive) {
            return;
        }
        pending.set(next);
        if (on_calendar) {
            on_calendar(next, which);
        }
        if (!alive) {
            return;
        }
        if (which == PickerEndpoint::Start && !policy.disabled[1]) {
            activate(PickerEndpoint::End);
        } else {
            static_cast<void>(commit());
        }
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
    for (auto input : state->inputs) {
        services.input_runtime()->configure_form_reset(input, [&services, state, reset_models] {
            if (!state->alive || state->resetting) {
                return;
            }
            state->resetting = true;
            try {
                state->pending.set(state->accepted.get());
                reset_models();
                for (const auto& reference : state->references) {
                    if (state->alive) {
                        const auto reset = capture_form_input(reference).second;
                        if (reset) {
                            reset();
                        }
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
        });
    }
}

} // namespace ryn::detail
