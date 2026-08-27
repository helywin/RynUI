#include "component/default_theme.hpp"

namespace ryn::detail {

const DefaultThemeSnapshot& default_theme_snapshot() noexcept {
    static constexpr DefaultThemeSnapshot snapshot;
    return snapshot;
}

} // namespace ryn::detail
