#include "component/button_component.hpp"

#include "runtime/layout_style_adapter.hpp"
#include "runtime/prop_connection.hpp"

#include <algorithm>
#include <array>
#include <cmath>
#include <limits>
#include <stdexcept>
#include <utility>

namespace ryn::detail {

struct ButtonPropsAccess final {
    [[nodiscard]] static const Prop<ButtonType>& type(
        const ButtonProps& props) noexcept {
        return props.type_;
    }

    [[nodiscard]] static const Prop<ControlSize>& size(
        const ButtonProps& props) noexcept {
        return props.size_;
    }

    [[nodiscard]] static const Prop<bool>& disabled(
        const ButtonProps& props) noexcept {
        return props.disabled_;
    }

    [[nodiscard]] static const Prop<bool>& loading(
        const ButtonProps& props) noexcept {
        return props.loading_;
    }

    [[nodiscard]] static const std::function<void()>& on_click(
        const ButtonProps& props) noexcept {
        return props.on_click_;
    }

    [[nodiscard]] static const LayoutStyle& layout(
        const ButtonProps& props) noexcept {
        return props.layout_;
    }
};

struct ButtonComponentState final {
    ButtonComponentState(
        runtime::SemanticForeground initial_foreground,
        runtime::SemanticTypography initial_typography)
        : foreground(std::move(initial_foreground)),
          typography(initial_typography) {}

    runtime::ComponentId component;
    runtime::NodeId node;
    input::InteractionId interaction;
    component::ButtonSceneId scene;
    runtime::SceneFragmentId fragment;
    ButtonType type{ButtonType::Default};
    ControlSize size{ControlSize::Middle};
    bool disabled{false};
    bool loading{false};
    bool hovered{false};
    bool pointer_pressed{false};
    input::FocusPresentation focus;
    Signal<runtime::SemanticForeground> foreground;
    Signal<runtime::SemanticTypography> typography;
    std::function<void()> on_click;
    component::ButtonVisualData visuals;
    component::ButtonEffectData effects;
    layout::HorizontalContentLayout layout_model;
    theme_runtime::Subscription color_subscription;
    theme_runtime::Subscription effect_subscription;
    theme_runtime::Subscription layout_subscription;
    theme_runtime::Subscription typography_subscription;
};

namespace {

thread_local ButtonComponentHost* active_button_host = nullptr;

class ActiveButtonHostGuard final {
public:
    explicit ActiveButtonHostGuard(ButtonComponentHost& host) noexcept
        : previous_(active_button_host) {
        active_button_host = &host;
    }

    ActiveButtonHostGuard(const ActiveButtonHostGuard&) = delete;
    ActiveButtonHostGuard& operator=(const ActiveButtonHostGuard&) = delete;

