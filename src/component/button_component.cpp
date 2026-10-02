#include "component/button_component.hpp"

#include "animation/material_transition_channels.hpp"
#include "component/space_compact.hpp"
#include "runtime/layout_style_adapter.hpp"
#include "runtime/prop_connection.hpp"

#include <algorithm>
#include <array>
#include <cmath>
#include <limits>
#include <numbers>
#include <stdexcept>
#include <thread>
#include <utility>

namespace ryn::detail {

struct ButtonRefState final {
    std::thread::id owner{std::this_thread::get_id()};
    bool binding{};
    std::function<bool()> focus;
    std::function<bool()> blur;

    void ensure_owner() const {
        if (owner != std::this_thread::get_id()) {
            throw std::logic_error("ButtonRef requires its owner thread");
        }
    }
};

struct ButtonPropsAccess final {
    [[nodiscard]] static const Prop<ButtonType>& type(const ButtonProps& props) noexcept {
        return props.type_;
    }

    [[nodiscard]] static const Prop<ControlSize>& size(const ButtonProps& props) noexcept {
        return props.size_;
    }

    static bool explicit_size(const ButtonProps& props) noexcept {
        return props.explicit_size_;
    }

    [[nodiscard]] static const std::optional<Prop<ButtonColor>>& color(const ButtonProps& props) noexcept {
        return props.color_;
    }

    [[nodiscard]] static const std::optional<Prop<ButtonVariant>>& variant(const ButtonProps& props) noexcept {
        return props.variant_;
    }

    [[nodiscard]] static const Prop<bool>& danger(const ButtonProps& props) noexcept {
        return props.danger_;
    }

    [[nodiscard]] static const Prop<bool>& ghost(const ButtonProps& props) noexcept {
        return props.ghost_;
    }

    [[nodiscard]] static const Prop<bool>& disabled(const ButtonProps& props) noexcept {
        return props.disabled_;
    }

    [[nodiscard]] static const Prop<bool>& loading(const ButtonProps& props) noexcept {
        return props.loading_;
    }

    static const Prop<Duration>& loading_delay(const ButtonProps& props) noexcept {
        return props.loading_delay_;
    }

    static const Prop<ButtonShape>& shape(const ButtonProps& props) noexcept {
        return props.shape_;
    }

    static const Prop<bool>& block(const ButtonProps& props) noexcept {
        return props.block_;
    }

    static const Prop<bool>& wave(const ButtonProps& props) noexcept {
        return props.wave_;
    }

    static const Prop<ButtonIconPlacement>& icon_placement(const ButtonProps& props) noexcept {
        return props.icon_placement_;
    }

    static std::shared_ptr<ButtonRefState> ref(const ButtonProps& props) noexcept {
        return props.ref_ ? props.ref_->state_ : nullptr;
    }

    static bool auto_focus(const ButtonProps& props) noexcept {
        return props.auto_focus_;
    }

    [[nodiscard]] static const std::function<void()>& on_click(const ButtonProps& props) noexcept {
        return props.on_click_;
    }

    [[nodiscard]] static const LayoutStyle& layout(const ButtonProps& props) noexcept {
        return props.layout_;
    }
};

struct ButtonComponentState final {
    ButtonComponentState(runtime::SemanticForeground initial_foreground, runtime::SemanticTypography initial_typography)
        : foreground(std::move(initial_foreground)), typography(initial_typography) {}

