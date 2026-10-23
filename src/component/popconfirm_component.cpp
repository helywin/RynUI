#include "component/popconfirm_component.hpp"
#include "component/popconfirm_model.hpp"
#include "component/select_component.hpp"
#include "runtime/layout_style_adapter.hpp"
#include "runtime/prop_connection.hpp"
#include <ryn/button.hpp>
#include <ryn/icon.hpp>
#include <algorithm>
#include <cmath>

namespace ryn::detail {
namespace {
thread_local PopconfirmComponentHost* active_popconfirms{};

struct PopconfirmState final {
    PopconfirmState(bool controlled, bool open) : model(controlled, open) {}

    PopconfirmModel model;
    PopconfirmSnapshot snapshot;
    runtime::NodeId node;
    runtime::ComponentId message;
    runtime::ComponentId body;
    runtime::ComponentId title_component;
    runtime::ComponentId footer;
    runtime::ComponentId arrow_component;
    input::InteractionId trigger_hit;
    input::InteractionId popup_hit;
    component::RetainedSurfaceId surface;
    Signal<String> title{String{}};
    Signal<String> description{String{}};
    Signal<String> ok_text{String{u8"OK"}};
    Signal<String> cancel_text{String{u8"Cancel"}};
    Signal<bool> loading{false};
    Signal<runtime::SemanticForeground> text_color{{0, 0, 0, 1}};
    Signal<runtime::SemanticForeground> title_color{{0, 0, 0, 1}};
    Signal<runtime::SemanticForeground> warning_color{{0, 0, 0, 1}};
    Signal<runtime::SemanticForeground> arrow_color{{1, 1, 1, 1}};
    Signal<runtime::SemanticTypography> font{runtime::SemanticTypography{}};
    Signal<runtime::SemanticTypography> title_font{runtime::SemanticTypography{}};
    Signal<runtime::SemanticTypography> arrow_font{runtime::SemanticTypography{}};
    Signal<String> arrow_glyph{String{u8"\uF000"}};
    std::array<theme_runtime::Subscription, 5> subscriptions;
    bool rich_title{};
    bool rich_description{};
    bool disabled{};
    bool show_cancel{true};
    bool arrow{true};
    bool point_at_center{};
    bool adjust{true};
    bool needs_measure{true};
    bool focus_pending{};
    bool restore{true};
    PopconfirmPlacement placement{PopconfirmPlacement::Top};
    runtime::Size viewport;
    runtime::Size popup_size;
    std::optional<input::InteractionId> restore_target;
    std::function<void(bool)> on_open;
    std::function<void()> on_confirm;
    std::function<void()> on_cancel;
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

runtime::SemanticForeground foreground(Color color) {
    return {color.red(), color.green(), color.blue(), color.alpha()};
}

bool can_focus(WindowComponentServices& services, input::InteractionId id) {
    const auto* record = services.interactions().find(id);
    return record && record->eligible && record->focusable && services.interactions().branch_active(id);
}

std::optional<input::InteractionId> first_focus(WindowComponentServices& services, runtime::ComponentId root) {
    for (auto hit : services.interactions().declaration_order()) {
        const auto* record = services.interactions().find(hit);
        if (record && can_focus(services, hit) && descendant(services, record->component, root)) {
            return hit;
        }
    }
    return {};
}

bool inside(runtime::Rect bounds, float x, float y) {
    return x >= bounds.x && y >= bounds.y && x < bounds.x + bounds.width && y < bounds.y + bounds.height;
}

void validate_placement(PopconfirmPlacement placement) {
    if (placement < PopconfirmPlacement::Top || placement > PopconfirmPlacement::RightBottom) {
        throw std::invalid_argument("Popconfirm placement is invalid");
    }
}

runtime::ComponentId group(WindowComponentServices& services, layout::LayoutModel model) {
    auto& build = runtime::require_component_build_context();
    const auto id = build.mount_component<int>(0);
    const auto node = build.root(id);
    services.layout().set_layout(node, std::move(model));
    build.on_resource_cleanup(id, [&services, node] { services.layout().remove_layout(node); });
    return id;
}
} // namespace

PopconfirmComponentHost::PopconfirmComponentHost(WindowComponentServices& services) : services_(&services) {
    services.attach(*this);
}

PopconfirmComponentHost::~PopconfirmComponentHost() {
    services_->detach(*this);
}

void* PopconfirmComponentHost::begin_mount() noexcept {
    auto* previous = active_popconfirms;
    active_popconfirms = this;
    return previous;
}

void PopconfirmComponentHost::end_mount(void* previous) noexcept {
    active_popconfirms = static_cast<PopconfirmComponentHost*>(previous);
}

void PopconfirmComponentHost::on_destroy() noexcept {
    std::erase_if(mounted_, [this](auto id) { return !services_->components().contains(id); });
}

void PopconfirmComponentHost::on_dispose() noexcept {
    mounted_.clear();
}

PopconfirmSnapshot PopconfirmComponentHost::snapshot(runtime::ComponentId id) const {
    const auto* state = services_->components().state<PopconfirmState>(id);
    if (!state) {
        throw std::out_of_range("Popconfirm component is stale");
    }
    return state->snapshot;
}

void PopconfirmComponentHost::refresh(runtime::ComponentId id) {
    auto* s = services_->components().state<PopconfirmState>(id);
    if (!s) {
        return;
    }
    const bool visible =
        s->model.open() && !s->disabled && window_active_ && services_->interactions().component_active(id);
    const bool changed = visible != s->snapshot.geometry.visible;
    s->snapshot.geometry.visible = visible;
    const bool description = s->rich_description || !s->description.get().empty();
    const bool title = s->rich_title || !s->title.get().empty();
    bool structure = services_->components().set_branch_active(s->snapshot.popup, visible);
    structure |= services_->components().set_branch_active(s->arrow_component, visible && s->arrow);
    structure |= services_->components().set_branch_active(s->snapshot.cancel, s->show_cancel);
    structure |= services_->components().set_branch_active(s->snapshot.description, description);
    structure |= services_->components().set_branch_active(s->title_component, title);
    services_->interactions().set_eligible(s->trigger_hit, !s->disabled);
    if (structure) {
        s->needs_measure = true;
        services_->mark_scene_structure_dirty();
        services_->dirty().invalidate(s->node, runtime::DirtyFlags::Measure | runtime::DirtyFlags::HitTest);
    }
    if (!changed) {
        return;
    }
    if (visible) {
        const auto focus = services_->focus().state().focused;
        const auto* record = focus ? services_->interactions().find(*focus) : nullptr;
        s->restore_target = record && descendant(*services_, record->component, s->snapshot.trigger)
                                ? focus
                                : first_focus(*services_, s->snapshot.trigger);
        if (!s->restore_target && can_focus(*services_, s->trigger_hit)) {
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
    // Descendant layers share logical ownership even when positioned outside
    // this popup. Suppress their controlled state before a later parent reopen.
    services_->select().on_input_boundary_changed();
    services_->tooltip().on_input_boundary_changed();
    if (restore && target && can_focus(*services_, *target)) {
        services_->focus().defer_focus(target, input::FocusModality::keyboard);
    } else {
        services_->focus().synchronize();
    }
}

void PopconfirmComponentHost::request_open(runtime::ComponentId id, bool open, bool restore) {
    auto* s = services_->components().state<PopconfirmState>(id);
    if (!s || !services_->components().scope(id).active() ||
        (open && (s->disabled || !window_active_ || !services_->interactions().component_active(id)))) {
        return;
    }
    s->restore = restore;
    const bool notify = s->model.request(open);
    const auto callback = s->on_open;
    refresh(id);
    if (notify && callback && services_->components().contains(id) && services_->components().scope(id).active()) {
        callback(open);
    }
}

void PopconfirmComponentHost::suppress(runtime::ComponentId id) {
    auto* s = services_->components().state<PopconfirmState>(id);
    if (!s) {
        return;
    }
    s->restore = false;
    const bool notify = s->model.suppress();
    const auto callback = s->on_open;
    refresh(id);
    if (notify && callback && services_->components().contains(id) && services_->components().scope(id).active()) {
        callback(false);
    }
}

void PopconfirmComponentHost::confirm(runtime::ComponentId id) {
    const auto* s = services_->components().state<PopconfirmState>(id);
    if (!s || !s->snapshot.geometry.visible || s->disabled || s->loading.get() ||
        !services_->interactions().component_active(id)) {
        return;
    }
    const auto callback = s->on_confirm;
    if (callback) {
        callback();
    }
    s = services_->components().state<PopconfirmState>(id);
    if (s && services_->components().scope(id).active() && !s->loading.get()) {
        request_open(id, false);
    }
}

void PopconfirmComponentHost::cancel(runtime::ComponentId id) {
    const auto* s = services_->components().state<PopconfirmState>(id);
    if (!s || !s->snapshot.geometry.visible || !s->show_cancel || !services_->interactions().component_active(id)) {
        return;
    }
    const auto callback = s->on_cancel;
    request_open(id, false);
    if (callback && services_->components().contains(id) && services_->components().scope(id).active()) {
        callback();
    }
}

void PopconfirmComponentHost::update_theme(runtime::ComponentId id, bool geometry) {
    auto* s = services_->components().state<PopconfirmState>(id);
    if (!s) {
        return;
    }
    const auto& token = services_->components().theme_scope(id)->snapshot().popconfirm();
    s->text_color.set(foreground(token.text));
    s->title_color.set(foreground(token.title));
    s->warning_color.set(foreground(token.warning));
    s->arrow_color.set(foreground(token.background));
    const bool description = s->rich_description || !s->description.get().empty();
    s->font.set({token.font_family, token.font_weight, false, token.font_size, token.line_height});
    s->title_font.set({token.font_family, description ? token.title_font_weight : token.font_weight, false,
                       token.font_size, token.line_height});
    if (geometry) {
        s->needs_measure = true;
        services_->dirty().invalidate(s->node, runtime::DirtyFlags::Measure);
    }
    if (services_->components().set_window_layer(s->snapshot.popup, token.z_index_popup) |
        services_->components().set_window_layer(s->arrow_component, token.z_index_popup)) {
        services_->mark_scene_structure_dirty();
    }
    services_->dirty().invalidate(s->node, runtime::DirtyFlags::Material);
}

void PopconfirmComponentHost::mount(const PopconfirmProps& props, const PopconfirmTrigger& trigger,
                                    const PopconfirmSlots& slots) {
    if (props.open_ && props.default_open_) {
        throw std::invalid_argument("Popconfirm cannot combine open and defaultOpen");
    }
    if ((slots.title && props.explicit_title_) || (slots.description && props.explicit_description_)) {
        throw std::invalid_argument("Popconfirm text and slot declarations conflict");
    }
    validate_placement(read_prop(props.placement_));
    auto& services = *services_;
    auto& build = runtime::require_component_build_context();
    const auto id = build.mount_component<PopconfirmState>(
        props.open_.has_value(), props.open_ ? read_prop(*props.open_) : props.default_open_.value_or(false));
    auto& s = build.state<PopconfirmState>(id);
    s.node = build.root(id);
    s.rich_title = slots.title.has_value();
    s.rich_description = slots.description.has_value();
    s.disabled = read_prop(props.disabled_);
    s.show_cancel = read_prop(props.show_cancel_);
    s.on_open = props.on_open_;
    s.on_confirm = props.on_confirm_;
    s.on_cancel = props.on_cancel_;
    s.title.set(read_prop(props.title_));
    s.description.set(read_prop(props.description_));
    s.trigger_hit = services.interactions().create({id, s.node, {}, !s.disabled, true, {}, true, true});
    const auto hit_fragment = build.register_scene_fragment(id, runtime::SceneFragmentPlacement::before_children);
    services.scene_composer().set_fragment(hit_fragment, {}, s.trigger_hit);
    build.on_resource_cleanup(id, [&services, node = s.node, hit = s.trigger_hit, hit_fragment] {
        services.pointer().cancel_interaction(hit);
        services.interactions().remove(hit);
        services.scene_composer().remove_fragment(hit_fragment);
        services.layout().remove_layout(node);
    });
    runtime::connect_layout_style(build.scope(id), props.layout_, s.node, services.nodes(), services.dirty());
    build.mount_slot(
        id, Content{[&] {
            auto& children = runtime::require_component_build_context();
            s.snapshot.trigger = group(services, layout::BoxLayout{});
            children.mount_slot(s.snapshot.trigger, trigger);
            const auto trigger_entry = first_focus(services, s.snapshot.trigger);
            services.interactions().set_tab_stop(s.trigger_hit, !trigger_entry);
            s.snapshot.popup = group(services, layout::BoxLayout{});
            const auto popup_node = children.root(s.snapshot.popup);
            services.components().set_window_layer(s.snapshot.popup, 1060);
            services.components().set_branch_active(s.snapshot.popup, false);
            services.nodes().require(popup_node).clip_content = true;
            s.popup_hit =
                services.interactions().create({s.snapshot.popup, popup_node, {}, true, false, {}, false, false});
            const auto surface =
                children.register_scene_fragment(s.snapshot.popup, runtime::SceneFragmentPlacement::before_children);
            graphics::QuadInstance initial;
            initial.opacity = 0;
            component::RetainedSurfaceEffects effects;
            effects.focus_enabled = false;
            s.surface = services.surfaces().create(s.snapshot.popup, popup_node, surface, s.popup_hit,
                                                   std::span{&initial, 1}, effects);
            children.on_resource_cleanup(s.snapshot.popup, [&services, hit = s.popup_hit, surface = s.surface] {
                services.pointer().cancel_interaction(hit);
                services.interactions().remove(hit);
                services.surfaces().destroy(surface);
            });
            children.mount_slot(
                s.snapshot.popup, Content{[&] {
                    auto& popup = runtime::require_component_build_context();
                    s.message = group(services, layout::FlexLayout{});
                    popup.mount_slot(
                        s.message, Content{[&] {
                            auto& message = runtime::require_component_build_context();
                            const auto icon = group(services, layout::BoxLayout{});
                            message.mount_slot_with_semantic_text_style(
                                icon, Content{[] { Icon(IconProps{}.name(IconName::ExclamationCircleFilled)); }},
                                s.warning_color, s.font);
                            s.body = group(services, layout::FlexLayout{});
                            message.mount_slot(
                                s.body, Content{[&] {
                                    auto& body = runtime::require_component_build_context();
                                    s.title_component = group(services, layout::BoxLayout{});
                                    body.mount_slot_with_semantic_text_style(
                                        s.title_component,
                                        slots.title ? Content{SlotContentAccess::function(*slots.title)}
                                                    : Content{[&s] { Text(TextProps{}.content(s.title)); }},
                                        s.title_color, s.title_font);
                                    s.snapshot.description = group(services, layout::BoxLayout{});
                                    body.mount_slot_with_semantic_text_style(
                                        s.snapshot.description,
                                        slots.description ? Content{SlotContentAccess::function(*slots.description)}
                                                          : Content{[&s] { Text(TextProps{}.content(s.description)); }},
                                        s.text_color, s.font);
                                }});
                        }});
                    s.footer = group(services, layout::FlexLayout{});
                    popup.mount_slot(
                        s.footer, Content{[&] {
                            Button(ButtonProps{}.size(ControlSize::Small).disabled(props.disabled_).onClick([this, id] {
                                cancel(id);
                            }),
                                   [&s] { Text(TextProps{}.content(s.cancel_text)); });
                            s.snapshot.cancel = services.components().children(s.footer).back();
                            Button(ButtonProps{}
                                       .type(ButtonType::Primary)
                                       .size(ControlSize::Small)
                                       .disabled(props.disabled_)
                                       .loading(s.loading)
                                       .onClick([this, id] { confirm(id); }),
                                   [&s] { Text(TextProps{}.content(s.ok_text)); });
                            s.snapshot.confirm = services.components().children(s.footer).back();
                        }});
                }});
            services.layout().set_layout(
                popup_node,
                layout::ComponentLayout{
                    [this, id](layout::LayoutEngine& engine, runtime::NodeId, layout::Constraints limits) {
                        auto& state = *services_->components().state<PopconfirmState>(id);
                        const auto& token = services_->components().theme_scope(id)->snapshot().popconfirm();
                        const float padding = std::min(token.padding, limits.max_width / 2);
                        const float width = std::max(0.0F, limits.max_width - padding * 2);
                        const auto message = engine.measure_child(services_->components().root(state.message),
                                                                  {0, width, 0, limits.max_height});
                        const auto footer = engine.measure_child(services_->components().root(state.footer),
                                                                 {0, width, 0, limits.max_height});
                        return limits.constrain({std::max(message.width, footer.width) + padding * 2,
                                                 message.height + footer.height + token.message_gap + padding * 2});
                    },
                    [this, id](layout::LayoutEngine& engine, runtime::NodeId, runtime::Rect rect) {
                        const auto& state = *services_->components().state<PopconfirmState>(id);
                        const auto& token = services_->components().theme_scope(id)->snapshot().popconfirm();
                        const float padding = std::min(token.padding, std::min(rect.width, rect.height) / 2);
                        const float width = std::max(0.0F, rect.width - padding * 2);
                        const auto footer_node = services_->components().root(state.footer);
                        const auto message_node = services_->components().root(state.message);
                        const auto footer = engine.measure_child(footer_node, {0, width, 0, rect.height});
                        const float footer_height = std::min(footer.height, rect.height - padding * 2);
                        const float message_height =
                            std::max(0.0F, rect.height - padding * 2 - footer_height - token.message_gap);
                        static_cast<void>(engine.measure_child(message_node, {width, width, 0, message_height}));
                        engine.place_child(message_node, {rect.x + padding, rect.y + padding, width, message_height});
                        engine.place_child(footer_node, {rect.x + rect.width - padding - footer.width,
                                                         rect.y + rect.height - padding - footer_height, footer.width,
                                                         footer_height});
                    }});
            s.arrow_component = group(services, layout::BoxLayout{});
            services.components().set_window_layer(s.arrow_component, 1060);
            services.components().set_branch_active(s.arrow_component, false);
            children.mount_slot_with_semantic_text_style(
                s.arrow_component, Content{[&s] { mount_text_component(TextProps{}.content(s.arrow_glyph), true); }},
                s.arrow_color, s.arrow_font);
        }});
    services.layout().set_layout(
        s.node, layout::ComponentLayout{
                    [this, id](layout::LayoutEngine& engine, runtime::NodeId, layout::Constraints limits) {
                        const auto* state = services_->components().state<PopconfirmState>(id);
                        return engine.measure_child(services_->components().root(state->snapshot.trigger), limits);
                    },
                    [this, id](layout::LayoutEngine& engine, runtime::NodeId, runtime::Rect rect) {
                        const auto* state = services_->components().state<PopconfirmState>(id);
                        engine.place_child(services_->components().root(state->snapshot.trigger), rect);
                    }});
    mounted_.push_back(id);
    auto& scope = build.scope(id);
    const auto text = [&](const Prop<String>& prop, Signal<String> PopconfirmState::* field) {
        connect_prop(scope, prop, [this, id, field](String value) {
            if (auto* state = services_->components().state<PopconfirmState>(id)) {
                (state->*field).set(std::move(value));
                state->needs_measure = true;
                update_theme(id, true);
                refresh(id);
            }
        });
    };
    text(props.title_, &PopconfirmState::title);
    text(props.description_, &PopconfirmState::description);
    text(props.ok_text_, &PopconfirmState::ok_text);
    text(props.cancel_text_, &PopconfirmState::cancel_text);
    if (props.open_) {
        connect_prop(scope, *props.open_, [this, id](bool value) {
            if (auto* state = services_->components().state<PopconfirmState>(id)) {
                state->model.reconcile(value);
                state->restore = true;
                refresh(id);
            }
        });
    }
    connect_prop(scope, props.disabled_, [this, id](bool value) {
        if (auto* state = services_->components().state<PopconfirmState>(id)) {
            state->disabled = value;
            if (value) {
                suppress(id);
            } else {
                refresh(id);
            }
        }
    });
    connect_prop(scope, props.loading_, [this, id](bool value) {
        if (auto* state = services_->components().state<PopconfirmState>(id)) {
            state->loading.set(value);
        }
    });
    const auto flag = [&](const Prop<bool>& prop, bool PopconfirmState::* field) {
        connect_prop(scope, prop, [this, id, field](bool value) {
            if (auto* state = services_->components().state<PopconfirmState>(id)) {
                state->*field = value;
                state->needs_measure = true;
                services_->dirty().invalidate(state->node, runtime::DirtyFlags::Geometry);
                refresh(id);
            }
        });
    };
    flag(props.show_cancel_, &PopconfirmState::show_cancel);
    flag(props.arrow_, &PopconfirmState::arrow);
    flag(props.point_at_center_, &PopconfirmState::point_at_center);
    flag(props.adjust_, &PopconfirmState::adjust);
    connect_prop(scope, props.placement_, [this, id](PopconfirmPlacement value) {
        validate_placement(value);
        if (auto* state = services_->components().state<PopconfirmState>(id)) {
            state->placement = value;
            services_->dirty().invalidate(state->node, runtime::DirtyFlags::Geometry);
        }
    });
    const auto theme = build.theme_scope();
    const std::array accessors{
        &theme_runtime::ThemeScope::popconfirm_colors, &theme_runtime::ThemeScope::popconfirm_metrics,
        &theme_runtime::ThemeScope::popconfirm_typography, &theme_runtime::ThemeScope::popconfirm_effects,
        &theme_runtime::ThemeScope::popconfirm_order};
    for (std::size_t i = 0; i < accessors.size(); ++i) {
        s.subscriptions[i] = theme->capture(
            [this, id, geometry = i == 1 || i == 2](theme_runtime::DirtyPhase) { update_theme(id, geometry); },
            [theme, accessor = accessors[i]] { static_cast<void>((theme.get()->*accessor)()); });
    }
    update_theme(id, true);
    refresh(id);
}

void PopconfirmComponentHost::on_window_active(bool active) {
    window_active_ = active;
    const auto ids = mounted_;
    for (auto id : ids) {
        if (!active) {
            suppress(id);
        }
    }
}

void PopconfirmComponentHost::on_input_boundary_changed() {
    const auto ids = mounted_;
    for (auto id : ids) {
        if (!services_->interactions().component_active(id)) {
            suppress(id);
        } else {
            refresh(id);
        }
    }
}

bool PopconfirmComponentHost::on_keyboard_input(const input::KeyboardInputEvent& event) {
    if (!window_active_) {
        return false;
    }
    const auto focus_state = services_->focus().state();
    const auto focus = focus_state.focused;
    const auto* record = focus ? services_->interactions().find(*focus) : nullptr;
    const auto focused_component = record ? std::optional{record->component} : std::optional<runtime::ComponentId>{};
    const auto ids = mounted_;
    std::optional<runtime::ComponentId> escape_target;
    for (auto id : ids) {
        const auto* s = services_->components().state<PopconfirmState>(id);
        if (!s || s->disabled || !services_->interactions().component_active(id)) {
            continue;
        }
        if ((focus == s->trigger_hit ||
             (focused_component && descendant(*services_, *focused_component, s->snapshot.trigger))) &&
            (event.key == input::Key::enter || event.key == input::Key::space) &&
            event.modifiers == input::KeyModifier::none) {
            const bool wrapper = focus == s->trigger_hit;
            const auto handlers = record ? record->focus_handlers : nullptr;
            const bool native_activation = handlers && handlers->activate && !handlers->text_edit;
            const bool activate = wrapper ? event.action == input::KeyAction::down
                                  : event.key == input::Key::enter
                                      ? event.action == input::KeyAction::down
                                      : event.action == input::KeyAction::up && focus_state.keyboard_pressed;
            if ((wrapper || native_activation) && activate && !event.repeat &&
                (wrapper || !handlers->activation_allowed || handlers->activation_allowed())) {
                request_open(id, !s->snapshot.geometry.visible);
            }
            // Focus transfers during layer positioning, after the native trigger
            // has completed Enter or Space activation through FocusManager.
            return wrapper;
        }
        if (event.key == input::Key::escape && event.action == input::KeyAction::down && s->snapshot.geometry.visible) {
            if (!escape_target || !descendant(*services_, *escape_target, id)) {
                escape_target = id;
            }
        }
    }
    if (escape_target) {
        request_open(*escape_target, false);
        return true;
    }
    return false;
}

void PopconfirmComponentHost::on_pointer_input(const input::PointerInputEvent& event,
                                               std::optional<input::InteractionId> hit,
                                               std::optional<input::InteractionId> origin) {
    if (!window_active_ || (event.action != input::PointerAction::down && event.action != input::PointerAction::up)) {
        return;
    }
    const auto ids = mounted_;
    for (auto id : ids) {
        const auto* s = services_->components().state<PopconfirmState>(id);
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
            request_open(id, false, false);
        } else if (event.action == input::PointerAction::up && event.button == input::PointerButton::primary && hit &&
                   origin == hit && in_trigger) {
            request_open(id, !s->snapshot.geometry.visible);
        }
    }
}

void PopconfirmComponentHost::position_window_layers(runtime::Size viewport, runtime::Rect clip) {
    const auto ids = mounted_;
    for (auto id : ids) {
        if (!services_->components().contains(id)) {
            continue;
        }
        if (!services_->interactions().component_active(id)) {
            suppress(id);
        }
        auto* s = services_->components().state<PopconfirmState>(id);
        if (!s || !s->snapshot.geometry.visible) {
            continue;
        }
        const auto& token = services_->components().theme_scope(id)->snapshot().popconfirm();
        auto& engine = services_->layout();
        const auto popup = services_->components().root(s->snapshot.popup);
        const bool measure = s->needs_measure || s->viewport != viewport ||
                             services_->nodes().require(popup).measure_generation != engine.generation();
        if (measure) {
            layout::FlexLayout message;
            message.main_gap = token.message_gap;
            message.item_policy = layout::FlexItemPolicy::sequential;
            engine.set_layout(services_->components().root(s->message), message);
            layout::FlexLayout body;
            body.direction = layout::FlexDirection::vertical;
            body.main_gap = token.description_gap;
            body.item_policy = layout::FlexItemPolicy::sequential;
            engine.set_layout(services_->components().root(s->body), body);
            layout::FlexLayout footer;
            footer.main_gap = token.button_gap;
            footer.item_policy = layout::FlexItemPolicy::sequential;
            engine.set_layout(services_->components().root(s->footer), footer);
            s->popup_size =
                engine.measure_child(popup, {0, std::min(token.max_width, viewport.width), 0, viewport.height});
            s->needs_measure = false;
            s->viewport = viewport;
        }
        const auto& node = services_->nodes().require(s->node);
        const runtime::Rect anchor{node.bounds.x + node.translation.x, node.bounds.y + node.translation.y,
                                   node.bounds.width, node.bounds.height};
        s->snapshot.geometry =
            position_tooltip(anchor, s->popup_size, clip, s->placement, token.gap + (s->arrow ? token.arrow_size : 0),
                             s->adjust, s->arrow && s->point_at_center, std::max(12.0F, token.border_radius + 2));
        if (measure || services_->nodes().require(popup).bounds != s->snapshot.geometry.bounds) {
            engine.place_child(popup, s->snapshot.geometry.bounds);
        }
        const auto bounds = s->snapshot.geometry.bounds;
        const int side = static_cast<int>(s->snapshot.geometry.placement) / 3;
        const float size = std::min(token.arrow_size, (side < 2 ? bounds.width : bounds.height) / 2);
        const bool arrow = s->arrow && size > 0;
        if (services_->components().set_branch_active(s->arrow_component, arrow)) {
            services_->mark_scene_structure_dirty();
        }
        if (arrow) {
            static const String glyphs[]{String{u8"\uF000"}, String{u8"\uF001"}, String{u8"\uF002"},
                                         String{u8"\uF003"}};
            s->arrow_glyph.set(glyphs[side]);
            const float font_size = std::max(1.0F, std::round(size * 2));
            const float half = font_size / 2;
            s->arrow_font.set({SystemFontFamily::ui_sans, 400, false, font_size, font_size});
            const auto arrow_node = services_->components().root(s->arrow_component);
            static_cast<void>(engine.measure_child(arrow_node, {0, font_size, 0, font_size}));
            const float center = s->snapshot.geometry.arrow_center;
            runtime::Rect rect{bounds.x + center - half, bounds.y + bounds.height, font_size, font_size};
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
            auto target = first_focus(*services_, s->snapshot.confirm);
            if (!target) {
                target = first_focus(*services_, s->snapshot.popup);
            }
            if (target) {
                services_->focus().defer_focus(target, input::FocusModality::keyboard);
            }
        }
    }
}

void PopconfirmComponentHost::synchronize_auxiliary_geometry(runtime::Size viewport, runtime::Rect) {
    const runtime::Rect clip{0, 0, viewport.width, viewport.height};
    for (auto id : mounted_) {
        const auto* s = services_->components().state<PopconfirmState>(id);
        if (!s || !s->snapshot.geometry.visible) {
            continue;
        }
        const auto& token = services_->components().theme_scope(id)->snapshot().popconfirm();
        const auto bounds = s->snapshot.geometry.bounds;
        graphics::QuadInstance body;
        body.bounds = {bounds.x, bounds.y, bounds.width, bounds.height};
        body.color = foreground(token.background);
        body.corner_radius = std::min(token.border_radius, std::min(bounds.width, bounds.height) / 2);
        static_cast<void>(services_->surfaces().update_surface(s->surface, std::span{&body, 1}));
        component::RetainedSurfaceEffects effects;
        effects.focus_enabled = false;
        effects.shape = {{bounds.x, bounds.y, bounds.width, bounds.height}, body.corner_radius};
        effects.shadows = token.shadow;
        effects.ancestor_clip = graphics::EffectClip{1, clip};
        static_cast<void>(services_->surfaces().update_effects(s->surface, effects));
    }
}
} // namespace ryn::detail

namespace ryn {
void Popconfirm(PopconfirmProps props, PopconfirmTrigger trigger, PopconfirmSlots slots) {
    if (!detail::active_popconfirms) {
        throw std::logic_error("Popconfirm requires window component services");
    }
    detail::active_popconfirms->mount(props, trigger, slots);
}
} // namespace ryn
