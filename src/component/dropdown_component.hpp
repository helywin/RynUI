#pragma once

#include "component/tooltip_component.hpp"
#include <ryn/dropdown.hpp>

namespace ryn::detail {
struct DropdownSnapshot final {
    runtime::ComponentId trigger;
    runtime::ComponentId popup;
    runtime::ComponentId menu;
    TooltipSnapshot geometry;
};

class DropdownComponentHost final : public WindowComponentParticipant {
public:
    explicit DropdownComponentHost(WindowComponentServices& services);
    ~DropdownComponentHost();
    void mount(const DropdownProps&, const DropdownTrigger&);

    [[nodiscard]] std::span<const runtime::ComponentId> mounted() const {
        return mounted_;
    }

    [[nodiscard]] DropdownSnapshot snapshot(runtime::ComponentId id) const;
    void* begin_mount() noexcept override;
    void end_mount(void*) noexcept override;
    void on_destroy() noexcept override;

    void on_dispose() noexcept override {
        mounted_.clear();
    }

    void on_window_active(bool active) override;
    void on_input_boundary_changed() override;
    bool on_keyboard_input(const input::KeyboardInputEvent&) override;
    bool on_scroll_input(const input::ScrollInputEvent&) override;
    void on_pointer_input(const input::PointerInputEvent&, std::optional<input::InteractionId>,
                          std::optional<input::InteractionId>) override;
    void position_window_layers(runtime::Size, runtime::Rect) override;
    void synchronize_auxiliary_geometry(runtime::Size, runtime::Rect) override;

private:
    void refresh(runtime::ComponentId id);
    void request_open(runtime::ComponentId id, bool open, DropdownOpenSource source, bool restore = true);
    void suppress(runtime::ComponentId id);
    void update_theme(runtime::ComponentId id, bool geometry);
    WindowComponentServices* services_;
    std::vector<runtime::ComponentId> mounted_;
    bool window_active_{true};
};
} // namespace ryn::detail
