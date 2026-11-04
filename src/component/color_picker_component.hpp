#pragma once
#include "component/window_component_services.hpp"
#include <ryn/color_picker.hpp>

namespace ryn::detail {
struct ColorPickerSnapshot final {
    ColorPickerValue value;
    ColorFormat format{};
    bool open{};
    bool dragging{};
    runtime::ComponentId panel;
    std::vector<runtime::ComponentId> fields;
    std::vector<input::InteractionId> interactions;
};

class ColorPickerComponentHost final : public WindowComponentParticipant {
public:
    explicit ColorPickerComponentHost(WindowComponentServices&);
    ~ColorPickerComponentHost();
    void mount(const ColorPickerProps&, ColorPickerSlots);
    void* begin_mount() noexcept override;
    void end_mount(void*) noexcept override;
    void on_destroy() noexcept override;

    void on_dispose() noexcept override {
        mounted_.clear();
    }

    void prepare_auxiliary_layout() override;
    void synchronize_auxiliary_geometry(runtime::Size, runtime::Rect) override;
    void on_window_active(bool) override;
    void on_input_boundary_changed() override;

    std::span<const runtime::ComponentId> mounted() const {
        return mounted_;
    }

    ColorPickerSnapshot snapshot(runtime::ComponentId) const;

private:
    void refresh(runtime::ComponentId);
    void publish(runtime::ComponentId, ColorPickerValue, bool complete);
    void complete(runtime::ComponentId);
    void cancel(runtime::ComponentId);
    void mount_picker(runtime::ComponentId);
    void mount_presets(runtime::ComponentId);
    void reconcile_presets(runtime::ComponentId);
    void mount_preset_group(runtime::ComponentId, ColorPreset);
    void paint(runtime::ComponentId, runtime::Rect);
    void pointer(runtime::ComponentId, std::size_t, input::PointerDispatchContext&);
    bool keyboard(runtime::ComponentId, std::size_t, const input::KeyboardInputEvent&);
    void channel(runtime::ComponentId, std::size_t, double, bool);
    void move_stop(runtime::ComponentId, double, bool);
    WindowComponentServices* services_;
    std::vector<runtime::ComponentId> mounted_;
};
} // namespace ryn::detail
