#pragma once
#include "component/window_component_services.hpp"
#include <ryn/slider.hpp>
#include <array>

namespace ryn::detail {
struct SliderState;
struct SliderThumb;

struct MountedSliderComponent final {
    runtime::ComponentId component;
    runtime::NodeId node;
    std::vector<input::InteractionId> thumbs;
    component::RetainedSurfaceId surface;
    bool range{};
};

struct SliderSnapshot final {
    SliderRange value;
    SliderLimits limits;
    bool range{};
    bool disabled{};
    bool dragging{};
    bool reverse{};
    SliderOrientation orientation{};
    std::vector<runtime::Point> centers;
    std::vector<input::FocusPresentation> focus;
    SliderValues values;
};

class SliderComponentHost final : private WindowComponentParticipant {
public:
    explicit SliderComponentHost(WindowComponentServices&);
    ~SliderComponentHost();

    [[nodiscard]] std::span<const MountedSliderComponent> mounted() const noexcept {
        return mounted_;
    }

    [[nodiscard]] SliderSnapshot snapshot(runtime::ComponentId) const;

private:
    friend struct SliderPropsAccess;
    void* begin_mount() noexcept override;
    void end_mount(void*) noexcept override;
    void on_destroy() noexcept override;

    void on_dispose() noexcept override {
        mounted_.clear();
    }

    void on_window_active(bool) override;
    void synchronize_auxiliary_geometry(runtime::Size, runtime::Rect) override;
    SliderState* find(runtime::ComponentId) noexcept;
    void release(SliderState&);
    void cancel(runtime::ComponentId);
    void update(runtime::ComponentId, bool geometry);
    void place(runtime::ComponentId, layout::LayoutEngine&, runtime::Rect);
    void mount_labels(runtime::ComponentId, runtime::ComponentBuildContext&);
    void synchronize_labels(runtime::ComponentId);
    void update_hints(runtime::ComponentId);
    void mount_thumb(runtime::ComponentId, SliderThumb&, runtime::ComponentBuildContext&);
    void set_values(runtime::ComponentId, SliderValues);
    std::optional<std::size_t> thumb_index(runtime::ComponentId, runtime::ComponentId) const;
    void change(runtime::ComponentId, std::size_t, double);
    void complete(runtime::ComponentId);
    void pointer(runtime::ComponentId, std::optional<std::size_t>, input::PointerDispatchContext&);
    bool keyboard(runtime::ComponentId, std::size_t, const input::KeyboardInputEvent&);
    WindowComponentServices* services_;
    std::vector<MountedSliderComponent> mounted_;
};
} // namespace ryn::detail
