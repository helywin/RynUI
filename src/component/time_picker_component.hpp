#pragma once
#include "component/window_component_services.hpp"
#include "component/time_picker_model.hpp"

namespace ryn::detail {
struct TimePickerSnapshot final {
    runtime::ComponentId selector;
    runtime::ComponentId popup;
    runtime::ComponentId footer;
    runtime::ComponentId confirm;
    runtime::ComponentId cancel;
    runtime::ComponentId clear;
    std::array<runtime::ComponentId, 3> columns;
    std::array<std::vector<runtime::ComponentId>, 3> rows;
    std::array<runtime::Rect, 3> column_clips;
    std::array<float, 3> scroll{};
    runtime::Rect bounds;
    TimePickerValue value;
    TimeOfDay pending;
    String draft;
    int active_column{};
    bool visible{};
    bool confirmable{};
};

class TimePickerComponentHost final : public WindowComponentParticipant {
public:
    explicit TimePickerComponentHost(WindowComponentServices&);
    ~TimePickerComponentHost();
    void mount(const TimePickerProps&, const std::optional<TimePickerFooter>&);

    [[nodiscard]] std::span<const runtime::ComponentId> mounted() const {
        return mounted_;
    }

    [[nodiscard]] TimePickerSnapshot snapshot(runtime::ComponentId) const;
    void* begin_mount() noexcept override;
    void end_mount(void*) noexcept override;
    void on_destroy() noexcept override;

    void on_dispose() noexcept override {
        mounted_.clear();
    }

    void on_window_active(bool) override;
    void on_input_boundary_changed() override;
    void prepare_auxiliary_layout() override;
    bool on_keyboard_input(const input::KeyboardInputEvent&) override;
    void on_pointer_input(const input::PointerInputEvent&, std::optional<input::InteractionId>,
                          std::optional<input::InteractionId>) override;
    bool on_scroll_input(const input::ScrollInputEvent&) override;
    bool consume_scroll_boundary(const input::ScrollInputEvent&) const;
    void position_window_layers(runtime::Size, runtime::Rect) override;
    void position_window_layer(runtime::ComponentId, runtime::Size, runtime::Rect);
    void synchronize_auxiliary_geometry(runtime::Size, runtime::Rect) override;

private:
    void refresh(runtime::ComponentId, bool geometry = true);
    void update_theme(runtime::ComponentId, bool geometry);
    void request_open(runtime::ComponentId, bool, bool restore = false);
    void suppress(runtime::ComponentId);
    void publish(runtime::ComponentId, TimePickerValue, bool close);
    void confirm(runtime::ComponentId);
    void choose(runtime::ComponentId, TimeColumn, int);
    void mount_rows(runtime::ComponentId, int);
    WindowComponentServices* services_;
    std::vector<runtime::ComponentId> mounted_;
    std::vector<runtime::ComponentId> positioning_;
    bool window_active_{true};
};
} // namespace ryn::detail
