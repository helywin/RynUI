#include "component/drawer_model.hpp"
#include <algorithm>
#include <cmath>
#include <stdexcept>

namespace ryn::detail {
void validate_drawer_geometry(DrawerGeometry geometry) {
    if (!std::isfinite(geometry.width) || geometry.width <= 0 || !std::isfinite(geometry.height) ||
        geometry.height <= 0) {
        throw std::invalid_argument("Drawer width and height must be positive and finite");
    }
    switch (geometry.placement) {
    case DrawerPlacement::Top:
    case DrawerPlacement::Bottom:
    case DrawerPlacement::Left:
    case DrawerPlacement::Right:
        break;
    default:
        throw std::invalid_argument("Drawer placement is invalid");
    }
}

runtime::Rect drawer_bounds(DrawerGeometry geometry, runtime::Rect window) {
    validate_drawer_geometry(geometry);
    const bool vertical = geometry.placement == DrawerPlacement::Left || geometry.placement == DrawerPlacement::Right;
    const float width =
        vertical ? std::min(geometry.width, std::max(0.0F, window.width)) : std::max(0.0F, window.width);
    const float height =
        vertical ? std::max(0.0F, window.height) : std::min(geometry.height, std::max(0.0F, window.height));
    return {window.x + (geometry.placement == DrawerPlacement::Right ? window.width - width : 0),
            window.y + (geometry.placement == DrawerPlacement::Bottom ? window.height - height : 0), width, height};
}
} // namespace ryn::detail
