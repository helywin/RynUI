#pragma once
#include "component/window_component_services.hpp"
#include <ryn/carousel.hpp>

namespace ryn::detail {
struct CarouselSnapshot final {
    std::optional<std::size_t> current;
    String key;
    std::vector<runtime::ComponentId> slides;
    std::vector<runtime::ComponentId> dots;
    std::array<runtime::ComponentId, 2> arrows;
    runtime::Rect viewport;
    float progress{1};
    float autoplay_progress{};
    bool transitioning{};
    bool dragging{};
    bool paused{};
};

class CarouselComponentHost final : public WindowComponentParticipant {
public:
    explicit CarouselComponentHost(WindowComponentServices&);
    ~CarouselComponentHost();
    void mount(const CarouselProps&);

    std::span<const runtime::ComponentId> mounted() const {
        return mounted_;
    }

    CarouselSnapshot snapshot(runtime::ComponentId) const;
    // Refresh only actual Carousel slide ancestors before consuming target input admission.
    // False means callbacks superseded this exact producer path; callers must stop old work.
    bool refresh_target_admission(runtime::ComponentId target, runtime::Size, runtime::Rect document_clip);
    void* begin_mount() noexcept override;
    void end_mount(void*) noexcept override;
    void on_destroy() noexcept override;

    void on_dispose() noexcept override {
        mounted_.clear();
    }

    bool on_keyboard_input(const input::KeyboardInputEvent&) override;
    void on_pointer_input(const input::PointerInputEvent&, std::optional<input::InteractionId>,
                          std::optional<input::InteractionId>) override;
    void on_window_active(bool) override;
    void on_focus_changed();
    void on_input_boundary_changed() override;
    void synchronize_auxiliary_motion() override;
    void prepare_auxiliary_layout() override;
    std::size_t tick_auxiliary(animation::AnimationTime) override;
    std::optional<animation::AnimationTime> next_auxiliary_deadline() const override;
    void synchronize_auxiliary_geometry(runtime::Size, runtime::Rect) override;

private:
    bool request(runtime::ComponentId, std::size_t, CarouselChangeSource, bool = false, int = 0);
    bool accept(runtime::ComponentId, std::size_t, bool = false, int = 0);
    bool step(runtime::ComponentId, int, CarouselChangeSource);
    bool playback(runtime::ComponentId, bool);
    bool focus(runtime::ComponentId);
    void finish(runtime::ComponentId, bool = true);
    void cancel_gesture(runtime::ComponentId);
    bool paused(runtime::ComponentId) const;
    void schedule(runtime::ComponentId, bool = false);
    void gates(runtime::ComponentId);
    void refresh_clip(runtime::ComponentId, runtime::Size, runtime::Rect);
    void refresh(runtime::ComponentId, bool geometry = true);
    void items(runtime::ComponentId, runtime::ComponentBuildContext* = nullptr);
    runtime::ComponentId slide(runtime::ComponentId, const CarouselSlide&, runtime::ComponentBuildContext*);
    runtime::ComponentId dot(runtime::ComponentId, String, runtime::ComponentBuildContext*);
    runtime::Size measure(runtime::ComponentId, layout::LayoutEngine&, layout::Constraints);
    void place(runtime::ComponentId, layout::LayoutEngine&, runtime::Rect);
    WindowComponentServices* services_;
    std::vector<runtime::ComponentId> mounted_;
};
} // namespace ryn::detail
