#pragma once

#include <ryn/detail/reactive_runtime.hpp>

#include <functional>
#include <memory>
#include <optional>
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

} // namespace ryn
