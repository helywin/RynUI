#pragma once

#include <ryn/reactive.hpp>

#include <concepts>
#include <memory>
#include <type_traits>
#include <utility>
#include <variant>

namespace ryn {
namespace detail {

struct PropAccess;

} // namespace detail

template <typename T>
    requires(std::is_object_v<T> && !std::is_array_v<T> && !std::is_const_v<T> && !std::is_volatile_v<T> &&
             std::copy_constructible<T>)
class Prop final {
    // Large token configurations must not multiply recursive composition stack frames.
    // Static values are immutable; sharing their storage preserves Prop value semantics.
    static constexpr bool indirect_static_value = sizeof(T) > 256;
    using StaticValue = std::conditional_t<indirect_static_value, std::shared_ptr<const T>, T>;

public:
    Prop(T value) : source_(std::in_place_type<StaticValue>, store_value(std::move(value))) {}

    template <typename Equal>
    Prop(Signal<T, Equal> signal)
        : source_(std::in_place_type<Binding<T>>, bind([signal = std::move(signal)]() -> T { return signal.get(); })) {}

    Prop(Binding<T> binding) : source_(std::in_place_type<Binding<T>>, std::move(binding)) {}

private:
    friend struct detail::PropAccess;

    static StaticValue store_value(T value) {
        if constexpr (indirect_static_value) {
            return std::make_shared<const T>(std::move(value));
        } else {
            return std::move(value);
        }
    }

    std::variant<StaticValue, Binding<T>> source_;
};

} // namespace ryn
