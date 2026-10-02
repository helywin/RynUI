#pragma once
#include "component/window_component_services.hpp"
#include <ryn/divider.hpp>

namespace ryn::detail {
struct DividerSnapshot {
    runtime::Rect left;
    runtime::Rect right;
    runtime::Rect label;
    float margin{};
    float line_width{};
    bool vertical{};
    bool has_label{};
    bool dashed{};
    bool plain{};
    DividerVariant variant{DividerVariant::Solid};
    ControlSize size{ControlSize::Large};
    DividerOrientation orientation{DividerOrientation::Center};
    DividerDirection direction{DividerDirection::LeftToRight};
};

class DividerComponentHost final : public WindowComponentParticipant {
public:
    explicit DividerComponentHost(WindowComponentServices&);
    ~DividerComponentHost();
    void mount(const DividerProps&, const std::optional<DividerText>&);

    [[nodiscard]] std::span<const runtime::ComponentId> mounted() const {
        return mounted_;
    }

    [[nodiscard]] DividerSnapshot snapshot(runtime::ComponentId) const;
    void* begin_mount() noexcept override;
    void end_mount(void*) noexcept override;
    void on_destroy() noexcept override;

    void on_dispose() noexcept override {
        mounted_.clear();
    }

    void synchronize_auxiliary_geometry(runtime::Size, runtime::Rect) override;

private:
    void update(runtime::ComponentId, bool geometry);
    WindowComponentServices* services_;
    std::vector<runtime::ComponentId> mounted_;
};
} // namespace ryn::detail