    runtime::ComponentId component;
    runtime::NodeId node;
    input::InteractionId interaction;
    component::ButtonSceneId scene;
    runtime::SceneFragmentId fragment;
    ButtonType type{ButtonType::Default};
    std::optional<ButtonColor> color;
    std::optional<ButtonVariant> variant;
    bool danger{};
    bool ghost{};
    ControlSize size{ControlSize::Middle};
    bool disabled{false};
    bool loading{false};
    bool loading_requested{};
    Duration loading_delay;
    std::optional<animation::AnimationTime> loading_deadline;
    ButtonShape shape{ButtonShape::Default};
    ButtonIconPlacement icon_placement{ButtonIconPlacement::Start};
    bool block{};
    bool wave{true};
    bool wave_active{};
    float wave_progress{1};
    Color wave_color;
    runtime::SceneFragmentId wave_fragment;
    component::RetainedSurfaceId wave_range;
    std::optional<runtime::Rect> wave_clip;
    bool icon_only{};
    bool has_icon{};
    bool has_loading_icon{};
    runtime::ComponentId icon_wrapper;
    runtime::ComponentId icon;
    runtime::ComponentId loading_icon;
    std::shared_ptr<ButtonRefState> ref;
    bool hovered{false};
    input::PressableBehavior press;
    input::FocusPresentation focus;
    Signal<runtime::SemanticForeground> foreground;
    Signal<runtime::SemanticTypography> typography;
    std::function<void()> on_click;
    component::ButtonVisualData visuals;
    component::ButtonEffectData effects;
    Color presentation_background;
    Color presentation_border;
    Color presentation_foreground;
    float presentation_loading_mix{0.0F};
    float spinner_phase{0.0F};
    std::unique_ptr<animation::MaterialTransitionTargets<button_animation_channel_count>> material_targets;
    std::array<animation::AnimationTargetId, button_animation_channel_count> animation_targets;
    std::array<animation::AnimationId, button_animation_channel_count> animations;
    layout::HorizontalContentLayout layout_model;
    theme_runtime::Subscription color_subscription;
    theme_runtime::Subscription effect_subscription;
    theme_runtime::Subscription layout_subscription;
    theme_runtime::Subscription typography_subscription;
    theme_runtime::Subscription motion_subscription;
    runtime::SceneFragmentId decoration_fragment;
    component::RetainedSurfaceId decoration_range;
    std::vector<graphics::RoundedEffectInstance> decorations;
    std::optional<std::array<float, 14>> decoration_geometry;
    std::optional<CompactMetadata> compact;
    std::weak_ptr<CompactContext> compact_context;
    component::RetainedSurfaceId compact_range;
    runtime::SceneFragmentId compact_fragment;
    component::RetainedSurfaceId compact_spinner_range;
    runtime::SceneFragmentId compact_spinner_fragment;
};

namespace {

thread_local ButtonComponentHost* active_button_host = nullptr;

constexpr std::size_t animation_channel_index(ButtonAnimationChannel channel) noexcept {
    return static_cast<std::size_t>(channel);
}

constexpr animation::AnimationDuration spinner_period = animation::AnimationDuration::microseconds(800'000);

float normalized_spinner_phase(float phase) noexcept {
    const float wrapped = phase - std::floor(phase);
    return wrapped < 0.0F ? wrapped + 1.0F : wrapped;
}

float spinner_segment_strength(float phase, std::size_t segment) noexcept {
    const float angle = 2.0F * std::numbers::pi_v<float> *
                        (static_cast<float>(segment) / static_cast<float>(component::button_loading_segment_count) -
                         normalized_spinner_phase(phase));
    const float wave = 0.5F + 0.5F * std::cos(angle);
    return 0.18F + 0.82F * wave * wave;
}

runtime::Rect spinner_segment_bounds(runtime::Rect indicator, std::size_t segment) noexcept {
    const float size = std::min(indicator.width, indicator.height);
    if (size <= 0.0F) {
        return {};
    }
    const float extent = size * 0.22F;
    const float orbit = 0.5F * (size - extent);
    const float angle =
        -0.5F * std::numbers::pi_v<float> + 2.0F * std::numbers::pi_v<float> * static_cast<float>(segment) /
                                                static_cast<float>(component::button_loading_segment_count);
    const float center_x = indicator.x + 0.5F * indicator.width + std::cos(angle) * orbit;
    const float center_y = indicator.y + 0.5F * indicator.height + std::sin(angle) * orbit;
    return {
        center_x - 0.5F * extent,
        center_y - 0.5F * extent,
        extent,
        extent,
    };
}

void validate(ButtonType type) {
    switch (type) {
    case ButtonType::Default:
    case ButtonType::Primary:
    case ButtonType::Danger:
    case ButtonType::Text:
    case ButtonType::Dashed:
    case ButtonType::Link:
        return;
    }
    throw std::invalid_argument("ButtonType value is invalid");
}

void validate(ButtonShape shape) {
    switch (shape) {
    case ButtonShape::Default:
    case ButtonShape::Circle:
    case ButtonShape::Round:
    case ButtonShape::Square:
        return;
    }
    throw std::invalid_argument("ButtonShape value is invalid");
}

void validate(ButtonIconPlacement placement) {
    switch (placement) {
    case ButtonIconPlacement::Start:
    case ButtonIconPlacement::End:
        return;
    }
    throw std::invalid_argument("ButtonIconPlacement value is invalid");
}

void validate(ButtonColor color) {
    if (static_cast<std::size_t>(color) >= button_color_count) {
        throw std::invalid_argument("ButtonColor value is invalid");
    }
}

void validate(ButtonVariant variant) {
    if (static_cast<std::size_t>(variant) > static_cast<std::size_t>(ButtonVariant::Link)) {
        throw std::invalid_argument("ButtonVariant value is invalid");
    }
}

struct ResolvedButtonVariant final {
    ButtonColor color;
    ButtonVariant variant;
};

ResolvedButtonVariant resolved_variant(const ButtonComponentState& state) {
    ResolvedButtonVariant value{ButtonColor::Default, ButtonVariant::Outlined};
    switch (state.type) {
    case ButtonType::Default:
        break;
    case ButtonType::Primary:
        value = {ButtonColor::Primary, ButtonVariant::Solid};
        break;
    case ButtonType::Danger:
        value = {ButtonColor::Danger, ButtonVariant::Solid};
        break;
    case ButtonType::Text:
        value.variant = ButtonVariant::Text;
        break;
    case ButtonType::Dashed:
        value.variant = ButtonVariant::Dashed;
        break;
    case ButtonType::Link:
        value.variant = ButtonVariant::Link;
        break;
    }
    value.color = state.color.value_or(state.danger ? ButtonColor::Danger : value.color);
    value.variant = state.variant.value_or(value.variant);
    if (state.ghost && value.variant == ButtonVariant::Solid) {
        value.variant = ButtonVariant::Outlined;
    }
    return value;
}

bool legacy_variant(const ButtonComponentState& state) noexcept {
    return !state.color && !state.variant && !state.danger && !state.ghost && state.type != ButtonType::Dashed &&
           state.type != ButtonType::Link;
}

bool unbordered(const ButtonComponentState& state) {
    const auto variant = resolved_variant(state).variant;
    return variant == ButtonVariant::Text || variant == ButtonVariant::Link;
}

bool dashed(const ButtonComponentState& state) {
    return resolved_variant(state).variant == ButtonVariant::Dashed;
}

void validate(ControlSize size) {
    switch (size) {
    case ControlSize::Small:
    case ControlSize::Middle:
    case ControlSize::Large:
        return;
    }
    throw std::invalid_argument("ControlSize value is invalid");
}

struct ResolvedButtonSizeToken final {
    float control_height{};
    float padding_inline{};
    float border_radius{};
    float content_font_size{};
    float content_line_height{};
};

ResolvedButtonSizeToken size_token(const ButtonThemeToken& button, ControlSize size) {
    validate(size);
    switch (size) {
    case ControlSize::Small:
        return {
            button.control_height_small,    button.padding_inline_small,      button.border_radius_small,
            button.content_font_size_small, button.content_line_height_small,
        };
    case ControlSize::Middle:
        return {
            button.control_height,    button.padding_inline,      button.border_radius,
            button.content_font_size, button.content_line_height,
        };
    case ControlSize::Large:
        return {
            button.control_height_large,    button.padding_inline_large,      button.border_radius_large,
            button.content_font_size_large, button.content_line_height_large,
        };
    }
    throw std::invalid_argument("ControlSize value is invalid");
}

struct ResolvedButtonVisualState final {
    Color background;
    Color border;
    Color foreground;
    ShadowList shadow;
};

[[nodiscard]] bool solid_fills_border_box(const ButtonComponentState& state) noexcept {
    if (legacy_variant(state)) {
        return !state.disabled && state.type != ButtonType::Default;
    }
    const auto variant = resolved_variant(state).variant;
    return (!state.disabled || unbordered(state)) && variant != ButtonVariant::Outlined &&
           variant != ButtonVariant::Dashed;
}

ResolvedButtonVisualState legacy_visual_token(const ButtonThemeToken& button, const ButtonComponentState& state) {
    const Color transparent = Color::rgba8(0, 0, 0, 0);
    if (state.disabled) {
        if (state.type == ButtonType::Text) {
            return {transparent, transparent, button.disabled_color, {}};
        }
        const auto* shadow = &button.default_shadow;
        if (state.type == ButtonType::Primary) {
            shadow = &button.primary_shadow;
        } else if (state.type == ButtonType::Danger) {
            shadow = &button.danger_shadow;
        }
        return {
            button.disabled_background,
            button.disabled_border_color,
            button.disabled_color,
            *shadow,
        };
    }
    const bool active = !state.loading && (state.press.pressed() || state.focus.keyboard_pressed);
    const bool hovered = !state.loading && !active && state.hovered;
    switch (state.type) {
    case ButtonType::Default:
        return {
            button.default_background,
            active    ? button.default_active_color
            : hovered ? button.default_hover_color
                      : button.default_border_color,
            active    ? button.default_active_color
            : hovered ? button.default_hover_color
                      : button.default_color,
            button.default_shadow,
        };
    case ButtonType::Primary:
        return {
            active    ? button.primary_active_background
            : hovered ? button.primary_hover_background
                      : button.primary_background,
            transparent,
            button.primary_color,
            button.primary_shadow,
        };
    case ButtonType::Danger:
        return {
            active    ? button.danger_active_background
            : hovered ? button.danger_hover_background
                      : button.danger_background,
            transparent,
            button.danger_color,
            button.danger_shadow,
        };
    case ButtonType::Text:
        return {
            active    ? button.text_active_background
            : hovered ? button.text_hover_background
                      : button.text_background,
            transparent,
            active    ? button.text_active_color
            : hovered ? button.text_hover_color
                      : button.text_color,
            {},
        };
    case ButtonType::Dashed:
    case ButtonType::Link:
        break;
    }
    throw std::invalid_argument("ButtonType value is invalid");
}

ResolvedButtonVisualState visual_token(const ButtonThemeToken& button, const ButtonComponentState& state) {
    if (legacy_variant(state)) {
        return legacy_visual_token(button, state);
    }
    const Color transparent = Color::rgba8(0, 0, 0, 0);
    const auto selected = resolved_variant(state);
    const auto& palette = button.variants.colors[static_cast<std::size_t>(selected.color)];
    if (state.disabled) {
        return {unbordered(state) ? transparent : button.disabled_background,
                unbordered(state) ? transparent : button.disabled_border_color,
                button.disabled_color,
                {}};
    }
    const bool active = !state.loading && (state.press.pressed() || state.focus.keyboard_pressed);
    const bool hover = !state.loading && !active && state.hovered;
    const Color color = active ? palette.active : hover ? palette.hover : palette.base;
    const Color light = active ? palette.light_active : hover ? palette.light_hover : palette.light;
    const bool neutral = selected.color == ButtonColor::Default;
    switch (selected.variant) {
    case ButtonVariant::Solid:
        return {neutral ? (active  ? button.variants.default_solid_active_background
                           : hover ? button.variants.default_solid_hover_background
                                   : button.variants.default_solid_background)
                        : color,
                transparent, palette.solid_text, palette.shadow};
    case ButtonVariant::Outlined:
    case ButtonVariant::Dashed: {
        const Color foreground = neutral ? (active  ? button.default_active_color
                                            : hover ? button.default_hover_color
                                                    : button.default_color)
                                         : color;
        const bool ghost = state.ghost;
        return {ghost ? button.variants.ghost_background : button.default_background,
                ghost && neutral && !hover && !active ? button.variants.default_ghost_border_color : color,
                ghost && neutral && !hover && !active ? button.variants.default_ghost_color : foreground,
                ghost ? ShadowList{} : palette.shadow};
    }
    case ButtonVariant::Filled:
        return {state.ghost ? button.variants.ghost_background : light,
                transparent,
                neutral ? button.default_color : color,
                {}};
    case ButtonVariant::Text:
        return {active  ? palette.light_active
                : hover ? palette.light
                        : transparent,
                transparent,
                neutral ? button.text_color : color,
                {}};
    case ButtonVariant::Link:
        return {hover ? button.variants.link_hover_background : transparent,
                transparent,
                neutral ? (active  ? button.variants.link_active_color
                           : hover ? button.variants.link_hover_color
                                   : button.variants.link_color)
                        : color,
                {}};
    }
    throw std::invalid_argument("Button variant is invalid");
}

[[nodiscard]] runtime::SemanticForeground channels(Color color) noexcept {
    return {color.red(), color.green(), color.blue(), color.alpha()};
}

runtime::SemanticForeground content_foreground(const ButtonThemeToken& button, const ButtonComponentState& state) {
    auto foreground = channels(visual_token(button, state).foreground);
    if (state.loading) {
        foreground[3] *= button.loading_opacity;
    }
    return foreground;
}

layout::HorizontalContentLayout content_layout(const ButtonThemeToken& button, const ButtonComponentState& state) {
    const auto& size = size_token(button, state.size);
    const bool builtin_loading = state.loading && !state.has_loading_icon;
    const bool visible_icon = state.loading ? state.has_loading_icon : state.has_icon;
    return {
        size.control_height,
        state.icon_only || state.shape == ButtonShape::Circle ? 0.0F : size.padding_inline,
        unbordered(state) ? 0.0F : button.border_width,
        button.icon_gap,
        builtin_loading,
        button.loading_indicator_size,
        state.icon_placement == ButtonIconPlacement::End,
        state.icon_wrapper.valid() && !visible_icon,
        state.icon_wrapper.valid(),
        state.icon_only || state.shape == ButtonShape::Circle ? size.control_height : 0.0F,
        state.block,
    };
}

float button_radius(const ButtonComponentState& state, runtime::Rect bounds, const ButtonThemeToken& token) {
    switch (state.shape) {
    case ButtonShape::Circle:
    case ButtonShape::Round:
        return 0.5F * std::min(bounds.width, bounds.height);
    case ButtonShape::Square:
        return 0;
    case ButtonShape::Default:
        return size_token(token, state.size).border_radius;
    }
    throw std::invalid_argument("ButtonShape value is invalid");
}

runtime::SemanticTypography content_typography(const ThemeSnapshot& theme, const ButtonComponentState& state) {
    const auto size = size_token(theme.button(), state.size);
    return {
        theme.text().font_family, theme.text().font_weight, false, size.content_font_size, size.content_line_height,
    };
}

bool material_changed(const graphics::QuadInstance& left, const graphics::QuadInstance& right) noexcept {
    return left.color != right.color || left.opacity != right.opacity;
}

bool geometry_changed(const graphics::QuadInstance& left, const graphics::QuadInstance& right) noexcept {
    return left.bounds != right.bounds || left.corner_radius != right.corner_radius ||
           left.translation != right.translation;
}

std::array<float, 4> logical_bounds(runtime::Rect pixels, runtime::Size viewport) {
    if (!std::isfinite(viewport.width) || !std::isfinite(viewport.height) || viewport.width <= 0.0F ||
        viewport.height <= 0.0F) {
        throw std::invalid_argument("Button viewport must be finite and positive");
    }
    return {
        pixels.x,
        pixels.y,
        pixels.width,
        pixels.height,
    };
}

float logical_radius(runtime::Rect bounds, float radius) noexcept {
    return std::clamp(radius, 0.0F, 0.5F * std::min(bounds.width, bounds.height));
}

graphics::QuadInstance make_quad(runtime::Rect bounds, runtime::Size viewport, std::array<float, 4> color,
                                 float opacity, float radius, runtime::Point translation) {
    return {
        logical_bounds(bounds, viewport),
        color,
        opacity,
        radius,
        {
            translation.x,
            translation.y,
        },
    };
}

} // namespace

ButtonComponentHost::ButtonComponentHost(runtime::NodeStore& nodes, layout::LayoutEngine& layout,
                                         runtime::DirtyQueues& dirty, TextSceneService& text_scene,
                                         std::vector<font::FontIdentity> default_font_chain,
                                         runtime::FrameRequestState& frame_requests)
    : ButtonComponentHost(std::make_unique<WindowComponentServices>(nodes, layout, dirty, text_scene,
                                                                    std::move(default_font_chain), frame_requests)) {}

ButtonComponentHost::ButtonComponentHost(runtime::NodeStore& nodes, layout::LayoutEngine& layout,
                                         runtime::DirtyQueues& dirty, TextSceneService& text_scene,
                                         ThemeFontResolver font_resolver, runtime::FrameRequestState& frame_requests)
    : ButtonComponentHost(std::make_unique<WindowComponentServices>(nodes, layout, dirty, text_scene,
                                                                    std::move(font_resolver), frame_requests)) {}

ButtonComponentHost::ButtonComponentHost(std::unique_ptr<WindowComponentServices> services)
    : owned_services_(std::move(services)), services_(owned_services_.get()), nodes_(&services_->nodes()),
      layout_(&services_->layout()), dirty_(&services_->dirty()), text_(services_->text()),
      interactions_(services_->interactions()), hit_test_(services_->hit_test()),
      scene_composer_(services_->scene_composer()), button_scene_(services_->surfaces()), focus_(services_->focus()),
      pointer_(services_->pointer()), animations_(services_->animations()) {
    animation_bindings_.reserve(256);
    services_->attach(*this);
}

ButtonComponentHost::ButtonComponentHost(WindowComponentServices& services)
    : services_(&services), nodes_(&services.nodes()), layout_(&services.layout()), dirty_(&services.dirty()),
      text_(services.text()), interactions_(services.interactions()), hit_test_(services.hit_test()),
      scene_composer_(services.scene_composer()), button_scene_(services.surfaces()), focus_(services.focus()),
      pointer_(services.pointer()), animations_(services.animations()) {
    animation_bindings_.reserve(256);
    services_->attach(*this);
}

ButtonComponentHost::~ButtonComponentHost() {
    dispose();
    services_->detach(*this);
}

void ButtonComponentHost::mount(const Content& content) {
    services_->mount(content);
}

bool ButtonComponentHost::destroy(runtime::ComponentId id) {
    return services_->destroy(id);
}

void ButtonComponentHost::dispose() noexcept {
    services_->dispose();
}

void* ButtonComponentHost::begin_mount() noexcept {
    return std::exchange(active_button_host, this);
}

void ButtonComponentHost::end_mount(void* previous) noexcept {
    active_button_host = static_cast<ButtonComponentHost*>(previous);
}

void ButtonComponentHost::on_destroy() noexcept {
    std::erase_if(mounted_buttons_, [this](const auto& mounted) { return !components().contains(mounted.component); });
}

void ButtonComponentHost::on_dispose() noexcept {
    animation_bindings_.clear();
    mounted_buttons_.clear();
}

void ButtonComponentHost::set_window_active(bool active) {
    services_->set_window_active(active);
}

void ButtonComponentHost::set_animation_time(animation::AnimationTime time) noexcept {
    services_->set_animation_time(time);
}

void ButtonComponentHost::set_motion_preference(animation::MotionPreference preference) {
    services_->set_motion_preference(preference);
}

void ButtonComponentHost::synchronize_auxiliary_motion() {
    for (const auto& mounted : mounted_buttons_) {
        if (auto* state = find_state(mounted.component)) {
            update_visuals(*state);
        }
    }
}

std::size_t ButtonComponentHost::tick_animations(animation::AnimationTime frame_time) {
    return services_->tick_animations(frame_time);
}

std::optional<animation::AnimationTime> ButtonComponentHost::next_deadline() const {
    return services_->next_frame_deadline();
}

bool ButtonComponentHost::layout_and_synchronize(runtime::Size viewport, runtime::Rect clip, runtime::Point origin,
                                                 float gap, bool unbounded_root_height) {
    return services_->layout_and_synchronize(viewport, clip, origin, gap, unbounded_root_height);
}

void ButtonComponentHost::synchronize_auxiliary_geometry(runtime::Size viewport, runtime::Rect clip) {
    for (const auto& mounted : mounted_buttons_) {
        if (auto* state = find_state(mounted.component)) {
            if (!components().branch_active(state->component)) {
                stop_spinner(*state);
                stop_wave(*state);
                continue;
            }
            update_spinner(*state,
                           animation::resolve_motion_policy(components().theme_scope(state->component)->snapshot(),
                                                            services_->motion_preference()));
            synchronize_geometry(*state, viewport, clip);
        }
    }
}

TextComponentHost& ButtonComponentHost::text() noexcept {
    return text_;
}

const TextComponentHost& ButtonComponentHost::text() const noexcept {
    return text_;
}

runtime::ComponentHost& ButtonComponentHost::components() noexcept {
    return text_.components();
}

const runtime::ComponentHost& ButtonComponentHost::components() const noexcept {
    return text_.components();
}

input::InteractionRegistry& ButtonComponentHost::interactions() noexcept {
    return interactions_;
}

input::HitTestSnapshot& ButtonComponentHost::hit_test() noexcept {
    return hit_test_;
}

input::FocusManager& ButtonComponentHost::focus() noexcept {
    return focus_;
}

input::PointerRouter& ButtonComponentHost::pointer() noexcept {
    return pointer_;
}

component::ComponentSceneComposer& ButtonComponentHost::scene_composer() noexcept {
    return scene_composer_;
}

component::ButtonSceneService& ButtonComponentHost::button_scene() noexcept {
    return button_scene_;
}

graphics::RoundedEffectStore& ButtonComponentHost::rounded_effects() noexcept {
    return button_scene_.effects();
}

runtime::NodeStore& ButtonComponentHost::nodes() noexcept {
    return *nodes_;
}

layout::LayoutEngine& ButtonComponentHost::layout() noexcept {
    return *layout_;
}

runtime::DirtyQueues& ButtonComponentHost::dirty() noexcept {
    return *dirty_;
}

void ButtonComponentHost::attach_auxiliary(AuxiliaryComponentSynchronizer& auxiliary) {
    services_->attach(auxiliary);
}

void ButtonComponentHost::detach_auxiliary(AuxiliaryComponentSynchronizer& auxiliary) noexcept {
    services_->detach(auxiliary);
}

animation::AnimationRuntime& ButtonComponentHost::animations() noexcept {
    return animations_;
}

const animation::AnimationRuntime& ButtonComponentHost::animations() const noexcept {
    return animations_;
}

std::span<const MountedButtonComponent> ButtonComponentHost::mounted_buttons() const noexcept {
    return mounted_buttons_;
}

ButtonComponentSnapshot ButtonComponentHost::snapshot(runtime::ComponentId component) const {
    const auto* state = find_state(component);
    if (state == nullptr) {
        throw std::out_of_range("Button component is stale or invalid");
    }
    return {
        state->type,
        state->size,
        state->disabled,
        state->loading,
        state->hovered,
        state->press.pressed(),
        state->focus,
        state->presentation_background,
        state->presentation_border,
        state->presentation_foreground,
        state->presentation_loading_mix,
        state->spinner_phase,
        animations_.contains(state->animations[animation_channel_index(ButtonAnimationChannel::spinner_phase)]),
        resolved_variant(*state).color,
        resolved_variant(*state).variant,
        state->ghost,
        state->decorations.size(),
        state->shape,
        state->icon_placement,
        state->block,
        state->loading_deadline.has_value(),
        state->icon,
        state->loading_icon,
        state->wave_active,
        state->wave_progress,
        state->compact.has_value(),
        state->compact ? state->compact->corners : std::array{true, true, true, true},
    };
}

void ButtonComponentHost::record_mounted_button(MountedButtonComponent mounted) {
    mounted_buttons_.push_back(std::move(mounted));
    services_->mark_scene_structure_dirty();
}

ButtonComponentState* ButtonComponentHost::find_state(runtime::ComponentId component) noexcept {
    return components().state<ButtonComponentState>(component);
}

const ButtonComponentState* ButtonComponentHost::find_state(runtime::ComponentId component) const noexcept {
    return components().state<ButtonComponentState>(component);
}

std::optional<input::InteractionId> ButtonComponentHost::interaction_for(runtime::ComponentId component) const {
    auto current = std::optional<runtime::ComponentId>{component};
    while (current.has_value()) {
        if (const auto* state = find_state(*current)) {
            if (interactions_.contains(state->interaction)) {
                return state->interaction;
            }
        }
        current = components().parent(*current);
    }
    return std::nullopt;
}

void ButtonComponentHost::apply_type(runtime::ComponentId component, ButtonType type) {
    validate(type);
    auto* state = find_state(component);
    if (state == nullptr || state->type == type) {
        return;
    }
    const bool previous_border_box = solid_fills_border_box(*state);
    const bool previous_dashed = dashed(*state);
    state->type = type;
    update_variant(*state, previous_border_box, previous_dashed);
}

void ButtonComponentHost::update_variant(ButtonComponentState& state, bool previous_border_box, bool previous_dashed) {
    update_layout(state);
    if (previous_border_box != solid_fills_border_box(state) || previous_dashed != dashed(state)) {
        state.decoration_geometry.reset();
        dirty_->invalidate(state.node, runtime::DirtyFlags::Geometry);
    }
    update_visuals(state);
}

void ButtonComponentHost::apply_color(runtime::ComponentId component, ButtonColor color) {
    validate(color);
    if (auto* state = find_state(component); state && state->color != color) {
        const bool previous = solid_fills_border_box(*state);
        const bool previous_dashed = dashed(*state);
        state->color = color;
        update_variant(*state, previous, previous_dashed);
    }
}

void ButtonComponentHost::apply_variant(runtime::ComponentId component, ButtonVariant variant) {
    validate(variant);
    if (auto* state = find_state(component); state && state->variant != variant) {
        const bool previous = solid_fills_border_box(*state);
        const bool previous_dashed = dashed(*state);
        state->variant = variant;
        update_variant(*state, previous, previous_dashed);
    }
}

void ButtonComponentHost::apply_danger(runtime::ComponentId component, bool danger) {
    if (auto* state = find_state(component); state && state->danger != danger) {
        const bool previous = solid_fills_border_box(*state);
        const bool previous_dashed = dashed(*state);
        state->danger = danger;
        update_variant(*state, previous, previous_dashed);
    }
}

void ButtonComponentHost::apply_ghost(runtime::ComponentId component, bool ghost) {
    if (auto* state = find_state(component); state && state->ghost != ghost) {
        const bool previous = solid_fills_border_box(*state);
        const bool previous_dashed = dashed(*state);
        state->ghost = ghost;
        update_variant(*state, previous, previous_dashed);
    }
}

void ButtonComponentHost::apply_size(runtime::ComponentId component, ControlSize size) {
    validate(size);
    auto* state = find_state(component);
    if (state == nullptr || state->size == size) {
        return;
    }
    state->size = size;
    update_layout(*state);
    update_typography(*state);
    update_visuals(*state);
}

void ButtonComponentHost::apply_disabled(runtime::ComponentId component, bool disabled) {
    auto* state = find_state(component);
    if (state == nullptr || state->disabled == disabled) {
        return;
    }
    const bool previous_border_box = solid_fills_border_box(*state);
    state->disabled = disabled;
    if (disabled) {
        state->hovered = false;
        static_cast<void>(state->press.reset());
        pointer_.cancel_interaction(state->interaction);
    }
    static_cast<void>(interactions_.set_eligible(state->interaction, !disabled));
    focus_.synchronize();
    if (previous_border_box != solid_fills_border_box(*state)) {
        dirty_->invalidate(state->node, runtime::DirtyFlags::Geometry);
    }
    update_visuals(*state);
}

void ButtonComponentHost::apply_loading(runtime::ComponentId component, bool loading) {
    auto* state = find_state(component);
    if (state == nullptr || state->loading_requested == loading) {
        return;
    }
    const auto deadline =
        loading && !state->loading && state->loading_delay.count_milliseconds() > 0
            ? std::optional{services_->animation_time() +
                            animation::AnimationDuration::milliseconds(state->loading_delay.count_milliseconds())}
            : std::nullopt;
    state->loading_requested = loading;
    state->loading_deadline = deadline;
    dirty_->invalidate(state->node, runtime::DirtyFlags::Animation);
    if (!deadline) {
        set_loading(*state, loading);
    }
}

void ButtonComponentHost::apply_loading_delay(runtime::ComponentId component, Duration delay) {
    auto* state = find_state(component);
    if (!state || state->loading_delay == delay) {
        return;
    }
    const auto deadline = state->loading_requested && !state->loading && delay.count_milliseconds() > 0
                              ? std::optional{services_->animation_time() +
                                              animation::AnimationDuration::milliseconds(delay.count_milliseconds())}
                              : std::nullopt;
    state->loading_delay = delay;
    state->loading_deadline = deadline;
    dirty_->invalidate(state->node, runtime::DirtyFlags::Animation);
    if (state->loading_requested && !deadline) {
        set_loading(*state, true);
    }
}

void ButtonComponentHost::set_loading(ButtonComponentState& value, bool loading) {
    if (value.loading == loading) {
        return;
    }
    const auto component = value.component;
    auto* state = &value;
    state->loading = loading;
    if (loading) {
        static_cast<void>(state->press.reset());
        pointer_.cancel_pointer_interaction(state->interaction);
        state = find_state(component);
        if (state == nullptr) {
            return;
        }
    }
    update_icon_branch(*state);
    update_layout(*state);
    update_visuals(*state);
}

void ButtonComponentHost::apply_shape(runtime::ComponentId component, ButtonShape shape) {
    validate(shape);
    if (auto* state = find_state(component); state && state->shape != shape) {
        state->shape = shape;
        state->decoration_geometry.reset();
        dirty_->invalidate(state->node, runtime::DirtyFlags::Geometry);
        update_layout(*state);
    }
}

void ButtonComponentHost::apply_block(runtime::ComponentId component, bool block) {
    if (auto* state = find_state(component); state && state->block != block) {
        state->block = block;
        update_layout(*state);
    }
}

void ButtonComponentHost::apply_icon_placement(runtime::ComponentId component, ButtonIconPlacement placement) {
    validate(placement);
    if (auto* state = find_state(component); state && state->icon_placement != placement) {
        state->icon_placement = placement;
        update_layout(*state);
    }
}

void ButtonComponentHost::update_icon_branch(ButtonComponentState& state) {
    if (!state.icon_wrapper.valid()) {
        return;
    }
    if (state.icon.valid()) {
        components().set_branch_active(state.icon, !state.loading);
    }
    if (state.loading_icon.valid()) {
        components().set_branch_active(state.loading_icon, state.loading);
    }
    services_->mark_scene_structure_dirty();
    dirty_->invalidate(components().root(state.icon_wrapper),
                       runtime::DirtyFlags::Measure | runtime::DirtyFlags::Layout);
}

std::size_t ButtonComponentHost::tick_auxiliary(animation::AnimationTime time) {
    std::size_t changed{};
    const auto buttons = mounted_buttons_;
    for (const auto& mounted : buttons) {
        if (auto* state = find_state(mounted.component);
            state && state->loading_deadline && time >= *state->loading_deadline) {
            state->loading_deadline.reset();
            set_loading(*state, state->loading_requested);
            ++changed;
        }
    }
    return changed;
}

std::optional<animation::AnimationTime> ButtonComponentHost::next_auxiliary_deadline() const {
    std::optional<animation::AnimationTime> result;
    for (const auto& mounted : mounted_buttons_) {
        if (const auto* state = find_state(mounted.component);
            state && state->loading_deadline && (!result || *state->loading_deadline < *result)) {
            result = state->loading_deadline;
        }
    }
    return result;
}

void ButtonComponentHost::apply_focus(runtime::ComponentId component, input::FocusPresentation focus) {
    auto* state = find_state(component);
    if (state == nullptr || state->focus == focus) {
        return;
    }
    state->focus = focus;
    update_visuals(*state);
}

void ButtonComponentHost::handle_pointer(runtime::ComponentId component, input::PointerDispatchContext& event) {
    auto* state = find_state(component);
    if (state == nullptr) {
        return;
    }
    switch (event.kind()) {
    case input::PointerEventKind::enter:
        if (!state->disabled) {
            state->hovered = true;
            update_visuals(*state);
        }
        return;
    case input::PointerEventKind::leave:
        if (state->hovered) {
            state->hovered = false;
            update_visuals(*state);
        }
        return;
    case input::PointerEventKind::down:
    case input::PointerEventKind::up:
    case input::PointerEventKind::cancel: {
        const auto result = state->press.dispatch(event, state->interaction, activation_allowed(component));
        state = find_state(component);
        if (state == nullptr) {
            return;
        }
        if (result.pressed_changed) {
            update_visuals(*state);
        }
        if (result.activate) {
            activate(component);
        }
        return;
    }
    case input::PointerEventKind::move:
        return;
    }
}

bool ButtonComponentHost::activation_allowed(runtime::ComponentId component) const noexcept {
    const auto* state = find_state(component);
    return state != nullptr && !state->disabled && !state->loading;
}

void ButtonComponentHost::activate(runtime::ComponentId component) {
    auto* state = find_state(component);
    if (state == nullptr || state->disabled || state->loading) {
        return;
    }
    start_wave(*state);
    auto callback = state->on_click;
    if (callback) {
        callback();
    }
}

void ButtonComponentHost::apply_wave(runtime::ComponentId component, bool wave) {
    if (auto* state = find_state(component); state && state->wave != wave) {
        state->wave = wave;
        if (!wave) {
            stop_wave(*state);
        }
    }
}

void ButtonComponentHost::start_wave(ButtonComponentState& state) {
    const auto& theme = components().theme_scope(state.component)->snapshot();
    const auto& token = theme.button();
    const auto policy = animation::resolve_motion_policy(theme, services_->motion_preference());
    const auto spec = policy.transition(animation::MotionDurationToken::slow, animation::MotionEasingToken::ease_out);
    if (!state.wave || state.disabled || state.loading || unbordered(state) || !policy.enabled() ||
        !focus_.state().window_active || token.wave_width <= 0 || token.wave_opacity <= 0 ||
        spec.duration.count_microseconds() == 0) {
        return;
    }
    stop_wave(state);
    const auto visual = visual_token(token, state);
    const auto selected = resolved_variant(state);
    state.wave_color = selected.color == ButtonColor::Default     ? token.default_hover_color
                       : selected.variant == ButtonVariant::Solid ? visual.background
                                                                  : visual.foreground;
    state.wave_active = true;
    state.wave_progress = 0;
    const auto index = animation_channel_index(ButtonAnimationChannel::wave_progress);
    state.animations[index] =
        animations_.play(state.animation_targets[index], 0.0F, 1.0F, spec, services_->animation_time());
    publish_wave(state);
    dirty_->invalidate(state.node, runtime::DirtyFlags::Geometry | runtime::DirtyFlags::Animation);
}

void ButtonComponentHost::stop_wave(ButtonComponentState& state) {
    auto& active = state.animations[animation_channel_index(ButtonAnimationChannel::wave_progress)];
    if (animations_.contains(active)) {
        static_cast<void>(animations_.cancel(active, services_->animation_time()));
    }
    active = {};
    const bool changed = state.wave_active;
    state.wave_active = false;
    state.wave_progress = 1;
    publish_wave(state);
    if (changed) {
        dirty_->invalidate(state.node, runtime::DirtyFlags::Geometry | runtime::DirtyFlags::Animation);
    }
}

void ButtonComponentHost::publish_wave(ButtonComponentState& state) {
    if (!state.wave_active || state.wave_progress >= 1) {
        if (state.wave_range.valid()) {
            button_scene_.update_content_effects(state.wave_range, {});
        }
        return;
    }
    if (!state.wave_range.valid()) {
        state.wave_fragment =
            components().register_scene_fragment(state.component, runtime::SceneFragmentPlacement::before_children);
        state.wave_range = button_scene_.create_content_range(state.wave_fragment, {});
    }
    const auto& node = nodes_->require(state.node);
    const auto& token = components().theme_scope(state.component)->snapshot().button();
    const graphics::LogicalRoundedRect shape{node.bounds,
                                             logical_radius(node.bounds, button_radius(state, node.bounds, token))};
    const auto clip = state.wave_clip ? std::optional{graphics::EffectClip{1, *state.wave_clip}} : std::nullopt;
    if (state.compact) {
        const auto effects = graphics::make_corner_outline_effects(
            shape, state.compact->corners, token.wave_width, token.wave_spread * state.wave_progress, state.wave_color,
            token.wave_opacity * (1.0F - state.wave_progress), node.translation, clip);
        button_scene_.update_content_effects(state.wave_range, effects);
    } else {
        const auto effect = graphics::make_outline_effect(
            shape, token.wave_width, token.wave_spread * state.wave_progress, state.wave_color,
            token.wave_opacity * (1.0F - state.wave_progress), node.translation, clip);
        button_scene_.update_content_effects(state.wave_range, std::span{&effect, 1});
    }
}

void ButtonComponentHost::on_window_active(bool active) {
    if (!active) {
        for (const auto& mounted : mounted_buttons_) {
            if (auto* state = find_state(mounted.component)) {
                stop_wave(*state);
            }
        }
    }
}

void ButtonComponentHost::update_visuals(ButtonComponentState& state) {
    const auto& theme = components().theme_scope(state.component)->snapshot();
    const auto& button = theme.button();
    const auto& visual = visual_token(button, state);
    const auto policy = animation::resolve_motion_policy(theme, services_->motion_preference());
    if (state.wave_active &&
        (!state.wave || state.disabled || state.loading || unbordered(state) || !policy.enabled() ||
         !focus_.state().window_active || button.wave_opacity <= 0 || button.wave_width <= 0)) {
        stop_wave(state);
    }
    const auto spec = policy.transition(animation::MotionDurationToken::mid, animation::MotionEasingToken::ease_in_out);

    if (!state.material_targets) {
        state.presentation_background = visual.background;
        state.presentation_border = visual.border;
        state.presentation_foreground = visual.foreground;
        state.presentation_loading_mix = state.loading ? 1.0F : 0.0F;
    } else {
        retarget_channel(state, ButtonAnimationChannel::background, visual.background, spec);
        retarget_channel(state, ButtonAnimationChannel::border, visual.border, spec);
        retarget_channel(state, ButtonAnimationChannel::foreground, visual.foreground, spec);
        retarget_channel(state, ButtonAnimationChannel::loading_mix, state.loading ? 1.0F : 0.0F, spec);
        update_spinner(state, policy);
    }
    apply_presentation(state);
}

void ButtonComponentHost::apply_presentation(ButtonComponentState& state, bool animation_update) {
    const auto& theme = components().theme_scope(state.component)->snapshot();
    const auto& button = theme.button();
    const auto& alias = theme.alias();
    const auto& visual = visual_token(button, state);
    const float loading_mix = std::clamp(state.presentation_loading_mix, 0.0F, 1.0F);
    const float layer_opacity = 1.0F + (button.loading_opacity - 1.0F) * loading_mix;
    auto next = state.visuals;
    next[static_cast<std::size_t>(component::ButtonVisualLayer::border)].color = channels(state.presentation_border);
    next[static_cast<std::size_t>(component::ButtonVisualLayer::border)].opacity =
        dashed(state) || state.compact ? 0.0F : layer_opacity;
    next[static_cast<std::size_t>(component::ButtonVisualLayer::background)].color =
        channels(state.presentation_background);
    next[static_cast<std::size_t>(component::ButtonVisualLayer::background)].opacity =
        state.compact ? 0 : layer_opacity;
    for (std::size_t segment = 0; segment < component::button_loading_segment_count; ++segment) {
        auto& indicator = next[component::button_loading_segment_index(segment)];
        indicator.color = channels(state.presentation_foreground);
        indicator.opacity =
            button.loading_opacity * loading_mix * spinner_segment_strength(state.spinner_phase, segment);
    }

    bool changed_material = false;
    bool changed_geometry = false;
    for (std::size_t index = 0; index < next.size(); ++index) {
        changed_material = changed_material || material_changed(state.visuals[index], next[index]);
        changed_geometry = changed_geometry || geometry_changed(state.visuals[index], next[index]);
    }
    state.visuals = next;
    auto next_effects = state.effects;
    next_effects.shadows = visual.shadow;
    next_effects.shadow_opacity = state.disabled ? 0.0F : layer_opacity;
    next_effects.focus_width = alias.line_width_focus;
    next_effects.focus_offset = alias.focus_outline_offset;
    next_effects.focus_color = alias.color_focus_outline;
    next_effects.focus_opacity = state.focus.focus_visible && !state.disabled ? 1.0F : 0.0F;
    next_effects.rounded_corners = state.compact ? std::optional{state.compact->corners} : std::nullopt;
    const bool changed_effect = next_effects != state.effects;
    state.effects = std::move(next_effects);
    if (state.scene.valid()) {
        auto published = state.visuals;
        if (state.compact) {
            for (std::size_t i = 2; i < published.size(); ++i) {
                published[i].opacity = 0;
            }
        }
        static_cast<void>(button_scene_.update(state.scene, published));
        if (changed_effect) {
            static_cast<void>(button_scene_.update_effects(state.scene, state.effects));
        }
    }
    if (changed_material) {
        auto flags = runtime::DirtyFlags::Material;
        if (animation_update) {
            flags = flags | runtime::DirtyFlags::Animation;
        }
        dirty_->invalidate(state.node, flags);
    }
    if (changed_geometry) {
        dirty_->invalidate(state.node, runtime::DirtyFlags::Geometry);
    }
    if (changed_effect) {
        dirty_->invalidate(state.node, runtime::DirtyFlags::Geometry | runtime::DirtyFlags::Material);
    }
    auto foreground = channels(state.presentation_foreground);
    foreground[3] *= layer_opacity;
    static_cast<void>(state.foreground.set(foreground));
    update_decoration_material(state);
    synchronize_compact(state);
    if (const auto context = state.compact_context.lock()) {
        context->publish_seams();
    }
    if (state.wave_active) {
        publish_wave(state);
    }
}

void ButtonComponentHost::register_animation_targets(ButtonComponentState& state) {
    constexpr auto dirty = animation::AnimationDirtyDomain::material | animation::AnimationDirtyDomain::animation;
    constexpr std::array kinds{
        animation::AnimationValueKind::color,  animation::AnimationValueKind::color,
        animation::AnimationValueKind::color,  animation::AnimationValueKind::scalar,
        animation::AnimationValueKind::scalar, animation::AnimationValueKind::scalar,
    };
    auto targets = std::make_unique<animation::MaterialTransitionTargets<button_animation_channel_count>>(
        animations_, static_cast<animation::AnimationTargetSink&>(*this), kinds, dirty);
    try {
        state.animation_targets = targets->targets();
        for (const auto channel : {ButtonAnimationChannel::background, ButtonAnimationChannel::border,
                                   ButtonAnimationChannel::foreground, ButtonAnimationChannel::loading_mix,
                                   ButtonAnimationChannel::spinner_phase, ButtonAnimationChannel::wave_progress}) {
            animation_bindings_.push_back({
                state.animation_targets[animation_channel_index(channel)],
                state.component,
                channel,
            });
        }
        state.material_targets = std::move(targets);
    } catch (...) {
        targets.reset();
        std::erase_if(animation_bindings_,
                      [this](const auto& binding) { return !animations_.contains(binding.target); });
        state.animation_targets = {};
        throw;
    }
}

void ButtonComponentHost::unregister_animation_targets(ButtonComponentState& state) noexcept {
    if (!state.material_targets) {
        return;
    }
    try {
        state.material_targets.reset();
        std::erase_if(animation_bindings_, [&state](const auto& binding) {
            return std::find(state.animation_targets.begin(), state.animation_targets.end(), binding.target) !=
                   state.animation_targets.end();
        });
    } catch (...) {
    }
    state.animation_targets = {};
    state.animations = {};
}

void ButtonComponentHost::retarget_channel(ButtonComponentState& state, ButtonAnimationChannel channel,
                                           const animation::AnimationValue& target,
                                           const animation::AnimationSpec& spec) {
    const auto index = animation_channel_index(channel);
    animation::AnimationValue current;
    switch (channel) {
    case ButtonAnimationChannel::background:
        current = state.presentation_background;
        break;
    case ButtonAnimationChannel::border:
        current = state.presentation_border;
        break;
    case ButtonAnimationChannel::foreground:
        current = state.presentation_foreground;
        break;
    case ButtonAnimationChannel::loading_mix:
        current = state.presentation_loading_mix;
        break;
    case ButtonAnimationChannel::spinner_phase:
        current = state.spinner_phase;
        break;
    case ButtonAnimationChannel::wave_progress:
        current = state.wave_progress;
        break;
    }

    auto& active = state.animations[index];
    animation::retarget_material_channel(animations_, active, state.animation_targets[index], std::move(current),
                                         target, spec, services_->animation_time());
}

void ButtonComponentHost::update_spinner(ButtonComponentState& state, const animation::MotionPolicy& policy) {
    if (state.loading && !state.has_loading_icon && policy.enabled() && components().branch_active(state.component)) {
        start_spinner(state);
        return;
    }
    stop_spinner(state);
}

void ButtonComponentHost::start_spinner(ButtonComponentState& state) {
    const auto index = animation_channel_index(ButtonAnimationChannel::spinner_phase);
    auto& active = state.animations[index];
    if (animations_.contains(active)) {
        return;
    }
    const float phase = normalized_spinner_phase(state.spinner_phase);
    state.spinner_phase = phase;
    active = animations_.play(state.animation_targets[index], phase, phase + 1.0F,
                              {{}, spinner_period, animation::Easing::linear()}, services_->animation_time());
}

void ButtonComponentHost::stop_spinner(ButtonComponentState& state) {
    const auto index = animation_channel_index(ButtonAnimationChannel::spinner_phase);
    auto& active = state.animations[index];
    if (animations_.contains(active)) {
        static_cast<void>(animations_.cancel(active, services_->animation_time()));
    }
    active = {};
    state.spinner_phase = 0.0F;
}

void ButtonComponentHost::apply(animation::AnimationId, animation::AnimationTargetId target,
                                const animation::AnimationValue& value, animation::AnimationDirtyDomain dirty_domain) {
    if (!animation::has_any(dirty_domain, animation::AnimationDirtyDomain::material) ||
        !animation::has_any(dirty_domain, animation::AnimationDirtyDomain::animation)) {
        throw std::logic_error("Button animation target lost Material/Animation dirty domains");
    }
    const auto binding = std::find_if(animation_bindings_.begin(), animation_bindings_.end(),
                                      [target](const auto& candidate) { return candidate.target == target; });
    if (binding == animation_bindings_.end()) {
        throw std::out_of_range("Button animation target binding is stale");
    }
    auto* state = find_state(binding->component);
    if (state == nullptr) {
        throw std::out_of_range("Button animation component is stale");
    }
    switch (binding->channel) {
    case ButtonAnimationChannel::background:
        state->presentation_background = std::get<Color>(value);
        break;
    case ButtonAnimationChannel::border:
        state->presentation_border = std::get<Color>(value);
        break;
    case ButtonAnimationChannel::foreground:
        state->presentation_foreground = std::get<Color>(value);
        break;
    case ButtonAnimationChannel::loading_mix:
        state->presentation_loading_mix = std::get<float>(value);
        break;
    case ButtonAnimationChannel::spinner_phase:
        state->spinner_phase = normalized_spinner_phase(std::get<float>(value));
        break;
    case ButtonAnimationChannel::wave_progress:
        state->wave_progress = std::clamp(std::get<float>(value), 0.0F, 1.0F);
        publish_wave(*state);
        dirty_->invalidate(state->node, runtime::DirtyFlags::Geometry | runtime::DirtyFlags::Animation);
        return;
    }
    apply_presentation(*state, true);
}

void ButtonComponentHost::completed(animation::AnimationId animation, animation::AnimationTargetId target) {
    const auto binding = std::find_if(animation_bindings_.begin(), animation_bindings_.end(),
                                      [target](const auto& candidate) { return candidate.target == target; });
    if (binding == animation_bindings_.end()) {
        return;
    }
    if (auto* state = find_state(binding->component)) {
        auto& active = state->animations[animation_channel_index(binding->channel)];
        if (active == animation) {
            active = {};
            if (binding->channel == ButtonAnimationChannel::wave_progress) {
                state->wave_active = false;
                state->wave_progress = 1;
                publish_wave(*state);
            }
            if (binding->channel == ButtonAnimationChannel::spinner_phase) {
                const auto& theme = components().theme_scope(state->component)->snapshot();
                const auto policy = animation::resolve_motion_policy(theme, services_->motion_preference());
                if (state->loading && !state->has_loading_icon && policy.enabled()) {
                    start_spinner(*state);
                }
            }
        }
    }
}

void ButtonComponentHost::update_typography(ButtonComponentState& state) {
    const auto& theme = components().theme_scope(state.component)->snapshot();
    static_cast<void>(state.typography.set(content_typography(theme, state)));
}

void ButtonComponentHost::update_layout(ButtonComponentState& state) {
    const auto& button = components().theme_scope(state.component)->snapshot().button();
    const auto candidate = content_layout(button, state);
    if (candidate == state.layout_model) {
        return;
    }
    state.layout_model = candidate;
    layout_->set_layout(state.node, candidate);
    dirty_->invalidate(state.node, runtime::DirtyFlags::Measure | runtime::DirtyFlags::Layout |
                                       runtime::DirtyFlags::Geometry | runtime::DirtyFlags::HitTest);
}

void ButtonComponentHost::subscribe_theme(ButtonComponentState& state) {
    const auto theme = components().theme_scope(state.component);
    state.color_subscription = theme->capture(
        [this, component = state.component](theme_runtime::DirtyPhase) {
            if (auto* current = find_state(component)) {
                update_visuals(*current);
            }
        },
        [theme] {
            static_cast<void>(theme->button_colors());
            static_cast<void>(theme->focus_outline_color());
        });
    state.effect_subscription = theme->capture(
        [this, component = state.component](theme_runtime::DirtyPhase) {
            if (auto* current = find_state(component)) {
                dirty_->invalidate(current->node, runtime::DirtyFlags::Geometry);
                update_visuals(*current);
            }
        },
        [theme] {
            static_cast<void>(theme->button_border_radius());
            static_cast<void>(theme->button_shadows());
            static_cast<void>(theme->focus_outline_width());
            static_cast<void>(theme->focus_outline_offset());
        });
    state.layout_subscription = theme->capture(
        [this, component = state.component](theme_runtime::DirtyPhase) {
            if (auto* current = find_state(component)) {
                dirty_->invalidate(current->node, runtime::DirtyFlags::Geometry);
                update_layout(*current);
                update_visuals(*current);
            }
        },
        [theme] {
            static_cast<void>(theme->button_control_heights());
            static_cast<void>(theme->button_padding_inline());
            static_cast<void>(theme->button_border_width());
            static_cast<void>(theme->button_icon_gap());
        });
    state.typography_subscription = theme->capture(
        [this, component = state.component](theme_runtime::DirtyPhase) {
            if (auto* current = find_state(component)) {
                update_typography(*current);
                update_layout(*current);
                update_visuals(*current);
            }
        },
        [theme] {
            static_cast<void>(theme->button_typography());
            static_cast<void>(theme->text_font_family());
            static_cast<void>(theme->text_font_weight());
        });
    state.motion_subscription = theme->capture(
        [this, component = state.component](theme_runtime::DirtyPhase) {
            if (auto* current = find_state(component)) {
                update_visuals(*current);
            }
        },
        [theme] {
            static_cast<void>(theme->motion_unit());
            static_cast<void>(theme->motion_base());
            static_cast<void>(theme->motion_enabled());
        });
}

void ButtonComponentHost::update_decoration_material(ButtonComponentState& state) {
    if (!state.decoration_range.valid()) {
        return;
    }
    const auto& token = components().theme_scope(state.component)->snapshot().button();
    const float opacity =
        1.0F + (token.loading_opacity - 1.0F) * std::clamp(state.presentation_loading_mix, 0.0F, 1.0F);
    for (auto& effect : state.decorations) {
        effect.material = {state.presentation_border, opacity, dashed(state)};
    }
    button_scene_.update_content_effects(state.decoration_range, state.decorations);
}

void ButtonComponentHost::synchronize_decorations(ButtonComponentState& state, runtime::Rect clip) {
    if (!dashed(state)) {
        if (!state.decorations.empty()) {
            state.decorations.clear();
            state.decoration_geometry.reset();
            button_scene_.update_content_effects(state.decoration_range, {});
        }
        return;
    }
    const auto& token = components().theme_scope(state.component)->snapshot().button();
    const auto& node = nodes_->require(state.node);
    const float radius = logical_radius(node.bounds, button_radius(state, node.bounds, token));
    const std::array key{node.bounds.x,
                         node.bounds.y,
                         node.bounds.width,
                         node.bounds.height,
                         node.translation.x,
                         node.translation.y,
                         clip.x,
                         clip.y,
                         clip.width,
                         clip.height,
                         radius,
                         token.border_width,
                         token.dash_length,
                         token.dash_gap};
    if (state.decoration_geometry == key) {
        return;
    }
    const float width = std::min(token.border_width, 0.5F * std::min(node.bounds.width, node.bounds.height));
    std::vector<graphics::RoundedEffectInstance> effects;
    if (width > 0) {
        const float band = std::min(0.5F * node.bounds.height, std::max(radius, width));
        const float period = token.dash_length + token.dash_gap;
        const double horizontal_count = std::ceil(static_cast<double>(node.bounds.width) / period);
        const double vertical_count = std::ceil(static_cast<double>(node.bounds.height - 2.0F * band) / period);
        const double count = 2 * (horizontal_count + vertical_count) * (state.compact ? 4 : 1);
        if (!std::isfinite(period) || !std::isfinite(count) || count > component::retained_content_visual_capacity) {
            throw std::length_error("Button dashed border exceeds 4096 effects");
        }
        effects.reserve(static_cast<std::size_t>(count));
        const graphics::LogicalRoundedRect shape{{node.bounds.x + width, node.bounds.y + width,
                                                  std::max(0.0F, node.bounds.width - 2.0F * width),
                                                  std::max(0.0F, node.bounds.height - 2.0F * width)},
                                                 std::max(0.0F, radius - width)};
        const auto append = [&](runtime::Rect segment) {
            segment.x += node.translation.x;
            segment.y += node.translation.y;
            const float right = std::min(segment.x + segment.width, clip.x + clip.width);
            const float bottom = std::min(segment.y + segment.height, clip.y + clip.height);
            segment.x = std::max(segment.x, clip.x);
            segment.y = std::max(segment.y, clip.y);
            segment.width = std::max(0.0F, right - segment.x);
            segment.height = std::max(0.0F, bottom - segment.y);
            if (segment.width <= 0 || segment.height <= 0) {
                return;
            }
            if (state.compact) {
                const auto corners = graphics::make_corner_outline_effects(
                    shape, state.compact->corners, width, 0, state.presentation_border, 1, node.translation,
                    graphics::EffectClip{effects.size() + 1, segment});
                effects.insert(effects.end(), corners.begin(), corners.end());
            } else {
                effects.push_back(graphics::make_outline_effect(shape, width, 0, state.presentation_border, 1,
                                                                node.translation,
                                                                graphics::EffectClip{effects.size() + 1, segment}));
            }
        };
        for (std::size_t index = 0; index < static_cast<std::size_t>(horizontal_count); ++index) {
            const float offset = static_cast<float>(index) * period;
            const float length = std::min(token.dash_length, node.bounds.width - offset);
            append({node.bounds.x + offset, node.bounds.y, length, band});
            append({node.bounds.x + offset, node.bounds.y + node.bounds.height - band, length, band});
        }
        for (std::size_t index = 0; index < static_cast<std::size_t>(vertical_count); ++index) {
            const float offset = static_cast<float>(index) * period;
            const float length = std::min(token.dash_length, node.bounds.height - 2.0F * band - offset);
            append({node.bounds.x, node.bounds.y + band + offset, width, length});
            append({node.bounds.x + node.bounds.width - width, node.bounds.y + band + offset, width, length});
        }
    }
    if (!state.decoration_range.valid() && !effects.empty()) {
        if (!state.decoration_fragment.valid()) {
            state.decoration_fragment =
                components().register_scene_fragment(state.component, runtime::SceneFragmentPlacement::before_children);
        }
        state.decoration_range = button_scene_.create_content_range(state.decoration_fragment, {});
    }
    state.decorations = std::move(effects);
    state.decoration_geometry = key;
    update_decoration_material(state);
}

void ButtonComponentHost::synchronize_compact(ButtonComponentState& state) {
    if (!state.compact) {
        return;
    }
    const auto& node = nodes_->require(state.node);
    const auto& button = components().theme_scope(state.component)->snapshot().button();
    const float radius = logical_radius(node.bounds, button_radius(state, node.bounds, button));
    const auto clip = state.wave_clip ? std::optional{graphics::EffectClip{1, *state.wave_clip}} : std::nullopt;
    const float mix = std::clamp(state.presentation_loading_mix, 0.0F, 1.0F);
    const float opacity = 1 + (button.loading_opacity - 1) * mix;
    const auto border =
        graphics::make_corner_fill_effects({node.bounds, radius}, state.compact->corners, state.presentation_border,
                                           dashed(state) ? 0 : opacity, node.translation, clip);
    const bool solid = solid_fills_border_box(state);
    const float width = solid ? 0 : button.border_width;
    const graphics::LogicalRoundedRect shape{{node.bounds.x + width, node.bounds.y + width,
                                              std::max(0.0F, node.bounds.width - 2 * width),
                                              std::max(0.0F, node.bounds.height - 2 * width)},
                                             std::max(0.0F, radius - width)};
    const auto background = graphics::make_corner_fill_effects(
        shape, state.compact->corners, state.presentation_background, opacity, node.translation, clip);
    std::array<graphics::RoundedEffectInstance, 8> effects;
    std::copy(border.begin(), border.end(), effects.begin());
    std::copy(background.begin(), background.end(), effects.begin() + 4);
    if (!state.compact_range.valid()) {
        state.compact_fragment =
            components().register_scene_fragment(state.component, runtime::SceneFragmentPlacement::before_children);
        state.compact_range = button_scene_.create_content_range(state.compact_fragment, {});
    }
    button_scene_.update_content_effects(state.compact_range, effects);
    if (!state.compact_spinner_range.valid()) {
        state.compact_spinner_fragment =
            components().register_scene_fragment(state.component, runtime::SceneFragmentPlacement::before_children);
        state.compact_spinner_range = button_scene_.create_content_range(state.compact_spinner_fragment, {});
    }
    button_scene_.update_content_range(state.compact_spinner_range, std::span{state.visuals}.subspan(2));
}

void ButtonComponentHost::synchronize_geometry(ButtonComponentState& state, runtime::Size viewport,
                                               runtime::Rect clip) {
    state.wave_clip = clip;
    const auto& theme = components().theme_scope(state.component)->snapshot();
    const auto& button = theme.button();
    const auto& node = nodes_->require(state.node);
    const float radius = button_radius(state, node.bounds, button);
    const auto& content = layout_->horizontal_content_geometry(state.node);
    const runtime::Rect inset_background_bounds{
        node.bounds.x + button.border_width,
        node.bounds.y + button.border_width,
        std::max(0.0F, node.bounds.width - 2.0F * button.border_width),
        std::max(0.0F, node.bounds.height - 2.0F * button.border_width),
    };
    const bool border_box_fill = solid_fills_border_box(state);
    const auto background_bounds = border_box_fill ? node.bounds : inset_background_bounds;
    auto next = state.visuals;
    next[static_cast<std::size_t>(component::ButtonVisualLayer::border)] =
        make_quad(node.bounds, viewport, next[0].color, next[0].opacity, radius, node.translation);
    next[static_cast<std::size_t>(component::ButtonVisualLayer::background)] =
        make_quad(background_bounds, viewport, next[1].color, next[1].opacity,
                  border_box_fill ? radius : std::max(0.0F, radius - button.border_width), node.translation);
    const auto indicator_bounds = content.loading_indicator_bounds.value_or(runtime::Rect{});
    for (std::size_t segment = 0; segment < component::button_loading_segment_count; ++segment) {
        const auto index = component::button_loading_segment_index(segment);
        const auto bounds = spinner_segment_bounds(indicator_bounds, segment);
        next[index] = make_quad(bounds, viewport, next[index].color, next[index].opacity,
                                0.5F * std::min(bounds.width, bounds.height), node.translation);
    }
    state.visuals = next;
    auto published = state.visuals;
    if (state.compact) {
        for (std::size_t i = 2; i < published.size(); ++i) {
            published[i].opacity = 0;
        }
    }
    static_cast<void>(button_scene_.update(state.scene, published));
    auto effects = state.effects;
    effects.shape = {node.bounds, logical_radius(node.bounds, radius)};
    effects.translation = node.translation;
    if (effects != state.effects) {
        state.effects = std::move(effects);
        static_cast<void>(button_scene_.update_effects(state.scene, state.effects));
    }
    synchronize_decorations(state, clip);
    synchronize_compact(state);
    if (const auto context = state.compact_context.lock()) {
        context->publish_seams();
    }
    if (state.wave_active) {
        publish_wave(state);
    }
}

void mount_button_component(const ButtonProps& props, const ButtonSlots& slots) {
    if (active_button_host == nullptr) {
        throw std::logic_error("ryn::Button can only be declared inside an active ButtonComponentHost");
    }
    auto& host = *active_button_host;
    auto& build = runtime::require_component_build_context();
    const auto compact = nearest_compact(build);
    const auto initial_type = read_prop(ButtonPropsAccess::type(props));
    const auto initial_size = compact && !ButtonPropsAccess::explicit_size(props)
                                  ? compact->metadata.size
                                  : read_prop(ButtonPropsAccess::size(props));
    const auto initial_shape = read_prop(ButtonPropsAccess::shape(props));
    const auto initial_placement = read_prop(ButtonPropsAccess::icon_placement(props));
    auto reference = ButtonPropsAccess::ref(props);
    if (reference) {
        reference->ensure_owner();
        if (reference->binding) {
            throw std::logic_error("ButtonRef is already bound");
        }
    }
    if (!slots.content && !slots.icon && !slots.loading) {
        throw std::invalid_argument("Button requires a content or icon slot");
    }
    if ((slots.content && !SlotContentAccess::function(*slots.content)) ||
        (slots.icon && !SlotContentAccess::function(*slots.icon)) ||
        (slots.loading && !SlotContentAccess::function(*slots.loading))) {
        throw std::invalid_argument("Button slots require a callable");
    }
    const auto initial_color =
        ButtonPropsAccess::color(props) ? std::optional{read_prop(*ButtonPropsAccess::color(props))} : std::nullopt;
    const auto initial_variant =
        ButtonPropsAccess::variant(props) ? std::optional{read_prop(*ButtonPropsAccess::variant(props))} : std::nullopt;
    validate(initial_type);
    validate(initial_size);
    validate(initial_shape);
    validate(initial_placement);
    if (initial_color) {
        validate(*initial_color);
    }
    if (initial_variant) {
        validate(*initial_variant);
    }
    const bool initial_disabled = read_prop(ButtonPropsAccess::disabled(props));
    const bool initial_requested = read_prop(ButtonPropsAccess::loading(props));
    const auto initial_delay = read_prop(ButtonPropsAccess::loading_delay(props));
    const bool initial_loading = initial_requested && initial_delay.count_milliseconds() == 0;
    const auto initial_deadline =
        initial_requested && !initial_loading
            ? std::optional{host.animation_time() +
                            animation::AnimationDuration::milliseconds(initial_delay.count_milliseconds())}
            : std::nullopt;
    const auto theme_scope = build.theme_scope();
    const auto& theme = theme_scope->snapshot();
    ButtonComponentState initial_state{channels(theme.button().default_color),
                                       {
                                           theme.text().font_family,
                                           theme.text().font_weight,
                                           false,
                                           theme.text().font_size,
                                           theme.text().line_height,
                                       }};
    initial_state.type = initial_type;
    initial_state.color = initial_color;
    initial_state.variant = initial_variant;
    initial_state.danger = read_prop(ButtonPropsAccess::danger(props));
    initial_state.ghost = read_prop(ButtonPropsAccess::ghost(props));
    initial_state.size = initial_size;
    initial_state.disabled = initial_disabled;
    initial_state.loading = initial_loading;
    initial_state.shape = initial_shape;
    initial_state.icon_placement = initial_placement;
    initial_state.block = read_prop(ButtonPropsAccess::block(props));
    initial_state.icon_only = !slots.content;
    initial_state.has_icon = slots.icon.has_value();
    initial_state.has_loading_icon = slots.loading.has_value();
    initial_state.on_click = ButtonPropsAccess::on_click(props);
    const auto component = build.mount_component<ButtonComponentState>(
        content_foreground(theme.button(), initial_state), content_typography(theme, initial_state));
    auto& state = build.state<ButtonComponentState>(component);
    state.component = component;
    state.node = build.root(component);
    if (compact) {
        compact->claim(component);
        build.on_resource_cleanup(component, [compact, component] { compact->detach(component); });
    }
    if (reference) {
        state.ref = reference;
        reference->binding = true;
        build.on_resource_cleanup(component, [&host, component] {
            if (auto* state = host.find_state(component); state && state->ref) {
                state->ref->binding = false;
                state->ref->focus = {};
                state->ref->blur = {};
            }
        });
    }
    state.type = initial_type;
    state.color = initial_color;
    state.variant = initial_variant;
    state.danger = initial_state.danger;
    state.ghost = initial_state.ghost;
    state.size = initial_size;
    state.disabled = initial_disabled;
    state.loading = initial_loading;
    state.loading_requested = initial_requested;
    state.loading_delay = initial_delay;
    state.loading_deadline = initial_deadline;
    state.shape = initial_shape;
    state.icon_placement = initial_placement;
    state.block = initial_state.block;
    state.wave = read_prop(ButtonPropsAccess::wave(props));
    state.icon_only = initial_state.icon_only;
    state.has_icon = initial_state.has_icon;
    state.has_loading_icon = initial_state.has_loading_icon;
    state.on_click = ButtonPropsAccess::on_click(props);
    state.layout_model = content_layout(theme.button(), state);
    host.layout_->set_layout(state.node, state.layout_model);
    runtime::connect_layout_style(build.scope(component), ButtonPropsAccess::layout(props), state.node, *host.nodes_,
                                  *host.dirty_);

    state.fragment = build.register_scene_fragment(component, runtime::SceneFragmentPlacement::before_children);
    build.on_resource_cleanup(component, [&host, component] {
        if (auto* current = host.find_state(component); current && current->decoration_range.valid()) {
            host.button_scene_.destroy_content_range(current->decoration_range);
        }
        if (auto* current = host.find_state(component); current && current->wave_range.valid()) {
            host.button_scene_.destroy_content_range(current->wave_range);
        }
        if (auto* current = host.find_state(component); current && current->compact_range.valid()) {
            host.button_scene_.destroy_content_range(current->compact_range);
        }
        if (auto* current = host.find_state(component); current && current->compact_spinner_range.valid()) {
            host.button_scene_.destroy_content_range(current->compact_spinner_range);
        }
    });
    const auto parent_interaction = host.interaction_for(component);
    state.interaction = host.interactions_.create({
        component,
        state.node,
        parent_interaction,
        !state.disabled,
        true,
        {},
    });
    input::InteractionHandlers pointer_handlers;
    pointer_handlers.target = [&host, component](input::PointerDispatchContext& event) {
        host.handle_pointer(component, event);
    };
    static_cast<void>(host.interactions_.set_handlers(state.interaction, std::move(pointer_handlers)));
    input::FocusHandlers focus_handlers;
    focus_handlers.state_changed = [&host, component](input::FocusPresentation focus) {
        host.apply_focus(component, focus);
    };
    focus_handlers.activation_allowed = [&host, component] {
        return host.activation_allowed(component);
    };
    focus_handlers.activate = [&host, component] {
        host.activate(component);
    };
    static_cast<void>(host.interactions_.set_focus_handlers(state.interaction, std::move(focus_handlers)));

    host.update_visuals(state);
    state.scene = host.button_scene_.create(component, state.node, state.fragment, state.interaction, state.visuals,
                                            state.effects);
    build.on_resource_cleanup(component, [buttons = &host.button_scene_, scene = state.scene] {
        static_cast<void>(buttons->destroy(scene));
    });
    build.on_resource_cleanup(
        component, [pointer = &host.pointer_, interactions = &host.interactions_, interaction = state.interaction] {
            pointer->cancel_interaction(interaction);
            static_cast<void>(interactions->remove(interaction));
        });
    host.register_animation_targets(state);
    host.update_visuals(state);
    build.on_resource_cleanup(component, [buttons = &host, component] {
        if (auto* current = buttons->find_state(component)) {
            buttons->unregister_animation_targets(*current);
        }
    });

    auto& scope = build.scope(component);
    static_cast<void>(connect_prop(scope, ButtonPropsAccess::type(props),
                                   [&host, component](ButtonType type) { host.apply_type(component, type); }));
    if (const auto& color = ButtonPropsAccess::color(props)) {
        connect_prop(scope, *color, [&host, component](ButtonColor value) { host.apply_color(component, value); });
    }
    if (const auto& variant = ButtonPropsAccess::variant(props)) {
        connect_prop(scope, *variant,
                     [&host, component](ButtonVariant value) { host.apply_variant(component, value); });
    }
    connect_prop(scope, ButtonPropsAccess::danger(props),
                 [&host, component](bool value) { host.apply_danger(component, value); });
    connect_prop(scope, ButtonPropsAccess::ghost(props),
                 [&host, component](bool value) { host.apply_ghost(component, value); });
    if (!compact || ButtonPropsAccess::explicit_size(props)) {
        static_cast<void>(connect_prop(scope, ButtonPropsAccess::size(props),
                                       [&host, component](ControlSize size) { host.apply_size(component, size); }));
    }
    static_cast<void>(connect_prop(scope, ButtonPropsAccess::disabled(props),
                                   [&host, component](bool disabled) { host.apply_disabled(component, disabled); }));
    static_cast<void>(connect_prop(scope, ButtonPropsAccess::loading(props),
                                   [&host, component](bool loading) { host.apply_loading(component, loading); }));
    connect_prop(scope, ButtonPropsAccess::loading_delay(props),
                 [&host, component](Duration delay) { host.apply_loading_delay(component, delay); });
    connect_prop(scope, ButtonPropsAccess::shape(props),
                 [&host, component](ButtonShape shape) { host.apply_shape(component, shape); });
    connect_prop(scope, ButtonPropsAccess::block(props),
                 [&host, component](bool block) { host.apply_block(component, block); });
    connect_prop(scope, ButtonPropsAccess::wave(props),
                 [&host, component](bool wave) { host.apply_wave(component, wave); });
    connect_prop(scope, ButtonPropsAccess::icon_placement(props), [&host, component](ButtonIconPlacement placement) {
        host.apply_icon_placement(component, placement);
    });

    host.subscribe_theme(state);
    if (slots.icon || slots.loading) {
        build.mount_slot_with_semantic_text_style(
            component, Content{[&] {
                auto& nested = runtime::require_component_build_context();
                state.icon_wrapper = nested.mount_component<int>(0);
                const auto wrapper = nested.root(state.icon_wrapper);
                host.layout_->set_layout(
                    wrapper,
                    layout::ComponentLayout{
                        [&host, component](layout::LayoutEngine& engine, runtime::NodeId, layout::Constraints limits) {
                            const auto& state = *host.find_state(component);
                            const auto selected = state.loading ? state.loading_icon : state.icon;
                            runtime::Size result{};
                            for (auto branch : {state.icon, state.loading_icon}) {
                                if (branch.valid()) {
                                    const auto measured = engine.measure_child(
                                        host.components().root(branch),
                                        branch == selected ? limits : layout::Constraints::fixed(0, 0));
                                    if (branch == selected) {
                                        result = measured;
                                    }
                                }
                            }
                            return result;
                        },
                        [&host, component](layout::LayoutEngine& engine, runtime::NodeId, runtime::Rect bounds) {
                            const auto& state = *host.find_state(component);
                            const auto selected = state.loading ? state.loading_icon : state.icon;
                            for (auto branch : {state.icon, state.loading_icon}) {
                                if (branch.valid()) {
                                    engine.place_child(host.components().root(branch),
                                                       branch == selected ? bounds
                                                                          : runtime::Rect{bounds.x, bounds.y, 0, 0});
                                }
                            }
                        }});
                nested.on_resource_cleanup(state.icon_wrapper,
                                           [&host, wrapper] { host.layout_->remove_layout(wrapper); });
                nested.mount_slot(state.icon_wrapper, Content{[&] {
                                      const auto mount_branch = [&](const auto& slot, runtime::ComponentId& branch) {
                                          if (!slot) {
                                              return;
                                          }
                                          auto& children = runtime::require_component_build_context();
                                          branch = children.mount_component<int>(0);
                                          const auto node = children.root(branch);
                                          host.layout_->set_layout(node, layout::BoxLayout{});
                                          children.on_resource_cleanup(
                                              branch, [&host, node] { host.layout_->remove_layout(node); });
                                          children.mount_slot(branch, Content{SlotContentAccess::function(*slot)});
                                      };
                                      mount_branch(slots.icon, state.icon);
                                      mount_branch(slots.loading, state.loading_icon);
                                  }});
            }},
            Prop<runtime::SemanticForeground>{state.foreground}, Prop<runtime::SemanticTypography>{state.typography});
        host.update_icon_branch(state);
        host.update_layout(state);
    }
    if (slots.content) {
        build.mount_slot_with_semantic_text_style(component, *slots.content,
                                                  Prop<runtime::SemanticForeground>{state.foreground},
                                                  Prop<runtime::SemanticTypography>{state.typography});
    }
    host.dirty_->invalidate(state.node, runtime::DirtyFlags::Measure | runtime::DirtyFlags::Layout |
                                            runtime::DirtyFlags::Geometry | runtime::DirtyFlags::Material |
                                            runtime::DirtyFlags::HitTest);
    host.record_mounted_button({
        component,
        state.node,
        state.interaction,
        state.scene,
        state.fragment,
    });
    const auto focus = [&host, component] {
        const auto* state = host.find_state(component);
        if (!state || state->disabled || !host.focus_.state().window_active) {
            return false;
        }
        host.focus_.defer_focus(state->interaction, input::FocusModality::keyboard);
        return true;
    };
    if (reference) {
        reference->focus = focus;
        reference->blur = [&host, component] {
            const auto* state = host.find_state(component);
            if (!state || host.focus_.state().focused != state->interaction) {
                return false;
            }
            host.focus_.defer_focus({}, input::FocusModality::keyboard);
            return true;
        };
    }
    if (ButtonPropsAccess::auto_focus(props)) {
        static_cast<void>(focus());
    }
    if (compact) {
        state.compact_context = compact;
        compact->attach(
            component,
            [&host, component, explicit_size = ButtonPropsAccess::explicit_size(props)](const CompactMetadata& value) {
                if (auto* current = host.find_state(component)) {
                    if (current->compact != value) {
                        current->compact = value;
                        current->decoration_geometry.reset();
                        if (!explicit_size) {
                            host.apply_size(component, value.size);
                        }
                        host.update_visuals(*current);
                        host.dirty_->invalidate(current->node, runtime::DirtyFlags::Geometry);
                    }
                }
            },
            [&host, component] {
                const auto& current = *host.find_state(component);
                const auto& node = host.nodes_->require(current.node);
                const auto& token = host.components().theme_scope(component)->snapshot().button();
                return CompactBorder{
                    .shape = {node.bounds, logical_radius(node.bounds, button_radius(current, node.bounds, token))},
                    .corners = current.compact ? current.compact->corners : std::array{true, true, true, true},
                    .color = current.presentation_border,
                    .width = unbordered(current) ? 0 : token.border_width,
                    .priority = current.disabled                                   ? 0
                                : current.hovered                                  ? 4
                                : current.focus.focused || current.press.pressed() ? 3
                                                                                   : 2,
                    .visible = !unbordered(current),
                    .translation = node.translation,
                    .clip =
                        current.wave_clip ? std::optional{graphics::EffectClip{1, *current.wave_clip}} : std::nullopt,
                    .dashed = dashed(current),
                    .decorations = current.decorations};
            },
            &host.button_scene_);
    }
}

} // namespace ryn::detail

namespace ryn {

void Button(ButtonProps props, ButtonContent content) {
    detail::mount_button_component(props, ButtonSlots{.content = std::move(content)});
}

void Button(ButtonProps props, ButtonSlots slots) {
    detail::mount_button_component(props, slots);
}

void Button(ButtonProps props, ButtonContent content, ButtonIcon icon) {
    detail::mount_button_component(props, ButtonSlots{.content = std::move(content), .icon = std::move(icon)});
}

void Button(ButtonProps props, ButtonContent content, ButtonIcon icon, ButtonLoadingIcon loading) {
    detail::mount_button_component(
        props, ButtonSlots{.content = std::move(content), .icon = std::move(icon), .loading = std::move(loading)});
}

ButtonRef::ButtonRef() : state_(std::make_shared<detail::ButtonRefState>()) {}

bool ButtonRef::focus() const {
    if (!state_) {
        return false;
    }
    state_->ensure_owner();
    const auto callback = state_->focus;
    return callback && callback();
}

bool ButtonRef::blur() const {
    if (!state_) {
        return false;
    }
    state_->ensure_owner();
    const auto callback = state_->blur;
    return callback && callback();
}

bool ButtonRef::bound() const {
    if (!state_) {
        return false;
    }
    state_->ensure_owner();
    return bool(state_->focus);
}

} // namespace ryn
