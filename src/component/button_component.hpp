#pragma once

#include "animation/motion_policy.hpp"
#include "component/button_scene_service.hpp"
#include "component/window_component_services.hpp"
#include "component/default_theme.hpp"
#include "component/text_component.hpp"
#include "input/focus_manager.hpp"
#include "input/pointer_router.hpp"
#include "input/pressable_behavior.hpp"
#include "layout/layout_engine.hpp"
#include "runtime/invalidation.hpp"
#include "text/text_scene_service.hpp"

#include <ryn/button.hpp>

#include <cstdint>
#include <memory>
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
    float spinner_phase{0.0F};
    bool spinner_running{false};
    ButtonColor color{ButtonColor::Default};
    ButtonVariant variant{ButtonVariant::Outlined};
    bool ghost{};
    std::size_t dashed_effects{};
    ButtonShape shape{ButtonShape::Default};
    ButtonIconPlacement icon_placement{ButtonIconPlacement::Start};
    bool block{};
    bool loading_pending{};
    runtime::ComponentId icon;
    runtime::ComponentId loading_icon;
    bool wave_active{};
    float wave_progress{1};
    float wave_fade{1};
    bool compact{};
    std::array<bool, 4> compact_corners{true, true, true, true};
};

enum class ButtonAnimationChannel : std::uint8_t {
    background,
    border,
    foreground,
    loading_mix,
    spinner_phase,
    wave_progress,
    wave_fade,
};

inline constexpr std::size_t button_animation_channel_count =
    static_cast<std::size_t>(ButtonAnimationChannel::wave_fade) + 1;

struct ButtonAnimationBinding final {
    animation::AnimationTargetId target;
    runtime::ComponentId component;
    ButtonAnimationChannel channel{ButtonAnimationChannel::background};
};

