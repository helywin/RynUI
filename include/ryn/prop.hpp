#pragma once

#include <ryn/reactive.hpp>

#include <concepts>
#include <type_traits>
#include <utility>
#include <variant>

namespace ryn {
namespace detail {

struct PropAccess;

} // namespace detail

template <typename T>
    requires(
        std::is_object_v<T>
        && !std::is_array_v<T>
        && !std::is_const_v<T>
        && !std::is_volatile_v<T>
        && std::copy_constructible<T>)
class Prop final {
public:
    Prop(T value) : source_(std::in_place_type<T>, std::move(value)) {}

    template <typename Equal>
    Prop(Signal<T, Equal> signal)
        : source_(
              std::in_place_type<Binding<T>>,
              bind([signal = std::move(signal)]() -> T {
                  return signal.get();
              })) {}

    Prop(Binding<T> binding)
        : source_(std::in_place_type<Binding<T>>, std::move(binding)) {}

private:
    friend struct detail::PropAccess;

    std::variant<T, Binding<T>> source_;
};

} // namespace ryn
