#pragma once

#include "component/window_component_services.hpp"
#include <ryn/tree_select.hpp>

namespace ryn::detail {
struct TreeSelectSnapshot final {
    runtime::ComponentId selector;
    runtime::ComponentId popover;
    runtime::ComponentId tree;
    runtime::Rect popup;
    String value;
    String label;
    bool open{};
};

class TreeSelectComponentHost final : public WindowComponentParticipant {
public:
    explicit TreeSelectComponentHost(WindowComponentServices&);
    ~TreeSelectComponentHost();
    void mount(const TreeSelectProps&, const std::vector<TreeSelectItem>&);

    [[nodiscard]] std::span<const runtime::ComponentId> mounted() const {
        return mounted_;
    }

    [[nodiscard]] TreeSelectSnapshot snapshot(runtime::ComponentId) const;
    void* begin_mount() noexcept override;
    void end_mount(void*) noexcept override;
    void on_destroy() noexcept override;
    void on_dispose() noexcept override;
    void on_window_active(bool) override;
    void on_input_boundary_changed() override;
    void on_focus_changed();
    bool on_keyboard_input(const input::KeyboardInputEvent&) override;
    void on_pointer_input(const input::PointerInputEvent&, std::optional<input::InteractionId>,
                          std::optional<input::InteractionId>) override;
    void position_window_layers(runtime::Size, runtime::Rect) override;
    void synchronize_auxiliary_geometry(runtime::Size, runtime::Rect) override;

private:
    bool intact(runtime::ComponentId);
    bool editable(runtime::ComponentId);
    void refresh(runtime::ComponentId);
    void request_open(runtime::ComponentId, bool, bool keyboard = false);
    void choose(runtime::ComponentId, String);
    void update_theme(runtime::ComponentId);
    WindowComponentServices* services_;
    std::vector<runtime::ComponentId> mounted_;
    bool window_active_{true};
};
} // namespace ryn::detail
