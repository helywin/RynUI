#pragma once

#include "animation/motion_policy.hpp"
#include "component/button_scene_service.hpp"
#include "component/default_theme.hpp"
#include "component/text_component.hpp"
#include "input/focus_manager.hpp"
#include "input/pointer_router.hpp"
#include "layout/layout_engine.hpp"
#include "runtime/invalidation.hpp"
#include "text/text_scene_service.hpp"

#include <ryn/button.hpp>

#include <cstdint>
#include <optional>
#include <span>
#include <vector>

namespace ryn::detail {

struct ButtonComponentState;

struct MountedButtonComponent final {
    runtime::ComponentId component;
    runtime::NodeId node;
    input::InteractionId interaction;
    component::ButtonSceneId scene;
    runtime::SceneFragmentId fragment;
};

struct ButtonComponentSnapshot final {
    ButtonType type{ButtonType::Default};
    ControlSize size{ControlSize::Middle};
    bool disabled{false};
    bool loading{false};
    bool hovered{false};
    bool pointer_pressed{false};
    input::FocusPresentation focus;
    Color presentation_background;
    Color presentation_border;
    Color presentation_foreground;
    float presentation_loading_mix{0.0F};
};

enum class ButtonAnimationChannel : std::uint8_t {
    background,
    border,
    foreground,
    loading_mix,
};

struct ButtonAnimationBinding final {
    animation::AnimationTargetId target;
    runtime::ComponentId component;
    ButtonAnimationChannel channel{ButtonAnimationChannel::background};
};

class ButtonComponentHost final : private animation::AnimationTargetSink {
public:
    ButtonComponentHost(
        runtime::NodeStore& nodes,
        layout::LayoutEngine& layout,
        runtime::DirtyQueues& dirty,
        TextSceneService& text_scene,
        std::vector<font::FontIdentity> default_font_chain,
        runtime::FrameRequestState& frame_requests);
    ButtonComponentHost(
        runtime::NodeStore& nodes,
        layout::LayoutEngine& layout,
        runtime::DirtyQueues& dirty,
        TextSceneService& text_scene,
        ThemeFontResolver font_resolver,
        runtime::FrameRequestState& frame_requests);
    ButtonComponentHost(const ButtonComponentHost&) = delete;
    ButtonComponentHost& operator=(const ButtonComponentHost&) = delete;
    ~ButtonComponentHost();

    void mount(const Content& content);
    bool destroy(runtime::ComponentId id);
    void dispose() noexcept;
    void set_window_active(bool active);
    void set_animation_time(animation::AnimationTime time) noexcept;
    void set_motion_preference(animation::MotionPreference preference);
    [[nodiscard]] std::size_t tick_animations(animation::AnimationTime frame_time);
    [[nodiscard]] bool layout_and_synchronize(
        runtime::Size viewport,
        runtime::Rect clip,
        runtime::Point origin = {},
        float gap = 0.0F);

    [[nodiscard]] TextComponentHost& text() noexcept;
    [[nodiscard]] const TextComponentHost& text() const noexcept;
    [[nodiscard]] runtime::ComponentHost& components() noexcept;
    [[nodiscard]] const runtime::ComponentHost& components() const noexcept;
    [[nodiscard]] input::InteractionRegistry& interactions() noexcept;
    [[nodiscard]] input::HitTestSnapshot& hit_test() noexcept;
    [[nodiscard]] input::FocusManager& focus() noexcept;
    [[nodiscard]] input::PointerRouter& pointer() noexcept;
    [[nodiscard]] component::ComponentSceneComposer& scene_composer() noexcept;
    [[nodiscard]] component::ButtonSceneService& button_scene() noexcept;
    [[nodiscard]] graphics::RoundedEffectStore& rounded_effects() noexcept;
    [[nodiscard]] animation::AnimationRuntime& animations() noexcept;
    [[nodiscard]] const animation::AnimationRuntime& animations() const noexcept;
    [[nodiscard]] std::span<const MountedButtonComponent>
        mounted_buttons() const noexcept;
    [[nodiscard]] ButtonComponentSnapshot snapshot(
        runtime::ComponentId component) const;

private:
    friend void mount_button_component(
        const ButtonProps& props,
        const ButtonContent& content);

    void record_mounted_button(MountedButtonComponent mounted);
    [[nodiscard]] ButtonComponentState* find_state(
        runtime::ComponentId component) noexcept;
    [[nodiscard]] const ButtonComponentState* find_state(
        runtime::ComponentId component) const noexcept;
    [[nodiscard]] std::optional<input::InteractionId> interaction_for(
        runtime::ComponentId component) const;
    void apply_type(runtime::ComponentId component, ButtonType type);
    void apply_size(runtime::ComponentId component, ControlSize size);
    void apply_disabled(runtime::ComponentId component, bool disabled);
    void apply_loading(runtime::ComponentId component, bool loading);
    void apply_focus(
        runtime::ComponentId component,
        input::FocusPresentation focus);
    void handle_pointer(
        runtime::ComponentId component,
        input::PointerDispatchContext& event);
    [[nodiscard]] bool activation_allowed(
        runtime::ComponentId component) const noexcept;
    void activate(runtime::ComponentId component);
    void update_visuals(ButtonComponentState& state);
    void apply_presentation(
        ButtonComponentState& state,
        bool animation_update = false);
    void register_animation_targets(ButtonComponentState& state);
    void unregister_animation_targets(ButtonComponentState& state) noexcept;
    void retarget_channel(
        ButtonComponentState& state,
        ButtonAnimationChannel channel,
        const animation::AnimationValue& target,
        const animation::AnimationSpec& spec);
    void apply(
        animation::AnimationId animation,
        animation::AnimationTargetId target,
        const animation::AnimationValue& value,
        animation::AnimationDirtyDomain dirty_domain) override;
    void completed(
        animation::AnimationId animation,
        animation::AnimationTargetId target) override;
    void update_typography(ButtonComponentState& state);
    void update_layout(ButtonComponentState& state);
    void subscribe_theme(ButtonComponentState& state);
    void synchronize_geometry(
        ButtonComponentState& state,
        runtime::Size viewport);

    runtime::NodeStore* nodes_;
    layout::LayoutEngine* layout_;
    runtime::DirtyQueues* dirty_;
    TextComponentHost text_;
    input::InteractionRegistry interactions_;
    input::HitTestSnapshot hit_test_;
    component::ComponentSceneComposer scene_composer_;
    component::ButtonSceneService button_scene_;
    input::FocusManager focus_;
    input::PointerRouter pointer_;
    animation::AnimationRuntime animations_;
    animation::AnimationTime animation_time_;
    animation::MotionPreference motion_preference_{
        animation::MotionPreference::normal};
    std::vector<ButtonAnimationBinding> animation_bindings_;
    std::vector<MountedButtonComponent> mounted_buttons_;
    bool scene_structure_dirty_{true};
};

void mount_button_component(
    const ButtonProps& props,
    const ButtonContent& content);

} // namespace ryn::detail
