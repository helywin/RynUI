#pragma once

#include <ryn/prop.hpp>

#include <functional>
#include <optional>
#include <utility>
#include <variant>

namespace ryn::detail {

struct PropAccess final {
    template <typename T>
    [[nodiscard]] static const T* static_value(const Prop<T>& prop) noexcept {
        return std::get_if<T>(&prop.source_);
    }

    template <typename T>
    [[nodiscard]] static const Binding<T>* binding(const Prop<T>& prop) noexcept {
        return std::get_if<Binding<T>>(&prop.source_);
    }
};

template <typename T, typename Apply, typename Equal = std::equal_to<T>>
BindingHandle connect_prop(
    Scope& scope,
    const Prop<T>& prop,
    Apply&& apply,
    Equal equal = {}) {
    if (!scope.active()) {
        return BindingHandle{};
    }

    auto apply_function = std::function<void(T)>(std::forward<Apply>(apply));
    if (const auto* value = PropAccess::static_value(prop)) {
        apply_function(*value);
        return BindingHandle{};
    }

    const auto* binding = PropAccess::binding(prop);
    return connect_binding(
        scope,
        *binding,
        [apply_function = std::move(apply_function),
         equal = std::move(equal),
         last_value = std::optional<T>{}](T value) mutable {
            if (last_value.has_value()
                    && std::invoke(equal, *last_value, value)) {
                return;
            }
            last_value = value;
            apply_function(std::move(value));
        });
}

} // namespace ryn::detail