    ~ActiveButtonHostGuard() {
        active_button_host = previous_;
    }

private:
    ButtonComponentHost* previous_;
};

void validate(ButtonType type) {
    switch (type) {
    case ButtonType::Default:
    case ButtonType::Primary:
    case ButtonType::Danger:
        return;
    }
    throw std::invalid_argument("ButtonType value is invalid");
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

ResolvedButtonSizeToken size_token(
    const ButtonThemeToken& button,
    ControlSize size) {
    validate(size);
    switch (size) {
    case ControlSize::Small:
        return {
            button.control_height_small,
            button.padding_inline_small,
            button.border_radius_small,
            button.content_font_size_small,
            button.content_line_height_small,
        };
    case ControlSize::Middle:
        return {
            button.control_height,
            button.padding_inline,
            button.border_radius,
            button.content_font_size,
            button.content_line_height,
        };
    case ControlSize::Large:
        return {
            button.control_height_large,
            button.padding_inline_large,
            button.border_radius_large,
            button.content_font_size_large,
            button.content_line_height_large,
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

ResolvedButtonVisualState visual_token(
    const ButtonThemeToken& button,
    const ButtonComponentState& state) {
    if (state.disabled) {
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
    const bool active = !state.loading
        && (state.pointer_pressed || state.focus.keyboard_pressed);
    const bool hovered = !state.loading && !active && state.hovered;
    const Color transparent = Color::rgba8(0, 0, 0, 0);
    switch (state.type) {
    case ButtonType::Default:
        return {
            button.default_background,
            active ? button.default_active_color
                   : hovered ? button.default_hover_color
                             : button.default_border_color,
            active ? button.default_active_color
                   : hovered ? button.default_hover_color
                             : button.default_color,
            button.default_shadow,
        };
    case ButtonType::Primary:
        return {
            active ? button.primary_active_background
                   : hovered ? button.primary_hover_background
                             : button.primary_background,
            transparent,
            button.primary_color,
            button.primary_shadow,
        };
    case ButtonType::Danger:
        return {
            active ? button.danger_active_background
                   : hovered ? button.danger_hover_background
                             : button.danger_background,
            transparent,
            button.danger_color,
            button.danger_shadow,
        };
    }
    throw std::invalid_argument("ButtonType value is invalid");
}

[[nodiscard]] runtime::SemanticForeground channels(Color color) noexcept {
    return {color.red(), color.green(), color.blue(), color.alpha()};
}

runtime::SemanticForeground content_foreground(
    const ButtonThemeToken& button,
    const ButtonComponentState& state) {
    auto foreground = channels(visual_token(button, state).foreground);
    if (state.loading) {
        foreground[3] *= button.loading_opacity;
    }
    return foreground;
}

layout::HorizontalContentLayout content_layout(
    const ButtonThemeToken& button,
    const ButtonComponentState& state) {
    const auto& size = size_token(button, state.size);
    return {
        size.control_height,
        size.padding_inline,
        button.border_width,
        button.icon_gap,
        state.loading,
        button.loading_indicator_size,
    };
}

runtime::SemanticTypography content_typography(
    const ThemeSnapshot& theme,
    const ButtonComponentState& state) {
    const auto size = size_token(theme.button(), state.size);
    return {
        theme.text().font_family,
        theme.text().font_weight,
        size.content_font_size,
        size.content_line_height,
    };
}

bool material_changed(
    const graphics::QuadInstance& left,
    const graphics::QuadInstance& right) noexcept {
    return left.color != right.color || left.opacity != right.opacity;
}

bool geometry_changed(
    const graphics::QuadInstance& left,
    const graphics::QuadInstance& right) noexcept {
    return left.clip_rect != right.clip_rect
        || left.corner_radius != right.corner_radius
        || left.translation != right.translation;
}

std::array<float, 4> clip_rect(
    runtime::Rect pixels,
    runtime::Size viewport) {
    if (!std::isfinite(viewport.width) || !std::isfinite(viewport.height)
            || viewport.width <= 0.0F || viewport.height <= 0.0F) {
        throw std::invalid_argument("Button viewport must be finite and positive");
    }
    return {
        -1.0F + 2.0F * pixels.x / viewport.width,
        1.0F - 2.0F * pixels.y / viewport.height,
        2.0F * pixels.width / viewport.width,
        -2.0F * pixels.height / viewport.height,
    };
}

float normalized_radius(runtime::Rect bounds, float radius) noexcept {
    const float extent = std::min(bounds.width, bounds.height);
    return extent > 0.0F
        ? std::clamp(radius / extent, 0.0F, 0.5F)
        : 0.0F;
}

float logical_radius(runtime::Rect bounds, float radius) noexcept {
    return std::clamp(
        radius,
        0.0F,
        0.5F * std::min(bounds.width, bounds.height));
}

graphics::QuadInstance make_quad(
    runtime::Rect bounds,
    runtime::Size viewport,
    std::array<float, 4> color,
    float opacity,
    float radius,
    runtime::Point translation) {
    return {
        clip_rect(bounds, viewport),
        color,
        opacity,
        normalized_radius(bounds, radius),
        {
            2.0F * translation.x / viewport.width,
            -2.0F * translation.y / viewport.height,
        },
    };
}

} // namespace

ButtonComponentHost::ButtonComponentHost(
    runtime::NodeStore& nodes,
    layout::LayoutEngine& layout,
    runtime::DirtyQueues& dirty,
    TextSceneService& text_scene,
    std::vector<font::FontIdentity> default_font_chain,
    runtime::FrameRequestState& frame_requests)
    : ButtonComponentHost(
          nodes,
          layout,
          dirty,
          text_scene,
          [chain = std::move(default_font_chain)](
              SystemFontFamily,
              std::uint32_t,
              std::uint32_t) { return chain; },
          frame_requests) {}

ButtonComponentHost::ButtonComponentHost(
    runtime::NodeStore& nodes,
    layout::LayoutEngine& layout,
    runtime::DirtyQueues& dirty,
    TextSceneService& text_scene,
    ThemeFontResolver font_resolver,
    runtime::FrameRequestState& frame_requests)
    : nodes_(&nodes),
      layout_(&layout),
      dirty_(&dirty),
      text_(
          nodes,
          layout,
          dirty,
          text_scene,
          std::move(font_resolver)),
      interactions_(text_.components(), nodes),
      hit_test_(interactions_, nodes),
      scene_composer_(text_.components(), interactions_, hit_test_),
      button_scene_(text_.components(), nodes, scene_composer_),
      focus_(interactions_, &frame_requests),
      pointer_(interactions_, hit_test_, &frame_requests, &focus_) {
    text_.attach_component_scene(scene_composer_);
}

ButtonComponentHost::~ButtonComponentHost() {
    dispose();
}

void ButtonComponentHost::mount(const Content& content) {
    const auto mounted_before = mounted_buttons_.size();
    ActiveButtonHostGuard guard(*this);
    try {
        text_.mount(content);
        scene_structure_dirty_ = true;
    } catch (...) {
        mounted_buttons_.resize(mounted_before);
        throw;
    }
}

bool ButtonComponentHost::destroy(runtime::ComponentId id) {
    if (!text_.destroy(id)) {
        return false;
    }
    std::erase_if(mounted_buttons_, [this](const auto& mounted) {
        return !components().contains(mounted.component);
    });
    scene_structure_dirty_ = true;
    return true;
}

void ButtonComponentHost::dispose() noexcept {
    if (!components().active()) {
        return;
    }
    try {
        pointer_.cancel_all();
    } catch (...) {
    }
    text_.dispose();
    mounted_buttons_.clear();
    scene_structure_dirty_ = true;
}

void ButtonComponentHost::set_window_active(bool active) {
    if (!active) {
        pointer_.cancel_all();
    }
    focus_.set_window_active(active);
}

bool ButtonComponentHost::layout_and_synchronize(
    runtime::Size viewport,
    runtime::Rect clip,
    runtime::Point origin,
    float gap) {
    if (!text_.layout_and_synchronize(
            viewport,
            clip,
            origin,
            gap,
            false)) {
        return false;
    }
    for (const auto& mounted : mounted_buttons_) {
        if (auto* state = find_state(mounted.component)) {
            synchronize_geometry(*state, viewport);
        }
    }
    if (button_scene_.compact_effects(
            {0.0F, 0.0F, viewport.width, viewport.height})) {
        scene_structure_dirty_ = true;
    }
    const bool text_fragments_changed = text_.synchronize_scene_fragments(
        [](runtime::ComponentId) {
            return std::optional<input::InteractionId>{};
        });
    if (scene_structure_dirty_ || text_fragments_changed) {
        scene_composer_.rebuild(clip);
        scene_structure_dirty_ = false;
    } else if (text_.layout_performed_last_sync()) {
        for (const auto& mounted : mounted_buttons_) {
            static_cast<void>(hit_test_.refresh_interaction(
                mounted.interaction));
        }
    } else if (!dirty_->hit_test_nodes().empty()) {
        static_cast<void>(hit_test_.refresh(dirty_->hit_test_nodes()));
    }
    focus_.synchronize();
    dirty_->clear();
    return true;
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

component::ComponentSceneComposer&
ButtonComponentHost::scene_composer() noexcept {
    return scene_composer_;
}

component::ButtonSceneService& ButtonComponentHost::button_scene() noexcept {
    return button_scene_;
}

graphics::RoundedEffectStore& ButtonComponentHost::rounded_effects() noexcept {
    return button_scene_.effects();
}

std::span<const MountedButtonComponent>
ButtonComponentHost::mounted_buttons() const noexcept {
    return mounted_buttons_;
}

ButtonComponentSnapshot ButtonComponentHost::snapshot(
    runtime::ComponentId component) const {
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
        state->pointer_pressed,
        state->focus,
    };
}

void ButtonComponentHost::record_mounted_button(
    MountedButtonComponent mounted) {
    mounted_buttons_.push_back(std::move(mounted));
    scene_structure_dirty_ = true;
}

ButtonComponentState* ButtonComponentHost::find_state(
    runtime::ComponentId component) noexcept {
    return components().state<ButtonComponentState>(component);
}

const ButtonComponentState* ButtonComponentHost::find_state(
    runtime::ComponentId component) const noexcept {
    return components().state<ButtonComponentState>(component);
}

std::optional<input::InteractionId> ButtonComponentHost::interaction_for(
    runtime::ComponentId component) const {
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

void ButtonComponentHost::apply_type(
    runtime::ComponentId component,
    ButtonType type) {
    validate(type);
    auto* state = find_state(component);
    if (state == nullptr || state->type == type) {
        return;
    }
    state->type = type;
    update_visuals(*state);
}

void ButtonComponentHost::apply_size(
    runtime::ComponentId component,
    ControlSize size) {
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

void ButtonComponentHost::apply_disabled(
    runtime::ComponentId component,
    bool disabled) {
    auto* state = find_state(component);
    if (state == nullptr || state->disabled == disabled) {
        return;
    }
    state->disabled = disabled;
    if (disabled) {
        state->hovered = false;
        state->pointer_pressed = false;
        pointer_.cancel_interaction(state->interaction);
    }
    static_cast<void>(interactions_.set_eligible(
        state->interaction,
        !disabled));
    focus_.synchronize();
    update_visuals(*state);
}

void ButtonComponentHost::apply_loading(
    runtime::ComponentId component,
    bool loading) {
    auto* state = find_state(component);
    if (state == nullptr || state->loading == loading) {
        return;
    }
    state->loading = loading;
    if (loading) {
        state->pointer_pressed = false;
        pointer_.cancel_pointer_interaction(state->interaction);
        state = find_state(component);
        if (state == nullptr) {
            return;
        }
    }
    update_layout(*state);
    update_visuals(*state);
}

void ButtonComponentHost::apply_focus(
    runtime::ComponentId component,
    input::FocusPresentation focus) {
    auto* state = find_state(component);
    if (state == nullptr || state->focus == focus) {
        return;
    }
    state->focus = focus;
    update_visuals(*state);
}

void ButtonComponentHost::handle_pointer(
    runtime::ComponentId component,
    input::PointerDispatchContext& event) {
    auto* state = find_state(component);
    if (state == nullptr) {
        return;
    }
    const bool primary = event.event().button == input::PointerButton::primary;
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
        if (primary && activation_allowed(component)) {
            state->pointer_pressed = true;
            update_visuals(*state);
            static_cast<void>(event.capture_pointer());
        }
        return;
    case input::PointerEventKind::up: {
        if (!primary) {
            return;
        }
        const bool should_activate = state->pointer_pressed
            && event.press_origin() == state->interaction
            && event.actual_hit_target() == state->interaction
            && activation_allowed(component);
        state->pointer_pressed = false;
        static_cast<void>(event.release_pointer_capture());
        update_visuals(*state);
        if (should_activate) {
            activate(component);
        }
        return;
    }
    case input::PointerEventKind::cancel:
        if (state->pointer_pressed) {
            state->pointer_pressed = false;
            update_visuals(*state);
        }
        static_cast<void>(event.release_pointer_capture());
        return;
    case input::PointerEventKind::move:
        return;
    }
}

bool ButtonComponentHost::activation_allowed(
    runtime::ComponentId component) const noexcept {
    const auto* state = find_state(component);
    return state != nullptr && !state->disabled && !state->loading;
}

void ButtonComponentHost::activate(runtime::ComponentId component) {
    auto* state = find_state(component);
    if (state == nullptr || state->disabled || state->loading) {
        return;
    }
    auto callback = state->on_click;
    if (callback) {
        callback();
    }
}

void ButtonComponentHost::update_visuals(ButtonComponentState& state) {
    const auto& theme = components().theme_scope(state.component)->snapshot();
    const auto& button = theme.button();
    const auto& alias = theme.alias();
    const auto& visual = visual_token(button, state);
    auto next = state.visuals;
    const float layer_opacity = state.loading ? button.loading_opacity : 1.0F;
    next[static_cast<std::size_t>(component::ButtonVisualLayer::border)].color =
        channels(visual.border);
    next[static_cast<std::size_t>(component::ButtonVisualLayer::border)].opacity =
        layer_opacity;
    next[static_cast<std::size_t>(component::ButtonVisualLayer::background)].color =
        channels(visual.background);
    next[static_cast<std::size_t>(component::ButtonVisualLayer::background)].opacity =
        layer_opacity;
    next[static_cast<std::size_t>(component::ButtonVisualLayer::loading_indicator)].color =
        channels(visual.foreground);
    next[static_cast<std::size_t>(component::ButtonVisualLayer::loading_indicator)].opacity =
        state.loading ? button.loading_opacity : 0.0F;

    bool changed_material = false;
    bool changed_geometry = false;
    for (std::size_t index = 0; index < next.size(); ++index) {
        changed_material = changed_material
            || material_changed(state.visuals[index], next[index]);
        changed_geometry = changed_geometry
            || geometry_changed(state.visuals[index], next[index]);
    }
    state.visuals = next;
    auto next_effects = state.effects;
    next_effects.shadows = visual.shadow;
    next_effects.shadow_opacity = state.disabled ? 0.0F : layer_opacity;
    next_effects.focus_width = alias.line_width_focus;
    next_effects.focus_offset = alias.focus_outline_offset;
    next_effects.focus_color = alias.color_focus_outline;
    next_effects.focus_opacity = state.focus.focus_visible && !state.disabled
        ? 1.0F : 0.0F;
    const bool changed_effect = next_effects != state.effects;
    state.effects = std::move(next_effects);
    if (state.scene.valid()) {
        static_cast<void>(button_scene_.update(state.scene, state.visuals));
        if (changed_effect) {
            static_cast<void>(button_scene_.update_effects(state.scene, state.effects));
        }
    }
    if (changed_material) {
        dirty_->invalidate(state.node, runtime::DirtyFlags::Material);
    }
    if (changed_geometry) {
        dirty_->invalidate(state.node, runtime::DirtyFlags::Geometry);
    }
    if (changed_effect) {
        dirty_->invalidate(
            state.node,
            runtime::DirtyFlags::Geometry | runtime::DirtyFlags::Material);
    }
    static_cast<void>(state.foreground.set(content_foreground(button, state)));
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
    dirty_->invalidate(
        state.node,
        runtime::DirtyFlags::Measure
            | runtime::DirtyFlags::Layout
            | runtime::DirtyFlags::Geometry
            | runtime::DirtyFlags::HitTest);
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
}

void ButtonComponentHost::synchronize_geometry(
    ButtonComponentState& state,
    runtime::Size viewport) {
    const auto& theme = components().theme_scope(state.component)->snapshot();
    const auto& button = theme.button();
    const auto size = size_token(button, state.size);
    const auto& node = nodes_->require(state.node);
    const auto& content = layout_->horizontal_content_geometry(state.node);
    const runtime::Rect background_bounds{
        node.bounds.x + button.border_width,
        node.bounds.y + button.border_width,
        std::max(0.0F, node.bounds.width - 2.0F * button.border_width),
        std::max(0.0F, node.bounds.height - 2.0F * button.border_width),
    };
    auto next = state.visuals;
    next[static_cast<std::size_t>(component::ButtonVisualLayer::border)] =
        make_quad(
            node.bounds,
            viewport,
            next[0].color,
            next[0].opacity,
            size.border_radius,
            node.translation);
    next[static_cast<std::size_t>(component::ButtonVisualLayer::background)] =
        make_quad(
            background_bounds,
            viewport,
            next[1].color,
            next[1].opacity,
            std::max(0.0F, size.border_radius - button.border_width),
            node.translation);
    const auto indicator_bounds = content.loading_indicator_bounds.value_or(
        runtime::Rect{});
    next[static_cast<std::size_t>(component::ButtonVisualLayer::loading_indicator)] =
        make_quad(
            indicator_bounds,
            viewport,
            next[2].color,
            next[2].opacity,
            button.loading_indicator_size * 0.5F,
            node.translation);
    state.visuals = next;
    static_cast<void>(button_scene_.update(state.scene, state.visuals));
    auto effects = state.effects;
    effects.shape = {node.bounds, logical_radius(node.bounds, size.border_radius)};
    effects.translation = node.translation;
    if (effects != state.effects) {
        state.effects = std::move(effects);
        static_cast<void>(button_scene_.update_effects(state.scene, state.effects));
    }
}

void mount_button_component(
    const ButtonProps& props,
    const ButtonContent& content) {
    if (active_button_host == nullptr) {
        throw std::logic_error(
            "ryn::Button can only be declared inside an active ButtonComponentHost");
    }
    auto& host = *active_button_host;
    auto& build = runtime::require_component_build_context();
    const auto initial_type = read_prop(ButtonPropsAccess::type(props));
    const auto initial_size = read_prop(ButtonPropsAccess::size(props));
    validate(initial_type);
    validate(initial_size);
    const bool initial_disabled = read_prop(ButtonPropsAccess::disabled(props));
    const bool initial_loading = read_prop(ButtonPropsAccess::loading(props));
    const auto theme_scope = build.theme_scope();
    const auto& theme = theme_scope->snapshot();
    ButtonComponentState initial_state{
        channels(theme.button().default_color),
        {
            theme.text().font_family,
            theme.text().font_weight,
            theme.text().font_size,
            theme.text().line_height,
        }};
    initial_state.type = initial_type;
    initial_state.size = initial_size;
    initial_state.disabled = initial_disabled;
    initial_state.loading = initial_loading;
    initial_state.on_click = ButtonPropsAccess::on_click(props);
    const auto component = build.mount_component<ButtonComponentState>(
        content_foreground(theme.button(), initial_state),
        content_typography(theme, initial_state));
    auto& state = build.state<ButtonComponentState>(component);
    state.component = component;
    state.node = build.root(component);
    state.type = initial_type;
    state.size = initial_size;
    state.disabled = initial_disabled;
    state.loading = initial_loading;
    state.on_click = ButtonPropsAccess::on_click(props);
    state.layout_model = content_layout(theme.button(), state);
    host.layout_->set_layout(state.node, state.layout_model);
    runtime::connect_layout_style(
        build.scope(component),
        ButtonPropsAccess::layout(props),
        state.node,
        *host.nodes_,
        *host.dirty_);

    state.fragment = build.register_scene_fragment(
        component,
        runtime::SceneFragmentPlacement::before_children);
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
    pointer_handlers.target = [&host, component](
        input::PointerDispatchContext& event) {
        host.handle_pointer(component, event);
    };
    static_cast<void>(host.interactions_.set_handlers(
        state.interaction,
        std::move(pointer_handlers)));
    input::FocusHandlers focus_handlers;
    focus_handlers.state_changed = [&host, component](
        input::FocusPresentation focus) {
        host.apply_focus(component, focus);
    };
    focus_handlers.activation_allowed = [&host, component] {
        return host.activation_allowed(component);
    };
    focus_handlers.activate = [&host, component] {
        host.activate(component);
    };
    static_cast<void>(host.interactions_.set_focus_handlers(
        state.interaction,
        std::move(focus_handlers)));

    host.update_visuals(state);
    state.scene = host.button_scene_.create(
        component,
        state.node,
        state.fragment,
        state.interaction,
        state.visuals,
        state.effects);
    build.on_resource_cleanup(component, [
        buttons = &host.button_scene_,
        scene = state.scene] {
        static_cast<void>(buttons->destroy(scene));
    });
    build.on_resource_cleanup(component, [
        pointer = &host.pointer_,
        interactions = &host.interactions_,
        interaction = state.interaction] {
        pointer->cancel_interaction(interaction);
        static_cast<void>(interactions->remove(interaction));
    });

    auto& scope = build.scope(component);
    static_cast<void>(connect_prop(
        scope,
        ButtonPropsAccess::type(props),
        [&host, component](ButtonType type) {
            host.apply_type(component, type);
        }));
    static_cast<void>(connect_prop(
        scope,
        ButtonPropsAccess::size(props),
        [&host, component](ControlSize size) {
            host.apply_size(component, size);
        }));
    static_cast<void>(connect_prop(
        scope,
        ButtonPropsAccess::disabled(props),
        [&host, component](bool disabled) {
            host.apply_disabled(component, disabled);
        }));
    static_cast<void>(connect_prop(
        scope,
        ButtonPropsAccess::loading(props),
        [&host, component](bool loading) {
            host.apply_loading(component, loading);
        }));

    host.subscribe_theme(state);
    build.mount_slot_with_semantic_text_style(
        component,
        content,
        Prop<runtime::SemanticForeground>{state.foreground},
        Prop<runtime::SemanticTypography>{state.typography});
    host.dirty_->invalidate(
        state.node,
        runtime::DirtyFlags::Measure
            | runtime::DirtyFlags::Layout
            | runtime::DirtyFlags::Geometry
            | runtime::DirtyFlags::Material
            | runtime::DirtyFlags::HitTest);
    host.record_mounted_button({
        component,
        state.node,
        state.interaction,
        state.scene,
        state.fragment,
    });
}

} // namespace ryn::detail

namespace ryn {

void Button(ButtonProps props, ButtonContent content) {
    detail::mount_button_component(props, content);
}

} // namespace ryn
