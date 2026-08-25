#include <ryn/version.hpp>

#include "internal/layer_anchors.hpp"

namespace ryn {

Version version() noexcept {
    return {
        static_cast<std::uint16_t>(RYNUI_VERSION_MAJOR),
        static_cast<std::uint16_t>(RYNUI_VERSION_MINOR),
        static_cast<std::uint16_t>(RYNUI_VERSION_PATCH),
    };
}

namespace detail {

void reactive_layer_anchor() noexcept {}

} // namespace detail
} // namespace ryn
