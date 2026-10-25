#pragma once

#include "component/tooltip_component.hpp"
#include <ryn/popover.hpp>

namespace ryn::detail {
struct PopoverSnapshot final {
    runtime::ComponentId trigger;
    runtime::ComponentId popup;
    runtime::ComponentId title;
    runtime::ComponentId content;
    runtime::ComponentId arrow;
    TooltipSnapshot geometry;
};

class PopoverComponentHost final : public WindowComponentParticipant {
public:
    explicit PopoverComponentHost(WindowComponentServices&);
    ~PopoverComponentHost();
    void mount(const PopoverProps&, const PopoverTrigger&, const PopoverSlots&);

    [[nodiscard]] std::span<const runtime::ComponentId> mounted() const {
        return mounted_;
    }

    [[nodiscard]] PopoverSnapshot snapshot(runtime::ComponentId) const;
    void* begin_mount() noexcept override;
    void end_mount(void*) noexcept override;
    void on_destroy() noexcept override;
    void on_dispose() noexcept override;
    void on_window_active(bool) override;
    void on_input_boundary_changed() override;
    bool on_keyboard_input(const input::KeyboardInputEvent&) override;
    void on_pointer_input(const input::PointerInputEvent&, std::optional<input::InteractionId>,
                          std::optional<input::InteractionId>) override;
    bool consume_scroll_boundary(const input::ScrollInputEvent&);
    void prepare_auxiliary_layout() override;
    void position_window_layers(runtime::Size, runtime::Rect) override;
    void position_window_layer(runtime::ComponentId, runtime::Size, runtime::Rect);
    void position_ancestors(runtime::ComponentId, runtime::Size, runtime::Rect);
    [[nodiscard]] bool owns_escape(runtime::ComponentId ancestor, runtime::ComponentId focused) const;
    void synchronize_auxiliary_geometry(runtime::Size, runtime::Rect) override;
    std::size_t tick_auxiliary(animation::AnimationTime) override;
    std::optional<animation::AnimationTime> next_auxiliary_deadline() const override;

private:
    void refresh(runtime::ComponentId);
    void request_open(runtime::ComponentId, bool, bool restore = true, bool keyboard = false);
    void suppress(runtime::ComponentId);
    void observe(runtime::ComponentId, animation::AnimationTime);
    void update_theme(runtime::ComponentId, bool);
    WindowComponentServices* services_;
    std::vector<runtime::ComponentId> mounted_;
    std::vector<runtime::ComponentId> positioning_;
    bool window_active_{true};
};
} // namespace ryn::detail
