#include "component/selection_component.hpp"

#include "runtime/layout_style_adapter.hpp"
#include "runtime/prop_connection.hpp"

#include <algorithm>
#include <array>
#include <cmath>
#include <numbers>
#include <stdexcept>
#include <utility>

namespace ryn::detail {

namespace {
thread_local SelectionComponentHost* active_selection_host{};
constexpr std::size_t switch_loading_segments = 8;
constexpr std::size_t switch_layer_count = 2 + switch_loading_segments;
constexpr std::size_t checkbox_check_segments = 12;
constexpr std::size_t checkbox_indeterminate_layer = 2 + checkbox_check_segments;
constexpr std::size_t checkbox_layer_count = checkbox_indeterminate_layer + 1;

std::array<float, 4> channels(Color color) noexcept {
    return {color.red(), color.green(), color.blue(), color.alpha()};
}

float rounded_radius(runtime::Rect rect, float radius) noexcept {
    const float side = std::min(rect.width, rect.height);
    return side > 0.0F ? std::clamp(radius / side, 0.0F, 0.5F) : 0.0F;
}

void set_geometry(graphics::QuadInstance& quad, runtime::Rect rect,
    runtime::Size viewport, float radius, runtime::Point translation) {
    if (viewport.width <= 0.0F || viewport.height <= 0.0F) return;
    quad.clip_rect = {
        -1.0F + 2.0F * rect.x / viewport.width,
        1.0F - 2.0F * rect.y / viewport.height,
        2.0F * rect.width / viewport.width,
        -2.0F * rect.height / viewport.height,
    };
    quad.corner_radius = rounded_radius(rect, radius);
    quad.translation = {
        2.0F * translation.x / viewport.width,
        -2.0F * translation.y / viewport.height,
    };
}

void set_material(graphics::QuadInstance& quad, Color color, float opacity = 1.0F) {
    quad.color = channels(color);
    quad.opacity = opacity;
}

void validate(SwitchSize size) {
    if (size != SwitchSize::Middle && size != SwitchSize::Small)
        throw std::invalid_argument("Switch size must be Middle or Small");
}

struct SelectionTokens {
    float switch_height{};
    float switch_width{};
    float handle_size{};
    float track_padding{};
    float checkbox_size{};
    float indicator_size{};
    float label_gap{};
    Color on, on_hover, on_active, off, off_hover, off_active, handle, checkmark, box_background;
    Color box_border, disabled_background, disabled_foreground, focus;
};

SelectionTokens resolve_tokens(const ThemeSnapshot& theme, SwitchSize size) {
    const auto& map = theme.map();
    const auto& alias = theme.alias();
    const auto& switch_token = theme.switch_token();
    return {
        size == SwitchSize::Small ? switch_token.track_height_small : switch_token.track_height,
        size == SwitchSize::Small ? switch_token.track_min_width_small : switch_token.track_min_width,
        size == SwitchSize::Small ? switch_token.handle_size_small : switch_token.handle_size,
        switch_token.track_padding,
        map.control_height / 2.0F,
        map.font_size_large / 2.0F,
        map.size_xs,
        map.color_primary,
        map.color_primary_hover,
        map.color_primary_active,
        // Ant 6.6.5 Switch track uses colorTextQuaternary/Tertiary.
        Color(map.color_text_base.red(), map.color_text_base.green(),
            map.color_text_base.blue(), 0.25F),
        Color(map.color_text_base.red(), map.color_text_base.green(),
            map.color_text_base.blue(), 0.45F),
        Color(map.color_text_base.red(), map.color_text_base.green(),
            map.color_text_base.blue(), 0.45F),
        switch_token.handle_background,
        Color::rgba8(255, 255, 255),
        alias.color_background_container,
        alias.color_border,
        alias.color_background_container_disabled,
        alias.color_text_disabled,
        alias.color_focus_outline,
    };
}
} // namespace

struct SelectionState final {
    runtime::ComponentId component;
    runtime::NodeId node;
    runtime::NodeId spacer;
    input::InteractionId interaction;
    runtime::SceneFragmentId fragment;
    component::RetainedSurfaceId surface;
    std::vector<graphics::QuadInstance> visuals;
    component::RetainedSurfaceEffects effects;
    bool checkbox{};
    bool controlled{};
    bool checked{};
    bool indeterminate{};
    bool disabled{};
    bool loading{};
    bool hovered{};
    SwitchSize size{SwitchSize::Middle};
    float presented_checked{};
    animation::AnimationScopeId animation_scope;
    animation::AnimationTargetId handle_target;
    animation::AnimationId handle_animation;
    animation::AnimationTargetId spinner_target;
    animation::AnimationId spinner_animation;
    float spinner_phase{};
    float layout_width{-1.0F};
    float layout_height{-1.0F};
    float layout_gap{-1.0F};
    input::PressableBehavior press;
    input::FocusPresentation focus;
    std::function<void(bool)> on_change;
    Signal<runtime::SemanticForeground> label_foreground{{0.0F, 0.0F, 0.0F, 1.0F}};
    Signal<runtime::SemanticTypography> label_typography{runtime::SemanticTypography{}};
    theme_runtime::Subscription theme_subscription;
};

SelectionComponentHost::SelectionComponentHost(WindowComponentServices& services)
    : services_(&services) {
    services_->attach(*this);
}

SelectionComponentHost::~SelectionComponentHost() {
    while (!mounted_.empty()) {
        const auto id = mounted_.back().component;
        if (!services_->destroy(id)) mounted_.pop_back();
    }
    services_->detach(*this);
}

void SelectionComponentHost::mount(const Content& content) { services_->mount(content); }

void* SelectionComponentHost::begin_mount() noexcept {
    return std::exchange(active_selection_host, this);
}

void SelectionComponentHost::end_mount(void* previous) noexcept {
    active_selection_host = static_cast<SelectionComponentHost*>(previous);
}

void SelectionComponentHost::on_destroy() noexcept {
    std::erase_if(mounted_, [this](const auto& item) {
        return !services_->components().contains(item.component);
    });
}

void SelectionComponentHost::on_dispose() noexcept { mounted_.clear(); }

void SelectionComponentHost::synchronize_auxiliary_motion() {
    for (const auto& item : mounted_) {
        auto* state = find(item.component);
        if (!state || state->checkbox) continue;
        synchronize_spinner(*state);
        const auto& theme = services_->components().theme_scope(item.component)->snapshot();
        if (state->handle_animation.valid()
            && !animation::resolve_motion_policy(theme, services_->motion_preference()).enabled()) {
            static_cast<void>(services_->animations().finish(state->handle_animation));
            state->handle_animation = {};
            state->presented_checked = state->checked ? 1.0F : 0.0F;
            if (viewport_.width > 0.0F && viewport_.height > 0.0F)
                update_geometry(*state, viewport_);
        }
    }
}

SelectionState* SelectionComponentHost::find(runtime::ComponentId id) noexcept {
    return services_->components().state<SelectionState>(id);
}

const SelectionState* SelectionComponentHost::find(runtime::ComponentId id) const noexcept {
    return services_->components().state<SelectionState>(id);
}

SelectionSnapshot SelectionComponentHost::snapshot(runtime::ComponentId id) const {
    const auto* state = find(id);
    if (!state) throw std::out_of_range("Selection component is stale or invalid");
    return {state->checkbox, state->checked, state->indeterminate,
        state->disabled, state->loading, state->hovered,
        state->press.pressed(), state->focus, state->size};
}

std::optional<input::InteractionId> SelectionComponentHost::parent_interaction(
    runtime::ComponentId component) const {
    for (auto ancestor = services_->components().parent(component); ancestor;
        ancestor = services_->components().parent(*ancestor)) {
        for (const auto interaction : services_->interactions().declaration_order()) {
            const auto* record = services_->interactions().find(interaction);
            if (record && record->component == *ancestor) return interaction;
        }
    }
    return {};
}

void SelectionComponentHost::attach_interaction(SelectionState& state) {
    const auto id = state.component;
    state.interaction = services_->interactions().create({id, state.node,
        parent_interaction(id), !state.disabled, true, {}});
    input::InteractionHandlers pointer_handlers;
    pointer_handlers.target = [this, id](input::PointerDispatchContext& event) {
        handle_pointer(id, event);
    };
    static_cast<void>(services_->interactions().set_handlers(
        state.interaction, std::move(pointer_handlers)));
    input::FocusHandlers focus_handlers;
    focus_handlers.state_changed = [this, id](input::FocusPresentation focus) {
        apply_focus(id, focus);
    };
    focus_handlers.activation_allowed = [this, id] { return activation_allowed(id); };
    focus_handlers.activate = [this, id] { activate(id); };
    // Selection controls use Space only; Enter is a Button action.
    focus_handlers.text_edit = [](const input::KeyboardInputEvent& event) {
        return event.key == input::Key::enter;
    };
    static_cast<void>(services_->interactions().set_focus_handlers(
        state.interaction, std::move(focus_handlers)));
}

bool SelectionComponentHost::activation_allowed(runtime::ComponentId id) const noexcept {
    const auto* state = find(id);
    return state && !state->disabled && !state->loading;
}

void SelectionComponentHost::activate(runtime::ComponentId id) {
    auto* state = find(id);
    if (!state || state->disabled || state->loading) return;
    const bool next = !state->checked;
    auto callback = state->on_change;
    if (!state->controlled) {
        state->checked = next;
        if (!state->checkbox) retarget_handle(*state);
        update_visuals(*state);
    }
    if (callback) callback(next);
}

void SelectionComponentHost::handle_pointer(runtime::ComponentId id,
    input::PointerDispatchContext& event) {
    auto* state = find(id);
    if (!state) return;
    if (event.kind() == input::PointerEventKind::enter) {
        if (!state->disabled && !state->hovered) {
            state->hovered = true;
            update_visuals(*state);
        }
        return;
    }
    if (event.kind() == input::PointerEventKind::leave) {
        if (state->hovered) {
            state->hovered = false;
            update_visuals(*state);
        }
        return;
    }
    const auto result = state->press.dispatch(event, state->interaction,
        activation_allowed(id));
    state = find(id);
    if (!state) return;
    if (result.pressed_changed) update_visuals(*state);
    if (result.activate) activate(id);
}

void SelectionComponentHost::apply_checked(runtime::ComponentId id, bool value) {
    if (auto* state = find(id); state && state->checked != value) {
        state->checked = value;
        if (!state->checkbox) retarget_handle(*state);
        update_visuals(*state);
    }
}

void SelectionComponentHost::retarget_handle(SelectionState& state) {
    const auto& theme = services_->components().theme_scope(state.component)->snapshot();
    const auto policy = animation::resolve_motion_policy(
        theme, services_->motion_preference());
    const float target = state.checked ? 1.0F : 0.0F;
    if (!policy.enabled() || !state.handle_target.valid()) {
        if (state.handle_animation.valid()) {
            static_cast<void>(services_->animations().finish(state.handle_animation));
            state.handle_animation = {};
        }
        state.presented_checked = target;
        return;
    }
    const auto spec = policy.transition(animation::MotionDurationToken::mid,
        animation::MotionEasingToken::ease_in_out);
    if (state.handle_animation.valid()
        && services_->animations().contains(state.handle_animation)
        && services_->animations().retarget(state.handle_animation,
            target, spec, services_->animation_time())) return;
    state.handle_animation = services_->animations().play(state.handle_target,
        state.presented_checked, target, spec, services_->animation_time());
}

void SelectionComponentHost::synchronize_spinner(SelectionState& state) {
    const auto& theme = services_->components().theme_scope(state.component)->snapshot();
    const bool animate = state.loading && animation::resolve_motion_policy(
        theme, services_->motion_preference()).enabled();
    if (!animate) {
        if (services_->animations().contains(state.spinner_animation))
            static_cast<void>(services_->animations().cancel(
                state.spinner_animation, services_->animation_time()));
        state.spinner_animation = {};
        state.spinner_phase = 0.0F;
        return;
    }
    if (services_->animations().contains(state.spinner_animation)) return;
    state.spinner_phase -= std::floor(state.spinner_phase);
    state.spinner_animation = services_->animations().play(state.spinner_target,
        state.spinner_phase, state.spinner_phase + 1.0F,
        {{}, animation::AnimationDuration::microseconds(800'000),
            animation::Easing::linear()}, services_->animation_time());
}

void SelectionComponentHost::apply(animation::AnimationId,
    animation::AnimationTargetId target, const animation::AnimationValue& value,
    animation::AnimationDirtyDomain) {
    for (const auto& item : mounted_) {
        auto* state = find(item.component);
        if (!state) continue;
        if (state->spinner_target == target) {
            state->spinner_phase = std::get<float>(value);
            update_visuals(*state);
            return;
        }
        if (state->handle_target != target) continue;
        state->presented_checked = std::get<float>(value);
        if (viewport_.width > 0.0F && viewport_.height > 0.0F)
            update_geometry(*state, viewport_);
        services_->dirty().invalidate(state->node, runtime::DirtyFlags::Geometry);
        return;
    }
}

void SelectionComponentHost::completed(animation::AnimationId animation,
    animation::AnimationTargetId target) {
    for (const auto& item : mounted_) {
        auto* state = find(item.component);
        if (!state) continue;
        if (state->spinner_target == target && state->spinner_animation == animation) {
            state->spinner_animation = {};
            synchronize_spinner(*state);
            return;
        }
        if (state->handle_target == target && state->handle_animation == animation) {
            state->handle_animation = {};
            return;
        }
    }
}

void SelectionComponentHost::apply_disabled(runtime::ComponentId id, bool value) {
    auto* state = find(id);
    if (!state || state->disabled == value) return;
    state->disabled = value;
    if (value) {
        state->hovered = false;
        static_cast<void>(state->press.reset());
        services_->pointer().cancel_interaction(state->interaction);
        state = find(id);
        if (!state) return;
    }
    static_cast<void>(services_->interactions().set_eligible(state->interaction, !value));
    services_->focus().synchronize();
    update_visuals(*state);
}

void SelectionComponentHost::apply_loading(runtime::ComponentId id, bool value) {
    auto* state = find(id);
    if (!state || state->loading == value) return;
    state->loading = value;
    if (value) {
        static_cast<void>(state->press.reset());
        services_->pointer().cancel_pointer_interaction(state->interaction);
        state = find(id);
        if (!state) return;
    }
    synchronize_spinner(*state);
    update_visuals(*state);
}

void SelectionComponentHost::apply_indeterminate(runtime::ComponentId id, bool value) {
    if (auto* state = find(id); state && state->indeterminate != value) {
        state->indeterminate = value;
        update_visuals(*state);
    }
}

void SelectionComponentHost::apply_size(runtime::ComponentId id, SwitchSize value) {
    validate(value);
    if (auto* state = find(id); state && state->size != value) {
        state->size = value;
        update_layout(*state);
        update_visuals(*state);
    }
}

void SelectionComponentHost::apply_focus(runtime::ComponentId id,
    input::FocusPresentation value) {
    auto* state = find(id);
    if (!state || state->focus == value) return;
    state->focus = value;
    if (!value.focused && state->press.reset()) {
        services_->pointer().cancel_pointer_interaction(state->interaction);
        state = find(id);
        if (!state) return;
    }
    update_visuals(*state);
}

void SelectionComponentHost::update_layout(SelectionState& state) {
    const auto& theme = services_->components().theme_scope(state.component)->snapshot();
    const auto token = resolve_tokens(theme, state.size);
    if (state.checkbox) {
        if (state.layout_height == token.checkbox_size
            && state.layout_gap == token.label_gap) return;
        layout::FlexLayout model;
        model.direction = layout::FlexDirection::horizontal;
        model.main_gap = token.label_gap;
        model.align = layout::FlexAlign::center;
        services_->layout().set_layout(state.node, model);
        if (state.spacer.valid()) services_->layout().set_layout(state.spacer,
            layout::LeafLayout{{token.checkbox_size, token.checkbox_size}});
        state.layout_height = token.checkbox_size;
        state.layout_gap = token.label_gap;
    } else {
        if (state.layout_width == token.switch_width
            && state.layout_height == token.switch_height) return;
        services_->layout().set_layout(state.node,
            layout::LeafLayout{{token.switch_width, token.switch_height}});
        state.layout_width = token.switch_width;
        state.layout_height = token.switch_height;
    }
    services_->dirty().invalidate(state.node,
        runtime::DirtyFlags::Measure | runtime::DirtyFlags::Layout
            | runtime::DirtyFlags::Geometry | runtime::DirtyFlags::HitTest);
}

void SelectionComponentHost::update_visuals(SelectionState& state) {
    const auto& theme = services_->components().theme_scope(state.component)->snapshot();
    const auto token = resolve_tokens(theme, state.size);
    const bool active = state.press.pressed() || state.focus.keyboard_pressed;
    const bool faded = state.disabled || state.loading;
    const float opacity = faded ? 0.65F : 1.0F;
    if (!state.checkbox) {
        const Color track = state.checked
            ? (active ? token.on_active : state.hovered ? token.on_hover : token.on)
            : (active ? token.off_active : state.hovered ? token.off_hover : token.off);
        set_material(state.visuals[0], track, opacity);
        set_material(state.visuals[1], token.handle, opacity);
        for (std::size_t segment = 0; segment < switch_loading_segments; ++segment) {
            const float angle = 2.0F * std::numbers::pi_v<float>
                * (static_cast<float>(segment) / static_cast<float>(switch_loading_segments)
                    - (state.spinner_phase - std::floor(state.spinner_phase)));
            const float wave = 0.5F + 0.5F * std::cos(angle);
            set_material(state.visuals[2 + segment], state.checked ? token.on : token.off,
                state.loading ? 0.18F + 0.82F * wave * wave : 0.0F);
        }
    } else {
        const bool mixed = state.indeterminate;
        const Color border = state.disabled ? token.box_border
            : state.checked && !mixed ? token.on
            : state.hovered ? token.on : token.box_border;
        const Color fill = state.disabled ? token.disabled_background
            : state.checked && !mixed ? (state.hovered ? token.on_hover : token.on)
            : token.box_background;
        set_material(state.visuals[0], border);
        set_material(state.visuals[1], fill);
        for (std::size_t segment = 0; segment < checkbox_check_segments; ++segment) {
            set_material(state.visuals[2 + segment],
                state.disabled ? token.disabled_foreground : token.checkmark,
                state.checked && !mixed ? 1.0F : 0.0F);
        }
        set_material(state.visuals[checkbox_indeterminate_layer],
            state.disabled ? token.disabled_foreground : token.on,
            mixed ? 1.0F : 0.0F);
    }
    state.effects.focus_color = token.focus;
    state.effects.focus_opacity = state.focus.focus_visible && !state.disabled ? 1.0F : 0.0F;
    state.effects.focus_width = state.checkbox ? 2.0F : 2.0F;
    state.effects.focus_offset = state.checkbox ? 0.0F : 2.0F;
    if (state.surface.valid()) {
        const auto visual_changes = services_->surfaces().update_surface(state.surface, state.visuals);
        const auto effect_changes = services_->surfaces().update_effects(state.surface, state.effects);
        if (visual_changes || effect_changes) {
            services_->dirty().invalidate(state.node, runtime::DirtyFlags::Material);
        }
    }
    const auto label_color = state.disabled
        ? token.disabled_foreground : theme.alias().color_text;
    static_cast<void>(state.label_foreground.set(channels(label_color)));
    static_cast<void>(state.label_typography.set({theme.text().font_family,
        theme.text().font_weight, theme.text().font_size, theme.text().line_height}));
}

void SelectionComponentHost::update_geometry(SelectionState& state,
    runtime::Size viewport) {
    const auto& node = services_->nodes().require(state.node);
    const auto& theme = services_->components().theme_scope(state.component)->snapshot();
    const auto token = resolve_tokens(theme, state.size);
    const auto rect = node.bounds;
    auto next = state.visuals;
    if (!state.checkbox) {
        set_geometry(next[0], rect, viewport, rect.height / 2.0F, node.translation);
        const float x = rect.x + token.track_padding
            + std::clamp(state.presented_checked, 0.0F, 1.0F)
                * (rect.width - token.handle_size - 2.0F * token.track_padding);
        const runtime::Rect handle{x, rect.y + (rect.height - token.handle_size) / 2.0F,
            token.handle_size, token.handle_size};
        set_geometry(next[1], handle, viewport, token.handle_size / 2.0F, node.translation);
        const float dot = std::max(1.0F, token.handle_size * 0.13F);
        const float orbit = token.handle_size * 0.27F;
        const float center_x = x + token.handle_size / 2.0F;
        const float center_y = rect.y + rect.height / 2.0F;
        for (std::size_t segment = 0; segment < switch_loading_segments; ++segment) {
            const float angle = 2.0F * std::numbers::pi_v<float>
                * static_cast<float>(segment) / static_cast<float>(switch_loading_segments);
            set_geometry(next[2 + segment],
                {center_x + std::sin(angle) * orbit - dot / 2.0F,
                    center_y - std::cos(angle) * orbit - dot / 2.0F, dot, dot},
                viewport, dot / 2.0F, node.translation);
        }
    } else {
        const runtime::Rect box{rect.x,
            rect.y + (rect.height - token.checkbox_size) / 2.0F,
            token.checkbox_size, token.checkbox_size};
        set_geometry(next[0], box, viewport, theme.map().border_radius_small,
            node.translation);
        set_geometry(next[1], {box.x + 1.0F, box.y + 1.0F,
            std::max(0.0F, box.width - 2.0F), std::max(0.0F, box.height - 2.0F)},
            viewport, std::max(0.0F, theme.map().border_radius_small - 1.0F), node.translation);
        const float stroke = std::max(1.5F, box.width * 0.15F);
        for (std::size_t segment = 0; segment < checkbox_check_segments; ++segment) {
            const float progress = static_cast<float>(segment)
                / static_cast<float>(checkbox_check_segments - 1);
            const float x = progress < 0.4F
                ? 0.22F + 0.20F * (progress / 0.4F)
                : 0.42F + 0.37F * ((progress - 0.4F) / 0.6F);
            const float y = progress < 0.4F
                ? 0.52F + 0.18F * (progress / 0.4F)
                : 0.70F - 0.40F * ((progress - 0.4F) / 0.6F);
            set_geometry(next[2 + segment],
                {box.x + box.width * x - stroke / 2.0F,
                    box.y + box.height * y - stroke / 2.0F, stroke, stroke},
                viewport, stroke / 2.0F, node.translation);
        }
        const float side = token.indicator_size;
        set_geometry(next[checkbox_indeterminate_layer],
            {box.x + (box.width - side) / 2.0F,
            box.y + (box.height - side) / 2.0F, side, side},
            viewport, 0.0F, node.translation);
    }
    if (next != state.visuals) {
        state.visuals = next;
        static_cast<void>(services_->surfaces().update_surface(state.surface, state.visuals));
    }
    auto effects = state.effects;
    effects.shape = {state.checkbox
        ? runtime::Rect{rect.x, rect.y + (rect.height - token.checkbox_size) / 2.0F,
            token.checkbox_size, token.checkbox_size} : rect,
        state.checkbox ? theme.map().border_radius_small : rect.height / 2.0F};
    effects.translation = node.translation;
    if (effects != state.effects) {
        state.effects = effects;
        static_cast<void>(services_->surfaces().update_effects(state.surface, state.effects));
    }
}

void SelectionComponentHost::synchronize_auxiliary_geometry(runtime::Size viewport,
    runtime::Rect) {
    viewport_ = viewport;
    for (const auto& item : mounted_) {
        if (auto* state = find(item.component)) update_geometry(*state, viewport);
    }
}

struct SwitchPropsAccess {
    static void mount(const SwitchProps& props) {
        if (!active_selection_host)
            throw std::logic_error("Switch requires an active SelectionComponentHost");
        auto& host = *active_selection_host;
        if (props.checked_ && props.default_checked_)
            throw std::invalid_argument("Switch checked and defaultChecked are mutually exclusive");
        const auto size = read_prop(props.size_);
        validate(size);
        auto& build = runtime::require_component_build_context();
        const auto component = build.mount_component<SelectionState>();
        auto& state = build.state<SelectionState>(component);
        state.component = component;
        state.node = build.root(component);
        state.controlled = props.checked_.has_value();
        state.checked = props.checked_ ? read_prop(*props.checked_)
            : props.default_checked_.value_or(false);
        state.presented_checked = state.checked ? 1.0F : 0.0F;
        state.disabled = read_prop(props.disabled_);
        state.loading = read_prop(props.loading_);
        state.size = size;
        state.visuals.resize(switch_layer_count);
        state.on_change = props.on_change_;
        build.on_resource_cleanup(component, [&host, component] {
            if (auto* current = host.find(component)) {
                host.services_->pointer().cancel_interaction(current->interaction);
                if (current->animation_scope.valid())
                    static_cast<void>(host.services_->animations().dispose_scope(
                        current->animation_scope));
                host.services_->focus().cancel_interaction(current->interaction);
                static_cast<void>(host.services_->interactions().remove(current->interaction));
                static_cast<void>(host.services_->surfaces().destroy(current->surface));
                static_cast<void>(host.services_->layout().remove_layout(current->node));
            }
        });
        host.update_layout(state);
        runtime::connect_layout_style(build.scope(component), props.layout_,
            state.node, host.services_->nodes(), host.services_->dirty());
        state.fragment = build.register_scene_fragment(component,
            runtime::SceneFragmentPlacement::before_children);
        host.attach_interaction(state);
        host.update_visuals(state);
        state.surface = host.services_->surfaces().create_surface(component, state.node,
            state.fragment, state.visuals, state.effects, state.interaction);
        state.animation_scope = host.services_->animations().create_scope();
        state.handle_target = host.services_->animations().register_target(
            state.animation_scope, host, animation::AnimationValueKind::scalar,
            animation::AnimationDirtyDomain::geometry);
        state.spinner_target = host.services_->animations().register_target(
            state.animation_scope, host, animation::AnimationValueKind::scalar,
            animation::AnimationDirtyDomain::material | animation::AnimationDirtyDomain::animation);
        host.synchronize_spinner(state);
        auto& scope = build.scope(component);
        if (props.checked_) static_cast<void>(connect_prop(scope, *props.checked_,
            [&host, component](bool value) { host.apply_checked(component, value); }));
        static_cast<void>(connect_prop(scope, props.disabled_,
            [&host, component](bool value) { host.apply_disabled(component, value); }));
        static_cast<void>(connect_prop(scope, props.loading_,
            [&host, component](bool value) { host.apply_loading(component, value); }));
        static_cast<void>(connect_prop(scope, props.size_,
            [&host, component](SwitchSize value) { host.apply_size(component, value); }));
        const auto theme = host.services_->components().theme_scope(component);
        state.theme_subscription = theme->capture([&host, component](theme_runtime::DirtyPhase) {
            if (auto* current = host.find(component)) {
                host.update_layout(*current);
                host.update_visuals(*current);
            }
        }, [theme] {
            static_cast<void>(theme->map());
            static_cast<void>(theme->alias());
            static_cast<void>(theme->switch_geometry());
            static_cast<void>(theme->switch_colors());
        });
        host.mounted_.push_back({component, state.node, state.interaction, state.surface, false});
    }
};

struct CheckboxPropsAccess {
    static void mount(const CheckboxProps& props, const std::optional<CheckboxLabel>& label) {
        if (!active_selection_host)
            throw std::logic_error("Checkbox requires an active SelectionComponentHost");
        auto& host = *active_selection_host;
        if (props.checked_ && props.default_checked_)
            throw std::invalid_argument("Checkbox checked and defaultChecked are mutually exclusive");
        auto& build = runtime::require_component_build_context();
        const auto component = build.mount_component<SelectionState>();
        auto& state = build.state<SelectionState>(component);
        state.component = component;
        state.node = build.root(component);
        state.checkbox = true;
        state.visuals.resize(checkbox_layer_count);
        state.controlled = props.checked_.has_value();
        state.checked = props.checked_ ? read_prop(*props.checked_)
            : props.default_checked_.value_or(false);
        state.indeterminate = read_prop(props.indeterminate_);
        state.disabled = read_prop(props.disabled_);
        state.on_change = props.on_change_;
        build.on_resource_cleanup(component, [&host, component] {
            if (auto* current = host.find(component)) {
                host.services_->pointer().cancel_interaction(current->interaction);
                host.services_->focus().cancel_interaction(current->interaction);
                static_cast<void>(host.services_->interactions().remove(current->interaction));
                static_cast<void>(host.services_->surfaces().destroy(current->surface));
                static_cast<void>(host.services_->layout().remove_layout(current->node));
            }
        });
        host.update_layout(state);
        runtime::connect_layout_style(build.scope(component), props.layout_,
            state.node, host.services_->nodes(), host.services_->dirty());
        state.fragment = build.register_scene_fragment(component,
            runtime::SceneFragmentPlacement::before_children);
        host.attach_interaction(state);
        host.update_visuals(state);
        state.surface = host.services_->surfaces().create_surface(component, state.node,
            state.fragment, state.visuals, state.effects, state.interaction);
        auto& scope = build.scope(component);
        if (props.checked_) static_cast<void>(connect_prop(scope, *props.checked_,
            [&host, component](bool value) { host.apply_checked(component, value); }));
        static_cast<void>(connect_prop(scope, props.disabled_,
            [&host, component](bool value) { host.apply_disabled(component, value); }));
        static_cast<void>(connect_prop(scope, props.indeterminate_,
            [&host, component](bool value) { host.apply_indeterminate(component, value); }));
        const auto theme = host.services_->components().theme_scope(component);
        state.theme_subscription = theme->capture([&host, component](theme_runtime::DirtyPhase) {
            if (auto* current = host.find(component)) {
                host.update_layout(*current);
                host.update_visuals(*current);
            }
        }, [theme] {
            static_cast<void>(theme->map());
            static_cast<void>(theme->alias());
            static_cast<void>(theme->text());
        });
        build.mount_slot(component, Content{[&] {
            auto& nested = runtime::require_component_build_context();
            const auto spacer = nested.mount_component<int>(0);
            const auto node = nested.root(spacer);
            state.spacer = node;
            host.services_->layout().set_layout(node,
                layout::LeafLayout{{host.services_->components().theme_scope(component)
                    ->snapshot().map().control_height / 2.0F,
                    host.services_->components().theme_scope(component)
                    ->snapshot().map().control_height / 2.0F}});
            nested.on_resource_cleanup(spacer, [layout = &host.services_->layout(), node] {
                static_cast<void>(layout->remove_layout(node));
            });
            if (label) nested.mount_slot_with_semantic_text_style(component, *label,
                Prop<runtime::SemanticForeground>{state.label_foreground},
                Prop<runtime::SemanticTypography>{state.label_typography});
        }});
        host.mounted_.push_back({component, state.node, state.interaction, state.surface, true});
    }
};

} // namespace ryn::detail

namespace ryn {
void Switch(SwitchProps props) { detail::SwitchPropsAccess::mount(props); }
void Checkbox(CheckboxProps props, std::optional<CheckboxLabel> label) {
    detail::CheckboxPropsAccess::mount(props, label);
}
} // namespace ryn
