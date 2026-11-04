#include "component/responsive_layout.hpp"

#include <cmath>
#include <stdexcept>

namespace ryn::detail {
namespace {
thread_local ResponsiveLayoutHost* active_responsive_layout{};
}

ResponsiveLayoutHost::ResponsiveLayoutHost(WindowComponentServices& services) : services_(&services) {
    services.attach(*this);
}

ResponsiveLayoutHost::~ResponsiveLayoutHost() {
    services_->detach(*this);
}

void* ResponsiveLayoutHost::begin_mount() noexcept {
    auto* previous = active_responsive_layout;
    active_responsive_layout = this;
    return previous;
}

void ResponsiveLayoutHost::end_mount(void* previous) noexcept {
    active_responsive_layout = static_cast<ResponsiveLayoutHost*>(previous);
}

void ResponsiveLayoutHost::set_window_width(float width) {
    if (!std::isfinite(width) || width < 0) {
        throw std::invalid_argument("Window width must be finite and non-negative");
    }
    width_.set(width);
}

ResponsiveLayoutHost& require_responsive_layout_host() {
    if (!active_responsive_layout) {
        throw std::logic_error("Responsive layouts require an active window component host");
    }
    return *active_responsive_layout;
}

} // namespace ryn::detail
