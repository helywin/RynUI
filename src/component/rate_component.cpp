#include "component/rate_component.hpp"
#include "component/rate_model.hpp"
#include "runtime/layout_style_adapter.hpp"
#include "runtime/prop_connection.hpp"
#include <algorithm>
#include <cmath>
#include <stdexcept>

namespace ryn::detail {
namespace {
thread_local RateComponentHost* active_rate{};

struct RateItemState final {
    RateItemSnapshot view;
    runtime::NodeId paint;
    Signal<String> text{String{}};
    Signal<IconSource> icon{IconSource{IconName::StarFilled}};
    Signal<bool> is_icon{true};
    Signal<bool> tip_open{false};
    Signal<runtime::SemanticForeground> background{{0, 0, 0, .06F}};
    Signal<runtime::SemanticForeground> foreground{{1, 1, 0, 1}};
    Signal<runtime::SemanticTypography> font{runtime::SemanticTypography{}};
    float paint_side{20};
};

struct RateState final {
    runtime::NodeId node;
    input::InteractionId focus;
    component::RetainedSurfaceId decoration;
    int count{5};
    float authority{};
    float value{};
    bool controlled{};
    bool half{};
    bool clear{true};
    bool disabled{};
    bool read_only{};
    bool keyboard{true};
    bool visible{true};
    bool geometry_active{true};
    bool empty_reconciled{};
    ControlSize size{ControlSize::Middle};
    std::optional<float> hover;
    std::optional<std::size_t> pressed;
    std::optional<std::size_t> tooltip_hover;
    std::optional<animation::AnimationTime> tooltip_leave_deadline;
    std::optional<input::InteractionId> observed_focus;
    std::optional<input::Key> pending_key;
    RateCharacter character;
    std::function<RateCharacter(std::size_t)> character_for;
    std::vector<RateTooltip> tooltips;
    std::vector<runtime::ComponentId> items;
    std::array<theme_runtime::Subscription, 2> subscriptions;
    std::function<void(float)> on_change;
    std::function<void(std::optional<float>)> on_hover;
    runtime::Rect clip;
};

float star_size(const RateState& s, const RateMetricToken& m) {
    return s.size == ControlSize::Small   ? m.star_size_small
           : s.size == ControlSize::Large ? m.star_size_large
                                          : m.star_size;
}

bool editable(const RateState& s) {
    return s.visible && s.count > 0 && !s.disabled && !s.read_only && s.geometry_active;
}

bool input_container_active(const RateState& s) {
    return s.visible && s.count > 0 && !s.disabled && s.geometry_active;
}

runtime::SemanticForeground foreground(Color c) {
    return {c.red(), c.green(), c.blue(), c.alpha()};
}

runtime::Rect translated(const runtime::Node& node) {
    return {node.bounds.x + node.translation.x, node.bounds.y + node.translation.y, node.bounds.width,
            node.bounds.height};
}

bool inside(runtime::Rect r, float x, float y) {
    return r.width > 0 && r.height > 0 && x >= r.x && y >= r.y && x < r.x + r.width && y < r.y + r.height;
}

void assign_character(RateItemState& row, const RateCharacter& character) {
    const auto* text = std::get_if<String>(&character.value());
    row.text.set(text ? *text : String{});
    row.is_icon.set(!text);
    if (!text) {
        row.icon.set(std::get<IconSource>(character.value()));
    }
}
} // namespace

RateComponentHost::RateComponentHost(WindowComponentServices& services) : services_(&services) {
    services.attach(*this);
}

RateComponentHost::~RateComponentHost() {
    services_->detach(*this);
}

void* RateComponentHost::begin_mount() noexcept {
    auto* previous = active_rate;
    active_rate = this;
    return previous;
}

void RateComponentHost::end_mount(void* previous) noexcept {
    active_rate = static_cast<RateComponentHost*>(previous);
}

void RateComponentHost::on_destroy() noexcept {
    std::erase_if(mounted_, [this](auto id) { return !services_->components().contains(id); });
}

RateSnapshot RateComponentHost::snapshot(runtime::ComponentId id) const {
    const auto* s = services_->components().state<RateState>(id);
    if (!s) {
        throw std::out_of_range("Rate component is stale");
    }
    RateSnapshot result{s->value, s->hover, s->count, s->focus, {}};
    for (auto item : s->items) {
        result.items.push_back(services_->components().state<RateItemState>(item)->view);
    }
    return result;
}

void RateComponentHost::set_hover(runtime::ComponentId id, std::optional<float> value) {
    auto* s = services_->components().state<RateState>(id);
    if (!s || !services_->components().scope(id).active() || s->hover == value) {
        return;
    }
    s->hover = value;
    const auto callback = s->on_hover;
    refresh(id, true);
    if (services_->components().contains(id) && callback) {
        callback(value);
    }
}

void RateComponentHost::commit(runtime::ComponentId id, float candidate, bool clear) {
    auto* s = services_->components().state<RateState>(id);
    if (!s || !window_active_ || !editable(*s) || !services_->interactions().branch_active(s->focus)) {
        return;
    }
    candidate = normalize_rate_value(candidate, s->count, s->half);
    if (clear && s->clear && candidate == s->value) {
        candidate = 0;
    }
    const auto change = s->on_change;
    const auto hover = s->hover ? s->on_hover : std::function<void(std::optional<float>)>{};
    const bool changed = candidate != s->value;
    s->pressed.reset();
    s->hover.reset();
    if (!s->controlled) {
        s->authority = candidate;
        s->value = candidate;
    }
    refresh(id, true);
    if (!services_->components().contains(id)) {
        return;
    }
    if (hover) {
        hover(std::nullopt);
    }
    // Hover callbacks can synchronously unmount or change authority/gates.
    s = services_->components().state<RateState>(id);
    if (s && editable(*s) && services_->interactions().branch_active(s->focus) && changed && change) {
        change(candidate);
    }
}

void RateComponentHost::refresh(runtime::ComponentId id, bool geometry) {
    auto* s = services_->components().state<RateState>(id);
    if (!s || !services_->components().scope(id).active()) {
        return;
    }
    const bool visible = s->visible && s->count > 0;
    geometry |= services_->components().set_branch_active(id, visible);
    services_->nodes().require(s->node).out_of_flow = !visible;
    // The root is also the native interaction parent of Tooltip descendants.
    services_->interactions().set_eligible(s->focus, input_container_active(*s) && window_active_);
    services_->interactions().set_focusable(s->focus, s->keyboard && !s->read_only);
    services_->interactions().set_tab_stop(s->focus, s->keyboard && !s->read_only);
    s->observed_focus = services_->focus().state().focused;
    const auto items = s->items;
    for (std::size_t i = 0; i < items.size(); ++i) {
        s = services_->components().state<RateState>(id);
        if (!s) {
            return;
        }
        const auto t = services_->components().theme_scope(id)->snapshot().rate();
        const auto focus = services_->focus().state();
        const auto focus_index = static_cast<std::size_t>(std::max(0.0F, std::ceil(s->value) - 1));
        auto& row = *services_->components().state<RateItemState>(items[i]);
        const bool active = i < static_cast<std::size_t>(s->count);
        geometry |= services_->components().set_branch_active(row.view.component, active);
        services_->nodes().require(row.view.node).out_of_flow = !active;
        services_->interactions().set_eligible(row.view.interaction, active && editable(*s) && window_active_);
        row.background.set(foreground(t.colors.background));
        row.foreground.set(foreground(t.colors.star));
        const bool emphasized =
            editable(*s) && ((s->hover && static_cast<std::size_t>(std::ceil(*s->hover) - 1) == i) ||
                             (focus.focused == s->focus && focus.focus_visible && focus_index == i));
        row.paint_side = star_size(*s, t.metrics) * (emphasized ? t.metrics.hover_scale : 1);
        row.font.set({t.metrics.font_family, 400, false, row.paint_side, row.paint_side});
        bool focused_content = false;
        const auto* record = focus.focused ? services_->interactions().find(*focus.focused) : nullptr;
        for (auto component = record ? std::optional{record->component} : std::nullopt; component && *component != id;
             component = services_->components().parent(*component)) {
            if (*component == row.view.component) {
                focused_content = true;
                break;
            }
        }
        // Closing a rich Tooltip synchronizes native focus and may unmount/rebind this owner.
        const auto tip_open = row.tip_open;
        tip_open.set(s->visible && s->count > 0 && s->geometry_active && active && !s->disabled && window_active_ &&
                     services_->interactions().component_active(id) &&
                     (s->tooltip_hover == i || focused_content ||
                      (focus.focused == s->focus && focus.focus_visible && focus_index == i)));
    }
    s = services_->components().state<RateState>(id);
    if (!s) {
        return;
    }
    services_->dirty().invalidate(s->node, geometry ? runtime::DirtyFlags::Measure | runtime::DirtyFlags::Layout |
                                                          runtime::DirtyFlags::Geometry | runtime::DirtyFlags::HitTest
                                                    : runtime::DirtyFlags::Material);
}

void RateComponentHost::reconcile_input(runtime::ComponentId id) {
    auto* s = services_->components().state<RateState>(id);
    if (!s) {
        return;
    }
    std::vector<input::InteractionId> cancel;
    const bool container_active =
        input_container_active(*s) && window_active_ && services_->interactions().component_active(id);
    const bool active = container_active && !s->read_only;
    for (std::size_t i = 0; i < s->items.size(); ++i) {
        if (!active || i >= static_cast<std::size_t>(s->count)) {
            cancel.push_back(services_->components().state<RateItemState>(s->items[i])->view.interaction);
        }
    }
    const auto focus = s->focus;
    const bool cancel_focus = !active || !s->keyboard;
    const bool cancel_hover = !active || (s->hover && *s->hover > s->count);
    if (!container_active || (s->tooltip_hover && *s->tooltip_hover >= static_cast<std::size_t>(s->count))) {
        s->tooltip_hover.reset();
        s->tooltip_leave_deadline.reset();
    }
    if (cancel_focus) {
        s->pending_key.reset();
    }
    for (std::size_t i = 0; i < s->items.size(); ++i) {
        services_->interactions().set_input_blocked(s->items[i], i >= static_cast<std::size_t>(s->count));
    }
    if (!active || (s->pressed && *s->pressed >= static_cast<std::size_t>(s->count))) {
        s->pressed.reset();
    }
    refresh(id, true);
    for (auto interaction : cancel) {
        services_->pointer().cancel_interaction(interaction);
    }
    if (cancel_focus) {
        services_->focus().cancel_interaction(focus);
    }
    if (cancel_hover) {
        set_hover(id, std::nullopt);
    }
    s = services_->components().state<RateState>(id);
    if (s) {
        services_->set_input_blocked(id, !s->visible || s->count == 0 || s->disabled || !s->geometry_active);
    }
}

void RateComponentHost::mount_item(runtime::ComponentId id, std::size_t index) {
    auto& build = runtime::require_component_build_context();
    auto& services = *services_;
    auto* s = services.components().state<RateState>(id);
    if (!s) {
        return;
    }
    const auto character = s->character_for ? s->character_for(index) : s->character;
    s = services.components().state<RateState>(id);
    if (!s) {
        return;
    }
    const auto item = build.mount_component<RateItemState>();
    auto& row = build.state<RateItemState>(item);
    row.view.component = item;
    row.view.node = build.root(item);
    s->items.push_back(item);
    assign_character(row, character);
    services.layout().set_layout(row.view.node, layout::BoxLayout{});
    build.on_resource_cleanup(item, [&services, item, node = row.view.node] {
        services.interactions().set_input_blocked(item, false);
        services.layout().remove_layout(node);
    });
    build.mount_slot(
        item, Content{[&, id, item, index] {
            const auto paint = [&, id, item, index] {
                auto& children = runtime::require_component_build_context();
                auto& current = *services.components().state<RateItemState>(item);
                const auto group = children.mount_component<int>(0);
                current.paint = children.root(group);
                const auto focus = services.components().state<RateState>(id)->focus;
                current.view.interaction =
                    services.interactions().create({group, current.paint, focus, true, false, {}, false, false});
                const auto interaction = current.view.interaction;
                const auto fragment =
                    children.register_scene_fragment(group, runtime::SceneFragmentPlacement::before_children);
                services.scene_composer().set_fragment(fragment, {}, interaction);
                children.on_resource_cleanup(group, [&services, interaction, fragment, node = current.paint] {
                    services.pointer().cancel_interaction(interaction);
                    services.focus().cancel_interaction(interaction);
                    services.interactions().remove(interaction);
                    services.scene_composer().remove_fragment(fragment);
                    services.layout().remove_layout(node);
                });
                input::InteractionHandlers handler;
                handler.target = [this, id, index, interaction](input::PointerDispatchContext& event) {
                    auto* state = services_->components().state<RateState>(id);
                    if (!state) {
                        return;
                    }
                    if (event.kind() == input::PointerEventKind::cancel) {
                        state->pressed.reset();
                        set_hover(id, std::nullopt);
                        return;
                    }
                    if (!editable(*state) || !window_active_ || !services_->interactions().branch_active(interaction) ||
                        event.event().button != input::PointerButton::primary) {
                        return;
                    }
                    if (event.kind() == input::PointerEventKind::down) {
                        state->pressed = index;
                        const auto focus_target = state->focus;
                        const bool focusable = state->keyboard;
                        static_cast<void>(event.capture_pointer());
                        event.stop_propagation();
                        if (focusable) {
                            services_->focus().defer_focus(focus_target, input::FocusModality::pointer);
                        }
                    } else if (event.kind() == input::PointerEventKind::up) {
                        const auto& row = *services_->components().state<RateItemState>(state->items[index]);
                        const auto& node = services_->nodes().require(row.paint);
                        const auto r = translated(node);
                        const auto clip = services_->nodes().content_clip(row.paint, state->clip);
                        const bool hit =
                            state->pressed == index && event.press_origin() == interaction &&
                            inside(graphics::intersect_effect_bounds(r, clip), event.event().x, event.event().y);
                        const float candidate =
                            rate_pointer_value(index, r.width > 0 ? (event.event().x - r.x) / r.width : 1, state->half);
                        state->pressed.reset();
                        static_cast<void>(event.release_pointer_capture());
                        event.stop_propagation();
                        if (hit) {
                            commit(id, candidate, true);
                        }
                    }
                };
                services.interactions().set_handlers(interaction, std::move(handler));
                services.layout().set_layout(
                    current.paint,
                    layout::ComponentLayout{
                        [this, id](layout::LayoutEngine&, runtime::NodeId, layout::Constraints limits) {
                            const auto* state = services_->components().state<RateState>(id);
                            const float side =
                                star_size(*state, services_->components().theme_scope(id)->snapshot().rate().metrics);
                            return limits.constrain({side, side});
                        },
                        [this, id, item, index](layout::LayoutEngine& engine, runtime::NodeId, runtime::Rect bounds) {
                            const auto& state = *services_->components().state<RateState>(id);
                            const auto& row = *services_->components().state<RateItemState>(item);
                            const float base =
                                star_size(state, services_->components().theme_scope(id)->snapshot().rate().metrics);
                            const float side = row.paint_side;
                            const float fill =
                                std::clamp(state.hover.value_or(state.value) - static_cast<float>(index), 0.0F, 1.0F);
                            const float x = bounds.x + (base - side) / 2;
                            const float y = bounds.y + (base - side) / 2;
                            for (bool front : {false, true}) {
                                const auto node = front ? row.view.foreground : row.view.background;
                                const float width = front ? side * fill : side;
                                static_cast<void>(engine.measure_child(node, layout::Constraints::fixed(width, side)));
                                engine.place_child(node, {x, y, width, side}, true, true);
                            }
                        }});
                children.mount_slot(
                    group, Content{[&, item] {
                        auto& layer_build = runtime::require_component_build_context();
                        for (bool front : {false, true}) {
                            auto& entry = *services.components().state<RateItemState>(item);
                            const auto layer = layer_build.mount_component<int>(0);
                            const auto node = layer_build.root(layer);
                            (front ? entry.view.foreground : entry.view.background) = node;
                            services.nodes().require(node).clip_content = true;
                            services.layout().set_layout(
                                node,
                                layout::ComponentLayout{
                                    [this, item](layout::LayoutEngine& engine, runtime::NodeId node,
                                                 layout::Constraints limits) {
                                        const float side =
                                            services_->components().state<RateItemState>(item)->paint_side;
                                        for (auto child : services_->nodes().require(node).children) {
                                            static_cast<void>(
                                                engine.measure_child(child, layout::Constraints::fixed(side, side)));
                                        }
                                        return limits.constrain({side, side});
                                    },
                                    [this, item](layout::LayoutEngine& engine, runtime::NodeId node,
                                                 runtime::Rect bounds) {
                                        const float side =
                                            services_->components().state<RateItemState>(item)->paint_side;
                                        for (auto child : services_->nodes().require(node).children) {
                                            engine.place_child(child, {bounds.x, bounds.y, side, side}, true, true);
                                        }
                                    }});
                            layer_build.on_resource_cleanup(
                                layer, [&services, node] { services.layout().remove_layout(node); });
                            layer_build.mount_slot_with_semantic_text_style(
                                layer, Content{[&entry] {
                                    Text(TextProps{}.content(entry.text));
                                    Icon(IconProps{}.source(entry.icon).visible(entry.is_icon));
                                }},
                                Prop<runtime::SemanticForeground>{front ? entry.foreground : entry.background},
                                Prop<runtime::SemanticTypography>{entry.font});
                        }
                    }});
            };
            auto* state = services.components().state<RateState>(id);
            if (index < state->tooltips.size()) {
                auto props = TooltipProps{}
                                 .open(services.components().state<RateItemState>(item)->tip_open)
                                 .trigger(TooltipTriggerMode::Manual)
                                 .mouseEnterDelay(Duration::milliseconds(0))
                                 .mouseLeaveDelay(Duration::milliseconds(0));
                if (state->tooltips[index].content_) {
                    Tooltip(std::move(props), TooltipTrigger{paint}, *state->tooltips[index].content_);
                } else {
                    Tooltip(props.title(state->tooltips[index].title_), TooltipTrigger{paint});
                }
            } else {
                paint();
            }
        }});
}

void RateComponentHost::ensure_items(runtime::ComponentId id, int count) {
    const auto* s = services_->components().state<RateState>(id);
    if (!s || s->items.size() >= static_cast<std::size_t>(count)) {
        return;
    }
    const auto first = s->items.size();
    services_->append_slot(id, Content{[this, id, first, count] {
                               for (std::size_t i = first;
                                    i < static_cast<std::size_t>(count) && services_->components().contains(id); ++i) {
                                   mount_item(id, i);
                               }
                           }});
    s = services_->components().state<RateState>(id);
    if (!s) {
        return;
    }
    // Append preserves existing nodes, but registry creation order puts new popup descendants after neighbors.
    // Group every retained logical descendant, including inactive items and Tooltip window layers.
    std::vector<std::vector<input::InteractionId>> groups(s->items.size());
    for (auto hit : services_->interactions().declaration_order()) {
        for (auto owner = std::optional{services_->interactions().require(hit).component}; owner && *owner != id;
             owner = services_->components().parent(*owner)) {
            const auto item = std::ranges::find(s->items, *owner);
            if (item != s->items.end()) {
                groups[static_cast<std::size_t>(item - s->items.begin())].push_back(hit);
                break;
            }
        }
    }
    std::vector<input::InteractionId> order;
    for (const auto& group : groups) {
        order.insert(order.end(), group.begin(), group.end());
    }
    services_->interactions().reorder_after(s->focus, order);
}

void RateComponentHost::mount(const RateProps& props) {
    const int count = read_prop(props.count_);
    const bool half = read_prop(props.allow_half_);
    const float authority = props.value_ ? read_prop(*props.value_) : props.default_value_.value_or(0);
    const float value = normalize_rate_value(authority, count, half);
    validate_rate_size(read_prop(props.size_));
    if (props.value_ && props.default_value_) {
        throw std::invalid_argument("Rate value and defaultValue are mutually exclusive");
    }
    auto& build = runtime::require_component_build_context();
    auto& services = *services_;
    const auto id = build.mount_component<RateState>();
    auto& s = build.state<RateState>(id);
    s.node = build.root(id);
    s.count = count;
    s.half = half;
    s.authority = authority;
    s.value = value;
    s.controlled = props.value_.has_value();
    s.clear = read_prop(props.allow_clear_);
    s.disabled = read_prop(props.disabled_);
    s.read_only = read_prop(props.read_only_);
    s.keyboard = read_prop(props.keyboard_);
    s.visible = read_prop(props.visible_);
    s.size = read_prop(props.size_);
    s.character = read_prop(props.character_);
    s.character_for = props.character_for_;
    s.tooltips = props.tooltips_;
    s.on_change = props.on_change_;
    s.on_hover = props.on_hover_;
    services.nodes().require(s.node).clip_content = true;
    runtime::connect_layout_style(build.scope(id), props.layout_, s.node, services.nodes(), services.dirty());
    s.focus = services.interactions().create({id, s.node, {}, true, true, {}, false, true});
    const auto hit = build.register_scene_fragment(id, runtime::SceneFragmentPlacement::before_children);
    services.scene_composer().set_fragment(hit, {}, s.focus);
    const auto fragment = build.register_scene_fragment(id, runtime::SceneFragmentPlacement::after_children);
    s.decoration = services.surfaces().create_content_range(fragment, {});
    build.on_resource_cleanup(id, [&services, id, node = s.node, focus = s.focus, decoration = s.decoration, hit] {
        services.interactions().set_input_blocked(id, false);
        services.pointer().cancel_interaction(focus);
        services.focus().cancel_interaction(focus);
        services.interactions().remove(focus);
        services.scene_composer().remove_fragment(hit);
        services.surfaces().destroy_content_range(decoration);
        services.layout().remove_layout(node);
    });
    input::FocusHandlers focus;
    focus.state_changed = [this, id](input::FocusPresentation state) {
        if (!state.focused) {
            if (auto* s = services_->components().state<RateState>(id)) {
                s->pending_key.reset();
            }
        }
        refresh(id, true);
        // refresh can close a rich Tooltip and invoke native blur/unmount/ref-rebind callbacks.
        apply_keyboard(id);
    };
    services.interactions().set_focus_handlers(s.focus, std::move(focus));
    services.layout().set_layout(
        s.node, layout::ComponentLayout{[this, id](layout::LayoutEngine& engine, runtime::NodeId,
                                                   layout::Constraints limits) { return measure(id, engine, limits); },
                                        [this, id](layout::LayoutEngine& engine, runtime::NodeId,
                                                   runtime::Rect bounds) { place(id, engine, bounds); }});
    build.mount_slot(id, Content{[this, id, count] {
                         for (int i = 0; i < count; ++i) {
                             mount_item(id, static_cast<std::size_t>(i));
                         }
                     }});
    mounted_.push_back(id);
    auto& scope = build.scope(id);
    if (props.value_) {
        connect_prop(scope, *props.value_, [this, id](float authority) {
            if (auto* s = services_->components().state<RateState>(id)) {
                const float value = normalize_rate_value(authority, s->count, s->half);
                s->authority = authority;
                s->value = value;
                refresh(id, true);
            }
        });
    }
    connect_prop(scope, props.count_, [this, id](int count) {
        validate_rate_count(count);
        ensure_items(id, count);
        if (auto* s = services_->components().state<RateState>(id)) {
            s->count = count;
            s->value = normalize_rate_value(s->authority, count, s->half);
            reconcile_input(id);
        }
    });
    connect_prop(scope, props.allow_half_, [this, id](bool half) {
        if (auto* s = services_->components().state<RateState>(id)) {
            s->half = half;
            s->value = normalize_rate_value(s->authority, s->count, half);
            s->pressed.reset();
            set_hover(id, std::nullopt);
            refresh(id, true);
        }
    });
    const auto status = [this, id](auto member, bool value) {
        if (auto* s = services_->components().state<RateState>(id)) {
            s->*member = value;
            reconcile_input(id);
        }
    };
    connect_prop(scope, props.disabled_, [status](bool value) { status(&RateState::disabled, value); });
    connect_prop(scope, props.visible_, [status](bool value) { status(&RateState::visible, value); });
    connect_prop(scope, props.read_only_, [status](bool value) { status(&RateState::read_only, value); });
    connect_prop(scope, props.keyboard_, [status](bool value) { status(&RateState::keyboard, value); });
    connect_prop(scope, props.allow_clear_, [this, id](bool value) {
        if (auto* s = services_->components().state<RateState>(id)) {
            s->clear = value;
        }
    });
    connect_prop(scope, props.size_, [this, id](ControlSize value) {
        validate_rate_size(value);
        if (auto* s = services_->components().state<RateState>(id)) {
            s->size = value;
            refresh(id, true);
        }
    });
    connect_prop(scope, props.character_, [this, id](RateCharacter value) {
        if (auto* s = services_->components().state<RateState>(id)) {
            s->character = value;
            if (!s->character_for) {
                for (auto item : s->items) {
                    assign_character(*services_->components().state<RateItemState>(item), value);
                }
            }
            refresh(id, true);
        }
    });
    const auto theme = build.theme_scope();
    const std::array accessors{&theme_runtime::ThemeScope::rate_colors, &theme_runtime::ThemeScope::rate_metrics};
    for (std::size_t i = 0; i < accessors.size(); ++i) {
        s.subscriptions[i] =
            theme->capture([this, id, geometry = i == 1](theme_runtime::DirtyPhase) { refresh(id, geometry); },
                           [theme, accessor = accessors[i]] { static_cast<void>((theme.get()->*accessor)()); });
    }
    refresh(id, true);
    services.set_input_blocked(id, !s.visible || s.count == 0 || s.disabled || !s.geometry_active);
}

runtime::Size RateComponentHost::measure(runtime::ComponentId id, layout::LayoutEngine& engine,
                                         layout::Constraints limits) {
    const auto& s = *services_->components().state<RateState>(id);
    const auto& m = services_->components().theme_scope(id)->snapshot().rate().metrics;
    const float side = star_size(s, m);
    for (int i = 0; i < s.count; ++i) {
        static_cast<void>(engine.measure_child(services_->components().state<RateItemState>(s.items[i])->view.node,
                                               layout::Constraints::fixed(side, side)));
    }
    const float width = s.count > 0 ? side * s.count + m.gap * (s.count - 1) : 0;
    return limits.constrain({s.visible ? width : 0, s.visible && s.count > 0 ? side : 0});
}

void RateComponentHost::place(runtime::ComponentId id, layout::LayoutEngine& engine, runtime::Rect bounds) {
    auto& s = *services_->components().state<RateState>(id);
    const auto& m = services_->components().theme_scope(id)->snapshot().rate().metrics;
    const float side = star_size(s, m);
    s.geometry_active = bounds.width > 0 && bounds.height > 0;
    for (int i = 0; i < s.count; ++i) {
        const auto node = services_->components().state<RateItemState>(s.items[i])->view.node;
        static_cast<void>(engine.measure_child(node, layout::Constraints::fixed(side, side)));
        engine.place_child(node, {bounds.x + i * (side + m.gap), bounds.y, side, side}, true, true);
    }
}

void RateComponentHost::apply_keyboard(runtime::ComponentId id) {
    auto* s = services_->components().state<RateState>(id);
    const auto focus = services_->focus().state();
    if (!s || !s->pending_key || !s->keyboard || !editable(*s) || !window_active_ || focus.focused != s->focus ||
        !focus.focus_visible || !services_->interactions().branch_active(s->focus)) {
        return;
    }
    const auto key = *s->pending_key;
    s->pending_key.reset();
    const bool activation = key == input::Key::enter || key == input::Key::space;
    const float step = s->half ? .5F : 1;
    const float next = key == input::Key::left    ? s->value - step
                       : key == input::Key::right ? s->value + step
                       : key == input::Key::home  ? 0
                       : key == input::Key::end   ? static_cast<float>(s->count)
                                                  : s->value;
    commit(id, next, activation);
}

bool RateComponentHost::on_keyboard_input(const input::KeyboardInputEvent& event) {
    const auto ids = mounted_;
    for (auto id : ids) {
        auto* s = services_->components().state<RateState>(id);
        if (!s || !s->keyboard || !editable(*s) || !window_active_ || services_->focus().state().focused != s->focus ||
            !services_->interactions().branch_active(s->focus)) {
            continue;
        }
        const bool activation = event.key == input::Key::enter || event.key == input::Key::space;
        const bool navigation = event.key == input::Key::left || event.key == input::Key::right ||
                                event.key == input::Key::home || event.key == input::Key::end;
        if (!activation && !navigation) {
            return false;
        }
        if (event.action == input::KeyAction::down && !(activation && event.repeat)) {
            s->pending_key = event.key;
            const auto target = s->focus;
            services_->focus().defer_focus(target, input::FocusModality::keyboard);
            // defer_focus may immediately run callbacks, or flush after the enclosing keyboard dispatch.
            // Always reacquire this full ComponentId; the focus callback also drains a queued command.
            apply_keyboard(id);
        }
        return true;
    }
    return false;
}

void RateComponentHost::on_pointer_input(const input::PointerInputEvent& event, std::optional<input::InteractionId> hit,
                                         std::optional<input::InteractionId>) {
    const auto ids = mounted_;
    for (auto id : ids) {
        auto* s = services_->components().state<RateState>(id);
        if (!s) {
            continue;
        }
        std::optional<std::size_t> tooltip_hover;
        if (hit && event.action != input::PointerAction::cancel && s->visible && s->count > 0 && !s->disabled &&
            s->geometry_active && window_active_ && services_->interactions().component_active(id)) {
            const auto* record = services_->interactions().find(*hit);
            for (auto component = record ? std::optional{record->component} : std::nullopt;
                 component && *component != id; component = services_->components().parent(*component)) {
                for (int i = 0; i < s->count; ++i) {
                    if (*component == s->items[i] && static_cast<std::size_t>(i) < s->tooltips.size()) {
                        tooltip_hover = static_cast<std::size_t>(i);
                        break;
                    }
                }
                if (tooltip_hover) {
                    break;
                }
            }
        }
        bool tooltip_changed = false;
        if (tooltip_hover) {
            s->tooltip_leave_deadline.reset();
            tooltip_changed = s->tooltip_hover != tooltip_hover;
            s->tooltip_hover = tooltip_hover;
        } else if (event.action == input::PointerAction::cancel || !window_active_ || !input_container_active(*s) ||
                   !services_->interactions().component_active(id)) {
            tooltip_changed = s->tooltip_hover.has_value();
            s->tooltip_hover.reset();
            s->tooltip_leave_deadline.reset();
        } else if (s->tooltip_hover && !s->tooltip_leave_deadline) {
            s->tooltip_leave_deadline =
                services_->animation_time() + animation::AnimationDuration::microseconds(100000);
            services_->request_frame();
        }
        if (tooltip_changed) {
            refresh(id, true);
            s = services_->components().state<RateState>(id);
            if (!s) {
                continue;
            }
        }
        std::optional<float> hover;
        if (event.action != input::PointerAction::cancel && event.action != input::PointerAction::up &&
            window_active_ && editable(*s) && services_->interactions().component_active(id)) {
            for (int i = 0; i < s->count; ++i) {
                const auto& row = *services_->components().state<RateItemState>(s->items[i]);
                if (hit == row.view.interaction) {
                    const auto r = translated(services_->nodes().require(row.paint));
                    hover = rate_pointer_value(static_cast<std::size_t>(i), r.width > 0 ? (event.x - r.x) / r.width : 1,
                                               s->half);
                    break;
                }
            }
        }
        set_hover(id, hover);
    }
}

void RateComponentHost::on_input_boundary_changed() {
    const auto ids = mounted_;
    for (auto id : ids) {
        reconcile_input(id);
    }
}

void RateComponentHost::on_window_active(bool active) {
    window_active_ = active;
    on_input_boundary_changed();
}

void RateComponentHost::prepare_auxiliary_layout() {
    const auto ids = mounted_;
    const auto focus = services_->focus().state().focused;
    for (auto id : ids) {
        const auto* s = services_->components().state<RateState>(id);
        if (s && s->observed_focus != focus) {
            refresh(id, true);
        }
    }
}

std::size_t RateComponentHost::tick_auxiliary(animation::AnimationTime time) {
    const auto ids = mounted_;
    std::size_t changed = 0;
    for (auto id : ids) {
        auto* s = services_->components().state<RateState>(id);
        if (s && s->tooltip_leave_deadline && time >= *s->tooltip_leave_deadline) {
            s->tooltip_leave_deadline.reset();
            s->tooltip_hover.reset();
            refresh(id, true);
            ++changed;
        }
    }
    return changed;
}

std::optional<animation::AnimationTime> RateComponentHost::next_auxiliary_deadline() const {
    std::optional<animation::AnimationTime> result;
    for (auto id : mounted_) {
        const auto* s = services_->components().state<RateState>(id);
        if (s && s->tooltip_leave_deadline && window_active_ && input_container_active(*s) &&
            services_->interactions().component_active(id) && (!result || *s->tooltip_leave_deadline < *result)) {
            result = s->tooltip_leave_deadline;
        }
    }
    return result;
}

void RateComponentHost::synchronize_auxiliary_geometry(runtime::Size viewport, runtime::Rect document_clip) {
    const auto ids = mounted_;
    for (auto id : ids) {
        auto* s = services_->components().state<RateState>(id);
        if (!s) {
            continue;
        }
        s->clip = services_->component_clip(id, viewport, document_clip);
        const bool empty = !s->geometry_active;
        if (s->empty_reconciled != empty) {
            s->empty_reconciled = empty;
            reconcile_input(id);
            s = services_->components().state<RateState>(id);
            if (!s) {
                continue;
            }
        }
        const auto focus = services_->focus().state();
        std::vector<graphics::RoundedEffectInstance> effects;
        if (s->visible && s->count > 0 && editable(*s) && focus.focused == s->focus && focus.focus_visible &&
            services_->interactions().branch_active(s->focus)) {
            const auto& t = services_->components().theme_scope(id)->snapshot().rate();
            const auto index = static_cast<std::size_t>(std::max(0.0F, std::ceil(s->value) - 1));
            const auto& row = *services_->components().state<RateItemState>(s->items[index]);
            const auto& node = services_->nodes().require(row.paint);
            if (t.metrics.focus_width > 0) {
                effects.push_back(graphics::make_outline_effect(
                    {node.bounds, 0}, t.metrics.focus_width, 0, t.colors.star,
                    services_->nodes().content_opacity(s->node), node.translation,
                    graphics::EffectClip{
                        1, services_->component_clip(id, viewport, document_clip, runtime::ClipPurpose::decoration)}));
            }
        }
        static_cast<void>(services_->surfaces().update_content_effects(s->decoration, effects));
        if (s->geometry_active) {
            services_->interactions().set_eligible(s->focus, input_container_active(*s) && window_active_);
            for (int i = 0; i < s->count; ++i) {
                services_->interactions().set_eligible(
                    services_->components().state<RateItemState>(s->items[i])->view.interaction,
                    editable(*s) && window_active_);
            }
        }
    }
}
} // namespace ryn::detail

namespace ryn {
void Rate(RateProps props) {
    if (!detail::active_rate) {
        throw std::logic_error("Rate requires window component services");
    }
    detail::active_rate->mount(props);
}
} // namespace ryn
