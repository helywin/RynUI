#include "component/splitter_component.hpp"

#include "component/splitter_model.hpp"
#include "input/pressable_behavior.hpp"
#include "runtime/layout_style_adapter.hpp"
#include "runtime/prop_connection.hpp"

#include <algorithm>
#include <cmath>
#include <numeric>
#include <stdexcept>

namespace ryn::detail {
namespace {

thread_local SplitterComponentHost* active_splitter{};

struct PanelState final {
    SplitterPanelSnapshot snapshot;
    SplitterPanelContent content{[] {}};
    std::optional<SplitterSize> controlled_size;
    std::optional<SplitterSize> initial_size;
    SplitterSize minimum{dp(0)};
    SplitterSize maximum{SplitterSize::percent(100)};
    SplitterCollapsible collapsible;
    std::optional<bool> destroy_override;
    bool resizable{true};
    bool collapsed_request{};
    float restore_size{};
    component::RetainedSurfaceId surface;
};

struct CollapseControl final {
    runtime::ComponentId component;
    input::InteractionId interaction;
    input::PressableBehavior press;
    input::FocusPresentation focus;
    component::RetainedSurfaceId surface;
    Signal<IconName> icon{IconName::LeftOutlined};
    bool enabled{};
    bool hovered{};
    SplitterIconVisibility visibility{SplitterIconVisibility::Auto};
};

struct BarState final {
    runtime::ComponentId component;
    runtime::ComponentId icon;
    input::InteractionId interaction;
    component::RetainedSurfaceId surface;
    component::RetainedSurfaceId mark_surface;
    CollapseControl start;
    CollapseControl end;
    input::FocusPresentation focus;
    bool hovered{};
    bool resizable{};
};

struct SplitterState final {
    runtime::NodeId node;
    std::vector<PanelState> panels;
    std::vector<BarState> bars;
    SplitterSizes sizes;
    SplitterSizes visual;
    SplitterSizes preview;
    SplitterSizes drag_start;
    std::vector<SplitterLimits> limits;
    SplitterOrientation orientation{SplitterOrientation::Horizontal};
    SplitterDirection direction{SplitterDirection::LeftToRight};
    bool lazy{};
    bool destroy_hidden{};
    bool collapse_motion{};
    bool initialized{};
    bool config_dirty{};
    bool constraints_dirty{};
    float total{};
    std::optional<std::size_t> dragging;
    std::size_t resize_index{};
    bool direction_confirmed{};
    input::PointerIdentity pointer;
    float drag_origin{};
    component::RetainedSurfaceId preview_surface;
    std::array<theme_runtime::Subscription, 2> subscriptions;
    Signal<runtime::SemanticForeground> foreground{{0, 0, 0, 1}};
    Signal<runtime::SemanticTypography> typography{runtime::SemanticTypography{}};
    bool animating{};
    SplitterSizes motion_from;
    animation::AnimationTime motion_start;
    animation::AnimationTime last_tick;
    animation::AnimationDuration motion_duration;
    animation::Easing motion_easing{animation::Easing::linear()};
    std::function<void(SplitterSizes)> on_start;
    std::function<void(SplitterSizes)> on_resize;
    std::function<void(SplitterSizes)> on_end;
    std::function<void(SplitterCollapsed, SplitterSizes)> on_collapse;
    std::function<void(std::size_t)> on_double_click;
};

bool alive(WindowComponentServices& services, runtime::ComponentId id) {
    return services.components().contains(id) && services.components().scope(id).active();
}

bool reverse(const SplitterState& s) {
    return s.orientation == SplitterOrientation::Horizontal && s.direction == SplitterDirection::RightToLeft;
}

void validate(SplitterOrientation value) {
    if (value != SplitterOrientation::Horizontal && value != SplitterOrientation::Vertical) {
        throw std::invalid_argument("Splitter orientation is invalid");
    }
}

void validate(SplitterDirection value) {
    if (value != SplitterDirection::LeftToRight && value != SplitterDirection::RightToLeft) {
        throw std::invalid_argument("Splitter direction is invalid");
    }
}

void validate(SplitterCollapsible value) {
    if (value.show_icon != SplitterIconVisibility::Auto && value.show_icon != SplitterIconVisibility::Always &&
        value.show_icon != SplitterIconVisibility::Hidden) {
        throw std::invalid_argument("Splitter collapse icon visibility is invalid");
    }
}

void validate_limits(SplitterSize minimum, SplitterSize maximum) {
    if (minimum.is_percentage() == maximum.is_percentage() && minimum.value() > maximum.value()) {
        throw std::invalid_argument("Splitter min exceeds max");
    }
}

runtime::SemanticForeground foreground(Color color) {
    return {color.red(), color.green(), color.blue(), color.alpha()};
}

bool descendant(WindowComponentServices& services, runtime::ComponentId child, runtime::ComponentId root) {
    for (auto current = std::optional{child}; current; current = services.components().parent(*current)) {
        if (*current == root) {
            return true;
        }
    }
    return false;
}

bool can_resize(const SplitterState& s, std::size_t i) {
    return s.total > 0 && s.panels[i].resizable && s.panels[i + 1].resizable &&
           (!s.panels[i].snapshot.collapsed || s.panels[i].minimum.value() == 0) &&
           (!s.panels[i + 1].snapshot.collapsed || s.panels[i + 1].minimum.value() == 0);
}

std::pair<bool, SplitterIconVisibility> collapse_control(const SplitterState& s, std::size_t i, bool start) {
    const auto& left = s.panels[i];
    const auto& right = s.panels[i + 1];
    const bool left_visible = !left.snapshot.collapsed;
    const bool right_visible = !right.snapshot.collapsed;
    const bool a = start ? left_visible && left.collapsible.end : right_visible && right.collapsible.start;
    const bool b = start ? !right_visible && left_visible && right.collapsible.start
                         : !left_visible && right_visible && left.collapsible.end;
    const auto first = start ? left.collapsible.show_icon : right.collapsible.show_icon;
    const auto second = start ? right.collapsible.show_icon : left.collapsible.show_icon;
    const auto visibility = a && b
                                ? (first == SplitterIconVisibility::Always || second == SplitterIconVisibility::Always
                                       ? SplitterIconVisibility::Always
                                   : first == SplitterIconVisibility::Auto || second == SplitterIconVisibility::Auto
                                       ? SplitterIconVisibility::Auto
                                       : SplitterIconVisibility::Hidden)
                            : a ? first
                                : second;
    return {s.total > 0 && (a || b), visibility};
}

void start_motion(WindowComponentServices& services, runtime::ComponentId id, SplitterState& s,
                  const SplitterSizes& before, bool requested) {
    const auto policy = animation::resolve_motion_policy(services.components().theme_scope(id)->snapshot(),
                                                         services.motion_preference());
    s.animating =
        requested && s.collapse_motion && policy.enabled() && before.size() == s.sizes.size() && before != s.sizes;
    if (s.animating) {
        s.motion_from = before;
        s.visual = before;
        s.motion_start = services.animation_time();
        s.last_tick = s.motion_start;
        s.motion_duration = policy.tokens().slow;
        s.motion_easing = policy.tokens().easing(animation::MotionEasingToken::ease_in_out);
        if (s.motion_duration.count_microseconds() == 0) {
            s.animating = false;
        }
    }
    if (!s.animating) {
        s.visual = s.sizes;
    }
}

} // namespace

SplitterComponentHost::SplitterComponentHost(WindowComponentServices& services) : services_(&services) {
    services.attach(*this);
}

SplitterComponentHost::~SplitterComponentHost() {
    services_->detach(*this);
}

void* SplitterComponentHost::begin_mount() noexcept {
    auto* previous = active_splitter;
    active_splitter = this;
    return previous;
}

void SplitterComponentHost::end_mount(void* previous) noexcept {
    active_splitter = static_cast<SplitterComponentHost*>(previous);
}

void SplitterComponentHost::on_destroy() noexcept {
    std::erase_if(mounted_, [this](auto id) { return !services_->components().contains(id); });
}

SplitterSnapshot SplitterComponentHost::snapshot(runtime::ComponentId id) const {
    const auto* s = services_->components().state<SplitterState>(id);
    if (!s) {
        throw std::out_of_range("Splitter component is stale");
    }
    SplitterSnapshot result;
    result.node = s->node;
    result.preview_surface = s->preview_surface;
    result.orientation = s->orientation;
    result.sizes = s->sizes;
    result.visual_sizes = s->visual;
    result.preview_sizes = s->preview;
    result.lazy = s->lazy;
    result.dragging = s->dragging.has_value();
    result.animating = s->animating;
    for (const auto& panel : s->panels) {
        result.panels.push_back(panel.snapshot);
    }
    for (const auto& bar : s->bars) {
        result.bars.push_back({bar.component, bar.start.component, bar.end.component, bar.interaction,
                               bar.start.interaction, bar.end.interaction, bar.resizable, bar.start.enabled,
                               bar.end.enabled});
    }
    return result;
}

void SplitterComponentHost::mount_content(runtime::ComponentId id, std::size_t index,
                                          runtime::ComponentBuildContext* initial) {
    auto* s = services_->components().state<SplitterState>(id);
    if (!s || index >= s->panels.size() || s->panels[index].snapshot.content.valid()) {
        return;
    }
    const auto parent = s->panels[index].snapshot.component;
    const auto content = Content{[this, id, index] {
        auto& build = runtime::require_component_build_context();
        auto* state = services_->components().state<SplitterState>(id);
        if (!state) {
            return;
        }
        const auto body = build.mount_component<int>(0);
        state->panels[index].snapshot.content = body;
        const auto node = build.root(body);
        services_->components().set_branch_active(body, !state->panels[index].snapshot.collapsed);
        services_->nodes().require(node).clip_content = true;
        services_->layout().set_layout(node, layout::BoxLayout{{}, true, true});
        build.on_resource_cleanup(body, [this, node] { services_->layout().remove_layout(node); });
        build.mount_slot_with_semantic_text_style(body, state->panels[index].content,
                                                  Prop<runtime::SemanticForeground>{state->foreground},
                                                  Prop<runtime::SemanticTypography>{state->typography});
    }};
    try {
        if (initial) {
            initial->mount_slot(parent, content);
        } else {
            services_->append_slot(parent, content);
        }
    } catch (...) {
        if (auto* state = services_->components().state<SplitterState>(id)) {
            if (!services_->components().contains(state->panels[index].snapshot.content)) {
                state->panels[index].snapshot.content = {};
            }
        }
        throw;
    }
}

void SplitterComponentHost::synchronize_content(runtime::ComponentId id, bool allow_mount) {
    auto* s = services_->components().state<SplitterState>(id);
    if (!s || !alive(*services_, id)) {
        return;
    }
    const auto count = s->panels.size();
    for (std::size_t i = 0; i < count; ++i) {
        s = services_->components().state<SplitterState>(id);
        if (!s || !alive(*services_, id)) {
            return;
        }
        auto& p = s->panels[i];
        const bool hidden = p.snapshot.collapsed;
        const bool destroy = p.destroy_override.value_or(s->destroy_hidden);
        p.snapshot.destroy_on_hidden = destroy;
        const auto body = p.snapshot.content;
        if (hidden && body.valid()) {
            if (services_->components().set_branch_active(body, false)) {
                services_->mark_scene_structure_dirty();
            }
            const auto order = services_->interactions().declaration_order();
            const std::vector<input::InteractionId> interactions{order.begin(), order.end()};
            for (auto hit : interactions) {
                const auto* record = services_->interactions().find(hit);
                if (record && services_->components().contains(body) &&
                    descendant(*services_, record->component, body)) {
                    services_->pointer().cancel_interaction(hit);
                }
            }
            if (!alive(*services_, id)) {
                return;
            }
            if (destroy) {
                services_->components().state<SplitterState>(id)->panels[i].snapshot.content = {};
                services_->destroy(body);
            }
        } else if (!hidden) {
            if (!body.valid()) {
                if (allow_mount) {
                    mount_content(id, i);
                } else {
                    services_->request_frame();
                }
            } else if (services_->components().set_branch_active(body, true)) {
                services_->mark_scene_structure_dirty();
            }
        }
    }
}

void SplitterComponentHost::mount(const SplitterProps& props, const std::vector<SplitterPanel>& panels) {
    if (panels.empty()) {
        throw std::invalid_argument("Splitter requires at least one Panel");
    }
    const auto orientation = props.orientation_           ? read_prop(*props.orientation_)
                             : read_prop(props.vertical_) ? SplitterOrientation::Vertical
                                                          : SplitterOrientation::Horizontal;
    validate(orientation);
    validate(read_prop(props.direction_));
    for (const auto& panel : panels) {
        if (panel.size_ && panel.default_size_) {
            throw std::invalid_argument("Splitter size/defaultSize conflict");
        }
        validate(read_prop(panel.collapsible_));
        validate_limits(read_prop(panel.min_), read_prop(panel.max_));
    }
    auto& build = runtime::require_component_build_context();
    auto& services = *services_;
    const auto id = build.mount_component<SplitterState>();
    auto& s = build.state<SplitterState>(id);
    s.node = build.root(id);
    s.orientation = orientation;
    s.direction = read_prop(props.direction_);
    s.lazy = read_prop(props.lazy_);
    s.destroy_hidden = read_prop(props.destroy_on_hidden_);
    s.collapse_motion = read_prop(props.collapse_motion_);
    s.on_start = props.on_resize_start_;
    s.on_resize = props.on_resize_;
    s.on_end = props.on_resize_end_;
    s.on_collapse = props.on_collapse_;
    s.on_double_click = props.on_double_click_;
    services.nodes().require(s.node).clip_content = true;
    runtime::connect_layout_style(build.scope(id), props.layout_, s.node, services.nodes(), services.dirty());
    const auto preview_fragment = build.register_scene_fragment(id, runtime::SceneFragmentPlacement::after_children);
    s.preview_surface = services.surfaces().create_content_range(preview_fragment, {});
    build.on_resource_cleanup(id, [&services, node = s.node, surface = s.preview_surface] {
        services.surfaces().destroy_content_range(surface);
        services.layout().remove_layout(node);
    });
    s.panels.reserve(panels.size());
    s.bars.reserve(panels.size() - 1);
    build.mount_slot(
        id, Content{[&] {
            auto& context = runtime::require_component_build_context();
            for (std::size_t i = 0; i < panels.size(); ++i) {
                const auto& panel = panels[i];
                s.panels.emplace_back();
                auto& p = s.panels.back();
                p.content = panel.content_;
                if (panel.size_) {
                    p.controlled_size = read_prop(*panel.size_);
                }
                p.initial_size = panel.default_size_;
                p.minimum = read_prop(panel.min_);
                p.maximum = read_prop(panel.max_);
                p.resizable = read_prop(panel.resizable_);
                p.collapsible = read_prop(panel.collapsible_);
                if (panel.destroy_on_hidden_) {
                    p.destroy_override = read_prop(*panel.destroy_on_hidden_);
                }
                const auto initial = p.controlled_size ? p.controlled_size : p.initial_size;
                p.collapsed_request = initial && initial->value() == 0;
                p.snapshot.collapsed = p.collapsed_request;
                p.snapshot.destroy_on_hidden = p.destroy_override.value_or(s.destroy_hidden);
                p.snapshot.component = context.mount_component<int>(0);
                const auto node = context.root(p.snapshot.component);
                services.nodes().require(node).clip_content = true;
                services.layout().set_layout(node, layout::BoxLayout{{}, true, true});
                const auto fragment = context.register_scene_fragment(p.snapshot.component,
                                                                      runtime::SceneFragmentPlacement::before_children);
                p.surface = services.surfaces().create_content_range(fragment, {});
                context.on_resource_cleanup(p.snapshot.component, [&services, node, surface = p.surface] {
                    services.surfaces().destroy_content_range(surface);
                    services.layout().remove_layout(node);
                });
                if (!p.snapshot.collapsed || !p.snapshot.destroy_on_hidden) {
                    mount_content(id, i, &context);
                }
            }
            for (std::size_t i = 0; i + 1 < panels.size(); ++i) {
                s.bars.emplace_back();
                auto& bar = s.bars.back();
                bar.component = context.mount_component<int>(0);
                const auto node = context.root(bar.component);
                services.layout().set_layout(node, layout::LeafLayout{});
                bar.interaction = services.interactions().create({bar.component, node, {}, true, true, {}, true, true});
                const auto hit =
                    context.register_scene_fragment(bar.component, runtime::SceneFragmentPlacement::before_children);
                services.scene_composer().set_fragment(hit, {}, bar.interaction);
                const auto fragment =
                    context.register_scene_fragment(bar.component, runtime::SceneFragmentPlacement::before_children);
                bar.surface = services.surfaces().create_content_range(fragment, {});
                const auto mark_fragment =
                    context.register_scene_fragment(bar.component, runtime::SceneFragmentPlacement::before_children);
                bar.mark_surface = services.surfaces().create_content_range(mark_fragment, {});
                context.on_resource_cleanup(bar.component, [&services, node, hit, interaction = bar.interaction,
                                                            surface = bar.surface, mark = bar.mark_surface] {
                    services.pointer().cancel_interaction(interaction);
                    services.interactions().remove(interaction);
                    services.scene_composer().remove_fragment(hit);
                    services.surfaces().destroy_content_range(surface);
                    services.surfaces().destroy_content_range(mark);
                    services.layout().remove_layout(node);
                });
                input::InteractionHandlers handlers;
                handlers.target = [this, id, i](input::PointerDispatchContext& event) {
                    pointer(id, i, event);
                };
                services.interactions().set_handlers(bar.interaction, std::move(handlers));
                input::FocusHandlers focus;
                focus.state_changed = [this, id, i](input::FocusPresentation value) {
                    if (auto* state = services_->components().state<SplitterState>(id)) {
                        state->bars[i].focus = value;
                        refresh(id, false);
                    }
                };
                focus.text_edit = [this, id, i](const input::KeyboardInputEvent& event) {
                    return keyboard(id, i, event);
                };
                services.interactions().set_focus_handlers(bar.interaction, std::move(focus));
                if (props.dragger_icon_) {
                    context.mount_slot(
                        bar.component, Content{[&] {
                            auto& child = runtime::require_component_build_context();
                            bar.icon = child.mount_component<int>(0);
                            const auto icon_node = child.root(bar.icon);
                            services.layout().set_layout(icon_node, layout::BoxLayout{});
                            child.on_resource_cleanup(
                                bar.icon, [&services, icon_node] { services.layout().remove_layout(icon_node); });
                            child.mount_slot_with_semantic_text_style(bar.icon, *props.dragger_icon_,
                                                                      Prop<runtime::SemanticForeground>{s.foreground},
                                                                      Prop<runtime::SemanticTypography>{s.typography});
                        }});
                }
                const auto control = [&](bool start, CollapseControl& c) {
                    c.component = context.mount_component<int>(0);
                    const auto cn = context.root(c.component);
                    services.layout().set_layout(cn, layout::FlexLayout{.fill_width = true,
                                                                        .fill_height = true,
                                                                        .justify = layout::FlexJustify::center,
                                                                        .align = layout::FlexAlign::center});
                    c.interaction = services.interactions().create({c.component, cn, {}, true, true, {}, true, true});
                    const auto ch =
                        context.register_scene_fragment(c.component, runtime::SceneFragmentPlacement::before_children);
                    services.scene_composer().set_fragment(ch, {}, c.interaction);
                    const auto cf =
                        context.register_scene_fragment(c.component, runtime::SceneFragmentPlacement::before_children);
                    c.surface = services.surfaces().create_content_range(cf, {});
                    context.on_resource_cleanup(c.component,
                                                [&services, cn, ch, hit = c.interaction, surface = c.surface] {
                                                    services.pointer().cancel_interaction(hit);
                                                    services.interactions().remove(hit);
                                                    services.scene_composer().remove_fragment(ch);
                                                    services.surfaces().destroy_content_range(surface);
                                                    services.layout().remove_layout(cn);
                                                });
                    input::InteractionHandlers events;
                    events.target = [this, id, i, start](input::PointerDispatchContext& event) {
                        auto* state = services_->components().state<SplitterState>(id);
                        if (!state) {
                            return;
                        }
                        auto& bar = state->bars[i];
                        auto& c = start ? bar.start : bar.end;
                        if (event.kind() == input::PointerEventKind::enter ||
                            event.kind() == input::PointerEventKind::leave ||
                            event.kind() == input::PointerEventKind::cancel) {
                            c.hovered = event.kind() == input::PointerEventKind::enter;
                            refresh(id, false);
                        }
                        const auto result = c.press.dispatch(
                            event, c.interaction, c.enabled && services_->interactions().component_active(c.component));
                        if (event.kind() == input::PointerEventKind::down ||
                            event.kind() == input::PointerEventKind::up) {
                            event.stop_propagation();
                        }
                        if (result.activate) {
                            collapse(id, i, start);
                        }
                    };
                    services.interactions().set_handlers(c.interaction, std::move(events));
                    input::FocusHandlers keys;
                    keys.state_changed = [this, id, i, start](input::FocusPresentation value) {
                        if (auto* state = services_->components().state<SplitterState>(id)) {
                            (start ? state->bars[i].start : state->bars[i].end).focus = value;
                            refresh(id, false);
                        }
                    };
                    keys.activate = [this, id, i, start] {
                        collapse(id, i, start);
                    };
                    services.interactions().set_focus_handlers(c.interaction, std::move(keys));
                    if (start && props.start_icon_) {
                        context.mount_slot_with_semantic_text_style(c.component, *props.start_icon_,
                                                                    Prop<runtime::SemanticForeground>{s.foreground},
                                                                    Prop<runtime::SemanticTypography>{s.typography});
                    } else if (!start && props.end_icon_) {
                        context.mount_slot_with_semantic_text_style(c.component, *props.end_icon_,
                                                                    Prop<runtime::SemanticForeground>{s.foreground},
                                                                    Prop<runtime::SemanticTypography>{s.typography});
                    } else {
                        context.mount_slot_with_semantic_text_style(c.component,
                                                                    Content{[&c] { Icon(IconProps{}.name(c.icon)); }},
                                                                    Prop<runtime::SemanticForeground>{s.foreground},
                                                                    Prop<runtime::SemanticTypography>{s.typography});
                    }
                };
                control(true, bar.start);
                control(false, bar.end);
            }
        }});
    services.layout().set_layout(
        s.node, layout::ComponentLayout{[this, id](layout::LayoutEngine& engine, runtime::NodeId,
                                                   layout::Constraints limits) { return measure(id, engine, limits); },
                                        [this, id](layout::LayoutEngine& engine, runtime::NodeId,
                                                   runtime::Rect bounds) { place(id, engine, bounds); }});
    mounted_.push_back(id);
    auto& scope = build.scope(id);
    const auto changed = [this, id](bool constraints) {
        if (auto* state = services_->components().state<SplitterState>(id)) {
            state->config_dirty = true;
            state->constraints_dirty = state->constraints_dirty || constraints;
            refresh(id);
        }
    };
    if (props.orientation_) {
        connect_prop(scope, *props.orientation_, [this, id, changed](SplitterOrientation value) {
            validate(value);
            if (auto* state = services_->components().state<SplitterState>(id)) {
                state->orientation = value;
                changed(true);
            }
        });
    } else {
        connect_prop(scope, props.vertical_, [this, id, changed](bool value) {
            if (auto* state = services_->components().state<SplitterState>(id)) {
                state->orientation = value ? SplitterOrientation::Vertical : SplitterOrientation::Horizontal;
                changed(true);
            }
        });
    }
    connect_prop(scope, props.direction_, [this, id, changed](SplitterDirection value) {
        validate(value);
        if (auto* state = services_->components().state<SplitterState>(id)) {
            state->direction = value;
            changed(true);
        }
    });
    connect_prop(scope, props.lazy_, [this, id, changed](bool value) {
        if (auto* state = services_->components().state<SplitterState>(id)) {
            state->lazy = value;
            changed(true);
        }
    });
    connect_prop(scope, props.destroy_on_hidden_, [this, id, changed](bool value) {
        if (auto* state = services_->components().state<SplitterState>(id)) {
            state->destroy_hidden = value;
            changed(false);
        }
    });
    connect_prop(scope, props.collapse_motion_, [this, id, changed](bool value) {
        if (auto* state = services_->components().state<SplitterState>(id)) {
            state->collapse_motion = value;
            changed(false);
        }
    });
    for (std::size_t i = 0; i < panels.size(); ++i) {
        const auto& panel = panels[i];
        if (panel.size_) {
            connect_prop(scope, *panel.size_, [this, id, i, changed](SplitterSize value) {
                if (auto* state = services_->components().state<SplitterState>(id)) {
                    state->panels[i].controlled_size = value;
                    state->panels[i].collapsed_request = value.value() == 0;
                    changed(false);
                }
            });
        }
        connect_prop(scope, panel.min_, [this, id, i, changed](SplitterSize value) {
            if (auto* state = services_->components().state<SplitterState>(id)) {
                validate_limits(value, state->panels[i].maximum);
                state->panels[i].minimum = value;
                changed(true);
            }
        });
        connect_prop(scope, panel.max_, [this, id, i, changed](SplitterSize value) {
            if (auto* state = services_->components().state<SplitterState>(id)) {
                validate_limits(state->panels[i].minimum, value);
                state->panels[i].maximum = value;
                changed(true);
            }
        });
        connect_prop(scope, panel.resizable_, [this, id, i, changed](bool value) {
            if (auto* state = services_->components().state<SplitterState>(id)) {
                state->panels[i].resizable = value;
                changed(true);
            }
        });
        connect_prop(scope, panel.collapsible_, [this, id, i, changed](SplitterCollapsible value) {
            validate(value);
            if (auto* state = services_->components().state<SplitterState>(id)) {
                state->panels[i].collapsible = value;
                changed(false);
            }
        });
        if (panel.destroy_on_hidden_) {
            connect_prop(scope, *panel.destroy_on_hidden_, [this, id, i, changed](bool value) {
                if (auto* state = services_->components().state<SplitterState>(id)) {
                    state->panels[i].destroy_override = value;
                    changed(false);
                }
            });
        }
    }
    const auto theme = services.components().theme_scope(id);
    const std::array accessors{&theme_runtime::ThemeScope::splitter_colors,
                               &theme_runtime::ThemeScope::splitter_metrics};
    for (std::size_t i = 0; i < accessors.size(); ++i) {
        s.subscriptions[i] =
            theme->capture([this, id](theme_runtime::DirtyPhase) { refresh(id); },
                           [theme, accessor = accessors[i]] { static_cast<void>((theme.get()->*accessor)()); });
    }
    refresh(id);
}

void SplitterComponentHost::resolve(runtime::ComponentId id, float total, bool animate) {
    auto& s = *services_->components().state<SplitterState>(id);
    const auto before = s.visual;
    SplitterSizes desired(s.panels.size());
    const bool controlled = std::ranges::any_of(s.panels, [](const auto& p) { return p.controlled_size.has_value(); });
    double fixed{};
    std::size_t automatic{};
    s.limits.resize(s.panels.size());
    for (std::size_t i = 0; i < s.panels.size(); ++i) {
        const auto value = controlled ? s.panels[i].controlled_size : s.panels[i].initial_size;
        if (value) {
            desired[i] = value->resolve(total);
            fixed += desired[i];
        } else {
            desired[i] = -1;
            ++automatic;
        }
    }
    if ((automatic == 0 || fixed > total) && fixed > 0) {
        for (auto& value : desired) {
            value = value < 0 ? 0 : static_cast<float>(value / fixed * total);
        }
    } else if (automatic == 0) {
        const auto share = total / static_cast<float>(desired.size());
        std::fill(desired.begin(), desired.end(), share);
    } else {
        const auto share = static_cast<float>(std::max(0.0, total - fixed) / automatic);
        for (auto& value : desired) {
            if (value < 0) {
                value = share;
            }
        }
    }
    for (std::size_t i = 0; i < s.panels.size(); ++i) {
        const auto& p = s.panels[i];
        s.limits[i] = {p.minimum.resolve(total), p.maximum.resolve(total), desired[i] == 0};
    }
    s.sizes = solve_splitter_sizes(total, desired, s.limits);
    s.total = total;
    const bool was_initialized = s.initialized;
    s.initialized = s.initialized || total > 0;
    s.config_dirty = false;
    bool collapse_changed{};
    for (std::size_t i = 0; i < s.panels.size(); ++i) {
        auto& p = s.panels[i];
        const bool collapsed = s.sizes[i] == 0;
        collapse_changed = collapse_changed || p.snapshot.collapsed != collapsed;
        p.snapshot.collapsed = collapsed;
        if (s.sizes[i] > 0 && total > 0) {
            p.restore_size = s.sizes[i];
        }
        if (p.snapshot.content.valid()) {
            if (services_->components().set_branch_active(p.snapshot.content, !collapsed)) {
                services_->mark_scene_structure_dirty();
            }
        }
    }
    start_motion(*services_, id, s, before, animate && was_initialized && collapse_changed);
}

void SplitterComponentHost::refresh(runtime::ComponentId id, bool geometry) {
    auto* s = services_->components().state<SplitterState>(id);
    if (!s || !alive(*services_, id)) {
        return;
    }
    const auto& t = services_->components().theme_scope(id)->snapshot().splitter();
    s->foreground.set(foreground(t.colors.icon));
    s->typography.set({SystemFontFamily::ui_sans, 400, false, t.metrics.icon_size, t.metrics.icon_size});
    std::vector<input::InteractionId> disabled;
    for (std::size_t i = 0; i < s->bars.size(); ++i) {
        auto& bar = s->bars[i];
        bar.resizable = s->initialized && can_resize(*s, i);
        services_->interactions().set_focusable(bar.interaction, bar.resizable);
        for (bool start : {true, false}) {
            auto& c = start ? bar.start : bar.end;
            const auto [enabled, visibility] = collapse_control(*s, i, start);
            c.enabled = enabled && visibility != SplitterIconVisibility::Hidden;
            c.visibility = visibility;
            if (!c.enabled) {
                c.hovered = false;
            }
            services_->interactions().set_eligible(c.interaction, c.enabled);
            if (services_->components().set_branch_active(c.component, c.enabled)) {
                services_->mark_scene_structure_dirty();
            }
            if (!c.enabled) {
                disabled.push_back(c.interaction);
            }
            const bool points_start = start != reverse(*s);
            c.icon.set(s->orientation == SplitterOrientation::Vertical
                           ? (points_start ? IconName::UpOutlined : IconName::DownOutlined)
                           : (points_start ? IconName::LeftOutlined : IconName::RightOutlined));
        }
        const bool hover_eligible = bar.resizable || bar.start.enabled || bar.end.enabled;
        if (!hover_eligible) {
            bar.hovered = false;
        }
        for (const auto* c : {&bar.start, &bar.end}) {
            const bool visible = c->visibility == SplitterIconVisibility::Always || bar.hovered || bar.start.hovered ||
                                 bar.end.hovered || bar.focus.focused || bar.start.focus.focused ||
                                 bar.end.focus.focused;
            auto& control_node = services_->nodes().require(services_->components().root(c->component));
            if (control_node.content_opacity != (visible ? 1 : 0)) {
                control_node.content_opacity = visible ? 1 : 0;
                services_->dirty().invalidate_subtree(control_node.id, runtime::DirtyFlags::Material);
            }
        }
        // A non-resizable seam still owns hover so Auto restore controls can be discovered.
        services_->interactions().set_eligible(bar.interaction, hover_eligible);
        if (!bar.resizable && s->dragging && (*s->dragging == i || s->resize_index == i)) {
            cancel(id);
        }
    }
    services_->dirty().invalidate(s->node, geometry ? runtime::DirtyFlags::Measure | runtime::DirtyFlags::Layout |
                                                          runtime::DirtyFlags::Geometry | runtime::DirtyFlags::HitTest |
                                                          runtime::DirtyFlags::Material
                                                    : runtime::DirtyFlags::Material);
    for (auto hit : disabled) {
        services_->pointer().cancel_interaction(hit);
    }
}

void SplitterComponentHost::prepare_auxiliary_layout() {
    const auto owners = mounted_;
    for (auto id : owners) {
        auto* s = services_->components().state<SplitterState>(id);
        if (!s || !alive(*services_, id)) {
            continue;
        }
        if (s->constraints_dirty) {
            cancel(id);
        }
        s = services_->components().state<SplitterState>(id);
        if (!s) {
            continue;
        }
        s->constraints_dirty = false;
        if (s->config_dirty && s->initialized) {
            resolve(id, s->total, true);
            refresh(id);
        }
        synchronize_content(id);
    }
}

void SplitterComponentHost::cancel(runtime::ComponentId id) {
    auto* s = services_->components().state<SplitterState>(id);
    if (!s || !s->dragging) {
        return;
    }
    const auto hit = s->bars[*s->dragging].interaction;
    s->dragging.reset();
    s->preview.clear();
    s->drag_start.clear();
    services_->dirty().invalidate(s->node, runtime::DirtyFlags::Geometry | runtime::DirtyFlags::Material);
    services_->pointer().cancel_pointer_interaction(hit);
}

void SplitterComponentHost::on_window_active(bool active) {
    if (!active) {
        const auto owners = mounted_;
        for (auto id : owners) {
            cancel(id);
        }
    }
}

void SplitterComponentHost::on_input_boundary_changed() {
    const auto owners = mounted_;
    for (auto id : owners) {
        const auto* s = services_->components().state<SplitterState>(id);
        if (s && s->dragging && !services_->interactions().component_active(s->bars[*s->dragging].component)) {
            cancel(id);
        }
    }
}

void SplitterComponentHost::commit(runtime::ComponentId id, std::size_t, SplitterSizes values, bool motion) {
    auto* s = services_->components().state<SplitterState>(id);
    if (!s || !alive(*services_, id)) {
        return;
    }
    if (std::ranges::any_of(s->panels, [](const auto& p) { return p.controlled_size.has_value(); })) {
        return;
    }
    const auto before = s->visual;
    s->sizes = std::move(values);
    for (std::size_t i = 0; i < s->panels.size(); ++i) {
        auto& p = s->panels[i];
        p.snapshot.collapsed = s->sizes[i] == 0;
        p.collapsed_request = p.snapshot.collapsed;
        p.initial_size = SplitterSize{dp(s->sizes[i])};
        if (s->sizes[i] > 0 && s->total > 0) {
            p.restore_size = s->sizes[i];
        }
    }
    start_motion(*services_, id, *s, before, motion);
    synchronize_content(id);
    refresh(id);
}

void SplitterComponentHost::emit(runtime::ComponentId id, const SplitterSizes& values, bool change, bool end,
                                 bool collapse_event) {
    const auto* s = services_->components().state<SplitterState>(id);
    if (!s || !alive(*services_, id)) {
        return;
    }
    const auto resize = s->on_resize;
    const auto finish = s->on_end;
    const auto collapsed = s->on_collapse;
    try {
        if (change && resize) {
            resize(values);
        }
        if (alive(*services_, id) && end && finish) {
            finish(values);
        }
        if (alive(*services_, id) && collapse_event && collapsed) {
            SplitterCollapsed flags;
            for (auto value : values) {
                flags.push_back(value == 0);
            }
            collapsed(std::move(flags), values);
        }
    } catch (...) {
        cancel(id);
        throw;
    }
}

void SplitterComponentHost::pointer(runtime::ComponentId id, std::size_t index, input::PointerDispatchContext& event) {
    auto* s = services_->components().state<SplitterState>(id);
    if (!s || !alive(*services_, id)) {
        return;
    }
    auto& bar = s->bars[index];
    const auto kind = event.kind();
    if (kind == input::PointerEventKind::enter || kind == input::PointerEventKind::leave) {
        bar.hovered = kind == input::PointerEventKind::enter;
        refresh(id, false);
        return;
    }
    if (kind == input::PointerEventKind::cancel) {
        if (s->dragging && s->pointer == event.event().pointer) {
            cancel(id);
        }
        return;
    }
    if (!bar.resizable || !services_->interactions().component_active(bar.component)) {
        cancel(id);
        return;
    }
    const auto& input = event.event();
    const float coordinate = s->orientation == SplitterOrientation::Vertical ? input.y : input.x;
    if (kind == input::PointerEventKind::down && input.button == input::PointerButton::primary) {
        event.stop_propagation();
        if (input.click_count >= 2) {
            const auto callback = s->on_double_click;
            if (callback) {
                callback(index);
            }
            return;
        }
        if (s->dragging || !event.capture_pointer()) {
            return;
        }
        s->dragging = index;
        s->resize_index = index;
        s->direction_confirmed = false;
        s->pointer = input.pointer;
        s->drag_origin = coordinate;
        s->drag_start = s->sizes;
        s->preview = s->sizes;
        s->animating = false;
        s->visual = s->sizes;
        const auto callback = s->on_start;
        const auto values = s->sizes;
        refresh(id, false);
        try {
            if (callback && alive(*services_, id)) {
                callback(values);
            }
        } catch (...) {
            cancel(id);
            throw;
        }
        return;
    }
    if (!s->dragging || *s->dragging != index || s->pointer != input.pointer) {
        return;
    }
    if (kind == input::PointerEventKind::move) {
        const float delta = (coordinate - s->drag_origin) * (reverse(*s) ? -1 : 1);
        if (!s->direction_confirmed && delta != 0) {
            s->direction_confirmed = true;
            if (delta < 0) {
                // A later bar paints over earlier bars at a collapsed seam.
                // Resolve the first movement toward the nearest preceding live pair.
                for (std::size_t candidate = index + 1; candidate-- > 0;) {
                    if (s->drag_start[candidate] > 0 && can_resize(*s, candidate)) {
                        s->resize_index = candidate;
                        break;
                    }
                }
            }
        }
        const auto pair = s->resize_index;
        auto values = resize_splitter_pair(s->drag_start, pair, s->drag_start[pair] + delta, s->limits);
        if (values == s->preview) {
            return;
        }
        s->preview = values;
        if (s->lazy) {
            services_->dirty().invalidate(s->node, runtime::DirtyFlags::Geometry | runtime::DirtyFlags::Material);
        } else {
            commit(id, pair, values, false);
            emit(id, values, true, false, false);
        }
    } else if (kind == input::PointerEventKind::up && input.button == input::PointerButton::primary) {
        event.stop_propagation();
        const auto values = s->preview;
        const bool lazy = s->lazy;
        const auto pair = s->resize_index;
        s->dragging.reset();
        s->preview.clear();
        s->drag_start.clear();
        static_cast<void>(event.release_pointer_capture());
        if (lazy) {
            commit(id, pair, values, false);
        }
        refresh(id, false);
        emit(id, values, lazy, true, false);
    }
}

bool SplitterComponentHost::keyboard(runtime::ComponentId id, std::size_t index,
                                     const input::KeyboardInputEvent& event) {
    auto* s = services_->components().state<SplitterState>(id);
    if (!s || !alive(*services_, id)) {
        return false;
    }
    if (event.key == input::Key::escape && s->dragging) {
        cancel(id);
        return true;
    }
    if (!s->bars[index].resizable || !services_->interactions().component_active(s->bars[index].component)) {
        return false;
    }
    const bool vertical = s->orientation == SplitterOrientation::Vertical;
    const bool negative = event.key == (vertical ? input::Key::up : input::Key::left);
    const bool positive = event.key == (vertical ? input::Key::down : input::Key::right);
    const bool home = event.key == input::Key::home;
    const bool end = event.key == input::Key::end;
    if (!negative && !positive && !home && !end) {
        return false;
    }
    if (event.action != input::KeyAction::down) {
        return true;
    }
    cancel(id);
    s = services_->components().state<SplitterState>(id);
    if (!s) {
        return true;
    }
    const auto initial = s->sizes;
    const float step = services_->components().theme_scope(id)->snapshot().splitter().metrics.keyboard_step *
                       (input::has_modifier(event.modifiers, input::KeyModifier::shift) ? 5 : 1);
    const float desired = home  ? 0
                          : end ? initial[index] + initial[index + 1]
                                : initial[index] + (negative ? -step : step) * (reverse(*s) ? -1 : 1);
    const auto values = resize_splitter_pair(initial, index, desired, s->limits);
    const auto callback = s->on_start;
    try {
        if (callback) {
            callback(initial);
        }
    } catch (...) {
        cancel(id);
        throw;
    }
    s = services_->components().state<SplitterState>(id);
    if (s && alive(*services_, id) && !s->constraints_dirty && can_resize(*s, index) &&
        services_->interactions().component_active(s->bars[index].component)) {
        commit(id, index, values, false);
        emit(id, values, values != initial, true, false);
    }
    return true;
}

void SplitterComponentHost::collapse(runtime::ComponentId id, std::size_t index, bool start) {
    auto* s = services_->components().state<SplitterState>(id);
    if (!s || !alive(*services_, id)) {
        return;
    }
    const auto& control = start ? s->bars[index].start : s->bars[index].end;
    if (!control.enabled || !services_->interactions().component_active(control.component)) {
        return;
    }
    cancel(id);
    s = services_->components().state<SplitterState>(id);
    if (!s) {
        return;
    }
    auto values = s->sizes;
    const auto initial = values;
    const std::size_t primary = start ? index : index + 1;
    const std::size_t other = start ? index + 1 : index;
    const std::size_t target =
        values[primary] > 0 && values[other] > 0 ? primary : (values[primary] == 0 ? primary : other);
    const std::size_t neighbor = target == index ? index + 1 : index;
    if (values[target] > 0) {
        s->panels[target].restore_size = values[target];
        values[neighbor] += values[target];
        values[target] = 0;
    } else {
        const float pair_total = values[index] + values[index + 1];
        auto limits = s->limits;
        limits[index].collapsed = false;
        limits[index + 1].collapsed = false;
        const float desired = std::clamp(
            (s->panels[target].restore_size > 0 ? s->panels[target].restore_size : pair_total / 2), 0.0F, pair_total);
        values = resize_splitter_pair(values, index, target == index ? desired : pair_total - desired, limits);
    }
    if (values == initial) {
        return;
    }
    commit(id, index, values, true);
    emit(id, values, true, true, true);
}

runtime::Size SplitterComponentHost::measure(runtime::ComponentId id, layout::LayoutEngine& engine,
                                             layout::Constraints limits) {
    auto& s = *services_->components().state<SplitterState>(id);
    const bool vertical = s.orientation == SplitterOrientation::Vertical;
    float width = limits.max_width;
    float height = limits.max_height;
    if (!std::isfinite(width)) {
        width = std::max(limits.min_width, vertical ? 200.0F : static_cast<float>(s.panels.size()) * 200);
    }
    if (!std::isfinite(height)) {
        height = std::max(limits.min_height, vertical ? static_cast<float>(s.panels.size()) * 200 : 200.0F);
    }
    const auto size = limits.constrain({width, height});
    const float total = vertical ? size.height : size.width;
    if (!s.initialized || s.total != total || s.config_dirty) {
        if (s.dragging && s.total != total) {
            cancel(id);
        }
        resolve(id, total);
        refresh(id, false);
    }
    const auto& m = services_->components().theme_scope(id)->snapshot().splitter().metrics;
    for (std::size_t i = 0; i < s.panels.size(); ++i) {
        const float extent = s.visual[i];
        static_cast<void>(engine.measure_child(
            services_->components().root(s.panels[i].snapshot.component),
            layout::Constraints::fixed(vertical ? size.width : extent, vertical ? extent : size.height)));
    }
    for (auto& bar : s.bars) {
        static_cast<void>(
            engine.measure_child(services_->components().root(bar.component),
                                 layout::Constraints::fixed(vertical ? size.width : m.split_trigger_size,
                                                            vertical ? m.split_trigger_size : size.height)));
        if (bar.icon.valid()) {
            static_cast<void>(engine.measure_child(services_->components().root(bar.icon),
                                                   layout::Constraints::fixed(m.icon_size, m.icon_size)));
        }
        for (const auto* c : {&bar.start, &bar.end}) {
            if (c->enabled) {
                static_cast<void>(
                    engine.measure_child(services_->components().root(c->component),
                                         layout::Constraints::fixed(vertical ? m.collapse_height : m.collapse_width,
                                                                    vertical ? m.collapse_width : m.collapse_height)));
            }
        }
    }
    return size;
}

void SplitterComponentHost::place(runtime::ComponentId id, layout::LayoutEngine& engine, runtime::Rect bounds) {
    auto& s = *services_->components().state<SplitterState>(id);
    const bool vertical = s.orientation == SplitterOrientation::Vertical;
    const bool rtl = reverse(s);
    const auto& m = services_->components().theme_scope(id)->snapshot().splitter().metrics;
    float offset{};
    for (std::size_t i = 0; i < s.panels.size(); ++i) {
        const float extent = s.visual[i];
        const auto rect = vertical ? runtime::Rect{bounds.x, bounds.y + offset, bounds.width, extent}
                                   : runtime::Rect{rtl ? bounds.x + bounds.width - offset - extent : bounds.x + offset,
                                                   bounds.y, extent, bounds.height};
        engine.place_child(services_->components().root(s.panels[i].snapshot.component), rect, true, true);
        offset += extent;
        if (i >= s.bars.size()) {
            continue;
        }
        auto& bar = s.bars[i];
        const float seam = vertical ? bounds.y + offset : rtl ? bounds.x + bounds.width - offset : bounds.x + offset;
        const auto bar_bounds =
            vertical ? runtime::Rect{bounds.x, seam - m.split_trigger_size / 2, bounds.width, m.split_trigger_size}
                     : runtime::Rect{seam - m.split_trigger_size / 2, bounds.y, m.split_trigger_size, bounds.height};
        engine.place_child(services_->components().root(bar.component), bar_bounds, true, true);
        if (bar.icon.valid()) {
            engine.place_child(services_->components().root(bar.icon),
                               {bar_bounds.x + (bar_bounds.width - m.icon_size) / 2,
                                bar_bounds.y + (bar_bounds.height - m.icon_size) / 2, m.icon_size, m.icon_size},
                               true, true);
        }
        for (bool start : {true, false}) {
            const auto& c = start ? bar.start : bar.end;
            if (!c.enabled) {
                continue;
            }
            const bool before = start != rtl;
            const auto r = vertical ? runtime::Rect{bounds.x + (bounds.width - m.collapse_height) / 2,
                                                    before ? seam - m.split_trigger_size / 2 - m.collapse_width
                                                           : seam + m.split_trigger_size / 2,
                                                    m.collapse_height, m.collapse_width}
                                    : runtime::Rect{before ? seam - m.split_trigger_size / 2 - m.collapse_width
                                                           : seam + m.split_trigger_size / 2,
                                                    bounds.y + (bounds.height - m.collapse_height) / 2,
                                                    m.collapse_width, m.collapse_height};
            engine.place_child(services_->components().root(c.component), r, true, true);
        }
    }
}

void SplitterComponentHost::synchronize_auxiliary_geometry(runtime::Size viewport, runtime::Rect document_clip) {
    const auto owners = mounted_;
    for (auto id : owners) {
        synchronize_content(id, false);
        const auto* s = services_->components().state<SplitterState>(id);
        if (!s || !services_->components().branch_active(id)) {
            continue;
        }
        const auto& t = services_->components().theme_scope(id)->snapshot().splitter();
        const bool vertical = s->orientation == SplitterOrientation::Vertical;
        const auto paint = [&](runtime::ComponentId component, component::RetainedSurfaceId surface, runtime::Rect rect,
                               Color color, float opacity, bool focus = false) {
            const auto node = services_->components().root(component);
            const auto& record = services_->nodes().require(node);
            const auto clip = graphics::EffectClip{
                1,
                services_->nodes().content_clip(node, services_->component_clip(component, viewport, document_clip))};
            std::array<graphics::RoundedEffectInstance, 8> effects;
            const auto fill = graphics::make_corner_fill_effects({rect, 0}, {true, true, true, true}, color,
                                                                 opacity * services_->nodes().content_opacity(node),
                                                                 record.translation, clip);
            std::copy(fill.begin(), fill.end(), effects.begin());
            const auto outline = graphics::make_corner_outline_effects(
                {record.bounds, 0}, {true, true, true, true}, t.metrics.focus_width, 0, t.colors.focus,
                focus ? services_->nodes().content_opacity(node) : 0, record.translation, clip);
            std::copy(outline.begin(), outline.end(), effects.begin() + 4);
            static_cast<void>(services_->surfaces().update_content_effects(surface, effects));
        };
        for (const auto& p : s->panels) {
            paint(p.snapshot.component, p.surface,
                  services_->nodes().require(services_->components().root(p.snapshot.component)).bounds,
                  t.colors.panel_background, 1);
        }
        for (std::size_t i = 0; i < s->bars.size(); ++i) {
            const auto& bar = s->bars[i];
            const auto r = services_->nodes().require(services_->components().root(bar.component)).bounds;
            const auto line = vertical ? runtime::Rect{r.x, r.y + (r.height - t.metrics.split_bar_size) / 2, r.width,
                                                       t.metrics.split_bar_size}
                                       : runtime::Rect{r.x + (r.width - t.metrics.split_bar_size) / 2, r.y,
                                                       t.metrics.split_bar_size, r.height};
            paint(bar.component, bar.surface, line,
                  s->dragging && s->resize_index == i ? t.colors.active_bar
                  : bar.hovered                       ? t.colors.hover_bar
                                                      : t.colors.bar,
                  1, bar.focus.focus_visible);
            const auto mark = vertical
                                  ? runtime::Rect{r.x + (r.width - t.metrics.split_bar_draggable_size) / 2, line.y,
                                                  std::min(r.width, t.metrics.split_bar_draggable_size), line.height}
                                  : runtime::Rect{line.x, r.y + (r.height - t.metrics.split_bar_draggable_size) / 2,
                                                  line.width, std::min(r.height, t.metrics.split_bar_draggable_size)};
            paint(bar.component, bar.mark_surface, mark, t.colors.icon, !bar.icon.valid() && bar.resizable ? 1 : 0);
            for (const auto* c : {&bar.start, &bar.end}) {
                if (!c->enabled) {
                    continue;
                }
                paint(c->component, c->surface,
                      services_->nodes().require(services_->components().root(c->component)).bounds,
                      t.colors.collapse_background, 1, c->focus.focus_visible);
            }
        }
        const auto& root = services_->nodes().require(s->node);
        runtime::Rect preview{};
        if (s->lazy && s->dragging && !s->preview.empty()) {
            float offset{};
            for (std::size_t i = 0; i <= s->resize_index; ++i) {
                offset += s->preview[i];
            }
            const float seam = vertical      ? root.bounds.y + offset
                               : reverse(*s) ? root.bounds.x + root.bounds.width - offset
                                             : root.bounds.x + offset;
            preview = vertical ? runtime::Rect{root.bounds.x, seam - t.metrics.split_bar_size / 2, root.bounds.width,
                                               t.metrics.split_bar_size}
                               : runtime::Rect{seam - t.metrics.split_bar_size / 2, root.bounds.y,
                                               t.metrics.split_bar_size, root.bounds.height};
        }
        paint(id, s->preview_surface, preview, t.colors.preview, s->lazy && s->dragging ? 1 : 0);
    }
}

std::size_t SplitterComponentHost::tick_auxiliary(animation::AnimationTime time) {
    std::size_t changed{};
    for (auto id : mounted_) {
        auto* s = services_->components().state<SplitterState>(id);
        if (!s || !s->animating) {
            continue;
        }
        const auto now = std::max(time, s->last_tick);
        s->last_tick = now;
        const auto policy = animation::resolve_motion_policy(services_->components().theme_scope(id)->snapshot(),
                                                             services_->motion_preference());
        const auto elapsed = (now - s->motion_start).count_microseconds();
        const float progress =
            !policy.enabled() || !s->collapse_motion
                ? 1
                : std::clamp(static_cast<float>(static_cast<double>(elapsed) / s->motion_duration.count_microseconds()),
                             0.0F, 1.0F);
        const float amount = s->motion_easing.sample(progress);
        for (std::size_t i = 0; i < s->sizes.size(); ++i) {
            s->visual[i] = std::max(0.0F, s->motion_from[i] + (s->sizes[i] - s->motion_from[i]) * amount);
        }
        if (progress >= 1) {
            s->animating = false;
            s->visual = s->sizes;
        }
        services_->dirty().invalidate(s->node, runtime::DirtyFlags::Measure | runtime::DirtyFlags::Layout |
                                                   runtime::DirtyFlags::Geometry);
        ++changed;
    }
    return changed;
}

std::optional<animation::AnimationTime> SplitterComponentHost::next_auxiliary_deadline() const {
    std::optional<animation::AnimationTime> result;
    for (auto id : mounted_) {
        const auto* s = services_->components().state<SplitterState>(id);
        if (!s || !s->animating) {
            continue;
        }
        const auto next = std::min(s->last_tick + animation::AnimationDuration::milliseconds(16),
                                   s->motion_start + s->motion_duration);
        if (!result || next < *result) {
            result = next;
        }
    }
    return result;
}

} // namespace ryn::detail

namespace ryn {
void Splitter(SplitterProps props, std::vector<SplitterPanel> panels) {
    if (!detail::active_splitter) {
        throw std::logic_error("Splitter requires window component services");
    }
    detail::active_splitter->mount(props, panels);
}
} // namespace ryn
