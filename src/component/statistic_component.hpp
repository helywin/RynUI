#pragma once
#include "component/statistic_model.hpp"
#include "component/window_component_services.hpp"

namespace ryn::detail {
struct StatisticSnapshot final {
    runtime::ComponentId title;
    runtime::ComponentId prefix;
    runtime::ComponentId value;
    runtime::ComponentId suffix;
    runtime::ComponentId skeleton;
    runtime::ComponentId content;
    String formatted_value;
    bool loading{};
    bool visible{true};
    bool animating{};
    bool timer_running{};
    Duration timer_value;
};

class StatisticComponentHost final : public WindowComponentParticipant {
public:
    explicit StatisticComponentHost(WindowComponentServices&);
    ~StatisticComponentHost() override;
    void* begin_mount() noexcept override;
    void end_mount(void*) noexcept override;
    void on_destroy() noexcept override;
    void on_dispose() noexcept override;

    void synchronize_auxiliary_geometry(runtime::Size, runtime::Rect) override;
    void synchronize_auxiliary_motion() override;
    void prepare_auxiliary_layout() override;
    void on_window_active(bool) override;
    void on_input_boundary_changed() override;
    std::size_t tick_auxiliary(animation::AnimationTime) override;
    std::optional<animation::AnimationTime> next_auxiliary_deadline() const override;

    void mount(const StatisticProps&);

    [[nodiscard]] std::span<const runtime::ComponentId> mounted() const noexcept {
        return mounted_;
    }

    [[nodiscard]] StatisticSnapshot snapshot(runtime::ComponentId) const;

private:
    bool valid_composition(runtime::ComponentId, bool complete = false);
    void refresh(runtime::ComponentId, bool);
    bool dynamic_allowed(runtime::ComponentId) const;
    void update_dynamic(runtime::ComponentId, animation::AnimationTime, bool notify = false);
    void publish_text(runtime::ComponentId, String);
    template <typename T, typename Apply, typename Equal = std::equal_to<T>>
    void watch(runtime::ComponentId, const Prop<T>&, Apply, Equal = {});
    template <typename T> void connect(runtime::ComponentId, const Prop<T>&, T StatisticModel::*);
    runtime::Size measure(runtime::ComponentId, layout::LayoutEngine&, layout::Constraints);
    void place(runtime::ComponentId, layout::LayoutEngine&, runtime::Rect);
    runtime::Size measure_value(runtime::ComponentId, layout::LayoutEngine&, layout::Constraints);
    void place_value(runtime::ComponentId, layout::LayoutEngine&, runtime::Rect);
    WindowComponentServices* services_;
    std::vector<runtime::ComponentId> mounted_;
    animation::MonotonicTimeCursor clock_;
};
} // namespace ryn::detail
