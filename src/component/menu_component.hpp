#pragma once

#include "component/window_component_services.hpp"
#include <ryn/menu.hpp>

namespace ryn::detail {
struct MenuSnapshot final {
    std::vector<runtime::ComponentId> rows;
    MenuKeys selected_keys;
    MenuKeys open_keys;
    std::optional<String> active_key;
    input::InteractionId interaction;
    float scroll{};
    std::vector<runtime::Rect> popups;
    bool overflow{};
    MenuColorToken colors;
};

class MenuComponentHost final : public WindowComponentParticipant {
public:
    explicit MenuComponentHost(WindowComponentServices& services);
    ~MenuComponentHost();
    void mount(const MenuProps& props, bool action_menu = false);
    static void validate(const MenuProps& props);

    [[nodiscard]] std::span<const runtime::ComponentId> mounted() const {
        return mounted_;
    }

    [[nodiscard]] MenuSnapshot snapshot(runtime::ComponentId id) const;
    bool close_deepest(runtime::ComponentId id);
    void focus_menu(runtime::ComponentId id, bool last = false);
    void* begin_mount() noexcept override;
    void end_mount(void*) noexcept override;
    void on_destroy() noexcept override;

    void on_dispose() noexcept override {
        mounted_.clear();
    }

    void on_window_active(bool active) override;
    void on_input_boundary_changed() override;
    void on_pointer_input(const input::PointerInputEvent&, std::optional<input::InteractionId>,
                          std::optional<input::InteractionId>) override;
    void position_window_layers(runtime::Size, runtime::Rect) override;
    bool on_keyboard_input(const input::KeyboardInputEvent&) override;
    std::size_t tick_auxiliary(animation::AnimationTime) override;
    std::optional<animation::AnimationTime> next_auxiliary_deadline() const override;
    bool on_scroll_input(const input::ScrollInputEvent&) override;
    void synchronize_auxiliary_geometry(runtime::Size, runtime::Rect) override;

private:
    void refresh(runtime::ComponentId id, bool geometry = false);
    void update_theme(runtime::ComponentId id, bool geometry);
    void activate(runtime::ComponentId id, std::size_t index);
    void toggle(runtime::ComponentId id, std::size_t index, bool open);
    void reconcile(runtime::ComponentId id, const std::vector<MenuItem>& items);
    void bind_row(runtime::ComponentId id, runtime::ComponentId row, const MenuItem& item);
    void mount_rows(runtime::ComponentId id, const std::vector<MenuItem>& items, std::optional<std::size_t> parent,
                    std::size_t depth);
    WindowComponentServices* services_;
    std::vector<runtime::ComponentId> mounted_;
    bool window_active_{true};
};
} // namespace ryn::detail
