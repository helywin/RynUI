#pragma once
#include "component/window_component_services.hpp"
#include <ryn/float_button.hpp>

namespace ryn::detail {
struct FloatButtonSnapshot final {
    runtime::Rect bounds;
    runtime::ComponentId menu;
    runtime::ComponentId trigger;
    runtime::ComponentId progress_view;
    bool open{};
    bool visible{};
    bool dragging{};
    bool animating{};
    float progress{};
};

class FloatButtonComponentHost final : public WindowComponentParticipant {
public:
    explicit FloatButtonComponentHost(WindowComponentServices&);
    ~FloatButtonComponentHost();
    void mount(const FloatButtonProps&, const FloatButtonSlots&);
    void mount(const FloatButtonGroupProps&, const FloatButtonGroupContent&);
    void mount(const FloatButtonBackTopProps&, FloatButtonSlots);

    std::span<const runtime::ComponentId> mounted() const {
        return mounted_;
    }

    FloatButtonSnapshot snapshot(runtime::ComponentId) const;
    void* begin_mount() noexcept override;
    void end_mount(void*) noexcept override;
    void on_destroy() noexcept override;
    void on_dispose() noexcept override;
    void on_window_active(bool) override;
    void on_input_boundary_changed() override;

    void prepare_auxiliary_layout() override {
        on_input_boundary_changed();
    }

    void position_window_layers(runtime::Size, runtime::Rect) override;
    void synchronize_auxiliary_geometry(runtime::Size, runtime::Rect) override;
    void on_pointer_input(const input::PointerInputEvent&, std::optional<input::InteractionId>,
                          std::optional<input::InteractionId>) override;
    bool on_keyboard_input(const input::KeyboardInputEvent&) override;
    std::size_t tick_auxiliary(animation::AnimationTime) override;
    std::optional<animation::AnimationTime> next_auxiliary_deadline() const override;

private:
    runtime::ComponentId create(const FloatButtonProps&, const FloatButtonSlots&);
    void refresh(runtime::ComponentId);
    void request_open(runtime::ComponentId, bool);
    void activate(runtime::ComponentId);
    void stop(runtime::ComponentId);
    WindowComponentServices* services_;
    std::vector<runtime::ComponentId> mounted_;
};
} // namespace ryn::detail
