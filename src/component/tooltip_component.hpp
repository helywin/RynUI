#pragma once

#include "component/window_component_services.hpp"
#include <ryn/tooltip.hpp>

namespace ryn::detail {
struct TooltipSnapshot final {
    runtime::Rect anchor;
    runtime::Rect bounds;
    TooltipPlacement placement{TooltipPlacement::Top};
    bool visible{};
};

[[nodiscard]] TooltipSnapshot position_tooltip(runtime::Rect anchor, runtime::Size popup, runtime::Rect viewport,
                                               TooltipPlacement placement, float distance, bool adjust);

class TooltipComponentHost final : public WindowComponentParticipant {
public:
    explicit TooltipComponentHost(WindowComponentServices& services);
    ~TooltipComponentHost();
    void mount(const TooltipProps& props, const TooltipTrigger& trigger);

    [[nodiscard]] std::span<const runtime::ComponentId> mounted() const {
        return mounted_;
    }

    [[nodiscard]] TooltipSnapshot snapshot(runtime::ComponentId id) const;
    void* begin_mount() noexcept override;
    void end_mount(void*) noexcept override;
    void on_destroy() noexcept override;

    void on_dispose() noexcept override {
        mounted_.clear();
    }

    void on_window_active(bool active) override;
    bool on_keyboard_input(const input::KeyboardInputEvent& event) override;
    void position_window_layers(runtime::Size viewport, runtime::Rect clip) override;
    void synchronize_auxiliary_geometry(runtime::Size viewport, runtime::Rect clip) override;
    std::size_t tick_auxiliary(animation::AnimationTime time) override;
    std::optional<animation::AnimationTime> next_auxiliary_deadline() const override;

private:
    void request_open(runtime::ComponentId id, bool open);
    void observe(runtime::ComponentId id, animation::AnimationTime time);
    void update_theme(runtime::ComponentId id, bool geometry);
    void synchronize_visibility(runtime::ComponentId id);
    WindowComponentServices* services_;
    std::vector<runtime::ComponentId> mounted_;
    bool window_active_{true};
};
} // namespace ryn::detail
