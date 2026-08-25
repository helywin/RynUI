#pragma once

#include <ryn/detail/reactive_runtime.hpp>

#include <functional>
#include <memory>
#include <optional>
#include <type_traits>
#include <utility>

namespace ryn {
namespace detail {

template <typename T, typename Equal>
class SignalCell final : public ReactiveSource {
public:
    SignalCell(T initial_value, Equal equal)
        : value_(std::move(initial_value)), equal_(std::move(equal)) {}

    [[nodiscard]] const T& get() {
        record_dependency(*this);
        return value_;
    }

    bool set(T value) {
        if (equal_(value_, value)) {
            return false;
        }
        value_ = std::move(value);
        notify_observers();
        return true;
    }

private:
    T value_;
    [[no_unique_address]] Equal equal_;
};

template <typename T, typename Equal>
class MemoCell final : public ReactiveSource,
                       public std::enable_shared_from_this<MemoCell<T, Equal>> {
public:
    static std::shared_ptr<MemoCell> create(
        std::function<T()> compute,
        Equal equal) {
        auto cell = std::shared_ptr<MemoCell>(
            new MemoCell(std::move(compute), std::move(equal)));
        cell->initialize();
        return cell;
    }

    MemoCell(const MemoCell&) = delete;
    MemoCell& operator=(const MemoCell&) = delete;

    ~MemoCell() override {
        if (observer_) {
            observer_->deactivate();
        }
    }

    [[nodiscard]] const T& get() {
        record_dependency(*this);
        return *value_;
    }

private:
    MemoCell(std::function<T()> compute, Equal equal)
        : compute_(std::move(compute)), equal_(std::move(equal)) {}

    void initialize() {
        std::weak_ptr<MemoCell> weak_cell = this->shared_from_this();
        observer_ = observe(
            ObserverPhase::memo,
            [weak_cell] {
                if (const auto cell = weak_cell.lock()) {
                    cell->recompute();
                }
            });
    }

    void recompute() {
        T next_value = compute_();
        if (!value_.has_value()) {
            value_.emplace(std::move(next_value));
            return;
        }
        if (equal_(*value_, next_value)) {
            return;
        }
        *value_ = std::move(next_value);
        notify_observers();
    }

    std::function<T()> compute_;
    [[no_unique_address]] Equal equal_;
    std::optional<T> value_;
    std::shared_ptr<ObserverNode> observer_;
};

} // namespace detail

template <typename T, typename Equal = std::equal_to<T>>
class Signal final {
public:
    explicit Signal(T initial_value, Equal equal = {})
        : cell_(std::make_shared<detail::SignalCell<T, Equal>>(
              std::move(initial_value),
              std::move(equal))) {}

    [[nodiscard]] const T& get() const {
        return cell_->get();
    }

    bool set(T value) const {
        return cell_->set(std::move(value));
    }

private:
    std::shared_ptr<detail::SignalCell<T, Equal>> cell_;
};

template <typename T, typename Equal = std::equal_to<T>>
class Memo final {
public:
    template <typename Compute>
    explicit Memo(Compute compute, Equal equal = {})
        : cell_(detail::MemoCell<T, Equal>::create(
              std::function<T()>(std::move(compute)),
              std::move(equal))) {}

    [[nodiscard]] const T& get() const {
        return cell_->get();
    }

private:
    std::shared_ptr<detail::MemoCell<T, Equal>> cell_;
};

class Effect;
class BindingHandle;
class Scope;

template <typename T>
class Binding;

template <typename T, typename Apply>
BindingHandle connect_binding(Scope& scope, const Binding<T>& binding, Apply&& apply);

template <typename T>
class Binding final {
public:
    explicit Binding(std::function<T()> compute) : compute_(std::move(compute)) {}

    [[nodiscard]] T get() const {
        return compute_();
    }

private:
    std::function<T()> compute_;
};

template <typename Function>
auto bind(Function&& function) {
    using Result = std::remove_cvref_t<std::invoke_result_t<Function>>;
    return Binding<Result>(std::function<Result()>(std::forward<Function>(function)));
}

class Scope final {
public:
    Scope();
    Scope(const Scope&) = delete;
    Scope& operator=(const Scope&) = delete;
    Scope(Scope&&) = delete;
    Scope& operator=(Scope&&) = delete;
    ~Scope();

    void on_cleanup(std::function<void()> cleanup);
    void dispose() noexcept;
    [[nodiscard]] bool active() const noexcept;

private:
    template <typename Function>
    friend Effect effect(Scope& scope, Function&& function);
    template <typename T, typename Apply>
    friend BindingHandle connect_binding(
        Scope& scope,
        const Binding<T>& binding,
        Apply&& apply);

    void own_observer(std::shared_ptr<detail::ObserverNode> observer);

    struct State;
    std::unique_ptr<State> state_;
};

class Effect final {
public:
    Effect() = default;

    [[nodiscard]] bool active() const noexcept {
        const auto observer = observer_.lock();
        return observer && observer->active();
    }

private:
    template <typename Function>
    friend Effect effect(Scope& scope, Function&& function);

    explicit Effect(const std::shared_ptr<detail::ObserverNode>& observer)
        : observer_(observer) {}

    std::weak_ptr<detail::ObserverNode> observer_;
};

class BindingHandle final {
public:
    BindingHandle() = default;

    [[nodiscard]] bool active() const noexcept {
        const auto observer = observer_.lock();
        return observer && observer->active();
    }

private:
    template <typename T, typename Apply>
    friend BindingHandle connect_binding(
        Scope& scope,
        const Binding<T>& binding,
        Apply&& apply);

    explicit BindingHandle(const std::shared_ptr<detail::ObserverNode>& observer)
        : observer_(observer) {}

    std::weak_ptr<detail::ObserverNode> observer_;
};

template <typename Function>
Effect effect(Scope& scope, Function&& function) {
    if (!scope.active()) {
        return Effect{};
    }
    auto observer = detail::observe(
        detail::ObserverPhase::effect,
        std::function<void()>(std::forward<Function>(function)),
        false);
    observer->run();
    scope.own_observer(observer);
    return Effect(observer);
}

template <typename T, typename Apply>
BindingHandle connect_binding(Scope& scope, const Binding<T>& binding, Apply&& apply) {
    if (!scope.active()) {
        return BindingHandle{};
    }
    auto observer = detail::observe(
        detail::ObserverPhase::binding,
        [binding, apply_function = std::function<void(T)>(std::forward<Apply>(apply))]() mutable {
            apply_function(binding.get());
        },
        false);
    observer->run();
    scope.own_observer(observer);
    return BindingHandle(observer);
}

template <typename Function>
std::invoke_result_t<Function> batch(Function&& function) {
    using Result = std::invoke_result_t<Function>;
    auto& scheduler = detail::Scheduler::current();
    scheduler.begin_batch();

    if constexpr (std::is_void_v<Result>) {
        try {
            std::invoke(std::forward<Function>(function));
        } catch (...) {
            try {
                scheduler.end_batch();
            } catch (...) {
            }
            throw;
        }
        scheduler.end_batch();
    } else {
        Result result = [&]() -> Result {
            try {
                return std::invoke(std::forward<Function>(function));
            } catch (...) {
                try {
                    scheduler.end_batch();
                } catch (...) {
                }
                throw;
            }
        }();
        scheduler.end_batch();
        return result;
    }
}

} // namespace ryn
