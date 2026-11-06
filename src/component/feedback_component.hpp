#pragma once
#include "component/window_component_services.hpp"
#include <ryn/alert.hpp>
#include <ryn/spin.hpp>
#include <ryn/progress.hpp>

namespace ryn::detail {
struct AlertSnapshot final {
    runtime::ComponentId icon;
    runtime::ComponentId body;
    runtime::ComponentId message;
    runtime::ComponentId description;
    runtime::ComponentId action;
    runtime::ComponentId close;
    input::InteractionId close_interaction;
    bool visible{true};
    bool closable{};
    bool has_description{};
    Status status{Status::Default};
    AlertPalette palette;
    bool banner{};
    bool closing{};
    float exit_fraction{1};
    float marquee_offset{};
};

struct SpinSnapshot final {
    runtime::ComponentId content;
    runtime::ComponentId overlay;
    runtime::ComponentId indicator;
    runtime::ComponentId description;
    input::InteractionId shield;
    bool spinning{true};
    bool in_view{};
    bool wrapped{};
    bool requested{true};
    bool fullscreen{};
    bool automatic{};
    std::optional<float> percent;
    runtime::Rect overlay_bounds;
};

struct ProgressSnapshot final {
    runtime::ComponentId info;
    float percent{};
    ProgressType type{ProgressType::Line};
    ProgressStatus status{ProgressStatus::Normal};
    Color color;
    runtime::Rect track;
    float sweep_degrees{};
};

class FeedbackComponentHost final : public WindowComponentParticipant {
public:
    explicit FeedbackComponentHost(WindowComponentServices& services);
    ~FeedbackComponentHost();
    void mount(const AlertProps&, const AlertSlots&);
    void mount(const SpinProps&, const std::optional<SpinContent>&, const SpinSlots&);
    void mount(const ProgressProps&);

    [[nodiscard]] std::span<const runtime::ComponentId> alerts() const {
        return alerts_;
    }

    [[nodiscard]] std::span<const runtime::ComponentId> spins() const {
        return spins_;
    }

    [[nodiscard]] std::span<const runtime::ComponentId> progress() const {
        return progress_;
    }

    [[nodiscard]] AlertSnapshot alert_snapshot(runtime::ComponentId) const;
    [[nodiscard]] SpinSnapshot spin_snapshot(runtime::ComponentId) const;
    [[nodiscard]] ProgressSnapshot progress_snapshot(runtime::ComponentId) const;
    void* begin_mount() noexcept override;
    void end_mount(void*) noexcept override;
    void on_destroy() noexcept override;
    void on_dispose() noexcept override;
    void synchronize_auxiliary_geometry(runtime::Size, runtime::Rect) override;
    bool on_scroll_input(const input::ScrollInputEvent&) override;
    void on_pointer_input(const input::PointerInputEvent&, std::optional<input::InteractionId>,
                          std::optional<input::InteractionId>) override;
    void position_window_layers(runtime::Size, runtime::Rect) override;
    void synchronize_auxiliary_motion() override;
    void on_window_active(bool) override;
    void on_input_boundary_changed() override;
    std::size_t tick_auxiliary(animation::AnimationTime) override;
    std::optional<animation::AnimationTime> next_auxiliary_deadline() const override;

private:
    void synchronize_alert_motion();
    std::size_t tick_alerts(animation::AnimationTime);
    std::optional<animation::AnimationTime> next_alert_deadline() const;
    void refresh_alert(runtime::ComponentId, bool);
    void refresh_spin(runtime::ComponentId, bool);
    void update_spin_delay(runtime::ComponentId);
    void refresh_progress(runtime::ComponentId, bool);
    void close_alert(runtime::ComponentId);
    void set_alert_visible(runtime::ComponentId, bool);
    bool alert_motion_allowed(runtime::ComponentId) const;
    void settle_alert(runtime::ComponentId);
    WindowComponentServices* services_;
    std::vector<runtime::ComponentId> alerts_;
    std::vector<runtime::ComponentId> spins_;
    std::vector<runtime::ComponentId> progress_;
    animation::MonotonicTimeCursor clock_;
};
} // namespace ryn::detail
