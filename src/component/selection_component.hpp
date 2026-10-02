#pragma once

#include "component/window_component_services.hpp"
#include "input/pressable_behavior.hpp"

#include <ryn/checkbox.hpp>
#include <ryn/radio.hpp>
#include <ryn/switch.hpp>

namespace ryn::detail {

struct SelectionState;
struct RadioGroupState;
struct CheckboxGroupState;

struct MountedSelectionComponent final {
    runtime::ComponentId component;
    runtime::NodeId node;
    input::InteractionId interaction;
    component::RetainedSurfaceId surface;
    bool checkbox{};
    bool radio{};
};

struct SelectionSnapshot final {
    bool checkbox{};
    bool checked{};
    bool indeterminate{};
    bool disabled{};
    bool loading{};
    bool hovered{};
    bool pointer_pressed{};
    input::FocusPresentation focus;
    SwitchSize size{SwitchSize::Middle};
    bool radio{};
    bool wave_active{};
    float wave_progress{1};
    component::RetainedSurfaceId wave_range;
};

struct MountedCheckboxGroup final {
    runtime::ComponentId component;
    runtime::NodeId node;
    input::InteractionId interaction;
};

// Selection controls borrow one window's retained resources and share
// pointer/focus lifecycle. Their checked and visual policies stay private.
class SelectionComponentHost final : private WindowComponentParticipant, private animation::AnimationTargetSink {
public:
    explicit SelectionComponentHost(WindowComponentServices& services);
    ~SelectionComponentHost();
    SelectionComponentHost(const SelectionComponentHost&) = delete;
    SelectionComponentHost& operator=(const SelectionComponentHost&) = delete;

    void mount(const Content& content);

    [[nodiscard]] std::span<const MountedSelectionComponent> mounted() const noexcept {
        return mounted_;
    }

    [[nodiscard]] SelectionSnapshot snapshot(runtime::ComponentId component) const;

    [[nodiscard]] std::span<const MountedCheckboxGroup> checkbox_groups() const noexcept {
        return checkbox_groups_;
    }

    [[nodiscard]] CheckboxValues checkbox_group_value(runtime::ComponentId component) const;

    [[nodiscard]] std::span<const runtime::ComponentId> radio_groups() const noexcept {
        return groups_;
    }

    [[nodiscard]] RadioSelection radio_group_value(runtime::ComponentId component) const;

private:
    friend struct SwitchPropsAccess;
    friend struct CheckboxPropsAccess;
    friend struct RadioPropsAccess;
    friend struct RadioGroupPropsAccess;
    friend struct CheckboxGroupPropsAccess;
    void* begin_mount() noexcept override;
    void end_mount(void* previous) noexcept override;
    void on_destroy() noexcept override;
    void on_dispose() noexcept override;
    void synchronize_auxiliary_geometry(runtime::Size, runtime::Rect) override;
    void synchronize_auxiliary_motion() override;
    void on_window_active(bool) override;
    void position_window_layers(runtime::Size, runtime::Rect) override;

    [[nodiscard]] SelectionState* find(runtime::ComponentId) noexcept;
    [[nodiscard]] const SelectionState* find(runtime::ComponentId) const noexcept;
    [[nodiscard]] std::optional<input::InteractionId> parent_interaction(runtime::ComponentId) const;
    void attach_interaction(SelectionState&);
    void release_selection(SelectionState&);
    void connect_state(SelectionState&);
    void update_visuals(SelectionState&);
    void update_geometry(SelectionState&, runtime::Size);
    void update_layout(SelectionState&);
    void place_switch_content(SelectionState&, layout::LayoutEngine&, runtime::Rect);
    void synchronize_switch_content(SelectionState&);
    void apply_direction(runtime::ComponentId, SwitchDirection);
    void apply_wave(runtime::ComponentId, bool);
    void start_wave(SelectionState&);
    void stop_wave(SelectionState&);
    void publish_wave(SelectionState&);
    void apply_checked(runtime::ComponentId, bool);
    void apply_radio_own_disabled(runtime::ComponentId, bool);
    void apply_checkbox_own_disabled(runtime::ComponentId, bool);
    void apply_checkbox_group_value(runtime::ComponentId, CheckboxValues);
    void apply_checkbox_group_disabled(runtime::ComponentId, bool);
    void apply_checkbox_group_orientation(runtime::ComponentId, CheckboxGroupOrientation);
    void apply_checkbox_group_direction(runtime::ComponentId, CheckboxDirection);
    void apply_checkbox_options(runtime::ComponentId, std::vector<CheckboxOption>);
    void update_checkbox_group_layout(CheckboxGroupState&);
    [[nodiscard]] runtime::ComponentId parent_checkbox_group(runtime::ComponentId) const;
    [[nodiscard]] runtime::ComponentId parent_radio_group(runtime::ComponentId) const;
    void apply_group_value(runtime::ComponentId, RadioSelection);
    void apply_group_disabled(runtime::ComponentId, bool);
    void apply_group_orientation(runtime::ComponentId, RadioGroupOrientation);
    void apply_group_direction(runtime::ComponentId, RadioDirection);
    void apply_radio_options(runtime::ComponentId, std::vector<RadioOption>);
    void update_radio_tab_stops(RadioGroupState&);
    bool handle_radio_key(runtime::ComponentId, const input::KeyboardInputEvent&);
    void update_group_layout(RadioGroupState&);
    void retarget_handle(SelectionState&);
    void synchronize_spinner(SelectionState&);
    void apply(animation::AnimationId, animation::AnimationTargetId, const animation::AnimationValue&,
               animation::AnimationDirtyDomain) override;
    void completed(animation::AnimationId, animation::AnimationTargetId) override;
    void apply_disabled(runtime::ComponentId, bool);
    void apply_loading(runtime::ComponentId, bool);
    void apply_indeterminate(runtime::ComponentId, bool);
    void apply_size(runtime::ComponentId, SwitchSize);
    void apply_focus(runtime::ComponentId, input::FocusPresentation);
    void handle_pointer(runtime::ComponentId, input::PointerDispatchContext&);
    [[nodiscard]] bool activation_allowed(runtime::ComponentId) const noexcept;
    void activate(runtime::ComponentId);

    WindowComponentServices* services_;
    std::vector<MountedSelectionComponent> mounted_;
    std::vector<runtime::ComponentId> groups_;
    std::vector<MountedCheckboxGroup> checkbox_groups_;
    runtime::Size viewport_{};
    std::optional<runtime::Rect> window_clip_;
};

} // namespace ryn::detail
