#include "component/input_component.hpp"
#include "component/input_material_transition.hpp"
#include "component/input_caret_blink.hpp"
#include "runtime/layout_style_adapter.hpp"
#include "runtime/prop_connection.hpp"
#include "theme/input_tokens.hpp"

#include <algorithm>
#include <cmath>
#include <limits>
#include <stdexcept>

namespace ryn::detail {
namespace {
thread_local InputComponentHost* active_input_host{};
constexpr auto text_dirty = runtime::DirtyFlags::Text | runtime::DirtyFlags::Measure
    | runtime::DirtyFlags::Layout | runtime::DirtyFlags::Geometry;
struct InputContainerPresentation {
    runtime::Rect bounds, clip;
    float radius{}, border_width{};
    Color background, border;
    ShadowList shadows;
    std::array<Color, input_shadow_layer_capacity> shadow_colors;
    float shadow_opacity{};
    friend bool operator==(const InputContainerPresentation&, const InputContainerPresentation&) = default;
};
struct InputState {
    MountedInputComponent mounted;
    bool controlled{}, disabled{}, read_only{}, focused{};
    std::size_t hovering_pointers{};
    std::optional<input::PointerIdentity> selecting_pointer;
    runtime::Point last_selection_position;
    ControlSize size{ControlSize::Middle};
    InputStatus status{InputStatus::Default};
    String placeholder;
    std::function<void(String)> on_change, on_submit;
    runtime::NodeId viewport;
    TextSceneId text_scene;
    TextSceneId selected_scene, placeholder_scene;
    runtime::SceneFragmentId text_fragment;
    std::vector<graphics::SceneDrawCommand> text_commands, pending_text_commands;
    runtime::SceneFragmentId container_fragment;
    component::ButtonSceneId selection_surface, overlay_surface;
    // Both shadow kinds retain all slots; changing a typed list never changes topology.
    std::array<graphics::RoundedEffectId, input_effect_layer_count> container_effects;
    std::optional<InputContainerPresentation> container_presentation;
    std::vector<graphics::SceneDrawCommand> container_commands;
    std::optional<runtime::Rect> container_clip;
    runtime::Rect next_container_clip;
    layout::InputContentLayout layout;
    runtime::SemanticTypography typography;
    Signal<runtime::SemanticTypography> slot_typography{runtime::SemanticTypography{}};
    Signal<runtime::SemanticForeground> slot_foreground{runtime::SemanticForeground{0, 0, 0, 1}};
    theme_runtime::Subscription theme_subscription;
    InputLayoutSnapshot geometry;
    InputCaretBlink caret_blink;
    InputDisplayState display;
    std::unique_ptr<InputMaterialTransition> transition;
    text::TextCaretMap carets;
    std::uint64_t measured_value_revision{};
};
struct InputSlotState {};
struct InputVisuals {
    Color background, border, foreground, affix, caret;
    const ShadowList* shadow{};
    bool shadow_visible{};
};
InputVisuals resolve_visuals(const InputState& state, const InputTokenSet& tokens) {
    const auto& colors = tokens.colors;
    const bool error = state.status == InputStatus::Error, warning = state.status == InputStatus::Warning;
    const Color status_color = error ? colors.error_border : warning ? colors.warning_border : colors.border;
    const Color hover_border = error ? colors.error_hover_border : warning ? colors.warning_hover_border : colors.hover_border;
    const Color active_border = error || warning ? status_color : colors.active_border;
    const Color foreground = state.disabled ? colors.disabled_foreground : colors.foreground;
    return {
        state.disabled ? colors.disabled_background : state.focused ? colors.active_background
            : state.hovering_pointers ? colors.hover_background : colors.background,
        state.disabled ? (error || warning ? status_color : colors.disabled_border)
            : state.focused ? active_border : state.hovering_pointers ? hover_border : status_color,
        foreground, state.disabled ? foreground : error || warning ? status_color : foreground,
        error ? colors.error_caret : warning ? colors.warning_caret : colors.caret,
        error ? &tokens.error_active_shadow : warning ? &tokens.warning_active_shadow : &tokens.active_shadow,
        state.focused && !state.disabled,
    };
}
std::array<float, 4> channels(Color color) {
    return {color.red(), color.green(), color.blue(), color.alpha()};
}
InputMaterialValues material_values(const InputState& state, const InputTokenSet& tokens) {
    const auto visual = resolve_visuals(state, tokens);
    InputMaterialValues result;
    result.colors[0] = visual.background; result.colors[1] = visual.border;
    result.colors[2] = visual.foreground; result.colors[3] = visual.affix; result.colors[4] = visual.caret;
    result.colors[5] = tokens.colors.placeholder; result.colors[6] = tokens.colors.selection_background;
    result.colors[7] = tokens.colors.selection_foreground;
    for(std::size_t i = 0; i < input_shadow_layer_capacity; ++i)
        result.colors[8 + i] = i < visual.shadow->size() ? (*visual.shadow)[i].color : Color(0, 0, 0, 0);
    result.shadow_opacity = visual.shadow_visible ? 1.0F : 0.0F;
    return result;
}
runtime::Rect translated_bounds(const runtime::NodeStore& nodes, runtime::NodeId id) {
    auto result = nodes.require(id).bounds;
    for(auto current = std::optional{id}; current; current = nodes.require(*current).parent) {
        result.x += nodes.require(*current).translation.x;
        result.y += nodes.require(*current).translation.y;
    }
    return result;
}
graphics::QuadInstance clipped_quad(runtime::Rect bounds, runtime::Rect clip,
    runtime::Size viewport, std::array<float, 4> color, float opacity) {
    bounds = graphics::intersect_effect_bounds(bounds, clip);
    return {{-1 + 2 * bounds.x / viewport.width, 1 - 2 * bounds.y / viewport.height,
        2 * bounds.width / viewport.width, -2 * bounds.height / viewport.height}, color, opacity};
}
runtime::Rect pixel_clip(runtime::Rect clip, float scale) {
    const float left = std::ceil(clip.x * scale) / scale;
    const float top = std::ceil(clip.y * scale) / scale;
    const float right = std::floor((clip.x + clip.width) * scale) / scale;
    const float bottom = std::floor((clip.y + clip.height) * scale) / scale;
    return {left, top, std::max(0.0F, right - left), std::max(0.0F, bottom - top)};
}
void check(input::TextEditResult result) {
    if(!result) throw std::runtime_error("Input editor update failed");
}
void validate(ControlSize value) {
    if(value != ControlSize::Small && value != ControlSize::Middle && value != ControlSize::Large)
        throw std::invalid_argument("Invalid Input size");
}
void validate(InputStatus value) {
    if(value != InputStatus::Default && value != InputStatus::Warning && value != InputStatus::Error)
        throw std::invalid_argument("Invalid Input status");
}
}

struct InputPropsAccess {
    static void mount(InputComponentHost& owner, const InputProps& props,
        const std::optional<InputPrefix>& prefix, const std::optional<InputSuffix>& suffix) {
        // Resolve and validate before allocating component, editor or interaction identities.
        if(props.value_ && props.default_value_)
            throw std::invalid_argument("Input value and defaultValue are mutually exclusive");
        const auto initial = props.value_ ? read_prop(*props.value_)
            : props.default_value_.value_or(String{});
        const auto size = read_prop(props.size_);
        const auto status = read_prop(props.status_);
        validate(size); validate(status);
        const auto disabled = read_prop(props.disabled_);
        const auto read_only = read_prop(props.read_only_);
        const input::TextEditorLimits limits{props.max_length_ ? read_prop(*props.max_length_)
            : std::numeric_limits<std::size_t>::max()};
        auto& host = *owner.host_;
        auto& build = runtime::require_component_build_context();
        const auto component = build.mount_component<InputState>();
        auto& state = build.state<InputState>(component);
        state.mounted.component = component;
        state.mounted.node = build.root(component);
        state.controlled = props.value_.has_value();
        state.size = size; state.status = status;
        state.disabled = disabled; state.read_only = read_only;
        state.on_change = props.on_change_; state.on_submit = props.on_submit_;
        // Install cleanup before subsequent resource acquisition can fail.
        build.on_resource_cleanup(component, [&owner, component] {
            auto& host = *owner.host_;
            if(auto* current = host.components().state<InputState>(component)) {
                current->transition.reset();
                const auto mounted = current->mounted;
                host.focus().cancel_interaction(mounted.interaction);
                host.pointer().cancel_interaction(mounted.interaction);
                static_cast<void>(host.interactions().remove(mounted.interaction));
                static_cast<void>(owner.editors_.destroy(mounted.editor));
                static_cast<void>(host.button_scene().destroy(current->selection_surface));
                static_cast<void>(host.button_scene().destroy(current->overlay_surface));
                for(const auto effect : current->container_effects) static_cast<void>(host.rounded_effects().remove(effect));
                static_cast<void>(host.scene_composer().remove_fragment(current->container_fragment));
                static_cast<void>(host.scene_composer().remove_fragment(current->text_fragment));
                static_cast<void>(host.text().scene_service().destroy(current->selected_scene));
                static_cast<void>(host.text().scene_service().destroy(current->placeholder_scene));
                static_cast<void>(host.text().scene_service().destroy(current->text_scene));
                static_cast<void>(host.layout().remove_intrinsic_measure(current->viewport));
                static_cast<void>(host.layout().remove_layout(mounted.node));
                std::erase_if(owner.mounted_, [component](const auto& item) { return item.component == component; });
            }
        });
        state.mounted.editor = owner.editors_.create(initial.bytes(), limits);
        owner.editors_.require(state.mounted.editor).set_eligibility(disabled, read_only);
        std::optional<input::InteractionId> parent;
        for(auto ancestor = host.components().parent(component); ancestor && !parent;
            ancestor = host.components().parent(*ancestor)) {
            for(const auto interaction : host.interactions().declaration_order()) {
                if(const auto* record = host.interactions().find(interaction);
                    record && record->component == *ancestor) { parent = interaction; break; }
            }
        }
        state.mounted.interaction = host.interactions().create(
            {component, state.mounted.node, parent, !disabled, true, {}});
        input::InteractionHandlers pointer_handlers;
        pointer_handlers.target = [&owner, component](input::PointerDispatchContext& context) {
            if(auto* current = owner.host_->components().state<InputState>(component)) {
                const auto before = current->hovering_pointers;
                if(context.kind() == input::PointerEventKind::enter) ++current->hovering_pointers;
                else if(context.kind() == input::PointerEventKind::leave && current->hovering_pointers) --current->hovering_pointers;
                if((before == 0) != (current->hovering_pointers == 0))
                    owner.invalidate(component, runtime::DirtyFlags::Material);
                owner.dispatch_pointer(component, context);
            }
        };
        host.interactions().set_handlers(state.mounted.interaction, std::move(pointer_handlers));
        input::FocusHandlers handlers;
        handlers.state_changed = [&owner, component](input::FocusPresentation focus) {
            if(auto* current = owner.host_->components().state<InputState>(component)) {
                const bool was_focused = current->focused;
                current->focused = focus.focused;
                if(!focus.focused) {
                    current->caret_blink.stop();
                    current->selecting_pointer.reset();
                    owner.host_->pointer().cancel_pointer_interaction(current->mounted.interaction);
                    current->hovering_pointers = 0;
                }
                if(focus.focused) static_cast<void>(owner.sessions_.focus(current->mounted.editor));
                else if(was_focused)
                    static_cast<void>(owner.sessions_.blur());
                owner.invalidate(component, runtime::DirtyFlags::Material);
            }
        };
        // Enter submission is routed separately from Button's Space/Enter activation.
        handlers.activation_allowed = [] { return false; };
        handlers.text_edit = [&owner, component](const input::KeyboardInputEvent& event) {
            return owner.dispatch_keyboard(component, event);
        };
        host.interactions().set_focus_handlers(state.mounted.interaction, std::move(handlers));
        state.container_fragment = build.register_scene_fragment(component,
            runtime::SceneFragmentPlacement::before_children);
        for(auto& effect : state.container_effects) effect = host.rounded_effects().add({});
        const auto overlay_fragment = build.register_scene_fragment(component,
            runtime::SceneFragmentPlacement::after_children);
        component::ButtonEffectData no_effects;
        no_effects.focus_enabled = false;
        const std::array<graphics::QuadInstance, 2> empty_overlays{};
        state.overlay_surface = host.button_scene().create_surface(component, state.mounted.node,
            overlay_fragment, empty_overlays, no_effects);
        state.layout.prefix = prefix.has_value(); state.layout.suffix = suffix.has_value();
        build.mount_slot(component, Content{[&] {
            auto& slots = runtime::require_component_build_context();
            const auto make_slot = [&] {
                const auto slot = slots.mount_component<InputSlotState>();
                const auto node = slots.root(slot);
                host.layout().set_layout(node, layout::BoxLayout{});
                slots.on_resource_cleanup(slot, [layout = &host.layout(), node] { static_cast<void>(layout->remove_layout(node)); });
                return slot;
            };
            const auto prefix_component = make_slot();
            if(prefix) slots.mount_slot_with_semantic_text_style(prefix_component, *prefix,
                Prop<runtime::SemanticForeground>{state.slot_foreground},
                Prop<runtime::SemanticTypography>{state.slot_typography});
            const auto editable = make_slot();
            state.viewport = slots.root(editable);
            const auto selection_fragment = slots.register_scene_fragment(editable,
                runtime::SceneFragmentPlacement::before_children);
            const std::array<graphics::QuadInstance, 1> empty_selection{};
            state.selection_surface = host.button_scene().create_surface(editable, state.viewport,
                selection_fragment, empty_selection, no_effects);
            state.text_fragment = slots.register_scene_fragment(editable,
                runtime::SceneFragmentPlacement::before_children);
            host.layout().set_layout(state.viewport, layout::LeafLayout{});
            const auto suffix_component = make_slot();
            if(suffix) slots.mount_slot_with_semantic_text_style(suffix_component, *suffix,
                Prop<runtime::SemanticForeground>{state.slot_foreground},
                Prop<runtime::SemanticTypography>{state.slot_typography});
        }});
        owner.update_theme(component);
        state.transition = std::make_unique<InputMaterialTransition>(host.animations(),
            material_values(state, derive_input_tokens(host.components().theme_scope(component)->snapshot())),
            [&owner, component] { owner.apply_material_transition(component); });
        state.selected_scene = host.text().scene_service().create_view(state.text_scene, state.viewport);
        state.placeholder_scene = host.text().scene_service().create_view(state.text_scene, state.viewport);
        owner.update_text(component);
        host.layout().set_intrinsic_measure(state.viewport, 1,
            [&owner, component](layout::Constraints) {
                auto* current = owner.host_->components().state<InputState>(component);
                if(!current) return runtime::Size{};
                auto& scene = owner.host_->text().scene_service();
                if(!scene.synchronize_measurement(current->text_scene, std::numeric_limits<float>::infinity()))
                    throw std::runtime_error("Input intrinsic text measurement failed");
                const auto& measurement = scene.text_state(current->text_scene).measurement();
                return runtime::Size{measurement.width, measurement.height};
            });
        auto& scope = build.scope(component);
        runtime::connect_layout_style(scope, props.layout_, state.mounted.node, host.nodes(), host.dirty());
        const auto connect = [&]<typename T, typename Apply>(const Prop<T>& prop, Apply apply) {
            static_cast<void>(connect_prop(scope, prop, [&owner, component, apply](T value) {
                if(auto* current = owner.host_->components().state<InputState>(component))
                    apply(owner, *current, std::move(value));
            }));
        };
        if(props.value_) connect(*props.value_, [](auto& owner, auto& current, String value) {
            const auto result = owner.editors_.require(current.mounted.editor).reconcile(value.bytes());
            check(result.edit);
            if(result.edit.value_changed) owner.update_text(current.mounted.component);
        });
        connect(props.placeholder_, [](auto& owner, auto& current, String value) {
            if(current.placeholder == value) return;
            current.placeholder = std::move(value);
            if(owner.editors_.require(current.mounted.editor).value().empty())
                owner.update_text(current.mounted.component);
        });
        connect(props.size_, [](auto& owner, auto& current, ControlSize value) {
            validate(value);
            if(current.size == value) return;
            current.size = value;
            owner.update_theme(current.mounted.component);
        });
        connect(props.status_, [](auto& owner, auto& current, InputStatus value) {
            validate(value);
            if(current.status == value) return;
            current.status = value;
            owner.invalidate(current.mounted.component, runtime::DirtyFlags::Material);
        });
        const auto eligibility = [](auto& owner, auto& current) {
            if(current.disabled || current.read_only) current.caret_blink.stop();
            if(current.disabled && current.focused) {
                current.focused = false;
                current.selecting_pointer.reset();
                current.hovering_pointers = 0;
                static_cast<void>(owner.sessions_.blur());
            }
            owner.editors_.require(current.mounted.editor).set_eligibility(current.disabled, current.read_only);
            owner.host_->interactions().set_eligible(current.mounted.interaction, !current.disabled);
            if(current.disabled) owner.host_->pointer().cancel_interaction(current.mounted.interaction);
            owner.host_->focus().synchronize();
            owner.invalidate(current.mounted.component, runtime::DirtyFlags::Material | runtime::DirtyFlags::HitTest);
        };
        connect(props.disabled_, [eligibility](auto& owner, auto& current, bool value) {
            if(current.disabled == value) return;
            current.disabled = value; eligibility(owner, current);
        });
        connect(props.read_only_, [eligibility](auto& owner, auto& current, bool value) {
            if(current.read_only == value) return;
            current.read_only = value; eligibility(owner, current);
        });
        if(props.max_length_) connect(*props.max_length_, [](auto& owner, auto& current, std::size_t value) {
            auto& editor = owner.editors_.require(current.mounted.editor);
            if(editor.limits().max_scalars == value) return;
            auto limits = editor.limits(); limits.max_scalars = value;
            const auto result = editor.set_limits(limits); check(result);
            if(result.value_changed) owner.update_text(current.mounted.component);
        });
        const auto theme = build.theme_scope();
        state.theme_subscription = theme->capture(
            [&owner, component](theme_runtime::DirtyPhase) { owner.update_theme(component); },
            [theme] { static_cast<void>(theme->text_font_family()); static_cast<void>(theme->text_font_weight());
                static_cast<void>(theme->input_layout_metrics()); static_cast<void>(theme->input_typography());
                static_cast<void>(theme->input_border_radius()); static_cast<void>(theme->input_colors());
                static_cast<void>(theme->input_shadows()); static_cast<void>(theme->motion_unit());
                static_cast<void>(theme->motion_base()); static_cast<void>(theme->motion_enabled()); });
        owner.mounted_.push_back(state.mounted);
        owner.invalidate(component, text_dirty | runtime::DirtyFlags::HitTest);
    }
};

InputComponentHost::InputComponentHost(ButtonComponentHost& host, input::TextInputPlatform& platform,
    input::TextClipboard& clipboard)
    : host_(&host), sessions_(editors_, platform), clipboard_(editors_, clipboard) { host_->attach_auxiliary(*this); }
InputComponentHost::~InputComponentHost() { dispose(); host_->detach_auxiliary(*this); }
void InputComponentHost::mount(const Content& content) {
    struct Restore { InputComponentHost* previous; ~Restore() { active_input_host = previous; } } restore{active_input_host};
    active_input_host = this;
    host_->mount(content);
}
void InputComponentHost::dispose() noexcept {
    while(!mounted_.empty()) {
        const auto id = mounted_.back().component;
        if(!host_->destroy(id)) mounted_.pop_back();
    }
}
void InputComponentHost::set_window_active(bool active) {
    if(!active) for(const auto& mounted : mounted_) {
        if(auto* state = host_->components().state<InputState>(mounted.component)) state->caret_blink.stop();
    }
    static_cast<void>(sessions_.set_window_active(active));
    host_->set_window_active(active);
}
void InputComponentHost::set_display_scale(float scale) {
    if(!std::isfinite(scale) || scale <= 0) throw std::invalid_argument("Input display scale must be positive and finite");
    if(display_scale_ == scale) return;
    display_scale_ = scale;
    for(const auto& mounted : mounted_) {
        if(auto* state = host_->components().state<InputState>(mounted.component)) {
            // The window owner updates its font resolver before this call.
            host_->text().scene_service().set_font_chain(state->text_scene,
                host_->text().resolve_fonts(state->typography));
            invalidate(mounted.component, runtime::DirtyFlags::Geometry);
        }
    }
}
void InputComponentHost::invalidate(runtime::ComponentId component, runtime::DirtyFlags flags) {
    if(auto* current = host_->components().state<InputState>(component)) {
        host_->dirty().invalidate(current->mounted.node, flags);
        if(runtime::has_any(flags, runtime::DirtyFlags::Material)) {
            update_caret(component);
            const auto& theme = host_->components().theme_scope(component)->snapshot();
            const auto values = material_values(*current, derive_input_tokens(theme));
            if(current->transition) {
                const auto spec = animation::resolve_motion_policy(theme, host_->motion_preference())
                    .transition(animation::MotionDurationToken::mid, animation::MotionEasingToken::ease_in_out);
                current->transition->retarget(values, spec, host_->animation_time());
            }
            const auto color = current->transition ? current->transition->value().colors[3] : values.colors[3];
            current->slot_foreground.set({color.red(), color.green(), color.blue(), color.alpha()});
        }
    }
}
void InputComponentHost::apply_material_transition(runtime::ComponentId component) {
    if(auto* state = host_->components().state<InputState>(component); state && state->transition) {
        host_->dirty().invalidate(state->mounted.node, runtime::DirtyFlags::Material | runtime::DirtyFlags::Animation);
        const auto color = state->transition->value().colors[3];
        state->slot_foreground.set({color.red(), color.green(), color.blue(), color.alpha()});
    }
}
void InputComponentHost::synchronize_auxiliary_motion() {
    for(const auto& mounted : mounted_) invalidate(mounted.component, runtime::DirtyFlags::Material);
}
void InputComponentHost::update_caret(runtime::ComponentId component, bool reset) {
    auto* state = host_->components().state<InputState>(component);
    if(!state) return;
    const bool eligible = state->focused && !state->disabled && !state->read_only && host_->focus().state().window_active;
    const auto policy = animation::resolve_motion_policy(host_->components().theme_scope(component)->snapshot(), host_->motion_preference());
    if(state->caret_blink.configure(eligible, policy.enabled() && !policy.reduced(), host_->animation_time(), reset))
        host_->dirty().invalidate(state->mounted.node, runtime::DirtyFlags::Geometry);
}
std::size_t InputComponentHost::tick_auxiliary(animation::AnimationTime now) {
    std::size_t changed{};
    for(const auto& mounted : mounted_) {
        if(auto* state = host_->components().state<InputState>(mounted.component); state && state->caret_blink.tick(now)) {
            host_->dirty().invalidate_in_frame(mounted.node, runtime::DirtyFlags::Material);
            ++changed;
        }
    }
    return changed;
}
void InputComponentHost::dispatch_pointer(runtime::ComponentId component, input::PointerDispatchContext& context) {
    auto* state = host_->components().state<InputState>(component);
    if(!state) return;
    const auto& event = context.event();
    const auto kind = context.kind();
    const bool owned = state->selecting_pointer == event.pointer;
    if(kind == input::PointerEventKind::cancel) {
        if(owned) {
            state->selecting_pointer.reset();
            static_cast<void>(context.release_pointer_capture());
        }
        return;
    }
    const bool down = kind == input::PointerEventKind::down && event.button == input::PointerButton::primary;
    const bool up = kind == input::PointerEventKind::up && event.button == input::PointerButton::primary;
    if(!down && !(owned && (kind == input::PointerEventKind::move || up))) return;
    if(state->disabled || !state->focused || (down && state->selecting_pointer && !owned)) return;
    const auto viewport = state->geometry.viewport;
    const auto clip = state->geometry.clip;
    // Affixes/padding may focus the Input, but are not editable text hit areas.
    if(down && (event.x < clip.x || event.x > clip.x + clip.width
        || event.y < clip.y || event.y > clip.y + clip.height || clip.width <= 0 || clip.height <= 0)) return;
    update_text(component, false);
    auto& scene = host_->text().scene_service();
    if(state->carets.revision() != scene.text_state(state->text_scene).revision()
        && !scene.synchronize_caret_map(state->text_scene, state->carets))
        throw std::runtime_error("Input pointer caret mapping failed");
    const auto stop = state->carets.nearest(event.x - viewport.x + state->geometry.scroll_offset,
        state->carets.revision());
    if(!stop) return;
    const auto byte = state->display.display_to_committed(stop->byte);
    auto& editor = editors_.require(state->mounted.editor);
    if(down) {
        if(editor.composition().active) {
            static_cast<void>(sessions_.cancel_composition());
        }
        check(event.click_count == 2 ? editor.select_word(byte) : editor.place(byte));
        if(context.capture_pointer()) state->selecting_pointer = event.pointer;
        state->last_selection_position = {event.x, event.y};
        update_caret(component, true);
    } else {
        // A release at the last position preserves a double-click word selection.
        if(state->last_selection_position != runtime::Point{event.x, event.y}) check(editor.place(byte, true));
        state->last_selection_position = {event.x, event.y};
        if(up) {
            state->selecting_pointer.reset();
            static_cast<void>(context.release_pointer_capture());
        }
    }
    update_text(component, false);
}
bool InputComponentHost::dispatch_keyboard(runtime::ComponentId component, const input::KeyboardInputEvent& event) {
    using input::Key; using input::KeyModifier;
    auto* state = host_->components().state<InputState>(component);
    if(!state || state->disabled || !state->focused) return false;
    auto& editor = editors_.require(state->mounted.editor);
    if(editor.composition().active) {
        if(event.key == Key::escape && event.action == input::KeyAction::down && !event.repeat) {
            static_cast<void>(sessions_.cancel_composition());
            update_text(component, false);
        }
        // IME owns navigation, deletion, candidate Tab/Enter and shortcuts until
        // it commits or cancels. Never mutate committed text from these keys.
        return true;
    }
    if(event.key == Key::tab || event.key == Key::escape) return false;
    const bool shift = input::has_modifier(event.modifiers, KeyModifier::shift);
    const auto other = event.primary_modifier == KeyModifier::control ? KeyModifier::meta : KeyModifier::control;
    const bool primary = input::has_modifier(event.modifiers, event.primary_modifier)
        && !input::has_modifier(event.modifiers, other) && !input::has_modifier(event.modifiers, KeyModifier::alt);
    const bool plain = !input::has_modifier(event.modifiers, KeyModifier::control)
        && !input::has_modifier(event.modifiers, KeyModifier::meta) && !input::has_modifier(event.modifiers, KeyModifier::alt);
    const bool shortcut = primary && (event.key == Key::a || event.key == Key::c || event.key == Key::x
        || event.key == Key::v || event.key == Key::z || event.key == Key::y);
    const bool navigation = plain && (event.key == Key::left || event.key == Key::right
        || event.key == Key::home || event.key == Key::end);
    const bool deletion = plain && (event.key == Key::backspace || event.key == Key::delete_forward);
    if(!shortcut && !navigation && !deletion && event.key != Key::enter && event.key != Key::space) return false;
    if(event.action == input::KeyAction::up) return true;
    update_caret(component, true);
    if(event.key == Key::space) return true; // Only TextCommitted inserts characters.
    if(event.key == Key::enter) {
        if(plain && !event.repeat) submit(component);
        return true;
    }
    input::TextEditResult result;
    const auto owner = state->mounted.editor;
    if(shortcut) {
        if(event.repeat) return true;
        if(event.key == Key::a) result = editor.select_all();
        else if(event.key == Key::c) result = clipboard_.copy(owner).edit;
        else if(!state->read_only) {
            if(event.key == Key::x) result = clipboard_.cut(owner).edit;
            else if(event.key == Key::v) result = clipboard_.paste(owner).edit;
            else if(event.key == Key::y || (event.key == Key::z && shift)) result = editor.redo();
            else result = editor.undo();
        }
    } else if(navigation) {
        const auto move = event.key == Key::left ? input::TextCaretMove::left : event.key == Key::right
            ? input::TextCaretMove::right : event.key == Key::home ? input::TextCaretMove::home : input::TextCaretMove::end;
        result = editor.move(move, shift);
    } else if(deletion && !state->read_only) {
        result = event.key == Key::backspace ? editor.erase_backward() : editor.erase_forward();
    }
    // Clipboard and onChange callbacks may synchronously destroy/reuse the owner.
    if(result.value_changed) notify_change(owner);
    else if(result) update_text(component, false);
    return true;
}
void InputComponentHost::update_theme(runtime::ComponentId component) {
    auto* state = host_->components().state<InputState>(component);
    if(!state) return;
    const auto& theme = host_->components().theme_scope(component)->snapshot();
    const auto tokens = derive_input_tokens(theme);
    const auto& size_tokens = tokens.size(state->size);
    auto model = state->layout;
    model.control_height = size_tokens.control_height;
    model.border_width = tokens.border_width;
    model.padding_inline = size_tokens.padding_inline;
    model.padding_block = size_tokens.padding_block;
    model.gap = tokens.affix_padding;
    if(!state->text_scene.valid() || model != state->layout) {
        state->layout = model;
        host_->layout().set_layout(state->mounted.node, model);
        invalidate(component, runtime::DirtyFlags::Measure | runtime::DirtyFlags::Layout
            | runtime::DirtyFlags::Geometry | runtime::DirtyFlags::HitTest);
    }
    runtime::SemanticTypography typography{theme.text().font_family, theme.text().font_weight,
        size_tokens.font_size, size_tokens.line_height};
    auto& scene = host_->text().scene_service();
    if(!state->text_scene.valid()) {
        state->text_scene = scene.create(state->viewport, String{}, host_->text().resolve_fonts(typography),
            static_cast<std::uint32_t>(std::lround(typography.font_size)),
            {typography.line_height, std::numeric_limits<float>::infinity()});
        state->typography = typography;
    } else if(state->typography != typography) {
        scene.set_font_chain(state->text_scene, host_->text().resolve_fonts(typography));
        scene.set_pixel_size(state->text_scene, static_cast<std::uint32_t>(std::lround(typography.font_size)));
        scene.set_line_height(state->text_scene, typography.line_height);
        state->typography = typography;
        const auto revisions = scene.revisions(state->text_scene);
        host_->layout().set_intrinsic_revision(state->viewport, revisions.content + revisions.layout);
        invalidate(component, text_dirty);
    }
    state->slot_typography.set(typography);
    invalidate(component, runtime::DirtyFlags::Material);
}
void InputComponentHost::update_text(runtime::ComponentId component, bool measure_layout) {
    auto* state = host_->components().state<InputState>(component);
    if(!state || !state->text_scene.valid()) return;
    const auto& editor = editors_.require(state->mounted.editor);
    const auto changed = state->display.update(editor, state->placeholder.view());
    auto& scene = host_->text().scene_service();
    if(changed.text_changed && scene.text_state(state->text_scene).content().bytes() != state->display.snapshot().text) {
        scene.set_content(state->text_scene, String::from_utf8(state->display.snapshot().text).value());
        const auto revisions = scene.revisions(state->text_scene);
        host_->layout().set_intrinsic_revision(state->viewport, revisions.content + revisions.layout);
        invalidate(component, measure_layout ? text_dirty : runtime::DirtyFlags::Geometry);
    }
    if(changed.geometry_changed) {
        invalidate(component, runtime::DirtyFlags::Geometry);
        update_caret(component, true);
    }
    if(measure_layout && state->measured_value_revision != editor.revision()) {
        state->measured_value_revision = editor.revision();
        invalidate(component, runtime::DirtyFlags::Measure | runtime::DirtyFlags::Layout | runtime::DirtyFlags::Geometry);
    }
}
InputLayoutSnapshot InputComponentHost::layout_snapshot(runtime::ComponentId component) const {
    const auto* state = host_->components().state<InputState>(component);
    if(!state) throw std::out_of_range("Input component is stale");
    return state->geometry;
}
TextSceneId InputComponentHost::text_scene(runtime::ComponentId component) const {
    const auto* state = host_->components().state<InputState>(component);
    if(!state) throw std::out_of_range("Input component is stale");
    return state->text_scene;
}
InputTextLayers InputComponentHost::text_layers(runtime::ComponentId component) const {
    const auto* state = host_->components().state<InputState>(component);
    if(!state) throw std::out_of_range("Input component is stale");
    return {state->text_scene, state->selected_scene, state->placeholder_scene};
}
InputDisplaySnapshot InputComponentHost::display_snapshot(runtime::ComponentId component) const {
    const auto* state = host_->components().state<InputState>(component);
    if(!state) throw std::out_of_range("Input component is stale");
    return state->display.snapshot();
}
const text::TextCaretMap& InputComponentHost::caret_map(runtime::ComponentId component) const {
    const auto* state = host_->components().state<InputState>(component);
    if(!state) throw std::out_of_range("Input component is stale");
    return state->carets;
}
bool InputComponentHost::set_caret_deadline(runtime::ComponentId component,
    std::optional<animation::AnimationTime> deadline) {
    auto* state = host_->components().state<InputState>(component);
    if(!state || (deadline && (!state->focused || state->disabled || state->read_only
        || !host_->focus().state().window_active))) return false;
    return state->caret_blink.override_deadline(deadline);
}
std::optional<animation::AnimationTime> InputComponentHost::next_caret_deadline() const {
    std::optional<animation::AnimationTime> next;
    for(const auto& mounted : mounted_) {
        const auto* state = host_->components().state<InputState>(mounted.component);
        const auto candidate = state ? state->caret_blink.deadline() : std::nullopt;
        if(candidate && (!next || *candidate < *next)) next = candidate;
    }
    return next;
}
void InputComponentHost::set_horizontal_scroll(runtime::ComponentId component, float offset) {
    if(!std::isfinite(offset)) throw std::invalid_argument("Input scroll must be finite");
    auto* state = host_->components().state<InputState>(component);
    if(!state) return;
    const auto next = std::clamp(offset, 0.0F, std::max(0.0F, state->geometry.text_width - state->geometry.viewport.width));
    if(next == state->geometry.scroll_offset) return;
    state->geometry.scroll_offset = next;
    invalidate(component, runtime::DirtyFlags::Geometry);
}
void InputComponentHost::synchronize_auxiliary_geometry(runtime::Size window, runtime::Rect clip) {
    for(const auto& mounted : mounted_) {
        auto* state = host_->components().state<InputState>(mounted.component);
        if(!state) continue;
        update_text(mounted.component, false);
        auto& text_scene = host_->text().scene_service();
        if(state->carets.revision() != text_scene.text_state(state->text_scene).revision()
            && !text_scene.synchronize_caret_map(state->text_scene, state->carets))
                throw std::runtime_error("Input caret mapping failed");
        const auto& node = host_->nodes().require(state->viewport);
        auto viewport = node.bounds;
        // Include parent translations; bounds themselves are absolute layout coordinates.
        for(auto ancestor = std::optional{state->viewport}; ancestor;
            ancestor = host_->nodes().require(*ancestor).parent) {
            const auto translation = host_->nodes().require(*ancestor).translation;
            viewport.x += translation.x; viewport.y += translation.y;
        }
        const auto right = std::min(viewport.x + viewport.width, clip.x + clip.width);
        const auto bottom = std::min(viewport.y + viewport.height, clip.y + clip.height);
        const auto left = std::max(viewport.x, clip.x), top = std::max(viewport.y, clip.y);
        const auto& measurement = host_->text().scene_service().text_state(state->text_scene).measurement();
        state->geometry.viewport = viewport;
        state->geometry.clip = pixel_clip(
            {left, top, std::max(0.0F, right - left), std::max(0.0F, bottom - top)}, display_scale_);
        state->geometry.baseline = viewport.y + measurement.first_baseline;
        state->geometry.text_width = measurement.width;
        const float thickness = std::max(1.0F, std::round(display_scale_)) / display_scale_;
        const auto caret_clip = pixel_clip(viewport, display_scale_);
        const float usable_width = std::max(0.0F, caret_clip.x + caret_clip.width - viewport.x);
        const auto scroll = state->display.scroll_for_caret(state->carets,
            state->carets.revision(), usable_width, state->geometry.scroll_offset, thickness).value();
        // Whole physical pixels preserve the cached glyph raster phase.
        state->geometry.scroll_offset = std::ceil(scroll * display_scale_) / display_scale_;
        const auto display = state->display.snapshot();
        const auto x = [&](std::size_t byte) { return viewport.x + state->carets.at(byte, state->carets.revision()).value().x
            - state->geometry.scroll_offset; };
        state->geometry.caret_x = x(display.caret);
        state->geometry.selection_start = x(display.selection.begin());
        state->geometry.selection_end = x(display.selection.end());
        state->geometry.composition_start = x(display.composition.begin());
        state->geometry.composition_end = x(display.composition.end());
        const auto snap = [&](float value) { return std::round(value * display_scale_) / display_scale_; };
        const auto caret_left = std::clamp(snap(state->geometry.caret_x), caret_clip.x,
            std::max(caret_clip.x, caret_clip.x + caret_clip.width - thickness));
        state->geometry.caret = {caret_left, caret_clip.y, std::min(thickness, caret_clip.width), caret_clip.height};
        const auto underline_left = snap(state->geometry.composition_start);
        state->geometry.underline = {underline_left,
            std::max(caret_clip.y, caret_clip.y + caret_clip.height - thickness),
            std::max(0.0F, snap(state->geometry.composition_end) - underline_left),
            std::min(thickness, caret_clip.height)};
        const auto& theme = host_->components().theme_scope(mounted.component)->snapshot();
        const auto root_bounds = translated_bounds(host_->nodes(), mounted.node);
        state->next_container_clip = clip;
        const float border = std::min(state->layout.border_width,
            0.5F * std::min(root_bounds.width, root_bounds.height));
        const auto& tokens = derive_input_tokens(theme);
        const float radius = tokens.size(state->size).border_radius;
        const auto visual = resolve_visuals(*state, tokens);
        const auto& presentation = state->transition->value();
        std::array<Color, input_shadow_layer_capacity> shadow_colors;
        std::copy_n(presentation.colors.begin() + 8, input_shadow_layer_capacity, shadow_colors.begin());
        const InputContainerPresentation next_container{root_bounds, clip, radius, border,
            presentation.colors[0], presentation.colors[1], *visual.shadow, shadow_colors, presentation.shadow_opacity};
        if(state->container_presentation != next_container) {
            for(std::size_t layer = 0; layer < state->container_effects.size(); ++layer) {
                const bool outer = layer < input_shadow_layer_capacity;
                const bool inner = layer >= input_inset_shadow_layer && layer < input_focus_layer;
                auto bounds = root_bounds;
                const float inset = layer == input_background_layer || inner ? border : 0.0F;
                bounds.x += inset; bounds.y += inset;
                bounds.width -= 2 * inset; bounds.height -= 2 * inset;
                graphics::RoundedEffectInstance effect;
                effect.geometry.shape = {bounds, std::clamp(radius - inset, 0.0F, 0.5F * std::min(bounds.width, bounds.height))};
                effect.geometry.ancestor_clip = graphics::EffectClip{1, clip};
                effect.material = {layer == input_background_layer ? presentation.colors[0] : presentation.colors[1],
                    layer == input_border_layer || layer == input_background_layer ? 1.0F : 0.0F, true};
                if(outer || inner) {
                    const auto slot = outer ? layer : layer - input_inset_shadow_layer;
                    const auto source = input_shadow_layer_capacity - 1 - slot;
                    if(source < visual.shadow->size()) {
                        const auto& shadow = (*visual.shadow)[source];
                        if((shadow.kind == ShadowKind::outer) == outer) {
                            effect = graphics::make_shadow_effect(effect.geometry.shape, shadow, {}, graphics::EffectClip{1, clip});
                            effect.material.color = shadow_colors[source];
                            effect.material.opacity = presentation.shadow_opacity;
                        }
                    }
                } else if(layer == input_focus_layer) {
                    // Outlined Input uses activeShadow for either focus modality;
                    // outline: 0 in pinned variants.ts. Keep the reserved layer hidden.
                    effect.geometry.kind = graphics::RoundedEffectKind::outline;
                    effect.geometry.outline_width = std::max(0.001F, 3 * tokens.border_width);
                    effect.geometry.outline_offset = 1;
                }
                static_cast<void>(host_->rounded_effects().update_geometry(state->container_effects[layer], effect.geometry));
                static_cast<void>(host_->rounded_effects().update_material(state->container_effects[layer], effect.material));
            }
            state->container_presentation = next_container;
        }
        const auto foreground = channels(presentation.colors[2]);
        const auto selection_color = presentation.colors[6];
        const std::array<graphics::QuadInstance, 1> selection{clipped_quad(
            {state->geometry.selection_start, viewport.y,
                state->geometry.selection_end - state->geometry.selection_start, viewport.height},
            state->geometry.clip, window,
            {selection_color.red(), selection_color.green(), selection_color.blue(), selection_color.alpha()},
            state->focused && !state->disabled && !display.placeholder ? 1.0F : 0.0F)};
        static_cast<void>(host_->button_scene().update_surface(state->selection_surface, selection));
        const std::array<graphics::QuadInstance, 2> overlays{
            clipped_quad(state->geometry.underline,
                state->geometry.clip, window, foreground, display.composing ? 1.0F : 0.0F),
            clipped_quad(state->geometry.caret,
                state->geometry.clip, window, channels(presentation.colors[4]),
                state->focused && !state->disabled && !state->read_only
                    && host_->focus().state().window_active && state->caret_blink.visible() ? 1.0F : 0.0F),
        };
        static_cast<void>(host_->button_scene().update_surface(state->overlay_surface, overlays));
        text_scene.set_color(state->text_scene, foreground);
        text_scene.set_color(state->selected_scene, channels(presentation.colors[7]));
        text_scene.set_color(state->placeholder_scene, channels(presentation.colors[5]));
        text_scene.set_opacity(state->text_scene, display.placeholder ? 0.0F : 1.0F);
        text_scene.set_opacity(state->selected_scene,
            state->focused && !state->disabled && !display.placeholder
                && display.selection.begin() != display.selection.end() ? 1.0F : 0.0F);
        text_scene.set_opacity(state->placeholder_scene, display.placeholder ? 1.0F : 0.0F);
        graphics::GlyphPlacement placement{{viewport.x, viewport.y}, window, state->geometry.clip};
        for(const auto id : {state->text_scene, state->selected_scene, state->placeholder_scene}) {
            auto layer_placement = placement;
            if(id == state->selected_scene) {
                layer_placement.clip_pixels = graphics::intersect_effect_bounds(state->geometry.clip,
                    {state->geometry.selection_start, viewport.y,
                        state->geometry.selection_end - state->geometry.selection_start, viewport.height});
            }
            text_scene.set_scroll_translation(id, {-state->geometry.scroll_offset, 0});
            if(!text_scene.synchronize(id, layer_placement)) throw std::runtime_error("Input glyph synchronization failed");
        }
    }
}
bool InputComponentHost::synchronize_auxiliary_fragments() {
    bool changed{};
    for(const auto& mounted : mounted_) {
        auto* state = host_->components().state<InputState>(mounted.component);
        if(!state) continue;
        std::array<graphics::SceneDrawCommand, input_effect_layer_count> container;
        std::size_t count{};
        for(const auto effect : state->container_effects) {
            if(const auto index = host_->rounded_effects().packed_index(effect))
                container[count++] = {graphics::SceneDrawKind::rounded_effect, *index, 1};
        }
        const auto commands = std::span{container}.first(count);
        if(!std::ranges::equal(commands, state->container_commands)
            || state->container_clip != state->next_container_clip) {
            host_->scene_composer().set_fragment(state->container_fragment, commands,
                mounted.interaction, state->next_container_clip);
            state->container_commands.assign(commands.begin(), commands.end());
            state->container_clip = state->next_container_clip;
            changed = true;
        }
        auto& pending = state->pending_text_commands;
        pending.clear();
        for(const auto id : {state->text_scene, state->selected_scene, state->placeholder_scene}) {
            for(const auto& range : host_->text().scene_service().primitive(id).draw_ranges)
                pending.push_back({graphics::SceneDrawKind::glyph, range.instances.first,
                    range.instances.count, range.atlas_page});
        }
        if(pending == state->text_commands) continue;
        host_->scene_composer().set_fragment(state->text_fragment, pending);
        state->text_commands.swap(pending);
        changed = true;
    }
    return changed;
}
void InputComponentHost::notify_change(input::TextInputOwnerId editor_id) {
    const auto found = std::find_if(mounted_.begin(), mounted_.end(), [editor_id](const auto& value) { return value.editor == editor_id; });
    if(found == mounted_.end()) return;
    const auto component = found->component;
    auto* state = host_->components().state<InputState>(component);
    auto* editor = editors_.find(editor_id);
    if(!state || !editor) return;
    update_text(component);
    // Own both callback and value across synchronous Signal echoes or self-unmount.
    auto callback = state->on_change;
    if(!callback) return;
    auto value = String::from_utf8(editor->value()).value();
    check(editor->note_emitted_value());
    callback(std::move(value));
}
input::TextEditResult InputComponentHost::dispatch(const input::TextCommitted& event) {
    auto result = sessions_.dispatch(event);
    if(result) for(const auto& mounted : mounted_)
        if(mounted.editor == event.session.owner) update_caret(mounted.component, true);
    if(result.value_changed) notify_change(event.session.owner);
    else if(result) for(const auto& mounted : mounted_)
        if(mounted.editor == event.session.owner) update_text(mounted.component, false);
    return result;
}
input::TextEditResult InputComponentHost::dispatch(const input::CompositionChanged& event) {
    auto result = sessions_.dispatch(event);
    if(result) for(const auto& mounted : mounted_) if(mounted.editor == event.session.owner) {
        update_caret(mounted.component, true);
        update_text(mounted.component, false);
    }
    return result;
}
input::TextEditResult InputComponentHost::dispatch(const input::CandidatesChanged& event) { return sessions_.dispatch(event); }
void InputComponentHost::submit(runtime::ComponentId component) {
    const auto* state = host_->components().state<InputState>(component);
    if(!state || state->disabled || state->read_only) return;
    auto& editor = editors_.require(state->mounted.editor);
    if(editor.composition().active) return;
    editor.break_history_merge();
    auto callback = state->on_submit;
    if(callback) { auto value = String::from_utf8(editor.value()).value(); callback(std::move(value)); }
}
} // namespace ryn::detail

namespace ryn {
void Input(InputProps props, std::optional<InputPrefix> prefix, std::optional<InputSuffix> suffix) {
    if(!detail::active_input_host) throw std::logic_error("Input requires an active InputComponentHost");
    detail::InputPropsAccess::mount(*detail::active_input_host, props, prefix, suffix);
}
}
