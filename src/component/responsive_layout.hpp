#pragma once

#include "component/window_component_services.hpp"

#include <ryn/breakpoints.hpp>

namespace ryn::detail {

// The window publishes available logical width before any component measurement.
// Responsive components share this signal even inside narrow nested containers.
class ResponsiveLayoutHost final : public WindowComponentParticipant {
public:
    explicit ResponsiveLayoutHost(WindowComponentServices& services);
    ~ResponsiveLayoutHost() override;
    void* begin_mount() noexcept override;
    void end_mount(void* previous) noexcept override;

    void synchronize_auxiliary_geometry(runtime::Size, runtime::Rect) override {}

    void set_window_width(float width);

    [[nodiscard]] const Signal<float>& window_width() const noexcept {
        return width_;
    }

    [[nodiscard]] WindowComponentServices& services() const noexcept {
        return *services_;
    }

private:
    WindowComponentServices* services_;
    Signal<float> width_{0};
};

[[nodiscard]] ResponsiveLayoutHost& require_responsive_layout_host();

} // namespace ryn::detail
