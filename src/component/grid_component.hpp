#pragma once

#include "runtime/component_host.hpp"

#include <ryn/grid.hpp>

namespace ryn::detail {

struct GridColumnSnapshot final {
    runtime::ComponentId component;
    ColOptions options;
    runtime::Rect bounds;
    bool hidden{};
};

[[nodiscard]] std::vector<GridColumnSnapshot> grid_columns(const runtime::ComponentHost& host,
                                                           runtime::ComponentId row);

} // namespace ryn::detail
