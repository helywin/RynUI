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
    SelectValues values;
    std::vector<std::size_t> row_indices;
    std::size_t total_rows{};
    bool pending{};
    std::vector<runtime::ComponentId> tags;
    String overflow;
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
    void prepare_auxiliary_layout() override;
    bool on_keyboard_input(const input::KeyboardInputEvent&) override;
    void on_pointer_input(const input::PointerInputEvent&, std::optional<input::InteractionId>,
                          std::optional<input::InteractionId>) override;
    bool on_scroll_input(const input::ScrollInputEvent&) override;
    void position_window_layers(runtime::Size, runtime::Rect) override;
    void position_window_layer(runtime::ComponentId, runtime::Size, runtime::Rect);
    void synchronize_auxiliary_geometry(runtime::Size, runtime::Rect) override;

private:
    void refresh(runtime::ComponentId id, bool geometry = true);
    void request_open(runtime::ComponentId id, bool open);
    void choose(runtime::ComponentId id, std::optional<std::size_t> index);
    void rebuild_rows(runtime::ComponentId id);
    void mount_rows(runtime::ComponentId id);
    void mount_row(runtime::ComponentId, std::optional<std::size_t>);
    void mount_label(runtime::ComponentId, runtime::ComponentId);
    void reconcile_tags(runtime::ComponentId);
    void publish_values(runtime::ComponentId, SelectValues, bool clear = false);
    void edit(runtime::ComponentId, String);
    void load(runtime::ComponentId, String);
    void configure(runtime::ComponentId);
    void mount_popup(runtime::ComponentId);
    void rebuild_popup(runtime::ComponentId);
    std::size_t tick_auxiliary(animation::AnimationTime) override;
    std::optional<animation::AnimationTime> next_auxiliary_deadline() const override;
    void update_theme(runtime::ComponentId id, bool geometry = true);
    WindowComponentServices* services_;
    std::vector<runtime::ComponentId> mounted_;
    std::vector<runtime::ComponentId> positioning_;
    bool window_active_{true};
    std::size_t mount_depth_{};
};
} // namespace ryn::detail
