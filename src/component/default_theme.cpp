#include "component/default_theme.hpp"

namespace ryn::detail {

const DefaultThemeSnapshot& default_theme_snapshot() noexcept {
    static const DefaultThemeSnapshot snapshot = resolve_theme();
    return snapshot;
}

} // namespace ryn::detail
