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
    explicit ButtonComponentState(runtime::SemanticForeground initial_foreground)
        : foreground(std::move(initial_foreground)) {}

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
    std::function<void()> on_click;
    component::ButtonVisualData visuals;
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

const DefaultButtonSizeToken& size_token(
    const DefaultButtonToken& button,
    ControlSize size) {
    validate(size);
    switch (size) {
    case ControlSize::Small:
        return button.small;
    case ControlSize::Middle:
        return button.middle;
    case ControlSize::Large:
        return button.large;
    }
    throw std::invalid_argument("ControlSize value is invalid");
}

const DefaultButtonVariantToken& variant_token(
    const DefaultButtonToken& button,
    ButtonType type) {
    validate(type);
    return type == ButtonType::Primary
        ? button.primary_variant
        : button.default_variant;
}

const DefaultButtonVisualStateToken& visual_token(
    const DefaultButtonToken& button,
    const ButtonComponentState& state) {
    if (state.disabled) {
        return button.disabled;
    }
    const auto& variant = variant_token(button, state.type);
    if (state.loading) {
        return variant.normal;
    }
    if (state.pointer_pressed || state.focus.keyboard_pressed) {
        return variant.active;
    }
    if (state.hovered) {
        return variant.hover;
    }
    return variant.normal;
}

runtime::SemanticForeground content_foreground(
    const DefaultButtonToken& button,
    const ButtonComponentState& state) {
    auto foreground = visual_token(button, state).foreground;
    if (state.loading) {
        foreground[3] *= button.loading_opacity;
    }
    return foreground;
}

layout::HorizontalContentLayout content_layout(
    const DefaultButtonToken& button,
    const ButtonComponentState& state) {
    const auto& size = size_token(button, state.size);
    return {
        size.control_height,
        size.padding_inline,
        button.border_width,
        button.content_gap,
        state.loading,
        button.loading_indicator_size,
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

graphics::QuadInstance make_quad(
    runtime::Rect bounds,
    runtime::Size viewport,
    DefaultColor color,
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
    runtime::FrameRequestState& frame_requests,
    const DefaultThemeSnapshot& theme)
    : nodes_(&nodes),
      layout_(&layout),
      dirty_(&dirty),
      theme_(&theme),
      text_(
          nodes,
          layout,
          dirty,
          text_scene,
          std::move(default_font_chain),
          theme),
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
    layout_->set_layout(state->node, content_layout(theme_->button, *state));
    update_visuals(*state);
    dirty_->invalidate(
        state->node,
        runtime::DirtyFlags::Measure
            | runtime::DirtyFlags::Layout
            | runtime::DirtyFlags::Geometry
            | runtime::DirtyFlags::HitTest);
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
    layout_->set_layout(state->node, content_layout(theme_->button, *state));
    update_visuals(*state);
    dirty_->invalidate(
        state->node,
        runtime::DirtyFlags::Measure
            | runtime::DirtyFlags::Layout
            | runtime::DirtyFlags::Geometry
            | runtime::DirtyFlags::HitTest);
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
    const auto& button = theme_->button;
    const auto& visual = visual_token(button, state);
    auto next = state.visuals;
    const float layer_opacity = state.loading ? button.loading_opacity : 1.0F;
    next[static_cast<std::size_t>(component::ButtonVisualLayer::focus_ring)].color =
        button.focus_visible;
    next[static_cast<std::size_t>(component::ButtonVisualLayer::focus_ring)].opacity =
        state.focus.focus_visible && !state.disabled ? 1.0F : 0.0F;
    next[static_cast<std::size_t>(component::ButtonVisualLayer::border)].color =
        visual.border;
    next[static_cast<std::size_t>(component::ButtonVisualLayer::border)].opacity =
        layer_opacity;
    next[static_cast<std::size_t>(component::ButtonVisualLayer::background)].color =
        visual.background;
    next[static_cast<std::size_t>(component::ButtonVisualLayer::background)].opacity =
        layer_opacity;
    next[static_cast<std::size_t>(component::ButtonVisualLayer::loading_indicator)].color =
        visual.foreground;
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
    if (state.scene.valid()) {
        static_cast<void>(button_scene_.update(state.scene, state.visuals));
    }
    if (changed_material) {
        dirty_->invalidate(state.node, runtime::DirtyFlags::Material);
    }
    if (changed_geometry) {
        dirty_->invalidate(state.node, runtime::DirtyFlags::Geometry);
    }
    static_cast<void>(state.foreground.set(content_foreground(button, state)));
}

void ButtonComponentHost::synchronize_geometry(
    ButtonComponentState& state,
    runtime::Size viewport) {
    const auto& button = theme_->button;
    const auto& size = size_token(button, state.size);
    const auto& node = nodes_->require(state.node);
    const auto& content = layout_->horizontal_content_geometry(state.node);
    const float focus_extent = button.focus_ring_width + button.focus_ring_offset;
    const runtime::Rect focus_bounds{
        node.bounds.x - focus_extent,
        node.bounds.y - focus_extent,
        node.bounds.width + 2.0F * focus_extent,
        node.bounds.height + 2.0F * focus_extent,
    };
    const runtime::Rect background_bounds{
        node.bounds.x + button.border_width,
        node.bounds.y + button.border_width,
        std::max(0.0F, node.bounds.width - 2.0F * button.border_width),
        std::max(0.0F, node.bounds.height - 2.0F * button.border_width),
    };
    auto next = state.visuals;
    next[static_cast<std::size_t>(component::ButtonVisualLayer::focus_ring)] =
        make_quad(
            focus_bounds,
            viewport,
            next[0].color,
            next[0].opacity,
            size.border_radius + focus_extent,
            node.translation);
    next[static_cast<std::size_t>(component::ButtonVisualLayer::border)] =
        make_quad(
            node.bounds,
            viewport,
            next[1].color,
            next[1].opacity,
            size.border_radius,
            node.translation);
    next[static_cast<std::size_t>(component::ButtonVisualLayer::background)] =
        make_quad(
            background_bounds,
            viewport,
            next[2].color,
            next[2].opacity,
            std::max(0.0F, size.border_radius - button.border_width),
            node.translation);
    const auto indicator_bounds = content.loading_indicator_bounds.value_or(
        runtime::Rect{});
    next[static_cast<std::size_t>(component::ButtonVisualLayer::loading_indicator)] =
        make_quad(
            indicator_bounds,
            viewport,
            next[3].color,
            next[3].opacity,
            button.loading_indicator_size * 0.5F,
            node.translation);
    state.visuals = next;
    static_cast<void>(button_scene_.update(state.scene, state.visuals));
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
    ButtonComponentState initial_state{
        host.theme_->button.default_variant.normal.foreground};
    initial_state.type = initial_type;
    initial_state.size = initial_size;
    initial_state.disabled = initial_disabled;
    initial_state.loading = initial_loading;
    initial_state.on_click = ButtonPropsAccess::on_click(props);
    const auto component = build.mount_component<ButtonComponentState>(
        content_foreground(host.theme_->button, initial_state));
    auto& state = build.state<ButtonComponentState>(component);
    state.component = component;
    state.node = build.root(component);
    state.type = initial_type;
    state.size = initial_size;
    state.disabled = initial_disabled;
    state.loading = initial_loading;
    state.on_click = ButtonPropsAccess::on_click(props);
    host.layout_->set_layout(
        state.node,
        content_layout(host.theme_->button, state));
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
        state.visuals);
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

    build.mount_slot_with_semantic_foreground(
        component,
        content,
        Prop<runtime::SemanticForeground>{state.foreground});
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