class ButtonComponentHost final : private animation::AnimationTargetSink,
                                  public runtime::FrameDeadlineSource,
                                  private WindowComponentParticipant {
public:
    ButtonComponentHost(runtime::NodeStore& nodes, layout::LayoutEngine& layout, runtime::DirtyQueues& dirty,
                        TextSceneService& text_scene, std::vector<font::FontIdentity> default_font_chain,
                        runtime::FrameRequestState& frame_requests);
    ButtonComponentHost(runtime::NodeStore& nodes, layout::LayoutEngine& layout, runtime::DirtyQueues& dirty,
                        TextSceneService& text_scene, ThemeFontResolver font_resolver,
                        runtime::FrameRequestState& frame_requests);
    explicit ButtonComponentHost(WindowComponentServices& services);
    ButtonComponentHost(const ButtonComponentHost&) = delete;
    ButtonComponentHost& operator=(const ButtonComponentHost&) = delete;
    ~ButtonComponentHost();

    void mount(const Content& content);
    bool destroy(runtime::ComponentId id);
    void dispose() noexcept;
    void set_window_active(bool active);
    void set_animation_time(animation::AnimationTime time) noexcept;

    [[nodiscard]] animation::AnimationTime animation_time() const noexcept {
        return services_->animation_time();
    }

    [[nodiscard]] animation::MotionPreference motion_preference() const noexcept {
        return services_->motion_preference();
    }

    void set_motion_preference(animation::MotionPreference preference);
    [[nodiscard]] std::size_t tick_animations(animation::AnimationTime frame_time);
    [[nodiscard]] std::optional<animation::AnimationTime> next_deadline() const override;
    [[nodiscard]] bool layout_and_synchronize(runtime::Size viewport, runtime::Rect clip, runtime::Point origin = {},
                                              float gap = 0.0F, bool unbounded_root_height = false);

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
    [[nodiscard]] runtime::NodeStore& nodes() noexcept;
    [[nodiscard]] layout::LayoutEngine& layout() noexcept;
    [[nodiscard]] runtime::DirtyQueues& dirty() noexcept;

    [[nodiscard]] WindowComponentServices& services() noexcept {
        return *services_;
    }

    void attach_auxiliary(AuxiliaryComponentSynchronizer& auxiliary);
    void detach_auxiliary(AuxiliaryComponentSynchronizer& auxiliary) noexcept;
    [[nodiscard]] animation::AnimationRuntime& animations() noexcept;
    [[nodiscard]] const animation::AnimationRuntime& animations() const noexcept;
    [[nodiscard]] std::span<const MountedButtonComponent> mounted_buttons() const noexcept;
    [[nodiscard]] ButtonComponentSnapshot snapshot(runtime::ComponentId component) const;

private:
    explicit ButtonComponentHost(std::unique_ptr<WindowComponentServices> services);
    friend void mount_button_component(const ButtonProps& props, const ButtonSlots& slots);

    void record_mounted_button(MountedButtonComponent mounted);
    [[nodiscard]] ButtonComponentState* find_state(runtime::ComponentId component) noexcept;
    [[nodiscard]] const ButtonComponentState* find_state(runtime::ComponentId component) const noexcept;
    [[nodiscard]] std::optional<input::InteractionId> interaction_for(runtime::ComponentId component) const;
    void apply_type(runtime::ComponentId component, ButtonType type);
    void apply_color(runtime::ComponentId component, ButtonColor color);
    void apply_variant(runtime::ComponentId component, ButtonVariant variant);
    void apply_danger(runtime::ComponentId component, bool danger);
    void apply_ghost(runtime::ComponentId component, bool ghost);
    void update_variant(ButtonComponentState& state, bool previous_border_box, bool previous_dashed);
    void update_decoration_material(ButtonComponentState& state);
    void synchronize_decorations(ButtonComponentState& state, runtime::Rect clip);
    void synchronize_compact(ButtonComponentState& state);
    void apply_size(runtime::ComponentId component, ControlSize size);
    void apply_disabled(runtime::ComponentId component, bool disabled);
    void apply_loading(runtime::ComponentId component, bool loading);
    void apply_loading_delay(runtime::ComponentId component, Duration delay);
    void set_loading(ButtonComponentState& state, bool loading);
    void apply_shape(runtime::ComponentId component, ButtonShape shape);
    void apply_block(runtime::ComponentId component, bool block);
    void apply_icon_placement(runtime::ComponentId component, ButtonIconPlacement placement);
    void update_icon_branch(ButtonComponentState& state);
    void apply_wave(runtime::ComponentId component, bool wave);
    void start_wave(ButtonComponentState& state);
    void stop_wave(ButtonComponentState& state);
    void publish_wave(ButtonComponentState& state);
    void apply_focus(runtime::ComponentId component, input::FocusPresentation focus);
    void handle_pointer(runtime::ComponentId component, input::PointerDispatchContext& event);
    [[nodiscard]] bool activation_allowed(runtime::ComponentId component) const noexcept;
    void activate(runtime::ComponentId component);
    void update_visuals(ButtonComponentState& state);
    void apply_presentation(ButtonComponentState& state, bool animation_update = false);
    void register_animation_targets(ButtonComponentState& state);
    void unregister_animation_targets(ButtonComponentState& state) noexcept;
    void retarget_channel(ButtonComponentState& state, ButtonAnimationChannel channel,
                          const animation::AnimationValue& target, const animation::AnimationSpec& spec);
    void update_spinner(ButtonComponentState& state, const animation::MotionPolicy& policy);
    void start_spinner(ButtonComponentState& state);
    void stop_spinner(ButtonComponentState& state);
    void apply(animation::AnimationId animation, animation::AnimationTargetId target,
               const animation::AnimationValue& value, animation::AnimationDirtyDomain dirty_domain) override;
    void completed(animation::AnimationId animation, animation::AnimationTargetId target) override;
    void update_typography(ButtonComponentState& state);
    void update_layout(ButtonComponentState& state);
    void subscribe_theme(ButtonComponentState& state);
    void synchronize_geometry(ButtonComponentState& state, runtime::Size viewport, runtime::Rect clip);
    void* begin_mount() noexcept override;
    void end_mount(void* previous) noexcept override;
    void on_destroy() noexcept override;
    void on_dispose() noexcept override;
    void synchronize_auxiliary_geometry(runtime::Size viewport, runtime::Rect clip) override;
    void synchronize_auxiliary_motion() override;
    void on_window_active(bool active) override;
    std::size_t tick_auxiliary(animation::AnimationTime time) override;
    std::optional<animation::AnimationTime> next_auxiliary_deadline() const override;

    std::unique_ptr<WindowComponentServices> owned_services_;
    WindowComponentServices* services_;
    runtime::NodeStore* nodes_;
    layout::LayoutEngine* layout_;
    runtime::DirtyQueues* dirty_;
    TextComponentHost& text_;
    input::InteractionRegistry& interactions_;
    input::HitTestSnapshot& hit_test_;
    component::ComponentSceneComposer& scene_composer_;
    component::ButtonSceneService& button_scene_;
    input::FocusManager& focus_;
    input::PointerRouter& pointer_;
    animation::AnimationRuntime& animations_;
    std::vector<ButtonAnimationBinding> animation_bindings_;
    std::vector<MountedButtonComponent> mounted_buttons_;
};

void mount_button_component(const ButtonProps& props, const ButtonSlots& slots);

} // namespace ryn::detail
