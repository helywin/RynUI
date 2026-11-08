#pragma once

#include "component/tooltip_component.hpp"
#include <ryn/popconfirm.hpp>

namespace ryn::detail {
struct PopconfirmAnchorBinding final {
    runtime::CapturedComponentContext context;
    WindowComponentServices* services{};
    runtime::ComponentId owner;
    runtime::ComponentId trigger;
    std::thread::id thread{std::this_thread::get_id()};
    bool alive{true};
    bool controlled{};
    bool service_only{};
    bool open{};
    std::weak_ptr<void> service;
};

struct PopconfirmRefState final {
    std::weak_ptr<PopconfirmAnchorBinding> binding;
    bool assigned{};
};

[[nodiscard]] bool popconfirm_anchor_live(const PopconfirmAnchorBinding&);

struct PopconfirmSnapshot final {
    runtime::ComponentId trigger;
    runtime::ComponentId popup;
    runtime::ComponentId confirm;
    runtime::ComponentId cancel;
    runtime::ComponentId description;
    TooltipSnapshot geometry;
    bool paint_visible{};
    bool transitioning{};
    float opacity{};
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
    [[nodiscard]] std::function<void()> detach_action(runtime::ComponentId) noexcept;
    std::size_t tick_auxiliary(animation::AnimationTime) override;
    std::optional<animation::AnimationTime> next_auxiliary_deadline() const override;
    void synchronize_auxiliary_motion() override;
    bool synchronize_auxiliary_fragments() override;
    [[nodiscard]] bool presentation_enabled(runtime::ComponentId) const;
    void arm_service_close(runtime::ComponentId);

private:
    void refresh(runtime::ComponentId id);
    void request_open(runtime::ComponentId id, bool open, bool restore = true);
    void suppress(runtime::ComponentId id);
    void confirm(runtime::ComponentId id);
    void cancel(runtime::ComponentId id);
    void update_theme(runtime::ComponentId id, bool geometry);
    void observe(runtime::ComponentId id, animation::AnimationTime time);
    WindowComponentServices* services_;
    std::vector<runtime::ComponentId> mounted_;
    bool window_active_{true};
    animation::MonotonicTimeCursor clock_;
    std::shared_ptr<int> lifetime_{std::make_shared<int>(0)};
};
} // namespace ryn::detail
