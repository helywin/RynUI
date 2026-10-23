#include "component/dropdown_component.hpp"
#include "component/menu_component.hpp"
#include "component/select_component.hpp"
#include "runtime/layout_style_adapter.hpp"
#include "runtime/prop_connection.hpp"
#include <algorithm>

namespace ryn::detail {
namespace {
thread_local DropdownComponentHost* active_dropdowns{};

struct DropdownState final {
    runtime::NodeId node;
    DropdownSnapshot snapshot;
    input::InteractionId trigger_hit;
    input::InteractionId popup_hit;
    component::RetainedSurfaceId surface;
    bool controlled{};
    bool open{};
    bool suppressed{};
    bool disabled{};
    bool adjust{true};
    bool close_on_select{true};
    bool needs_measure{true};
    bool focus_pending{};
    bool focus_last{};
    bool restore{true};
    DropdownTriggerMode mode{DropdownTriggerMode::Click};
    DropdownPlacement placement{DropdownPlacement::BottomLeft};
    runtime::Size viewport;
    runtime::Size popup_size;
    std::optional<runtime::Point> context_anchor;
    std::optional<input::InteractionId> restore_target;
    std::function<void(DropdownOpenInfo)> on_open;
    std::array<theme_runtime::Subscription, 4> subscriptions;
};

bool descendant(WindowComponentServices& services, runtime::ComponentId child, runtime::ComponentId ancestor) {
    for (auto current = std::optional{child}; current && services.components().contains(*current);
         current = services.components().parent(*current)) {
        if (*current == ancestor) {
            return true;
        }
    }
    return false;
}

bool inside(runtime::Rect rect, float x, float y) {
    return x >= rect.x && y >= rect.y && x < rect.x + rect.width && y < rect.y + rect.height;
}

bool can_focus(WindowComponentServices& services, input::InteractionId hit) {
    const auto* record = services.interactions().find(hit);
    return record && record->eligible && record->focusable && services.interactions().branch_active(hit);
}

std::optional<input::InteractionId> trigger_focus(WindowComponentServices& services, runtime::ComponentId root) {
    for (auto hit : services.interactions().declaration_order()) {
        const auto* record = services.interactions().find(hit);
        if (record && record->eligible && record->focusable && descendant(services, record->component, root)) {
            return hit;
        }
    }
    return {};
}

runtime::ComponentId group(WindowComponentServices& services, layout::LayoutModel model) {
    auto& build = runtime::require_component_build_context();
    const auto id = build.mount_component<int>(0);
    const auto node = build.root(id);
    services.layout().set_layout(node, std::move(model));
    build.on_resource_cleanup(id, [&services, node] { services.layout().remove_layout(node); });
    return id;
}

void validate(DropdownTriggerMode mode, DropdownPlacement placement) {
    if (mode < DropdownTriggerMode::Manual || mode > DropdownTriggerMode::ClickContextMenu ||
        placement < DropdownPlacement::Top || placement > DropdownPlacement::RightBottom) {
        throw std::invalid_argument("Dropdown trigger or placement is invalid");
    }
}

bool click_trigger(DropdownTriggerMode mode) {
    return mode == DropdownTriggerMode::Click || mode == DropdownTriggerMode::ClickContextMenu;
}

bool context_trigger(DropdownTriggerMode mode) {
    return mode == DropdownTriggerMode::ContextMenu || mode == DropdownTriggerMode::ClickContextMenu;
}
} // namespace

DropdownComponentHost::DropdownComponentHost(WindowComponentServices& services) : services_(&services) {
    services.attach(*this);
}

DropdownComponentHost::~DropdownComponentHost() {
    services_->detach(*this);
}

void* DropdownComponentHost::begin_mount() noexcept {
    auto* previous = active_dropdowns;
    active_dropdowns = this;
    return previous;
}

void DropdownComponentHost::end_mount(void* previous) noexcept {
    active_dropdowns = static_cast<DropdownComponentHost*>(previous);
}

void DropdownComponentHost::on_destroy() noexcept {
    std::erase_if(mounted_, [this](auto id) { return !services_->components().contains(id); });
}

DropdownSnapshot DropdownComponentHost::snapshot(runtime::ComponentId id) const {
    const auto* s = services_->components().state<DropdownState>(id);
    if (!s) {
        throw std::out_of_range("Dropdown component is stale");
    }
    return s->snapshot;
}

void DropdownComponentHost::refresh(runtime::ComponentId id) {
    auto* s = services_->components().state<DropdownState>(id);
    if (!s || !services_->components().scope(id).active()) {
        return;
    }
    const bool visible =
        s->open && !s->suppressed && !s->disabled && window_active_ && services_->interactions().component_active(id);
    const bool changed = visible != s->snapshot.geometry.visible;
    s->snapshot.geometry.visible = visible;
    services_->interactions().set_eligible(s->trigger_hit, !s->disabled);
    if (services_->components().set_branch_active(s->snapshot.popup, visible)) {
        s->needs_measure = true;
        services_->mark_scene_structure_dirty();
        services_->dirty().invalidate(s->node, runtime::DirtyFlags::Geometry | runtime::DirtyFlags::HitTest);
    }
    if (!changed) {
        return;
    }
    if (visible) {
        const auto focused = services_->focus().state().focused;
        const auto* record = focused ? services_->interactions().find(*focused) : nullptr;
        s->restore_target = record && descendant(*services_, record->component, s->snapshot.trigger)
                                ? focused
                                : trigger_focus(*services_, s->snapshot.trigger);
        if (!s->restore_target) {
            s->restore_target = s->trigger_hit;
        }
        s->focus_pending = true;
        return;
    }
    s->focus_pending = false;
    const auto popup = s->snapshot.popup;
    const auto focused = services_->focus().state().focused;
    const auto* record = focused ? services_->interactions().find(*focused) : nullptr;
    const bool restore = s->restore && record && descendant(*services_, record->component, popup);
    const auto target = s->restore_target;
    s->restore_target.reset();
    const auto order = services_->interactions().declaration_order();
    const std::vector<input::InteractionId> hits{order.begin(), order.end()};
    for (auto hit : hits) {
        const auto* live = services_->interactions().find(hit);
        if (live && descendant(*services_, live->component, popup)) {
            services_->pointer().cancel_pointer_interaction(hit);
        }
    }
    services_->select().on_input_boundary_changed();
    services_->tooltip().on_input_boundary_changed();
    if (restore && target && can_focus(*services_, *target)) {
        services_->focus().defer_focus(target, input::FocusModality::keyboard);
    } else {
        services_->focus().synchronize();
    }
}

void DropdownComponentHost::request_open(runtime::ComponentId id, bool open, DropdownOpenSource source, bool restore) {
    auto* s = services_->components().state<DropdownState>(id);
    if (!s || !services_->components().scope(id).active() ||
        (open && (s->disabled || !window_active_ || !services_->interactions().component_active(id)))) {
        return;
    }
    const bool changed = s->open != open;
    s->restore = restore;
    if (open) {
        s->suppressed = false;
    }
    if (!s->controlled) {
        s->open = open;
    }
    const auto callback = s->on_open;
    refresh(id);
    if (changed && callback && services_->components().contains(id) && services_->components().scope(id).active()) {
        callback({open, source});
    }
}

void DropdownComponentHost::suppress(runtime::ComponentId id) {
    auto* s = services_->components().state<DropdownState>(id);
    if (!s || s->suppressed) {
        return;
    }
    const bool notify = s->open;
    s->suppressed = true;
    s->restore = false;
    if (!s->controlled) {
        s->open = false;
    }
    s->context_anchor.reset();
    const auto callback = s->on_open;
    refresh(id);
    if (notify && callback && services_->components().contains(id) && services_->components().scope(id).active()) {
        callback({false, DropdownOpenSource::Environment});
    }
}

void DropdownComponentHost::update_theme(runtime::ComponentId id, bool geometry) {
    auto* s = services_->components().state<DropdownState>(id);
    if (!s) {
        return;
    }
    const auto& token = services_->components().theme_scope(id)->snapshot().dropdown();
    if (services_->components().set_window_layer(s->snapshot.popup, token.z_index_popup)) {
        services_->mark_scene_structure_dirty();
    }
    if (geometry) {
        s->needs_measure = true;
    }
    services_->dirty().invalidate(s->node, geometry ? runtime::DirtyFlags::Geometry : runtime::DirtyFlags::Material);
}

void DropdownComponentHost::mount(const DropdownProps& props, const DropdownTrigger& trigger) {
    if (props.open_ && props.default_open_) {
        throw std::invalid_argument("Dropdown cannot combine open and defaultOpen");
    }
    validate(read_prop(props.trigger_), read_prop(props.placement_));
    MenuComponentHost::validate(props.menu_);
    auto& services = *services_;
    auto& build = runtime::require_component_build_context();
    const auto id = build.mount_component<DropdownState>();
    auto& s = build.state<DropdownState>(id);
    s.node = build.root(id);
    s.controlled = props.open_.has_value();
    s.open = props.open_ ? read_prop(*props.open_) : props.default_open_.value_or(false);
    s.disabled = read_prop(props.disabled_);
    s.mode = read_prop(props.trigger_);
    s.placement = read_prop(props.placement_);
    s.adjust = read_prop(props.adjust_);
    s.close_on_select = read_prop(props.close_on_select_);
    s.on_open = props.on_open_;
    runtime::connect_layout_style(build.scope(id), props.layout_, s.node, services.nodes(), services.dirty());
    s.trigger_hit = services.interactions().create({id, s.node, {}, !s.disabled, true, {}, true, true});
    const auto hit = build.register_scene_fragment(id, runtime::SceneFragmentPlacement::before_children);
    services.scene_composer().set_fragment(hit, {}, s.trigger_hit);
    build.on_resource_cleanup(id, [&services, node = s.node, interaction = s.trigger_hit, hit] {
        services.pointer().cancel_interaction(interaction);
        services.interactions().remove(interaction);
        services.scene_composer().remove_fragment(hit);
        services.layout().remove_layout(node);
    });
    build.mount_slot(
        id, Content{[&] {
            s.snapshot.trigger = group(services, layout::BoxLayout{});
            build.mount_slot(s.snapshot.trigger, trigger);
            const auto trigger_entry = trigger_focus(services, s.snapshot.trigger);
            services.interactions().set_tab_stop(s.trigger_hit, !trigger_entry);
            // Preserve native control activation. Only decorate its activation callback,
            // leaving state, gate, text-edit handling and key down/up with FocusManager.
            const auto order = services.interactions().declaration_order();
            const std::vector<input::InteractionId> trigger_hits{order.begin(), order.end()};
            for (auto interaction : trigger_hits) {
                const auto* record = services.interactions().find(interaction);
                if (interaction != s.trigger_hit &&
                    (!record || !record->focusable || !descendant(services, record->component, s.snapshot.trigger))) {
                    continue;
                }
                auto handlers = record && record->focus_handlers ? *record->focus_handlers : input::FocusHandlers{};
                const auto original = handlers.activate;
                handlers.activate = [this, id, original] {
                    if (original) {
                        original();
                    }
                    auto* state = services_->components().state<DropdownState>(id);
                    if (state && click_trigger(state->mode)) {
                        state->context_anchor.reset();
                        request_open(id, !state->snapshot.geometry.visible, DropdownOpenSource::Trigger);
                    }
                };
                services.interactions().set_focus_handlers(interaction, std::move(handlers));
            }
            s.snapshot.popup = group(services, layout::BoxLayout{});
            const auto popup_node = build.root(s.snapshot.popup);
            services.components().set_window_layer(s.snapshot.popup,
                                                   build.theme_scope()->snapshot().dropdown().z_index_popup);
            services.components().set_branch_active(s.snapshot.popup, false);
            services.nodes().require(popup_node).clip_content = true;
            s.popup_hit =
                services.interactions().create({s.snapshot.popup, popup_node, {}, true, false, {}, false, false});
            const auto fragment =
                build.register_scene_fragment(s.snapshot.popup, runtime::SceneFragmentPlacement::before_children);
            graphics::QuadInstance initial;
            initial.opacity = 0;
            component::RetainedSurfaceEffects effects;
            effects.focus_enabled = false;
            s.surface = services.surfaces().create(s.snapshot.popup, popup_node, fragment, s.popup_hit,
                                                   std::span{&initial, 1}, effects);
            build.on_resource_cleanup(s.snapshot.popup, [&services, hit = s.popup_hit, surface = s.surface] {
                services.pointer().cancel_interaction(hit);
                services.interactions().remove(hit);
                services.surfaces().destroy(surface);
            });
            auto menu = props.menu_;
            if (!menu.explicit_selectable_) {
                menu.selectable(false);
            }
            const auto original = menu.on_click_;
            menu.on_click_ = [this, id, original](MenuClickInfo info) {
                if (original) {
                    original(std::move(info));
                }
                const auto* state = services_->components().state<DropdownState>(id);
                if (state && state->close_on_select) {
                    request_open(id, false, DropdownOpenSource::Menu);
                }
            };
            build.mount_slot(s.snapshot.popup, Content{[&] {
                                 services.menu().mount(menu, true);
                                 s.snapshot.menu = services.components().children(s.snapshot.popup).back();
                             }});
        }});
    services.layout().set_layout(
        s.node, layout::ComponentLayout{
                    [this, id](layout::LayoutEngine& engine, runtime::NodeId, layout::Constraints limits) {
                        const auto* state = services_->components().state<DropdownState>(id);
                        return engine.measure_child(services_->components().root(state->snapshot.trigger), limits);
                    },
                    [this, id](layout::LayoutEngine& engine, runtime::NodeId, runtime::Rect rect) {
                        const auto* state = services_->components().state<DropdownState>(id);
                        engine.place_child(services_->components().root(state->snapshot.trigger), rect);
                    }});
    mounted_.push_back(id);
    auto& scope = build.scope(id);
    if (props.open_) {
        connect_prop(scope, *props.open_, [this, id](bool value) {
            if (auto* s = services_->components().state<DropdownState>(id)) {
                s->open = value;
                s->suppressed = false;
                s->restore = true;
                refresh(id);
            }
        });
    }
    connect_prop(scope, props.disabled_, [this, id](bool value) {
        if (auto* s = services_->components().state<DropdownState>(id)) {
            s->disabled = value;
            if (value) {
                suppress(id);
            } else {
                refresh(id);
            }
        }
    });
    connect_prop(scope, props.trigger_, [this, id](DropdownTriggerMode mode) {
        auto* s = services_->components().state<DropdownState>(id);
        if (!s) {
            return;
        }
        validate(mode, s->placement);
        s->mode = mode;
    });
    connect_prop(scope, props.placement_, [this, id](DropdownPlacement placement) {
        auto* s = services_->components().state<DropdownState>(id);
        if (!s) {
            return;
        }
        validate(s->mode, placement);
        s->placement = placement;
        update_theme(id, false);
    });
    connect_prop(scope, props.adjust_, [this, id](bool value) {
        if (auto* s = services_->components().state<DropdownState>(id)) {
            s->adjust = value;
            update_theme(id, false);
        }
    });
    connect_prop(scope, props.close_on_select_, [this, id](bool value) {
        if (auto* s = services_->components().state<DropdownState>(id)) {
            s->close_on_select = value;
        }
    });
    const auto theme = build.theme_scope();
    const std::array accessors{
        &theme_runtime::ThemeScope::dropdown_colors, &theme_runtime::ThemeScope::dropdown_metrics,
        &theme_runtime::ThemeScope::dropdown_effects, &theme_runtime::ThemeScope::dropdown_order};
    for (std::size_t i = 0; i < accessors.size(); ++i) {
        s.subscriptions[i] =
            theme->capture([this, id, geometry = i == 1](theme_runtime::DirtyPhase) { update_theme(id, geometry); },
                           [theme, accessor = accessors[i]] { static_cast<void>((theme.get()->*accessor)()); });
    }
    refresh(id);
}

void DropdownComponentHost::on_window_active(bool active) {
    window_active_ = active;
    const auto ids = mounted_;
    for (auto id : ids) {
        if (!active) {
            suppress(id);
        } else {
            refresh(id);
        }
    }
}

void DropdownComponentHost::on_input_boundary_changed() {
    const auto ids = mounted_;
    for (auto id : ids) {
        const auto* s = services_->components().state<DropdownState>(id);
        if (s && s->snapshot.geometry.visible && !services_->interactions().component_active(id)) {
            suppress(id);
        } else {
            refresh(id);
        }
    }
}

bool DropdownComponentHost::on_keyboard_input(const input::KeyboardInputEvent& event) {
    if (!window_active_ || event.action != input::KeyAction::down) {
        return false;
    }
    const auto focus = services_->focus().state().focused;
    const auto* record = focus ? services_->interactions().find(*focus) : nullptr;
    const auto component = record ? std::optional{record->component} : std::optional<runtime::ComponentId>{};
    const auto ids = mounted_;
    std::optional<runtime::ComponentId> escape;
    for (auto id : ids) {
        auto* s = services_->components().state<DropdownState>(id);
        if (!s || s->disabled || !services_->interactions().component_active(id)) {
            continue;
        }
        const bool in_trigger =
            focus == s->trigger_hit || (component && descendant(*services_, *component, s->snapshot.trigger));
        if (in_trigger && event.modifiers == input::KeyModifier::none &&
            (event.key == input::Key::down || event.key == input::Key::up) && s->mode != DropdownTriggerMode::Manual) {
            s->focus_last = event.key == input::Key::up;
            s->context_anchor.reset();
            request_open(id, true, DropdownOpenSource::Trigger);
            return true;
        }
        if (event.key == input::Key::escape && s->snapshot.geometry.visible) {
            if (!escape || !descendant(*services_, *escape, id)) {
                escape = id;
            }
        }
        if (event.key == input::Key::tab && s->snapshot.geometry.visible && component &&
            descendant(*services_, *component, s->snapshot.popup)) {
            std::vector<input::InteractionId> stops;
            for (auto hit : services_->interactions().declaration_order()) {
                const auto* candidate = services_->interactions().find(hit);
                if (candidate && candidate->tab_stop && can_focus(*services_, hit) &&
                    !descendant(*services_, candidate->component, s->snapshot.popup)) {
                    stops.push_back(hit);
                }
            }
            const auto found = s->restore_target ? std::ranges::find(stops, *s->restore_target) : stops.end();
            const bool reverse = input::has_modifier(event.modifiers, input::KeyModifier::shift);
            std::optional<input::InteractionId> target;
            if (!stops.empty()) {
                const auto index = found == stops.end() ? (reverse ? 0 : stops.size() - 1)
                                                        : static_cast<std::size_t>(found - stops.begin());
                target = stops[(index + (reverse ? stops.size() - 1 : 1)) % stops.size()];
            }
            request_open(id, false, DropdownOpenSource::Outside, false);
            if (target && can_focus(*services_, *target)) {
                services_->focus().defer_focus(target, input::FocusModality::keyboard);
            }
            return true;
        }
    }
    if (escape) {
        request_open(*escape, false, DropdownOpenSource::Escape);
        return true;
    }
    return false;
}

void DropdownComponentHost::on_pointer_input(const input::PointerInputEvent& event,
                                             std::optional<input::InteractionId> hit,
                                             std::optional<input::InteractionId> origin) {
    if (!window_active_ || (event.action != input::PointerAction::down && event.action != input::PointerAction::up)) {
        return;
    }
    const auto ids = mounted_;
    for (auto id : ids) {
        auto* s = services_->components().state<DropdownState>(id);
        if (!s || s->disabled || !services_->interactions().component_active(id)) {
            continue;
        }
        const auto* record = hit ? services_->interactions().find(*hit) : nullptr;
        const bool in_trigger =
            hit == s->trigger_hit || (record && descendant(*services_, record->component, s->snapshot.trigger));
        const bool in_popup =
            s->snapshot.geometry.visible && ((record && descendant(*services_, record->component, s->snapshot.popup)) ||
                                             inside(s->snapshot.geometry.bounds, event.x, event.y));
        if (event.action == input::PointerAction::down && s->snapshot.geometry.visible && !in_trigger && !in_popup) {
            request_open(id, false, DropdownOpenSource::Outside, false);
        } else if (event.action == input::PointerAction::up && in_trigger && hit) {
            if (event.button == input::PointerButton::primary && origin == hit && click_trigger(s->mode)) {
                s->context_anchor.reset();
                request_open(id, !s->snapshot.geometry.visible, DropdownOpenSource::Trigger);
            } else if (event.button == input::PointerButton::secondary && context_trigger(s->mode)) {
                s->context_anchor = runtime::Point{event.x, event.y};
                s->needs_measure = true;
                request_open(id, true, DropdownOpenSource::Trigger);
            }
        }
    }
}

void DropdownComponentHost::position_window_layers(runtime::Size viewport, runtime::Rect clip) {
    auto ids = mounted_;
    // Child components finish mounting before their parent. Position the owner
    // first so nested triggers have committed coordinates in this same frame.
    std::reverse(ids.begin(), ids.end());
    for (auto id : ids) {
        const auto* before = services_->components().state<DropdownState>(id);
        if (before && before->snapshot.geometry.visible && !services_->interactions().component_active(id)) {
            suppress(id);
        } else {
            refresh(id);
        }
        auto* s = services_->components().state<DropdownState>(id);
        if (!s || !s->snapshot.geometry.visible) {
            continue;
        }
        const auto& token = services_->components().theme_scope(id)->snapshot().dropdown();
        auto& engine = services_->layout();
        const auto popup = services_->components().root(s->snapshot.popup);
        const bool measure = s->needs_measure || s->viewport != viewport ||
                             services_->nodes().require(popup).measure_generation != engine.generation();
        if (measure) {
            const float padding = std::min(token.metrics.padding, std::min(clip.width, clip.height) / 2);
            engine.set_layout(popup, layout::BoxLayout{{padding, padding, padding, padding}});
            s->popup_size = engine.measure_child(popup, {std::min(token.metrics.min_width, clip.width), clip.width, 0,
                                                         std::min(token.metrics.max_height, clip.height)});
            s->needs_measure = false;
            s->viewport = viewport;
        }
        const auto& node = services_->nodes().require(s->node);
        runtime::Rect anchor{node.bounds.x + node.translation.x, node.bounds.y + node.translation.y, node.bounds.width,
                             node.bounds.height};
        if (s->context_anchor) {
            anchor = {s->context_anchor->x, s->context_anchor->y, 0, 0};
        }
        s->snapshot.geometry = position_tooltip(anchor, s->popup_size, clip, s->placement,
                                                s->context_anchor ? 0 : token.metrics.gap, s->adjust);
        if (measure || services_->nodes().require(popup).bounds != s->snapshot.geometry.bounds) {
            engine.place_child(popup, s->snapshot.geometry.bounds);
        }
        if (s->focus_pending) {
            s->focus_pending = false;
            services_->menu().focus_menu(s->snapshot.menu, s->focus_last);
            s->focus_last = false;
        }
    }
}

void DropdownComponentHost::synchronize_auxiliary_geometry(runtime::Size viewport, runtime::Rect) {
    for (auto id : mounted_) {
        const auto* s = services_->components().state<DropdownState>(id);
        if (!s || !s->snapshot.geometry.visible) {
            continue;
        }
        const auto& token = services_->components().theme_scope(id)->snapshot().dropdown();
        const auto bounds = s->snapshot.geometry.bounds;
        graphics::QuadInstance q;
        q.bounds = {bounds.x, bounds.y, bounds.width, bounds.height};
        q.color = {token.background.red(), token.background.green(), token.background.blue(), token.background.alpha()};
        q.corner_radius = std::min(token.metrics.border_radius, std::min(bounds.width, bounds.height) / 2);
        static_cast<void>(services_->surfaces().update_surface(s->surface, std::span{&q, 1}));
        component::RetainedSurfaceEffects effects;
        effects.focus_enabled = false;
        effects.shape = {{bounds.x, bounds.y, bounds.width, bounds.height}, q.corner_radius};
        effects.shadows = token.shadow;
        effects.ancestor_clip = graphics::EffectClip{1, {0, 0, viewport.width, viewport.height}};
        static_cast<void>(services_->surfaces().update_effects(s->surface, effects));
    }
}
} // namespace ryn::detail

namespace ryn {
void Dropdown(DropdownProps props, DropdownTrigger trigger) {
    if (!detail::active_dropdowns) {
        throw std::logic_error("Dropdown requires window component services");
    }
    detail::active_dropdowns->mount(props, trigger);
}
} // namespace ryn
