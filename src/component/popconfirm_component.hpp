#pragma once

#include "component/tooltip_component.hpp"
#include <ryn/popconfirm.hpp>

namespace ryn::detail {
struct PopconfirmSnapshot final {
    runtime::ComponentId trigger;
    runtime::ComponentId popup;
    runtime::ComponentId confirm;
    runtime::ComponentId cancel;
    runtime::ComponentId description;
    TooltipSnapshot geometry;
};

class PopconfirmComponentHost final : public WindowComponentParticipant {
public:
    explicit PopconfirmComponentHost(WindowComponentServices& services);
    ~PopconfirmComponentHost();
    void mount(const PopconfirmProps& props, const PopconfirmTrigger& trigger, const PopconfirmSlots& slots);

    [[nodiscard]] std::span<const runtime::ComponentId> mounted() const {
        return mounted_;
    }

    [[nodiscard]] PopconfirmSnapshot snapshot(runtime::ComponentId id) const;
    void* begin_mount() noexcept override;
    void end_mount(void*) noexcept override;
    void on_destroy() noexcept override;
    void on_dispose() noexcept override;
    void on_window_active(bool active) override;
    void on_input_boundary_changed() override;
    bool on_keyboard_input(const input::KeyboardInputEvent& event) override;
    void on_pointer_input(const input::PointerInputEvent& event, std::optional<input::InteractionId> hit,
                          std::optional<input::InteractionId> origin) override;
    void position_window_layers(runtime::Size viewport, runtime::Rect clip) override;
    void synchronize_auxiliary_geometry(runtime::Size viewport, runtime::Rect clip) override;

private:
    void refresh(runtime::ComponentId id);
    void request_open(runtime::ComponentId id, bool open, bool restore = true);
    void suppress(runtime::ComponentId id);
    void confirm(runtime::ComponentId id);
    void cancel(runtime::ComponentId id);
    void update_theme(runtime::ComponentId id, bool geometry);
    WindowComponentServices* services_;
    std::vector<runtime::ComponentId> mounted_;
    bool window_active_{true};
};
} // namespace ryn::detail
