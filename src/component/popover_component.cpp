#include "component/popover_component.hpp"
#include "component/select_component.hpp"
#include "component/auto_complete_component.hpp"
#include "component/popover_model.hpp"
#include "component/table_component.hpp"
#include "component/date_picker_component.hpp"
#include "component/time_picker_component.hpp"
#include "runtime/layout_style_adapter.hpp"
#include "runtime/prop_connection.hpp"
#include <algorithm>
#include <cmath>

namespace ryn::detail {
namespace {
thread_local PopoverComponentHost* active_popovers{};

struct PopoverState final {
    PopoverState(bool controlled, bool open) : model(controlled, open) {}

    PopoverModel model;
    PopoverSnapshot view;
    runtime::NodeId node;
    input::InteractionId trigger_hit;
    input::InteractionId popup_hit;
    component::RetainedSurfaceId surface;
    Signal<String> title{String{}};
    Signal<String> content{String{}};
    Signal<runtime::SemanticForeground> text_color{{0, 0, 0, 1}};
    Signal<runtime::SemanticForeground> title_color{{0, 0, 0, 1}};
    Signal<runtime::SemanticForeground> arrow_color{{1, 1, 1, 1}};
    Signal<runtime::SemanticTypography> font{runtime::SemanticTypography{}};
    Signal<runtime::SemanticTypography> title_font{runtime::SemanticTypography{}};
    Signal<runtime::SemanticTypography> arrow_font{runtime::SemanticTypography{}};
    Signal<String> arrow_glyph{String{u8"\uF000"}};
    std::array<theme_runtime::Subscription, 5> subscriptions;
    bool rich_title{};
    bool rich_content{};
    bool disabled{};
    bool native_trigger{};
    bool arrow{true};
    bool point_at_center{};
    bool adjust{true};
    bool needs_measure{true};
    bool hovered{};
    bool click_open{};
    bool focus_pending{};
    bool restore{true};
    std::optional<bool> press_open;
    PopoverTriggers triggers{true, false, false};
    Duration enter{Duration::milliseconds(100)};
    Duration leave{Duration::milliseconds(100)};
    PopoverPlacement placement{PopoverPlacement::Top};
    runtime::Rect clip;
    runtime::Size popup_size;
    std::optional<input::InteractionId> restore_target;
    std::function<void(bool)> on_open;
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

std::optional<runtime::ComponentId> nearest_layer(WindowComponentServices& services, runtime::ComponentId child) {
    for (auto current = std::optional{child}; current && services.components().contains(*current);
         current = services.components().parent(*current)) {
        if (services.components().is_window_layer_root(*current)) {
            return current;
        }
    }
    return {};
}

bool can_focus(WindowComponentServices& services, input::InteractionId hit) {
    const auto* r = services.interactions().find(hit);
    return r && r->eligible && r->focusable && services.interactions().branch_active(hit);
}

std::optional<input::InteractionId> first_focus(WindowComponentServices& services, runtime::ComponentId root) {
    for (auto hit : services.interactions().declaration_order()) {
        const auto* r = services.interactions().find(hit);
        if (r && can_focus(services, hit) && descendant(services, r->component, root)) {
            return hit;
        }
    }
    return {};
}

bool trigger_can_activate(WindowComponentServices& services, runtime::ComponentId id,
                          std::optional<input::InteractionId> hit) {
    const auto* s = services.components().state<PopoverState>(id);
    const auto* r = hit ? services.interactions().find(*hit) : nullptr;
    if (!s || !r || !services.interactions().component_active(id)) {
        return false;
    }
    if (!s->native_trigger) {
        return *hit == s->trigger_hit;
    }
    const auto root = s->view.trigger;
    const auto component = r->component;
    for (auto candidate : services.interactions().declaration_order()) {
        const auto* control = services.interactions().find(candidate);
        if (control && control->focusable && control->eligible && services.interactions().branch_active(candidate) &&
            descendant(services, control->component, root) && descendant(services, component, control->component)) {
            const auto handlers = control->focus_handlers;
            // The gate may synchronously unmount/rebind either the wrapper or
            // native control. Validate all original generations after it runs.
            const bool allowed = !handlers || !handlers->activation_allowed || handlers->activation_allowed();
            const auto* owner = services.components().state<PopoverState>(id);
            const auto* live = services.interactions().find(candidate);
            const auto* target = services.interactions().find(*hit);
            return allowed && owner && live && target && live->eligible &&
                   services.interactions().branch_active(candidate) &&
                   descendant(services, live->component, owner->view.trigger) &&
                   descendant(services, target->component, live->component);
        }
    }
    return false;
}

bool has_title(const PopoverState& s) {
    return s.rich_title || !s.title.get().empty();
}

bool has_content(const PopoverState& s) {
    return s.rich_content || !s.content.get().empty();
}

runtime::SemanticForeground foreground(Color c) {
    return {c.red(), c.green(), c.blue(), c.alpha()};
}

void validate_placement(PopoverPlacement p) {
    if (p < PopoverPlacement::Top || p > PopoverPlacement::RightBottom) {
        throw std::invalid_argument("Popover placement is invalid");
    }
}

runtime::ComponentId group(WindowComponentServices& services) {
    auto& build = runtime::require_component_build_context();
    const auto id = build.mount_component<int>(0);
    const auto node = build.root(id);
    services.layout().set_layout(node, layout::BoxLayout{{}, true});
    build.on_resource_cleanup(id, [&services, node] { services.layout().remove_layout(node); });
    return id;
}
} // namespace

PopoverComponentHost::PopoverComponentHost(WindowComponentServices& services) : services_(&services) {
    services.attach(*this);
}

PopoverComponentHost::~PopoverComponentHost() {
    services_->detach(*this);
}

void* PopoverComponentHost::begin_mount() noexcept {
    auto* previous = active_popovers;
    active_popovers = this;
    return previous;
}

void PopoverComponentHost::end_mount(void* previous) noexcept {
    active_popovers = static_cast<PopoverComponentHost*>(previous);
}

void PopoverComponentHost::on_destroy() noexcept {
    std::erase_if(mounted_, [this](auto id) { return !services_->components().contains(id); });
}

void PopoverComponentHost::on_dispose() noexcept {
    mounted_.clear();
}

PopoverSnapshot PopoverComponentHost::snapshot(runtime::ComponentId id) const {
    const auto* s = services_->components().state<PopoverState>(id);
    if (!s) {
        throw std::out_of_range("Popover component is stale");
    }
    return s->view;
}

void PopoverComponentHost::refresh(runtime::ComponentId id) {
    auto* s = services_->components().state<PopoverState>(id);
    if (!s || !services_->components().scope(id).active()) {
        return;
    }
    const bool visible = s->model.open() && !s->disabled && (has_title(*s) || has_content(*s)) && window_active_ &&
                         services_->interactions().component_active(id);
    const bool changed = visible != s->view.geometry.visible;
    s->view.geometry.visible = visible;
    const auto popup = s->view.popup;
    bool structure = services_->components().set_branch_active(popup, visible);
    structure |= services_->components().set_branch_active(s->view.arrow, visible && s->arrow);
    structure |= services_->components().set_branch_active(s->view.title, has_title(*s));
    structure |= services_->components().set_branch_active(s->view.content, has_content(*s));
    services_->nodes().require(services_->components().root(s->view.title)).out_of_flow = !has_title(*s);
    services_->nodes().require(services_->components().root(s->view.content)).out_of_flow = !has_content(*s);
    services_->interactions().set_eligible(s->trigger_hit, !s->disabled);
    if (structure) {
        s->needs_measure = true;
        services_->mark_scene_structure_dirty();
        services_->dirty().invalidate(s->node, runtime::DirtyFlags::Measure | runtime::DirtyFlags::HitTest);
    }
    if (!changed) {
        return;
    }
    const auto focused = services_->focus().state().focused;
    const auto* record = focused ? services_->interactions().find(*focused) : nullptr;
    if (visible) {
        s->restore_target = record && descendant(*services_, record->component, s->view.trigger)
                                ? focused
                                : first_focus(*services_, s->view.trigger);
        if (!s->restore_target && can_focus(*services_, s->trigger_hit)) {
            s->restore_target = s->trigger_hit;
        }
        services_->set_input_blocked(popup, false);
        return;
    }
    s->focus_pending = false;
    const bool restore = s->restore && record && descendant(*services_, record->component, popup);
    const auto target = s->restore_target;
    s->restore_target.reset();
    // Own this exact popup root, never erase an enclosing/foreign block.
    // Boundary reconciliation suppresses all descendant popup hosts, including
    // controlled defaultOpen children which never had focus.
    services_->set_input_blocked(popup, true);
    // Boundary/focus callbacks can destroy or reopen us synchronously.
    s = services_->components().state<PopoverState>(id);
    if (restore && s && !s->view.geometry.visible && target && can_focus(*services_, *target) &&
        (!services_->focus().state().focused || services_->focus().state().focused == focused)) {
        services_->focus().defer_focus(target, input::FocusModality::keyboard);
    }
}

void PopoverComponentHost::request_open(runtime::ComponentId id, bool open, bool restore, bool keyboard) {
    auto* s = services_->components().state<PopoverState>(id);
    if (!s || !services_->components().scope(id).active() ||
        (open && (s->disabled || !window_active_ || !services_->interactions().component_active(id)))) {
        return;
    }
    s->restore = restore;
    if (open && keyboard && !s->view.geometry.visible) {
        s->focus_pending = true;
    }
    const bool notify = s->model.request(open);
    const auto callback = s->on_open;

    struct ManualRequestGuard final {
        WindowComponentServices& services;
        runtime::ComponentId id;

        ~ManualRequestGuard() noexcept {
            auto* live = services.components().state<PopoverState>(id);
            if (live && !live->triggers.hover && !live->triggers.focus && !live->triggers.click) {
                live->model.settle_request();
            }
        }
    } guard{*services_, id};

    refresh(id);
    if (notify && callback && services_->components().contains(id) && services_->components().scope(id).active()) {
        callback(open);
    }
}

void PopoverComponentHost::suppress(runtime::ComponentId id) {
    auto* s = services_->components().state<PopoverState>(id);
    if (!s) {
        return;
    }
    s->restore = false;
    s->hovered = false;
    s->click_open = false;
    s->press_open.reset();
    const bool notify = s->model.suppress();
    const auto callback = s->on_open;
    refresh(id);
    if (notify && callback && services_->components().contains(id) && services_->components().scope(id).active()) {
        callback(false);
    }
}

void PopoverComponentHost::observe(runtime::ComponentId id, animation::AnimationTime time) {
    auto* s = services_->components().state<PopoverState>(id);
    if (!s || !services_->components().scope(id).active()) {
        return;
    }
    if (!window_active_ || s->disabled || !services_->interactions().component_active(id)) {
        suppress(id);
        return;
    }
    const auto focused = services_->focus().state().focused;
    const auto* record = focused ? services_->interactions().find(*focused) : nullptr;
    const bool focus = record && descendant(*services_, record->component, id);
    const bool content_focus =
        record && s->view.geometry.visible && descendant(*services_, record->component, s->view.popup);
    const bool desire = s->click_open || (s->triggers.hover && s->hovered) || (s->triggers.focus && focus) ||
                        ((s->triggers.hover || s->triggers.focus) && content_focus);
    if (has_title(*s) || has_content(*s)) {
        s->model.observe(desire, s->triggers.focus && focus, time, s->enter, s->leave);
        if (const auto due = s->model.due(time)) {
            request_open(id, *due);
        }
    }
    refresh(id);
}

void PopoverComponentHost::update_theme(runtime::ComponentId id, bool geometry) {
    auto* s = services_->components().state<PopoverState>(id);
    if (!s) {
        return;
    }
    const auto theme = services_->components().theme_scope(id);
    const auto token = theme->snapshot().popover();
    if (geometry) {
        s->needs_measure = true;
        services_->dirty().invalidate(s->node, runtime::DirtyFlags::Measure);
    }
    if (services_->components().set_window_layer(s->view.popup, token.z_index_popup) |
        services_->components().set_window_layer(s->view.arrow, token.z_index_popup)) {
        services_->mark_scene_structure_dirty();
    }
    services_->dirty().invalidate(s->node, runtime::DirtyFlags::Material);
    s->text_color.set(foreground(token.text));
    s = services_->components().state<PopoverState>(id);
    if (!s) {
        return;
    }
    s->title_color.set(foreground(token.title));
    s = services_->components().state<PopoverState>(id);
    if (!s) {
        return;
    }
    s->arrow_color.set(foreground(token.background));
    s = services_->components().state<PopoverState>(id);
    if (!s) {
        return;
    }
    s->font.set({token.font_family, token.font_weight, false, token.font_size, token.line_height});
    s = services_->components().state<PopoverState>(id);
    if (!s) {
        return;
    }
    s->title_font.set({token.font_family, token.title_font_weight, false, token.font_size, token.line_height});
}

void PopoverComponentHost::mount(const PopoverProps& props, const PopoverTrigger& trigger, const PopoverSlots& slots) {
    if (props.open_ && props.default_open_) {
        throw std::invalid_argument("Popover cannot combine open and defaultOpen");
    }
    if ((slots.title && props.explicit_title_) || (slots.content && props.explicit_content_) ||
        (props.triggers_ && props.explicit_trigger_)) {
        throw std::invalid_argument("Popover declarations conflict");
    }
    validate_placement(read_prop(props.placement_));
    validate_popover_delay(read_prop(props.enter_));
    validate_popover_delay(read_prop(props.leave_));
    const auto actions = props.triggers_ ? read_prop(*props.triggers_) : popover_triggers(read_prop(props.trigger_));
    const bool initial_open = props.open_ ? read_prop(*props.open_) : props.default_open_.value_or(false);
    const auto initial_title = read_prop(props.title_);
    const auto initial_content = read_prop(props.content_);
    const bool disabled = read_prop(props.disabled_);
    auto& services = *services_;
    auto& build = runtime::require_component_build_context();
    const auto id = build.mount_component<PopoverState>(props.open_.has_value(), initial_open);
    auto& s = build.state<PopoverState>(id);
    s.node = build.root(id);
    s.rich_title = slots.title.has_value();
    s.rich_content = slots.content.has_value();
    s.disabled = disabled;
    s.triggers = actions;
    s.on_open = props.on_open_;
    s.title.set(initial_title);
    s.content.set(initial_content);
    s.trigger_hit = services.interactions().create({id, s.node, {}, !disabled, true, {}, true, true});
    const auto fragment = build.register_scene_fragment(id, runtime::SceneFragmentPlacement::before_children);
    services.scene_composer().set_fragment(fragment, {}, s.trigger_hit);
    build.on_resource_cleanup(id, [&services, node = s.node, hit = s.trigger_hit, fragment] {
        services.pointer().cancel_interaction(hit);
        services.interactions().remove(hit);
        services.scene_composer().remove_fragment(fragment);
        services.layout().remove_layout(node);
    });
    runtime::connect_layout_style(build.scope(id), props.layout_, s.node, services.nodes(), services.dirty());
    build.mount_slot(
        id, Content{[&] {
            auto& children = runtime::require_component_build_context();
            s.view.trigger = group(services);
            children.mount_slot(s.view.trigger, trigger);
            for (auto hit : services.interactions().declaration_order()) {
                const auto* r = services.interactions().find(hit);
                if (r && r->focusable && descendant(services, r->component, s.view.trigger)) {
                    s.native_trigger = true;
                    break;
                }
            }
            services.interactions().set_tab_stop(s.trigger_hit, !s.native_trigger);
            services.interactions().set_focusable(s.trigger_hit, !s.native_trigger);
            s.view.popup = group(services);
            const auto popup_node = children.root(s.view.popup);
            services.components().set_window_layer(s.view.popup, 1030);
            services.components().set_branch_active(s.view.popup, false);
            services.interactions().set_input_blocked(s.view.popup, true);
            services.nodes().require(popup_node).clip_content = true;
            services.nodes().require(popup_node).clip_decorations = false;
            s.popup_hit = services.interactions().create({s.view.popup, popup_node, {}, true, false, {}, false, false});
            const auto surface =
                children.register_scene_fragment(s.view.popup, runtime::SceneFragmentPlacement::before_children);
            graphics::QuadInstance initial;
            initial.opacity = 0;
            component::RetainedSurfaceEffects effects;
            effects.focus_enabled = false;
            s.surface = services.surfaces().create(s.view.popup, popup_node, surface, s.popup_hit,
                                                   std::span{&initial, 1}, effects);
            children.on_resource_cleanup(s.view.popup,
                                         [&services, popup = s.view.popup, hit = s.popup_hit, surface = s.surface] {
                                             services.interactions().set_input_blocked(popup, false);
                                             services.pointer().cancel_interaction(hit);
                                             services.interactions().remove(hit);
                                             services.surfaces().destroy(surface);
                                         });
            children.mount_slot(s.view.popup, Content{[&] {
                                    auto& popup = runtime::require_component_build_context();
                                    s.view.title = group(services);
                                    popup.mount_slot_with_semantic_text_style(
                                        s.view.title,
                                        slots.title ? Content{SlotContentAccess::function(*slots.title)}
                                                    : Content{[&s] { Text(TextProps{}.content(s.title)); }},
                                        s.title_color, s.title_font);
                                    s.view.content = group(services);
                                    popup.mount_slot_with_semantic_text_style(
                                        s.view.content,
                                        slots.content ? Content{SlotContentAccess::function(*slots.content)}
                                                      : Content{[&s] { Text(TextProps{}.content(s.content)); }},
                                        s.text_color, s.font);
                                }});
            services.layout().set_layout(
                popup_node,
                layout::ComponentLayout{
                    [this, id](layout::LayoutEngine& engine, runtime::NodeId, layout::Constraints limits) {
                        const auto* state = services_->components().state<PopoverState>(id);
                        const auto& t = services_->components().theme_scope(id)->snapshot().popover();
                        const float padding = std::min(t.padding, std::min(limits.max_width, limits.max_height) / 2);
                        const float width = std::max(0.0F, limits.max_width - 2 * padding);
                        const float height = std::max(0.0F, limits.max_height - 2 * padding);
                        const auto title = has_title(*state)
                                               ? engine.measure_child(services_->components().root(state->view.title),
                                                                      {0, width, 0, height})
                                               : runtime::Size{};
                        const auto content =
                            has_content(*state)
                                ? engine.measure_child(services_->components().root(state->view.content),
                                                       {0, width, 0, height})
                                : runtime::Size{};
                        const float minimum = has_title(*state) ? std::min(t.title_min_width, width) : 0;
                        return limits.constrain({std::max({title.width, content.width, minimum}) + 2 * padding,
                                                 title.height + content.height +
                                                     (has_title(*state) && has_content(*state) ? t.title_gap : 0) +
                                                     2 * padding});
                    },
                    [this, id](layout::LayoutEngine& engine, runtime::NodeId, runtime::Rect rect) {
                        const auto* state = services_->components().state<PopoverState>(id);
                        const auto& t = services_->components().theme_scope(id)->snapshot().popover();
                        const float padding = std::min(t.padding, std::min(rect.width, rect.height) / 2);
                        const float width = std::max(0.0F, rect.width - 2 * padding);
                        const float height = std::max(0.0F, rect.height - 2 * padding);
                        const auto title_node = services_->components().root(state->view.title);
                        const auto content_node = services_->components().root(state->view.content);
                        const auto title = has_title(*state)
                                               ? engine.measure_child(title_node, {width, width, 0, height})
                                               : runtime::Size{};
                        const float title_height = std::min(title.height, height);
                        const float gap =
                            has_title(*state) && has_content(*state) ? std::min(t.title_gap, height - title_height) : 0;
                        const float content_height = std::max(0.0F, height - title_height - gap);
                        if (has_title(*state)) {
                            engine.place_child(title_node, {rect.x + padding, rect.y + padding, width, title_height});
                        }
                        if (has_content(*state)) {
                            static_cast<void>(engine.measure_child(content_node, {width, width, 0, content_height}));
                            engine.place_child(content_node, {rect.x + padding, rect.y + padding + title_height + gap,
                                                              width, content_height});
                        }
                    }});
            s.view.arrow = group(services);
            services.components().set_window_layer(s.view.arrow, 1030);
            services.components().set_branch_active(s.view.arrow, false);
            children.mount_slot_with_semantic_text_style(
                s.view.arrow, Content{[&s] { mount_text_component(TextProps{}.content(s.arrow_glyph), true); }},
                s.arrow_color, s.arrow_font);
        }});
    services.layout().set_layout(
        s.node, layout::ComponentLayout{
                    [this, id](layout::LayoutEngine& engine, runtime::NodeId, layout::Constraints limits) {
                        const auto* state = services_->components().state<PopoverState>(id);
                        return engine.measure_child(services_->components().root(state->view.trigger), limits);
                    },
                    [this, id](layout::LayoutEngine& engine, runtime::NodeId, runtime::Rect rect) {
                        const auto* state = services_->components().state<PopoverState>(id);
                        engine.place_child(services_->components().root(state->view.trigger), rect);
                    }});
    mounted_.push_back(id);
    const auto connect = [this, id]<typename T, typename Apply>(const Prop<T>& prop, Apply&& apply) {
        if (services_->components().contains(id) && services_->components().scope(id).active()) {
            connect_prop(services_->components().scope(id), prop, std::forward<Apply>(apply));
        }
    };
    const auto text = [&](const Prop<String>& prop, Signal<String> PopoverState::* field) {
        connect(prop, [this, id, field](String value) {
            auto* state = services_->components().state<PopoverState>(id);
            if (!state) {
                return;
            }
            state->needs_measure = true;
            services_->dirty().invalidate(state->node, runtime::DirtyFlags::Measure);
            (state->*field).set(std::move(value));
            refresh(id);
        });
    };
    text(props.title_, &PopoverState::title);
    text(props.content_, &PopoverState::content);
    if (props.open_) {
        connect(*props.open_, [this, id](bool value) {
            if (auto* state = services_->components().state<PopoverState>(id)) {
                state->model.reconcile(value);
                state->click_open = false;
                state->restore = true;
                refresh(id);
            }
        });
    }
    connect(props.disabled_, [this, id](bool value) {
        if (auto* state = services_->components().state<PopoverState>(id)) {
            state->disabled = value;
            if (value) {
                suppress(id);
            } else {
                refresh(id);
            }
        }
    });
    const auto actions_update = [this, id](PopoverTriggers value) {
        if (auto* state = services_->components().state<PopoverState>(id); state && state->triggers != value) {
            state->triggers = value;
            state->click_open = false;
            state->model.reset_observation();
            request_open(id, false);
        }
    };
    if (props.triggers_) {
        connect(*props.triggers_, actions_update);
    } else {
        connect(props.trigger_,
                [actions_update](PopoverTriggerMode value) { actions_update(popover_triggers(value)); });
    }
    const auto flag = [&](const Prop<bool>& prop, bool PopoverState::* field) {
        connect(prop, [this, id, field](bool value) {
            if (auto* state = services_->components().state<PopoverState>(id)) {
                state->*field = value;
                state->needs_measure = true;
                services_->dirty().invalidate(state->node, runtime::DirtyFlags::Geometry);
                refresh(id);
            }
        });
    };
    flag(props.arrow_, &PopoverState::arrow);
    flag(props.point_at_center_, &PopoverState::point_at_center);
    flag(props.adjust_, &PopoverState::adjust);
    connect(props.placement_, [this, id](PopoverPlacement value) {
        validate_placement(value);
        if (auto* state = services_->components().state<PopoverState>(id)) {
            state->placement = value;
            services_->dirty().invalidate(state->node, runtime::DirtyFlags::Geometry);
        }
    });
    const auto delay = [&](const Prop<Duration>& prop, Duration PopoverState::* field) {
        connect(prop, [this, id, field](Duration value) {
            validate_popover_delay(value);
            if (auto* state = services_->components().state<PopoverState>(id)) {
                if (state->*field != value) {
                    state->*field = value;
                    state->model.retime(services_->animation_time(), state->enter, state->leave);
                    services_->request_frame();
                }
            }
        });
    };
    delay(props.enter_, &PopoverState::enter);
    delay(props.leave_, &PopoverState::leave);
    if (!services_->components().contains(id) || !services_->components().scope(id).active()) {
        return;
    }
    const auto theme = build.theme_scope();
    const std::array accessors{&theme_runtime::ThemeScope::popover_colors, &theme_runtime::ThemeScope::popover_metrics,
                               &theme_runtime::ThemeScope::popover_typography,
                               &theme_runtime::ThemeScope::popover_effects, &theme_runtime::ThemeScope::popover_order};
    for (std::size_t i = 0; i < accessors.size(); ++i) {
        s.subscriptions[i] = theme->capture(
            [this, id, geometry = i == 1 || i == 2](theme_runtime::DirtyPhase) { update_theme(id, geometry); },
            [theme, accessor = accessors[i]] { static_cast<void>((theme.get()->*accessor)()); });
    }
    update_theme(id, true);
    refresh(id);
}

void PopoverComponentHost::on_window_active(bool active) {
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

void PopoverComponentHost::on_input_boundary_changed() {
    const auto ids = mounted_;
    for (auto id : ids) {
        if (!services_->interactions().component_active(id)) {
            suppress(id);
        } else {
            refresh(id);
        }
    }
}

void PopoverComponentHost::prepare_auxiliary_layout() {
    const auto ids = mounted_;
    for (auto id : ids) {
        observe(id, services_->animation_time());
    }
}

bool PopoverComponentHost::on_keyboard_input(const input::KeyboardInputEvent& event) {
    if (!window_active_ || mounted_.empty()) {
        return false;
    }
    const auto focused = services_->focus().state();
    const auto* record = focused.focused ? services_->interactions().find(*focused.focused) : nullptr;
    const auto component = record ? std::optional{record->component} : std::nullopt;
    const auto handlers = record ? record->focus_handlers : nullptr;
    const auto ids = mounted_;
    for (auto id : ids) {
        auto* s = services_->components().state<PopoverState>(id);
        if (!s || s->disabled || !s->triggers.click || !services_->interactions().component_active(id)) {
            continue;
        }
        const bool wrapper = focused.focused == s->trigger_hit;
        const bool trigger = wrapper || (component && descendant(*services_, *component, s->view.trigger));
        if (trigger && event.modifiers == input::KeyModifier::none && !event.repeat &&
            (event.key == input::Key::enter || event.key == input::Key::space)) {
            const bool native = handlers && handlers->activate && !handlers->text_edit;
            const bool activate = wrapper ? event.action == input::KeyAction::down
                                  : event.key == input::Key::enter
                                      ? event.action == input::KeyAction::down
                                      : event.action == input::KeyAction::up && focused.keyboard_pressed;
            if ((wrapper || native) && activate && trigger_can_activate(*services_, id, focused.focused)) {
                s = services_->components().state<PopoverState>(id);
                if (s && services_->components().scope(id).active() && services_->interactions().component_active(id)) {
                    const bool open = !s->view.geometry.visible;
                    s->click_open = open;
                    if (!open) {
                        s->model.dismiss();
                    }
                    request_open(id, open, true, true);
                }
            }
            return wrapper;
        }
    }
    if (event.key == input::Key::escape && event.action == input::KeyAction::down) {
        // Paint order already accounts for nested layer ownership and Theme order.
        const auto traversal = services_->components().paint_traversal();
        for (auto it = traversal.rbegin(); it != traversal.rend(); ++it) {
            for (auto id : ids) {
                auto* s = services_->components().state<PopoverState>(id);
                if (s && s->view.geometry.visible && it->component == s->view.popup) {
                    s->click_open = false;
                    s->model.dismiss();
                    request_open(id, false);
                    return true;
                }
            }
        }
    }
    return false;
}

void PopoverComponentHost::on_pointer_input(const input::PointerInputEvent& event,
                                            std::optional<input::InteractionId> hit,
                                            std::optional<input::InteractionId> origin) {
    if (!window_active_) {
        return;
    }
    const auto ids = mounted_;
    for (auto id : ids) {
        auto* s = services_->components().state<PopoverState>(id);
        if (!s || s->disabled || !services_->interactions().component_active(id)) {
            continue;
        }
        const auto* r = hit ? services_->interactions().find(*hit) : nullptr;
        const bool trigger = hit == s->trigger_hit || (r && descendant(*services_, r->component, s->view.trigger));
        const bool popup = r && descendant(*services_, r->component, s->view.popup);
        // Focus is acquired on pointer down, before the pointer observer. A
        // combined focus/click trigger must use this press's opening intent,
        // rather than toggle the focus-opened popup closed on pointer up.
        if (event.action == input::PointerAction::down && event.button == input::PointerButton::primary) {
            s->press_open = trigger ? std::optional{s->view.geometry.visible} : std::nullopt;
        }
        if (event.action != input::PointerAction::cancel) {
            s->hovered = trigger || popup;
        } else {
            s->hovered = false;
        }
        if (event.action == input::PointerAction::down && s->view.geometry.visible && !trigger && !popup) {
            s->click_open = false;
            s->model.dismiss();
            request_open(id, false, false);
        } else if (event.action == input::PointerAction::up && event.button == input::PointerButton::primary &&
                   trigger && hit && hit == origin && s->triggers.click && trigger_can_activate(*services_, id, hit)) {
            s = services_->components().state<PopoverState>(id);
            if (s && services_->components().scope(id).active() && services_->interactions().component_active(id)) {
                const bool open = !s->press_open.value_or(s->view.geometry.visible);
                s->press_open.reset();
                s->click_open = open;
                if (!open) {
                    s->model.dismiss();
                }
                request_open(id, open);
            }
        }
        observe(id, services_->animation_time());
        s = services_->components().state<PopoverState>(id);
        if (s && (event.action == input::PointerAction::cancel || event.action == input::PointerAction::up)) {
            s->press_open.reset();
        }
        services_->request_frame();
    }
}

bool PopoverComponentHost::consume_scroll_boundary(const input::ScrollInputEvent& event) {
    const auto hit = services_->hit_test().hit_test({event.x, event.y});
    const auto* r = hit ? services_->interactions().find(*hit) : nullptr;
    if (!r) {
        return false;
    }
    const auto owner = nearest_layer(*services_, r->component);
    for (auto id : mounted_) {
        const auto* s = services_->components().state<PopoverState>(id);
        if (s && s->view.geometry.visible && owner == s->view.popup) {
            return true;
        }
    }
    return false;
}

std::size_t PopoverComponentHost::tick_auxiliary(animation::AnimationTime time) {
    std::size_t changed{};
    const auto ids = mounted_;
    for (auto id : ids) {
        const auto* s = services_->components().state<PopoverState>(id);
        if (!s) {
            continue;
        }
        const bool before = s->view.geometry.visible;
        observe(id, time);
        s = services_->components().state<PopoverState>(id);
        if (s && before != s->view.geometry.visible) {
            ++changed;
        }
    }
    return changed;
}

std::optional<animation::AnimationTime> PopoverComponentHost::next_auxiliary_deadline() const {
    std::optional<animation::AnimationTime> result;
    for (auto id : mounted_) {
        const auto* s = services_->components().state<PopoverState>(id);
        if (s && s->model.deadline() && (!result || *s->model.deadline() < *result)) {
            result = s->model.deadline();
        }
    }
    return result;
}

bool PopoverComponentHost::owns_escape(runtime::ComponentId ancestor, runtime::ComponentId focused) const {
    for (auto id : mounted_) {
        const auto* s = services_->components().state<PopoverState>(id);
        if (s && s->view.geometry.visible && descendant(*services_, id, ancestor) &&
            descendant(*services_, focused, id)) {
            return true;
        }
    }
    return false;
}

void PopoverComponentHost::position_ancestors(runtime::ComponentId id, runtime::Size viewport, runtime::Rect clip) {
    if (!services_->components().contains(id)) {
        return;
    }
    for (auto p = services_->components().parent(id); p && services_->components().contains(*p);
         p = services_->components().parent(*p)) {
        if (std::ranges::find(mounted_, *p) != mounted_.end()) {
            position_window_layer(*p, viewport, clip);
            return;
        }
        const auto dates = services_->date_picker().mounted();
        if (std::ranges::find(dates, *p) != dates.end()) {
            services_->date_picker().position_window_layer(*p, viewport, clip);
            return;
        }
        const auto times = services_->time_picker().mounted();
        if (std::ranges::find(times, *p) != times.end()) {
            services_->time_picker().position_window_layer(*p, viewport, clip);
            return;
        }
        const auto completions = services_->auto_complete().mounted();
        if (std::ranges::find(completions, *p) != completions.end()) {
            services_->auto_complete().position_window_layer(*p, viewport, clip);
            return;
        }
        const auto selects = services_->select().mounted();
        if (std::ranges::find(selects, *p) != selects.end()) {
            services_->select().position_window_layer(*p, viewport, clip);
            return;
        }
    }
}

void PopoverComponentHost::position_window_layers(runtime::Size viewport, runtime::Rect document_clip) {
    const auto ids = mounted_;
    for (auto id : ids) {
        position_window_layer(id, viewport, document_clip);
    }
}

void PopoverComponentHost::position_window_layer(runtime::ComponentId id, runtime::Size viewport,
                                                 runtime::Rect document_clip) {
    if (std::ranges::find(positioning_, id) != positioning_.end()) {
        return;
    }
    positioning_.push_back(id);

    struct PositionGuard final {
        std::vector<runtime::ComponentId>& stack;

        ~PositionGuard() {
            stack.pop_back();
        }
    } guard{positioning_};

    services_->popover().position_ancestors(id, viewport, document_clip);
    observe(id, services_->animation_time());
    bool positioned{};
    auto* s = services_->components().state<PopoverState>(id);
    if (!s || !s->view.geometry.visible) {
        return;
    }
    const auto& t = services_->components().theme_scope(id)->snapshot().popover();
    const auto popup_node = services_->components().root(s->view.popup);
    const auto clip = services_->component_clip(s->view.popup, viewport, document_clip);
    auto& engine = services_->layout();
    const bool measure = s->needs_measure || s->clip != clip ||
                         services_->nodes().require(popup_node).measure_generation != engine.generation();
    if (measure) {
        s->popup_size = engine.measure_child(popup_node, {0, std::min(t.max_width, clip.width), 0, clip.height});
        s->clip = clip;
        s->needs_measure = false;
    }
    const auto& node = services_->nodes().require(s->node);
    const runtime::Rect anchor{node.bounds.x + node.translation.x, node.bounds.y + node.translation.y,
                               node.bounds.width, node.bounds.height};
    s->view.geometry =
        position_tooltip(anchor, s->popup_size, clip, s->placement, t.gap + (s->arrow ? t.arrow_size : 0), s->adjust,
                         s->arrow && s->point_at_center, std::max(12.0F, t.border_radius + 2));
    if (measure || services_->nodes().require(popup_node).bounds != s->view.geometry.bounds) {
        engine.place_child(popup_node, s->view.geometry.bounds);
        positioned = true;
    }
    const auto bounds = s->view.geometry.bounds;
    const int side = static_cast<int>(s->view.geometry.placement) / 3;
    const float size = std::min(t.arrow_size, (side < 2 ? bounds.width : bounds.height) / 2);
    const bool arrow = s->arrow && size > 0;
    if (services_->components().set_branch_active(s->view.arrow, arrow)) {
        services_->mark_scene_structure_dirty();
    }
    if (arrow) {
        static const String glyphs[]{String{u8"\uF000"}, String{u8"\uF001"}, String{u8"\uF002"}, String{u8"\uF003"}};
        s->arrow_glyph.set(glyphs[side]);
        const float font = std::max(1.0F, std::round(size * 2));
        const float half = font / 2;
        s->arrow_font.set({SystemFontFamily::ui_sans, 400, false, font, font});
        const auto arrow_node = services_->components().root(s->view.arrow);
        static_cast<void>(engine.measure_child(arrow_node, {0, font, 0, font}));
        const float center = s->view.geometry.arrow_center;
        runtime::Rect rect{bounds.x + center - half, bounds.y + bounds.height, font, font};
        if (side == 1) {
            rect.y = bounds.y - half;
        } else if (side == 2) {
            rect.x = bounds.x + bounds.width;
            rect.y = bounds.y + center - half;
        } else if (side == 3) {
            rect.x = bounds.x - half;
            rect.y = bounds.y + center - half;
        }
        engine.place_child(arrow_node, rect);
    }
    if (s->focus_pending) {
        s->focus_pending = false;
        const auto target = first_focus(*services_, s->view.popup);
        if (target) {
            services_->focus().defer_focus(target, input::FocusModality::keyboard);
        }
    }

    if (positioned && std::ranges::any_of(services_->table().mounted(), [&](auto table) {
            const auto* live = services_->components().state<PopoverState>(id);
            return live && live->view.geometry.visible && descendant(*services_, table, live->view.popup);
        })) {
        services_->table().position_window_layers(viewport, document_clip);
    }
}

void PopoverComponentHost::synchronize_auxiliary_geometry(runtime::Size, runtime::Rect) {
    const auto ids = mounted_;
    for (auto id : ids) {
        const auto* s = services_->components().state<PopoverState>(id);
        if (!s || !s->view.geometry.visible) {
            continue;
        }
        const auto& token = services_->components().theme_scope(id)->snapshot().popover();
        const auto bounds = s->view.geometry.bounds;
        graphics::QuadInstance body;
        body.bounds = {bounds.x, bounds.y, bounds.width, bounds.height};
        body.color = foreground(token.background);
        body.corner_radius = std::min(token.border_radius, std::min(bounds.width, bounds.height) / 2);
        static_cast<void>(services_->surfaces().update_surface(s->surface, std::span{&body, 1}));
        component::RetainedSurfaceEffects effects;
        effects.focus_enabled = false;
        effects.shape = {{bounds.x, bounds.y, bounds.width, bounds.height}, body.corner_radius};
        effects.shadows = token.shadow;
        effects.ancestor_clip = graphics::EffectClip{1, s->clip};
        static_cast<void>(services_->surfaces().update_effects(s->surface, effects));
    }
}
} // namespace ryn::detail

namespace ryn {
void Popover(PopoverProps props, PopoverTrigger trigger, PopoverSlots slots) {
    if (!detail::active_popovers) {
        throw std::logic_error("Popover requires window component services");
    }
    detail::active_popovers->mount(props, trigger, slots);
}
} // namespace ryn
