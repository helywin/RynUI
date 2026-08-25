#pragma once

#include <ryn/detail/reactive_runtime.hpp>

#include <functional>
#include <memory>
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

} // namespace ryn
