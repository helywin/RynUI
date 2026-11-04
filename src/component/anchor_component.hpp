#pragma once
#include "component/window_component_services.hpp"
#include <ryn/anchor.hpp>

namespace ryn::detail {
struct AnchorSnapshot final {
    String active;
    std::vector<String> keys;
    std::vector<runtime::ComponentId> items;
    bool animating{};
    bool affixed{};
};

class AnchorComponentHost final : public WindowComponentParticipant {
public:
    explicit AnchorComponentHost(WindowComponentServices&);
    ~AnchorComponentHost();
    void mount(const AnchorProps&);

    [[nodiscard]] std::span<const runtime::ComponentId> mounted() const {
        return mounted_;
    }

    [[nodiscard]] AnchorSnapshot snapshot(runtime::ComponentId) const;
    void* begin_mount() noexcept override;
    void end_mount(void*) noexcept override;
    void on_destroy() noexcept override;

    void on_dispose() noexcept override {
        mounted_.clear();
    }

    void prepare_auxiliary_layout() override;
    void position_window_layers(runtime::Size, runtime::Rect) override;
    void synchronize_auxiliary_geometry(runtime::Size, runtime::Rect) override;
    bool on_scroll_input(const input::ScrollInputEvent&) override;
    void on_input_boundary_changed() override;
    void on_window_active(bool) override;
    std::size_t tick_auxiliary(animation::AnimationTime) override;
    std::optional<animation::AnimationTime> next_auxiliary_deadline() const override;

private:
    void validate_items(const std::vector<AnchorItem>&) const;
    void reconcile(runtime::ComponentId, runtime::ComponentBuildContext* = nullptr);
    void refresh(runtime::ComponentId, bool);
    void track(runtime::ComponentId);
    void select(runtime::ComponentId, String, bool);
    void activate(runtime::ComponentId, String);
    void request_scroll(runtime::ComponentId, float);
    runtime::Size measure(runtime::ComponentId, layout::LayoutEngine&, layout::Constraints);
    void place(runtime::ComponentId, layout::LayoutEngine&, runtime::Rect);
    WindowComponentServices* services_;
    std::vector<runtime::ComponentId> mounted_;
};
} // namespace ryn::detail
