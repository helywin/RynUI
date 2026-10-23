#pragma once

#include "component/window_component_services.hpp"
#include <ryn/select.hpp>

namespace ryn::detail {
struct SelectSnapshot final {
    runtime::ComponentId selector;
    runtime::Rect popup;
    bool visible{};
    bool above{};
    SelectSelection value;
    String query;
    std::optional<std::size_t> active;
    std::vector<runtime::ComponentId> rows;
    float scroll{};
};

class SelectComponentHost final : public WindowComponentParticipant {
public:
    explicit SelectComponentHost(WindowComponentServices& services);
    ~SelectComponentHost();
    void mount(const SelectProps& props, const std::optional<SelectPrefix>& prefix,
               const std::optional<SelectSuffix>& suffix);

    [[nodiscard]] std::span<const runtime::ComponentId> mounted() const {
        return mounted_;
    }

    [[nodiscard]] SelectSnapshot snapshot(runtime::ComponentId id) const;
    void* begin_mount() noexcept override;
    void end_mount(void*) noexcept override;
    void on_destroy() noexcept override;

    void on_dispose() noexcept override {
        mounted_.clear();
    }

    void on_window_active(bool active) override;
    void on_input_boundary_changed() override;
    bool on_keyboard_input(const input::KeyboardInputEvent&) override;
    void on_pointer_input(const input::PointerInputEvent&, std::optional<input::InteractionId>,
                          std::optional<input::InteractionId>) override;
    bool on_scroll_input(const input::ScrollInputEvent&) override;
    void position_window_layers(runtime::Size, runtime::Rect) override;
    void synchronize_auxiliary_geometry(runtime::Size, runtime::Rect) override;

private:
    void refresh(runtime::ComponentId id, bool geometry = true);
    void request_open(runtime::ComponentId id, bool open);
    void choose(runtime::ComponentId id, std::optional<std::size_t> index);
    void rebuild_rows(runtime::ComponentId id);
    void mount_rows(runtime::ComponentId id);
    void update_theme(runtime::ComponentId id, bool geometry = true);
    WindowComponentServices* services_;
    std::vector<runtime::ComponentId> mounted_;
    bool window_active_{true};
};
} // namespace ryn::detail
