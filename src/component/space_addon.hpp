#pragma once

#include "component/window_component_services.hpp"
#include "component/space_compact.hpp"

namespace ryn::detail {

struct SpaceAddonSnapshot final {
    ControlSize size{ControlSize::Middle};
    InputVariant variant{InputVariant::Outlined};
    InputStatus status{InputStatus::Default};
    bool disabled{};
    std::array<bool, 4> corners{true, true, true, true};
    Color fill;
    Color border;
    Color foreground;
    float border_width{};
    graphics::LogicalRoundedRect shape;
};

class SpaceAddonHost final : public WindowComponentParticipant {
public:
    explicit SpaceAddonHost(WindowComponentServices&);
    ~SpaceAddonHost();
    [[nodiscard]] SpaceAddonSnapshot snapshot(runtime::ComponentId) const;
    void mount(const SpaceAddonProps&, const SpaceAddonContent&);

private:
    void* begin_mount() noexcept override;
    void end_mount(void*) noexcept override;
    void on_destroy() noexcept override;
    void synchronize_auxiliary_geometry(runtime::Size, runtime::Rect) override;
    void update(runtime::ComponentId);
    WindowComponentServices* services_;
    std::vector<runtime::ComponentId> mounted_;
};

} // namespace ryn::detail
