#pragma once
#include "component/window_component_services.hpp"
#include <ryn/calendar.hpp>

namespace ryn::detail {
struct CalendarSnapshot final {
    CivilDate value;
    CivilDate panel;
    CalendarMode mode;
    std::vector<CalendarCellInfo> cells;
    std::vector<runtime::ComponentId> components;
    runtime::ComponentId header;
};

class CalendarComponentHost final : public WindowComponentParticipant {
public:
    explicit CalendarComponentHost(WindowComponentServices&);
    ~CalendarComponentHost();
    void mount(const CalendarProps&);

    std::span<const runtime::ComponentId> mounted() const {
        return mounted_;
    }

    CalendarSnapshot snapshot(runtime::ComponentId) const;
    void* begin_mount() noexcept override;
    void end_mount(void*) noexcept override;
    void on_destroy() noexcept override;

    void on_dispose() noexcept override {
        mounted_.clear();
    }

    bool on_keyboard_input(const input::KeyboardInputEvent&) override;
    void on_input_boundary_changed() override;
    void on_window_active(bool) override;
    void prepare_auxiliary_layout() override;
    void synchronize_auxiliary_geometry(runtime::Size, runtime::Rect) override;

private:
    bool select(runtime::ComponentId, CivilDate, CalendarSelectSource);
    bool mode(runtime::ComponentId, CalendarMode);
    bool focus(runtime::ComponentId);
    void refresh(runtime::ComponentId, bool geometry = true);
    void cells(runtime::ComponentId, runtime::ComponentBuildContext* = nullptr);
    runtime::ComponentId header(runtime::ComponentId, CalendarHeaderRenderer,
                                runtime::ComponentBuildContext* = nullptr);
    runtime::ComponentId cell(runtime::ComponentId, CalendarCellInfo, CalendarCellRenderer, bool,
                              runtime::ComponentBuildContext*);
    runtime::Size measure(runtime::ComponentId, layout::LayoutEngine&, layout::Constraints);
    void place(runtime::ComponentId, layout::LayoutEngine&, runtime::Rect);
    WindowComponentServices* services_;
    std::vector<runtime::ComponentId> mounted_;
};
} // namespace ryn::detail
