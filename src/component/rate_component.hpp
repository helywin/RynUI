#pragma once
#include "component/window_component_services.hpp"
#include <ryn/rate.hpp>

namespace ryn::detail {
struct RateItemSnapshot final {
    runtime::ComponentId component;
    runtime::NodeId node;
    runtime::NodeId background;
    runtime::NodeId foreground;
    input::InteractionId interaction;
};

struct RateSnapshot final {
    float value{};
    std::optional<float> hover;
    int count{};
    input::InteractionId focus;
    std::vector<RateItemSnapshot> items;
};

class RateComponentHost final : public WindowComponentParticipant {
public:
    explicit RateComponentHost(WindowComponentServices&);
    ~RateComponentHost();
    void mount(const RateProps&);

    [[nodiscard]] std::span<const runtime::ComponentId> mounted() const {
        return mounted_;
    }

    [[nodiscard]] RateSnapshot snapshot(runtime::ComponentId) const;
    void* begin_mount() noexcept override;
    void end_mount(void*) noexcept override;
    void on_destroy() noexcept override;

    void on_dispose() noexcept override {
        mounted_.clear();
    }

    void on_window_active(bool) override;
    void on_input_boundary_changed() override;
    bool on_keyboard_input(const input::KeyboardInputEvent&) override;
    void on_pointer_input(const input::PointerInputEvent&, std::optional<input::InteractionId>,
                          std::optional<input::InteractionId>) override;
    void synchronize_auxiliary_geometry(runtime::Size, runtime::Rect) override;

private:
    void mount_item(runtime::ComponentId, std::size_t);
    void ensure_items(runtime::ComponentId, int);
    void refresh(runtime::ComponentId, bool geometry);
    void reconcile_input(runtime::ComponentId);
    void set_hover(runtime::ComponentId, std::optional<float>);
    void commit(runtime::ComponentId, float, bool clear);
    runtime::Size measure(runtime::ComponentId, layout::LayoutEngine&, layout::Constraints);
    void place(runtime::ComponentId, layout::LayoutEngine&, runtime::Rect);
    WindowComponentServices* services_;
    std::vector<runtime::ComponentId> mounted_;
    bool window_active_{true};
};
} // namespace ryn::detail
