#pragma once

#include "component/window_component_services.hpp"
#include <ryn/splitter.hpp>

namespace ryn::detail {

struct SplitterBarSnapshot final {
    runtime::ComponentId component;
    runtime::ComponentId start;
    runtime::ComponentId end;
    input::InteractionId interaction;
    input::InteractionId start_interaction;
    input::InteractionId end_interaction;
    bool resizable{};
    bool start_enabled{};
    bool end_enabled{};
};

struct SplitterPanelSnapshot final {
    runtime::ComponentId component;
    runtime::ComponentId content;
    bool collapsed{};
    bool destroy_on_hidden{};
};

struct SplitterSnapshot final {
    runtime::NodeId node;
    component::RetainedSurfaceId preview_surface;
    SplitterOrientation orientation{};
    SplitterSizes sizes;
    SplitterSizes visual_sizes;
    SplitterSizes preview_sizes;
    std::vector<SplitterPanelSnapshot> panels;
    std::vector<SplitterBarSnapshot> bars;
    bool lazy{};
    bool dragging{};
    bool animating{};
};

class SplitterComponentHost final : public WindowComponentParticipant {
public:
    explicit SplitterComponentHost(WindowComponentServices&);
    ~SplitterComponentHost();
    void mount(const SplitterProps&, const std::vector<SplitterPanel>&);

    [[nodiscard]] std::span<const runtime::ComponentId> mounted() const {
        return mounted_;
    }

    [[nodiscard]] SplitterSnapshot snapshot(runtime::ComponentId) const;
    void* begin_mount() noexcept override;
    void end_mount(void*) noexcept override;
    void on_destroy() noexcept override;

    void on_dispose() noexcept override {
        mounted_.clear();
    }

    void on_window_active(bool) override;
    void on_input_boundary_changed() override;
    void prepare_auxiliary_layout() override;
    void synchronize_auxiliary_geometry(runtime::Size, runtime::Rect) override;
    std::size_t tick_auxiliary(animation::AnimationTime) override;
    std::optional<animation::AnimationTime> next_auxiliary_deadline() const override;

private:
    void mount_content(runtime::ComponentId, std::size_t, runtime::ComponentBuildContext* initial = nullptr);
    void synchronize_content(runtime::ComponentId, bool allow_mount = true);
    void refresh(runtime::ComponentId, bool geometry = true);
    void resolve(runtime::ComponentId, float, bool animate = false);
    void cancel(runtime::ComponentId);
    void pointer(runtime::ComponentId, std::size_t, input::PointerDispatchContext&);
    bool keyboard(runtime::ComponentId, std::size_t, const input::KeyboardInputEvent&);
    void collapse(runtime::ComponentId, std::size_t, bool);
    void commit(runtime::ComponentId, std::size_t, SplitterSizes, bool motion);
    void emit(runtime::ComponentId, const SplitterSizes&, bool change, bool end, bool collapse);
    runtime::Size measure(runtime::ComponentId, layout::LayoutEngine&, layout::Constraints);
    void place(runtime::ComponentId, layout::LayoutEngine&, runtime::Rect);
    WindowComponentServices* services_;
    std::vector<runtime::ComponentId> mounted_;
};

} // namespace ryn::detail
