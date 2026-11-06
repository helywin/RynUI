#pragma once
#include "component/window_component_services.hpp"
#include <ryn/date_picker.hpp>
#include "component/picker_candidate.hpp"

namespace ryn::detail {
struct DatePickerSnapshot final {
    runtime::ComponentId input;
    runtime::ComponentId popup;
    runtime::ComponentId footer;
    std::vector<runtime::ComponentId> cells;
    std::vector<runtime::ComponentId> navigation;
    DatePickerValue value;
    CivilDate active;
    String draft;
    runtime::Rect bounds;
    bool visible{};
    std::vector<component::RetainedSurfaceId> surfaces;
    DatePickerMultipleValue multiple_value;
    DatePickerMultipleValue pending_multiple;
    DatePickerDateTimeValue date_time_value;
    DatePickerValue pending_date;
    TimePickerValue pending_time;
    DatePickerMode mode{DatePickerMode::Date};
    std::vector<DatePickerCellInfo> cell_info;
    bool grid_pending{};
};

class DatePickerComponentHost final : public WindowComponentParticipant {
public:
    explicit DatePickerComponentHost(WindowComponentServices&);
    ~DatePickerComponentHost();
    void mount(const DatePickerProps&, const std::optional<DatePickerFooter>&);
    void mount_range(const DateRangePickerProps&, const std::optional<DatePickerFooter>&);
    void mount_range(const DateTimeRangePickerProps&, const std::optional<DatePickerFooter>&);

    [[nodiscard]] std::span<const runtime::ComponentId> mounted() const {
        return mounted_;
    }

    [[nodiscard]] DatePickerSnapshot snapshot(runtime::ComponentId) const;
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
    bool consume_scroll_boundary(const input::ScrollInputEvent&);
    void position_window_layers(runtime::Size, runtime::Rect) override;
    void position_window_layer(runtime::ComponentId, runtime::Size, runtime::Rect);
    void synchronize_auxiliary_geometry(runtime::Size, runtime::Rect) override;

private:
    void refresh(runtime::ComponentId, bool geometry = true);
    void update_theme(runtime::ComponentId, bool geometry);
    void request_open(runtime::ComponentId, bool, bool restore = false);
    PickerRequestDisposition choose(runtime::ComponentId, DatePickerValue, bool close = true, bool confirm = false,
                                    bool from_text = false, const PickerCandidate& = {}, TimePickerValue = {});
    void navigate(runtime::ComponentId, int);
    void confirm(runtime::ComponentId);
    PickerRequestDisposition commit(runtime::ComponentId, bool blur);
    void mount_items(runtime::ComponentId);
    runtime::ComponentId mount_item(runtime::ComponentId, std::size_t, DatePickerValue, DatePickerMode,
                                    std::vector<runtime::ComponentId>* created = nullptr);
    void reconcile_items(runtime::ComponentId);
    void reorder_items(runtime::ComponentId);
    WindowComponentServices* services_;
    std::vector<runtime::ComponentId> mounted_;
    std::vector<runtime::ComponentId> positioning_;
    bool active_{true};
};
} // namespace ryn::detail
