#pragma once
#include "runtime/geometry.hpp"
#include <ryn/drawer.hpp>

namespace ryn::detail {
struct DrawerGeometry final {
    DrawerPlacement placement{DrawerPlacement::Right};
    float width{378};
    float height{378};
    friend bool operator==(DrawerGeometry, DrawerGeometry) = default;
};

void validate_drawer_geometry(DrawerGeometry);
[[nodiscard]] runtime::Rect drawer_bounds(DrawerGeometry, runtime::Rect window);
} // namespace ryn::detail
