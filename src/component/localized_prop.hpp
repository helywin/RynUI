#pragma once

#include "runtime/config_context.hpp"

#include <ryn/prop.hpp>

#include <optional>
#include <functional>
#include <utility>

namespace ryn::detail {

// The optional is the source marker: an explicit complete Locale (including
// English and empty fields) never subscribes to inherited defaults.
template <typename T, typename Projection>
[[nodiscard]] Prop<T> localized_prop(const std::optional<Prop<T>>& explicit_value,
                                     std::shared_ptr<runtime::ConfigScope> config, Projection projection) {
    if (explicit_value) {
        return *explicit_value;
    }
    return bind([config = std::move(config), projection = std::move(projection)]() -> T {
        const auto& snapshot = config->snapshot();
        return std::invoke(projection, snapshot.catalog.pack(snapshot.locale.effective).data());
    });
}

} // namespace ryn::detail
