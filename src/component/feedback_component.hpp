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

private:
    void refresh_alert(runtime::ComponentId, bool);
    void refresh_spin(runtime::ComponentId, bool);
    void refresh_progress(runtime::ComponentId, bool);
    void close_alert(runtime::ComponentId);
    WindowComponentServices* services_;
    std::vector<runtime::ComponentId> alerts_;
    std::vector<runtime::ComponentId> spins_;
    std::vector<runtime::ComponentId> progress_;
};
} // namespace ryn::detail
