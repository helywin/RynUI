#pragma once

#include "component/window_component_services.hpp"
#include "input/pressable_behavior.hpp"

#include <ryn/checkbox.hpp>
#include <ryn/radio.hpp>
#include <ryn/switch.hpp>

namespace ryn::detail {

struct SelectionState;
struct RadioGroupState;

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
};

// Selection controls borrow one window's retained resources and share
// pointer/focus lifecycle. Their checked and visual policies stay private.
class SelectionComponentHost final : private WindowComponentParticipant,
    private animation::AnimationTargetSink {
public:
    explicit SelectionComponentHost(WindowComponentServices& services);
    ~SelectionComponentHost();
    SelectionComponentHost(const SelectionComponentHost&) = delete;
    SelectionComponentHost& operator=(const SelectionComponentHost&) = delete;

    void mount(const Content& content);
    [[nodiscard]] std::span<const MountedSelectionComponent> mounted() const noexcept { return mounted_; }
    [[nodiscard]] SelectionSnapshot snapshot(runtime::ComponentId component) const;

private:
    friend struct SwitchPropsAccess;
    friend struct CheckboxPropsAccess;
    friend struct RadioPropsAccess;
    friend struct RadioGroupPropsAccess;
    void* begin_mount() noexcept override;
    void end_mount(void* previous) noexcept override;
    void on_destroy() noexcept override;
    void on_dispose() noexcept override;
    void synchronize_auxiliary_geometry(runtime::Size, runtime::Rect) override;
    void synchronize_auxiliary_motion() override;

    [[nodiscard]] SelectionState* find(runtime::ComponentId) noexcept;
    [[nodiscard]] const SelectionState* find(runtime::ComponentId) const noexcept;
    [[nodiscard]] std::optional<input::InteractionId> parent_interaction(runtime::ComponentId) const;
    void attach_interaction(SelectionState&);
    void release_selection(SelectionState&);
    void connect_state(SelectionState&);
    void update_visuals(SelectionState&);
    void update_geometry(SelectionState&, runtime::Size);
    void update_layout(SelectionState&);
    void apply_checked(runtime::ComponentId, bool);
    void apply_radio_own_disabled(runtime::ComponentId, bool);
    void apply_group_value(runtime::ComponentId, std::optional<String>);
    void apply_group_disabled(runtime::ComponentId, bool);
    void apply_group_orientation(runtime::ComponentId, RadioGroupOrientation);
    void select_group_option(runtime::ComponentId, const String&);
    void update_group_layout(RadioGroupState&);
    void retarget_handle(SelectionState&);
    void synchronize_spinner(SelectionState&);
    void apply(animation::AnimationId, animation::AnimationTargetId,
        const animation::AnimationValue&, animation::AnimationDirtyDomain) override;
    void completed(animation::AnimationId,
        animation::AnimationTargetId) override;
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
    runtime::Size viewport_{};
};

} // namespace ryn::detail
