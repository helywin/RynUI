#pragma once
#include "component/window_component_services.hpp"
#include "graphics/image_scene.hpp"
#include <ryn/image.hpp>

namespace ryn::detail {
struct ImageSnapshot final {
    ImageStatus status{};
    ImageError error;
    String alt;
    std::optional<graphics::ImageId> image;
    runtime::ComponentId modal;
    runtime::ComponentId canvas;
    runtime::ComponentId actions;
    input::InteractionId interaction;
    input::InteractionId canvas_interaction;
    runtime::Rect bounds;
    runtime::Rect canvas_bounds;
    ImageTransform transform;
    bool open{};
    bool dragging{};
    std::size_t current{};
    std::size_t total{1};
    std::vector<runtime::ComponentId> thumbnails;
};

class ImageComponentHost final : public WindowComponentParticipant {
public:
    explicit ImageComponentHost(WindowComponentServices&);
    ~ImageComponentHost();
    void mount(const ImageProps&, const ImageSlots&);
    void mount(const PreviewGroupProps&);
    void set_cover_rounding(runtime::ComponentId cover, float radius);
    void* begin_mount() noexcept override;
    void end_mount(void*) noexcept override;
    void on_destroy() noexcept override;
    void on_dispose() noexcept override;
    void on_window_active(bool) override;
    void on_input_boundary_changed() override;
    bool on_keyboard_input(const input::KeyboardInputEvent&) override;
    bool on_scroll_input(const input::ScrollInputEvent&) override;
    void prepare_auxiliary_layout() override;
    void synchronize_auxiliary_geometry(runtime::Size, runtime::Rect) override;

    [[nodiscard]] std::span<const runtime::ComponentId> mounted() const noexcept {
        return mounted_;
    }

    [[nodiscard]] ImageSnapshot snapshot(runtime::ComponentId) const;
    bool request_open(runtime::ComponentId, bool);
    bool action(runtime::ComponentId, ImageAction);

private:
    struct Lifetime;
    void refresh(runtime::ComponentId, bool geometry = true);
    void load(runtime::ComponentId, ImageSource, bool fallback = false);
    void load_preview(runtime::ComponentId);
    void cancel_preview(runtime::ComponentId) noexcept;
    void cancel_load(runtime::ComponentId) noexcept;
    bool complete(runtime::ComponentId, std::uint64_t, ImageDecodeResult, bool fallback);
    void mount_preview(runtime::ComponentId);
    void replace_actions(runtime::ComponentId, std::shared_ptr<const ImageActionsRenderer>);
    void reconcile(runtime::ComponentId, const std::vector<ImagePreviewItem>&);
    void select(runtime::ComponentId, std::size_t, bool request);
    void canvas_pointer(runtime::ComponentId, input::PointerDispatchContext&);
    void publish(runtime::ComponentId, runtime::Size, runtime::Rect);
    runtime::Size measure(runtime::ComponentId, layout::LayoutEngine&, layout::Constraints);
    void place(runtime::ComponentId, layout::LayoutEngine&, runtime::Rect);
    WindowComponentServices* services_;
    std::shared_ptr<Lifetime> lifetime_;
    std::vector<runtime::ComponentId> mounted_;
    bool active_{true};
    std::vector<std::pair<runtime::ComponentId, float>> cover_policies_;
};
} // namespace ryn::detail
